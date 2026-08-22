set(DEBUGGER_SRC
  debugger_control.cpp
  debugger.cpp
  debugger_disasm.cpp
  debugger_gui.cpp)

target_sources(dosboxcommon PRIVATE
  debugger_control.cpp
  debugger.cpp
  debugger_disasm.cpp
  debugger_gui.cpp)

target_link_libraries(dosboxcommon PRIVATE imgui)
