target_sources(dosboxcommon PRIVATE
  bridge.cpp
  webserver.cpp
  cpu.cpp
  memory.cpp
  dos.cpp)

target_link_libraries(dosboxcommon PRIVATE simde)
