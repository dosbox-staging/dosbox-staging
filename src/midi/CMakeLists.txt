target_sources(dosboxcommon PRIVATE
  fluidsynth.cpp
  midi.cpp
  mt32.cpp
  mt32_lasynth_model.cpp
  soundcanvas.cpp
)

# macOS-only CoreMIDI backend
if (C_COREMIDI)
  target_sources(dosboxcommon PRIVATE coremidi.cpp)
  target_link_libraries(dosboxcommon PRIVATE "-framework CoreFoundation")
  target_link_libraries(dosboxcommon PRIVATE "-framework CoreMIDI")
endif()

# macOS-only CoreAudio MIDI backend
if (C_COREAUDIO)
  target_sources(dosboxcommon PRIVATE coreaudio.cpp)
endif()

# Windows-only MIDI backend
if (WIN32)
  target_sources(dosboxcommon PRIVATE win32.cpp)
endif()

# Linux-only ALSA MIDI backend
if (C_ALSA)
  target_sources(dosboxcommon PRIVATE alsa.cpp)
  target_link_libraries(dosboxcommon PRIVATE ALSA::ALSA)
endif()

if (C_MT32EMU)
  target_link_libraries(dosboxcommon PRIVATE MT32Emu::mt32emu)
endif()

target_link_libraries(dosboxcommon PRIVATE FluidSynth::libfluidsynth clap)

