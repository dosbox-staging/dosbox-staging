manymouse_sources = files(
  'linux_evdev.c',
  'macosx_hidmanager.c',
  'macosx_hidutilities.c',
  'manymouse.c',
  'windows_wminput.c',
  'x11_xinput2.c',
)

libmanymouse = static_library(
  'manymouse',
  manymouse_sources,
  include_directories: incdir,
  dependencies: [
    iokit_dep,
    xinput2_dep,
  ],
)

manymouse_dep = declare_dependency(link_with: libmanymouse)
