add_library(
    us4_runtime_backends_onednn
    src/block_gemm_contract.cpp)

target_include_directories(
    us4_runtime_backends_onednn
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR})

target_link_libraries(
    us4_runtime_backends_onednn
    PUBLIC
        us4_build_options
        us4_build_warnings)
