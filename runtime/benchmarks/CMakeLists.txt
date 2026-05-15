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
