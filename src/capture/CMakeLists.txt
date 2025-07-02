add_library(libcapture STATIC
  capture.cpp
  capture_audio.cpp
  capture_midi.cpp
  capture_video.cpp

  image/image_capturer.cpp
  image/image_decoder.cpp
  image/image_saver.cpp
  image/image_scaler.cpp
  image/png_writer.cpp
)

find_package(PNG REQUIRED)
target_link_libraries(libcapture PRIVATE PNG::PNG)

target_link_libraries(libcapture PRIVATE
  zmbv
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)
