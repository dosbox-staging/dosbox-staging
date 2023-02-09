project(
    'sdl-key-logger',
    'cpp',
    default_options: [ 'cpp_std=c++11' ]
)

sdl2_dep = dependency(
    'sdl2',
    version: ['>= 2.0.5', '< 3'],
)

executable(
    'logger',
    'src/main.cpp',
    dependencies: sdl2_dep,
)
