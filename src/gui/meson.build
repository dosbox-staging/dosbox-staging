libgui_sources = files(
    'render.cpp',
    'render_scalers.cpp',
    'sdl_gui.cpp',
    'sdl_mapper.cpp',
    'sdlmain.cpp',
)

libgui = static_library(
    'gui',
    libgui_sources,
    include_directories: incdir,
    dependencies: [
        sdl2_dep,
        sdl2_image_dep,
        opengl_dep,
        ghc_dep,
        libiir_dep,
        libloguru_dep,
        tracy_dep,
        libppscale_dep,
    ],
)

libgui_dep = declare_dependency(link_with: libgui)

internal_deps += libgui_dep
