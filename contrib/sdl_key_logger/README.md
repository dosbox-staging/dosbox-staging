# SDL key logger

The key logger prints the following SDL key details:

- scancode names and values
- sdlk names and values
- kmod values

Its purpose is to help figure out the connection between physical
keyboard keys and SDL's enumerated scancodes and sdlk values.

## Build

```shell
meson setup build
meson compile -C build
```

## Run

```shell
  ./build/logger
```

## Add handling for a new key

Pre-requisite: the physical keyboard with the (currently)
unhandled key

1. Compile and run the logger. Press the unhandled key and get
   the SDL scancode number for the key (get the decimal value, not
   the hex value).

2. Using the key's SDL scancode number, open SDL's
   `SDL_scancode.h` header and find the corresponding enum key
   name.

3. In `sdl_mapper.cpp`:

   - Confirm the SDL enum name doesn't exist (yet) in this
     file.

   - Add the SDL enum and a corresponding new shorthand string
     to the DefaultKeys[] array.

   - add the key's default symbol and your new shorthand name
     to the best KeyBlock array, along with a new KBD\_<name> for
     the key

   - if the logger didn't report a text scancode name for the
     key, then add your new shorthand name for it in
     GetBindName(), similar to 'oem102' and 'abnt1', for which
     SDL also doesn't have SDL scancode names.

4. In keyboard.h and .cpp:

   - Using http://kbdlayout.info using the correct arrangement,
     look up the key's (non-SDL) scancode hex value.

   - Convert that (non-SDL) hex scancode to decimal value.

   - Add your new KBD_shortname enum name KEYBOARD_AddKey()
     switch statement, with the ret=<value> being the (non-SDL)
     scancode decimal value.

5. In bios_keyboard.cpp's get_key_codes_for() function:

   - Find (or add) the decimal index entry matching the ret=
     (non-SDL) scancode decimal value.

   - Add four codes for the key, matching the same pattern (or
     use existing code). For example, the codes usually start
     with the high-byte being the hex value of the (non-SDL)
     scancode number.

For a graphical layout of this process, see the image attached
in PR: https://github.com/dosbox-staging/dosbox-staging/pull/2209
