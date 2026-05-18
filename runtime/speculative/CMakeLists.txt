add_library(us4_runtime_speculative
  src/draft_model_loader.cpp
  src/eagle3_decoder.cpp
  src/peagle_decoder.cpp
  src/speculative_engine.cpp
)

target_include_directories(us4_runtime_speculative
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(us4_runtime_speculative
  PUBLIC
    us4_runtime_backends
    us4_build_options
    us4_build_warnings
)
