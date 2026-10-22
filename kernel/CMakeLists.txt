cmake_minimum_required(VERSION 3.20)

project(PhoxtraKernel)

set(PHOXTRA_PRODUCT_PROFILE "laptop" CACHE STRING "Phoxtra product profile: laptop, desktop, embedded")
set_property(CACHE PHOXTRA_PRODUCT_PROFILE PROPERTY STRINGS laptop desktop embedded)
if(PHOXTRA_PRODUCT_PROFILE STREQUAL "embedded")
    set(PHOXTRA_PRODUCT_KIND 3)
elseif(PHOXTRA_PRODUCT_PROFILE STREQUAL "desktop")
    set(PHOXTRA_PRODUCT_KIND 2)
else()
    set(PHOXTRA_PRODUCT_KIND 1)
endif()

enable_language(ASM)
enable_language(ASM_NASM)

set(KERNEL_SOURCES
    arch/x86_64/boot.S
    arch/x86_64/cpu/cpu.cpp
    arch/x86_64/gdt/gdt.cpp
    arch/x86_64/gdt/gdt_flush.S
    arch/x86_64/serial/serial.cpp
    arch/x86_64/idt.c
    arch/x86_64/interrupts.asm
    arch/x86_64/isr.asm
    arch/x86_64/context.asm
    arch/x86_64/user_mode.asm
    src/kernel.cpp
    src/init.cpp
    src/console.cpp
    src/panic.cpp
    src/boot_screen.cpp
    src/pic.c
    src/irq.c
    src/timer.c
    src/keyboard.c
    src/mouse.c
    src/shell.c
    log/logger.c
    memory/pmm.c
    memory/vmm.c
    memory/heap.c
    hal/hal.c
    process/pcb.c
    process/process.c
    scheduler/scheduler.c
    syscall/syscall.c
    events/events.c
    vfs/vfs.c
    vfs/devfs.c
    vfs/initrd.c
    storage/blockdev.c
    storage/ahci.c
    storage/partition.c
    storage/ext2.c
    storage/fat32.c
    security/sha256.c
    security/tpm2.c
    security/process_identity.c
    security/audit_logger.c
    security/capability.c
    security/mac.c
    security/stack_guard.c
    net/netdev.c
    net/e1000.c
    net/ethernet.c
    net/arp.c
    net/ipv4.c
    net/udp.c
    net/tcp.c
    net/socket.c
    net/dhcp.c
    net/dns.c
    exec/elf.c
    exec/ld_so.c
    drivers/framebuffer.c
    drivers/pci.c
    ui/font.c
    ui/theme.c
    ui/file_manager.c
    ui/compositor.c
    ui/wm.c
    installer/installer.c
    ai/dipa_core.c
    services/phoxair.c
    services/platform.c
    security/verify.c
    services/entitlements.c
    ai/dipa_nl.c
    drivers/continuum.c
    exec/phoxtra_box.c
    drivers/phoxtra_gl.c
    net/cloud_sync.c
    process/stress.c
    memory/heap_audit.c
    sdk/sdk.c
    release/production.c
)

add_executable(phoxtra-kernel
    ${KERNEL_SOURCES}
)

target_include_directories(phoxtra-kernel PRIVATE
    include
    arch/x86_64
    arch/x86_64/include
    arch/x86_64/cpu
    arch/x86_64/serial
    arch/x86_64/gdt
    log
    memory
    hal
    process
    scheduler
    syscall
    events
    vfs
    storage
    security
    net
    exec
    drivers
    ui
    sdk
    release
)


target_compile_features(phoxtra-kernel PRIVATE cxx_std_20)
target_compile_definitions(phoxtra-kernel PRIVATE PHOXTRA_PRODUCT_KIND=${PHOXTRA_PRODUCT_KIND})

target_compile_options(phoxtra-kernel PRIVATE
    $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>
    $<$<COMPILE_LANGUAGE:C,CXX>:-fno-pie>
    $<$<COMPILE_LANGUAGE:C,CXX>:-fno-stack-protector>
    $<$<COMPILE_LANGUAGE:C,CXX>:-fcf-protection=none>
    $<$<COMPILE_LANGUAGE:C,CXX>:-mno-red-zone>
    $<$<COMPILE_LANGUAGE:C,CXX>:-mno-sse>
    $<$<COMPILE_LANGUAGE:C,CXX>:-mno-sse2>
    $<$<COMPILE_LANGUAGE:C,CXX>:-mno-avx>
    $<$<COMPILE_LANGUAGE:C,CXX>:-mno-avx2>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wall>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wextra>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)

target_link_options(phoxtra-kernel PRIVATE
    -nostdlib
    -no-pie
    -static
    -Wl,--build-id=none
    -T ${CMAKE_SOURCE_DIR}/linker/linker.ld
)

set_target_properties(phoxtra-kernel PROPERTIES
    OUTPUT_NAME phoxtra
)
