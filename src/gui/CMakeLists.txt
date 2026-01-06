target_sources(libdosboxcommon PRIVATE
  auto_image_adjustments.cpp
  clipboard.cpp
  mapper.cpp
  sdl_gui.cpp
  shader_manager.cpp
  titlebar.cpp

  render/deinterlacer.cpp
  render/opengl_renderer.cpp
  render/render.cpp
  render/scaler/scalers.cpp
  render/sdl_renderer.cpp
)

target_link_libraries(libdosboxcommon PRIVATE
  OpenGL::GL
  $<IF:$<BOOL:${C_OPENGL}>,libglad,>)
