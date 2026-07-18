cmake_minimum_required(VERSION 3.20)

project(PhoxtraKernel)

enable_language(ASM)

set(KERNEL_SOURCES
    arch/x86_64/boot.S 
    arch/x86_64/cpu/cpu.cpp
    arch/x86_64/serial/serial.cpp
    src/kernel.cpp
    src/init.cpp 
    src/console.cpp
    src/panic.cpp
    src/memory.cpp
    src/scheduler.cpp
    src/process.cpp
    src/interrupt.cpp
    src/syscall.cpp
)

add_executable(phoxtra-kernel
    ${KERNEL_SOURCES}
)

target_include_directories(phoxtra-kernel PRIVATE
    include
    arch/x86_64/include
)

target_compile_features(phoxtra-kernel PRIVATE cxx_std_20)

target_compile_options(phoxtra-kernel PRIVATE
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -fno-pie
    -fno-stack-protector
    -fcf-protection=none
    -mno-red-zone
    -Wall
    -Wextra
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
