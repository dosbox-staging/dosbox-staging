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
)

target_link_libraries(dosboxcommon PRIVATE OpenGL::GL)

if (C_OPENGL)
  target_link_libraries(dosboxcommon PRIVATE glad)
endif()
