#include "rtl8139.h"
#include "../../kernel_services/kernel_services.h"
#include "../../lib/string/string.h"
#include "../../utils/logging/logger.h"
#include "../pci/pci.h"
#include "../ports/IO.h"
#include "../../mem/physical_memory_manager/pmm.h"
#include "../../mem/virtual_memory_manager/vmm.h"
#include "../../arch/x86/interrupt_descriptor_table/idt.h"
#include "../../net/net.h"

#define MODULE "RTL8139"

static uint32_t rtl_io_base = 0;
static uint8_t rtl_mac[6];

static uint32_t rtl_rx_buffer_phys;
static uint8_t *rtl_rx_buffer_virt;
static uint32_t current_rx_ptr = 0;

static uint32_t rtl_tx_buffer_phys;
static uint8_t *rtl_tx_buffer_virt;
static uint8_t tx_idx = 0;

void rtl8139_send(uint8_t *data, uint32_t len) {
    if (!rtl_io_base) {
        log_error(MODULE, "rtl8139_send: NIC not initialized or I/O base is zero");
        return;
    }

    log_trace(MODULE, "rtl8139_send: transmitting packet of size %u bytes via descriptor %u", len, tx_idx);
    void *tx_virt = (void *)(rtl_tx_buffer_virt + (tx_idx * 2048));
    memcpy(tx_virt, data, len);
    
    uint32_t tx_phys = rtl_tx_buffer_phys + (tx_idx * 2048);
    outl(rtl_io_base + 0x20 + (tx_idx * 4), tx_phys); // TSAD
    outl(rtl_io_base + 0x10 + (tx_idx * 4), len);     // TSD (Start TX)
    
    tx_idx = (tx_idx + 1) % 4;
}

static void rtl8139_receive(void) {
    while ((inb(rtl_io_base + 0x37) & 0x01) == 0) {
        uint32_t offset = current_rx_ptr % 8192;
        uint32_t *header = (uint32_t *)(rtl_rx_buffer_virt + offset);
        uint16_t rx_status = *header & 0xFFFF;
        uint16_t rx_size = (*header >> 16) & 0xFFFF;
        
        if (rx_status & 0x0001) { // ROK (Receive OK)
            uint8_t *packet = rtl_rx_buffer_virt + offset + 4;
            uint16_t packet_len = rx_size - 4; // Drop CRC
            
            log_trace(MODULE, "rtl8139_receive: received valid packet, length=%u bytes", packet_len);
            uint8_t *packet_copy = kmem_alloc(packet_len);
            if (!packet_copy) {
                log_error(MODULE, "rtl8139_receive: failed to allocate memory for incoming packet copy");
                break;
            }

            if (offset + 4 + rx_size > 8192) {
                uint32_t part1 = 8192 - (offset + 4);
                memcpy(packet_copy, packet, part1);
                memcpy(packet_copy + part1, rtl_rx_buffer_virt, packet_len - part1);
            } else {
                memcpy(packet_copy, packet, packet_len);
            }
            
            net_handle_packet(packet_copy, packet_len);
            kmem_free(packet_copy);
        } else {
            log_warning(MODULE, "rtl8139_receive: received packet with error status 0x%X", rx_status);
        }
        
        current_rx_ptr = (current_rx_ptr + rx_size + 4 + 3) & ~3;
        outw(rtl_io_base + 0x38, current_rx_ptr - 16);
    }
}

bool rtl8139_isr(void) {
    uint16_t status = inw(rtl_io_base + 0x3E);
    if (status) {
        log_trace(MODULE, "rtl8139_isr: handled interrupt status register 0x%X", status);
        outw(rtl_io_base + 0x3E, status);
    }
    if (status & 0x01) {
        rtl8139_receive();
    }
    return false;
}

void rtl8139_initialize(void) {
    log_debug(MODULE, "Searching for RTL8139 on PCI Bus...");
    pci_device_t *dev = pci_get_device(0x10EC, 0x8139);
    if (!dev) {
        log_warning(MODULE, "RTL8139 Network Interface not found on PCI bus");
        return;
    }
    
    rtl_io_base = dev->bar[0] & ~3;
    log_info(MODULE, "Found RTL8139 at I/O base 0x%X, IRQ %u", rtl_io_base, dev->irq_line);
    
    pci_enable_bus_mastering(dev);
    
    outb(rtl_io_base + 0x52, 0x0); // Power on
    
    outb(rtl_io_base + 0x37, 0x10); // Reset
    while ((inb(rtl_io_base + 0x37) & 0x10) != 0) {
        ksleep(10);
    }
    
    rtl_rx_buffer_phys = (uint32_t)pmm_alloc_contiguous_blocks(3);
    rtl_tx_buffer_phys = (uint32_t)pmm_alloc_contiguous_blocks(2);
    
    if (!rtl_rx_buffer_phys || !rtl_tx_buffer_phys) {
        log_error(MODULE, "Failed to allocate contiguous DMA ring buffers for RTL8139");
        return;
    }

    vmm_map_region(rtl_rx_buffer_phys, 0xE0000000, 3 * 4096, PAGE_PRESENT | PAGE_WRITE);
    vmm_map_region(rtl_tx_buffer_phys, 0xE0003000, 2 * 4096, PAGE_PRESENT | PAGE_WRITE);
    
    rtl_rx_buffer_virt = (uint8_t *)0xE0000000;
    rtl_tx_buffer_virt = (uint8_t *)0xE0003000;
    memset(rtl_rx_buffer_virt, 0, 3 * 4096);
    memset(rtl_tx_buffer_virt, 0, 2 * 4096);
    
    outl(rtl_io_base + 0x30, rtl_rx_buffer_phys);
    outw(rtl_io_base + 0x3C, 0x0005); // Interrupt Mask
    outl(rtl_io_base + 0x44, 0x8F | (1 << 7)); // RX Config
    outb(rtl_io_base + 0x37, 0x0C); // Enable RX/TX
    
    for (int i = 0; i < 6; i++) {
        rtl_mac[i] = inb(rtl_io_base + i);
    }
    net_set_mac(rtl_mac);
    
    log_info(MODULE, "Hardware MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             rtl_mac[0], rtl_mac[1], rtl_mac[2], rtl_mac[3], rtl_mac[4], rtl_mac[5]);
             
    uint8_t irq = dev->irq_line;
    uint8_t vector = IRQ_TO_VECTOR(irq);
    register_interrupt_handler(vector, rtl8139_isr);
    pic_unmask(irq);
    
    log_info(MODULE, "Driver Activated & DMA Ring Queued successfully");
}