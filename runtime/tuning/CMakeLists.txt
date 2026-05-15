add_library(us4_runtime_tuning
  src/auto_tuner.cpp
)

target_include_directories(us4_runtime_tuning
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(us4_runtime_tuning
  PUBLIC
    us4_runtime_backends
    us4_build_options
    us4_build_warnings
)
