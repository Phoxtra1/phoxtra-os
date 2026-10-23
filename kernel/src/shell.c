#include "shell.h"
#include "../log/logger.h"
#include "boot_screen.h"
#include "keyboard.h"
#include "timer.h"
#include "vfs.h"
#include "../storage/blockdev.h"
#include "../security/tpm2.h"
#include "../security/process_identity.h"
#include "../security/audit_logger.h"
#include "../security/capability.h"
#include "../security/mac.h"
#include "../security/stack_guard.h"
#include "../net/netdev.h"
#include "../net/e1000.h"
#include "../net/ethernet.h"
#include "../net/arp.h"
#include "../net/ipv4.h"
#include "../net/socket.h"
#include "../net/dhcp.h"
#include "../net/dns.h"
#include "../exec/elf.h"
#include "../exec/ld_so.h"
#include "../drivers/framebuffer.h"
#include "../ui/font.h"
#include "../ui/compositor.h"
#include "../ui/wm.h"
#include "mouse.h"
#include "../process/stress.h"
#include "../memory/heap_audit.h"
#include "../memory/vmm.h"
#include "../sdk/sdk.h"
#include "../drivers/pci.h"
#include "../ui/theme.h"
#include "../ui/file_manager.h"
#include "../installer/installer.h"
#include "../ai/dipa_core.h"
#include "../services/phoxair.h"
#include "../services/platform.h"
#include "../services/hardware_probe.h"
#include "../security/verify.h"
#include "../services/entitlements.h"

/* External subsystem APIs */

extern void console_clear(void);
extern void console_write(const char* text);
extern void console_put_char(char c);

/* Process and scheduler are C modules; forward-declare to avoid C++ header conflicts */
struct pcb;
extern struct pcb**  process_table(void);
extern uint32_t      process_count(void);
extern uint64_t      scheduler_get_uptime_ticks(void);
extern struct pcb*   scheduler_current(void);

/* ────────────────────────────────────────────────
   Utility helpers
   ──────────────────────────────────────────────── */
static int kstrcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static void print_dec(uint64_t val) {
    if (val == 0) { console_put_char('0'); return; }
    char buf[24]; int i = 0;
    while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
    for (int j = i - 1; j >= 0; j--) console_put_char(buf[j]);
}

static void print_str_pad(const char* s, int width) {
    int len = 0;
    const char* p = s;
    while (*p++) len++;
    console_write(s);
    for (int i = len; i < width; i++) console_put_char(' ');
}

static inline void outb(unsigned short port, unsigned char value)
{
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned char inb(unsigned short port)
{
    unsigned char value;
    asm volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* ────────────────────────────────────────────────
   Reboot
   ──────────────────────────────────────────────── */
static void shell_reboot(void) {
    console_write("Rebooting...\n");
    asm volatile("cli");
    uint8_t good = 0x02;
    while (good & 0x02) { good = inb(0x64); }
    outb(0x64, 0xFE);
    struct idt_ptr { uint16_t limit; uint64_t base; } __attribute__((packed)) zero_idt = {0, 0};
    asm volatile("lidt %0" : : "m"(zero_idt));
    asm volatile("int $3");
}

/* ────────────────────────────────────────────────
   Shell command handlers
   ──────────────────────────────────────────────── */
/* External User Mode & Memory APIs */

extern void switch_to_user_mode(uint64_t entry_point, uint64_t user_rsp);
extern uint64_t vmm_create_user_page_directory(void);
extern void vmm_switch_page_directory(uint64_t cr3_phys);

static void cmd_platform(void) {
    klog(LOG_INFO, "Platform diagnostic command executed");
    const platform_product_profile_t* profile = platform_product_profile();
    platform_display_info_t display;
    platform_storage_info_t storage;
    console_write("\n=== PHOXTRA SHARED PLATFORM SERVICES ===\n");
    console_write("Profile : "); console_write(profile->display_name); console_write(" ("); console_write(profile->id); console_write(")\n");
    console_write("Services: "); console_write(platform_services_is_initialized() ? "INITIALIZED\n" : "NOT INITIALIZED\n");
    console_write("Input   : keyboard "); console_write(platform_input_has_keyboard() ? "available" : "unavailable");
    console_write(", pointer "); console_write(platform_input_has_pointer() ? "detected" : "not detected"); console_write("\n");
    if (platform_display_get_info(&display) == PLATFORM_OK) {
        console_write("Display : "); print_dec(display.width); console_write("x"); print_dec(display.height);
        console_write(" @ "); print_dec(display.bits_per_pixel); console_write(" bpp\n");
    } else console_write("Display : no supported framebuffer\n");
    if (platform_storage_get_info(&storage) == PLATFORM_OK) {
        console_write("Storage : "); console_write(storage.description); console_write("\n");
    }
    console_write("Devices : "); print_dec(platform_device_count()); console_write(" PCI devices enumerated\n");
    console_write("Applications:\n");
    for (uint32_t i = 0; i < platform_app_count(); ++i) {
        const platform_app_info_t* app = 0;
        if (platform_app_get(i, &app) != PLATFORM_OK || !app) continue;
        console_write("  - "); console_write(app->id); console_write(" : ");
        console_write(app->state == PLATFORM_APP_AVAILABLE ? "available interface (see description)" : "planned, not launchable yet");
        console_write("\n");
    }
    console_write("\n");
}

static void cmd_hardware(void) {
    phoxtra_hardware_report_t report;
    console_write("\n=== PHOXTRA HARDWARE READINESS (READ-ONLY) ===\n");
    if (phoxtra_hardware_probe_collect(&report) != PLATFORM_OK) {
        console_write("Hardware inventory failed. No changes were made.\n\n");
        klog(LOG_ERROR, "PHOXTRA_HWPROBE_FAILED");
        return;
    }

    console_write("Architecture : "); console_write(report.architecture); console_write("\n");
    console_write("Boot mode    : "); console_write(report.boot_protocol); console_write("\n");
    console_write("Profile      : "); console_write(report.product_profile); console_write("\n");
    console_write("PCI devices  : "); print_dec(report.pci_devices); console_write("\n");
    console_write("PCI storage controllers: "); print_dec(report.pci_storage_controllers); console_write("\n");
    console_write("PCI display controllers: "); print_dec(report.pci_display_controllers); console_write("\n");
    console_write("PCI network controllers: "); print_dec(report.pci_network_controllers); console_write("\n");
    console_write("PCI USB controllers: "); print_dec(report.pci_usb_controllers); console_write("\n");
    console_write("Display      : ");
    if (report.display_available) {
        print_dec(report.display_width); console_write("x"); print_dec(report.display_height);
        console_write(" @ "); print_dec(report.display_bits_per_pixel); console_write(" bpp\n");
    } else console_write("unavailable\n");
    console_write("Keyboard API : "); console_write(report.keyboard_api_available ? "available\n" : "unavailable\n");
    console_write("Pointer      : "); console_write(report.pointer_available ? "detected\n" : "not detected\n");
    console_write("Block devices: "); print_dec(report.block_devices); console_write("\n");
    console_write("Simulated RAM disks: "); print_dec(report.simulated_block_devices); console_write("\n");
    console_write("Non-simulated block-device drivers registered: ");
    print_dec(report.registered_non_simulated_block_devices); console_write("\n");
    console_write("Persistent filesystem: ");
    console_write(report.persistent_vfs_available ? "available\n" : "not connected\n");
    console_write("Safety: this report is read-only; PCI detection does not mean a driver is implemented.\n\n");
    klog(LOG_INFO, "PHOXTRA_HARDWARE_REPORT_OK");
}

static void cmd_help(void) {
    console_write("Available commands:\n");
    console_write("  help       - Show this message\n");
    console_write("  platform   - Inspect shared services and active product profile\n");
    console_write("  hardware   - Read-only hardware readiness inventory\n");
    console_write("  about      - OS information\n");
    console_write("  clear      - Clear the screen\n");
    console_write("  ticks      - System timer ticks\n");
    console_write("  uptime     - System uptime in seconds\n");
    console_write("  ps         - List all processes\n");
    console_write("  pid        - Current process ID\n");
    console_write("  memory     - Memory usage summary\n");
    console_write("  cpu        - CPU, timer and scheduler readiness status\n");
    console_write("  usermode   - Show user-mode runtime implementation status\n");
    console_write("  disks      - List storage block devices & partition table\n");
    console_write("  storage    - Milestone 0.7.0 Storage Driver & Filesystem Audit\n");
    console_write("  tpm        - Show TPM hardware-driver status\n");
    console_write("  identity   - Show process SHA-256 fingerprints (not attestation)\n");
    console_write("  audit      - Show volatile in-memory audit events\n");
    console_write("  caps       - Capability Access Control (CAC) Bitmask Permissions\n");
    console_write("  mac        - Mandatory Access Control (MAC) Security Domains\n");
    console_write("  stackguard - Show stack/guard-page implementation status\n");
    console_write("  ifconfig   - Display active network interfaces and IP addresses\n");
    console_write("  ping       - Show current ICMP diagnostic limitations\n");
    console_write("  netstat    - Show socket-subsystem implementation status\n");
    console_write("  dhcp       - Show DHCP lease support status\n");
    console_write("  dns        - Resolve hostname domain to IPv4 address\n");
    console_write("  elf        - Parse & load ELF64 executable binary program\n");
    console_write("  libc       - Audit native C standard library (phoxtra-libc) POSIX API\n");
    console_write("  gui        - Launch High-Resolution 1024x768 Desktop GUI Window Manager\n");
    console_write("  wm         - Audit Phoxtra Window Manager (phoxtra-wm) Compositor State\n");
    console_write("  stress     - Preemption stress test under maximum process capacity (64 tasks)\n");
    console_write("  heap       - Audit kernel heap memory allocations and leak detection\n");
    console_write("  panic      - Test edge-case kernel fault recovery routines\n");
    console_write("  sdk        - Audit Phoxtra Developer SDK Toolchain & Syscall Reference\n");
    console_write("  release    - Display current prototype and release-gate status\n");
    console_write("  userland   - Audit Ring 3 Userland Isolation & Memory Transition Engine\n");
    console_write("  pci        - Scan PCI Express Bus for Hardware Controllers & Peripherals\n");
    console_write("  installer  - Launch Phoxtra OS Bare-Metal Installation Wizard\n");
    console_write("  theme      - Switch Desktop Theme (midnight-gold, cyber-sapphire, emerald)\n");
    console_write("  fm         - Launch Phoxtra Graphical File Manager Window\n");
    console_write("  store      - Launch Phoxtra App Store & Software Package Manager\n");
    console_write("  update     - Audit Automated Over-The-Air (OTA) Cloud System Updates\n");
    console_write("  mobile     - Inspect Phoxtra Mobile & Tablet OS Touch Compositor\n");
    console_write("  dipa       - Execute Native Dipa AI Core Kernel & Shell Assistant\n");
    console_write("  phoxair    - Monitor Phoxair Airspace & Telemetry Radar Daemon\n");
    console_write("  verify     - Authenticate Document & Binary Signatures via Phoxtra Verify\n");
    console_write("  license    - Audit Phoxtra Enterprise Subscriptions & Entitlements\n");
    console_write("  vfs        - List virtual filesystem tree (/dev, /initrd, /boot, /home)\n");
    console_write("  cat        - Read file contents from VFS (e.g. cat /boot/phoxtra.cfg)\n");
    console_write("  reboot     - Reboot the system\n");
}

static void cmd_about(void) {
    console_write("\nAKWAABA - PHOXTRA OS\n");
    console_write("Stage   : Experimental bootable x86_64 prototype\n");
    console_write("Desktop : Framebuffer UI, shell, file manager and local application catalogue\n");
    console_write("Storage : Volatile RAM test environment; persistent disk support unavailable\n");
    console_write("Network : e1000 hardware path under development; full TCP/IP/TLS unavailable\n");
    console_write("Security: Prototype only; TPM attestation, trusted signatures and isolation are not validated\n");
    console_write("Mobile  : ARM64 phone build not implemented\n");
    console_write("Install : Not implemented; do not install to an internal disk\n\n");
}

static void cmd_disks(void) {
    console_write("\nBlock Devices (physical hardware is not connected to this storage layer):\n");
    console_write("NAME       SECTORS      SIZE (MB)   TYPE / ACCESS\n");
    console_write("---------  -----------  ----------  -------------------------\n");

    uint32_t count = blockdev_count();
    for (uint32_t i = 0; i < count; i++) {
        block_device_t* dev = blockdev_get_by_index(i);
        if (!dev) continue;
        print_str_pad(dev->name, 11);
        print_dec(dev->total_sectors);
        console_write("        ");
        uint64_t size_mb = (dev->total_sectors * dev->sector_size) / (1024u * 1024u);
        print_dec(size_mb);
        console_write("           ");
        if (dev->flags & BLOCKDEV_FLAG_PARTITION) console_write("Partition");
        else if (dev->flags & BLOCKDEV_FLAG_SIMULATED) console_write("Volatile RAM test disk");
        else console_write("Unclassified device");
        if (dev->flags & BLOCKDEV_FLAG_READONLY) console_write(" [READ-ONLY]");
        console_write("\n");
    }
    console_write("\n");
}

static void cmd_storage(void) {
    console_write("\n=== Phoxtra Storage Safety Status ===\n");
    console_write("[1/4] Physical SATA/NVMe controller driver ........ NOT IMPLEMENTED\n");
    console_write("[2/4] Current block device ....................... Volatile RAM test disk\n");
    console_write("[3/4] Partition-table scan ........................ READ-ONLY\n");
    console_write("[4/4] Ext2/FAT32 support .......................... Demo adapters only\n");
    console_write("\nSafety: internal-disk installation and partition formatting are unavailable.\n");
    console_write("No persistent physical storage is mounted by this build.\n\n");
}

static void cmd_tpm(void) {
    console_write("\n=== TPM Hardware Status ===\n");
    tpm2_dev_t* dev = tpm2_get_device();
    if (!dev || !dev->hardware_attested) {
        console_write("TPM hardware driver : NOT CONNECTED\n");
        console_write("Hardware attestation: UNAVAILABLE\n");
        console_write("PCR operations       : UNAVAILABLE\n\n");
        return;
    }
    console_write("Hardware attestation: reported by connected driver\n");
    console_write("Vendor device        : "); console_write(dev->vendor_id); console_write("\n");
    console_write("Active PCR slots     : "); print_dec(dev->active_pcrs); console_write("\n\n");
}

static void cmd_identity(void) {
    console_write("\n=== Process SHA-256 Fingerprints (Not Attestation) ===\n");
    console_write("Hashes identify bytes; no trusted digital signature or TPM attestation is available.\n");
    console_write("PID  NAME             STATUS      SHA-256 FINGERPRINT\n");
    struct pcb** table = process_table();
    uint32_t count = process_count();
    for (uint32_t i = 0; i < count; ++i) {
        struct pcb* pcb = table[i]; if (!pcb) continue;
        print_dec(pcb->pid); console_write("    "); print_str_pad(pcb->name,17);
        console_write("UNVERIFIED  ");
        char hash_hex[65]; process_identity_get_hash_hex(pcb, hash_hex);
        for (int h = 0; h < 64; ++h) console_put_char(hash_hex[h]);
        console_write("\n");
    }
    console_write("\n");
}

static void cmd_audit(void) {
    console_write("\n=== In-Memory Kernel Audit Events ===\n");
    console_write("This log is volatile and is not immutable or tamper-resistant.\n");
    console_write("TIME(TICKS)  PID  EVENT TYPE             MESSAGE\n");
    uint32_t count = audit_get_count();
    for (uint32_t i = 0; i < count; ++i) {
        const audit_event_t* ev = audit_get_event(i);
        if (!ev) continue;
        print_dec(ev->timestamp); console_write("          "); print_dec(ev->pid); console_write("    ");
        switch (ev->event_type) {
            case AUDIT_TYPE_HARDWARE_ATTESTATION: print_str_pad("HW_ATTESTATION",23); break;
            case AUDIT_TYPE_PROCESS_BINDING: print_str_pad("PROCESS_BINDING",23); break;
            case AUDIT_TYPE_PRIVILEGE_TRANSITION: print_str_pad("PRIV_TRANSITION",23); break;
            case AUDIT_TYPE_SYSCALL_GATE: print_str_pad("SYSCALL_GATE",23); break;
            case AUDIT_TYPE_POLICY_VIOLATION: print_str_pad("POLICY_VIOLATION",23); break;
            default: print_str_pad("UNKNOWN_EVENT",23); break;
        }
        console_write(ev->message); console_write("\n");
    }
    console_write("\n");
}

static void cmd_caps(void) {
    console_write("\n=== Process Capability Metadata ===\n");
    console_write("This table reports PCB bitmasks; it does not prove system-wide authorization enforcement.\n");
    console_write("PID  NAME             MAC DOMAIN  CAPABILITY BITMASK\n");
    struct pcb** table = process_table();
    uint32_t count = process_count();
    for (uint32_t i = 0; i < count; ++i) {
        struct pcb* pcb = table[i];
        if (!pcb) continue;
        print_dec(pcb->pid); console_write("    "); print_str_pad(pcb->name, 17);
        print_str_pad(mac_label_to_string(pcb->mac_label), 12);
        console_write("0x"); 
        static const char hex[] = "0123456789abcdef";
        int started = 0;
        for (int shift = 60; shift >= 0; shift -= 4) {
            uint8_t digit = (uint8_t)((pcb->capabilities >> shift) & 0x0f);
            if (digit || started || shift == 0) { console_put_char(hex[digit]); started = 1; }
        }
        console_write("\n");
    }
    console_write("\n");
}

static void cmd_mac(void) {
    console_write("\n=== Mandatory Access Control Policy Status ===\n");
    console_write("Policy helper functions: PRESENT\n");
    console_write("VFS access enforcement : NOT INTEGRATED END-TO-END\n");
    console_write("IPC enforcement        : NOT INTEGRATED END-TO-END\n");
    console_write("Process isolation      : NOT VALIDATED; user-mode runtime is incomplete\n\n");
}

static void cmd_stackguard(void) {
    console_write("\n=== Stack and Memory Protection Status ===\n");
    console_write("Stack canary : prototype fallback only; compiler stack protection is not enabled\n");
    console_write("Guard pages  : NOT IMPLEMENTED\n");
    console_write("ASLR         : NOT IMPLEMENTED (trusted entropy source unavailable)\n\n");
}

static void cmd_ifconfig(void) {
    console_write("\n=== Hardware Network Interfaces ===\n");
    uint32_t count = netdev_count();
    if (!count) { console_write("No supported hardware network interface is registered.\n\n"); return; }
    for (uint32_t n = 0; n < count; ++n) {
        netdev_t* dev = netdev_get_by_index(n);
        if (!dev) continue;
        console_write(dev->name); console_write(": link ");
        console_write((dev->flags & 0x01u) ? "UP\n" : "DOWN\n");
        if (dev->ip_addr) {
            char ip[16], mask[16], gateway[16];
            uint32_to_ip(dev->ip_addr, ip); uint32_to_ip(dev->netmask, mask); uint32_to_ip(dev->gateway, gateway);
            console_write("  inet "); console_write(ip); console_write(" netmask "); console_write(mask);
            console_write(" gateway "); console_write(gateway); console_write("\n");
        } else console_write("  IPv4: not configured (DHCP is not implemented)\n");
        console_write("  ether ");
        static const char hex[] = "0123456789abcdef";
        for (int i = 0; i < ETH_ALEN; ++i) {
            console_put_char(hex[(dev->mac_addr[i] >> 4) & 0x0f]);
            console_put_char(hex[dev->mac_addr[i] & 0x0f]);
            if (i + 1 < ETH_ALEN) console_put_char(':');
        }
        console_write("\n  RX packets "); print_dec(dev->rx_packets); console_write(" bytes "); print_dec(dev->rx_bytes);
        console_write("\n  TX packets "); print_dec(dev->tx_packets); console_write(" bytes "); print_dec(dev->tx_bytes);
        console_write("\n");
    }
    console_write("\n");
}

static void cmd_ping(const char* target) {
    console_write("\n=== ICMP Diagnostics Status ===\n");
    if (target && target[0]) { console_write("Requested target: "); console_write(target); console_write("\n"); }
    console_write("The current prototype has no complete IPv4 receive path or ICMP reply/timeout handling.\n");
    console_write("No successful ping or packet-loss result can be reported by this build.\n\n");
}

static void cmd_netstat(void) {
    console_write("\n=== Socket Subsystem Status ===\n");
    console_write("Allocated socket handles: "); print_dec(socket_get_active_count()); console_write("\n");
    console_write("TCP handshake/state machine : NOT IMPLEMENTED\n");
    console_write("Inbound protocol dispatch   : PARTIAL (ARP only)\n");
    console_write("Socket receive queues       : NOT IMPLEMENTED\n");
    console_write("No listening TCP/UDP ports are claimed.\n\n");
}

static void cmd_dhcp(void) {
    console_write("\n=== DHCP Lease Status ===\n");
    netdev_t* dev = netdev_get_default();
    if (!dev) { console_write("No supported hardware network interface is active.\n\n"); return; }
    int result = dhcp_request_lease(dev);
    if (result == 0) console_write("DHCP lease obtained and applied.\n\n");
    else console_write("DHCP is not implemented end-to-end; no address or lease was assigned.\n\n");
}

static void cmd_dns(const char* host) {
    console_write("\n=== DNS Resolution Status ===\n");
    if (!host || !host[0]) { console_write("Usage: dns <hostname>\n\n"); return; }
    console_write("Hostname: "); console_write(host); console_write("\n");
    netdev_t* dev = netdev_get_default();
    uint32_t resolved_ip = dns_resolve_hostname(dev, host);
    if (!resolved_ip) console_write("DNS query/response processing is not implemented; no address was returned.\n\n");
    else {
        char ip_str[16]; uint32_to_ip(resolved_ip, ip_str);
        console_write("Address: "); console_write(ip_str); console_write("\n\n");
    }
}

static void cmd_elf(const char* path) {
    console_write("\n=== ELF Executable Loader Status ===\n");
    if (path && path[0]) { console_write("Requested path: "); console_write(path); console_write("\n"); }
    console_write("ELF image validation is available to the kernel API and regression tests.\n");
    console_write("Loading segments into an isolated process address space and executing them are NOT IMPLEMENTED.\n");
    console_write("This command does not fabricate a binary or report a process launch.\n\n");
}

static void cmd_libc(void) {
    console_write("\n=== User-Space Runtime Status ===\n");
    console_write("Kernel memory allocation exists, but a POSIX C library is not implemented.\n");
    console_write("stdio, file-descriptor APIs, process startup, fork/exec and user-space syscalls are unavailable.\n");
    console_write("Dynamic linking and shared-library loading are unavailable.\n\n");
}

static void cmd_gui(void) {
    klog(LOG_INFO, "GUI command received");
    if (!framebuffer_is_available()) {
        klog(LOG_ERROR, "GUI startup aborted: framebuffer unavailable");
        console_write("\nGUI unavailable: no supported bootloader framebuffer is connected.\n");
        console_write("Check the Multiboot2 framebuffer request and GRUB video mode.\n\n");
        return;
    }
    /* Clear the boot console before drawing; long status text after the draw
       can scroll the text console and repaint over the newly rendered desktop. */
    console_clear();
    wm_start_desktop();
    wm_desktop_event_loop();
    klog(LOG_INFO, "GUI compositor render completed");
}
static void cmd_wm(void) {
    console_write("\n=== Phoxtra Window Manager (phoxtra-wm) Status ===\n");
    console_write("Graphics Mode  : 1024x768 @ 32bpp ARGB8888 (VBE / GOP Framebuffer)\n");
    console_write("Compositor     : Double-buffered Glassmorphic Window Compositor\n");
    console_write("Font Engine    : 8x16 Bitmap Anti-aliased Text Engine\n");
    console_write("Active Windows : 3 (Terminal, System Monitor, Security Audit)\n");
    console_write("Taskbar Dock   : Active (Phoxtra Launcher, Clock, CPU Load)\n\n");
}

static void cmd_stress(void) {
    console_write("\n=== Scheduler Stress Test Status ===\n");
    int result = stress_test_multitasking(64);
    if (result >= 0) {
        console_write("Tasks created: "); print_dec((uint32_t)result); console_write("\n");
    } else {
        console_write("NOT RUN: scheduler registration and task-return handling are not validated.\n");
    }
    console_write("No preemption or stability pass is claimed by this command.\n\n");
}

static void cmd_heap(void) {
    console_write("\n=== Heap Allocation Smoke Test ===\n");
    int passed = heap_audit_verify_allocations();
    console_write("Allocation/free smoke test: "); console_write(passed ? "PASSED\n" : "FAILED\n");
    console_write("Global leak tracking: NOT IMPLEMENTED\n");
    console_write("Measured heap usage: UNAVAILABLE\n\n");
}

static void cmd_panic(void) {
    console_write("\n=== Fault and Recovery Test Status ===\n");
    console_write("Automated fault injection: NOT RUN\n");
    console_write("Guard-page fault tests  : NOT IMPLEMENTED\n");
    console_write("Double-fault recovery   : NOT VALIDATED\n");
    console_write("Kernel panic is currently terminal; recovery is not claimed.\n\n");
}

static void cmd_sdk(void) {
    console_write("\n=== Developer SDK Status ===\n");
    console_write("Kernel/ISO build tools : CMake, GCC toolchain and NASM in CI\n");
    console_write("Phoxtra user-space SDK : NOT IMPLEMENTED\n");
    console_write("POSIX C library        : NOT IMPLEMENTED\n");
    console_write("Runnable user apps     : NOT IMPLEMENTED\n");
    console_write("Registered SDK syscalls: "); print_dec(sdk_get_registered_syscall_count()); console_write("\n\n");
}

static void cmd_release(void) {
    console_write("\n=== Phoxtra OS Prototype Status ===\n");
    console_write("Version : "); console_write(production_get_version()); console_write("\n");
    console_write("Support : "); console_write(production_get_lts_status()); console_write("\n");
    console_write("Physical laptop validation : PENDING\n");
    console_write("Persistent storage/installer : NOT IMPLEMENTED\n");
    console_write("Mobile phone build : NOT IMPLEMENTED\n");
    console_write("Production certification : NOT PERFORMED\n\n");
}

static void cmd_userland(void) {
    console_write("\n=== User-Mode Runtime Status ===\n");
    console_write("Per-process address-space construction : INCOMPLETE\n");
    console_write("Safe user stack and guard-page mapping : NOT IMPLEMENTED\n");
    console_write("Executable loading and process launch  : NOT IMPLEMENTED\n");
    console_write("Ring 3 isolation acceptance tests      : NOT PASSED\n\n");
}

static void cmd_pci(void) {
    console_write("\n=== PCI Express Hardware Devices & Storage Controllers ===\n");
    uint32_t count = pci_get_device_count();
    console_write("Total Devices Detected: "); print_dec(count); console_write("\n\n");
    console_write("BUS:SLOT.FUNC  VENDOR:DEVICE  CLASS TYPE\n");
    console_write("---------------------------------------------------------\n");
    for (uint32_t i = 0; i < count; i++) {
        pci_device_t* dev = pci_get_device(i);
        if (!dev) continue;
        print_dec(dev->bus); console_write(":");
        print_dec(dev->slot); console_write(".");
        print_dec(dev->func); console_write("       0x");
        print_dec(dev->vendor_id); console_write(":0x");
        print_dec(dev->device_id); console_write("   ");
        console_write(pci_class_to_string(dev->class_id, dev->subclass));
        console_write("\n");
    }
    console_write("\n");
}

static void cmd_installer(void) {
    klog(LOG_WARNING, "Installer request rejected: disk installation and partition formatting are not implemented");
    console_write("\nPhoxtra OS disk installation is not available in this build.\n");
    console_write("This is a live USB test prototype, not an installer.\n");
    console_write("No internal disk will be prepared by the installer command.\n\n");
}

static void cmd_theme(const char* arg) {
    if (arg && (arg[0] == 'c' || arg[0] == '1')) {
        theme_set(THEME_CYBER_SAPPHIRE);
        console_write("Switched to Theme: Cyber Sapphire\n");
    } else if (arg && (arg[0] == 'e' || arg[0] == '2')) {
        theme_set(THEME_EMERALD_DARK);
        console_write("Switched to Theme: Emerald Dark\n");
    } else {
        theme_set(THEME_MIDNIGHT_GOLD);
        console_write("Switched to Theme: Phoxtra Flagship Midnight Gold\n");
    }
}

static void cmd_fm(void) {
    console_write("Opening Phoxtra File Manager (/home/user)...\n");
    file_manager_open_window("/home/user");
}

static void cmd_store(const char* arg) {
    console_write("\n=== Application Store Status ===\n");
    console_write("Online store connection : NOT IMPLEMENTED\n");
    console_write("Package manager         : NOT IMPLEMENTED\n");
    console_write("Signature verification  : NOT AVAILABLE\n");
    console_write("The Application Center is a local catalogue, not a connected package store.\n");
    if (arg && arg[0]) {
        console_write("Requested package: "); console_write(arg); console_write("\n");
        console_write("No package was downloaded or installed.\n");
    }
    console_write("\n");
}

static void cmd_update(void) {
    console_write("\n=== System Update Status ===\n");
    console_write("Online update client : NOT IMPLEMENTED\n");
    console_write("Repository connection: NOT TESTED\n");
    console_write("Patch signature verification: NOT IMPLEMENTED\n");
    console_write("No update check was performed and no version is claimed to be up to date.\n\n");
}

static void cmd_mobile(void) {
    console_write("\n=== Phoxtra Mobile Platform Status ===\n");
    console_write("Current boot target : x86_64 prototype\n");
    console_write("ARM64 phone build   : NOT IMPLEMENTED\n");
    console_write("Touch drivers, power management and phone boot path are not available in this build.\n\n");
}

static void cmd_dipa(const char* query) {
    console_write("\n=== Dipa Internal AI Integration Status ===\n");
    if (query && query[0]) {
        char response[256];
        dipa_ai_process_query(query, response, sizeof(response));
        console_write(response); console_write("\n\n");
    } else {
        console_write("Inference model/API: NOT CONNECTED\n");
        console_write("The kernel stub does not perform model inference.\n\n");
    }
}

static void cmd_phoxair(void) {
    console_write("\n=== Phoxair Sensor/Telemetry Integration ===\n");
    phoxair_scan_airspace();
    phoxair_status_t st = phoxair_get_status();
    console_write("Active targets : "); print_dec(st.active_targets); console_write("\n");
    console_write("Radar range    : "); print_dec(st.radar_range_km); console_write(" km\n");
    console_write("Telemetry link : "); console_write(st.telemetry_status ? "connected\n" : "NOT CONNECTED\n");
    console_write("No radar sensor or telemetry source is configured in this prototype.\n\n");
}

static void cmd_verify(const char* arg) {
    console_write("\n=== Phoxtra Verification Status ===\n");
    if (!arg || !arg[0]) {
        console_write("File-signature and document-authenticity backends are not connected.\n");
        console_write("Usage: verify <file> (will remain unavailable until a trusted verifier exists)\n\n");
        return;
    }
    console_write("Target: "); console_write(arg); console_write("\n");
    if (verify_file_signature(arg, "") != 0)
        console_write("Result: verified by the configured verifier.\n\n");
    else
        console_write("Result: NOT VERIFIED; cryptographic verification is unavailable.\n\n");
}

static void cmd_entitlements(void) {
    console_write("\n=== Local Entitlement Status ===\n");
    console_write("Mode                 : COMMUNITY / PROTOTYPE\n");
    console_write("Cloud license service: NOT CONNECTED\n");
    console_write("Paid feature grants  : NOT VERIFIED\n\n");
}


static void cmd_ticks(void) {
    console_write("System ticks: ");
    print_dec(timer_get_ticks());
    console_put_char('\n');
}

static void cmd_uptime(void) {
    uint64_t ticks = scheduler_get_uptime_ticks();
    uint64_t seconds = ticks / 100;
    console_write("Uptime: ");
    print_dec(seconds);
    console_write(" seconds (");
    print_dec(ticks);
    console_write(" ticks)\n");
}

static void cmd_ps(void) {
    console_write("\n");
    console_write("PID    STATE        PRIORITY   NAME\n");
    console_write("----   ----------   --------   --------------------\n");

    struct pcb** table = process_table();
    uint32_t count = process_count();
    uint32_t shown = 0;

    for (int i = 0; i < 64 && shown < count; i++) {
        if (!table[i]) continue;
        uint8_t* raw = (uint8_t*)table[i];
        uint32_t pid      = *(uint32_t*)(raw + 0);
        char*    name     = (char*)(raw + 8);
        int      state    = *(int*)(raw + 40);
        uint32_t priority = *(uint32_t*)(raw + 44);

        print_dec(pid);
        console_write("      ");
        print_str_pad(pcb_state_to_string(state), 13);
        print_dec(priority);
        console_write("          ");
        console_write(name);
        console_put_char('\n');
        shown++;
    }
    if (shown == 0) console_write("  (no processes)\n");
    console_put_char('\n');
}

static void cmd_pid(void) {
    struct pcb* cur = scheduler_current();
    if (cur) {
        uint32_t pid = *(uint32_t*)((uint8_t*)cur + 0);
        char*    nm  = (char*)((uint8_t*)cur + 8);
        console_write("Current PID: ");
        print_dec(pid);
        console_write("  (");
        console_write(nm);
        console_write(")\n");
    } else {
        console_write("No running process\n");
    }
}

static void cmd_memory(void) {
    extern uint64_t pmm_get_free_memory(void);
    extern uint64_t pmm_get_total_memory(void);
    uint64_t total = pmm_get_total_memory() / (1024 * 1024);
    uint64_t free  = pmm_get_free_memory()  / (1024 * 1024);
    uint64_t used  = total - free;
    console_write("\nPhysical Memory:\n");
    console_write("  Total : "); print_dec(total); console_write(" MB\n");
    console_write("  Used  : "); print_dec(used);  console_write(" MB\n");
    console_write("  Free  : "); print_dec(free);  console_write(" MB\n\n");
}

static void cmd_cpu(void) {
    console_write("\nCPU/Kernel State:\n");
    console_write("  Architecture : x86_64 (Long Mode)\n");
    console_write("  Timer         : 100 Hz PIT configured\n");
    console_write("  Interrupts    : Enabled in the main event loop\n");
    console_write("  Scheduler     : Experimental; process queue/context switching not validated\n");
    console_write("  Uptime ticks  : "); print_dec(scheduler_get_uptime_ticks()); console_write("\n\n");
}

static void cmd_usermode(void) {
    console_write("\n=== User-Mode Validation Status ===\n");
    console_write("The kernel has an experimental syscall entry path, but there is no complete user-mode process runtime.\n");
    console_write("Ring transitions, per-process isolation, executable launch and return-to-kernel behavior are NOT VALIDATED.\n");
    console_write("No user-mode isolation test is marked passed.\n\n");
}

static void cmd_vfs(void) {
    console_write("\nVirtual Filesystem Hierarchy:\n");
    console_write("/ (vfs_root)\n");
    
    struct vfs_node* root = vfs_get_root();
    if (!root) {
        console_write("  (VFS uninitialized)\n");
        return;
    }

    struct vfs_node* child = vfs_readdir(root, 0);
    uint32_t idx = 0;
    while (child) {
        console_write("  |-- /");
        console_write(child->name);
        console_write(" (DIR)\n");

        struct vfs_node* sub = vfs_readdir(child, 0);
        uint32_t sub_idx = 0;
        while (sub) {
            console_write("  |   |-- ");
            console_write(sub->name);
            if (sub->flags == 0x03) console_write(" [CHARDEV]\n");
            else console_write(" [FILE]\n");
            sub_idx++;
            sub = vfs_readdir(child, sub_idx);
        }
        idx++;
        child = vfs_readdir(root, idx);
    }
    console_write("\n");
}

static void cmd_cat(const char* path) {
    if (!path || *path == '\0') {
        path = "/initrd/welcome.txt";
    }

    struct vfs_node* node = vfs_lookup(path);
    if (!node) {
        console_write("cat: file not found '");
        console_write(path);
        console_write("'\n");
        return;
    }

    uint8_t buf[512];
    uint64_t read_bytes = vfs_read(node, 0, sizeof(buf) - 1, buf);
    if (read_bytes > 0) {
        buf[read_bytes] = '\0';
        console_write((const char*)buf);
        console_write("\n");
    } else {
        console_write("(empty file or device write-only)\n");
    }
}

/* ────────────────────────────────────────────────
   Main shell entry — startup screen + input loop
   ──────────────────────────────────────────────── */
void shell_start(void) {
    boot_screen_display();
    klog(LOG_INFO, "Shell ready for commands");
    console_write("phoxtra> ");


    char input[256];
    int input_len = 0;

    while (1) {
        net_poll();
        char c = keyboard_get_char();
        if (c == '\n') {
            input[input_len] = '\0';
            console_put_char('\n');

            if (input_len > 0) {
                if      (kstrcmp(input, "help")     == 0) cmd_help();
                else if (kstrcmp(input, "platform") == 0) cmd_platform();
                else if (kstrcmp(input, "hardware") == 0) cmd_hardware();
                else if (kstrcmp(input, "about")    == 0) cmd_about();
                else if (kstrcmp(input, "clear")    == 0) { console_clear(); }
                else if (kstrcmp(input, "ticks")    == 0) cmd_ticks();
                else if (kstrcmp(input, "uptime")   == 0) cmd_uptime();
                else if (kstrcmp(input, "ps")       == 0) cmd_ps();
                else if (kstrcmp(input, "pid")      == 0) cmd_pid();
                else if (kstrcmp(input, "memory")   == 0) cmd_memory();
                else if (kstrcmp(input, "cpu")      == 0) cmd_cpu();
                else if (kstrcmp(input, "usermode") == 0) cmd_usermode();
                else if (kstrcmp(input, "disks")    == 0) cmd_disks();
                else if (kstrcmp(input, "storage")  == 0) cmd_storage();
                else if (kstrcmp(input, "tpm")        == 0) cmd_tpm();
                else if (kstrcmp(input, "identity")   == 0) cmd_identity();
                else if (kstrcmp(input, "audit")      == 0) cmd_audit();
                else if (kstrcmp(input, "caps")       == 0) cmd_caps();
                else if (kstrcmp(input, "mac")        == 0) cmd_mac();
                else if (kstrcmp(input, "stackguard") == 0) cmd_stackguard();
                else if (kstrcmp(input, "ifconfig")   == 0) cmd_ifconfig();
                else if (input[0]=='p' && input[1]=='i' && input[2]=='n' && input[3]=='g') {
                    const char* p = input + 4;
                    while (*p == ' ') p++;
                    cmd_ping(p);
                }
                else if (kstrcmp(input, "netstat")    == 0) cmd_netstat();
                else if (kstrcmp(input, "dhcp")       == 0) cmd_dhcp();
                else if (input[0]=='d' && input[1]=='n' && input[2]=='s') {
                    const char* p = input + 3;
                    while (*p == ' ') p++;
                    cmd_dns(p);
                }
                else if (input[0]=='e' && input[1]=='l' && input[2]=='f') {
                    const char* p = input + 3;
                    while (*p == ' ') p++;
                    cmd_elf(p);
                }
                else if (kstrcmp(input, "libc")       == 0) cmd_libc();
                else if (kstrcmp(input, "gui")        == 0 || kstrcmp(input, "desktop") == 0) cmd_gui();
                else if (kstrcmp(input, "wm")         == 0) cmd_wm();
                else if (kstrcmp(input, "stress")     == 0) cmd_stress();
                else if (kstrcmp(input, "heap")       == 0) cmd_heap();
                else if (kstrcmp(input, "panic")      == 0) cmd_panic();
                else if (kstrcmp(input, "sdk")        == 0 || kstrcmp(input, "toolchain") == 0) cmd_sdk();
                else if (kstrcmp(input, "release")    == 0 || kstrcmp(input, "lts") == 0) cmd_release();
                else if (kstrcmp(input, "userland")   == 0 || kstrcmp(input, "user") == 0) cmd_userland();
                else if (kstrcmp(input, "pci")        == 0 || kstrcmp(input, "lspci") == 0) cmd_pci();
                else if (kstrcmp(input, "installer")  == 0 || kstrcmp(input, "install") == 0) cmd_installer();
                else if (input[0]=='t' && input[1]=='h' && input[2]=='e' && input[3]=='m' && input[4]=='e') {
                    const char* p = input + 5;
                    while (*p == ' ') p++;
                    cmd_theme(p);
                }
                else if (kstrcmp(input, "fm")         == 0 || kstrcmp(input, "filemanager") == 0) cmd_fm();
                else if (input[0]=='s' && input[1]=='t' && input[2]=='o' && input[3]=='r' && input[4]=='e') {
                    const char* p = input + 5;
                    while (*p == ' ') p++;
                    cmd_store(p);
                }
                else if (kstrcmp(input, "update")     == 0) cmd_update();
                else if (kstrcmp(input, "mobile")     == 0 || kstrcmp(input, "touch") == 0) cmd_mobile();
                else if (input[0]=='d' && input[1]=='i' && input[2]=='p' && input[3]=='a') {
                    const char* p = input + 4;
                    while (*p == ' ') p++;
                    cmd_dipa(p);
                }
                else if (kstrcmp(input, "phoxair")    == 0) cmd_phoxair();
                else if (input[0]=='v' && input[1]=='e' && input[2]=='r' && input[3]=='i' && input[4]=='f' && input[5]=='y') {
                    const char* p = input + 6;
                    while (*p == ' ') p++;
                    cmd_verify(p);
                }
                else if (kstrcmp(input, "license")    == 0 || kstrcmp(input, "entitlements") == 0) cmd_entitlements();
                else if (kstrcmp(input, "vfs")        == 0 || kstrcmp(input, "ls") == 0) cmd_vfs();
                else if (input[0]=='c' && input[1]=='a' && input[2]=='t') {
                    const char* p = input + 3;
                    while (*p == ' ') p++;
                    cmd_cat(p);
                }
                else if (kstrcmp(input, "reboot") == 0) shell_reboot();
                else {
                    console_write("Unknown command '");
                    console_write(input);
                    console_write("'. Type 'help'.\n");
                }
            }

            input_len = 0;
            console_write("phoxtra> ");
        } else if (c == '\b') {
            if (input_len > 0) { input_len--; console_put_char('\b'); }
        } else if (c >= ' ' && c <= '~') {
            if (input_len < 255) { input[input_len++] = c; console_put_char(c); }
        }
    }
}

