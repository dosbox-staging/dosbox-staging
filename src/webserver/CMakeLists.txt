target_sources(libdosboxcommon PRIVATE
  bridge.cpp
  webserver.cpp
  cpu.cpp
  memory.cpp
  dos.cpp)

target_link_libraries(libdosboxcommon PRIVATE simde)
