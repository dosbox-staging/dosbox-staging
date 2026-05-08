
if (C_MANYMOUSE)

  add_library(manymouse STATIC)
  target_link_libraries(manymouse PRIVATE project_headers)
  target_sources(manymouse PRIVATE
    linux_evdev.c
    macosx_hidmanager.c
    macosx_hidutilities.c
    manymouse.c
    windows_wminput.c
    x11_xinput2.c
  )

  if (APPLE)
    target_link_libraries(manymouse PRIVATE "-framework IOKit")
  endif()

  if (SUPPORT_XINPUT2)
    target_link_libraries(manymouse PRIVATE ${X11_Xinput_LIB})
  endif()

endif()
