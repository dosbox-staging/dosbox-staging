libaudio_sources = files(
    'clap/event_list.cpp',
    'clap/plugin.cpp',
    'clap/plugin_manager.cpp',
)

libaudio = static_library(
    'audio',
    libaudio_sources,
    include_directories: incdir,
    dependencies: [
        libloguru_dep,
    ],
    cpp_args: warnings,
)

libaudio_dep = declare_dependency(link_with: libaudio)

internal_deps += libaudio_dep
