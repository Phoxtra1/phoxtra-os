cmake_minimum_required(VERSION 3.20)

project(PhoxtraBoot)

set(BOOT_SOURCES
    bootloader/main.cpp
    uefi/main.cpp
)

add_library(phoxtra-boot STATIC
    ${BOOT_SOURCES}
)

target_compile_features(phoxtra-boot PRIVATE
    cxx_std_20
)
