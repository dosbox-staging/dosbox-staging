add_library(libshellcore STATIC
  autoexec.cpp
  command_line.cpp
  file_reader.cpp
  shell.cpp
  shell_batch.cpp
  shell_history.cpp
  shell_misc.cpp
)

target_link_libraries(libshellcore PRIVATE
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)

add_library(libshell STATIC shell_cmds.cpp)
target_link_libraries(libshell PRIVATE libshellcore)
