target_sources(libdosboxcommon PRIVATE
  bridge.cpp
  webserver.cpp
  cpu.cpp
  memory.cpp
  dos.cpp
  video.cpp)

find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)

target_link_libraries(libdosboxcommon PRIVATE simde JPEG::JPEG PNG::PNG)
