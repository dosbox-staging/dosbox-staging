project('dosbox-staging', 'c', 'cpp',
        default_options : ['cpp_std=c++14', 'b_ndebug=if-release'],
        version : '0.77.0')


# compiler flags
#
# Warnings enabled below are our baseline; use '-Dwarning_level=3' to
# turn on more.  The default level is 1 (meson turns on -Wall).
#
cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

add_global_arguments('-Weffc++', language : 'cpp')
if cxx.has_argument('-Wextra-semi')
    add_global_arguments('-Wextra-semi', language : 'cpp')
endif


# config file
#
conf_data = configuration_data()
conf_data.set('version', meson.project_version())
conf_data.set10('C_MODEM', get_option('use_sdl2_net'))
conf_data.set10('C_IPX', get_option('use_sdl2_net'))
conf_data.set10('C_OPENGL', get_option('use_opengl'))
conf_data.set10('C_FLUIDSYNTH', get_option('use_fluidsynth'))
conf_data.set10('C_SSHOT', get_option('use_png'))

if cc.has_function('clock_gettime', prefix : '#include <time.h>')
  conf_data.set('DB_HAVE_CLOCK_GETTIME', true)
endif

if cc.has_member('struct dirent', 'd_type', prefix : '#include<dirent.h>')
  conf_data.set('DIRENT_HAS_D_TYPE', true)
endif

configure_file(input : 'src/config.h.in', output : 'config.h',
               configuration : conf_data)


# external dependencies
#
msg = 'You can disable this dependency with: -D@0@=false'
dummy_dep = dependency('', required : false)
threads_dep = dependency('threads')
sdl2_dep = dependency('sdl2', version : '>= 2.0.2')

if get_option('use_sdl2_net')
  sdl2_net_dep = dependency('SDL2_net', version : '>= 2.0.1',
                            not_found_message : msg.format('use_sdl2_net'))
else
  sdl2_net_dep = dummy_dep
endif

if get_option('use_opengl')
  opengl_dep = dependency('gl', not_found_message : msg.format('use_opengl'))
else
  opengl_dep = dummy_dep
endif

if get_option('use_fluidsynth')
  fluid_dep = dependency('fluidsynth', version : '>= 2.0.0',
                         not_found_message : msg.format('use_fluidsynth'))
else
  fluid_dep = dummy_dep
endif

if get_option('use_opusfile')
  opus_dep = dependency('opusfile', not_found_message : msg.format('use_opusfile'))
else
  opus_dep = dummy_dep
endif

if get_option('use_png')
  png_dep = dependency('libpng', not_found_message : msg.format('use_png'))
  z_dep = dependency('zlib') # libpng in Ubuntu 16.04 needs zlib linked explicitly
else
  png_dep = dummy_dep
  z_dep = dummy_dep
endif


# include directories
#
incdir = include_directories('include', '.')


# bundled dependencies
#
subdir('src/libs/decoders')
subdir('src/libs/nuked')
subdir('src/libs/ppscale')

libdecoders_dep = declare_dependency(link_with : libdecoders)
libnuked_dep    = declare_dependency(link_with : libnuked)
libppscale_dep  = declare_dependency(link_with : libppscale)


# internal libs
#
internal_deps = []
subdir('src/cpu')
subdir('src/debug')
subdir('src/dos')
subdir('src/fpu')
subdir('src/gui')
subdir('src/hardware')
subdir('src/ints')
subdir('src/midi')
subdir('src/misc')
subdir('src/shell')


# dosbox executable
#
executable('dosbox', ['src/main.cpp', 'src/dosbox.cpp'],
           dependencies : [threads_dep, sdl2_dep] + internal_deps,
           include_directories : incdir)
