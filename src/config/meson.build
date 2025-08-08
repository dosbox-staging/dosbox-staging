libconfig_sources = files(
    'setup.cpp',
)

libconfig = static_library(
    'config',
    libconfig_sources,
    include_directories: incdir,
    cpp_args: warnings,
)

libconfig_dep = declare_dependency(link_with: libconfig)

internal_deps += libconfig_dep
