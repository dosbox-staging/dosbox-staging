# gtest dependency
#
# Meson will first try to find pre-installed gmock and gtest via pkg-config;
# if this fails, it will download wrap dependency (subproject).
#
# Users can further configure the download behaviour using:
#
#   meson setup --wrap-mode={nofallback,nodownload,forcefallback}
#
# If gmock and gtest are not available at all, unit tests will be disabled.
#

# Depend on gmock > 1.8.0 pulling in gtest as a dependency, otherwise it's hard
# to set up gtest and gmock in mixed wrap/system-installed edge-cases.
gmock_dep = dependency(
    'gmock',
    main: true,
    version: ['> 1.8.0', '< 2'],
    disabler: true,
    required: get_option('unit_tests'),
    fallback: ['gtest', 'gmock_main_dep'],
)
summary('Unit tests', gmock_dep.found())

# Disable compiler flags that generate warnings
# from deliberately flawed unit test code.
#
cpp_args = warnings
foreach false_positive_flag : [
    '-Wno-effc++',
    '-Wno-gnu-zero-variadic-macro-arguments',
]
    if cxx.has_argument(false_positive_flag)
        cpp_args += false_positive_flag
    endif
endforeach

# unit tests with specific requirements
#
# - example  - has a failing testcase (on purpose)
# - fs_utils - depends on files in: tests/files/
#
example = executable(
    'example',
    ['example_tests.cpp', 'stubs.cpp'],
    dependencies: [gmock_dep, libmisc_stubs_dep, libshell_stubs_dep, libloguru_dep],
    include_directories: incdir,
    cpp_args: cpp_args,
)
test('gtest example', example, should_fail: true)

fs_utils = executable(
    'fs_utils',
    ['fs_utils_tests.cpp', 'stubs.cpp'],
    dependencies: [gmock_dep, libmisc_stubs_dep, libshell_stubs_dep, libloguru_dep],
    include_directories: incdir,
    cpp_args: cpp_args,
)
test(
    'gtest fs_utils',
    fs_utils,
    workdir: meson.project_source_root(),
    is_parallel: false,
)

program_mixer = executable(
    'program_mixer',
    ['program_mixer_tests.cpp'],
    dependencies: [gmock_dep, dosbox_dep, libiir_dep],
    include_directories: incdir,
    cpp_args: cpp_args,
)
test(
    'gtest program_mixer',
    program_mixer,
    workdir: meson.project_source_root(),
    is_parallel: false,
)

# other unit tests

unit_tests = [
    {'name': 'ansi_code_markup', 'deps': [libmisc_stubs_dep, libshell_stubs_dep]},
    {'name': 'batch_file', 'deps': [dosbox_dep]},
    {'name': 'bit_view', 'deps': []},
    {'name': 'bitops', 'deps': []},
    {'name': 'cmd_move', 'deps': [dosbox_dep], 'extra_cpp': []},
    {'name': 'dos_files', 'deps': [dosbox_dep], 'extra_cpp': []},
    {'name': 'dos_memory_struct', 'deps': [dosbox_dep], 'extra_cpp': []},
    {'name': 'drives', 'deps': [dosbox_dep], 'extra_cpp': []},
    {'name': 'fraction', 'deps': []},
    {'name': 'int10_modes', 'deps': [dosbox_dep], 'extra_cpp': []},
    {'name': 'math_utils', 'deps': [libmisc_stubs_dep, libshell_stubs_dep]},
    {'name': 'mixer', 'deps': [dosbox_dep, libiir_dep], 'extra_cpp': []},
    {'name': 'port_containers', 'deps': [libmisc_stubs_dep, libshell_stubs_dep]},
    {'name': 'rect', 'deps': []},
    {'name': 'ring_buffer', 'deps': []},
    {'name': 'rgb', 'deps': []},
    {'name': 'rwqueue', 'deps': [libmisc_stubs_dep, libshell_stubs_dep]},
    {'name': 'shell_cmds', 'deps': [dosbox_dep], 'extra_cpp': []},
    {'name': 'shell_redirection', 'deps': [dosbox_dep], 'extra_cpp': []},
    {'name': 'string_utils', 'deps': [libmisc_stubs_dep, libshell_stubs_dep]},
    {'name': 'support', 'deps': [libmisc_stubs_dep, libshell_stubs_dep]},
]

extra_link_flags = []
if get_option('b_pie') == false
    no_pie = '-no-pie'
    if host_machine.system() != 'darwin' and cxx.has_link_argument(no_pie)
        extra_link_flags += no_pie
    endif
endif

foreach ut : unit_tests
    name = ut.get('name')
    extra_cpp = ut.get('extra_cpp', ['stubs.cpp'])
    exe = executable(
        name,
        [name + '_tests.cpp'] + extra_cpp,
        dependencies: [gmock_dep, libloguru_dep] + ut.get('deps'),
        link_args: extra_link_flags,
        include_directories: incdir,
        cpp_args: cpp_args,
    )

    test('gtest ' + name, exe)
endforeach
