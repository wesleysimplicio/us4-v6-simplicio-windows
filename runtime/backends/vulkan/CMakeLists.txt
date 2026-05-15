add_library(
    us4_runtime_backends_vulkan
    src/vulkan_context.cpp)

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
