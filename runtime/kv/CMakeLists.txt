add_library(us4_runtime_kv
  src/kv_pager.cpp
  src/summarizer.cpp
)

target_include_directories(us4_runtime_kv
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(us4_runtime_kv
  PUBLIC
    us4_build_options
    us4_build_warnings
)
