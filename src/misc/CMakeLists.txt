target_sources(libdosboxcommon PRIVATE
  ansi_code_markup.cpp
  console.cpp
  cross.cpp
  dos_rwops.cpp
  fs_utils.cpp
  fs_utils_posix.cpp
  fs_utils_win32.cpp
  help_util.cpp
  host_locale.cpp
  host_locale_macos.cpp
  host_locale_posix.cpp
  host_locale_win32.cpp
  image_decoder.cpp
  iso_locale_codes.cpp
  messages_adjust.cpp
  messages_po_entry.cpp
  rwqueue.cpp
  support.cpp
  unicode.cpp
  unicode_encodings.cpp
  video.cpp
)

target_sources(dosbox PRIVATE messages.cpp)

if(OPT_TESTS)
  target_sources(dosbox_tests PRIVATE messages_stubs.cpp)
endif()

target_include_directories(libdosboxcommon PRIVATE ../libs/include)
target_link_libraries(libdosboxcommon PRIVATE libwhereami)

