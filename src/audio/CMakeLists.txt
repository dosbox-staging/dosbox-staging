add_library(libaudio STATIC
		clap/event_list.cpp
		clap/library.cpp
		clap/plugin.cpp
		clap/plugin_manager.cpp

		compressor.cpp
		envelope.cpp
)

target_link_libraries(libaudio PRIVATE
		loguru
)
