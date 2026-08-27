#include "acpi.h"
#include "../ports/IO.h"
#include "../../utils/logging/logger.h"
#include "../../lib/string/string.h"
#include "../../mem/virtual_memory_manager/vmm.h"

#define MODULE "ACPI"

typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed)) rsdp_t;

typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_header_t;

static uint32_t smi_cmd_port = 0;
static uint8_t acpi_enable = 0;
static uint8_t acpi_disable = 0;
static uint32_t pm1a_cnt_blk = 0;
static uint32_t pm1b_cnt_blk = 0;
static uint16_t slp_typa = 0;
static uint16_t slp_typb = 0;
static bool acpi_enabled_flag = false;

static acpi_header_t *get_rsdt(void) {
    // 1. Search the Extended BIOS Data Area (EBDA) first
    uint16_t ebda_seg = *((uint16_t *)0x40E);
    uint32_t ebda_phys = ebda_seg << 4;
    if (ebda_phys != 0) {
        uint32_t *ptr = (uint32_t *)ebda_phys;
        uint32_t *end = (uint32_t *)(ebda_phys + 1024);
        while (ptr < end) {
            if (*ptr == 0x20445352 && *(ptr+1) == 0x20525450) { // "RSD PTR "
                uint8_t sum = 0;
                uint8_t *b = (uint8_t *)ptr;
                for (uint32_t i = 0; i < sizeof(rsdp_t); i++) sum += b[i];
                if (sum == 0) return (acpi_header_t *)((rsdp_t *)ptr)->rsdt_address;
            }
            ptr += 4; // 16-byte boundaries
        }
    }

    // 2. Search BIOS Read-Only Memory Space
    uint32_t *ptr = (uint32_t *)0x000E0000;
    uint32_t *end = (uint32_t *)0x000FFFFF;
    while (ptr < end) {
        if (*ptr == 0x20445352 && *(ptr+1) == 0x20525450) { // "RSD PTR "
            uint8_t sum = 0;
            uint8_t *b = (uint8_t *)ptr;
            for (uint32_t i = 0; i < sizeof(rsdp_t); i++) sum += b[i];
            if (sum == 0) return (acpi_header_t *)((rsdp_t *)ptr)->rsdt_address;
        }
        ptr += 4; 
    }

    return NULL;
}

void acpi_initialize(void) {
    acpi_header_t *rsdt = get_rsdt();
    if (!rsdt) {
        log_warning(MODULE, "RSDP not found. Standard ACPI missing (Emulators will use fallback ports).");
        return;
    }

    // Map 64KB for the RSDT to prevent high-memory page faults
    vmm_map_region((uint32_t)rsdt & ~0xFFF, (uint32_t)rsdt & ~0xFFF, 65536, PAGE_PRESENT | PAGE_WRITE);

    if (strncmp(rsdt->signature, "RSDT", 4) != 0) {
        log_warning(MODULE, "Invalid RSDT signature.");
        return;
    }

    uint32_t entries = (rsdt->length - sizeof(acpi_header_t)) / 4;
    uint32_t *ptrs = (uint32_t *)((uint8_t *)rsdt + sizeof(acpi_header_t));

    acpi_header_t *fadt = NULL;
    for (uint32_t i = 0; i < entries; i++) {
        acpi_header_t *h = (acpi_header_t *)ptrs[i];
        
        // Map 8KB for the target header table before accessing
        vmm_map_region((uint32_t)h & ~0xFFF, (uint32_t)h & ~0xFFF, 8192, PAGE_PRESENT | PAGE_WRITE);

        if (strncmp(h->signature, "FACP", 4) == 0) {
            fadt = h;
            break;
        }
    }

    if (!fadt) {
        log_warning(MODULE, "FADT not found.");
        return;
    }

    uint8_t *fadt_bytes = (uint8_t *)fadt;
    uint32_t dsdt_addr = *(uint32_t *)(fadt_bytes + 40);
    smi_cmd_port = *(uint32_t *)(fadt_bytes + 48);
    acpi_enable = *(uint8_t *)(fadt_bytes + 52);
    acpi_disable = *(uint8_t *)(fadt_bytes + 53);
    pm1a_cnt_blk = *(uint32_t *)(fadt_bytes + 64);
    pm1b_cnt_blk = *(uint32_t *)(fadt_bytes + 68);

    if (dsdt_addr) {
        // Map 64KB for the DSDT block
        vmm_map_region(dsdt_addr & ~0xFFF, dsdt_addr & ~0xFFF, 65536, PAGE_PRESENT | PAGE_WRITE);
        acpi_header_t *dsdt = (acpi_header_t *)dsdt_addr;
        
        if (strncmp(dsdt->signature, "DSDT", 4) == 0) {
            uint8_t *s5_addr = (uint8_t *)dsdt + sizeof(acpi_header_t);
            int dsdt_length = dsdt->length - sizeof(acpi_header_t);

            while (dsdt_length-- > 0) {
                if (memcmp(s5_addr, "_S5_", 4) == 0) break;
                s5_addr++;
            }

            if (dsdt_length > 0) {
                // Check for valid AML package
                if ((*(s5_addr - 1) == 0x08 || *(s5_addr - 2) == 0x08) && *(s5_addr - 1) != '\\') {
                    s5_addr += 4;
                    s5_addr += ((*s5_addr & 0xC0) >> 6) + 2; // Skip package length
                    if (*s5_addr == 0x0A) s5_addr++;
                    slp_typa = *(s5_addr) << 10;
                    s5_addr++;
                    if (*s5_addr == 0x0A) s5_addr++;
                    slp_typb = *(s5_addr) << 10;
                } else {
                    slp_typa = (5 << 10); // QEMU fallback
                }
            } else {
                slp_typa = (5 << 10); // QEMU fallback
            }
        }
    }

    acpi_enabled_flag = true;
    log_info(MODULE, "Initialized. Soft power-off available.");
}

void acpi_poweroff(void) {
    if (acpi_enabled_flag) {
        // Send ACPI Enable command if not already enabled
        if (smi_cmd_port != 0 && acpi_enable != 0) {
            outb(smi_cmd_port, acpi_enable);
            for (volatile int i = 0; i < 10000; i++);
        }

        // Trigger the sleep state S5 (Power Off)
        if (pm1a_cnt_blk != 0) {
            outw(pm1a_cnt_blk, slp_typa | (1 << 13));
        }
        if (pm1b_cnt_blk != 0) {
            outw(pm1b_cnt_blk, slp_typb | (1 << 13));
        }
    }

    // Direct Magic Ports (Executes unconditionally to guarantee emulator shutdown)
    outw(0xB004, 0x2000); // Bochs/QEMU 1
    outw(0x604, 0x2000);  // Bochs/QEMU 2
    outw(0x4004, 0x3400); // VirtualBox

    log_error(MODULE, "System halted. It is now safe to turn off your computer.");
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

void acpi_reboot(void) {
    // 1. PCI Host Controller Reset (Most reliable for QEMU)
    log_info(MODULE, "Attempting PCI Host Controller reset...");
    uint8_t reset_val = inb(0xCF9) & ~6;
    outb(0xCF9, reset_val | 2); // Request reset
    for (volatile int i = 0; i < 100000; i++);
    outb(0xCF9, reset_val | 6); // Trigger reset
    for (volatile int i = 0; i < 100000; i++);

    log_info(MODULE, "Triggering 8042 PS/2 controller reset...");
    while (inb(0x64) & 0x01) {
        inb(0x60);
    }
    outb(0x64, 0xFE); // Pulse CPU reset line
    for (volatile int i = 0; i < 100000; i++);

    log_warning(MODULE, "Falling back to Triple Fault reset...");
    volatile struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) idt_ptr = {0, 0};
    __asm__ volatile("cli; lidt %0; int $3" : : "m"(idt_ptr) : "memory");
    
    uint32_t *null_ptr = (uint32_t *)0xFFFFFFFF;
    *null_ptr = 0xDEADBEEF;

    log_error(MODULE, "Reboot failed. Halting.");
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}