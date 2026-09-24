target_sources(dosboxcommon PRIVATE
  bridge.cpp
  capture.cpp
  file_log.cpp
  signatures.cpp
  webserver.cpp
  cpu.cpp
  memory.cpp
  dos.cpp
  dosbox.cpp)

target_link_libraries(dosboxcommon PRIVATE simde base64 httplib json)
