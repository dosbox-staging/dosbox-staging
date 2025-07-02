set(DEBUGGER_SRC
  debug.cpp
  debug_disasm.cpp
  debug_gui.cpp)

target_sources(libdosboxcommon PRIVATE
  debug.cpp
  debug_disasm.cpp
  debug_gui.cpp)
  
target_link_libraries(libdosboxcommon PRIVATE libpdcurses)
