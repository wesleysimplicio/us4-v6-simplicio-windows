add_library(us4_runtime_scheduler
  src/continuous_batcher.cpp
  src/session_pool.cpp
)

target_include_directories(us4_runtime_scheduler
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(us4_runtime_scheduler
  PUBLIC
    us4_build_options
    us4_build_warnings
)
