#include "ac97.h"
#include "../pci/pci.h"
#include "../ports/IO.h"
#include "../../utils/logging/logger.h"
#include "../../mem/physical_memory_manager/pmm.h"
#include "../../mem/virtual_memory_manager/vmm.h"
#include "../../arch/x86/interrupt_descriptor_table/idt.h"
#include "../../arch/x86/sync/spinlock.h"
#include "../../kernel_services/kernel_services.h"
#include "../../lib/string/string.h"
#include "../../arch/x86/pit/pit.h"
#include "../../arch/x86/task/task.h"

#define MODULE "AC97"

#define AC97_PCI_VENDOR_ID 0x8086
#define AC97_PCI_DEVICE_ID 0x2415

#define AC97_MIXER_RESET             0x00
#define AC97_MIXER_MASTER_VOL        0x02
#define AC97_MIXER_PCM_OUT_VOL       0x18
#define AC97_MIXER_EXT_AUDIO_ID      0x28
#define AC97_MIXER_EXT_AUDIO_CTRL    0x2A
#define AC97_MIXER_PCM_FRONT_DAC_RATE 0x2C

#define AC97_PO_BDBAR  0x10
#define AC97_PO_CIV    0x14
#define AC97_PO_LVI    0x15
#define AC97_PO_SR     0x16
#define AC97_PO_PICB   0x18
#define AC97_PO_PIV    0x1A
#define AC97_PO_CR     0x1B
#define AC97_GLOB_CNT  0x2C
#define AC97_GLOB_STA  0x30

#define AC97_CR_RUN_PAUSE    0x01
#define AC97_CR_RESET        0x02
#define AC97_CR_IOC_ENABLE   0x04
#define AC97_SR_DCH          0x0001
#define AC97_SR_CELV         0x0002
#define AC97_SR_LVBCI        0x0004
#define AC97_SR_BCIS         0x0008
#define AC97_SR_FIFOE        0x0010

#define AC97_DMA_BUFFER_SIZE 8192

static uint32_t ac97_nambar = 0;
static uint32_t ac97_nabmbar = 0;
static bool ac97_available = false;

static uint32_t ac97_bdl_phys = 0;
static ac97_bdl_entry_t *ac97_bdl_virt = NULL;

static uint32_t ac97_dma_pool_phys = 0;
static uint8_t *ac97_dma_pool_virt = NULL;

static spinlock_t ac97_lock = SPINLOCK_INIT;

bool ac97_is_present(void) {
    log_trace(MODULE, "ac97_is_present called, status=%s", ac97_available ? "true" : "false");
    return ac97_available;
}

bool ac97_isr(void) {
    log_trace(MODULE, "ac97_isr invoked");
    if (!ac97_available) return false;

    uint16_t status = inw(ac97_nabmbar + AC97_PO_SR);
    if (status & (AC97_SR_BCIS | AC97_SR_LVBCI | AC97_SR_FIFOE)) {
        log_debug(MODULE, "ac97_isr: Handled PO_SR status=0x%X", status);
        outw(ac97_nabmbar + AC97_PO_SR, status & (AC97_SR_BCIS | AC97_SR_LVBCI | AC97_SR_FIFOE));
        return true;
    }
    return false;
}

static void ac97_reset_bus_master(void) {
    log_trace(MODULE, "ac97_reset_bus_master: resetting PCM Out channel");
    outb(ac97_nabmbar + AC97_PO_CR, AC97_CR_RESET);
    uint32_t timeout = 100000;
    while ((inb(ac97_nabmbar + AC97_PO_CR) & AC97_CR_RESET) && --timeout) {
        __asm__ volatile("pause");
    }
    if (timeout == 0) {
        log_warning(MODULE, "Timeout resetting AC'97 PCM Out channel");
    } else {
        log_debug(MODULE, "AC'97 PCM Out channel reset successfully");
    }
}

int ac97_play_pcm(const uint8_t *pcm_data, uint32_t length, uint32_t sample_rate, uint8_t channels, uint8_t bits_per_sample) {
    log_trace(MODULE, "ac97_play_pcm: length=%u, rate=%u, ch=%u, bits=%u", length, sample_rate, channels, bits_per_sample);
    if (!ac97_available) {
        log_error(MODULE, "ac97_play_pcm: AC'97 hardware not present");
        return -1;
    }
    if (!pcm_data || length == 0) {
        log_error(MODULE, "ac97_play_pcm: Invalid PCM audio buffer");
        return -1;
    }
    if (bits_per_sample != 16 || channels != 2) {
        log_warning(MODULE, "ac97_play_pcm: Standard AC'97 DAC requires 16-bit stereo PCM (got %u-bit, %u ch)",
                    bits_per_sample, channels);
        return -2;
    }

    uint32_t flags = spinlock_acquire_irq_save(&ac97_lock);

    log_info(MODULE, "Streaming PCM audio: %u bytes @ %u Hz (Stereo 16-bit)", length, sample_rate);

    outw(ac97_nambar + AC97_MIXER_PCM_FRONT_DAC_RATE, (uint16_t)(sample_rate & 0xFFFF));
    uint16_t actual_rate = inw(ac97_nambar + AC97_MIXER_PCM_FRONT_DAC_RATE);
    log_debug(MODULE, "Configured Front DAC sample rate: %u Hz (Readback: %u Hz)", sample_rate, actual_rate);

    outb(ac97_nabmbar + AC97_PO_CR, 0);
    ac97_reset_bus_master();

    outw(ac97_nabmbar + AC97_PO_SR, 0x001C);

    uint32_t bytes_played = 0;
    while (bytes_played < length) {
        uint32_t remaining = length - bytes_played;
        uint32_t num_descriptors = 0;

        for (int i = 0; i < AC97_MAX_BDL_ENTRIES && remaining > 0; i++) {
            uint32_t chunk_size = (remaining > AC97_DMA_BUFFER_SIZE) ? AC97_DMA_BUFFER_SIZE : remaining;

            uint8_t *dst = ac97_dma_pool_virt + (i * AC97_DMA_BUFFER_SIZE);
            memcpy(dst, pcm_data + bytes_played, chunk_size);

            if (chunk_size < AC97_DMA_BUFFER_SIZE) {
                memset(dst + chunk_size, 0, AC97_DMA_BUFFER_SIZE - chunk_size);
            }

            ac97_bdl_virt[i].buffer_phys = ac97_dma_pool_phys + (i * AC97_DMA_BUFFER_SIZE);
            ac97_bdl_virt[i].samples = (uint16_t)(chunk_size / 2);
            ac97_bdl_virt[i].flags = 0;

            bytes_played += chunk_size;
            remaining -= chunk_size;
            num_descriptors++;
        }

        if (num_descriptors == 0) break;

        uint8_t lvi = (uint8_t)(num_descriptors - 1);
        ac97_bdl_virt[lvi].flags = (1 << 15);

        outl(ac97_nabmbar + AC97_PO_BDBAR, ac97_bdl_phys);
        outb(ac97_nabmbar + AC97_PO_LVI, lvi);

        outb(ac97_nabmbar + AC97_PO_CR, AC97_CR_RUN_PAUSE | AC97_CR_IOC_ENABLE);

        while (1) {
            uint16_t sr = inw(ac97_nabmbar + AC97_PO_SR);
            uint8_t civ = inb(ac97_nabmbar + AC97_PO_CIV);

            if ((sr & AC97_SR_DCH) || (civ == lvi && (sr & AC97_SR_CELV))) {
                break;
            }
            task_yield();
        }

        outb(ac97_nabmbar + AC97_PO_CR, 0);
    }

    outb(ac97_nabmbar + AC97_PO_CR, 0);
    spinlock_release_irq_restore(&ac97_lock, flags);

    log_trace(MODULE, "Audio playback completed successfully");
    return 0;
}

void ac97_initialize(void) {
    log_info(MODULE, "Scanning PCI Bus for Intel 82801AA AC'97 Audio Controller...");

    pci_device_t *dev = pci_get_device(AC97_PCI_VENDOR_ID, AC97_PCI_DEVICE_ID);
    if (!dev) {
        log_warning(MODULE, "AC'97 Audio Controller (0x8086:0x2415) not found on PCI bus");
        ac97_available = false;
        return;
    }

    ac97_nambar = dev->bar[0] & ~3;
    ac97_nabmbar = dev->bar[1] & ~3;

    log_info(MODULE, "Found AC'97 Controller: NAMBAR=0x%X, NABMBAR=0x%X, IRQ=%u",
             ac97_nambar, ac97_nabmbar, dev->irq_line);

    pci_enable_bus_mastering(dev);

    outl(ac97_nabmbar + AC97_GLOB_CNT, 0x02);
    for (volatile int i = 0; i < 200000; i++) { __asm__ volatile("pause"); }
    outl(ac97_nabmbar + AC97_GLOB_CNT, 0x00);

    outw(ac97_nambar + AC97_MIXER_RESET, 0x0000);
    for (volatile int i = 0; i < 200000; i++) { __asm__ volatile("pause"); }

    outw(ac97_nambar + AC97_MIXER_MASTER_VOL, 0x0000);
    outw(ac97_nambar + AC97_MIXER_PCM_OUT_VOL, 0x0000);

    uint16_t ext_id = inw(ac97_nambar + AC97_MIXER_EXT_AUDIO_ID);
    if (ext_id & 1) {
        outw(ac97_nambar + AC97_MIXER_EXT_AUDIO_CTRL, 1);
        log_debug(MODULE, "Enabled Variable Rate Audio (VRA) support");
    }

    outw(ac97_nambar + AC97_MIXER_PCM_FRONT_DAC_RATE, 44100);

    ac97_bdl_phys = (uint32_t)pmm_alloc_block();
    // Allocate 64 contiguous physical pages (256 KB) to support 32 descriptors * 8 KB buffers without page faults
    ac97_dma_pool_phys = (uint32_t)pmm_alloc_contiguous_blocks(64);

    if (!ac97_bdl_phys || !ac97_dma_pool_phys) {
        log_error(MODULE, "Failed to allocate contiguous DMA pages for AC'97 audio engine");
        ac97_available = false;
        return;
    }

    vmm_map_region(ac97_bdl_phys, 0xE1000000, 4096, PAGE_PRESENT | PAGE_WRITE);
    vmm_map_region(ac97_dma_pool_phys, 0xE1001000, 64 * 4096, PAGE_PRESENT | PAGE_WRITE);

    ac97_bdl_virt = (ac97_bdl_entry_t *)0xE1000000;
    ac97_dma_pool_virt = (uint8_t *)0xE1001000;

    memset(ac97_bdl_virt, 0, 4096);
    memset(ac97_dma_pool_virt, 0, 64 * 4096);

    uint8_t irq = dev->irq_line;
    uint8_t vector = IRQ_TO_VECTOR(irq);
    register_interrupt_handler(vector, ac97_isr);
    pic_unmask(irq);

    ac97_available = true;
    log_info(MODULE, "AC'97 PCM Audio Driver Initialized & Ready (Vector 0x%X)", vector);
}