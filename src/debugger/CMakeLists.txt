set(DEBUGGER_SRC
  debugger.cpp
  debugger_disasm.cpp
  debugger_gui.cpp)

target_sources(dosboxcommon PRIVATE
  debugger.cpp
  debugger_disasm.cpp
  debugger_gui.cpp)

target_link_libraries(dosboxcommon PRIVATE imgui)
