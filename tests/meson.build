# gtest dependency
#
# If feature 'unit_tests' is set to 'auto', then unit tests are disabled
# for release builds and enabled everywhere else.
#
# Meson will first try to find pre-installed gtest via pkg-config;
# if this fails, it will download wrap dependency (subproject).
#
# Users can further configure the download behaviour using:
#
#   meson setup --wrap-mode={nofallback,nodownload,forcefallback}
#
# If gtest is not available at all, unit tests will be disabled.
#
gtest_dep = optional_dep
if not (get_option('buildtype') == 'release' and get_option('unit_tests').auto())
  gtest_dep = dependency('gtest', main : true,
                         required : get_option('unit_tests'),
                         fallback : ['gtest', 'gtest_main_dep'])
endif
if not gtest_dep.found()
  gtest_dep = disabler()
endif

# unit tests with specific requirements
#
# - example  - has a failing testcase (on purpose)
# - fs_utils - depends on files in: tests/files/
#
example = executable('example', 'example.cpp',
                     dependencies : [gtest_dep, sdl2_dep, libmisc_dep],
                     include_directories : incdir)
test('gtest example', example,
     suite : 'ut', should_fail : true)

fs_utils = executable('fs_utils', 'fs_utils.cpp',
                      dependencies : [gtest_dep, libmisc_dep],
                      include_directories : incdir)
test('gtest fs_utils', fs_utils,
     suite : 'ut', workdir : project_source_root, is_parallel : false)


# other unit tests
#
unit_tests = [
  {'name' : 'readerwritercircularbuffer', 'deps' : []},
  {'name' : 'rwqueue',                    'deps' : [libmisc_dep]},
  {'name' : 'soft_limiter',               'deps' : [sdl2_dep, libmisc_dep]},
  {'name' : 'string_utils',               'deps' : []},
  {'name' : 'setup',                      'deps' : [sdl2_dep, libmisc_dep]},
  {'name' : 'support',                    'deps' : [sdl2_dep, libmisc_dep]},
]

foreach ut : unit_tests
  name = ut.get('name')
  exe = executable(name, [name + '.cpp', 'stubs.cpp'],
                   dependencies : [gtest_dep] + ut.get('deps'),
                   include_directories : incdir)
  test('gtest ' + name, exe, suite : 'ut')
endforeach


# autoexec tests
#
autoexec_tests = [
  {'name' : 't1000_cmd_echo'},
  {'name' : 't1001_cmd_date'},
]

foreach at : autoexec_tests
  name = at.get('name')
  exe = find_program(name + '.py')
  test(name, exe, args : [dosbox_bin.full_path()],
       suite : 'at',
       depends : dosbox_bin,
       protocol : 'exitcode')
endforeach
