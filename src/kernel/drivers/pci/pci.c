#include "pci.h"
#include "../ports/IO.h"
#include "../../utils/logging/logger.h"
#include "../../lib/string/string.h"

#define MODULE "PCI"

static pci_device_t pci_devices[PCI_MAX_DEVICES];
static uint32_t pci_device_count = 0;

static inline uint32_t pci_get_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (uint32_t)((1U << 31) |
                      ((uint32_t)bus << 16) |
                      ((uint32_t)slot << 11) |
                      ((uint32_t)func << 8) |
                      (offset & 0xFC));
}

uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_get_addr(bus, slot, func, offset));
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_get_addr(bus, slot, func, offset));
    return (uint16_t)((inl(PCI_CONFIG_DATA) >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_config_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_get_addr(bus, slot, func, offset));
    return (uint8_t)((inl(PCI_CONFIG_DATA) >> ((offset & 3) * 8)) & 0xFF);
}

void pci_config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    outl(PCI_CONFIG_ADDRESS, pci_get_addr(bus, slot, func, offset));
    outl(PCI_CONFIG_DATA, val);
}

void pci_config_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val) {
    uint32_t current = pci_config_read_dword(bus, slot, func, offset);
    uint32_t shift = (offset & 2) * 8;
    current &= ~(0xFFFF << shift);
    current |= ((uint32_t)val << shift);
    pci_config_write_dword(bus, slot, func, offset, current);
}

const char *pci_class_to_string(uint8_t class_code, uint8_t subclass) {
    switch (class_code) {
        case 0x00: return "Legacy/Unclassified";
        case 0x01:
            switch (subclass) {
                case 0x01: return "IDE Controller";
                case 0x06: return "SATA Controller";
                default:   return "Mass Storage Controller";
            }
        case 0x02:
            switch (subclass) {
                case 0x00: return "Ethernet Controller";
                default:   return "Network Controller";
            }
        case 0x03:
            switch (subclass) {
                case 0x00: return "VGA Compatible Controller";
                default:   return "Display Controller";
            }
        case 0x04: return "Multimedia Device";
        case 0x05: return "Memory Controller";
        case 0x06:
            switch (subclass) {
                case 0x00: return "Host Bridge";
                case 0x01: return "ISA Bridge";
                case 0x04: return "PCI-to-PCI Bridge";
                default:   return "Bridge Device";
            }
        case 0x07: return "Simple Communications Device";
        case 0x08: return "Base System Peripheral";
        case 0x09: return "Input Device Controller";
        case 0x0C:
            switch (subclass) {
                case 0x03: return "USB Controller";
                default:   return "Serial Bus Controller";
            }
        default: return "Unknown Device";
    }
}

static void pci_scan_function(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t vendor_id = pci_config_read_word(bus, slot, func, 0x00);
    if (vendor_id == 0xFFFF || vendor_id == 0x0000) {
        return;
    }

    if (pci_device_count >= PCI_MAX_DEVICES) {
        log_warning(MODULE, "Maximum device capacity reached (%d)", PCI_MAX_DEVICES);
        return;
    }

    pci_device_t *dev = &pci_devices[pci_device_count++];
    dev->bus = bus;
    dev->slot = slot;
    dev->func = func;
    dev->vendor_id = vendor_id;
    dev->device_id = pci_config_read_word(bus, slot, func, 0x02);
    dev->class_code = pci_config_read_byte(bus, slot, func, 0x0B);
    dev->subclass = pci_config_read_byte(bus, slot, func, 0x0A);
    dev->prog_if = pci_config_read_byte(bus, slot, func, 0x09);
    dev->revision_id = pci_config_read_byte(bus, slot, func, 0x08);
    dev->header_type = pci_config_read_byte(bus, slot, func, 0x0E) & 0x7F;
    dev->irq_line = pci_config_read_byte(bus, slot, func, 0x3C);
    dev->irq_pin = pci_config_read_byte(bus, slot, func, 0x3D);

    for (int b = 0; b < 6; b++) {
        dev->bar[b] = pci_config_read_dword(bus, slot, func, 0x10 + (b * 4));
    }

    log_info(MODULE, "[%02x:%02x.%d] %04x:%04x Class %02x:%02x (%s) IRQ %u",
             bus, slot, func, dev->vendor_id, dev->device_id,
             dev->class_code, dev->subclass,
             pci_class_to_string(dev->class_code, dev->subclass),
             dev->irq_line);
}

static void pci_scan_slot(uint8_t bus, uint8_t slot) {
    uint16_t vendor_id = pci_config_read_word(bus, slot, 0, 0x00);
    if (vendor_id == 0xFFFF || vendor_id == 0x0000) {
        return;
    }

    pci_scan_function(bus, slot, 0);
    uint8_t header_type = pci_config_read_byte(bus, slot, 0, 0x0E);
    if (header_type & 0x80) { // Multi-function device
        for (uint8_t func = 1; func < 8; func++) {
            pci_scan_function(bus, slot, func);
        }
    }
}

void pci_initialize(void) {
    pci_device_count = 0;

    uint8_t header_type = pci_config_read_byte(0, 0, 0, 0x0E);
    if ((header_type & 0x80) == 0) {
        // Single PCI host controller: scan bus 0
        for (uint8_t slot = 0; slot < 32; slot++) {
            pci_scan_slot(0, slot);
        }
    } else {
        // Multiple PCI host controllers
        for (uint8_t func = 0; func < 8; func++) {
            if (pci_config_read_word(0, 0, func, 0x00) != 0xFFFF) {
                for (uint8_t slot = 0; slot < 32; slot++) {
                    pci_scan_slot(func, slot);
                }
            }
        }
    }
    log_info(MODULE, "Enumeration complete. %u PCI devices registered", pci_device_count);
}

uint32_t pci_get_devices(pci_device_t *out_devices, uint32_t max_count) {
    if (!out_devices || max_count == 0) return 0;
    uint32_t count = (pci_device_count < max_count) ? pci_device_count : max_count;
    memcpy(out_devices, pci_devices, count * sizeof(pci_device_t));
    return count;
}

pci_device_t *pci_get_device(uint16_t vendor_id, uint16_t device_id) {
    for (uint32_t i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id && pci_devices[i].device_id == device_id) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

void pci_enable_bus_mastering(pci_device_t *dev) {
    uint16_t cmd = pci_config_read_word(dev->bus, dev->slot, dev->func, 0x04);
    cmd |= 0x0004; // Bus Master bit
    pci_config_write_word(dev->bus, dev->slot, dev->func, 0x04, cmd);
    log_debug(MODULE, "Enabled Bus Mastering for %04x:%04x", dev->vendor_id, dev->device_id);
}