cmake_minimum_required(VERSION 3.16)
project(qert VERSION 0.2.0 LANGUAGES CXX)

# --- Options ---
option(BUILD_TESTING "Build the test suite" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(ENABLE_PAPI "Enable hardware performance counters via PAPI" OFF)
option(ENABLE_OPENMP "Enable OpenMP parallelism" OFF)
option(ENABLE_SANITIZERS "Enable ASan and UBSan" OFF)

# --- C++ Standard ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Compiler Warnings ---
set(QERT_WARNING_FLAGS "")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(QERT_WARNING_FLAGS
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# --- Git Commit (for reproducibility metadata) ---
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
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

# Convert compiler flags list -> string
string(JOIN " " QERT_COMPILER_FLAGS ${QERT_WARNING_FLAGS})

# --- Build Info Header (generated) ---
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include/qert/build_info.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/build_info.hpp
)

# --- Core Library ---
add_library(qert_core STATIC
    core/src/seed.cpp
    core/src/statevector.cpp
    core/src/gates.cpp
    core/src/hardware.cpp
    core/src/telemetry.cpp
    core/src/entropy.cpp
    core/src/circuit.cpp
)

target_include_directories(qert_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# Apply warning flags to core library
target_compile_options(qert_core PRIVATE
    ${QERT_WARNING_FLAGS}
)

# --- Debug Build: enable assertions ---
target_compile_definitions(qert_core PRIVATE
    $<$<CONFIG:Debug>:QERT_DEBUG>
)

# --- Sanitizers ---
if(ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(qert_core PRIVATE -fsanitize=address,undefined)
    target_link_options(qert_core PUBLIC -fsanitize=address,undefined)
    message(STATUS "Sanitizers enabled: address, undefined")
endif()

# --- Main Executable ---
add_executable(qert
    app/main.cpp
)

target_link_libraries(qert PRIVATE qert_core)

# --- Optional: PAPI (for hardware performance counters) ---
if(ENABLE_PAPI)
    if(DEFINED PAPI_ROOT)
        find_library(PAPI_LIBRARY papi
            PATHS ${PAPI_ROOT}/lib
            NO_DEFAULT_PATH
        )
        find_path(PAPI_INCLUDE_DIR papi.h
            PATHS ${PAPI_ROOT}/include
            NO_DEFAULT_PATH
        )
    else()
        find_library(PAPI_LIBRARY papi)
        find_path(PAPI_INCLUDE_DIR papi.h)
    endif()

    if(PAPI_LIBRARY AND PAPI_INCLUDE_DIR)
        target_link_libraries(qert_core PUBLIC ${PAPI_LIBRARY})
        target_include_directories(qert_core PUBLIC ${PAPI_INCLUDE_DIR})
        target_compile_definitions(qert_core PUBLIC QERT_HAS_PAPI)
        message(STATUS "PAPI enabled: ${PAPI_LIBRARY}")
    else()
        message(FATAL_ERROR "PAPI requested but not found. "
                            "Install libpapi-dev or set -DPAPI_ROOT=/path/to/papi-install")
    endif()
endif()

# --- Optional: OpenMP (for parallel gate application) ---
if(ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(qert_core PUBLIC OpenMP::OpenMP_CXX)
        target_compile_definitions(qert_core PUBLIC QERT_HAS_OPENMP)
        message(STATUS "OpenMP enabled")
    else()
        message(FATAL_ERROR "OpenMP requested but not found")
    endif()
endif()

# --- Tests ---
if(BUILD_TESTING)
    enable_testing()

    # Download Catch2 v3 amalgamated files if not present
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/catch_amalgamated.hpp)
        file(DOWNLOAD
            https://github.com/catchorg/Catch2/releases/download/v3.15.0/catch_amalgamated.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/catch_amalgamated.hpp
            SHOW_PROGRESS
        )
    endif()

    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/catch_amalgamated.cpp)
        file(DOWNLOAD
            https://github.com/catchorg/Catch2/releases/download/v3.15.0/catch_amalgamated.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/catch_amalgamated.cpp
            SHOW_PROGRESS
        )
    endif()

    add_executable(qert_tests
        tests/test_main.cpp
        tests/test_common.cpp
        tests/test_seed.cpp
        tests/test_statevector.cpp
        tests/test_gates.cpp
        tests/test_telemetry.cpp
        tests/test_entropy.cpp
        tests/test_circuit.cpp
        tests/test_integration.cpp
        tests/catch_amalgamated.cpp
    )

    target_link_libraries(qert_tests PRIVATE qert_core)
    target_include_directories(qert_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_BINARY_DIR}/generated
    )

    target_compile_options(qert_tests PRIVATE
        ${QERT_WARNING_FLAGS}
    )

    if(ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(qert_tests PRIVATE -fsanitize=address,undefined)
        target_link_options(qert_tests PUBLIC -fsanitize=address,undefined)
    endif()

    add_test(NAME qert_tests COMMAND qert_tests)

    message(STATUS "Tests enabled")
endif()

# --- Benchmarks ---
if(BUILD_BENCHMARKS)
    add_executable(qert_benchmarks
        benchmarks/bench_circuits.cpp
    )
    target_link_libraries(qert_benchmarks PRIVATE qert_core)
    target_include_directories(qert_benchmarks PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/generated
    )
    target_compile_options(qert_benchmarks PRIVATE
        ${QERT_WARNING_FLAGS}
    )
    message(STATUS "Benchmarks enabled")
endif()

# --- Install ---
install(TARGETS qert_core qert
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)