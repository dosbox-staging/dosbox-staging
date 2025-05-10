add_library(libshell STATIC
  autoexec.cpp
  command_line.cpp
  file_reader.cpp
  shell.cpp
  shell_batch.cpp
  shell_cmds.cpp
  shell_history.cpp
  shell_misc.cpp
)

target_link_libraries(libshell PRIVATE
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)
