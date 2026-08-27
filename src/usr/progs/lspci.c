#include "../libs/meow_libc.h"

#define MAX_PCI_LIST 32

DESCRIPTION("lspci.elf: List PCI devices");

static const char *get_class_name(uint8_t class_code, uint8_t subclass) {
    switch (class_code) {
        case 0x00: return "Unclassified device";
        case 0x01:
            switch (subclass) {
                case 0x00: return "SCSI storage controller";
                case 0x01: return "IDE interface";
                case 0x05: return "ATA controller";
                case 0x06: return "SATA controller";
                case 0x08: return "NVMe controller";
                default:   return "Mass storage controller";
            }
        case 0x02:
            switch (subclass) {
                case 0x00: return "Ethernet controller";
                default:   return "Network controller";
            }
        case 0x03:
            switch (subclass) {
                case 0x00: return "VGA compatible controller";
                default:   return "Display controller";
            }
        case 0x04:
            switch (subclass) {
                case 0x00: return "Multimedia video controller";
                case 0x01: return "Multimedia audio controller";
                case 0x03: return "Audio device (HD Audio)";
                default:   return "Multimedia controller";
            }
        case 0x05: return "Memory controller";
        case 0x06:
            switch (subclass) {
                case 0x00: return "Host bridge";
                case 0x01: return "ISA bridge";
                case 0x04: return "PCI bridge";
                default:   return "Bridge";
            }
        case 0x07: return "Communication controller";
        case 0x08:
            switch (subclass) {
                case 0x00: return "PIC";
                case 0x01: return "DMA controller";
                case 0x02: return "Timer";
                case 0x03: return "RTC";
                default:   return "Generic system peripheral";
            }
        case 0x09:
            switch (subclass) {
                case 0x00: return "Keyboard controller";
                case 0x02: return "Mouse controller";
                default:   return "Input device controller";
            }
        case 0x0C:
            switch (subclass) {
                case 0x00: return "FireWire (IEEE 1394)";
                case 0x03: return "USB controller";
                default:   return "Serial bus controller";
            }
        default: return "Unknown";
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    pci_device_t devices[MAX_PCI_LIST];
    int count = get_pci_devices(devices, MAX_PCI_LIST);

    if (count < 0) {
        printf("lspci: failed to retrieve PCI device table\n");
        return 1;
    }

    if (count == 0) {
        printf("lspci: no PCI devices detected\n");
        return 0;
    }

    printf("======================================================================\n");
    printf("SLOT    VENDOR:DEVICE  CLASS:SUB   IRQ  DESCRIPTION\n");
    printf("======================================================================\n");

    for (int i = 0; i < count; i++) {
        pci_device_t *d = &devices[i];
        
        char slot_str[16];
        snprintf(slot_str, sizeof(slot_str), "%02x:%02x.%d", d->bus, d->slot, d->func);

        char id_str[16];
        snprintf(id_str, sizeof(id_str), "%04x:%04x", d->vendor_id, d->device_id);

        char class_str[16];
        snprintf(class_str, sizeof(class_str), "%02x:%02x", d->class_code, d->subclass);

        printf("%s %s      %s       %2u   %s\n",
               slot_str, id_str, class_str, d->irq_line,
               get_class_name(d->class_code, d->subclass));

        // Show non-zero Base Address Registers (BARs)
        for (int b = 0; b < 6; b++) {
            if (d->bar[b] != 0) {
                if (d->bar[b] & 1) {
                    printf("  BAR%d: I/O at 0x%x\n", b, d->bar[b] & ~3u);
                } else {
                    printf("  BAR%d: Memory at 0x%x\n", b, d->bar[b] & ~0xFu);
                }
            }
        }
    }

    printf("======================================================================\n");
    printf("Total devices: %d\n", count);
    return 0;
}