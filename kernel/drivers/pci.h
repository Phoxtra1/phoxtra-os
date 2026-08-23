#ifndef PHOXTRA_PCI_H
#define PHOXTRA_PCI_H

#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

typedef struct pci_device {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_id;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  revision;
    uint32_t bar0;
    uint32_t bar1;
    uint32_t bar2;
    uint32_t bar3;
    uint32_t bar4;
    uint32_t bar5;
} pci_device_t;

#ifdef __cplusplus
extern "C" {
#endif

void pci_init(void);
uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);

uint32_t pci_get_device_count(void);
pci_device_t* pci_get_device(uint32_t index);
const char* pci_class_to_string(uint8_t class_id, uint8_t subclass);

#ifdef __cplusplus
}
#endif

#endif
