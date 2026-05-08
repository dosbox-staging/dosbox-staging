
if(C_MANYMOUSE)

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

  target_link_libraries(dosbox PRIVATE
    $<IF:$<BOOL:${SUPPORT_XINPUT2}>,${X11_Xinput_LIB},>
  )

endif()
