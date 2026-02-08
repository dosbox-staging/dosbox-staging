libmidi_sources = [
    'alsa.cpp',
    'coreaudio.cpp',
    'coremidi.cpp',
    'fluidsynth.cpp',
    'midi.cpp',
    'mt32.cpp',
    'mt32_lasynth_model.cpp',
    'soundcanvas.cpp',
    'win32.cpp',
]

libmidi = static_library(
    'midi',
    libmidi_sources,
    include_directories: incdir,
    dependencies: [
        alsa_dep,
        coreaudio_dep,
        coremidi_dep,
        fluid_dep,
        libiir_dep,
        libloguru_dep,
        mt32emu_dep,
        sdl2_dep,
        winmm_dep,
    ],
    cpp_args: warnings,
)

libmidi_dep = declare_dependency(link_with: libmidi)

internal_deps += libmidi_dep
