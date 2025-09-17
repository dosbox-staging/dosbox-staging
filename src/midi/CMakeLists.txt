target_sources(libdosboxcommon PRIVATE
  alsa.cpp
  coreaudio.cpp
  coremidi.cpp
  fluidsynth.cpp
  midi.cpp
  mt32.cpp
  mt32_lasynth_model.cpp
  soundcanvas.cpp
  win32.cpp
)

target_include_directories(libdosboxcommon PRIVATE ../libs/include)
target_link_libraries(libdosboxcommon PRIVATE MT32Emu::mt32emu)

if (C_ALSA)
  target_link_libraries(libdosboxcommon PRIVATE ALSA::ALSA)
endif()

if (C_COREMIDI)
  target_link_libraries(libdosboxcommon PRIVATE "-framework CoreFoundation")
  target_link_libraries(libdosboxcommon PRIVATE "-framework CoreMIDI")
endif()
