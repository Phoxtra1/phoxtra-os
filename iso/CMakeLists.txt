cmake_minimum_required(VERSION 3.20)

set(ISO_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(KERNEL_BINARY ${CMAKE_BINARY_DIR}/kernel/phoxtra)

set(ISO_OUTPUT ${CMAKE_BINARY_DIR}/phoxtra.iso)

add_custom_command(
    OUTPUT ${ISO_OUTPUT}
    COMMAND mkdir -p ${ISO_DIR}/boot
    COMMAND cp ${KERNEL_BINARY} ${ISO_DIR}/boot/phoxtra
    COMMAND grub-mkrescue -o ${ISO_OUTPUT} ${ISO_DIR}
    DEPENDS phoxtra-kernel
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Creating PhoxtraOS bootable ISO"
)

add_custom_target(iso
    DEPENDS ${ISO_OUTPUT}
)
