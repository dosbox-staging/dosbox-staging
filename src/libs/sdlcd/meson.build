libsdlcd_sources = files([
  'SDL_cdrom.c',
  'win32/SDL_syscdrom.c',
  'linux/SDL_syscdrom.c',
  'macosx/SDL_syscdrom.c',
  'macosx/AudioFilePlayer.c',
  'macosx/AudioFileReaderThread.c',
  'macosx/CDPlayer.c',
  'macosx/SDLOSXCAGuard.c',
  'dummy/SDL_syscdrom.c',
])

libsdlcd = static_library('sdlcd', libsdlcd_sources,
                          include_directories : incdir,
                          dependencies: [sdl2_dep, libghc_dep, libloguru_dep])

libsdlcd_dep = declare_dependency(link_with : libsdlcd)
