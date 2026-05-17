# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(qert VERSION 0.1.0 LANGUAGES CXX)

# --- C++ Standard ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Compiler Warnings ---
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- Git Commit (for metadata) ---
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_COMMIT)
    set(GIT_COMMIT "unknown")
endif()

# --- Compiler Identification ---
set(QERT_COMPILER "${CMAKE_CXX_COMPILER_ID}")
set(QERT_COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}")

# --- Build Info Header ---
configure_file(
    ${CMAKE_SOURCE_DIR}/src/build_info.hpp.in
    ${CMAKE_BINARY_DIR}/generated/build_info.hpp
)

# --- Executable ---
add_executable(qert
    src/common.hpp
    src/seed.hpp
    src/seed.cpp
    src/statevector.hpp
    src/statevector.cpp
    src/gates.hpp
    src/gates.cpp
    src/telemetry.hpp
    src/telemetry.cpp
    src/main.cpp
)

target_include_directories(qert PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)

# --- Link Libraries ---
# PAPI will be added later when telemetry is implemented.
# find_library(PAPI_LIBRARY papi)
# target_link_libraries(qert PRIVATE ${PAPI_LIBRARY})