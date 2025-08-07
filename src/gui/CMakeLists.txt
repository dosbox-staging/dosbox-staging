target_sources(libdosboxcommon PRIVATE
  clipboard.cpp
  sdl_mapper.cpp
  sdlmain.cpp
  shader_manager.cpp
  titlebar.cpp

  render/render.cpp
  render/scaler/scalers.cpp
)

target_link_libraries(libdosboxcommon PRIVATE
  OpenGL::GL
  $<IF:$<BOOL:${C_OPENGL}>,libglad,>)
