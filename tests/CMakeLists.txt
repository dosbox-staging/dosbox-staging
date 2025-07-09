# Tests support
find_package(GTest REQUIRED)

add_executable(dosbox_tests
    ansi_code_markup_tests.cpp
    batch_file_tests.cpp
    bit_view_tests.cpp
    bitops_tests.cpp
    cmd_move_tests.cpp
    dos_files_tests.cpp
    dos_memory_struct_tests.cpp
    dosbox_test_fixture.h
    drives_tests.cpp
    fraction_tests.cpp
    fs_utils_tests.cpp
    int10_modes_tests.cpp
    iohandler_containers_tests.cpp
    math_utils_tests.cpp
    mixer_tests.cpp
    program_mixer_tests.cpp
    rect_tests.cpp
    rgb_tests.cpp
    ring_buffer_tests.cpp
    rwqueue_tests.cpp
    setup_tests.cpp
    shell_cmds_tests.cpp
    shell_redirection_tests.cpp
    string_utils_tests.cpp
    # stubs.cpp
    support_tests.cpp
)

# Disable some warnings for deliberately flawed test cases
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT MSVC)
    target_compile_options(dosbox_tests PRIVATE
        "-Wno-effc++"
        "-Wno-gnu-zero-variadic-macro-arguments"
    )
    # Clang does not support -Wno-format-overflow
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(dosbox_tests PRIVATE
            "-Wno-format-overflow"
        )
    endif()
endif()

target_link_libraries(dosbox_tests PRIVATE
    GTest::gmock_main
    libdosboxcommon
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)

include(GoogleTest)
gtest_discover_tests(dosbox_tests WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")