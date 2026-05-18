cmake_minimum_required(VERSION 3.27)

project(
    us4_v6_simplicio_windows
    VERSION 0.1.43
    DESCRIPTION "US4 V6 Windows Edition runtime scaffold"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(US4_ENABLE_TESTS "Build US4 unit tests" ON)
option(US4_ENABLE_BENCHMARKS "Build US4 benchmark scaffolds" ON)
option(US4_ENABLE_CUDA "Enable CUDA backend integration" ON)
option(US4_ENABLE_DIRECTML "Enable DirectML backend integration" ON)
option(US4_ENABLE_VULKAN "Enable Vulkan backend integration" ON)
option(US4_ENABLE_WINDOWS_ML "Enable Windows ML / NPU backend integration" ON)

include(CTest)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/us4/runtime")
configure_file(
    "${CMAKE_SOURCE_DIR}/runtime/version.h.in"
    "${CMAKE_BINARY_DIR}/generated/us4/runtime/version.h"
    @ONLY)

add_library(us4_build_options INTERFACE)
target_compile_features(us4_build_options INTERFACE cxx_std_20)
target_include_directories(us4_build_options INTERFACE "${CMAKE_BINARY_DIR}/generated")

add_library(us4_build_warnings INTERFACE)
if(MSVC)
    target_compile_options(us4_build_warnings INTERFACE /W4 /permissive-)
else()
    target_compile_options(us4_build_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

message(STATUS "US4 backend options:")
message(STATUS "  CUDA: ${US4_ENABLE_CUDA}")
message(STATUS "  DirectML: ${US4_ENABLE_DIRECTML}")
message(STATUS "  Vulkan: ${US4_ENABLE_VULKAN}")
message(STATUS "  Windows ML: ${US4_ENABLE_WINDOWS_ML}")

add_subdirectory(runtime/backends)
add_subdirectory(profiles)
add_subdirectory(runtime/adapters)
add_subdirectory(runtime/memory)
add_subdirectory(runtime/kv)
add_subdirectory(runtime/cache)
add_subdirectory(runtime/moe)
add_subdirectory(runtime/scheduler)
add_subdirectory(runtime/speculative)
add_subdirectory(runtime/tuning)
add_subdirectory(runtime/telemetry)
add_subdirectory(runtime/core)
if(US4_ENABLE_BENCHMARKS)
    add_subdirectory(runtime/benchmarks)
endif()
add_subdirectory(runtime/cli)

if(BUILD_TESTING AND US4_ENABLE_TESTS)
    find_package(GTest CONFIG QUIET)

    if(GTest_FOUND)
        add_executable(
            us4_core_tests
            tests/unit/backend_planner_test.cpp
            tests/unit/bitnet_ternary_test.cpp
            tests/unit/continuous_batcher_test.cpp
            tests/unit/cuda_backend_test.cpp
            tests/unit/draft_model_loader_test.cpp
            tests/unit/eagle3_decoder_test.cpp
            tests/unit/glm_adapter_test.cpp
            tests/unit/hardware_probe_test.cpp
            tests/unit/auto_tuner_test.cpp
            tests/unit/kv_runtime_test.cpp
            tests/unit/llama_config_test.cpp
            tests/unit/llama_runtime_test.cpp
            tests/unit/minimax_adapter_test.cpp
            tests/unit/moe_runtime_test.cpp
            tests/unit/multimodal_cache_test.cpp
            tests/unit/peagle_decoder_test.cpp
            tests/unit/session_pool_test.cpp
            tests/unit/sparsity_aware_cache_test.cpp
            tests/unit/speculative_prefetch_test.cpp
            tests/unit/ssd_cold_store_test.cpp)

        target_link_libraries(
            us4_core_tests
            PRIVATE
                us4_runtime_adapters
                us4_runtime_benchmark_registry
                us4_runtime_cache
                us4_runtime_core
                us4_runtime_kv
                us4_runtime_matrix_runner
                us4_runtime_moe
                us4_runtime_scheduler
                us4_runtime_speculative
                GTest::gtest_main
                us4_build_options
                us4_build_warnings)

        include(GoogleTest)
        gtest_discover_tests(us4_core_tests)
    else()
        message(
            WARNING
            "GTest not found. Unit tests were not generated. "
            "Install GoogleTest or configure GTest_DIR to enable tests.")
    endif()
endif()
