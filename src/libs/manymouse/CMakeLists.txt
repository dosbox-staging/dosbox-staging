
if (C_MANYMOUSE)

  add_library(libmanymouse STATIC)
  target_link_libraries(libmanymouse PRIVATE project_headers)
  target_sources(libmanymouse PRIVATE
    linux_evdev.c
    macosx_hidmanager.c
    macosx_hidutilities.c
    manymouse.c
    windows_wminput.c
    x11_xinput2.c
  )

  if (APPLE)
    target_link_libraries(libmanymouse PRIVATE "-framework IOKit")
  endif()

  if (SUPPORT_XINPUT2)
    target_link_libraries(libmanymouse PRIVATE ${X11_Xinput_LIB})
  endif()

endif()
