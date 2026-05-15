add_executable(
    us4-cli
    main.cpp)

target_include_directories(
    us4-cli
    PRIVATE
        ${CMAKE_SOURCE_DIR})

target_link_libraries(
    us4-cli
    PRIVATE
        us4_runtime_matrix_runner
        us4_runtime_core
        us4_build_options
        us4_build_warnings)
