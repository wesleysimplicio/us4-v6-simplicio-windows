add_library(us4_profiles
  src/profile_catalog.cpp
)

target_include_directories(us4_profiles
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(us4_profiles
  PUBLIC
    us4_runtime_backends
    us4_build_options
    us4_build_warnings
)
