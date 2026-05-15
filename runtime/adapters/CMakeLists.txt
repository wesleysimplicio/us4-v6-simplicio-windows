add_library(us4_runtime_adapters
  src/dense_adapter_base.cpp
  src/qwen_scalar_adapter.cpp
  src/gemma_scalar_adapter.cpp
  src/null_windows_adapter.cpp
)

target_include_directories(us4_runtime_adapters
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(us4_runtime_adapters
  PUBLIC
    us4_runtime_backends
    us4_build_options
    us4_build_warnings
)
