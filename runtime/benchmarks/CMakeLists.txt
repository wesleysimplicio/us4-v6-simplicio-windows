add_library(us4_runtime_benchmark_registry
  src/benchmark_registry.cpp
)

target_include_directories(us4_runtime_benchmark_registry
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(us4_runtime_benchmark_registry
  PUBLIC
    us4_runtime_backends
    us4_runtime_core
    us4_profiles
    us4_build_options
    us4_build_warnings
)

add_library(us4_runtime_matrix_runner
  src/matrix_runner.cpp
)

target_include_directories(us4_runtime_matrix_runner
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(us4_runtime_matrix_runner
  PUBLIC
    us4_runtime_benchmark_registry
    us4_runtime_tuning
    us4_runtime_core
    us4_build_options
    us4_build_warnings
)

add_executable(dense_baseline
  src/dense_baseline.cpp
)

target_link_libraries(dense_baseline
  PRIVATE
    us4_runtime_benchmark_registry
    us4_runtime_core
    us4_build_options
    us4_build_warnings
)

add_executable(cpu_block_gemm_bench
  src/cpu_block_gemm_bench.cpp
)

target_link_libraries(cpu_block_gemm_bench
  PRIVATE
    us4_runtime_backends
    us4_runtime_core
    us4_build_options
    us4_build_warnings
)

add_executable(cpu_attention_bench
  src/cpu_attention_bench.cpp
)

target_link_libraries(cpu_attention_bench
  PRIVATE
    us4_runtime_backends
    us4_runtime_core
    us4_build_options
    us4_build_warnings
)

add_executable(cpu_bitnet_bench
  src/cpu_bitnet_bench.cpp
)

target_link_libraries(cpu_bitnet_bench
  PRIVATE
    us4_runtime_backends
    us4_runtime_core
    us4_build_options
    us4_build_warnings
)

add_executable(cuda_fallback_policy_bench
  src/cuda_fallback_policy_bench.cpp
)

target_link_libraries(cuda_fallback_policy_bench
  PRIVATE
    us4_runtime_backends_cuda
    us4_build_options
    us4_build_warnings
)

add_executable(cuda_graph_reuse_bench
  src/cuda_graph_reuse_bench.cpp
)

target_link_libraries(cuda_graph_reuse_bench
  PRIVATE
    us4_runtime_backends_cuda
    us4_build_options
    us4_build_warnings
)

add_executable(cuda_kernel_correctness_bench
  src/cuda_kernel_correctness_bench.cpp
)

target_link_libraries(cuda_kernel_correctness_bench
  PRIVATE
    us4_runtime_backends_cuda
    us4_build_options
    us4_build_warnings
)

add_executable(cuda_bitnet_bench
  src/cuda_bitnet_bench.cpp
)

target_link_libraries(cuda_bitnet_bench
  PRIVATE
    us4_runtime_backends_cpu_avx
    us4_runtime_backends_cuda
    us4_build_options
    us4_build_warnings
)

add_executable(moe_throughput
  src/moe_throughput.cpp
)

target_link_libraries(moe_throughput
  PRIVATE
    us4_runtime_benchmark_registry
    us4_runtime_core
    us4_build_options
    us4_build_warnings
)

add_executable(correctness_diff
  src/correctness_diff.cpp
)

target_link_libraries(correctness_diff
  PRIVATE
    us4_runtime_benchmark_registry
    us4_runtime_core
    us4_build_options
    us4_build_warnings
)

add_executable(hybrid_planner_gate
  src/hybrid_planner_gate.cpp
)

target_link_libraries(hybrid_planner_gate
  PRIVATE
    us4_runtime_benchmark_registry
    us4_runtime_backends_vulkan
    us4_runtime_backends_windows_ml
    us4_profiles
    us4_build_options
    us4_build_warnings
)
