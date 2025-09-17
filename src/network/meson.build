libnetwork_sources = [
    'ethernet.cpp',
    'ethernet_slirp.cpp'
]

libnetwork = static_library(
    'network',
    libnetwork_sources,
    include_directories: incdir,
    dependencies: [
        libslirp_dep,
    ],
    cpp_args: warnings,
)

libnetwork_dep = declare_dependency(link_with: libnetwork)

internal_deps += libnetwork_dep
