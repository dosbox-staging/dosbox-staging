target_sources(libdosboxcommon PRIVATE 
  clipboard.cpp
  render.cpp
  render_scalers.cpp
  sdl_mapper.cpp
  sdlmain.cpp
  shader_manager.cpp
  titlebar.cpp)
  
target_link_libraries(libdosboxcommon PRIVATE 
  OpenGL::GL
  $<IF:$<BOOL:${C_OPENGL}>,libglad,>)
