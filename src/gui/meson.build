libgui_sources = files(
    'clipboard.cpp',
    'mapper.cpp',
    'mapper.cpp',
    'sdl_gui.cpp',
    'titlebar.cpp',

    'render/auto_image_adjustments.cpp',
    'render/auto_shader_switcher.cpp',
    'render/deinterlacer.cpp',
    'render/opengl_renderer.cpp',
    'render/render.cpp',
    'render/scaler/scalers.cpp',
    'render/sdl_renderer.cpp',
    'render/shader.cpp'
    'render/shader_manager.cpp',
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
