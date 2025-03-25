libaudio_sources = files(
    'clap/event_list.cpp',
    'clap/library.cpp',
    'clap/plugin.cpp',
    'clap/plugin_manager.cpp',

    'compressor.cpp',
    'envelope.cpp',
)

libaudio = static_library(
    'audio',
    libaudio_sources,
    include_directories: incdir,
    dependencies: [
        libiir_dep,
        libloguru_dep,
    ],
    cpp_args: warnings,
)

libaudio_dep = declare_dependency(link_with: libaudio)

internal_deps += libaudio_dep
