add_subdirectory(cpu_avx)

add_library(
    us4_runtime_backends
    src/backend_selector.cpp
    src/hardware_probe.cpp)

target_include_directories(
    us4_runtime_backends
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR})

target_compile_definitions(
    us4_runtime_backends
    PUBLIC
        US4_ENABLE_CUDA=$<BOOL:${US4_ENABLE_CUDA}>
        US4_ENABLE_DIRECTML=$<BOOL:${US4_ENABLE_DIRECTML}>
        US4_ENABLE_VULKAN=$<BOOL:${US4_ENABLE_VULKAN}>
        US4_ENABLE_WINDOWS_ML=$<BOOL:${US4_ENABLE_WINDOWS_ML}>)

target_link_libraries(
    us4_runtime_backends
    PUBLIC
        us4_runtime_backends_cpu_avx
        us4_build_options
        us4_build_warnings)
