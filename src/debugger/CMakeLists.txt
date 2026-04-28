set(DEBUGGER_SRC
  debugger.cpp
  debugger_disasm.cpp
  debugger_gui.cpp)

target_sources(libdosboxcommon PRIVATE
  debugger.cpp
  debugger_disasm.cpp
  debugger_gui.cpp)

target_link_libraries(libdosboxcommon PRIVATE imgui)
