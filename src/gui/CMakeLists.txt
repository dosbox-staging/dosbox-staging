target_sources(libdosboxcommon PRIVATE
  clipboard.cpp
  mapper.cpp
  sdlmain.cpp
  shader_manager.cpp
  titlebar.cpp

  render/opengl_renderer.cpp
  render/render.cpp
  render/scaler/scalers.cpp
  render/sdl_renderer.cpp
)

target_link_libraries(libdosboxcommon PRIVATE
  OpenGL::GL
  $<IF:$<BOOL:${C_OPENGL}>,libglad,>)
