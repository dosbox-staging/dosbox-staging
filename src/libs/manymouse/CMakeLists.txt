
if(C_MANYMOUSE)

  add_library(libmanymouse STATIC
    linux_evdev.c
    macosx_hidmanager.c
    macosx_hidutilities.c
    manymouse.c
    windows_wminput.c
    x11_xinput2.c
  )

  target_link_libraries(dosbox PRIVATE
    $<IF:$<BOOL:${SUPPORT_XINPUT2}>,${X11_Xinput_LIB},>
  )

  if (DOSBOX_PLATFORM_MACOS)
    target_link_libraries(libmanymouse PRIVATE ${IOKit_LIBRARIES})
  endif()

endif()
