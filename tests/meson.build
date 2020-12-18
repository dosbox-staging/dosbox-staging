# gtest unit tests

# In general, you can add new unit test this way:
#
#     exe = executable('example', 'example.cpp',
#                      dependencies : gtest_dep)
#     test('gtest example', exe)
#
# This might be useful, when test has some specific requirements.
# In most cases, you can add a new line to unit_tests list instead.

gtest_dep = dependency('gtest', main : true, required : false)
if not gtest_dep.found()
  message('optional gtest dependency not found, unit tests are disabled')
  gtest_dep = disabler()
endif


# unit tests with specific requirements
#
# - example  - has a failing testcase (on purpose)
# - fs_utils - depends on files in: tests/files/

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
  {'name' : 'soft_limiter', 'deps' : [sdl2_dep]},
  {'name' : 'string_utils', 'deps' : []},
  {'name' : 'support',      'deps' : [sdl2_dep, libmisc_dep]},
]

foreach ut : unit_tests
  name = ut.get('name')
  exe = executable(name, name + '.cpp',
                   dependencies : [gtest_dep] + ut.get('deps'),
                   include_directories : incdir)
  test('gtest ' + name, exe)
endforeach
