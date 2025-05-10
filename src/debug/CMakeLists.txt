add_library(libdebug STATIC
  debug.cpp
  debug_disasm.cpp
  debug_gui.cpp
)

target_link_libraries(libdebug PRIVATE
  libpdcurses $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)
