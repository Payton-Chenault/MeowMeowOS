#ifndef PCI_H
#define PCI_H

#include <stdbool.h>
#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC
#define PCI_MAX_DEVICES    32

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision_id;
    uint8_t header_type;
    uint8_t irq_line;
    uint8_t irq_pin;
    uint32_t bar[6];
} pci_device_t;

void pci_initialize(void);
uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t pci_config_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
void pci_config_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val);

uint32_t pci_get_devices(pci_device_t *out_devices, uint32_t max_count);
pci_device_t *pci_get_device(uint16_t vendor_id, uint16_t device_id);
void pci_enable_bus_mastering(pci_device_t *dev);
const char *pci_class_to_string(uint8_t class_code, uint8_t subclass);

#endif