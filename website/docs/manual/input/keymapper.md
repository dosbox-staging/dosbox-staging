# Key mapper

The **key mapper** lets you rebind any keyboard key, mouse button, or joystick
control to any emulated input event. It is the central tool for customising
controls in DOSBox Staging --- whether you need to swap two keys, assign a
gamepad button to a keyboard action, or build a complete controller profile
for a keyboard-only game.

Open the key mapper by pressing ++ctrl+f1++ on Windows/Linux or ++cmd+f1++ on
macOS during gameplay.


## The key mapper interface

The key mapper displays a visual layout of a virtual keyboard and joystick.
Each element on this layout represents an **event** --- the input that DOSBox
reports to the running DOS program. Clicking an event shows its current
**bindings**: the physical keys, buttons, or axes that trigger it.

The interface provides the following controls:

<div class="compact" markdown>

- **Add** --- Bind a new physical key or button to the selected event. After
  clicking Add, press the key or button you want to assign.
- **Del** --- Remove the currently displayed binding from the selected event.
- **Next** --- Cycle through multiple bindings for the same event (an event
  can have more than one binding).
- **Save** --- Save the current mappings to a mapper file and return to the
  game.
- **Exit** --- Return to the game without saving.

</div>

The modifier checkboxes (**mod1**, **mod2**, **mod3**) let you create
modified bindings --- for example, requiring ++ctrl++ to be held along with a
key. This is how DOSBox's own shortcuts work (e.g., ++ctrl+f9++ for shutdown).


## Remapping keys

To swap two keys (for example, Y and Z on a German keyboard layout):

1. Open the key mapper (++ctrl+f1++ / ++cmd+f1++).
2. Click the **Y** key on the virtual keyboard.
3. Click **Del** to remove its default binding.
4. Click **Add**, then press the physical **Z** key.
5. Now click the **Z** key on the virtual keyboard.
6. Click **Del** to remove its default binding.
7. Click **Add**, then press the physical **Y** key.
8. Click **Save** to store the mappings.

The same process works for any key --- click the event on the virtual layout,
remove the old binding, add a new one.


## Remapping joystick axes

To invert a joystick axis (e.g., reverse the Y-axis):

1. Click the top of the Y-axis on the virtual joystick layout.
2. Click **Del** to remove the default binding.
3. Click **Add**, then push your physical joystick in the *opposite* direction.
4. Repeat for the other direction of the same axis.
5. Click **Save**.


## Mapping controller buttons to keys

The key mapper can bind gamepad buttons to keyboard keys, which is how you
play keyboard-only DOS games with a controller. The basic approach:

1. Configure the game for keyboard-only controls (disable joystick in the
   game's setup if possible).
2. In the key mapper, click the key you want to trigger (e.g., the
   ++up++ arrow on the virtual keyboard).
3. Click **Add**, then press the corresponding button or direction on your
   controller.
4. Repeat for all keys the game uses.
5. Click **Save**.

!!! tip

    When using a controller this way, set `joysticktype = hidden` in the
    `[joystick]` section so the game doesn't detect the controller as a
    joystick. You may also want to set `capture_mouse = nomouse` in the
    `[sdl]` section to prevent analog stick movement from being interpreted as
    mouse input.


## Mapper files

Mappings are saved to a **mapper file** --- a text file that records every
binding. By default, the file is named `mapper-sdl2-<version>.map` and is
stored alongside your primary config file.

You can specify a custom mapper file path via the
[`mapperfile`](../graphics/display-and-window.md#mapperfile) setting in the
`[sdl]` section:

``` ini
[sdl]
mapperfile = my-game.map
```

DOSBox Staging ships with preconfigured mapper files for many games in the
`resources/mapperfiles` directory. These can be loaded by name:

``` ini
[sdl]
mapperfile = xbox/doom.map
```


### Mapper file format

Each line in a mapper file maps an event name to one or more bindings:

```
event_name "binding_1" "binding_2"
```

For example:

```
key_esc "key 41" "stick_0 button 6"
```

This binds both the physical Escape key (`key 41`) and gamepad button 6
(Xbox Select) to the emulated Escape key.


### Binding syntax

**Keyboard bindings** use the format `key <scancode>`, optionally followed by
modifier flags (`mod1`, `mod2`, `mod3`):

```
key 41          (Escape)
key 66 mod1     (Ctrl+F9, i.e., mod1 = Ctrl)
```

**Joystick bindings** use the format `stick_N <type> <id>`, where:

- `stick_0` is the first controller
- `button <id>` is a button press
- `axis <id> <direction>` is an axis movement (0 = negative, 1 = positive)
- `hat <id> <direction>` is a D-pad direction (1 = up, 2 = right, 4 = down,
  8 = left)

Examples:

```
stick_0 button 0      (A button)
stick_0 axis 1 0      (left stick up)
stick_0 hat 0 1       (D-pad up)
```

**Modifier bindings** let you define a button as a modifier key, then use it
in combination with other buttons:

```
mod_3 "stick_0 button 6"
hand_shutdown "key 66 mod1" "stick_0 button 7 mod3"
```

This defines gamepad button 6 (Select) as `mod3`, then binds
Select+Start (button 7) to ++ctrl+f9++ (shutdown).


### Xbox controller reference

For convenience, here are the common Xbox controller mappings:

<div class="compact" markdown>

| Control       | Binding                           |
|---------------|-----------------------------------|
| A             | `stick_0 button 0`                |
| B             | `stick_0 button 1`                |
| X             | `stick_0 button 2`                |
| Y             | `stick_0 button 3`                |
| LB            | `stick_0 button 4`                |
| RB            | `stick_0 button 5`                |
| Select / Back | `stick_0 button 6`                |
| Start         | `stick_0 button 7`                |
| L3            | `stick_0 button 9`                |
| R3            | `stick_0 button 10`               |
| D-pad up      | `stick_0 hat 0 1`                 |
| D-pad right   | `stick_0 hat 0 2`                 |
| D-pad down    | `stick_0 hat 0 4`                 |
| D-pad left    | `stick_0 hat 0 8`                 |
| Left stick X  | `stick_0 axis 0` (0=left, 1=right) |
| Left stick Y  | `stick_0 axis 1` (0=up, 1=down)  |
| Right stick X | `stick_0 axis 3` (0=left, 1=right) |
| Right stick Y | `stick_0 axis 4` (0=up, 1=down)  |
| LT            | `stick_0 axis 2 2`                |
| RT            | `stick_0 axis 5 2`                |

</div>

!!! note

    Button numbers may differ between controller brands. On Linux, you can
    use `jstest` to identify the correct IDs for your controller.


## Resetting the mapper

If your key mappings get into a broken state, use the
[`--erasemapper`](../command-line.md#-erasemapper) command line option to
delete the default mapper file and reset all bindings to their defaults.

!!! warning

    `--erasemapper` only deletes the default mapper file. If you're using a
    custom `mapperfile` path, you'll need to delete or edit that file manually.


## Limitations

- Only keyboard keys can be remapped to controller buttons and vice versa.
  Mouse buttons and mouse movement cannot be mapped through the key mapper.
- The key mapper interface shows a US keyboard layout regardless of your
  physical keyboard layout. The bindings work correctly --- only the visual
  representation in the mapper UI uses US layout positions.

For the full list of mappable events and their default bindings, see the
[Keymapper reference](../appendices/keymapper.md).
