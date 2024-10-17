# prepare configuration for dynrec core
#
# host_machine is a machine we're building for; meson reference:
# https://mesonbuild.com/Reference-manual.html#host_machine-object
# https://mesonbuild.com/Reference-tables.html#cpu-families
#

# Defaults
conf_data.set('C_TARGETCPU', 'UNKNOWN')
conf_data.set('C_UNALIGNED_MEMORY', 0)

per_page_w_or_x_pref = get_option('per_page_w_or_x')
conf_data.set10('C_PER_PAGE_W_OR_X', per_page_w_or_x_pref.auto() or per_page_w_or_x_pref.enabled())

# Platform-specific
core_selection = [
    # cpu_family              endian      selected_core        dynrec_define    target     unaligned  per-page
    #                                                                                      mem        W^X
    [ ['x86_64'],             ['little'], ['auto', 'dyn-x86'], 'C_DYNAMIC_X86', 'X86_64',  1,         1 ],
    [ ['x86'],                ['little'], ['auto', 'dyn-x86'], 'C_DYNAMIC_X86', 'X86',     1,         1 ],
    [ ['x86_64'],             ['little'], ['dynrec'],          'C_DYNREC',      'X86_64',  1,         1 ],
    [ ['x86'],                ['little'], ['dynrec'],          'C_DYNREC',      'X86',     1,         1 ],
    [ ['aarch64'],            ['little'], ['auto', 'dynrec'],  'C_DYNREC',      'ARMV8LE', 1,         1 ], # ARMv8+ (64-bit)
    [ ['arm'],                ['little'], ['auto', 'dynrec'],  'C_DYNREC',      'ARMV7LE', 1,         1 ], # ARMv7+
    [ ['armv6'],              ['little'], ['auto', 'dynrec'],  'C_DYNREC',      'ARMV4LE', 0,         0 ], # ARMv6 or older
    [ ['ppc64', 'powerpc64'], ['big'],    ['auto', 'dynrec'],  'C_DYNREC',      'POWERPC', 1,         1 ], # 64 bit PPC processors (big-endian), running 32-bit big-endian code
    [ ['ppc64', 'powerpc64'], ['little'], ['auto', 'dynrec'],  'C_DYNREC',      'PPC64LE', 1,         1 ], # 64 bit PPC processors (little-endian)
    [ ['ppc',   'powerpc'],   ['big'],    ['auto', 'dynrec'],  'C_DYNREC',      'POWERPC', 1,         0 ], # 32 bit PPC processors (big-endian)
    [ ['mips'],               ['little'], ['auto', 'dynrec'],  'C_DYNREC',      'MIPSEL',  0,         0 ], # 32 bit MIPS processor
]

selected_core = get_option('dynamic_core')

foreach line : core_selection
    cpu_family = line[0]
    endian = line[1]
    opts_for_arch = line[2]
    dynrec_define = line[3]
    target_cpu = line[4]
    unaligned_mem = line[5]
    per_page_w_or_x = line[6]
    if (
        cpu_family.contains(host_machine.cpu_family())
        and endian.contains(host_machine.endian())
        and opts_for_arch.contains(selected_core)
    )
        conf_data.set('C_TARGETCPU', target_cpu)
        conf_data.set('C_UNALIGNED_MEMORY', unaligned_mem)
        conf_data.set(dynrec_define, 1)

        if per_page_w_or_x_pref.auto()
            conf_data.set('C_PER_PAGE_W_OR_X', per_page_w_or_x)
        endif

    endif
endforeach

summary('Byte order', host_machine.endian() + '-endian')
summary('Per-page W^X', conf_data.get('C_PER_PAGE_W_OR_X') == 1 ? 'True' : 'False')
if conf_data.has('C_DYNAMIC_X86')
    summary('CPU dynamic core', 'optimized for x86/x86_64 (dyn-x86)')
    summary('CPU core target arch', conf_data.get('C_TARGETCPU'))
elif conf_data.has('C_DYNREC')
    summary('CPU dynamic core', 'generic (dynrec)')
    summary('CPU core target arch', conf_data.get('C_TARGETCPU'))
else
    warning('Building without dynamic core support')
    summary('CPU core', 'disabled')
endif

# cpu module sources
#
libcpu_sources = files(
    'callback.cpp',
    'core_dyn_x86.cpp',
    'core_dynrec.cpp',
    'core_full.cpp',
    'core_normal.cpp',
    'core_prefetch.cpp',
    'core_simple.cpp',
    'cpu.cpp',
    'flags.cpp',
    'mmx.cpp',
    'modrm.cpp',
    'paging.cpp',
)

libcpu = static_library(
    'cpu',
    libcpu_sources,
    include_directories: incdir,
    dependencies: [
        sdl2_dep,
        ghc_dep,
        tracy_dep,
        libloguru_dep,
    ],
    cpp_args: warnings,
)

libcpu_dep = declare_dependency(link_with: libcpu)

internal_deps += libcpu_dep
