libfpu = static_library(
    'fpu',
    ['fpu.cpp'],
    include_directories: incdir,
    dependencies: [ghc_dep, libloguru_dep],
    cpp_args: warnings,
)

libfpu_dep = declare_dependency(link_with: libfpu)

internal_deps += libfpu_dep
