libsdlcd_sources = ['SDL_cdrom.c', 'dummy/SDL_syscdrom.c']

if host_machine.system() in ['windows', 'cygwin']
  libsdlcd_sources += ['win32/SDL_syscdrom.c']

elif host_machine.system() == 'linux'
  libsdlcd_sources += ['linux/SDL_syscdrom.c']

elif host_machine.system() == 'darwin'
  libsdlcd_sources += ['macosx/SDL_syscdrom.c',
                       'macosx/AudioFilePlayer.c',
                       'macosx/AudioFileReaderThread.c',
                       'macosx/CDPlayer.c',
                       'macosx/SDLOSXCAGuard.c']
endif

# Convert list of strings to file objects
libsdlcd_sources = files(libsdlcd_sources)

libsdlcd_deps = [sdl2_dep]

if host_machine.system() == 'darwin'
  libsdlcd_deps += [coreservices_dep]
endif

libsdlcd = static_library('sdlcd', libsdlcd_sources,
                          include_directories : incdir,
                          dependencies: libsdlcd_deps)

libsdlcd_dep = declare_dependency(link_with : libsdlcd)
