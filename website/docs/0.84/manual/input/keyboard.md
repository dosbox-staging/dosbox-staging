# Keyboard

DOSBox Staging captures your keyboard input and passes it straight to the
emulated DOS environment --- most keys just work without any fuss.

The one wrinkle is that your host operating system intercepts certain key
combinations (++alt+tab++, ++cmd+h++, etc.) before DOSBox ever sees them.
The `keyboard_capture` setting lets you override this, so DOSBox gets first
dibs on those keys. This is especially useful in fullscreen mode or when a
game relies on key combinations that collide with OS shortcuts.

Set it to `auto` to make the keyboard follow the mouse: the shortcuts are only
captured while the [mouse is captured](mouse.md#mouse_capture). Releasing the
mouse (++ctrl+f10++ on Windows/Linux, ++cmd+f10++ on macOS) hands them back to
your operating system, so there's a single way out.

All key bindings can be customised through the [key mapper](keymapper.md)
(++ctrl+f1++ on Windows/Linux, ++cmd+f1++ on macOS). See
[Keyboard shortcuts](../appendices/shortcuts.md) for the full list of
default bindings.

## DOS keyboard layout and code pages

DOSBox Staging also emulates DOS's own keyboard layout and code page
(screen font) system, separate from the capture settings above. It's
managed from the DOS prompt with the `KEYB` and `CHCP` commands rather than
through configuration here --- see
[Keyboard layout and code pages](../system/localisation.md#keyboard-layout-and-code-pages)
for the full explanation.


## Configuration settings

The keyboard capture setting is configured in the `[sdl]` configuration
section.


##### keyboard_layout

:   See [`keyboard_layout`](../system/localisation.md#keyboard_layout).


##### keyboard_capture

:   Capture system keyboard shortcuts. When captured, most system shortcuts
    such as ++alt+tab++ are sent to DOSBox Staging instead of the host OS.
    Examples of captured shortcuts:

    - ++alt+tab++ (task switching on all platforms)
    - ++ctrl+esc++ (Start menu on Windows)
    - ++ctrl+f4++ (virtual desktop switching on KDE)
    - ++ctrl+left++ (show desktop on macOS)

    This is particularly useful for [Windows 3.1](../using-dosbox-staging/windows-31.md) and DOS
    programs that use key combinations which conflict with your OS shortcuts.

    Possible values:

    <div class="compact" markdown>

    - `auto` -- Capture system shortcuts only while the mouse is captured.
      Releasing the mouse gives the shortcuts back to the host OS.
    - `on` -- Always capture system shortcuts.
    - `off` *default*{ .default } -- Never capture system shortcuts.

    </div>


