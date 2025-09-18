libutils_sources = [
    'string_utils.cpp',
]

libutils = static_library(
    'utils',
    libutils_sources,
    include_directories: incdir,
    dependencies: [],
    cpp_args: warnings,
)

libutils_dep = declare_dependency(link_with: libutils)

internal_deps += libutils_dep
