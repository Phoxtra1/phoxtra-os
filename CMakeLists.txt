cmake_minimum_required(VERSION 3.20)

project(PhoxtraOS
    VERSION 0.1.0
    DESCRIPTION "Phoxtra Operating System"
    LANGUAGES C CXX ASM
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PHOXTRA_BUILD_TESTS "Build tests" ON)
option(PHOXTRA_BUILD_DOCS "Build documentation" OFF)
option(PHOXTRA_ENABLE_GUEST_HTTPS_SMOKE "Embed a guest-mode HTTPS integration probe for QEMU CI" OFF)

add_subdirectory(kernel)
add_subdirectory(boot)
add_subdirectory(iso)

# ---- Verified HTTPS transport (Step 2) ----
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(PHOXTRA_MBEDTLS_USER_CONFIG "${CMAKE_SOURCE_DIR}/libc/tls/mbedtls_phoxtra_config.h")
set(MBEDTLS_USER_CONFIG_FILE "${CMAKE_SOURCE_DIR}/libc/tls/mbedtls_phoxtra_config.h"
    CACHE FILEPATH "Phoxtra Mbed TLS platform configuration" FORCE)
set(TF_PSA_CRYPTO_USER_CONFIG_FILE "${CMAKE_SOURCE_DIR}/libc/tls/psa_crypto_phoxtra_config.h"
    CACHE FILEPATH "Phoxtra TF-PSA-Crypto user configuration" FORCE)

if(PHOXTRA_ENABLE_GUEST_HTTPS_SMOKE)
    # Generate a matching TLS/PSA configuration instead of trying to undo host-only
    # defaults in a user header after their source files have already been selected.
    set(MBEDTLS_CONFIG_UNSET
        "MBEDTLS_SELF_TEST;MBEDTLS_DEBUG_C;MBEDTLS_FS_IO;MBEDTLS_NET_C;MBEDTLS_TIMING_C"
        CACHE STRING "Disable unsupported hosted Mbed TLS features" FORCE)
else()
    set(MBEDTLS_CONFIG_UNSET "" CACHE STRING "Disable unsupported hosted Mbed TLS features" FORCE)
endif()
set(ENABLE_PROGRAMS OFF CACHE BOOL "Disable Mbed TLS sample programs" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "Disable upstream Mbed TLS tests in OS build" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "Build static Mbed TLS libraries" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "Disable shared Mbed TLS libraries" FORCE)
if(PHOXTRA_ENABLE_GUEST_HTTPS_SMOKE)
    # Third-party TLS archives must match the freestanding guest ELF ABI.
    add_compile_options(-mcmodel=large -fno-pie -fno-stack-protector -ffunction-sections -fdata-sections)
endif()
add_subdirectory(third_party/mbedtls EXCLUDE_FROM_ALL)

set(PHOXTRA_CA_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(
    OUTPUT "${PHOXTRA_CA_GENERATED_DIR}/phoxtra_default_ca.h"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/embed_ca_bundle.py"
            "${CMAKE_SOURCE_DIR}/third_party/certifi/certifi/cacert.pem"
            "${PHOXTRA_CA_GENERATED_DIR}/phoxtra_default_ca.h"
    DEPENDS "${CMAKE_SOURCE_DIR}/scripts/embed_ca_bundle.py"
            "${CMAKE_SOURCE_DIR}/third_party/certifi/certifi/cacert.pem"
    VERBATIM
)
add_custom_target(phoxtra_ca_bundle DEPENDS "${PHOXTRA_CA_GENERATED_DIR}/phoxtra_default_ca.h")
add_library(phoxtra_https_client STATIC userland/net/https_client.c userland/net/default_ca.c)
add_dependencies(phoxtra_https_client phoxtra_ca_bundle)
target_include_directories(phoxtra_https_client PUBLIC userland/net PRIVATE "${PHOXTRA_CA_GENERATED_DIR}")
target_compile_definitions(phoxtra_https_client PRIVATE "MBEDTLS_USER_CONFIG_FILE=\"${PHOXTRA_MBEDTLS_USER_CONFIG}\"")
target_compile_definitions(phoxtra_https_client PRIVATE PHOXTRA_HTTPS_TEST_DIAGNOSTICS=1)
target_compile_options(phoxtra_https_client PRIVATE -Wall -Wextra -Werror)
target_link_libraries(phoxtra_https_client PUBLIC mbedtls mbedx509 tfpsacrypto)

add_library(phoxtra_userlib OBJECT
    libc/src/libc.c libc/src/net.c libc/src/random.c libc/src/psa_rng.c
    libc/src/time.c libc/src/heap.c libc/src/https_io.c
    userland/net/https_client.c userland/net/default_ca.c)
add_dependencies(phoxtra_userlib phoxtra_ca_bundle)
target_include_directories(phoxtra_userlib PRIVATE libc/include userland/net "${PHOXTRA_CA_GENERATED_DIR}"
    third_party/mbedtls/include third_party/mbedtls/tf-psa-crypto/include
    third_party/mbedtls/tf-psa-crypto/drivers/builtin/include)
target_compile_definitions(phoxtra_userlib PRIVATE "MBEDTLS_USER_CONFIG_FILE=\"${PHOXTRA_MBEDTLS_USER_CONFIG}\"")
target_compile_definitions(phoxtra_userlib PRIVATE "TF_PSA_CRYPTO_USER_CONFIG_FILE=\"${CMAKE_SOURCE_DIR}/libc/tls/psa_crypto_phoxtra_config.h\"")
target_compile_options(phoxtra_userlib PRIVATE -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -Wall -Wextra -Werror)

if(PHOXTRA_ENABLE_GUEST_HTTPS_SMOKE)
    set(PHOXTRA_GUEST_HTTPS_DIR "${CMAKE_BINARY_DIR}/guest-https")
    set(PHOXTRA_GUEST_HTTPS_CA_HEADER "${PHOXTRA_CA_GENERATED_DIR}/phoxtra_guest_test_ca.h")
    set(PHOXTRA_GUEST_HTTPS_CERT "${PHOXTRA_GUEST_HTTPS_DIR}/server.pem")
    set(PHOXTRA_GUEST_HTTPS_KEY "${PHOXTRA_GUEST_HTTPS_DIR}/server.key")
    add_custom_command(
        OUTPUT "${PHOXTRA_GUEST_HTTPS_CA_HEADER}" "${PHOXTRA_GUEST_HTTPS_CERT}" "${PHOXTRA_GUEST_HTTPS_KEY}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/prepare_guest_https_fixture.py" "${PHOXTRA_GUEST_HTTPS_DIR}"
        DEPENDS "${CMAKE_SOURCE_DIR}/tests/prepare_guest_https_fixture.py"
        VERBATIM
    )
    add_custom_target(phoxtra_guest_https_fixture
        DEPENDS "${PHOXTRA_GUEST_HTTPS_CA_HEADER}" "${PHOXTRA_GUEST_HTTPS_CERT}" "${PHOXTRA_GUEST_HTTPS_KEY}")
    target_compile_definitions(phoxtra_userlib PRIVATE PHOXTRA_HTTPS_PORT=44243)
    add_executable(phoxtra_guest_https_elf
        userland/net/https_guest_entry.S userland/net/https_guest_smoke.c
        $<TARGET_OBJECTS:phoxtra_userlib>)
    add_dependencies(phoxtra_guest_https_elf phoxtra_guest_https_fixture phoxtra_ca_bundle)
    target_include_directories(phoxtra_guest_https_elf PRIVATE
        libc/include userland/net "${PHOXTRA_CA_GENERATED_DIR}"
        third_party/mbedtls/include third_party/mbedtls/tf-psa-crypto/include
        third_party/mbedtls/tf-psa-crypto/drivers/builtin/include)
    target_compile_options(phoxtra_guest_https_elf PRIVATE
        -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -mno-red-zone -Wall -Wextra -Werror)
    target_link_options(phoxtra_guest_https_elf PRIVATE
        -nostdlib -static -no-pie -Wl,--build-id=none -Wl,--no-undefined -Wl,--no-relax -Wl,--gc-sections
        -Wl,-T,${CMAKE_SOURCE_DIR}/userland/linker.ld)
    target_link_libraries(phoxtra_guest_https_elf PRIVATE
        "-Wl,--start-group" mbedtls mbedx509 tfpsacrypto gcc "-Wl,--end-group")
    set(PHOXTRA_GUEST_HTTPS_IMAGE_HEADER "${PHOXTRA_CA_GENERATED_DIR}/phoxtra_guest_https_elf.h")
    add_custom_command(
        OUTPUT "${PHOXTRA_GUEST_HTTPS_IMAGE_HEADER}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/embed_binary.py"
                "$<TARGET_FILE:phoxtra_guest_https_elf>" "${PHOXTRA_GUEST_HTTPS_IMAGE_HEADER}" phoxtra_guest_https_elf
        DEPENDS phoxtra_guest_https_elf "${CMAKE_SOURCE_DIR}/scripts/embed_binary.py"
        VERBATIM)
    add_custom_target(phoxtra_guest_https_image DEPENDS "${PHOXTRA_GUEST_HTTPS_IMAGE_HEADER}")
    target_sources(phoxtra-kernel PRIVATE kernel/exec/guest_https_image.c)
    target_include_directories(phoxtra-kernel PRIVATE "${PHOXTRA_CA_GENERATED_DIR}")
    target_compile_definitions(phoxtra-kernel PRIVATE PHOXTRA_ENABLE_GUEST_HTTPS_SMOKE=1)
    add_dependencies(phoxtra-kernel phoxtra_guest_https_image)
endif()


add_executable(test_https_client tests/test_https_client.c)
target_include_directories(test_https_client PRIVATE third_party/mbedtls/include
    third_party/mbedtls/tf-psa-crypto/include)
target_compile_definitions(test_https_client PRIVATE "MBEDTLS_USER_CONFIG_FILE=\"${PHOXTRA_MBEDTLS_USER_CONFIG}\"")
target_compile_options(test_https_client PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_https_client PRIVATE phoxtra_https_client mbedtls mbedx509 tfpsacrypto)
if(PHOXTRA_ENABLE_GUEST_HTTPS_SMOKE)
    # Mbed TLS archives are built no-PIE for the freestanding guest ELF.
    # Keep the host-side regression executable compatible with those archives.
    target_link_options(test_https_client PRIVATE -no-pie)
endif()
