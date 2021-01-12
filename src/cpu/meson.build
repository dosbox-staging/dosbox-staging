# prepare configuration for dynrec core
#
# host_machine is a machine we're building for; meson reference:
# https://mesonbuild.com/Reference-manual.html#host_machine-object
# https://mesonbuild.com/Reference-tables.html#cpu-families
#
# In code we have code paths for two additional architectures:
#
# - ARMV4LE - autoconf used to set it when "armv6l" was reported, presumably
#   it covers everything up to ARMv7.  Notably, Raspberry Pi 1 is using ARMv6.
#   This arch was marked as NOT handling unaligned memory.
# - MIPSEL - which used to be called MIPSEL32, and seems to be never hooked
#   into the autoconf buildsystem (but it was used by several DOSBox ports)
#
conf_data.set('C_TARGETCPU', 'UNKNOWN')
conf_data.set('C_UNALIGNED_MEMORY', 0)

core_selection = [
  # cpu_family selected_core        dynrec_define    target     unaligned_mem
  [ 'x86_64',  ['auto', 'dyn-x86'], 'C_DYNAMIC_X86', 'X86_64',  1 ],
  [ 'x86',     ['auto', 'dyn-x86'], 'C_DYNAMIC_X86', 'X86',     1 ],
  [ 'x86_64',  ['dynrec'],          'C_DYNREC',      'X86_64',  1 ],
  [ 'x86',     ['dynrec'],          'C_DYNREC',      'X86',     1 ],
  [ 'aarch64', ['auto', 'dynrec'],  'C_DYNREC',      'ARMV8LE', 1 ],
  [ 'arm',     ['auto', 'dynrec'],  'C_DYNREC',      'ARMV7LE', 1 ],
  [ 'ppc64',   ['auto', 'dynrec'],  'C_DYNREC',      'PPC64LE', 1 ],
  [ 'ppc64le', ['auto', 'dynrec'],  'C_DYNREC',      'PPC64LE', 1 ], # for meson < 0.50
  [ 'ppc',     ['auto', 'dynrec'],  'C_DYNREC',      'POWERPC', 1 ],
]

selected_core = get_option('dynamic_core')

foreach line : core_selection 
  cpu_family    = line[0]
  opts_for_arch = line[1]
  dynrec_define = line[2]
  target_cpu    = line[3]
  unaligned_mem = line[4]
  if (host_machine.cpu_family() == cpu_family) and opts_for_arch.contains(selected_core)
    conf_data.set('C_TARGETCPU', target_cpu)
    conf_data.set('C_UNALIGNED_MEMORY', unaligned_mem)
    conf_data.set(dynrec_define, 1)
  endif
endforeach

target_cpu = conf_data.get('C_TARGETCPU')
if conf_data.has('C_DYNAMIC_X86')
  message('Building dynamic core optimized for x86/x86_64 (dyn-x86) for ' + target_cpu)
elif conf_data.has('C_DYNREC')
  message('Building dynamic core (dynrec) for ' + target_cpu)
else
  warning('Building without dynamic core support')
endif

message('Building for @0@-endian architecture'.format(host_machine.endian()))


# cpu module sources
#
libcpu_sources = files([
  'callback.cpp',
  'core_normal.cpp',
  'core_prefetch.cpp',
  'core_simple.cpp',
  'flags.cpp',
  'modrm.cpp',
  'core_dyn_x86.cpp',
  'core_full.cpp',
  'cpu.cpp',
  'paging.cpp',
  'core_dynrec.cpp',
])

libcpu = static_library('cpu', libcpu_sources,
                        include_directories : incdir,
                        dependencies : sdl2_dep)

libcpu_dep = declare_dependency(link_with : libcpu)

internal_deps += libcpu_dep
