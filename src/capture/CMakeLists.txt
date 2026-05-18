target_sources(dosboxcommon PRIVATE 
  capture.cpp
  capture_audio.cpp
  capture_midi.cpp
  capture_video.cpp

  image/image_capturer.cpp
  image/image_saver.cpp
  image/image_scaler.cpp
  image/png_writer.cpp
  video/zmbv.cpp
)

target_link_libraries(dosboxcommon PRIVATE 
  ${ZMBV_ZLIB_TARGET}
  PNG::PNG
)
