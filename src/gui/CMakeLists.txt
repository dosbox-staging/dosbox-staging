add_library(libgui STATIC
  clipboard.cpp
  render.cpp
  render_scalers.cpp
  sdl_mapper.cpp
  sdlmain.cpp
  shader_manager.cpp
  titlebar.cpp
)

find_package(OpenGL REQUIRED)
target_link_libraries(libgui PRIVATE OpenGL::GL)

target_link_libraries(libgui PRIVATE
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
  libmisc
)
