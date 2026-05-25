# Mouse

DOS games were written for two-button serial mice running through a DOS mouse driver. Modern
mice, modern OS drivers, and DOSBox Staging's own input layer can all produce behaviour that
doesn't quite match what the game expects. Most of these issues are straightforward to fix. See
the [Mouse reference](../input/mouse.md) for the full list of configuration options.

---

## Mouse sensitivity

If the cursor is too fast or too slow, adjust
[`mouse_sensitivity`](../input/mouse.md#mouse_sensitivity) in the `[mouse]` section. The
setting takes two values — horizontal and vertical — and some games need values significantly
above or below the default:

```ini
[mouse]
mouse_sensitivity = 100,100   ; default; raise or lower as needed
```

[Ultima Underworld I](https://www.mobygames.com/game/640/ultima-underworld-the-stygian-abyss/)
and [Ultima Underworld II](https://www.mobygames.com/game/1065/ultima-underworld-ii-labyrinth-of-worlds/)
are a well-documented case: the mouse has notoriously awkward handling that existed on real
hardware too, with a minimum movement threshold that makes fine movements very difficult.
`mouse_sensitivity = 150` or higher is needed to overcome the threshold.

---

## Erratic or inconsistent movement

Try routing mouse input through the OS rather than reading the hardware directly:

```ini
[mouse]
mouse_raw_input = off
```

Some games are also sensitive to CPU cycles for mouse behaviour — the cursor can disappear or
move sluggishly at certain cycle counts. If mouse problems correlate with a specific cycle
setting, try the documented recommended range for that game.

---

## Stuck movement at very low speeds

If the mouse feels "sticky" — movement only registers after overcoming a threshold — try:

```ini
[mouse]
builtin_dos_mouse_driver_move_threshold = 2
```

This is specifically useful for Ultima Underworld I & II, alongside raising
`mouse_sensitivity`.

---

## Game refuses to start: 3-button mouse required

Some games check the mouse button count at startup and exit immediately if they only detect
two buttons. DOSBox Staging defaults to emulating a 2-button mouse. Fix with:

```ini
[mouse]
mouse_button_count = 3
```

Or, equivalently, using the built-in driver model setting:

```ini
[mouse]
builtin_dos_mouse_driver_model = 3button
```

[Tornado](https://www.mobygames.com/game/TODO/tornado/) is a documented example —
it crashes on startup without a 3-button mouse reported.

---

## Game requires its own DOS mouse driver

Some games need a specific DOS mouse driver — such as Microsoft's `MSMOUSE.EXE` — rather than
DOSBox Staging's built-in driver. This was common in the Windows 3.1 era where games had
specific expectations about the driver stack, but some DOS games also require it. In these
cases, disable the built-in driver and load the game's driver in `[autoexec]` before launching:

```ini
[mouse]
builtin_dos_mouse_driver = off

[autoexec]
MSMOUSE.EXE /q
GAME.EXE
```

[C.I.T.Y. 2000 (1999)](https://www.mobygames.com/game/TODO/city-2000/) needs `MSMOUSE.EXE`
with specific parameters (`/p4 /r1 /s50 /q`) to keep the mouse responsive during dialogue
scenes in Madame Tussauds. Without it, the mouse freezes when starting dialogue.

[Viper (1998)](https://www.mobygames.com/game/TODO/viper/) takes this further — it freezes
on startup entirely unless the built-in DOS mouse driver is disabled, even without loading a
replacement driver.

[Indiana Jones and His Desktop Adventures (Win 3.1)](https://www.mobygames.com/game/TODO/indiana-jones-and-his-desktop-adventures/)
requires `file_locking = off` rather than a different driver, but is similarly sensitive to
the driver environment — see [File locking](sound.md#file-locking) on the Sound page.

---

## Mouse completely unresponsive

If the mouse doesn't respond at all — not a sensitivity issue, but zero movement registered —
try disabling the built-in driver:

```ini
[mouse]
builtin_dos_mouse_driver = off
```

This is distinct from the "requires its own driver" case above. Some games conflict with
DOSBox Staging's built-in driver without needing a specific replacement — they just need the
built-in one out of the way.
