target_sources(dosboxcommon PRIVATE
  coreaudio.cpp
  coremidi.cpp
  fluidsynth.cpp
  midi.cpp
  mt32.cpp
  mt32_lasynth_model.cpp
  soundcanvas.cpp
  win32.cpp
)

target_link_libraries(dosboxcommon PRIVATE FluidSynth::libfluidsynth clap)

if (C_MT32EMU)
  target_link_libraries(dosboxcommon PRIVATE MT32Emu::mt32emu)
endif()

# Linux-only ALSA MIDI backend
if (C_ALSA)
  target_sources(dosboxcommon PRIVATE alsa.cpp)
  target_link_libraries(dosboxcommon PRIVATE ALSA::ALSA)
endif()

if (C_COREMIDI)
  target_link_libraries(dosboxcommon PRIVATE "-framework CoreFoundation")
  target_link_libraries(dosboxcommon PRIVATE "-framework CoreMIDI")
endif()
