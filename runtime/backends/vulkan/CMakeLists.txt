add_library(
    us4_runtime_backends_vulkan
    src/vulkan_context.cpp
    src/vulkan_kernel_library.cpp)

target_include_directories(
    us4_runtime_backends_vulkan
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/runtime/backends/include
        ${CMAKE_SOURCE_DIR}/runtime/adapters/include
    PRIVATE
        ${CMAKE_SOURCE_DIR})

target_link_libraries(
    us4_runtime_backends_vulkan
    PUBLIC
        us4_build_options
        us4_build_warnings)

target_compile_definitions(
    us4_runtime_backends_vulkan
    PUBLIC
        US4_VULKAN_KERNEL_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/kernels")
