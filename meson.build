project('dosbox-staging', 'c', 'cpp',
  version : '0.79.0',
  license : 'GPL-2.0-or-later',
  meson_version : '>= 0.54.2',
  default_options : [
    'cpp_std=c++17',
    'warning_level=3',
    'b_ndebug=if-release',
    'b_staticpic=false',
    'fluidsynth:enable-floats=true',
    'fluidsynth:openmp=disabled',
    'fluidsynth:enable-threads=false',
    'fluidsynth:try-static-deps=false',
    'fluidsynth:warning_level=0',
    'glib:glib_assert=false',
    'glib:glib_checks=false',
    'glib:glib_debug=disabled',
    'glib:libmount=disabled',
    'glib:libelf=disabled',
    'glib:nls=disabled',
    'glib:tests=false',
    'glib:warning_level=0',
    'glib:xattr=false',
    'gtest:warning_level=0',
    'mt32emu:warning_level=0',
    'slirp:warning_level=0',
  ]
)

# After increasing the minimum-required meson version, make the following
# improvements:
#
# - 0.55.0 - subproject wraps are automatically promoted to fallbacks,
#            stop using: "fallback : ['foo', 'foo_dep']" for dependencies
# - 0.56.0 - use meson.current_source_dir() in unit tests

cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')
prefers_static_libs = (get_option('default_library') == 'static')

summary('Build type',     get_option('buildtype'), section : 'Build Summary')
summary('Install prefix', get_option('prefix'),    section : 'Build Summary')

# extra compiler flags
extra_flags = []

# If the compiler provides std::filesystem, then we consider it modern enough
# that we can trust it's extra helpful warnings to let us improve the code quality.
if cxx.has_header('filesystem')
  extra_flags += ['-Wmaybe-uninitialized', '-Weffc++', '-Wextra-semi']
else
  # Otherwise, it's an old compiler and we're just trying to build, and don't
  # care about fixing their warnings (some generate warnings from their own STL).
  warning('Compiler lacks the C++17 std::filesystem - try to upgrade your compiler!')
endif

if host_machine.system() == 'windows'
  extra_flags += '-Wno-pedantic-ms-format'
endif
if prefers_static_libs
  extra_flags += ['-static-libstdc++', '-static-libgcc']
else
  extra_flags += ['-shared-libstdc++', '-shared-libgcc', '-fPIC']
endif
foreach flag : extra_flags
  if cc.has_argument(flag)
    add_project_arguments(flag, language : 'c')
  endif
  if cxx.has_argument(flag)
    add_project_arguments(flag, language : 'cpp')
  endif
endforeach

# gather data for config file
#
# Actual config.h file will be generated after all interpreting build files 
# for all subdirs.
#
conf_data = configuration_data()
conf_data.set('version', meson.project_version())

os_family_name = {
  'linux'     : 'LINUX',
  'windows'   : 'WIN32',
  'cygwin'    : 'WIN32',
  'darwin'    : 'MACOSX',
  'freebsd'   : 'BSD',
  'netbsd'    : 'BSD',
  'openbsd'   : 'BSD',
  'dragonfly' : 'BSD',
}.get(host_machine.system(), 'UNKNOWN_OS')
conf_data.set(os_family_name, 1)

conf_data.set10('C_MODEM', get_option('use_sdl2_net'))
conf_data.set10('C_IPX', get_option('use_sdl2_net'))
conf_data.set10('C_SLIRP', get_option('use_slirp'))
conf_data.set10('C_NE2000', get_option('use_slirp'))
conf_data.set10('C_OPENGL', get_option('use_opengl'))
conf_data.set10('C_FLUIDSYNTH', get_option('use_fluidsynth'))
conf_data.set10('C_MT32EMU', get_option('use_mt32emu'))
conf_data.set10('C_SSHOT', get_option('use_png'))
conf_data.set10('C_FPU', true)
conf_data.set10('C_FPU_X86', host_machine.cpu_family() in ['x86', 'x86_64'])

if get_option('enable_debugger') != 'none'
  conf_data.set10('C_DEBUG', true)
endif

if get_option('enable_debugger') == 'heavy'
  conf_data.set10('C_HEAVY_DEBUG', true)
endif

foreach osdef : ['LINUX', 'WIN32', 'MACOSX', 'BSD']
  if conf_data.has(osdef)
    conf_data.set10('C_DIRECTSERIAL', true)
  endif
endforeach

if cc.has_function('clock_gettime', prefix : '#include <time.h>')
  conf_data.set10('HAVE_CLOCK_GETTIME', true)
endif

if cc.has_function('__builtin_available')
  conf_data.set10('HAVE_BUILTIN_AVAILABLE', true)
endif

if cc.has_function('__builtin___clear_cache')
  conf_data.set10('HAVE_BUILTIN_CLEAR_CACHE', true)
endif

if cc.has_function('mprotect', prefix : '#include <sys/mman.h>')
  conf_data.set10('HAVE_MPROTECT', true)
endif

if cc.has_function('mmap', prefix : '#include <sys/mman.h>')
  conf_data.set10('HAVE_MMAP', true)
endif

if cc.has_header_symbol('sys/mman.h', 'MAP_JIT')
  conf_data.set10('HAVE_MAP_JIT', true)
endif

if cc.has_function('pthread_jit_write_protect_np', prefix : '#include <pthread.h>')
  conf_data.set10('HAVE_PTHREAD_WRITE_PROTECT_NP', true)
endif

if cc.has_function('sys_icache_invalidate', prefix : '#include <libkern/OSCacheControl.h>')
  conf_data.set10('HAVE_SYS_ICACHE_INVALIDATE', true)
endif

if cxx.has_function('pthread_setname_np', prefix : '#include <pthread.h>',
                    dependencies : dependency('threads'))
  conf_data.set10('HAVE_PTHREAD_SETNAME_NP', true)
endif

if cc.has_function('realpath', prefix : '#include <stdlib.h>')
  conf_data.set10('HAVE_REALPATH', true)
endif

if cc.has_member('struct dirent', 'd_type', prefix : '#include <dirent.h>')
  conf_data.set10('HAVE_STRUCT_DIRENT_D_TYPE', true)
endif

foreach header : ['pwd.h', 'strings.h', 'netinet/in.h']
  if cc.has_header(header)
    conf_data.set('HAVE_' + header.underscorify().to_upper(), 1)
  endif
endforeach

# Check for the actual calls we need in socket.h, because some systems
# have socket.h but are missing some calls.
if cc.has_header('sys/socket.h')
  if cc.has_function('getpeername', prefix : '#include <sys/socket.h>') and \
     cc.has_function('getsockname', prefix : '#include <sys/socket.h>')
    conf_data.set10('HAVE_SYS_SOCKET_H', true)
  endif
endif

# Header windows.h defines old min/max macros, that conflict with C++11
# std::min/std::max.  Defining NOMINMAX prevents these macros from appearing.
if cxx.get_id() == 'msvc'
  conf_data.set('NOMINMAX', true)
endif

if host_machine.system() in ['windows', 'cygwin']
  conf_data.set('_USE_MATH_DEFINES', true)
endif

if host_machine.endian() == 'big'
  conf_data.set('WORDS_BIGENDIAN', 1)
endif

# Non-4K memory page size is supported only for ppc64 at the moment.
# TODO re-enable ppc dynrec while working on W^X stuff
#      disabled because SVN r4424 broke compilation of ppc backends
#if host_machine.cpu_family() in ['ppc64', 'ppc64le']
#  conf_data.set('PAGESIZE', 65536)
#endif

set_prio_code = '''
#include <sys/resource.h>
int main() {
  return setpriority(PRIO_PROCESS, 0, PRIO_MIN + PRIO_MAX);
}
'''
if cc.compiles(set_prio_code, name : 'test for setpriority support')
  conf_data.set('HAVE_SETPRIORITY', 1)
endif

# New compilers can check for this feature using __has_builtin, but this is
# broken prior to Clang 10 and GCC 10, so we prefer to have this compilation
# check for now:
builtin_expect_code = '''
void fun(bool test) {
  // value of 'test' is usually going to be true
  if (__builtin_expect(test, true)) {
    /* likely branch */
  } else {
    /* unlikely branch */
  }
}
'''
if cxx.compiles(builtin_expect_code, name : 'test for __builtin_expect support')
  conf_data.set('C_HAS_BUILTIN_EXPECT', 1)
endif


# external dependencies
#
static_libs_list = get_option('try_static_libs')
msg = 'You can disable this dependency with: -D@0@=false'
optional_dep       = dependency('', required : false)
dl_dep             = cc.find_library('dl', required : false)
atomic_dep         = cxx.find_library('atomic', required : false)
stdcppfs_dep       = cxx.find_library('stdc++fs', required : false)
opus_dep           = dependency('opusfile',
                                static : ('opusfile' in static_libs_list))
threads_dep        = dependency('threads')
sdl2_dep           = dependency('sdl2', version : '>= 2.0.5',
                                static : ('sdl2' in static_libs_list))
sdl2_net_dep       = optional_dep
opengl_dep         = optional_dep
fluid_dep          = optional_dep
libslirp_dep       = optional_dep
mt32emu_dep        = optional_dep
png_dep            = optional_dep
slirp_dep          = optional_dep
curses_dep         = optional_dep # necessary for debugger builds
alsa_dep           = optional_dep # Linux-only
coreaudio_dep      = optional_dep # macOS-only
corefoundation_dep = optional_dep # macOS-only
coremidi_dep       = optional_dep # macOS-only
winsock2_dep       = optional_dep # Windows-only
winmm_dep          = optional_dep # Windows-only


if get_option('use_sdl2_net')
  sdl2_net_dep = dependency('SDL2_net', version : '>= 2.0.0',
                            static : ('sdl2_net' in static_libs_list),
                            not_found_message : msg.format('use_sdl2_net'))
endif

if get_option('use_opengl')
  opengl_dep = dependency('gl', not_found_message : msg.format('use_opengl'))
endif

# Use a single static setting for the pool of libraries that use glib
is_glib_realm_static = prefers_static_libs
foreach l : ['fluidsynth', 'slirp']
  if l in static_libs_list
    is_glib_realm_static = true
  endif
endforeach

if get_option('use_fluidsynth')
  fluid_dep = dependency('fluidsynth', version : '>= 2.2.3',
                         static : is_glib_realm_static,
                         fallback : ['fluidsynth', 'fluidsynth_dep'],
                         not_found_message : msg.format('use_fluidsynth'))
endif

if get_option('use_slirp')
  libslirp_dep = dependency('slirp', version : '>= 4.6.1',
                            static : is_glib_realm_static,
                            fallback : ['slirp', 'libslirp_dep'],
                            not_found_message : msg.format('use_slirp'))
endif

if get_option('use_mt32emu')
  mt32emu_dep = dependency('mt32emu', version : '>= 2.5.3',
                           static : prefers_static_libs or ('mt32emu' in static_libs_list),
                           fallback : ['mt32emu', 'mt32emu_dep'],
                           not_found_message : msg.format('use_mt32emu'))
endif

if get_option('use_png')
  png_dep = dependency('libpng',
                       static : prefers_static_libs or ('png' in static_libs_list),
                       not_found_message : msg.format('use_png'))
endif

if get_option('enable_debugger') != 'none'
  curses_dep = dependency('curses') # use the new 'curses' for msys2 compatibility
endif

# macOS-only dependencies
#
if host_machine.system() == 'darwin'

  # ObjectiveC parsing, if possible
  if cxx.has_argument('-lobjc')
    add_project_arguments('-lobjc', language : 'cpp')
  endif

  # Core Audio
  coreaudio_dep = dependency('appleframeworks',
                             modules : ['CoreAudio', 'AudioUnit', 'AudioToolbox'],
                             required : false)
  if coreaudio_dep.found()
    if cxx.check_header('AudioToolbox/AUGraph.h')
      conf_data.set('C_COREAUDIO', 1)
    else
      warning('''Core Audio support disabled: header is unsable''')
    endif
  else
    warning('''Core Audio support disabled: compiler can't detect/use Apple Frameworks''')
  endif

  # Core MIDI
  coremidi_dep = dependency('appleframeworks',
                             modules : ['CoreMIDI', 'CoreFoundation'],
                             required : false)
  if coremidi_dep.found()
    if cxx.check_header('CoreMIDI/MIDIServices.h')
      conf_data.set('C_COREMIDI', 1)
    else
      warning('''Core Audio support disabled: header is unsable''')
    endif
  else
    warning('''Core MIDI support disabled: compiler can't detect/use Apple Frameworks''')
  endif

  # Locale discovery
  corefoundation_dep = dependency('appleframeworks',
                                  modules : ['CoreFoundation', 'CoreServices'],
                                  required : false)
  if corefoundation_dep.found()
    if cxx.check_header('CoreFoundation/CoreFoundation.h')
      conf_data.set('C_COREFOUNDATION', 1)
    else
      warning('''Core Foundation support disabled: header is unsable''')
    endif
  else
    warning('''Core Foundation support disabled: compiler can't detect/use Apple Frameworks''')
  endif

  # Apple Silicon has 16k pages
  pagesize_cmd = run_command('pagesize')
  if pagesize_cmd.returncode() != 0
    error('''error executing pagesize''')
  else 
    pagesize = pagesize_cmd.stdout().strip().to_int()
    if pagesize != 4096
      conf_data.set('PAGESIZE', pagesize)
    endif
  endif
endif

# Linux-only dependencies
#
using_linux = (host_machine.system() == 'linux')
force_alsa  = (get_option('use_alsa') == 'true')
if force_alsa or (using_linux and get_option('use_alsa') == 'auto')
  alsa_dep = dependency('alsa')
  conf_data.set('C_ALSA', 1)
endif

# Windows-only dependencies
#
if host_machine.system() in ['windows', 'cygwin']
  winsock2_dep = cxx.find_library('ws2_32', required : true)
  winmm_dep = cxx.find_library('winmm', required : true)
endif

summary('OpenGL support', get_option('use_opengl'))


# include directories
#
incdir = include_directories('include', '.')

# bundled dependencies, in dependency-order
#
subdir('src/libs/ghc')
subdir('src/libs/loguru')
subdir('src/libs/decoders')
subdir('src/libs/nuked')
subdir('src/libs/ppscale')
subdir('src/libs/residfp')
subdir('src/libs/whereami')

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


# dosbox executable
#
version_file = vcs_tag(input : 'src/version.cpp.in', output : 'version.cpp')
dosbox_sources = ['src/main.cpp', 'src/dosbox.cpp', version_file]

# Add Windows resources file if building on Windows
if host_machine.system() == 'windows'
  winmod = import('windows')
  res_file = winmod.compile_resources('src/winres.rc')
  dosbox_sources += res_file
endif

executable('dosbox', dosbox_sources,
           dependencies : [atomic_dep,
                           stdcppfs_dep,
                           sdl2_dep,
                           threads_dep,
                           libghc_dep,
                           libloguru_dep,
                           libwhereami_dep]
                          + internal_deps,
           include_directories : incdir,
           install : true)
# create a library so we can test things inside DOSBOX dep path
libdosbox = static_library('dosbox', ['src/dosbox.cpp', version_file],
                        include_directories : incdir,
                        dependencies : [atomic_dep,
                                        threads_dep,
                                        sdl2_dep,
                                        libghc_dep,
                                        libloguru_dep]
                                      + internal_deps)

dosbox_dep = declare_dependency(link_with : libdosbox)



# tests
#
# Some tests use relative paths; in meson 0.56.0 this can be replaced
# with meson.project_source_root().
project_source_root = meson.current_source_dir()
subdir('tests')



# additional files for installation
#
data_dir     = get_option('datadir')
licenses_dir = data_dir / 'licenses' / 'dosbox-staging'
doc_dir      = data_dir / 'doc' / 'dosbox-staging'

install_man('docs/dosbox.1')
install_data('COPYING', install_dir : licenses_dir)
install_data('AUTHORS', 'README', 'THANKS', install_dir : doc_dir)

subdir('contrib/linux')
subdir('contrib/icons')
subdir('contrib/translations')
