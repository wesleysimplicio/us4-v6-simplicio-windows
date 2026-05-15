add_library(
    us4_runtime_backends_directml
    src/dml_device.cpp
    src/dml_graph.cpp)

target_include_directories(
    us4_runtime_backends_directml
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/runtime/backends/include
    PRIVATE
        ${CMAKE_SOURCE_DIR})

target_link_libraries(
    us4_runtime_backends_directml
    PUBLIC
        us4_build_options
        us4_build_warnings)
