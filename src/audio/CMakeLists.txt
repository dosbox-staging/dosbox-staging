target_sources(libdosboxcommon PRIVATE
  clap/event_list.cpp
  clap/library.cpp
  clap/plugin.cpp
  clap/plugin_manager.cpp

  disk_noise.cpp
  compressor.cpp
  envelope.cpp
  mixer.cpp
  noise_gate.cpp
  opl_capture.cpp
)
