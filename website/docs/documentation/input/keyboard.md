# Keyboard

DOSBox Staging captures your keyboard input and passes it straight to the
emulated DOS environment — most keys just work without any fuss.

The one wrinkle is that your host operating system intercepts certain key
combinations (++alt+tab++, ++cmd+h++, etc.) before DOSBox ever sees them.
The `keyboard_capture` setting lets you override this, so DOSBox gets first
dibs on those keys. This is especially useful in fullscreen mode or when a
game relies on key combinations that collide with OS shortcuts.

All key bindings can be customised through the key mapper (++ctrl+f1++ on
Windows/Linux, ++cmd+f1++ on macOS). See [Keyboard shortcuts](../shortcuts.md)
for the full list of default bindings.


## Configuration settings

The keyboard capture setting is configured in the `[sdl]` configuration
section.


##### keyboard_capture

:   Capture system keyboard shortcuts. When enabled, most system shortcuts
    such as Alt+Tab are captured and sent to DOSBox Staging. This is useful
    for Windows 3.1x and some DOS programs with unchangeable keyboard
    shortcuts that conflict with system shortcuts.

    Possible values: `on`, `off` *default*{ .default }


##### keyboard_layout

:   See [keyboard_layout](../system/dos.md#keyboard_layout).
