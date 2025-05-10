add_library(libpdcurses STATIC
  pdcurses/addch.c
  pdcurses/addchstr.c
  pdcurses/addstr.c
  pdcurses/attr.c
  pdcurses/beep.c
  pdcurses/bkgd.c
  pdcurses/border.c
  pdcurses/clear.c
  pdcurses/color.c
  pdcurses/debug.c
  pdcurses/delch.c
  pdcurses/deleteln.c
  pdcurses/getch.c
  pdcurses/getstr.c
  pdcurses/getyx.c
  pdcurses/inch.c
  pdcurses/inchstr.c
  pdcurses/initscr.c
  pdcurses/inopts.c
  pdcurses/insch.c
  pdcurses/insstr.c
  pdcurses/instr.c
  pdcurses/kernel.c
  pdcurses/keyname.c
  pdcurses/mouse.c
  pdcurses/move.c
  pdcurses/outopts.c
  pdcurses/overlay.c
  pdcurses/pad.c
  pdcurses/panel.c
  pdcurses/printw.c
  pdcurses/refresh.c
  pdcurses/scanw.c
  pdcurses/scr_dump.c
  pdcurses/scroll.c
  pdcurses/slk.c
  pdcurses/termattr.c
  pdcurses/touch.c
  pdcurses/util.c
  pdcurses/window.c

  sdl2_queue/pdcclip.cpp
  sdl2_queue/pdcdisp.cpp
  sdl2_queue/pdcgetsc.cpp
  sdl2_queue/pdckbd.cpp
  sdl2_queue/pdcscrn.cpp
  sdl2_queue/pdcsetsc.cpp
  sdl2_queue/pdcutil.cpp
)

target_include_directories(libpdcurses PUBLIC . pdcurses)

target_link_libraries(
  libpdcurses PRIVATE $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)
