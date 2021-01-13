project('dosbox-staging', 'c', 'cpp',
        default_options : ['cpp_std=c++14', 'b_ndebug=if-release'],
        version : '0.77.0')

assert(meson.version().version_compare('>= 0.45.1'),
       'Expecting meson 0.45.1 or newer.')

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


# gather data for config file
#
# Actual config.h file will be generated after all interpreting build files 
# for all subdirs.
#
conf_data = configuration_data()
conf_data.set('version', meson.project_version())

host_os = host_machine.system()
if host_os == 'linux'
  conf_data.set('LINUX', 1)
elif host_os == 'windows' or host_os == 'cygwin'
  conf_data.set('WIN32', 1)
elif host_os == 'darwin'
  conf_data.set('MACOSX', 1)
elif host_os.endswith('bsd') or host_os == 'dragonfly'
  conf_data.set('BSD', 1)
endif

conf_data.set10('C_MODEM', get_option('use_sdl2_net'))
conf_data.set10('C_IPX', get_option('use_sdl2_net'))
conf_data.set10('C_OPENGL', get_option('use_opengl'))
conf_data.set10('C_FLUIDSYNTH', get_option('use_fluidsynth'))
conf_data.set10('C_SSHOT', get_option('use_png'))
conf_data.set10('C_FPU', true)
conf_data.set10('C_FPU_X86', (host_machine.cpu_family() == 'x86_64' or
                              host_machine.cpu_family() == 'x86'))

if get_option('enable_debugger') != 'none'
  conf_data.set10('C_DEBUG', true)
endif

if get_option('enable_debugger') == 'heavy'
  conf_data.set10('C_HEAVY_DEBUG', true)
endif

if cc.has_function('clock_gettime', prefix : '#include <time.h>')
  conf_data.set10('HAVE_CLOCK_GETTIME', true)
endif

if cc.has_function('mprotect', prefix : '#include <sys/mman.h>')
  conf_data.set10('HAVE_MPROTECT', true)
endif

if cc.has_function('realpath', prefix : '#include <stdlib.h>')
  conf_data.set10('HAVE_REALPATH', true)
endif

if cc.has_member('struct dirent', 'd_type', prefix : '#include <dirent.h>')
  conf_data.set10('HAVE_STRUCT_DIRENT_D_TYPE', true)
endif

if cc.has_header('netinet/in.h')
  conf_data.set10('HAVE_NETINET_IN_H', true)
endif

if cc.has_header('pwd.h')
  conf_data.set10('HAVE_PWD_H', true)
endif

if cc.has_header('strings.h')
  conf_data.set10('HAVE_STRINGS_H', true)
endif

if cc.has_header('sys/socket.h')
  conf_data.set10('HAVE_SYS_SOCKET_H', true)
endif

if host_machine.endian() == 'big'
  conf_data.set('WORDS_BIGENDIAN', 1)
endif

# Non-4K memory page size is supported only for ppc64 at the moment.
if host_machine.cpu_family() == 'ppc64' or host_machine.cpu_family() == 'ppc64le'
  conf_data.set('PAGESIZE', 65536)
endif

set_prio_code = '''
#include <sys/resource.h>
int main() {
  return setpriority(PRIO_PROCESS, 0, PRIO_MIN + PRIO_MAX);
}
'''
if cc.compiles(set_prio_code, name : 'test for setpriority support')
  conf_data.set('HAVE_SETPRIORITY', 1)
endif


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

if get_option('enable_debugger') == 'none'
  curses_dep = dummy_dep
else
  curses_dep = dependency('ncurses') # or pdcurses on Windows
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


# generate config.h
#
configure_file(input : 'src/config.h.in', output : 'config.h',
               configuration : conf_data)


# tests
#
# We use dictionaries for test declarations, dictionaries were added
# in meson 0.47.0.
#
if meson.version().version_compare('>= 0.47.0')
  # Some tests use relative paths; in meson 0.56.0 this can be replaced
  # with meson.project_source_root().
  project_source_root = meson.current_source_dir()
  subdir('tests')
endif


# dosbox executable
#
executable('dosbox', ['src/main.cpp', 'src/dosbox.cpp'],
           dependencies : [threads_dep, sdl2_dep] + internal_deps,
           include_directories : incdir)
