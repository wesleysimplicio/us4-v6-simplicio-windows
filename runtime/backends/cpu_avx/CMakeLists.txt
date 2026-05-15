add_library(
    us4_runtime_backends_cpu_avx
    src/scalar_attention.cpp
    src/scalar_matmul.cpp)

target_include_directories(
    us4_runtime_backends_cpu_avx
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR})

target_link_libraries(
    us4_runtime_backends_cpu_avx
    PUBLIC
        us4_build_options
        us4_build_warnings)
