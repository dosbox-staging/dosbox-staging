# gtest dependency
#
# If feature 'unit_tests' is set to 'auto', then unit tests are disabled
# for release builds and enabled everywhere else.
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
gmock_dep = disabler()

if not (get_option('buildtype') == 'release' and get_option('unit_tests').auto())
  # Depend on gmock > 1.8.0 pulling in gtest as a dependency, otherwise it's hard
  # to set up gtest and gmock in mixed wrap/system-installed edge-cases.
  gmock_dep = dependency('gmock', main : true,
                         version : '> 1.8.0',
                         disabler : true,
                         required : get_option('unit_tests'),
                         fallback : ['gtest', 'gmock_main_dep'])
endif

summary('Unit tests', gmock_dep.found())

# Disable compiler flags that generate warnings
# from deliberately flawed unit test code.
#
cpp_args = []
foreach false_positive_flag : ['-Wno-effc++', '-Wno-gnu-zero-variadic-macro-arguments']
  if cxx.has_argument(false_positive_flag)
    cpp_args += false_positive_flag
  endif
endforeach

# unit tests with specific requirements
#
# - example  - has a failing testcase (on purpose)
# - fs_utils - depends on files in: tests/files/
#
example = executable('example', ['example_tests.cpp', 'stubs.cpp'],
                     dependencies : [gmock_dep, libmisc_dep, libghc_dep, libloguru_dep],
                     include_directories : incdir, cpp_args : cpp_args)
test('gtest example', example, 
     should_fail : true)

fs_utils = executable('fs_utils', ['fs_utils_tests.cpp', 'stubs.cpp'],
                      dependencies : [gmock_dep, libmisc_dep, libghc_dep, libloguru_dep],
                      include_directories : incdir, cpp_args : cpp_args)
test('gtest fs_utils', fs_utils,
     workdir : project_source_root, is_parallel : false)

# other unit tests

unit_tests = [
  {'name' : 'bitops',               'deps' : []},
  {'name' : 'iohandler_containers', 'deps' : [libmisc_dep]},
  {'name' : 'rwqueue',              'deps' : [libmisc_dep]},
  {'name' : 'soft_limiter',         'deps' : [atomic_dep, libmisc_dep]},
  {'name' : 'string_utils',         'deps' : []},
  {'name' : 'setup',                'deps' : [libmisc_dep]},
  {'name' : 'support',              'deps' : [libmisc_dep]},
  {'name' : 'drives',               'deps' : [dosbox_dep], 'extra_cpp': []},
  {'name' : 'dos_files',            'deps' : [dosbox_dep], 'extra_cpp': []},
  {'name' : 'shell_cmds',           'deps' : [dosbox_dep], 'extra_cpp': []},
  {'name' : 'shell_redirection',    'deps' : [dosbox_dep], 'extra_cpp': []},
  {'name' : 'ansi_code_markup',     'deps' : [libmisc_dep]},
]

foreach ut : unit_tests
  name = ut.get('name')
  extra_cpp = ut.get('extra_cpp', ['stubs.cpp'])
  exe = executable(name, [name + '_tests.cpp'] + extra_cpp,
                   dependencies : [gmock_dep, libghc_dep, libloguru_dep] + ut.get('deps'),
                   include_directories : incdir, cpp_args : cpp_args)
  test('gtest ' + name, exe)
endforeach
