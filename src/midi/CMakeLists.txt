add_library(libmidi STATIC
		midi.cpp
		midi_alsa.cpp
		midi_coreaudio.cpp
		midi_coremidi.cpp
		midi_fluidsynth.cpp
		midi_lasynth_model.cpp
		midi_mt32.cpp
		midi_win32.cpp
		midi_soundcanvas.cpp
)

find_package(MT32Emu REQUIRED)
target_link_libraries(libmidi PRIVATE MT32Emu::mt32emu)

find_package(FluidSynth REQUIRED)
target_link_libraries(libmidi PRIVATE FluidSynth::libfluidsynth)

if (C_ALSA)
	find_package(ALSA REQUIRED)
	target_link_libraries(libmidi PRIVATE ALSA::ALSA)
endif()

if (C_COREMIDI)
	target_link_libraries(libmidi PRIVATE "-framework CoreFoundation")
	target_link_libraries(libmidi PRIVATE "-framework CoreMIDI")
endif()

target_link_libraries(libmidi PRIVATE
		$<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
		libaudio
		libhardware
)
