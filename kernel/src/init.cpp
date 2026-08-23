#include "init.hpp"
#include "gdt.hpp"
#include "cpu.hpp"

extern "C" {
#include "idt.h"
#include "irq.h"
#include "timer.h"
#include "keyboard.h"
#include "logger.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "hal.h"
#include "process.h"
#include "scheduler.h"
#include "syscall.h"
#include "events.h"
#include "vfs.h"
#include "devfs.h"
#include "initrd.h"
#include "../storage/blockdev.h"
#include "../storage/ahci.h"
#include "../storage/partition.h"
#include "../storage/ext2.h"
#include "../storage/fat32.h"
#include "../security/sha256.h"
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
#include "../net/udp.h"
#include "../net/tcp.h"
#include "../net/socket.h"
#include "../net/dhcp.h"
#include "../net/dns.h"
#include "../exec/elf.h"
#include "../exec/ld_so.h"
#include "../drivers/framebuffer.h"
#include "../ui/font.h"
#include "../ui/compositor.h"
#include "../ui/wm.h"
#include "../process/stress.h"
#include "../memory/heap_audit.h"
#include "../sdk/sdk.h"
#include "../release/production.h"
#include "../drivers/pci.h"
#include "../ai/dipa_core.h"
#include "../services/phoxair.h"
#include "../security/verify.h"
#include "../services/entitlements.h"
#include "../ai/dipa_nl.h"
#include "../drivers/continuum.h"
#include "../exec/phoxtra_box.h"
#include "../drivers/phoxtra_gl.h"
#include "../net/cloud_sync.h"
}

/* ────────────────────────────────────────────────
   Kernel System Task Stubs
   These represent kernel services visible to the
   process table. Real work happens via events/IRQs.
   The shell task stub below is a placeholder only —
   the real shell runs directly on the boot stack.
   ──────────────────────────────────────────────── */
static void kernel_task(void) {
    while (1) { asm volatile("hlt"); }
}

static void logger_task(void) {
    while (1) { asm volatile("hlt"); }
}

static void security_task(void) {
    while (1) { asm volatile("hlt"); }
}

static void shell_task(void) {
    /* Placeholder — real shell runs on the boot stack */
    while (1) { asm volatile("hlt"); }
}

/* ────────────────────────────────────────────────
   system_init — called from kernel_init before STI
   ──────────────────────────────────────────────── */
void system_init()
{
    /* ── Phase 1: Hardware Foundation ──────────── */
    logger_init();
    cpu_init();
    gdt::init();
    idt_init();
    irq_init();
    timer_init(100);
    keyboard_init();

    /* ── Phase 2: Memory Subsystem ─────────────── */
    pmm_init(0);
    vmm_init();
    heap_init();

    /* ── Phase 3: HAL & PCI Express Scanner ───── */
    hal_init();
    pci_init();

    /* ── Phase 4: Event Bus ────────────────────── */
    events_init();

    /* ── Phase 5: Process Manager ──────────────── */
    process_init();

    /* ── Phase 6: Scheduler ────────────────────── */
    scheduler_init();

    /* ── Phase 7: Phoxtra Identity & Security Engine ─────────── */
    tpm2_init();
    process_identity_init();
    audit_logger_init();
    capability_init();
    mac_init();
    stack_guard_init();

    /* ── Phoxtra Core Enterprise Subsystems ── */
    entitlements_init();
    dipa_ai_init();
    dipa_nl_init();
    verify_init();
    phoxair_init();
    continuum_init();
    phoxtra_box_init();
    phoxtra_gl_init();
    cloud_sync_init();

    /* ── Phase 8: TCP/IP Network Stack & e1000 Driver ───────── */
    netdev_init();
    e1000_init();
    ethernet_init();
    arp_init();
    ipv4_init();
    udp_init();
    tcp_init();
    socket_subsystem_init();
    dhcp_init();
    dns_init();

    /* ── Phase 9: Dynamic Execution Runtime (ELF64 & libc) ─── */
    elf_init();
    ld_so_init();

    /* ── Phase 10: High-Resolution Framebuffer & GUI Compositor ── */
    framebuffer_init();
    font_init();
    compositor_init();
    wm_init();

    /* ── Phase 11: Stress Testing & Panic Resilience ───────── */
    stress_init();
    heap_audit_init();

    /* ── Phase 12: Developer SDK Toolchain Gateway ─────────── */
    sdk_init();

    /* ── Phase 13: v1.0.0 Commercial Release Engine ───────── */
    production_init();

    pcb_t* sproc  = process_create("shell",    shell_task,     9);
    pcb_t* kproc  = process_create("kernel",   kernel_task,   10);
    pcb_t* lproc  = process_create("logger",   logger_task,    5);
    pcb_t* sproc2 = process_create("security", security_task,  8);

    /* Shell is the active boot-stack process — mark RUNNING */
    if (sproc)  sproc->state  = PROCESS_STATE_RUNNING;
    /* Others are idle kernel daemons — mark WAITING */
    if (kproc)  kproc->state  = PROCESS_STATE_WAITING;
    if (lproc)  lproc->state  = PROCESS_STATE_WAITING;
    if (sproc2) sproc2->state = PROCESS_STATE_WAITING;

    /* ── Phase 8: System Call Framework ─────────── */
    syscall_init();

    /* ── Phase 9: VFS, DevFS & InitRD ──────────── */
    vfs_init();
    devfs_init();
    initrd_init();

    /* ── Phase 10: Storage Drivers & Filesystems ── */
    blockdev_init();
    ahci_init();
    partition_init();
    ext2_init();
    fat32_init();

    klog(LOG_KERNEL, "All subsystems initialized");
}

