set(freedoskeyboard_files
    KEYBOARD.SYS
    KEYBRD2.SYS
    KEYBRD3.SYS
    KEYBRD4.SYS
)

list(TRANSFORM freedoskeyboard_files PREPEND "freedos-keyboard/")