add_library(libaudio STATIC
		clap/event_list.cpp
		clap/library.cpp
		clap/plugin.cpp
		clap/plugin_manager.cpp

		compressor.cpp
		envelope.cpp
		noise_gate.cpp
)

find_package(iir REQUIRED)
target_link_libraries(libaudio PRIVATE $<IF:$<TARGET_EXISTS:iir::iir>,iir::iir,iir::iir_static>)

target_link_libraries(libaudio PRIVATE
		loguru
)

