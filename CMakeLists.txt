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
