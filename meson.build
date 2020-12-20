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

if get_option('enable_debugger') != 'none'
  conf_data.set10('C_DEBUG', true)
endif

if get_option('enable_debugger') == 'heavy'
  conf_data.set10('C_HEAVY_DEBUG', true)
endif

# prepare configuration for dynrec core
#
# host_machine is a machine we're building for
#
# In code we have code paths for two additional architectures:
#
# - ARMV4LE - autoconf used to set it when "armv6l" was reported, presumably
#   it covers everything up to ARMv7.  Notably, Raspberry Pi 1 is using ARMv6.
#   This arch was marked as NOT handling unaligned memory.
# - MIPSEL - which used to be called MIPSEL32, and seems to be never hooked
#   into the autoconf buildsystem (but it was used by several DOSBox ports)
#
selected_core = get_option('dynamic_core')
host_cpu = host_machine.cpu_family()
conf_data.set('C_TARGETCPU', 'UNKNOWN')
cpu_handles_unaligned_memory = false

core_selection = [
  # cpu         selected_core       dynrec_define     C_TARGETCPU
  [ 'x86_64',  ['auto', 'dyn-x86'], 'C_DYNAMIC_X86',  'X86_64'    ],
  [ 'x86',     ['auto', 'dyn-x86'], 'C_DYNAMIC_X86',  'X86'       ],
  [ 'x86_64',  ['dynrec'],          'C_DYNREC',       'X86_64'    ],
  [ 'x86',     ['dynrec'],          'C_DYNREC',       'X86'       ],
  [ 'aarch64', ['auto', 'dynrec'],  'C_DYNREC',       'ARMV8LE'   ],
  [ 'arm',     ['auto', 'dynrec'],  'C_DYNREC',       'ARMV7LE'   ],
  [ 'ppc64',   ['auto', 'dynrec'],  'C_DYNREC',       'PPC64LE'   ], # stable name
  [ 'ppc64le', ['auto', 'dynrec'],  'C_DYNREC',       'PPC64LE'   ], # unstable name
  [ 'ppc',     ['auto', 'dynrec'],  'C_DYNREC',       'POWERPC'   ],
]

foreach line : core_selection 
  cpu = line[0]
  opts_for_arch = line[1]
  dynrec_define = line[2]
  target_cpu = line[3]

  if (host_cpu == cpu) and opts_for_arch.contains(selected_core)
    conf_data.set('C_TARGETCPU', target_cpu)
    conf_data.set(dynrec_define, 1)
    cpu_handles_unaligned_memory = true
  endif
endforeach

conf_data.set10('C_UNALIGNED_MEMORY', cpu_handles_unaligned_memory)

target_cpu = conf_data.get('C_TARGETCPU')
if conf_data.has('C_DYNAMIC_X86')
  message('Building dynamic core optimized for x86/x86_64 (dyn-x86) for ' + target_cpu)
elif conf_data.has('C_DYNREC')
  message('Building dynamic core (dynrec) for ' + target_cpu)
else
  warning('Building without dynamic core support')
endif


# generate config.h
#
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
