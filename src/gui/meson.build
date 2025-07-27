libgui_sources = files(
    'clipboard.cpp',
    'sdl_mapper.cpp',
    'sdlmain.cpp',
    'shader_manager.cpp',
    'titlebar.cpp',

    'render/render.cpp',
    'render/scaler/scalers.cpp',
)

libgui = static_library(
    'gui',
    libgui_sources,
    include_directories: incdir,
    dependencies: [
        ghc_dep,
        libglad_dep,
        libiir_dep,
        libloguru_dep,
        opengl_dep,
        sdl2_dep,
        tracy_dep,
    ],
    cpp_args: warnings,
)

libgui_dep = declare_dependency(link_with: libgui)

internal_deps += libgui_dep
