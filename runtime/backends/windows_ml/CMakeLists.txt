add_library(
    us4_runtime_backends_windows_ml
    src/winml_adapter.cpp
    src/layer_offloader.cpp
    src/mixed_dispatch_planner.cpp)

target_include_directories(
    us4_runtime_backends_windows_ml
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/runtime/backends/include
        ${CMAKE_SOURCE_DIR}/runtime/backends/vulkan/include
        ${CMAKE_SOURCE_DIR}/runtime/adapters/include
    PRIVATE
        ${CMAKE_SOURCE_DIR})

target_link_libraries(
    us4_runtime_backends_windows_ml
    PUBLIC
        us4_build_options
        us4_build_warnings)
