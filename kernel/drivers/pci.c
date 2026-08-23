#include "pci.h"
#include "../log/logger.h"

#define MAX_PCI_DEVICES 32

static pci_device_t pci_devices[MAX_PCI_DEVICES];
static uint32_t pci_device_count = 0;

static inline void outl(uint16_t port, uint32_t val) {
    asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, val);
}

const char* pci_class_to_string(uint8_t class_id, uint8_t subclass) {
    switch (class_id) {
        case 0x01:
            if (subclass == 0x06) return "SATA AHCI Storage Controller";
            if (subclass == 0x08) return "NVMe PCI Express SSD Controller";
            return "Mass Storage Controller";
        case 0x02:
            if (subclass == 0x00) return "Ethernet NIC Controller";
            if (subclass == 0x80) return "Wireless Wi-Fi Adapter";
            return "Network Controller";
        case 0x03: return "Display / GPU Graphics Controller";
        case 0x06: return "PCI Bridge / Host Controller";
        case 0x0C:
            if (subclass == 0x03) return "USB 3.0 xHCI Host Controller";
            return "Serial Bus Controller";
        default:   return "Hardware Peripheral Controller";
    }
}

void pci_init(void) {
    klog(LOG_INFO, "Scanning PCI Express Bus for Hardware Storage & Peripherals...");
    pci_device_count = 0;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t vendor_device = pci_config_read32((uint8_t)bus, slot, func, 0x00);
                uint16_t vendor_id = vendor_device & 0xFFFF;
                uint16_t device_id = (vendor_device >> 16) & 0xFFFF;

                if (vendor_id == 0xFFFF || vendor_id == 0x0000) continue;

                uint32_t class_rev = pci_config_read32((uint8_t)bus, slot, func, 0x08);
                uint8_t revision = class_rev & 0xFF;
                uint8_t prog_if  = (class_rev >> 8) & 0xFF;
                uint8_t subclass = (class_rev >> 16) & 0xFF;
                uint8_t class_id = (class_rev >> 24) & 0xFF;

                if (pci_device_count < MAX_PCI_DEVICES) {
                    pci_device_t* dev = &pci_devices[pci_device_count++];
                    dev->bus       = (uint8_t)bus;
                    dev->slot      = slot;
                    dev->func      = func;
                    dev->vendor_id = vendor_id;
                    dev->device_id = device_id;
                    dev->class_id  = class_id;
                    dev->subclass  = subclass;
                    dev->prog_if   = prog_if;
                    dev->revision  = revision;

                    dev->bar0 = pci_config_read32((uint8_t)bus, slot, func, 0x10);
                    dev->bar1 = pci_config_read32((uint8_t)bus, slot, func, 0x14);
                    dev->bar2 = pci_config_read32((uint8_t)bus, slot, func, 0x18);
                    dev->bar3 = pci_config_read32((uint8_t)bus, slot, func, 0x1C);
                    dev->bar4 = pci_config_read32((uint8_t)bus, slot, func, 0x20);
                    dev->bar5 = pci_config_read32((uint8_t)bus, slot, func, 0x24);
                }
            }
        }
    }

    klog(LOG_INFO, "PCI Express Bus Enumeration Complete");
}

uint32_t pci_get_device_count(void) {
    return pci_device_count;
}

pci_device_t* pci_get_device(uint32_t index) {
    if (index >= pci_device_count) return (void*)0;
    return &pci_devices[index];
}
