# DOS name handling

How dosbox-staging parses DOS names, what the layers do, and the
quirks contributors will trip over otherwise.

## Why this doc exists

DOS name parsing has non-obvious quirks: drive letters look like
filenames until they don't, device names shadow real files in every
directory, the optional trailing colon on devices is a thing. The
code comments handle the local "why this line"; this doc handles the
broader "why the architecture". If you're touching `DOS_MakeName`,
`DOS_FindDevice`, or any of the shell commands that resolve user
input to file handles, read this first.

## The three name shapes

DOS accepts three different shapes of name in the same syntactic
position (a file argument, a redirection target, an FCB name, etc.):

1. **Filenames** — `foo.txt`, `subdir\bar.txt`, `Z:\AUTOEXEC.BAT`.
   Standard 8.3 paths. Allowed characters in a path component:
   `A-Z`, `0-9`, upper-ASCII, and a small set of symbols:
   `$ # @ ( ) ! % { } \` ~ _ - . * ? & ' + ^` plus a handful of
   specific high-bit code points.

2. **Drive specs** — `A:`, `C:`. Exactly two characters: a letter and
   a colon. Means "current directory on that drive". May appear as
   a prefix (`C:foo.txt` = "file foo.txt in the current dir on C:")
   or standalone (`C:` = "the current dir on C:" — implicit on
   commands that change context, like `CD C:`).

3. **Device names** — `CON`, `PRN`, `AUX`, `NUL`, `LPT1`-`LPT3`,
   `COM1`-`COM4`. With OR without an optional trailing colon: `LPT1`
   and `LPT1:` are the same device. Case-insensitive (`lpt1`, `LPT1`,
   `Lpt1` all match).

## Drive letters vs single-letter filenames

This is the most-confusing distinction:

| Input    | Interpreted as |
| -------- | -------------- |
| `A`      | Filename `A` on the default drive |
| `A:`     | Drive spec — current dir on A: |
| `A:foo`  | Drive A:, file `foo` in current dir |
| `A:\foo` | Drive A:, root-relative path |

`COPY foo A` copies `foo` to a file literally called `A`. `COPY foo A:`
copies `foo` to A:'s current directory.

The parser distinguishes the two via `name[1] == ':'` at the top of
`DOS_MakeName` (`src/dos/dos_files.cpp`). Only fires when position 1
is a colon — a single-letter name has nothing at position 1, so it
flows through as a filename. Anything longer ending in `:` (like
`LPT1:`) is a device-name trailing colon, handled separately further
down.

## Device-name reservation is global

Device names shadow real files across the **entire filesystem**, not
just the root:

| Input | Resolves to |
| ----- | ----------- |
| `LPT1` | LPT1 device |
| `LPT1.txt` | LPT1 device (extension stripped) |
| `subdir\LPT1` | LPT1 device (the parent dir must exist on the drive — see staging quirk below) |
| `C:\foo\bar\CON.dat` | CON device |
| `Z:CON` | CON device (drive prefix consumed, then CON matched) |

This is the famous "you can't have a file called CON" reservation
that bit Windows for decades.

**Mechanism:** `DOS_OpenFile` and `DOS_CreateFile` both call
`DOS_FindDevice(name)` BEFORE the file-system lookup. If it matches,
the device wins and the file-system lookup never happens.
`DOS_FindDevice` parses the name (via `DOS_MakeName`), strips the
directory component with `strrchr('\\')`, strips a trailing extension
with `strrchr('.')`, and matches the remainder against the device
table.

**Staging quirk:** `DOS_FindDevice` also calls `TestDir()` on the
parent directory before matching the device. So a name like
`nonexistent\LPT1` does NOT match the LPT1 device — the path
validation fails first. Real MS-DOS is more permissive (the device
wins regardless of whether the path exists). This is a stricter
behaviour in staging, not a bug we need to fix for the current scope.

## The optional trailing colon

`LPT1` and `LPT1:` refer to the same device. `CON` and `CON:`,
`PRN` and `PRN:`, etc. The trailing colon is DOS punctuation — same
way `A:` puts a colon after a drive letter, the shells let you
write `LPT1:` after a device name for visual consistency.

This was not always handled at the parser layer. Before the fix,
`DOS_MakeName` validated each character against an allow-set that
didn't include `:`, so `LPT1:` failed parsing and `COPY foo LPT1:`
emitted `Illegal path`. The shell-redirection parser
(`src/shell/shell.cpp` `GetRedirection`) had its own caller-side
strip to work around this, but other entry points (COPY, plain
`DOS_OpenFile`) didn't.

The fix lives at the parser:
`src/dos/dos_files.cpp` `DOS_MakeName`, just after the trailing-space
strip, drops a final `:` from the in-progress name. Drive specs
(`C:`) are consumed at the very top of the function, so they're
never affected by this strip. Multiple trailing colons (`LPT1::`)
strip exactly one — the remaining `:` then fails the validation
loop as it should.

## What's still illegal

- Internal colons: `foo:bar`, `LPT1:foo`, `C:\foo:bar`. Only drive
  prefixes and trailing device-name colons are allowed; a colon
  anywhere else in a path component is a parse error.
- Multiple trailing colons: `LPT1::`. We strip one, the second
  fails the validation loop.
- Colon-only input: `:`. Stripping it would leave an empty name,
  which the parser refuses (intentional — was a parse error before
  the fix, still is).
- Non-allowed characters in a path component (control chars, most
  punctuation outside the allow-set).
- Names longer than `DOS_PATHLENGTH`.

The bare-non-device case (`foo:`) is a documented lenience: the parser
strips the colon and treats it as the filename `foo`. This matches
MS-DOS's permissive behaviour and what `shell.cpp`'s redirect parser
already did before the parser fix. DOS filenames can't legitimately
end in `:` (it's a reserved character), so this is a behaviour change
on inputs that were always malformed.

## The layering

```
            user input (shell, INT 21h, etc.)
                          |
                          v
                   DOS_MakeName              parses to (drive, fullpath)
                          |                  UPPERCASE, normalised,
                          v                  `.` and `..` resolved,
                  fullpath string            trailing device-colon stripped
                          |
          +---------------+---------------+
          v                               v
   DOS_FindDevice                file-system lookup
   (strip path + ext,            (DOS_FindFirst, the Drive's
    match against Devices)        File* implementations)
          |                               |
          v                               v
   device handle                  file handle (or error)
```

`DOS_MakeName` is the single parser. It doesn't know files from
devices. `DOS_FindDevice` is the device matcher; it runs on the
parsed name and gates the file lookup in `DOS_OpenFile` /
`DOS_CreateFile`. This separation is intentional and worth keeping:
MS-DOS itself uses one name format for files and devices
(`INT 21h` function `3Dh` handles both), so staging's parser
mirrors that.

## Gotchas for contributors

- **Don't add device awareness to `DOS_MakeName`.** The parser stays
  dumb-and-lenient about names; device matching is downstream in
  `DOS_FindDevice`. Detecting "is this a device name?" inside the
  parser would either require a hardcoded list there (duplication)
  or a call into the device subsystem (layering inversion).

- **Don't strip trailing colons in shell commands.** The parser
  handles it now. `src/shell/shell.cpp` still strips at the
  redirection-parser layer, defence-in-depth; future new code
  doesn't need to.

- **If you need stricter file-only semantics in a caller**, do it
  downstream: `if (DOS_FindDevice(name) != DOS_DEVICES) return error;`
  at the call site. Don't fork the parser.

- **Device handles and file handles flow through the same APIs.**
  `DOS_OpenFile` and `DOS_CreateFile` return either, transparently
  to most callers. `device_LPT1`, `device_CON`, `device_NUL` etc.
  implement `DOS_Device` which inherits from the same base as
  `DOS_File`.

- **LPT2 and LPT3 are not registered DOS devices today.** Only LPT1
  has a `device_LPT1` class. Opening LPT2 or LPT3 as a character
  device falls through to `device_NUL` (writes are discarded).
  Apps that talk to the parallel port directly (via `OUT` to ports
  0x278 / 0x378 / 0x3BC) still work because the printer's I/O port
  handlers are installed independently.

## Related code

| File | Role |
| ---- | ---- |
| `src/dos/dos_files.cpp` | `DOS_MakeName`, `DOS_OpenFile`, `DOS_CreateFile` |
| `src/dos/dos_devices.cpp` | `DOS_FindDevice`, device classes |
| `src/shell/shell.cpp` | Redirection parser (`GetRedirection`) |
| `src/shell/shell_cmds.cpp` | `CMD_COPY`, `CMD_TYPE`, etc. |
| `tests/dos_files_tests.cpp` | Parser and device-matching tests |
