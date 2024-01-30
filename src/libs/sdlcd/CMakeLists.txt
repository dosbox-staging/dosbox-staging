add_library(libsdlcd STATIC SDL_cdrom.c dummy/SDL_syscdrom.c)

if (DOSBOX_PLATFORM_WINDOWS)
	target_sources(libsdlcd PRIVATE win32/SDL_syscdrom.c)
elseif(DOSBOX_PLATFORM_LINUX)
	target_sources(libsdlcd PRIVATE linux/SDL_syscdrom.c)
elseif(DOSBOX_PLATFORM_MACOS)
	target_sources(libsdlcd PRIVATE
			macosx/SDL_syscdrom.c
			macosx/AudioFilePlayer.c
			macosx/AudioFileReaderThread.c
			macosx/CDPlayer.c
			macosx/SDLOSXCAGuard.c
	)
endif()

target_link_libraries(libsdlcd PRIVATE $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>)
