target_sources(libdosboxcommon PRIVATE 
  capture.cpp
  capture_audio.cpp
  capture_midi.cpp
  capture_video.cpp

  image/image_capturer.cpp
  image/image_saver.cpp
  image/image_scaler.cpp
  image/png_writer.cpp)

target_link_libraries(libdosboxcommon PRIVATE 
  zmbv
  PNG::PNG)
