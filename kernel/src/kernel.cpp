#include "serial.hpp"
#include "console.hpp"
#include "init.hpp"
extern "C" {
#include "../drivers/framebuffer.h"
#include "../memory/pmm.h"
}

extern "C" {
#include "shell.h"
#include "boot_screen.h"
#include "../net/netdev.h"
#include "../net/dhcp.h"
#ifdef PHOXTRA_ENABLE_GUEST_HTTPS_SMOKE
#include "../exec/guest_https_image.h"
#endif
}

extern "C" void kernel_init(uint32_t multiboot_magic, uint32_t multiboot_info_phys)
{
    serial::init();
    serial::write("[BOOT] kernel_init reached\n");
    framebuffer_set_boot_info(multiboot_magic, multiboot_info_phys);
    pmm_set_boot_info(multiboot_magic, multiboot_info_phys);

    system_init();

    serial::write("[BOOT] System initialization complete\n");

    asm volatile("sti");

    /* Network clients poll timer/receive state, so start DHCP only after STI. */
    netdev_t* network_device = netdev_get_default();
    if (network_device && (network_device->flags & 0x01u)) {
        serial::write("[NETWORK] Supported link is up; requesting DHCP configuration\n");
        if (dhcp_request_lease(network_device) == 0) {
            serial::write("[NETWORK] PHOXTRA_NETWORK_DHCP_OK\n");
#ifdef PHOXTRA_ENABLE_GUEST_HTTPS_SMOKE
            guest_https_smoke_start();
#endif
        } else {
            serial::write("[NETWORK] PHOXTRA_NETWORK_DHCP_FAILED; use 'dhcp' to retry\n");
        }
    } else if (network_device) {
        serial::write("[NETWORK] Supported adapter present but link is down; DHCP deferred\n");
    } else {
        serial::write("[NETWORK] No supported network adapter registered; automatic network configuration unavailable\n");
    }

    shell_start();

    while (true)
    {
        asm volatile("hlt");
    }
}

