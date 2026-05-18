add_subdirectory(../onednn ${CMAKE_CURRENT_BINARY_DIR}/onednn)

add_library(
    us4_runtime_backends_cpu_avx
    src/amx_dispatcher.cpp
    src/avx_attention.cpp
    src/avx2_matmul.cpp
    src/avx512_matmul.cpp
    src/bitnet_matmul.cpp
    src/bitnet_reference.cpp
    src/blocked_matmul.cpp
    src/dequant_int4.cpp
    src/dequant_int8.cpp
    src/kernel_profile.cpp
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
        us4_build_warnings
        us4_runtime_backends_onednn)

if(MSVC)
    set_source_files_properties(src/avx_attention.cpp PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
    set_source_files_properties(src/avx2_matmul.cpp PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
    set_source_files_properties(src/avx512_matmul.cpp PROPERTIES COMPILE_OPTIONS "/arch:AVX512")
    set_source_files_properties(src/bitnet_matmul.cpp PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
    set_source_files_properties(src/dequant_int4.cpp PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
    set_source_files_properties(src/dequant_int8.cpp PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
else()
    set_source_files_properties(src/avx_attention.cpp PROPERTIES COMPILE_OPTIONS "-mavx2")
    set_source_files_properties(src/avx2_matmul.cpp PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
    set_source_files_properties(src/bitnet_matmul.cpp PROPERTIES COMPILE_OPTIONS "-mavx2")
    set_source_files_properties(src/dequant_int4.cpp PROPERTIES COMPILE_OPTIONS "-mavx2")
    set_source_files_properties(src/dequant_int8.cpp PROPERTIES COMPILE_OPTIONS "-mavx2")
    set_source_files_properties(
        src/avx512_matmul.cpp
        PROPERTIES COMPILE_OPTIONS "-mavx512f;-mavx512vl;-mavx512dq;-mavx512bw;-mavx512vnni")
endif()
