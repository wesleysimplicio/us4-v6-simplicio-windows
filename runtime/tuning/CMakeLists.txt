add_library(us4_runtime_tuning
  src/profile_store.cpp
  src/auto_tuner.cpp
)

target_include_directories(us4_runtime_tuning
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(us4_runtime_tuning
  PUBLIC
    us4_profiles
    us4_runtime_backends
    us4_build_options
    us4_build_warnings
)

file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/runtime/tuning/profiles.json" US4_PROFILE_STORE_PATH_VALUE)
target_compile_definitions(us4_runtime_tuning
  PUBLIC
    US4_PROFILE_STORE_PATH="${US4_PROFILE_STORE_PATH_VALUE}"
)
