libgui_sources = files(
    'auto_image_adjustments.cpp',
    'clipboard.cpp',
    'mapper.cpp',
    'mapper.cpp',
    'sdl_gui.cpp',
    'shader_manager.cpp',
    'titlebar.cpp',

    'render/deinterlacer.cpp',
    'render/opengl_renderer.cpp',
    'render/render.cpp',
    'render/scaler/scalers.cpp',
    'render/sdl_renderer.cpp'
)

libgui = static_library(
    'gui',
    libgui_sources,
    include_directories: incdir,
    dependencies: [
        libglad_dep,
        libiir_dep,
        libloguru_dep,
        opengl_dep,
        sdl2_dep,
    ],
    cpp_args: warnings,
)

libgui_dep = declare_dependency(link_with: libgui)

internal_deps += libgui_dep
