add_library(us4_runtime_moe
  src/expert_pager.cpp
  src/expert_router.cpp
  src/speculative_prefetch.cpp
)

target_include_directories(us4_runtime_moe
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(us4_runtime_moe
  PUBLIC
    us4_build_options
    us4_build_warnings
)
