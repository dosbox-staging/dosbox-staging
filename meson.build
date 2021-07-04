project('dosbox-staging', 'c', 'cpp',
        version : '0.78.0',
        license : 'GPL-2.0-or-later',
        default_options : ['cpp_std=c++17', 'b_ndebug=if-release'],
        meson_version : '>= 0.51.0')

# After increasing the minimum-required meson version, make the following
# improvements:
#
# - 0.53.0 - use summary() to communicate setup result
# - 0.55.0 - subproject wraps are automatically promoted to fallbacks,
#            stop using: "fallback : ['foo', 'foo_dep']" for dependencies
# - 0.56.0 - use meson.current_source_dir() in unit tests


# compiler flags
#
# Warnings enabled below are our baseline; use '-Dwarning_level=3' to
# turn on more.  The default level is 1 (meson turns on -Wall).
#
cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

add_project_arguments('-Weffc++', language : 'cpp')
if cxx.has_argument('-Wextra-semi')
    add_project_arguments('-Wextra-semi', language : 'cpp')
endif


# gather data for config file
#
# Actual config.h file will be generated after all interpreting build files 
# for all subdirs.
#
conf_data = configuration_data()
conf_data.set('version', meson.project_version())

if get_option('buildtype').startswith('debug')
  conf_data.set('conf_suffix', '-staging-git')
else
  conf_data.set('conf_suffix', '-staging')
endif

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
conf_data.set10('C_NE2000', get_option('use_slirp') or get_option('use_pcap'))
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

foreach header : ['pwd.h', 'strings.h', 'netinet/in.h', 'sys/socket.h']
  if cc.has_header(header)
    conf_data.set('HAVE_' + header.underscorify().to_upper(), 1)
  endif
endforeach

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

optional_dep  = dependency('', required : false)
atomic_dep    = cxx.find_library('atomic', required : false)
opus_dep      = dependency('opusfile',
                           static : ('opusfile' in static_libs_list))
threads_dep   = dependency('threads')
sdl2_dep      = dependency('sdl2', version : '>= 2.0.5',
                           static : ('sdl2' in static_libs_list))
sdl2_net_dep  = optional_dep
opengl_dep    = optional_dep
fluid_dep     = optional_dep
mt32emu_dep   = optional_dep
png_dep       = optional_dep
pcap_dep      = optional_dep
slirp_dep     = optional_dep
curses_dep    = optional_dep # necessary for debugger builds
alsa_dep      = optional_dep # Linux-only
coreaudio_dep = optional_dep # macOS-only
coremidi_dep  = optional_dep # macOS-only
winsock2_dep  = optional_dep # Windows-only
winmm_dep     = optional_dep # Windows-only


if get_option('use_sdl2_net')
  sdl2_net_dep = dependency('SDL2_net', version : '>= 2.0.0',
                            static : ('sdl2_net' in static_libs_list),
                            not_found_message : msg.format('use_sdl2_net'))
endif

if get_option('use_opengl')
  opengl_dep = dependency('gl', not_found_message : msg.format('use_opengl'))
endif

if get_option('use_fluidsynth')
  fluid_dep = dependency('fluidsynth', version : '>= 2.0.0',
                         static : ('fluidsynth' in static_libs_list),
                         fallback : ['fluidsynth', 'fluidsynth_dep'],
                         not_found_message : msg.format('use_fluidsynth'))
endif

if get_option('use_mt32emu')
  mt32emu_dep = dependency('mt32emu', version : '>= 2.5.0',
                           static : ('mt32emu' in static_libs_list),
                           fallback : ['mt32emu', 'mt32emu_dep'],
                           not_found_message : msg.format('use_mt32emu'))
endif

if get_option('use_png')
  png_dep = dependency('libpng', static : ('png' in static_libs_list),
                       not_found_message : msg.format('use_png'))
endif

if get_option('use_pcap') # disabled by default
  pcap_dep = dependency('libpcap')
endif

if get_option('use_slirp') # disabled by default
  slirp_dep = dependency('slirp')
endif

if get_option('enable_debugger') != 'none'
  curses_dep = dependency('ncurses') # or pdcurses on Windows
endif

# macOS-only dependencies
#
if host_machine.system() == 'darwin'
  coreaudio_dep = dependency('appleframeworks',
                             modules : ['CoreAudio', 'AudioUnit', 'AudioToolbox'],
                             required : false)
  if coreaudio_dep.found()
    conf_data.set('C_COREAUDIO', 1)
  else
    warning('''Core Audio support disabled: compiler can't detect/use Apple Frameworks''')
  endif

  coremidi_dep = dependency('appleframeworks',
                             modules : ['CoreMIDI', 'CoreFoundation'],
                             required : false)
  if coremidi_dep.found()
    conf_data.set('C_COREMIDI', 1)
  else
    warning('''Core MIDI support disabled: compiler can't detect/use Apple Frameworks''')
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

# include directories
#
incdir = include_directories('include', '.')


# bundled dependencies
#
subdir('src/libs/decoders')
subdir('src/libs/nuked')
subdir('src/libs/ppscale')
subdir('src/libs/residfp')

libdecoders_dep = declare_dependency(link_with : libdecoders)
libnuked_dep    = declare_dependency(link_with : libnuked)
libppscale_dep  = declare_dependency(link_with : libppscale)
libresidfp_dep  = declare_dependency(link_with : libresidfp)


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
# Some tests use relative paths; in meson 0.56.0 this can be replaced
# with meson.project_source_root().
project_source_root = meson.current_source_dir()
subdir('tests')


# dosbox executable
#
version_file = vcs_tag(input : 'src/version.cpp.in', output : 'version.cpp')
executable('dosbox', ['src/main.cpp', 'src/dosbox.cpp', version_file],
           dependencies : [atomic_dep, threads_dep, sdl2_dep] + internal_deps,
           include_directories : incdir,
           install : true)


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
