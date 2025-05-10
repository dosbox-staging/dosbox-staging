add_library(libints STATIC
  bios.cpp
  bios_disk.cpp
  bios_keyboard.cpp
  bios_pci.cpp
  ems.cpp
  int10.cpp
  int10_char.cpp
  int10_memory.cpp
  int10_misc.cpp
  int10_modes.cpp
  int10_pal.cpp
  int10_put_pixel.cpp
  int10_vesa.cpp
  int10_video_state.cpp
  int10_vptable.cpp
  xms.cpp
)

target_link_libraries(libints PRIVATE
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)
