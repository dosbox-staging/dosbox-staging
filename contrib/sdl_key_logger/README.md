# SDL key logger

The key logger prints the following SDL key details:

- scancode names and values
- sdlk names and values
- kmod values

Its purpose is to help figure out the connection between physical
keyboard keys and SDL's enumerated scancodes and sdlk values.

## Build

 meson setup build
 meson compile -C build

## Run

 ./build/logger
