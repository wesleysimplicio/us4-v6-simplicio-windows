add_library(us4_runtime_cache
  src/multimodal_cache.cpp
  src/prefix_cache.cpp
  src/sparsity_aware_cache.cpp
)

target_include_directories(us4_runtime_cache
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(us4_runtime_cache
  PUBLIC
    us4_build_options
    us4_build_warnings
)
