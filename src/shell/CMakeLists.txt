target_sources(libdosboxcommon PRIVATE
  autoexec.cpp
  command_line.cpp
  file_reader.cpp
  shell.cpp
  shell_batch.cpp
  shell_history.cpp
  shell_misc.cpp)

target_sources(dosbox PRIVATE shell_cmds.cpp)
