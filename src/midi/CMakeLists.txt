add_library(libmidi STATIC
		midi.cpp
		midi_alsa.cpp
		midi_fluidsynth.cpp
		midi_mt32.cpp
		midi_lasynth_model.cpp
		midi_oss.cpp
)

find_package(MT32Emu CONFIG REQUIRED)
find_package(FluidSynth CONFIG REQUIRED)

if (C_COREMIDI)
	target_link_libraries(libmidi PRIVATE "-framework CoreFoundation")
	target_link_libraries(libmidi PRIVATE "-framework CoreMIDI")
endif()

target_link_libraries(libmidi PRIVATE
		$<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
		MT32Emu::mt32emu
		FluidSynth::libfluidsynth
)
