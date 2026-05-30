target_sources(dosboxcommon PRIVATE
  bridge.cpp
  webserver.cpp
  cpu.cpp
  drives.cpp
  memory.cpp
  dos.cpp
  dosbox.cpp)

target_link_libraries(dosboxcommon PRIVATE simde base64 httplib json)
