project(
    'dosbox-staging',
    'c',
    'cpp',
    version: '0.81.0-alpha',
    license: 'GPL-3.0-or-later',
    meson_version: '>= 0.57.0',
    default_options: [
        'cpp_std=c++17',
        'buildtype=release',
        'b_ndebug=if-release',
        'b_staticpic=false',
        'b_pie=false',
        'warning_level=3',
        'glib:b_staticpic=true',
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
        'libjpeg-turbo:b_staticpic=true',
        'libpng:b_staticpic=true',
        'sdl2_image:b_staticpic=true',
    ],
)

# Gather internal resource dependencies
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# These are always present regardless of host, compiler, dependencies, or options.
#
data_dir = get_option('datadir')
licenses_dir = data_dir / 'licenses' / meson.project_name()
doc_dir = data_dir / 'doc' / meson.project_name()

install_man('docs/dosbox.1')
install_data('COPYING', install_dir: licenses_dir)
install_data('AUTHORS', 'README', 'THANKS', install_dir: doc_dir)

subdir('contrib/linux')
subdir('contrib/icons')
subdir('contrib/resources')

# Gather OS family type
# ~~~~~~~~~~~~~~~~~~~~~
#
os_family_name = {
    'linux': 'LINUX',
    'windows': 'WIN32',
    'cygwin': 'WIN32',
    'darwin': 'MACOSX',
    'freebsd': 'BSD',
    'netbsd': 'BSD',
    'openbsd': 'BSD',
    'dragonfly': 'BSD',
}.get(host_machine.system(), 'UNKNOWN_OS')


# Gather compiler settings
# ~~~~~~~~~~~~~~~~~~~~~~~~
#
cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')
prefers_static_libs = (get_option('default_library') == 'static')

summary('Build type', get_option('buildtype'), section: 'Build Summary')
summary('Install prefix', get_option('prefix'), section: 'Build Summary')

# extra build flags
extra_flags = ['-Wno-unknown-pragmas']
# generate linker map file
extra_link_flags = [
    '-Wl,-map,dosbox.map',
    '-Wl,-Map,dosbox.map',
    '-Wl,--Map,dosbox.map',
]

# If the compiler provides std::filesystem, then we consider it modern enough
# that we can trust it's extra helpful warnings to let us improve the code quality.
if cxx.has_header('filesystem')
    extra_flags += ['-Wmaybe-uninitialized', '-Weffc++', '-Wextra-semi']
else
    # Otherwise, it's an old compiler and we're just trying to build, and don't
    # care about fixing their warnings (some generate warnings from their own STL).
    warning(
        'Compiler lacks the C++17 std::filesystem - try to upgrade your compiler!',
    )
endif

if get_option('buildtype') in ['release', 'minsize']
    # For release and small build types, we're not anticipating
    # needing debuggable floating point signals.
    # These safety measures are still enabled in debug builds,
    # so if an issue is reported where these happen help, then
    # testing with debug builds will make use of them.
    #
    extra_flags += [
        '-fstrict-aliasing',
        '-Wstrict-aliasing',
        '-fmerge-all-constants',
        '-fno-math-errno',
        '-fno-signed-zeros',
        '-fno-trapping-math',
        '-fassociative-math',
        '-freciprocal-math',
        '-ffinite-math-only',
        '-frename-registers',
        '-ffunction-sections',
        '-fdata-sections',
    ]
    extra_link_flags += ['-Wl,--gc-sections']
endif

if get_option('asm')
    extra_flags += ['--save-temps', '/FAs']
endif

# Don't flood us with hundreds of suggestions to use Microsoft-specific calls
if host_machine.system() == 'windows'
    extra_flags += '-Wno-pedantic-ms-format'
endif

# Don't flood us with hundreds of suggestions to use snprintf on Apple + Clang
if host_machine.system() == 'darwin' and  cxx.get_id() == 'clang'
    extra_flags += '-Wno-deprecated-declarations'
endif

if prefers_static_libs
    extra_flags += ['-static-libstdc++', '-static-libgcc']
    if host_machine.system() != 'darwin'
        extra_link_flags += ['-no-pie']
    endif
else
    extra_flags += [
        '-shared-libstdc++',
        '-shared-libgcc',
        '-lstdc++_s',
        '-fPIC',
    ]
endif

if get_option('narrowing_warnings')
    extra_flags += ['-Wconversion', '-Wnarrowing']
endif

if get_option('autovec_info')
    # At least O2 is needed enable auto-vectorizion
    extra_flags += [
        '-march=native',
        '-O2',
        '-Wno-system-headers',
        '-Rpass-analysis=loop-vectorize',
        '-fopt-info-vec-missed',
        '-fopt-info-vec',
    ]
endif

# Tag BSD executables with the WX-needed bit
if os_family_name == 'BSD'
    extra_link_flags += ['-Wl,-z,wxneeded']
endif

# Allow-list the flags against the compiler, and add them to the project
foreach flag : extra_flags
    if cc.has_argument(flag)
        add_project_arguments(flag, language: 'c')
    endif
    if cxx.has_argument(flag)
        add_project_arguments(flag, language: 'cpp')
    endif
endforeach
foreach flag : extra_link_flags
    if cxx.has_link_argument(flag)
        add_project_link_arguments(flag, language: 'cpp')
    endif
endforeach


# Gather data to populate config.h
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# The actual config.h file will be generated after interpreting
# all the build files in all the subdirs.
#
conf_data = configuration_data()
conf_data.set('version', meson.project_version())
conf_data.set('project_name', meson.project_name())
conf_data.set_quoted(
    'CUSTOM_DATADIR',
    get_option('prefix') / get_option('datadir'),
)

conf_data.set10(os_family_name, true)
conf_data.set10('C_MODEM', get_option('use_sdl2_net'))
conf_data.set10('C_IPX', get_option('use_sdl2_net'))
conf_data.set10('C_SLIRP', get_option('use_slirp'))
conf_data.set10('C_NE2000', get_option('use_slirp'))
conf_data.set10('C_FLUIDSYNTH', get_option('use_fluidsynth'))
conf_data.set10('C_MT32EMU', get_option('use_mt32emu'))
conf_data.set10('C_SSHOT', get_option('use_png'))
conf_data.set10('C_SDL_IMAGE', get_option('use_sdl2_image'))
conf_data.set10('C_TRACY', get_option('tracy'))
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

if cc.has_function('clock_gettime', prefix: '#include <time.h>')
    conf_data.set10('HAVE_CLOCK_GETTIME', true)
endif

if cc.has_function('__builtin_available')
    conf_data.set10('HAVE_BUILTIN_AVAILABLE', true)
endif

if cc.has_function('__builtin___clear_cache')
    conf_data.set10('HAVE_BUILTIN_CLEAR_CACHE', true)
endif

if cc.has_function('mprotect', prefix: '#include <sys/mman.h>')
    conf_data.set10('HAVE_MPROTECT', true)
endif

if cc.has_function('mmap', prefix: '#include <sys/mman.h>')
    conf_data.set10('HAVE_MMAP', true)
endif

if cc.has_header_symbol('sys/mman.h', 'MAP_JIT')
    conf_data.set10('HAVE_MAP_JIT', true)
endif

if cc.has_function(
    'pthread_jit_write_protect_np',
    prefix: '#include <pthread.h>',
)
    conf_data.set10('HAVE_PTHREAD_WRITE_PROTECT_NP', true)
endif

if cc.has_function(
    'sys_icache_invalidate',
    prefix: '#include <libkern/OSCacheControl.h>',
)
    conf_data.set10('HAVE_SYS_ICACHE_INVALIDATE', true)
endif

if cxx.has_function(
    'pthread_setname_np',
    prefix: '#include <pthread.h>',
    dependencies: dependency('threads'),
)
    conf_data.set10('HAVE_PTHREAD_SETNAME_NP', true)
endif

if cc.has_function('realpath', prefix: '#include <stdlib.h>')
    conf_data.set10('HAVE_REALPATH', true)
endif

# strnlen was originally a Linux-only function
if cc.has_function('strnlen', prefix: '#include <string.h>')
    conf_data.set10('HAVE_STRNLEN', true)
endif

if cc.has_member('struct dirent', 'd_type', prefix: '#include <dirent.h>')
    conf_data.set10('HAVE_STRUCT_DIRENT_D_TYPE', true)
endif

foreach header : ['pwd.h', 'strings.h', 'netinet/in.h']
    if cc.has_header(header)
        conf_data.set10('HAVE_' + header.underscorify().to_upper(), true)
    endif
endforeach

# Check for the actual calls we need in socket.h, because some systems
# have socket.h but are missing some calls.
if cc.has_header('sys/socket.h')
    if (
        cc.has_function('getpeername', prefix: '#include <sys/socket.h>')
        and cc.has_function('getsockname', prefix: '#include <sys/socket.h>')
    )
        conf_data.set10('HAVE_SYS_SOCKET_H', true)
    endif
endif

# Header windows.h defines old min/max macros, that conflict with C++11
# std::min/std::max.  Defining NOMINMAX prevents these macros from appearing.
if cxx.get_id() == 'msvc'
    conf_data.set10('NOMINMAX', true)
endif

if host_machine.system() in ['windows', 'cygwin']
    conf_data.set10('_USE_MATH_DEFINES', true)
endif

if host_machine.endian() == 'big'
    conf_data.set10('WORDS_BIGENDIAN', true)
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
if cc.compiles(set_prio_code, name: 'test for setpriority support')
    conf_data.set10('HAVE_SETPRIORITY', true)
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
if cxx.compiles(builtin_expect_code, name: 'test for __builtin_expect support')
    conf_data.set10('C_HAS_BUILTIN_EXPECT', true)
endif

atomic_code = '''
  #include <atomic>
  #include <cstdint>
 int main() {
  std::atomic<std::int32_t> x32 = 1;
  std::atomic<std::int64_t> x64 = 1;
  return static_cast<int>(x64.load() - x32.load());
 }
 '''

atomic_external_dep = dependency('atomic', required: false)
atomic_internal_dep = declare_dependency(link_args: '-latomic')

if cxx.links(
    atomic_code,
    dependencies: atomic_external_dep,
    name: 'compiler supports atomic types using external library',
)
    atomic_dep = atomic_external_dep
elif cxx.links(
    atomic_code,
    dependencies: atomic_internal_dep,
    name: 'compiler supports atomic types using internal library',
)
    atomic_dep = atomic_internal_dep
else
    atomic_dep = dependency('atomic')
endif

# Gather external dependencies
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
# system and compiler libraries

# Optional libraries
optional_dep = dependency('', required: false)
msg = 'You can disable this dependency with: -D@0@=false'

default_wrap_options = ['default_library=static', 'warning_level=0']

dl_dep = cc.find_library('dl', required: false)
stdcppfs_dep = cxx.find_library('stdc++fs', required: false)
threads_dep = dependency('threads')

# 3rd party libraries
static_libs_list = get_option('try_static_libs')

wants_tests = true
# tests are disabled for release builds unless requested
if get_option('buildtype') == 'release' and get_option('unit_tests').auto()
    wants_tests = false
elif get_option('unit_tests').disabled()
    wants_tests = false
endif

libiir_dep = dependency(
    'iir',
    version: ['>= 1.9.3', '< 2'],
    default_options: default_wrap_options + ['tests=' + wants_tests.to_string()],
    static: ('iir' in static_libs_list or prefers_static_libs),
)

opus_dep = dependency(
    'opusfile',
    version: ['>= 0.8', '< 1'],
    static: ('opusfile' in static_libs_list or prefers_static_libs),
)

sdl2_dep = dependency(
    'sdl2',
    version: ['>= 2.0.5', '< 3'],
    static: ('sdl2' in static_libs_list),
)

zlib_dep = dependency(
    'zlib',
    version: ['>= 1.2.11', '< 2'],
    default_options: default_wrap_options,
    static: ('zlib' in static_libs_list or prefers_static_libs),
)

# SpeexDSP
# ~~~~~~~~
# Default to the system library
speexdsp_dep = dependency(
    'speexdsp',
    version: ['>= 1.2', '< 2'],
    required: false,
    fallback: [],
    static: ('speexdsp' in static_libs_list or prefers_static_libs),
)

# The library needs to be available and testable to be trusted
can_trust_system_speexdsp = (
    speexdsp_dep.found()
    and meson.can_run_host_binaries()
)

# Test the library. Trust is dropped if the test fails.
if can_trust_system_speexdsp
    system_speexdsp_test = cxx.run(
        files('contrib/check-speexdsp/test_speexdsp_float_api.cpp'),
        dependencies: speexdsp_dep,
        name: 'SpeexDSP system library has reliable floating-point API',
    )
    can_trust_system_speexdsp = (
        system_speexdsp_test.compiled()
        and system_speexdsp_test.returncode() == 0
    )
    if can_trust_system_speexdsp
        speexdsp_summary_msg = 'system library'
    endif
endif

# Use the wrap if the system doesn't have SpeexDSP, we couldn't test it, or testing failed
if not can_trust_system_speexdsp
    speexdsp_dep = subproject(
        'speexdsp',
        default_options: default_wrap_options,
    ).get_variable('speexdsp_dep')
    speexdsp_summary_msg = 'built-in'
endif
summary('SpeexDSP provider', speexdsp_summary_msg)

# File-descriptor manipulation routines, such as FD_ZERO, are used
# by Enet, slirp, and ManyMouse's X11 interface. Unfortunately these
# routines aren't universally available, such as on Android.
#
have_fd_zero = (
    cc.has_header_symbol('sys/select.h', 'FD_ZERO')
    or cc.has_header_symbol('winsock2.h', 'FD_ZERO')
)

# SDL Networking
sdl2_net_dep = optional_dep
sdl2_net_summary_msg = 'Disabled'
if get_option('use_sdl2_net')
    sdl2_net_dep = dependency(
        'SDL2_net',
        version: ['>= 2.0.0', '< 3'],
        static: ('sdl2_net' in static_libs_list),
        not_found_message: msg.format('use_sdl2_net'),
    )
    sdl2_net_summary_msg = sdl2_net_dep.found()

    if sdl2_net_dep.found() and not have_fd_zero
        sdl2_net_dep = optional_dep
        sdl2_net_summary_msg = 'Disabled due to host missing file-descriptor routines'
    endif

endif
summary('SDL_net 2.0 support', sdl2_net_summary_msg)

# slirp (depends on glib)
libslirp_dep = optional_dep
libslirp_summary_msg = 'Disabled'
if get_option('use_slirp')
    libslirp_dep = dependency(
        'slirp',
        version: ['>= 4.6.1', '< 5'],
        default_options: default_wrap_options,
        static: ('slirp' in static_libs_list or prefers_static_libs),
        not_found_message: msg.format('use_slirp'),
    )

    libslirp_summary_msg = libslirp_dep.found()

    if libslirp_dep.found() and not have_fd_zero
        libslirp_summary_msg = 'Disabled due to host missing file-descriptor routines'
        libslirp_dep = optional_dep
    endif

endif
summary('slirp support', libslirp_summary_msg)


# SDL Image
sdl2_image_dep = optional_dep
if get_option('use_sdl2_image')
    sdl2_image_dep = dependency(
        'SDL2_image',
        version: ['>= 2.0.0', '< 3'],
        default_options: default_wrap_options,
        static: ('sdl2_image' in static_libs_list or prefers_static_libs),
        not_found_message: msg.format('use_sdl2_image'),
    )
endif
summary('SDL_image 2.0 support', sdl2_image_dep.found())

# OpenGL
opengl_dep = optional_dep
opengl_summary_msg = 'Disabled'
if get_option('use_opengl') != 'false'
    opengl_dep = dependency('gl', not_found_message: msg.format('use_opengl'))
endif
if (
    opengl_dep.found()
    and host_machine.system() == 'linux'
    and get_option('use_opengl') == 'auto'
    and get_option('enable_debugger') != 'none'
)

    warning(
        '''OpenGL is disabled because it's not compatible with the debugger.
            To use OpenGL with the debugger, force it with "-Duse_opengl=true"''',
    )

    conf_data.set10('C_OPENGL', false)
    opengl_summary_msg = 'Found, but disabled due to conflict with debugger'
else
    conf_data.set10('C_OPENGL', opengl_dep.found())
    opengl_summary_msg = opengl_dep.found()
endif
summary('OpenGL support', opengl_summary_msg)

# FluidSynth (depends on glib)
fluid_dep = optional_dep
if get_option('use_fluidsynth')
    fluid_dep = dependency(
        'fluidsynth',
        version: ['>= 2.2.3', '< 3'],
        modules: ['FluidSynth::libfluidsynth'],
        static: ('fluidsynth' in static_libs_list or prefers_static_libs),
        not_found_message: msg.format('use_fluidsynth'),
        default_options: [
            'default_library=static',
            'try-static-deps=true',
            'enable-floats=true',
            'openmp=disabled',
            'enable-threads=false',
            'tests=' + wants_tests.to_string(),
            'warning_level=0',
        ],
    )
endif
summary('FluidSynth support', fluid_dep.found())

# mt32emu
mt32emu_dep = optional_dep
if get_option('use_mt32emu')
    mt32emu_dep = dependency(
        'mt32emu',
        version: ['>= 2.5.3', '< 3'],
        default_options: default_wrap_options,
        static: ('mt32emu' in static_libs_list or prefers_static_libs),
        not_found_message: msg.format('use_mt32emu'),
    )
endif
summary('mt32emu support', mt32emu_dep.found())

# PNG
png_dep = optional_dep
if get_option('use_png')
    png_dep = dependency(
        'libpng',
        version: ['>= 1.2', '< 2'],
        static: ('png' in static_libs_list or prefers_static_libs),
        not_found_message: msg.format('use_png'),
    )
endif
summary('PNG support', png_dep.found())

# Tracy
tracy_dep = optional_dep
if get_option('tracy')
    tracy_dep = dependency(
        'tracy',
        version: ['>= 0.9', '< 1'],
        default_options: default_wrap_options,
        static: ('tracy' in static_libs_list or prefers_static_libs),
        not_found_message: msg.format('tracy'),
    )
    add_project_arguments('-g', language: ['c', 'cpp'])
    add_project_arguments('-fno-omit-frame-pointer', language: ['c', 'cpp'])
endif

# macOS-only dependencies
coreaudio_dep = optional_dep
coremidi_dep = optional_dep
corefoundation_dep = optional_dep
iokit_dep = optional_dep
if host_machine.system() == 'darwin'

    # ObjectiveC parsing, if possible
    if cxx.has_argument('-lobjc')
        add_project_arguments('-lobjc', language: 'cpp')
    endif

    # Core Audio
    coreaudio_dep = dependency(
        'appleframeworks',
        modules: ['CoreAudio', 'AudioUnit', 'AudioToolbox'],
        required: false,
    )
    if coreaudio_dep.found()
        if cxx.check_header('AudioToolbox/AUGraph.h')
            conf_data.set10('C_COREAUDIO', true)
        else
            warning('''Core Audio disabled because header is unusable''')
            coreaudio_dep = disabler()
        endif
    else
        warning('''Core Audio disabled because Apple Framework missing''')
    endif
    summary('CoreAudio support', coreaudio_dep.found())

    # Core MIDI
    coremidi_dep = dependency(
        'appleframeworks',
        modules: ['CoreMIDI', 'CoreFoundation'],
        required: false,
    )
    if coremidi_dep.found()
        if cxx.check_header('CoreMIDI/MIDIServices.h')
            conf_data.set10('C_COREMIDI', true)
        else
            warning('''Core Audio disabled because header is unusable''')
            coremidi_dep = disabler()
        endif
    else
        warning('''Core MIDI disabled because Apple Framework missing''')
    endif
    summary('CoreMIDI support', coremidi_dep.found())

    # IOKit
    iokit_dep = dependency(
        'appleframeworks',
        modules: ['IOKit'],
        required: false,
    )
    if iokit_dep.found()
        if cxx.check_header('IOKit/IOKitLib.h')
            iokit_code = '''
                #include <IOKit/hid/IOHIDLib.h>
                int main() {
                    dispatch_block_t test_var;
                    return 0;
                }
            '''
            is_iokit_compilable = cxx.links(
                iokit_code,
                name: 'compiler is capable of compiling IOKit',
            )
            if is_iokit_compilable
                conf_data.set10('C_IOKIT', true)
            else
                warning('''IOKit disabled because compiler cannot handle it''')
                iokit_dep = disabler()
            endif
        else
            warning('''IOKit disabled because header is unusable''')
        endif
    else
        warning('''IOKit disabled because Apple Framework missing''')
    endif
    summary('IOKit support', iokit_dep.found())

    # Locale discovery
    corefoundation_dep = dependency(
        'appleframeworks',
        modules: ['CoreFoundation'],
        required: false,
    )
    if corefoundation_dep.found()
        if cxx.check_header('CoreFoundation/CoreFoundation.h')
            conf_data.set10('C_COREFOUNDATION', true)
        else
            warning('''Core Foundation disabled because header is unusable''')
            corefoundation_dep = disabler()
        endif
    else
        warning('''Core Foundation disabled becaue Foundation missing''')
    endif
    summary('CoreFoundation support', corefoundation_dep.found())

    # SDL CD dependency
    coreservices_dep = dependency(
        'appleframeworks',
        modules: ['CoreServices'],
        required: false,
    )
    if coreservices_dep.found()
        if cxx.check_header('CoreServices/CoreServices.h')
            conf_data.set10('C_CORESERVICES', true)
        else
            warning('''Core Services disabled because header is unusable''')
            coreservices_dep = disabler()
        endif
    else
        warning('''Core Services disabled because Frameworks is missing''')
    endif
    summary('CoreServices support', coreservices_dep.found())

    # Apple Silicon has 16k pages
    pagesize_cmd = run_command('pagesize', check: true)
    if pagesize_cmd.returncode() != 0
        error('''error executing pagesize''')
    else
        pagesize = pagesize_cmd.stdout().strip().to_int()
        if pagesize != 4096
            conf_data.set('PAGESIZE', pagesize)
        endif
    endif
endif

# Determine if system is capable of using ManyMouse library
conf_data.set10('C_MANYMOUSE', true)
manymouse_summary_msg = 'Enabled'

# ManyMouse optionally supports the X Input 2.0 protocol (regardless of OS)
xinput2_dep = optional_dep
xinput2_required = (get_option('use_xinput2') == 'true')
if xinput2_required or (get_option('use_xinput2') == 'auto')
    xinput2_dep = dependency(
        'xi',
        version: ['>= 1.4', '< 2'],
        required: xinput2_required,
    )
endif
conf_data.set10('SUPPORT_XINPUT2', xinput2_dep.found())

if os_family_name == 'MACOSX'
    if not iokit_dep.found()
        manymouse_summary_msg = 'Disabled due to host missing IOKit'
        conf_data.set10('C_MANYMOUSE', false)
    endif
endif
if os_family_name in ['LINUX', 'BSD']
    if not have_fd_zero
        manymouse_summary_msg = 'Disabled due to host missing file-descriptor routines'
        conf_data.set10('C_MANYMOUSE', false)
    endif
endif
summary('ManyMouse support', manymouse_summary_msg)


# Linux-only dependencies
alsa_dep = optional_dep
using_linux = (host_machine.system() == 'linux')
force_alsa = (get_option('use_alsa') == 'true')
if force_alsa or (using_linux and get_option('use_alsa') == 'auto')
    alsa_dep = dependency('alsa', version: ['>= 1', '< 2'])
    conf_data.set10('C_ALSA', true)
    summary('ALSA support', alsa_dep.found())
endif

# Windows-only dependencies
winsock2_dep = optional_dep
winmm_dep = optional_dep
if host_machine.system() in ['windows', 'cygwin']
    winsock2_dep = cxx.find_library('ws2_32', required: true)
    summary('Winsock 2 support', winsock2_dep.found())

    winmm_dep = cxx.find_library('winmm', required: true)
    summary('Windows Multimedia support', winmm_dep.found())
endif

# Setup include directories
incdir = include_directories('include', '.')

# A list of DOSBox's internal libraries populated
# by each of the src/ subdir imports below.
internal_deps = []

# bundled dependencies, in dependency-order
#
subdir('src/libs/ghc')
subdir('src/libs/loguru')
subdir('src/libs/decoders')
subdir('src/libs/nuked')
subdir('src/libs/ppscale')
subdir('src/libs/residfp')
subdir('src/libs/sdlcd')
subdir('src/libs/whereami')
subdir('src/libs/YM7128B_emu')
if conf_data.get('C_MANYMOUSE') != 0
    subdir('src/libs/manymouse')
endif

# ZMBV and TalChorus use some support functionality from misc
subdir('src/misc')
subdir('src/libs/zmbv')
subdir('src/libs/tal-chorus')

# A list of DOSBox's bundled 3rd party dependencies,
# as defined by the above subdir includes. Used for
# both the executable and libdosbox (unit testing).
third_party_deps = [
    atomic_dep,
    stdcppfs_dep,
    sdl2_dep,
    sdl2_image_dep,
    threads_dep,
    ghc_dep,
    libiir_dep,
    libloguru_dep,
    libsdlcd_dep,
    tracy_dep,
    libwhereami_dep,
    libtalchorus_dep,
]

if conf_data.get('C_MANYMOUSE') != 0
    third_party_deps += manymouse_dep
endif

# internal libs
subdir('src/cpu')
subdir('src/dos')
subdir('src/fpu')
subdir('src/gui')
subdir('src/hardware')
subdir('src/ints')
subdir('src/midi')
subdir('src/shell')

# debugger-specific libs
if get_option('enable_debugger') != 'none'
    subdir('src/libs/PDCurses')
    subdir('src/debug')
    third_party_deps += libpdcurses_dep
endif

# generate config.h
configure_file(
    input: 'src/config.h.in',
    output: 'config.h',
    configuration: conf_data,
)


# Setup the executable and libraries
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
version_file = vcs_tag(input: 'src/version.cpp.in', output: 'version.cpp')
dosbox_sources = ['src/main.cpp', 'src/dosbox.cpp', version_file]

# Add Windows resources file if building on Windows
if host_machine.system() == 'windows'
    winmod = import('windows')
    res_file = winmod.compile_resources('src/winres.rc')
    dosbox_sources += res_file
endif

executable(
    'dosbox',
    dosbox_sources,
    dependencies: internal_deps + third_party_deps,
    include_directories: incdir,
    install: true,
)

# create a library so we can test things inside DOSBOX dep path
libdosbox = static_library(
    'dosbox',
    ['src/dosbox.cpp', version_file],
    include_directories: incdir,
    dependencies: internal_deps + third_party_deps,
)

dosbox_dep = declare_dependency(link_with: libdosbox)


# Setup unit tests
# ~~~~~~~~~~~~~~~~
# Some tests use relative paths; in meson 0.56.0 this can be replaced
# with meson.project_source_root().
#
if wants_tests
    project_source_root = meson.current_source_dir()
    subdir('tests')
endif
