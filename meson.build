project(
    'dosbox-staging', 'c', 'cpp',

    version: run_command(
      meson.project_source_root() + '/scripts/get-version.sh',
      'version', check: true,
    ).stdout().strip(),

    license: 'GPL-2.0-or-later',
    meson_version: '>= 0.59.0',
    default_options: [
        'cpp_std=c++20',
        'buildtype=release',
        'b_ndebug=if-release',
        'b_staticpic=false',
        'b_pie=false',
        'warning_level=3',
        'glib:b_staticpic=true',
        'glib:bsymbolic_functions=false',
        'glib:glib_assert=false',
        'glib:glib_checks=false',
        'glib:glib_debug=disabled',
        'glib:dtrace=false',
        'glib:force_posix_threads=false',
        'glib:libmount=disabled',
        'glib:libelf=disabled',
        'glib:nls=disabled',
        'glib:selinux=disabled',
        'glib:systemtap=false',
        'glib:sysprof=disabled',
        'glib:tests=false',
        'glib:warning_level=0',
        'glib:xattr=false',
        'gtest:warning_level=0',
        'libjpeg-turbo:b_staticpic=true',
        'libpng:b_staticpic=true',
        'pcre2:grep=false',
        'zlib-ng:c_std=c11',
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
# Bundle licenses, but skip the ones that are not relevant for
# binary distribution or allow us to not distribute the license text.
install_data(
    'LICENSE',
    'licenses/BSD-2-Clause.txt',
    'licenses/BSD-3-Clause.txt',
    'licenses/DEBUG.COM.txt',
    'licenses/GPL-2.0.txt',
    'licenses/LGPL-2.1.txt',
    'licenses/MIT.txt',
    'licenses/Zlib.txt',
    install_dir: licenses_dir,
)
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
extra_flags = [
    '-Wno-unknown-pragmas',
    '-fpch-preprocess',
]

warnings = []

# Enable additional warnings
foreach flag : [
    '-Walloca',
    '-Wctor-dtor-privacy',
    '-Wdate-time',
    '-Wdisabled-optimization',
    '-Wduplicated-branches',
    '-Weffc++',
    '-Wextra-semi',
    '-Wlogical-op',
    '-Wlogical-not-parentheses',
    '-Wredundant-decls',
    '-Wmismatched-tags',
    '-Wsizeof-pointer-div',
    '-Wstack-protector',
    '-Wstrict-null-sentinel',
    '-Wsuggest-override',
    '-Wzero-as-null-pointer-constant',
]
    if cxx.has_argument(flag)
        warnings += flag
    endif
endforeach

# Ignore some warnings enabled by default
foreach flag : ['-Wno-format-security']
    if cxx.has_argument(flag)
        warnings += flag
    endif
endforeach

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

# As of commit e73aaa57, overriding CXXFLAGS with -Os produces a binary that hangs
# when using a relase build with GCC.
#
# CXXFLAGS="-Os" meson setup --buildtype=release --native-file=.github/meson/native-gcc.ini build
#
# This is most likely a GCC bug because the optimization level should not change
# functionality. All other -O-levels work. Clang also handles this (replace
# native-gcc.ini with native-clang.ini to test it).
#
if get_option('buildtype') == 'release' and cxx.get_id() == 'gcc'
    cxxflags_env = run_command('sh', '-c', 'echo "$CXXFLAGS"', check: false)
    if cxxflags_env.returncode() == 0 and cxxflags_env.stdout().contains('-Os')
        error(
            '\n\n',
            'Overriding CXXFLAGS with the size-optimization flag, "-Os", when\n',
            'performing a release build with GCC produces a binary that hangs.\n',
            'Use the "minsize" buildtype instead.',
        )
    endif
endif

is_optimized_buildtype = (
    get_option('buildtype') in ['release', 'minsize', 'debugoptimized']
)
if is_optimized_buildtype
    # For optimized build types, we're not anticipating
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
        '-frename-registers',
        '-ffunction-sections',
        '-fdata-sections',
    ]
    extra_link_flags += ['-Wl,--gc-sections']
endif

# Let sanitizer builds recover and continue
if get_option('b_sanitize') != 'none'
    extra_flags += ['-fsanitize-recover=all']
endif

# Add Debug-specific flags here
if get_option('buildtype').startswith('debug')

    # Use GCC's and Clang's maximum check flags except
    # for macOS and Windows, where Xcode and MSYS2 both
    # fails with undefined symbols.
    if os_family_name not in ['MACOSX', 'WIN32']
        extra_flags += [
            '--enable-concept-checks',
            '-D_GLIBCXX_ASSERTIONS=1',
            '-D_GLIBCXX_DEBUG=1',
            '-D_GLIBCXX_DEBUG_PEDANTIC=1',
            '-D_GLIBCXX_SANITIZE_VECTOR=1',
            '-D_LIBCPP_DEBUG=1',
            '-D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1',
            '-D_LIBCPP_ENABLE_HARDENED_MODE=1',
        ]
    endif
endif

if get_option('asm')
    extra_flags += ['--save-temps', '/FAs']
endif

if get_option('time_trace')
    extra_flags += ['-ftime-trace']
endif

# Don't flood us with hundreds of suggestions to use Microsoft-specific calls
if host_machine.system() == 'windows'
    extra_flags += '-Wno-pedantic-ms-format'
endif

# Don't flood us with hundreds of suggestions to use snprintf on Apple + Clang
if host_machine.system() == 'darwin' and cxx.get_id() == 'clang'
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
        '-fPIC',
    ]
    extra_link_flags += '-lstdc++_s'
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

# SIMD assessment for optimized builds with zlib-ng
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# If the user asked for 'native' SIMD flags then we check if
# their compiler supports the native arch type. If it doesn't,
# we try to enable NEON (on Arm) and SSE2 and/or SSSE3 (on x86).
#
# Currently zlib-ng is the driving force behind these checks as
# it provides a meaningful speedup in video and frame capture.
#
# Note that these build flags needs to be defined before adding
# subpackages because the flags are used when compiling them.

simd_instruction_sets = []

zlib_ng_options = get_option('use_zlib_ng')
zlib_ng_wants_native = zlib_ng_options.contains('native')
zlib_ng_is_native = zlib_ng_wants_native and cc.has_argument('-march=native')

if is_optimized_buildtype and zlib_ng_is_native
    extra_flags += '-march=native'
    simd_instruction_sets += 'native'
    message('Enabling native SIMD optimizations')
endif

# NEON on Aarch64
# ~~~~~~~~~~~~~~~
if target_machine.cpu_family() == 'aarch64'
    # aarch64 always supports NEON:
    # ref: https://developer.arm.com/documentation/den0024/a/AArch64-Floating-point-and-NEON
    zlib_ng_options += 'neon'
    simd_instruction_sets += 'NEON'
    message('Using the ARM NEON instruction set')
endif

# NEON on Aarch32
# ~~~~~~~~~~~~~~~
if (
    target_machine.cpu_family() == 'arm'
    and (zlib_ng_is_native
    or zlib_ng_wants_native
    or 'neon' in zlib_ng_options)
)
    neon_test_code = '''
                #include <arm_neon.h>
                int main() {
                uint8x16_t a = vdupq_n_u8(0);
                uint8x16_t b = vdupq_n_u8(0);
                uint8x16_t result = vaddq_u8(a, b);
                return 0;
                }
            '''
    neon_cflag = '-mfpu=neon-vfpv4'
    neon_test = cc.run(
        neon_test_code,
        args: neon_cflag,
        name: 'ARM NEON instruction set test',
    )
    if (neon_test.compiled() and neon_test.returncode() == 0)
        zlib_ng_options += 'neon'
        extra_flags += neon_cflag
        simd_instruction_sets += 'NEON'
        message('Enabling the ARM NEON instruction set')
    endif
endif

# SSE2 and SSSE3 on x86
# ~~~~~~~~~~~~~~~~~~~~~
if (
    target_machine.cpu_family().startswith('x86')
    and (
        zlib_ng_is_native
        or zlib_ng_wants_native
        or 'sse2' in zlib_ng_options
        or 'ssse3' in zlib_ng_options
    )
)
    sse2_test_code = '''
                #include <emmintrin.h>
                int main() {
                __m128i a = _mm_setzero_si128();
                __m128i b = _mm_setzero_si128();
                __m128i result = _mm_add_epi32(a, b);
                return 0;
                }
            '''
    sse2_cflags = '-msse2'
    sse2_test = cc.run(
        sse2_test_code,
        args: sse2_cflags,
        name: 'SSE2 instruction set test',
    )
    if (sse2_test.compiled() and sse2_test.returncode() == 0)
        zlib_ng_options += 'sse2'
        extra_flags += sse2_cflags
        simd_instruction_sets += 'SSE2'
        message('Enabling the SSE2 instruction set')
    endif

    ssse3_test_code = '''
                #include <tmmintrin.h>
                int main() {
                __m128i a = _mm_setzero_si128();
                __m128i b = _mm_setzero_si128();
                __m128i result = _mm_hadd_epi16(a, b);
                return 0;
                }
            '''
    ssse3_cflag = '-mssse3'
    ssse3_test = cc.run(
        ssse3_test_code,
        args: ssse3_cflag,
        name: 'SSSE3 instruction set test',
    )
    if (ssse3_test.compiled() and ssse3_test.returncode() == 0)
        zlib_ng_options += 'ssse3'
        extra_flags += ssse3_cflag
        simd_instruction_sets += 'SSSE3'
        message('Enabling the SSSE3 instruction set')
    endif
endif

if simd_instruction_sets.length() == 0
    simd_instruction_sets = ['none']
endif

summary(
    'SIMD instruction sets',
    ', '.join(simd_instruction_sets),
    section: 'Build Summary',
)

# Allow-list the flags against the compiler, and add them to the project
cc_supported_arguments = cc.get_supported_arguments(extra_flags)
add_project_arguments(cc_supported_arguments, language: 'c')

cxx_supported_arguments = cxx.get_supported_arguments(extra_flags)
add_project_arguments(cxx_supported_arguments, language: 'cpp')

link_supported_arguments = cxx.get_supported_link_arguments(extra_link_flags)
add_project_link_arguments(link_supported_arguments, language: 'cpp')

# Gather data to populate config.h
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# The actual config.h file will be generated after interpreting
# all the build files in all the subdirs.
#
git_hash = run_command(
  './scripts/get-version.sh', 'hash', check: true,
).stdout().strip()

conf_data = configuration_data()
conf_data.set('version', meson.project_version())
conf_data.set('project_name', meson.project_name())
conf_data.set('git_hash', git_hash)
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

if cxx.has_type(
    'std::pmr::unsynchronized_pool_resource',
    prefix: '#include <memory_resource>',
)
    conf_data.set10('HAVE_MEMORY_RESOURCE', true)
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

# strnlen was originally a Linux-only function
if cc.has_function('strnlen', prefix: '#include <string.h>')
    conf_data.set10('HAVE_STRNLEN', true)
endif

if cc.has_member('struct dirent', 'd_type', prefix: '#include <dirent.h>')
    conf_data.set10('HAVE_STRUCT_DIRENT_D_TYPE', true)
endif

foreach header : [
    'libgen.h',
    'pwd.h',
    'strings.h',
    'sys/xattr.h',
    'netinet/in.h',
]
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


# Get host page size
# ~~~~~~~~~~~~~~~~~~
# When detecting the page size, we use Meson's configured
# compiler to make a system call because Meson will build and run
# it via the cross compiler (if one is in use), which is
# particularly important we get the destiantion machine's page
# size as opposed to the local build machine's page size.
#
pagesize = 4096
pagesize_option = get_option('pagesize')
if pagesize_option > 0
    # User has provided the page size
    pagesize = pagesize_option
else
    # Detect the page size using a syscall via the cross compiler
    pagesize_api = os_family_name == 'WIN32' ? 'windows' : 'posix'
    pagesize_src = files('contrib/pagesize' / pagesize_api + '_pagesize.c')
    pagesize_cmd = cc.run(
        pagesize_src,
        name: 'Query host page size',
    )
    if pagesize_cmd.returncode() == 0
        pagesize = pagesize_cmd.stdout().strip().to_int()
    else
        error('''Unable to detect the host's page size''')
    endif
endif
conf_data.set('PAGESIZE', pagesize)
summary(
    'Host page size (bytes)',
    pagesize.to_string(),
    section: 'Build Summary',
)


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
wraps_forced = get_option('wrap_mode') == 'forcefallback'
wraps_disabled = get_option('wrap_mode') in ['nodownload', 'nofallback']

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
    include_type: 'system',
)

opus_dep = dependency(
    'opusfile',
    version: ['>= 0.8', '< 1'],
    static: ('opusfile' in static_libs_list or prefers_static_libs),
    include_type: 'system',
)

sdl2_dep = dependency(
    'sdl2',
    version: ['>= 2.0.5', '< 3'],
    static: ('sdl2' in static_libs_list or prefers_static_libs),
    default_options: default_wrap_options
    + [
        'assertions=' + (is_optimized_buildtype ? 'disabled' : 'enabled'),
        'prefer_dlopen=true',
        'test=' + wants_tests.to_string(),
        'use_audio=enabled',
        'use_events=enabled',
        'use_joystick=enabled',
        'use_loadso=enabled',
        'use_timers=enabled',
        'use_video=enabled',

        'use_dummies=false',
        'system_iconv=disabled',
        'use_atomic=enabled',
        'use_file=disabled',
        'use_filesystem=disabled',
        'use_hidapi=disabled',
        'use_haptic=disabled',
        'use_locale=disabled',
        'use_sensor=disabled',
        'use_power=disabled',
    ],
    include_type: 'system',
)

# zlib
# ~~~~
zlib_provider = 'none'
zlib_ng_provider = 'none'

zlib_dep = disabler()
zlib_is_static = 'zlib' in static_libs_list or prefers_static_libs

zlib_ng_is_allowed = 'false' not in zlib_ng_options
if not zlib_ng_is_allowed
    zlib_ng_provider = 'disabled'
endif

# Try using the system-provided zlib-ng library so long as the wrap isn't
# forced. Using the system-provided library is a free upgrade for all the build
# types, so we always prefer this first if the user allows it.
#
try_system_zlib_ng = (
    zlib_ng_is_allowed
    and not wraps_forced
    and 'zlib-ng' not in get_option('force_fallback_for')
)

# The zlib-ng wrap, on the other hand, is a costly endeavour because it
# single-handedly quadruples the 'meson setup' time for the entire project (6.1s
# to 25.8s) due to the use of the much slower CMake build system, therefore we
# only expend this effort (to optimize zlib) for commensurate optimized builds.
# This nuance is also explained in meson_options.txt for packagers.
#
try_builtin_zlib_ng = (
    zlib_ng_is_allowed
    and (not wraps_disabled
    or 'zlib-ng' in get_option('force_fallback_for'))
)

system_zlib_ng_dep = disabler()
if try_system_zlib_ng
    system_zlib_ng_dep = dependency(
        'zlib-ng',
        required: false,
        fallback: [],
        static: zlib_is_static,
        include_type: 'system',
    )
    if (system_zlib_ng_dep.found())
        zlib_ng_provider = 'system library'
        conf_data.set10('C_SYSTEM_ZLIB_NG', true)
        try_builtin_zlib_ng = false
    else
        zlib_ng_provider = 'skipped (system library not found)'
    endif
endif

# We only justify the extra time to setup zlib-ng (using CMake) if
# the user wants an optimized build, so check if we can skip it:
#
if try_builtin_zlib_ng and not is_optimized_buildtype
    zlib_ng_provider = 'skipped (non-optimized build)'
    try_builtin_zlib_ng = false
endif

# Meson needs to be new enough to setup the zlib-ng CMake project, so
# check if Meson is too old:
#
if try_builtin_zlib_ng and meson.version() < '1.3.0'
    zlib_ng_provider = 'skipped (meson is < 1.3.0)'
    try_builtin_zlib_ng = false
endif

# CMake needs to be available both on the host and in Meson as a module to
# setup the zlib-ng CMake project, so check if we're missing either:
#
cmake_module = disabler()
if try_builtin_zlib_ng
    cmake_bin = find_program('cmake', required: false)
    cmake_module = import('cmake', required: false)
    if not cmake_bin.found()
        zlib_ng_provider = 'skipped (CMake not found)'
        try_builtin_zlib_ng = false

    elif not cmake_module.found()
        zlib_ng_provider = 'skipped (Meson\'s CMake module not found)'
        try_builtin_zlib_ng = false
    endif
endif

# If we passed all the above checks, everything is in place to use the wrap!
if try_builtin_zlib_ng
    cmake_options = cmake_module.subproject_options()

    zlib_ng_defines = {
        'ZLIB_COMPAT': true,
        'WITH_OPTIM': true,
        'ZLIB_BUILD_STATIC': true,
        'PIC': get_option('b_staticpic'),
        'BUILD_SHARED_LIBS': false,
        'WITH_GTEST': false,
        'ZLIB_ENABLE_TESTS': false,
        'WITH_NATIVE_INSTRUCTIONS': zlib_ng_is_native,
        'WITH_SANITIZER': get_option('b_sanitize'),
        'CMAKE_C_FLAGS': ' '.join(cc_supported_arguments),
    }

    foreach instruction_set : [
        'avx2',
        'avx512',
        'avx512vnni',
        'sse2',
        'ssse3',
        'sse42',
        'pclmulqdq',
        'vpclmulqdq',
        'acle',
        'neon',
        'armv6',
        'altivec',
        'power8',
        'rvv',
        'crc32_vx',
        'dfltcc_deflate',
        'dfltcc_inflate',
    ]
        cmake_define_key = 'WITH_' + instruction_set.to_upper()
        cmake_define_value = (
            zlib_ng_is_native
            or zlib_ng_options.contains(instruction_set)
        )
        zlib_ng_defines += {cmake_define_key: cmake_define_value}
    endforeach

    cmake_options.add_cmake_defines(zlib_ng_defines)

    zlib_ng_subproject = cmake_module.subproject(
        'zlib-ng',
        options: cmake_options,
    )
    zlib_ng_provider = 'built-in'

    # We configure zlib-ng to be a drop in replacement for zlib, so we
    # can directly assign the zlib_dep from the zlib-ng subproject:
    #
    zlib_dep = zlib_ng_subproject.get_variable('zlib_dep')
    zlib_provider = 'built-in (zlib-ng)'
endif

# Otherwise use the system's zlib
if not zlib_dep.found()
    zlib_dep = dependency(
        'zlib',
        version: ['>= 1.2.11', '< 2'],
        required: true,
        fallback: [],
        static: zlib_is_static,
        include_type: 'system',
    )
    zlib_provider = 'system library'
endif

summary('zlib-ng provider', zlib_ng_provider)
summary('zlib provider', zlib_provider)

# PNG
# ~~~
png_has_internal_dep = zlib_dep.type_name() == 'internal'
if png_has_internal_dep
    png_dep = subproject(
        'libpng',
        default_options: default_wrap_options,
    ).get_variable('png_dep')
else
    png_dep = dependency(
        'libpng',
        version: ['>= 1.2', '< 2'],
        required: true,
        fallback: [],
        static: 'png' in static_libs_list or prefers_static_libs,
        include_type: 'system',
    )
endif
summary(
    'PNG provider',
    png_dep.type_name() == 'internal' ? 'built-in' : 'system library',
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
    include_type: 'system',
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
        static: ('sdl2_net' in static_libs_list or prefers_static_libs),
        default_options: default_wrap_options,
        not_found_message: msg.format('use_sdl2_net'),
        include_type: 'system',
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
        include_type: 'system',
    )

    libslirp_summary_msg = libslirp_dep.found()

    if libslirp_dep.found() and not have_fd_zero
        libslirp_summary_msg = 'Disabled due to host missing file-descriptor routines'
        libslirp_dep = optional_dep
    endif

endif
summary('slirp support', libslirp_summary_msg)


# OpenGL
opengl_dep = optional_dep
if get_option('use_opengl')
    opengl_dep = dependency('gl', not_found_message: msg.format('use_opengl'))
endif
conf_data.set10('C_OPENGL', opengl_dep.found())

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
        include_type: 'system',
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
        include_type: 'system',
    )
endif
summary('mt32emu support', mt32emu_dep.found())

# Tracy
tracy_dep = optional_dep
if get_option('tracy')
    tracy_dep = dependency(
        'tracy',
        version: ['>= 0.10', '< 1'],
        default_options: default_wrap_options,
        static: ('tracy' in static_libs_list or prefers_static_libs),
        not_found_message: msg.format('tracy'),
        include_type: 'system',
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
        include_type: 'system',
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
        include_type: 'system',
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
        include_type: 'system',
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
        include_type: 'system',
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
        include_type: 'system',
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
endif

# Determine if system is capable of using ManyMouse library
conf_data.set10('C_MANYMOUSE', true)
manymouse_summary_msg = 'True'

# ManyMouse optionally supports the X Input 2.0 protocol (regardless of OS)
xinput2_dep = optional_dep
xinput2_required = (get_option('use_xinput2') == 'true')
if xinput2_required or (get_option('use_xinput2') == 'auto')
    xinput2_dep = dependency(
        'xi',
        version: ['>= 1.4', '< 2'],
        required: xinput2_required,
        include_type: 'system',
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
    alsa_dep = dependency(
        'alsa',
        version: ['>= 1', '< 2'],
        include_type: 'system',
    )
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

# Set up include directories
incdir = [
    include_directories('include', '.'),
    include_directories('src/libs', is_system: true),
]


# A list of DOSBox's internal libraries populated
# by each of the src/ subdir imports below.
internal_deps = []

# bundled dependencies, in dependency-order
#
subdir('src/libs/ghc')
subdir('src/libs/loguru')
subdir('src/libs/decoders')
subdir('src/libs/nuked')
subdir('src/libs/ESFMu')
subdir('src/libs/residfp')
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
    threads_dep,
    ghc_dep,
    libiir_dep,
    libloguru_dep,
    tracy_dep,
    libwhereami_dep,
    libtalchorus_dep,
]

if conf_data.get('C_MANYMOUSE') != 0
    third_party_deps += manymouse_dep
endif

# internal libs
subdir('src/audio')
subdir('src/capture')
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


# Set up the executable and libraries
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
dosbox_sources = ['src/main.cpp', 'src/dosbox.cpp']

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
    cpp_args: warnings,
)

# create a library so we can test things inside DOSBOX dep path
libdosbox = static_library(
    'dosbox',
    ['src/dosbox.cpp'],
    include_directories: incdir,
    dependencies: internal_deps + third_party_deps,
)

dosbox_dep = declare_dependency(link_with: libdosbox)


# Set up unit tests
# ~~~~~~~~~~~~~~~~~
#
if wants_tests
    subdir('tests')
endif
