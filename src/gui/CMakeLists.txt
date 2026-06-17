target_sources(dosboxcommon PRIVATE
  clipboard.cpp
  mapper.cpp
  sdl_gui.cpp
  titlebar.cpp

  render/auto_image_adjustments.cpp
  render/auto_shader_switcher.cpp
  render/deinterlacer.cpp
  render/opengl_renderer.cpp
  render/render.cpp
  render/scaler/scalers.cpp
  render/sdl_renderer.cpp
  render/shader.cpp
  render/shader_manager.cpp
  render/shader_pipeline.cpp
  render/shader_pragma_parser.cpp
)

if(APPLE)
target_sources(dosboxcommon PRIVATE
  render/macos_colorspace.mm
)
endif()

if (OPT_DEBUGGER)
  target_link_libraries(dosboxcommon PRIVATE imgui::imgui)
endif()

target_link_libraries(dosboxcommon PRIVATE OpenGL::GL simpleini)

if (C_OPENGL)
  target_link_libraries(dosboxcommon PRIVATE glad)
endif()