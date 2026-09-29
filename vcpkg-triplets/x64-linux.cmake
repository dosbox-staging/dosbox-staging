set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Since microsoft/vcpkg#53196, the sdl3 port forces these optional features
# off, which breaks relative mouse mode on X11 (XInput2), among other things.
# Restore SDL's defaults so they're auto-detected from the host again; the
# packages they need are in extras/linux/ubuntu-24.04-packages.txt.
#
# These options are appended after the port's own, so they take precedence.
if(PORT STREQUAL "sdl3")
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS
        -DSDL_FRIBIDI=ON
        -DSDL_JACK=ON
        -DSDL_KMSDRM=ON
        -DSDL_LIBTHAI=ON
        -DSDL_LIBUDEV=ON
        -DSDL_LIBURING=ON
        -DSDL_PIPEWIRE=ON
        -DSDL_PULSEAUDIO=ON
        -DSDL_ROCKCHIP=ON
        -DSDL_RPI=ON
        -DSDL_SNDIO=ON
        -DSDL_WAYLAND_LIBDECOR=ON
        -DSDL_X11_XCURSOR=ON
        -DSDL_X11_XDBE=ON
        -DSDL_X11_XFIXES=ON
        -DSDL_X11_XINPUT=ON
        -DSDL_X11_XRANDR=ON
        -DSDL_X11_XSHAPE=ON
        -DSDL_X11_XSYNC=ON
        -DSDL_X11_XTEST=ON
    )
endif()
