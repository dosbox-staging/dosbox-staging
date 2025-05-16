
if(C_MANYMOUSE)

  add_library(libmanymouse STATIC
    linux_evdev.c
    macosx_hidmanager.c
    macosx_hidutilities.c
    manymouse.c
    windows_wminput.c
    x11_xinput2.c
  )

endif()
