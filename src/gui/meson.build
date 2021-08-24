libgui_sources = files([
  'render.cpp',
  'render_scalers.cpp',
  'sdl_gui.cpp',
  'sdlmain.cpp',
  'sdl_mapper.cpp',
])

libgui = static_library('gui', libgui_sources,
                        include_directories : incdir,
                        dependencies : [
                          sdl2_dep,
                          opengl_dep,
                          libloguru_dep,
                          libppscale_dep,
                        ])

libgui_dep = declare_dependency(link_with : libgui)

internal_deps += libgui_dep
