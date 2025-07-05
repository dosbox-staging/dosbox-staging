target_sources(libdosboxcommon PRIVATE
  midi.cpp
  midi_alsa.cpp
  midi_coreaudio.cpp
  midi_coremidi.cpp
  midi_fluidsynth.cpp
  midi_lasynth_model.cpp
  midi_mt32.cpp
  midi_win32.cpp
  midi_soundcanvas.cpp)

target_include_directories(libdosboxcommon PRIVATE ../libs/include)
target_link_libraries(libdosboxcommon PRIVATE MT32Emu::mt32emu)

if (C_ALSA)
  target_link_libraries(libdosboxcommon PRIVATE ALSA::ALSA)
endif()

if (C_COREMIDI)
  target_link_libraries(libdosboxcommon PRIVATE "-framework CoreFoundation")
  target_link_libraries(libdosboxcommon PRIVATE "-framework CoreMIDI")
endif()
