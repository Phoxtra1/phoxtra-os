#ifndef PHOXTRA_HARDWARE_PROBE_H
#define PHOXTRA_HARDWARE_PROBE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct phoxtra_hardware_report {
    const char* architecture;
    const char* boot_protocol;
    const char* product_profile;
    uint32_t pci_devices;
    uint32_t pci_storage_controllers;
    uint32_t pci_display_controllers;
    uint32_t pci_network_controllers;
    uint32_t pci_usb_controllers;
    uint32_t block_devices;
    uint32_t simulated_block_devices;
    uint32_t registered_physical_block_devices;
    uint32_t display_width;
    uint32_t display_height;
    uint32_t display_bits_per_pixel;
    uint8_t display_available;
    uint8_t keyboard_api_available;
    uint8_t pointer_available;
    uint8_t persistent_vfs_available;
} phoxtra_hardware_report_t;

/* Read-only inventory. These routines never write sectors or alter firmware. */
int phoxtra_hardware_probe_collect(phoxtra_hardware_report_t* report);
void phoxtra_hardware_probe_log(void);

#ifdef __cplusplus
}
#endif

#endif
