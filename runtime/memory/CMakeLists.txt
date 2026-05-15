add_library(us4_runtime_memory
  src/buffer_arena.cpp
)

target_include_directories(us4_runtime_memory
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(us4_runtime_memory
  PUBLIC
    us4_build_options
    us4_build_warnings
)
