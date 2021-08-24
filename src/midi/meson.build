libmidi_sources = [
  'midi.cpp',
  'midi_alsa.cpp',
  'midi_fluidsynth.cpp',
  'midi_mt32.cpp',
  'midi_lasynth_model.cpp',
  'midi_oss.cpp',
]

libmidi = static_library('midi', libmidi_sources,
                         include_directories : incdir,
                         dependencies : [
                           sdl2_dep,
                           fluid_dep,
                           mt32emu_dep,
                           alsa_dep,
                           coreaudio_dep,
                           coremidi_dep,
                           winmm_dep,
                           libloguru_dep,
                         ])

libmidi_dep = declare_dependency(link_with : libmidi)

internal_deps += libmidi_dep
