#include "hardware_probe.h"

#include "platform.h"
#include "../storage/blockdev.h"
#include "../drivers/pci.h"
#include "../log/logger.h"

static void count_pci_classes(phoxtra_hardware_report_t* report) {
    const uint32_t count = platform_device_count();
    report->pci_devices = count;
    for (uint32_t i = 0; i < count; ++i) {
        platform_device_info_t device;
        if (platform_device_get(i, &device) != PLATFORM_OK) continue;
        switch (device.class_id) {
            case 0x01: ++report->pci_storage_controllers; break;
            case 0x03: ++report->pci_display_controllers; break;
            case 0x02: ++report->pci_network_controllers; break;
            case 0x0C:
                if (device.subclass == 0x03) ++report->pci_usb_controllers;
                break;
            default: break;
        }
    }
}

static void count_block_devices(phoxtra_hardware_report_t* report) {
    report->block_devices = blockdev_count();
    for (uint32_t i = 0; i < report->block_devices; ++i) {
        block_device_t* device = blockdev_get_by_index(i);
        if (!device || (device->flags & BLOCKDEV_FLAG_PARTITION)) continue;

        if (device->flags & BLOCKDEV_FLAG_SIMULATED) {
            ++report->simulated_block_devices;
        } else {
            ++report->registered_non_simulated_block_devices;
        }
    }
}

int phoxtra_hardware_probe_collect(phoxtra_hardware_report_t* report) {
    if (!report) return PLATFORM_ERR_INVALID;

    report->architecture = "x86_64";
    report->boot_protocol = "Multiboot2";
    report->product_profile = "unknown";
    report->pci_devices = 0;
    report->pci_storage_controllers = 0;
    report->pci_display_controllers = 0;
    report->pci_network_controllers = 0;
    report->pci_usb_controllers = 0;
    report->block_devices = 0;
    report->simulated_block_devices = 0;
    report->registered_non_simulated_block_devices = 0;
    report->display_width = 0;
    report->display_height = 0;
    report->display_bits_per_pixel = 0;
    report->display_available = 0;
    report->keyboard_api_available = (uint8_t)(platform_input_has_keyboard() != 0);
    report->pointer_available = (uint8_t)(platform_input_has_pointer() != 0);
    report->persistent_vfs_available = 0;

    const platform_product_profile_t* profile = platform_product_profile();
    if (profile && profile->id) report->product_profile = profile->id;

    platform_display_info_t display;
    if (platform_display_get_info(&display) == PLATFORM_OK && display.available) {
        report->display_available = 1;
        report->display_width = display.width;
        report->display_height = display.height;
        report->display_bits_per_pixel = display.bits_per_pixel;
    }

    platform_storage_info_t storage;
    if (platform_storage_get_info(&storage) == PLATFORM_OK) {
        report->persistent_vfs_available = storage.persistent ? 1u : 0u;
    }

    count_pci_classes(report);
    count_block_devices(report);
    return PLATFORM_OK;
}

void phoxtra_hardware_probe_log(void) {
    phoxtra_hardware_report_t report;
    if (phoxtra_hardware_probe_collect(&report) != PLATFORM_OK) {
        klog(LOG_ERROR, "PHOXTRA_HWPROBE_FAILED");
        return;
    }

    klog(LOG_INFO, "PHOXTRA_HWPROBE_READY");
    klog(LOG_INFO, "Hardware probe: read-only device inventory completed");
    if (report.simulated_block_devices > 0) {
        klog(LOG_INFO, "Hardware probe: simulated RAM test storage is present");
    }
    if (report.registered_non_simulated_block_devices == 0) {
        klog(LOG_WARNING, "Hardware probe: no non-simulated block-device driver is registered");
    } else {
        klog(LOG_INFO, "Hardware probe: non-simulated block devices are registered");
    }
    if (!report.persistent_vfs_available) {
        klog(LOG_WARNING, "Hardware probe: writable persistent filesystem service is not connected");
    }
    if (report.pci_storage_controllers > 0) {
        klog(LOG_INFO, "Hardware probe: PCI storage-class controller detected; driver support is not implied");
    }
}
