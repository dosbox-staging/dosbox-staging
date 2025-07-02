add_library(libmisccore STATIC
  ansi_code_markup.cpp
  cross.cpp
  ethernet.cpp
  ethernet_slirp.cpp
  fs_utils.cpp
  fs_utils_posix.cpp
  fs_utils_win32.cpp
  help_util.cpp
  host_locale.cpp
  host_locale_macos.cpp
  host_locale_posix.cpp
  host_locale_win32.cpp
  pacer.cpp
  programs.cpp
  rwqueue.cpp
  setup.cpp
  string_utils.cpp
  support.cpp
  unicode.cpp
)

target_include_directories(libmisccore PRIVATE ../libs/include)

target_link_libraries(libmisccore PRIVATE
  libwhereami $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)

add_library(libmisc messages.cpp)
target_link_libraries(libmisc PRIVATE libmisccore)

add_library(libmiscstubs messages_stubs.cpp)
target_link_libraries(libmiscstubs PRIVATE libmisccore)