if (NOT OPT_DEBUGGER)
  message(FATAL_ERROR "PDCurses should only be built when the debugger is enabled")
endif()

set(sdl2_dir sdl2_queue)

add_library(pdcurses STATIC
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
  ${sdl2_dir}/pdcclip.cpp
  ${sdl2_dir}/pdcdisp.cpp
  ${sdl2_dir}/pdcgetsc.cpp
  ${sdl2_dir}/pdckbd.cpp
  ${sdl2_dir}/pdcscrn.cpp
  ${sdl2_dir}/pdcsetsc.cpp
  sdl2_queue/pdcutil.cpp
)

target_include_directories(pdcurses PUBLIC . pdcurses)

target_link_libraries(pdcurses PRIVATE
  project_headers
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)
