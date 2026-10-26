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

add_subdirectory(kernel)
add_subdirectory(boot)
add_subdirectory(iso)

# ---- Verified HTTPS transport (Step 2) ----
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(PHOXTRA_MBEDTLS_USER_CONFIG "${CMAKE_SOURCE_DIR}/libc/tls/mbedtls_phoxtra_config.h")
set(MBEDTLS_USER_CONFIG_FILE "${CMAKE_SOURCE_DIR}/libc/tls/mbedtls_phoxtra_config.h"
    CACHE FILEPATH "Phoxtra Mbed TLS platform configuration" FORCE)
add_compile_definitions(TF_PSA_CRYPTO_USER_CONFIG_FILE="${CMAKE_SOURCE_DIR}/libc/tls/psa_crypto_phoxtra_config.h")
set(ENABLE_PROGRAMS OFF CACHE BOOL "Disable Mbed TLS sample programs" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "Disable upstream Mbed TLS tests in OS build" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "Build static Mbed TLS libraries" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "Disable shared Mbed TLS libraries" FORCE)
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
target_compile_definitions(phoxtra_https_client PRIVATE MBEDTLS_USER_CONFIG_FILE="${PHOXTRA_MBEDTLS_USER_CONFIG}")
target_compile_options(phoxtra_https_client PRIVATE -Wall -Wextra -Werror)
target_link_libraries(phoxtra_https_client PUBLIC mbedtls mbedx509 tfpsacrypto)

add_library(phoxtra_userlib OBJECT
    libc/src/libc.c libc/src/net.c libc/src/random.c libc/src/psa_rng.c
    libc/src/time.c libc/src/heap.c libc/src/https_io.c
    userland/net/https_client.c userland/net/default_ca.c)
add_dependencies(phoxtra_userlib phoxtra_ca_bundle)
target_include_directories(phoxtra_userlib PRIVATE libc/include userland/net "${PHOXTRA_CA_GENERATED_DIR}"
    third_party/mbedtls/include third_party/mbedtls/tf-psa-crypto/include)
target_compile_definitions(phoxtra_userlib PRIVATE MBEDTLS_USER_CONFIG_FILE="${PHOXTRA_MBEDTLS_USER_CONFIG}")
target_compile_options(phoxtra_userlib PRIVATE -ffreestanding -fno-builtin -Wall -Wextra -Werror)

add_executable(test_https_client tests/test_https_client.c)
target_include_directories(test_https_client PRIVATE third_party/mbedtls/include
    third_party/mbedtls/tf-psa-crypto/include)
target_compile_definitions(test_https_client PRIVATE MBEDTLS_USER_CONFIG_FILE="${PHOXTRA_MBEDTLS_USER_CONFIG}")
target_compile_options(test_https_client PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_https_client PRIVATE phoxtra_https_client mbedtls mbedx509 tfpsacrypto)
