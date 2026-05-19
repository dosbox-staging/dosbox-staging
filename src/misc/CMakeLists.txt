target_sources(dosboxcommon PRIVATE
  ansi_code_markup.cpp
  console.cpp
  cross.cpp
  dos_rwops.cpp
  fs_utils.cpp
  help_util.cpp
  host_locale.cpp
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

# Linux & macOS-only filesystem helpers
if (NOT WIN32)
  target_sources(dosboxcommon PRIVATE fs_utils_posix.cpp)
endif()

# macOS-only locale detection
if (C_COREFOUNDATION)
  target_sources(dosboxcommon PRIVATE host_locale_macos.cpp)
endif()

target_sources(dosbox PRIVATE messages.cpp)

if (OPT_TESTS)
  target_sources(dosbox_tests PRIVATE messages_stubs.cpp)
endif()

target_include_directories(dosboxcommon SYSTEM PRIVATE ../libs/include)
target_link_libraries(dosboxcommon PRIVATE whereami)

# Windows-only filesystem helpers
if (WIN32)
  target_sources(dosboxcommon PRIVATE fs_utils_win32.cpp)
  # Shlwapi needed for PathFileExistsW
  target_link_libraries(dosboxcommon PRIVATE Shlwapi)
endif()
