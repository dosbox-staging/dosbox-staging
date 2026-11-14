target_sources(dosboxcommon PRIVATE
  bridge.cpp
  webserver.cpp
  cpu.cpp
  memory.cpp
  dos.cpp
  dosbox.cpp)

if (OPT_DEBUGGER)
  target_sources(dosboxcommon PRIVATE
    debug.cpp)
endif()

target_link_libraries(dosboxcommon PRIVATE simde base64 httplib json)
