add_library(
    us4_runtime_backends_cuda
    src/cuda_backend.cpp
    src/cublas_fallback.cpp
    src/cuda_context.cpp)

target_include_directories(
    us4_runtime_backends_cuda
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/runtime/backends/include
    PRIVATE
        ${CMAKE_SOURCE_DIR})

target_link_libraries(
    us4_runtime_backends_cuda
    PUBLIC
        us4_build_options
        us4_build_warnings)
