# Tests support
find_package(GTest REQUIRED)

add_executable(dosbox_tests
    ansi_code_markup_tests.cpp
    batch_file_tests.cpp
    bit_view_tests.cpp
    bitops_tests.cpp
    clap_plugin_tests.cpp
    cmd_move_tests.cpp
    config_tests.cpp
    dos_files_tests.cpp
    dos_memory_struct_tests.cpp
    dosbox_pause_fsm_tests.cpp
    dosbox_restart_tests.cpp
    dosbox_test_fixture.h
    drives_tests.cpp
    fraction_tests.cpp
    fs_utils_tests.cpp
    i8042_wide_read_tests.cpp
    image_decoder_tests.cpp
    int10_modes_tests.cpp
    math_utils_tests.cpp
    messages_adjust_tests.cpp
    mixer_tests.cpp
    mount_tests.cpp
    port_containers_tests.cpp
    program_mixer_tests.cpp
    rect_tests.cpp
    rgb_tests.cpp
    ring_buffer_tests.cpp
    rwqueue_tests.cpp
    shader_pragma_parser_tests.cpp
    shell_cmds_tests.cpp
    shell_redirection_tests.cpp
    string_utils_tests.cpp
    # stubs.cpp
    support_tests.cpp
    unicode_tests.cpp
    window_geometry_tests.cpp
    multi_prefix_tests.cpp
)

# Disable some warnings for deliberately flawed test cases
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT MSVC)
    target_compile_options(dosbox_tests PRIVATE
        "-Wno-effc++"
    )
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(dosbox_tests PRIVATE
            "-Wno-gnu-zero-variadic-macro-arguments"
        )
    else()
        target_compile_options(dosbox_tests PRIVATE
            "-Wno-format-overflow"
        )
    endif()
endif()

target_link_libraries(dosbox_tests PRIVATE
    clap
    GTest::gmock_main
    dosboxcommon
    SDL3::Headers
)

foreach(plugin_target IN ITEMS clap_test_plugin clap_test_incompatible_plugin)
    add_library(${plugin_target} MODULE clap_test_plugin.cpp)
    target_link_libraries(${plugin_target} PRIVATE clap)
    set_target_properties(${plugin_target} PROPERTIES PREFIX "" SUFFIX ".clap")
endforeach()

target_compile_definitions(clap_test_incompatible_plugin PRIVATE
    CLAP_TEST_INCOMPATIBLE_ENTRY
)

target_compile_definitions(dosbox_tests PRIVATE
    CLAP_TEST_PLUGIN_PATH="$<TARGET_FILE:clap_test_plugin>"
    CLAP_TEST_INCOMPATIBLE_PLUGIN_PATH="$<TARGET_FILE:clap_test_incompatible_plugin>"
)
add_dependencies(dosbox_tests clap_test_plugin clap_test_incompatible_plugin)

include(GoogleTest)

# `DISCOVERY_MODE PRE_TEST` is needed for local builds on macOS for test
# discovery to work, otherwise the build will just fail with an error.
#
# Probably related to:
# https://gitlab.kitware.com/cmake/cmake/-/issues/21845
#
gtest_discover_tests(
  dosbox_tests WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  DISCOVERY_MODE PRE_TEST
  DISCOVERY_TIMEOUT 60
)
