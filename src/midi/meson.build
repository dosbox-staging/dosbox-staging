libmidi_sources = [
    'midi.cpp',
    'midi_alsa.cpp',
    'midi_coreaudio.cpp',
    'midi_coremidi.cpp',
    'midi_fluidsynth.cpp',
    'midi_lasynth_model.cpp',
    'midi_mt32.cpp',
    'midi_win32.cpp',
    'midi_soundcanvas.cpp',
]

libmidi = static_library(
    'midi',
    libmidi_sources,
    include_directories: incdir,
    dependencies: [
        alsa_dep,
        coreaudio_dep,
        coremidi_dep,
        ghc_dep,
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
