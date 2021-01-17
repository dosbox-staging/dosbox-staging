# gtest dependency
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
gtest_dep = dependency('gtest', main : true, required : false,
                       fallback : ['gtest', 'gtest_main_dep'])
if not gtest_dep.found()
  message('optional gtest dependency not found, unit tests are disabled')
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
     should_fail : true)

fs_utils = executable('fs_utils', 'fs_utils.cpp',
                      dependencies : [gtest_dep, libmisc_dep],
                      include_directories : incdir)
test('gtest fs_utils', fs_utils,
     workdir : project_source_root, is_parallel : false)


# other unit tests
#
unit_tests = [
  {'name' : 'readerwritercircularbuffer', 'deps' : []},
  {'name' : 'soft_limiter',               'deps' : [sdl2_dep]},
  {'name' : 'string_utils',               'deps' : []},
  {'name' : 'setup',                      'deps' : [sdl2_dep, libmisc_dep]},
  {'name' : 'support',                    'deps' : [sdl2_dep, libmisc_dep]},
]

foreach ut : unit_tests
  name = ut.get('name')
  exe = executable(name, [name + '.cpp', 'stubs.cpp'],
                   dependencies : [gtest_dep] + ut.get('deps'),
                   include_directories : incdir)
  test('gtest ' + name, exe)
endforeach
