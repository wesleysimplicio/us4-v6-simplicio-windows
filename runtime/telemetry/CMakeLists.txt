add_library(us4_runtime_telemetry
  src/telemetry_sink.cpp
)

target_include_directories(us4_runtime_telemetry
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(us4_runtime_telemetry
  PUBLIC
    us4_build_options
    us4_build_warnings
)
