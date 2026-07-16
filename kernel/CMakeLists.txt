cmake_minimum_required(VERSION 3.20)

project(PhoxtraKernel)

enable_language(ASM)

set(KERNEL_SOURCES
    arch/x86_64/boot.S
    src/kernel.cpp
    src/init.cpp
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
)

target_compile_features(phoxtra-kernel PRIVATE cxx_std_20)

target_compile_options(phoxtra-kernel PRIVATE
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
)

target_link_options(phoxtra-kernel PRIVATE
    -nostdlib
    -T ${CMAKE_SOURCE_DIR}/linker/linker.ld
)

set_target_properties(phoxtra-kernel PROPERTIES
    OUTPUT_NAME phoxtra
)
