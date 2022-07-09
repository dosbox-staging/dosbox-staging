# ManyMouse

ManyMouse's website is https://icculus.org/manymouse/

This is a simple library to abstract away the reading of multiple input
devices. It is designed to work cross-platform.

Just copy all of the C files and headers in this directory into your
project, and build the C files. Unless explicitly noted, you shouldn't have
to #define anything, and each file is wrapped in #ifdefs to avoid compiling
on the wrong platforms, so it is safe to build everything without close
examination.

You don't have to build this as a shared library; we encourage you to just
compile the source and statically link them into your application...this
makes integrating ManyMouse much less complex.

The "example" directory contains complete programs to demostrate the use of
the ManyMouse API in action. These files don't need to be copied into your
project, but you can cut-and-paste their contents as needed.

## Basic usage:

- Copy *.c and *.h from the base of the manymouse folder to your project.
- Add the new files to your project's build system.
- #include "manymouse.h" in your source code
- Call ManyMouse_Init() once before using anything else in the library,
  usually at program startup time. If it returns > 0, that's the number of
  mice it found. If it returns zero, it means the system works, but there
  aren't any mice to be found, and calling ManyMouse_Init() may report mice
  in the future if one is plugged in. If it returns < 0, it means the system
  will never report mice; this can happen, for example, on Windows 95, which
  lacks functionality we need that was introduced with Windows XP.
- Call ManyMouse_DriverName() if you want to know the human-readable
  name of the driver that handles devices behind the scenes. Some platforms
  have different drivers depending on the system being used. This is for
  debugging purposes only: it is not localized and we don't promise they
  won't change. The string is in UTF-8 format. Don't free this string.
  This will return NULL if ManyMouse_Init() failed.
- Call ManyMouse_DeviceName() if you want to know the human-readable
  name of each device ("Logitech USB mouse", etc). This is for debugging
  purposes only: it is not localized and we don't promise they won't change.
  As these strings are created by the device and the OS, we can't even
  promise they'll even actually help you identify the mouse in your hand;
  sometimes, they are quite lousy descriptions. The string is in UTF-8
  format. Don't free this string.
- Read input from the mice with ManyMouse_PollEvent() in a loop until the
  function returns 0. Each time through the loop, examine the event that
  was returned and react appropriately. Do this with regular frequency:
  generally, a good rule is to poll for ManyMouse events at the same time
  you poll for other system GUI events...once per iteration of your
  program's main loop.
- When you are done processing mice, call ManyMouse_Quit() once, usually at
  program termination. You should call this even if ManyMouse_Init() returned
  zero.

There are examples of complete usage in the "example" directory. The simplest
is test_manymouse_stdio.c


## Thread safety note:

Pick a thread to call into ManyMouse from, and don't call into it from any
other. We make no promises on any platform of thread safety. For safety's
sake, you might want to use the same thread that talks to the system's
GUI interfaces and/or the main thread, if you have one.


## Building the code:

The test apps on Linux and Mac OS X can be built by running "make" from a
terminal. The SDL test app will fail if
[Simple Directmedia Layer](https://libsdl.org/) isn't installed. The stdio
apps will still work.

Windows isn't integrated into the Makefile, since most people will want to
put it in a VS.NET project anyhow, but here's the command line used to
build some of the standalone test apps:

    cl /I. *.c example\test_manymouse_stdio.c /Fetest_manymouse_stdio.exe
    cl /I. *.c example\detect_mice.c /Fedetect_mice.exe

(yes, that's simple, that's the point)...getting the SDL test app to work
on Windows can be done, but takes too much effort unrelated to ManyMouse
itself for this document to explain.


## Java bindings:

There are now Java bindings available in the contrib/java directory.
They should work on any platform that ManyMouse supports that has a Java
virtual machine. If you're using the makefile, you can run "make java" and
it will produce the native code glue library and class files that you can
include in your application. Please see contrib/java/TestManyMouse.java
for an example of how to use ManyMouse from your Java application. Most
of this documentation talks about the C interface to ManyMouse, but with
minor modifications also applies to the Java bindings. We welcome patches
and bug reports on the Java bindings, but don't officially support them.

Mac OS X, Linux and Cygwin can run `make java` to build the bindings and run
`java TestManyMouse` to make sure they worked. Cygwin users may need to
adjust the `WINDOWS_JDK_PATH` line at the top of the Makefile to match their
JDK installation. Linux users should do the same with `LINUX_JDK_PATH` and
make sure that the resulting libManyMouse.so file is in their dynamic loader
path (LD_LIBRARY_PATH or whatnot) so that the Java VM can find it.

Thanks to Brian Ballsun-Stanton for kicking this into gear. Jeremy Brown
gave me the rundown on getting this to work with Cygwin.


## Matlab/Octave bindings:

There's a little Matlab/Octave wrapper in contrib/matlab. It can be compiled
using the function `compile_manymouse_mex.m`. Tested under Windows 7
(Matlab 2010b) and Ubuntu 12.04 (Octave 3.2.4 and Matlab 2011a).

Demo scripts for Octave compatible function call ('demo_mex.m') as well as
Matlab's class Interface ('ManyMouse.m', 'demo_class.m') are also included.

Thanks to Thomas Weibel for the Matlab work.


## Some general ManyMouse usage notes:

- If a mouse is disconnected, it will not return future events, even if you
  plug it right back in. You will be alerted of disconnects programmatically
  through the MANYMOUSE_EVENT_DISCONNECT event, which will be the last
  event sent for the disconnected device. You can safely redetect all mice by
  calling ManyMouse_Quit() followed by ManyMouse_Init(), but be warned that
  this may cause mice (even ones that weren't unplugged) to suddenly have a
  different device index, since on most systems, the replug will cause the
  mouse to show up elsewhere in the system's USB device tree. It is
  recommended that you make redetection an explicit user-requested function
  for this reason.
- In most systems, all mice will control the same system cursor. It's
  recommended that you ask your window system to grab the mouse input to your
  application and hide the system cursor, and then do all mouse input
  processing through ManyMouse. Most GUI systems will continue to deliver
  mouse events through the system cursor even when ManyMouse is working; in
  these cases, you should continue to read the usual GUI system event queue,
  for the usual reasons, and just throw away the mouse events, which you
  instead grab via ManyMouse_PollEvent(). Hiding the system cursor will mean
  that you may need to draw your own cursor in an app-specific way, but
  chances are you need to do that anyhow if you plan to support multiple
  mice. Grabbing the input means preventing other apps from getting mouse
  events, too, so you'll probably need to build in a means to ungrab the
  mouse if the user wants to, say, respond to an instant message window or
  email...again, you will probably need to do this anyhow.
- On Windows, ManyMouse requires Windows XP or later to function, since it
  relies on APIs that are new to XP...it uses LoadLibrary() on User32.dll and
  GetProcAddress() to get all the Windows entry points it uses, so on pre-XP
  systems, it will run, but fail to find any mice in ManyMouse_Init().
  ManyMouse does not require a window to run, and can even be used by console
  (stdio) applications. Please note that using DirectInput at the same time
  as ManyMouse can cause problems; ManyMouse does not use DirectInput, due
  to DI8's limitations, but its parallel use seems to prevent ManyMouse from
  getting mouse input anyhow. This is mostly not an issue, but users of
  [Simple Directmedia Layer](https://libsdl.org/) may find that it uses
  DirectInput behind the scenes. We are still researching the issue, but we
  recommend using SDL's "windib" target in the meantime to avoid this
  problem.
- On Unix systems, we try to use the XInput2 extension if possible.
  ManyMouse will try to fallback to other approaches if there is no X server
  available or the X server doesn't support XInput2. If you want to use the
  XInput2 target, make sure you link with "-ldl", since we use dlopen() to
  find the X11/XInput2 libraries. You do not have to link against Xlib
  directly, and ManyMouse will fail gracefully (reporting no mice in the
  ManyMouse XInput2 driver) if the libraries don't exist on the end user's
  system. Naturally, you'll need the X11 headers on your system (on Ubuntu,
  you would want to apt-get install libxi-dev). You can build with
  `SUPPORT_XINPUT2` defined to zero to disable XInput2 support completely.
  Please note that the XInput2 target does not need your app to supply an X11
  window. The test_manymouse_stdio app works with this target, so long as the
  X server is running. Please note that the X11 DGA extension conflicts with
  XInput2 (specifically: SDL might use it). This is a good way to deal with
  this in SDL 1.2:
  ```c
  char namebuf[16];
  const char *driver;

  SDL_Init(SDL_INIT_VIDEO);
  driver = SDL_VideoDriverName(namebuf, sizeof (namebuf));
  if (driver && (strcmp(driver, "x11") == 0)) {
      if (strcmp(ManyMouse_DriverName(), "X11 XInput2 extension") == 0) {
          setenv("SDL_MOUSE_RELATIVE", "0", 1);
      }
  }

  // now you may call SDL_SetVideoMode() or SDL_WM_GrabInput() safely.
  ```
- On Linux, we can try to use the /dev/input/event* devices; this means
  that ManyMouse can function with or without an X server. Please note that
  modern Linux systems only allow root access to these devices. Most users
  will want XInput2, but this can be used if the device permissions allow.
- There (currently) exists a class of users that have Linux systems with
  evdev device nodes forbidden to all but the root user, and no XInput2
  support. These users are out of luck; they should either force the
  permissions on /dev/input/event*, or upgrade their X server. This is a
  problem that will solve itself with time.
- On Mac OS X, we use IOKit's HID Manager API, which means you can use this
  C-callable library from Cocoa, Carbon, and generic Unix applications, with
  or without a GUI. There are Java bindings available, too, letting you use
  ManyMouse from any of the official Mac application layers. This code may or
  may not work on Darwin (we're not sure if IOKit is available to that
  platform); reports of success are welcome. If you aren't already, you will
  need to make sure you link against the "Carbon" and "IOKit" frameworks once
  you add ManyMouse to your project.
- Support for other platforms than Mac OS X, Linux, and Windows is not
  planned, but contributions of implementations for other platforms are
  welcome.

Please see the file LICENSE.txt in the source's root directory.

This library was written by Ryan C. Gordon: icculus@icculus.org

--ryan.

