target_sources(libdosboxcommon PRIVATE
  auto_image_adjustments.cpp
  clipboard.cpp
  mapper.cpp
  sdl_gui.cpp
  shader_manager.cpp
  titlebar.cpp

  render/angle_renderer.cpp
  render/render.cpp
  render/scaler/scalers.cpp
  render/sdl_renderer.cpp
)

target_link_libraries(libdosboxcommon PRIVATE
  unofficial::angle::libEGL
  unofficial::angle::libGLESv2
  $<IF:$<BOOL:${C_OPENGL}>,libglad,>
  $<IF:$<BOOL:${C_ANGLE}>,libglad,>
  )
