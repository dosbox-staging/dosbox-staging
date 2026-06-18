# DOSBox Staging config migration — findings

Research output for the planned config-migration feature. Captures the complete
delta of configuration settings between v0.80.0, v0.81.2, v0.82.2, and current
`main` (in-progress v0.83). Built from primary-source reading of the code at
each tag plus cross-referencing against the release notes.

This document is the rule data for the migrator implementation. It is not yet
filtered to the eventual migrator schema.

## Scope and method

- **Versions covered**: migrations FROM v0.80.x, v0.81.x, v0.82.x TO current
  main. Older versions explicitly out of scope.
- **Tags walked**: `v0.80.0`, `v0.81.2`, `v0.82.2`, `main` (HEAD). Patch
  releases between minor tags rarely change config and were spot-checked only
  where release notes hinted at a config change.
- **Code sources**: `git show <tag>:<path>` to read each section's
  registration code (`AddSection`, `Add_string`, `Add_bool`, `Add_int`,
  `Add_path`, `Add_hex`, `AddMultiVal`, `Add_double`) at each tag. Paths shift
  across versions (e.g. `src/gui/sdlmain.cpp` → `src/gui/sdl_gui.cpp`,
  `src/hardware/sblaster.cpp` → `src/hardware/audio/soundblaster.cpp`).
- **Release notes**: `website/docs/releases/release-notes/0.80.0.md`,
  `0.80.1.md`, `0.81.0.md`..`0.81.2.md`, `0.82.0.md`..`0.82.2.md`,
  `0.83.0-rc1.md`.
- **Confidence**: H = verified in both code AND release notes; M = one source
  only; L = inferred.

## Verdict vocabulary

- `no-op` — no migration needed
- `rename` — same section, key renamed
- `move` — different section, same key
- `rename+move` — both
- `remove` — key gone, no replacement
- `value-map` — specific old value(s) translate to specific new value(s)
- `value-set-narrow` — accepted values reduced (may silently invalidate user
  values)
- `value-set-expand` — accepted values added (no migration needed)
- `default-change` — preserve explicit values; default-only users get new
  default
- `type-change` — registered type changed (e.g. `Add_bool` → `Add_string`)
- `split` — one key becomes two
- `new` — key did not exist before
- `new-section` — entire section is new
- `deprecated-alias` — old name still accepted at runtime via
  `SetDeprecatedWithAlternateValue` or `DeprecatedButAllowed`

---

# Graphics and display sections

## `[sdl]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `fullscreen` | bool, false | bool, false | bool, false | bool, false | no-op |
| `display` | int, 0 | int, 0 | int, 0 | int, 0 | no-op |
| `fullresolution` | str, `desktop` (free-form) | str, `desktop` | str, `desktop` | **Deprecated** | rename+value-map → `fullscreen_mode` |
| `fullscreen_mode` | absent | absent | absent | **new** (str, `standard`; values `standard`, `forced-borderless` (Windows only); `desktop` deprecated-alias → `standard`) | new |
| `windowresolution` | str, `default` | str, `default` | str, `default` | **DeprecatedButAllowed** | rename → `window_size` |
| `window_size` | absent | absent | absent | **new** (str, `default`) | new |
| `window_position` | str, `auto` | str, `auto` | str, `auto` | str, `auto` | no-op |
| `window_decorations` | bool, true | bool, true | bool, true | bool, true | no-op |
| `transparency` | int, 0 (0–90) | int, 0 | int, 0 | **Deprecated** | rename → `window_transparency` |
| `window_transparency` | absent | absent | absent | **new** (int, 0; 0–90) | new |
| `viewport_resolution` | path, `fit` | **Deprecated** → `[render] viewport` | Deprecated | Deprecated | rename+move → `[render] viewport` |
| `max_resolution` | **Deprecated** → `viewport_resolution` | **Deprecated** → `[render] viewport` | Deprecated | Deprecated | rename+move → `[render] viewport` |
| `host_rate` | str, `auto` (`auto`/`sdi`/`vrr`/`<Hz>`) | str, `auto` | str, `auto` | **absent** | remove (replacement is `[dosbox] dos_rate = host`, semantically lossy) |
| `vsync` | **bool**, false | **str**, `auto` (`{auto, on, adaptive, off, yield}`) | str, `auto` (same set) | str, `off` (`{off, on, fullscreen-only}`) | type-change at 0.81.0; value-set-narrow + default-change at main |
| `vsync_skip` | int, 7000 (0–14000) | int, 0 | int, 0 | **absent** | remove |
| `presentation_mode` | str, `auto` (`{auto, cfr, vfr}`) | str, `auto` (same) | str, `auto` (same) | str, `auto` (`{auto, dos-rate, host-rate}`) | value-map: `cfr → host-rate`, `vfr → dos-rate`, `auto → auto` |
| `output` | str, `opengl` (`{surface, texture, texturenb, texturepp, opengl, openglnb, openglpp}`) | values narrowed: `{texture, texturenb, opengl, openglnb}`; old values deprecated-alias | values `{opengl, texture, texturenb}`; `openglnb` deprecated-alias → `opengl` | same as 0.82.2 | value-set-narrow; aliases handled at runtime |
| `texture_renderer` | str, `auto` | str, `auto` | str, `auto` | str, `auto` | no-op |
| `capture_mouse` | Deprecated → `[mouse]` | Deprecated → `[mouse] mouse_capture` | Deprecated | Deprecated | rename+move → `[mouse] mouse_capture` |
| `sensitivity` | Deprecated | Deprecated → `[mouse] mouse_sensitivity` | Deprecated | Deprecated | rename+move → `[mouse] mouse_sensitivity` |
| `raw_mouse_input` | Deprecated | Deprecated → `[mouse] mouse_raw_input` | Deprecated | Deprecated | rename+move → `[mouse] mouse_raw_input` |
| `waitonerror` | bool, true | bool, true | bool, true | **Deprecated** | remove |
| `priority` | MultiVal, `auto auto` | present | present | **Deprecated** | remove |
| `mute_when_inactive` | bool, false | bool, false | bool, false | bool, false | no-op |
| `pause_when_inactive` | bool, false | bool, false | bool, false | bool, false | no-op |
| `keyboard_capture` | absent | absent | absent | **new** (bool, false) | new |
| `mapperfile` | path | path | path | path | no-op |
| `screensaver` | str, `auto` (`{auto, allow, block}`) | same | same | same | no-op |

### Migration rules

- `fullresolution = desktop` → `fullscreen_mode = standard` (and remove `fullresolution`)
- `fullresolution = original` → `fullscreen_mode = standard` + WARNING
- `fullresolution = <WxH>` → `fullscreen_mode = standard` + WARNING
- `windowresolution = X` → `window_size = X` (value pass-through)
- `transparency = N` → `window_transparency = N`
- `viewport_resolution = X` → cross-section: `[render] viewport = X`
- `max_resolution = X` → cross-section: `[render] viewport = X` (if both set, prefer `viewport_resolution`)
- `vsync` from v0.80.0 (bool): `true → on`, `false → off`
- `vsync` from v0.81.2/v0.82.2: `auto → off` (with WARNING), `adaptive → on` (with WARNING), `yield → off` (with WARNING), `on → on`, `off → off`
- `vsync_skip = N` → drop + WARNING
- `presentation_mode = cfr` → `host-rate`; `vfr` → `dos-rate`; `auto → auto`
- `output` deprecated values are runtime-mapped, but migrator should explicitly rewrite: `surface → opengl`, `openglpp → opengl`, `openglnb → opengl`, `texturepp → texture`
- `waitonerror` → drop + WARNING
- `priority` → drop + WARNING
- `capture_mouse`/`sensitivity`/`raw_mouse_input` → see `[mouse]` section
- `host_rate` → cross-section to `[dosbox] dos_rate` (with WARNING for non-`auto` values)

### Unmigrateable / warning cases

- `fullresolution = <WxH>` — fixed-resolution fullscreen removed.
  > `# WARNING: '[sdl] fullresolution = <WxH>' is no longer supported; migrated to 'fullscreen_mode = standard'. Custom fullscreen resolutions are not available in current DOSBox Staging.`
- `fullresolution = original` — removed.
  > `# WARNING: '[sdl] fullresolution = original' has been removed; migrated to 'fullscreen_mode = standard'. Consider 'forced-borderless' on Windows for exclusive-fullscreen-like behaviour.`
- `host_rate = <Hz>` or `host_rate = sdi`/`vrr` — entire setting removed.
  > `# WARNING: '[sdl] host_rate' has been removed. VRR is now auto-handled. Use '[dosbox] dos_rate = host' to drive frame presentation from the host display, or '[dosbox] dos_rate = <Hz>' for a custom DOS-side rate.`
- `vsync_skip = N` — removed.
  > `# WARNING: '[sdl] vsync_skip' has been removed. For similar behaviour set '[sdl] presentation_mode = host-rate' and '[sdl] vsync = off'.`
- `vsync = auto` — value removed.
  > `# WARNING: '[sdl] vsync = auto' has been removed; migrated to 'off' (the new default). Set to 'on' or 'fullscreen-only' if you need vsync.`
- `vsync = adaptive` — value removed.
  > `# WARNING: '[sdl] vsync = adaptive' has been removed; migrated to 'on'.`
- `vsync = yield` — value removed.
  > `# WARNING: '[sdl] vsync = yield' has been removed; migrated to 'off'.`
- `waitonerror = false` — setting removed.
  > `# WARNING: '[sdl] waitonerror' has been removed; the setting no longer has any effect.`
- `priority = <anything except 'auto auto'>` — setting removed.
  > `# WARNING: '[sdl] priority' has been removed; thread priority is now managed by the host OS.`

### Code references (main)

- `src/gui/sdl_gui.cpp:2652-2920` — `init_sdl_config_settings`
- `src/gui/sdl_gui.cpp:2923` — section registration
- `src/gui/titlebar.cpp:721` — `TITLEBAR_AddConfigSettings` (registers `window_titlebar` — in `[sdl]` per current code)

---

## `[render]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `frameskip` | **Deprecated** | Deprecated | Deprecated | Deprecated | remove (already deprecated) |
| `aspect` | **bool**, true | **str**, `auto` (`{auto, on, square-pixels, off, stretch}`) | str, `auto` | str, `auto` | type-change at 0.81.0: bool→string |
| `monochrome_palette` | str, `white` (`{white, paperwhite, green, amber}`) | str, **`amber`** (set unchanged) | str, `amber` | str, `amber` | default-change at 0.81.0 |
| `cga_colors` | str, `default` | same | same | same | no-op |
| `scaler` | **Deprecated** | Deprecated | Deprecated | Deprecated | remove |
| `glshader` | path, `default` (C_OPENGL only) | str, **`crt-auto`** | str, `crt-auto` | **DeprecatedButAllowed** | rename → `shader` (also default changed `default → crt-auto` in 0.81.0) |
| `shader` | absent | absent | absent | **new** (str, `crt-auto`) | new |
| `integer_scaling` | absent | **new** (str, `auto`; `{auto, vertical, horizontal, off}`) | present | present | new in 0.81.0 |
| `viewport` | absent | **new** (path, `fit`) | present | present | new in 0.81.0 (incoming from `[sdl] viewport_resolution`/`max_resolution`) |
| `color_space` | absent | absent | absent | **new** | new |
| `image_adjustments` | absent | absent | absent | **new** (bool, true) | new |
| `crt_color_profile` | absent | absent | absent | **new** (str, `auto`) | new |
| `brightness` | absent | absent | absent | **new** (int, 45; 0–100) | new (DO NOT migrate from `[composite] brightness`) |
| `contrast` | absent | absent | absent | **new** (int, 65; 0–100) | new (DO NOT migrate from `[composite] contrast`) |
| `gamma` | absent | absent | absent | **new** (int, 0; −50..50) | new |
| `digital_contrast` | absent | absent | absent | **new** | new |
| `black_level` | absent | absent | absent | **new** | new |
| `saturation` | absent | absent | absent | **new** (int, 0; −50..50) | new (DO NOT migrate from `[composite] saturation`) |
| `color_temperature` | absent | absent | absent | **new** | new |
| `color_temperature_luma_preserve` | absent | absent | absent | **new** | new |
| `red_gain`/`green_gain`/`blue_gain` | absent | absent | absent | **new** | new |
| `deinterlacing` | absent | absent | absent | **new** | new |
| `dedithering` | absent | absent | absent | **new** | new |

### Migration rules

- `frameskip` → drop silently
- `aspect = true` (bool from v0.80.0) → `aspect = on`
- `aspect = false` → `aspect = off`
- `scaler` → drop silently (deprecated everywhere)
- `glshader = X` → `shader = X` (pass-through) — but:
  - `glshader = default` → `shader = crt-auto` + WARNING
  - `glshader = none` → `shader = sharp` + WARNING
  - custom shader paths → pass-through + WARNING about possible non-existence
- `viewport` receives `[sdl] viewport_resolution`/`max_resolution` values pass-through

### Unmigrateable / warning cases

- `glshader = default` (v0.80.0 default value)
  > `# WARNING: '[render] glshader = default' was remapped to 'shader = crt-auto'. The old "default" was a non-CRT pass-through; use 'shader = sharp' if you prefer the legacy look.`
- `glshader = none`
  > `# WARNING: '[render] glshader = none' was remapped to 'shader = sharp'. Pre-shader pass-through is no longer available.`
- Custom shader paths
  > `# WARNING: Verify that '<shader_path>' still exists; the bundled shader set was overhauled in 0.81.0.`

### Code references (main)

- `src/gui/render/render.cpp:1448-1574` — `init_color_space_setting`
- `src/gui/render/render.cpp:1576-2068` — `init_render_settings`
- `src/gui/render/render.cpp:2935` — `RENDER_AddConfigSection`

---

## `[composite]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `composite` | str, `auto` (`{auto, on, off}`) | same | same | same | no-op |
| `era` | str, `auto` (`{auto, old, new}`) | same | same | same | no-op |
| `hue` | int, 0 (−360..360) | same | same | same | no-op |
| `saturation` | int, 100 (0..360) | same | same | **absent** | remove + WARNING (DO NOT migrate to `[render] saturation` — different scale/meaning) |
| `contrast` | int, 100 (0..360) | same | same | **absent** | remove + WARNING (DO NOT migrate) |
| `brightness` | int, 0 (−100..100) | same | same | **absent** | remove + WARNING (DO NOT migrate) |
| `convergence` | int, 0 (−50..50) | same | same | same | no-op |

### Migration rules

- `composite`, `era`, `hue`, `convergence` → pass-through
- `saturation`, `contrast`, `brightness` → DO NOT migrate values; emit WARNING

### Unmigrateable / warning cases

- `[composite] saturation = N`
  > `# WARNING: '[composite] saturation' has been removed. The new '[render] saturation' has a different range (−50..50, was 0..360) and meaning (digital RGB saturation vs CGA composite). Tune '[render] saturation' manually if needed.`
- `[composite] contrast = N`
  > `# WARNING: '[composite] contrast' has been removed. The new '[render] contrast' has a different range (0..100, was 0..360) and meaning. Tune '[render] contrast' manually.`
- `[composite] brightness = N`
  > `# WARNING: '[composite] brightness' has been removed. The new '[render] brightness' has a different range (0..100, was −100..100) and meaning. Tune '[render] brightness' manually.`

### Code references (main)

- `src/hardware/video/vga_other.cpp:1550-1609` — `init_composite_settings`
- `src/hardware/video/vga_other.cpp:1613` — section registration

---

## `[voodoo]`

Section did not exist at v0.80.0.

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `voodoo` | absent | bool, true | bool, true | bool, true | new in 0.81.0 |
| `voodoo_memsize` | absent | str, `4` (`{4, 12}`) | same | same | new in 0.81.0 |
| `voodoo_multithreading` | absent | bool, true | **Deprecated** | **Deprecated** | rename+value-map → `voodoo_threads` in 0.82.0 |
| `voodoo_threads` | absent | absent | **new** (str, `auto`; `{auto, 1..128}`) | present | new in 0.82.0 |
| `voodoo_bilinear_filtering` | absent | bool, **false** | bool, **true** | bool, true | new in 0.81.0; default flipped in 0.83.0-rc1 |

### Migration rules

- `voodoo_multithreading = true` → `voodoo_threads = auto`
- `voodoo_multithreading = false` → `voodoo_threads = 1`
- `voodoo_bilinear_filtering` → pass-through (default-change handled by absence)

### Unmigrateable / warning cases

None — clean mapping.

### Code references (main)

- `src/hardware/video/voodoo.cpp:7913-7964` — `init_voodoo_config_settings`
- `src/hardware/video/voodoo.cpp:7968` — section registration

---

## `[capture]`

Section did not exist at v0.80.0; `captures` lived in `[dosbox]`.

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `captures` (in `[dosbox]`) | path, `capture` | Deprecated → `[capture] capture_dir` | Deprecated | Deprecated | rename+move → `[capture] capture_dir` |
| `capture_dir` | absent | path, `capture` | path, `capture` | path, `capture` | new in 0.81.0 |
| `default_image_capture_formats` | absent | str, `upscaled` (`{upscaled, rendered, raw}`) | same | same | new in 0.81.0 |

### Migration rules

- `[dosbox] captures = X` → `[capture] capture_dir = X` (cross-section move + rename)

### Unmigrateable / warning cases

None — direct path move.

### Code references (main)

- `src/capture/capture.cpp:655-694` — `init_capture_config_settings`
- `src/capture/capture.cpp:699` — section registration

---

## `[reelmagic]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `reelmagic` | str, `off` (`{off, on, cardonly}`) | same | same | same | no-op |
| `reelmagic_key` | str, `auto` (`{auto, common, thehorde, <hex>}`) | same | same | same | no-op |
| `reelmagic_fcode` | int, 0 (0..7) | same | same | same | no-op |

### Migration rules

None.

### Code references (main)

- `src/hardware/video/reelmagic/driver.cpp:1438-1469`
- `src/hardware/video/reelmagic/driver.cpp:1476`

---

# CPU and system sections

## `[dosbox]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `language` | str, `""` | str, `""` | str, `""` | str, **`auto`** | default-change at main |
| `machine` | str, `svga_s3` (includes `vgaonly`) | `vgaonly` Deprecated → `svga_paradise` | same | same | value-map at 0.81.0 |
| `captures` | path, `capture` | **Deprecated** → `[capture] capture_dir` | Deprecated | Deprecated | rename+move (see `[capture]`) |
| `memsize` | int, 16 (WhenIdle) | int, 16 (WhenIdle) | int, 16 (**OnlyAtStart**) | int, 16 (OnlyAtStart) | changeability tightened at 0.82.0 |
| `mcb_fault_strategy` | str, **`deny`** | str, **`repair`** | str, `repair` | str, `repair` | default-change at 0.81.0 |
| `vmemsize` | str, `auto` | same | same | same | no-op |
| `vmem_delay` | absent | str, `off` | same | same | new in 0.81.x |
| `dos_rate` | str, `default` | same | same | same | no-op (help expanded) |
| `vesa_modes` | str, `compatible` (`{compatible, all, halfline}`) | same | same | same | no-op |
| `vga_8dot_font` | absent | bool, false | same | same | new in 0.81.x |
| `vga_render_per_scanline` | absent | bool, true | same | same | new in 0.81.x |
| `speed_mods` | bool, true | same | same | same | no-op |
| `autoexec_section` | str, `join` (`{join, overwrite}`) | same | same | same | no-op |
| `automount` | bool, true | same | same | same | no-op |
| `startup_verbosity` | str, `auto` | same | same | same | no-op |
| `allow_write_protected_files` | absent | bool, true | same | same | new in 0.81.0 |
| `shell_config_shortcuts` | absent | bool, true | same | same | new in 0.81.0 |
| `hard_disk_speed` | absent | absent | absent | str, `maximum` | new in 0.83.0-rc1 |
| `floppy_disk_speed` | absent | absent | absent | str, `maximum` | new in 0.83.0-rc1 |

### Migration rules

- `captures = X` → `[capture] capture_dir = X` (drop original line in `[dosbox]`)
- `machine = vgaonly` → `machine = svga_paradise`
- `mcb_fault_strategy = deny` → keep as-is (value still valid; only default changed)
- `language = ""` (empty) → optionally rewrite to `language = auto` (recommended; runtime still accepts empty)

### Unmigrateable / warning cases

None — all rewrites are safe.

### Code references (main)

- `src/dosbox.cpp:780-1050` — `add_dosbox_config_section`
- `src/dosbox.cpp:818` — `machine vgaonly → svga_paradise` alias
- `src/dosbox.cpp:858` — `captures` deprecation marker

---

## `[cpu]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `core` | str, `auto` (`{auto, dynamic, normal, simple}`) | same | same | same | no-op |
| `cputype` | str, `auto` (`{auto, 386, 386_slow, 486_slow, pentium_slow, 386_prefetch}`) | same | str, `auto` (`{auto, 386, 386_fast, 386_prefetch, 486, pentium, pentium_mmx}`); deprecated aliases `386_slow → 386`, `486_slow → 486`, `486_prefetch → 486`, `pentium_slow → pentium` | same | value-map at 0.82.0; semantic change for old plain `386` (was today's `386_fast`) |
| `cycles` | MultiValRemain, default `auto` (Always) | same | string-like MultiValRemain (**DeprecatedButAllowed**) | str (NOT multival), default `""` (DeprecatedButAllowed) | rename+split at 0.82.0; type-change at main |
| `cpu_cycles` | absent | absent | str, `3000` (50..2'000'000 or `max`) | same | new in 0.82.0 |
| `cpu_cycles_protected` | absent | absent | str, `60000` (`auto`, `<int>`, `max`) | same | new in 0.82.0 |
| `cpu_throttle` | absent | absent | bool, false | same | new in 0.82.0 |
| `cpu_idle` | absent | absent | absent | bool, true | new in 0.83.0-rc1 |
| `cycleup` | int, 10 (1..1'000'000) | same | same | same | no-op |
| `cycledown` | int, 20 (1..1'000'000) | same | same | same | no-op |

### Migration rules

- `cputype = 386_slow` → `cputype = 386`
- `cputype = 486_slow` → `cputype = 486`
- `cputype = pentium_slow` → `cputype = pentium`
- `cputype = 486_prefetch` → `cputype = 486` + WARNING (loses prefetch behaviour)
- `cputype = 386` (from v0.80.0/v0.81.2) → **`cputype = 386_fast`** (semantic remap — see landmine below)
- `cputype = 386_prefetch` → keep as-is
- `cycles = X` → see cycles deep-dive below

### Unmigrateable / warning cases

- **Silent semantic change**: `cputype = 386` in v0.80.0/v0.81.2 meant today's `386_fast`. On main, plain `386` is valid but is a different (slower) core. No deprecation alias fires.
  > `# WARNING: '[cpu] cputype = 386' had different semantics in pre-0.82 — it meant today's '386_fast'. Migrated to 'cputype = 386_fast'. Use 'cputype = 386' (without _fast) only if you intentionally want the slower modern '386' core.`
- `cputype = 486_prefetch`
  > `# WARNING: '[cpu] cputype = 486_prefetch' was migrated to '486'. The new code does not have a 486-with-prefetch core. Use 'cputype = 386_prefetch' if you need prefetch behaviour (it is a 386 core, but it has prefetch).`

### Code references (main)

- `src/cpu/cpu.cpp:3820-4020` — `init_cpu_config_settings`
- `src/cpu/cpu.cpp:3893-3896` — `cputype` deprecated aliases
- `src/cpu/cpu.cpp:3900` — `cycles` deprecation marker
- `src/cpu/cpu.cpp:3910, 3950, 3971` — new `cpu_cycles*` declarations
- `src/cpu/cpu.h:22-27` — `CpuCyclesMin=50`, `CpuCyclesMax=2'000'000`, `CpuCyclesRealModeDefault=3000`, `CpuCyclesProtectedModeDefault=60000`

---

# `cycles` parser deep-dive

## Parser location and porting strategy

**File**: `src/cpu/cpu.cpp` on main.

**Legacy parser**: `ConfigureCyclesLegacy(SectionProp* secprop)` at lines
**3421–3548**. Self-contained except for:

- `CommandLine` (`shell/command_line.h`)
- `trim()` (utility)
- Four runtime globals: `CPU_CycleMax`, `CPU_CyclePercUsed`,
  `CPU_CycleAutoAdjust`, `CPU_CycleLimit`
- `old_cycle_max` (used in `auto`-with-explicit-N branch)
- `auto_determine_mode.auto_cycles`
- `set_if_in_range` lambda (lines 3424–3438) — the bounds checker

**Modern parser**: `ConfigureCyclesModern(SectionProp* secprop)` at lines 3252–3371.

**Porting plan for the migrator**:

1. Copy `ConfigureCyclesLegacy` (lines 3421–3548) and the `set_if_in_range`
   lambda verbatim.
2. Replace the four runtime globals with local int variables.
3. Replace the `secprop->GetString("cycles")` call (line 3460) with the raw
   value of the `cycles =` config line read by the migrator.
4. Skip `maybe_display_switch_to_dynamic_core_warning` (line 3464) and
   `display_dynamic_core_warning_once` (3469) — these are runtime LOG noise.
5. After parsing, emit the new keys from the resulting locals using the
   mapping table below.

State populated by the legacy parser:

- `CPU_CycleAutoAdjust` — `true` iff `max` mode (set at line 3478 in the
  `if (type == "max")` branch; the `auto` branch sets it `false` at line 3540)
- `CPU_CycleMax` — fixed cycles target, or `0` in `max` mode
- `CPU_CyclePercUsed` — percent cap (used in `max` mode in modern semantics;
  legacy also used in `auto`)
- `CPU_CycleLimit` — hard upper cap, `-1` if unset
- `auto_determine_mode.auto_cycles` — `true` iff top-level type was `auto`
- `old_cycle_max` — explicit N in `auto N` form

## Accepted input forms

Top-level types (first whitespace-separated token of `cycles`):

1. **`<N>`** (plain integer) — `cycles = 12000`
2. **`fixed <N>`** — `cycles = fixed 12000`
3. **`max`** — `cycles = max`
4. **`max <N>%`** — `cycles = max 90%`
5. **`max limit <N>`** — `cycles = max limit 50000`
6. **`max <N>% limit <M>`** — combo
7. **`auto`** — `cycles = auto`
8. **`auto <N>%`**
9. **`auto limit <N>`**
10. **`auto <N>% limit <M>`**
11. **`auto <N>`** — `auto` with explicit real-mode fixed N
12. **`auto <N> max`** — explicit `max` keyword in tail (re-asserts protected-mode max)
13. **`auto <N> max <P>% limit <M>`** — fully spelled-out

The parser is **lenient**: tokens are read in any order within a type. `auto
max 90%`, `auto 90% max`, and `auto 90% limit 5000 max` are all silently
accepted.

## Defaults

In modern mode (when `cycles = ""`): real-mode 3000, protected-mode 60000,
throttle off. See `src/cpu/cpu.h:22-27`.

In legacy mode (when `cycles = auto` or explicit): real-mode 3000 default;
protected-mode dynamically becomes `max`.

## Mapping table

The new system has three keys:

- `cpu_cycles` — real-mode (or both, if `cpu_cycles_protected = auto`). Values: `<int 50..2'000'000>` or `max`.
- `cpu_cycles_protected` — protected-mode. Values: `auto, <int>, max`. **Constraint**: if `cpu_cycles = max`, fixed integer in `cpu_cycles_protected` is rejected and forced to `auto`. See `src/cpu/cpu.cpp:3320–3340`.
- `cpu_throttle` — bool. Modern mode only. No legacy equivalent; must be emitted as `off` in all migrations (default).

| Legacy `cycles =` value | `cpu_cycles` | `cpu_cycles_protected` | `cpu_throttle` | Notes |
|---|---|---|---|---|
| empty / not present | (omit) | (omit) | off | No migration needed |
| `<N>` (plain int) | `<N>` | `<N>` | off | Old plain-int = fixed for both modes. Clamp N to 50..2'000'000 first. |
| `fixed <N>` | `<N>` | `<N>` | off | Same as plain int. |
| `max` | `max` | `auto` | off | Equivalent. |
| `max <N>%` | `max` | `auto` | off | **LOSSY**: percent cap not representable. WARNING. |
| `max limit <N>` | `max` | `auto` | off | **LOSSY**: limit not representable. WARNING. |
| `max <N>% limit <M>` | `max` | `auto` | off | **LOSSY** (both). WARNING. |
| `auto` | `3000` | `max` | off | Real 3000, protected max. |
| `auto <N>%` | `3000` | `max` | off | **LOSSY**: percent not representable. WARNING. |
| `auto limit <N>` | `3000` | `max` | off | **LOSSY**: limit not representable. WARNING. |
| `auto <N>% limit <M>` | `3000` | `max` | off | **LOSSY**. WARNING. |
| `auto <N>` (explicit real fixed) | `<N>` | `max` | off | Explicit real-mode N; protected stays max. |
| `auto <N> limit <M>` | `<N>` | `max` | off | **LOSSY**: limit. WARNING. |
| `auto <N> <P>%` | `<N>` | `max` | off | **LOSSY**: percent. WARNING. |
| `auto <N> <P>% limit <M>` | `<N>` | `max` | off | **LOSSY** both. WARNING. |
| `auto <N> max` | `<N>` | `max` | off | Equivalent to `auto <N>`. |
| `auto <N> max <P>%` | `<N>` | `max` | off | **LOSSY**: percent. WARNING. |
| `auto <N> max limit <M>` | `<N>` | `max` | off | **LOSSY**: limit. WARNING. |
| `auto <N> max <P>% limit <M>` | `<N>` | `max` | off | **LOSSY** both. WARNING. |
| unknown token (`banana`) | unchanged | unchanged | unchanged | Keep original line + WARNING. |

## Edge cases / warnings

1. **Invalid input** (e.g. `cycles = banana`): legacy parser silently treats
   as `0` then clamps to default. Migrator should keep line as-is + WARNING.
   > `# WARNING: 'cycles = <value>' is not recognised. Kept as-is for the legacy parser to handle (defaults to 3000 cycles).`

2. **`<N>%` cap** (`max N%`): no equivalent. Modern `max` is 100% of one core.
   > `# WARNING: 'cycles = max <N>%' percent cap is not supported. Migrated to 'cpu_cycles = max'. Set a fixed value if you want a lower CPU load.`

3. **`limit <N>`**: no equivalent.
   > `# WARNING: 'cycles = ... limit <N>' upper-bound cap is not supported. The 'limit' modifier was dropped during migration.`

4. **`auto 0` or `cycles = 0`**: parser silently treats as default.
   > `# WARNING: 'cycles = 0' was treated as default by the legacy parser. Migrated to 'cpu_cycles = 3000'.`

5. **Out-of-range N** (N < 50 or N > 2'000'000):
   > `# WARNING: 'cycles = <N>' is outside the modern valid range (50..2'000'000). Migrated to 'cpu_cycles = <clamped>'.`

6. **`max <N>` with N≥1 and no `%`**: legacy parser silently ignores the
   trailing token. Treat as `cpu_cycles = max`. No warning needed.

7. **Whitespace-only `cycles = ` (spaces only)**: `trim()` strips to empty;
   modern mode. Drop the line.

---

# `[autoexec]` command syntax

The `[autoexec]` section body is DOS commands. Same parsing context applies to
`.bat` files reachable via MOUNT.

## `config -set "..."`

**No syntax change** between v0.80.0 and main. Forms accepted in both:

- `config -set "section property=value"`
- `config -set "section property value"`
- `config -set "property=value"` (omitted section)
- `config -set "property value"`
- `config -set property value` (after the `config` command)

No migration needed for `config -set` lines themselves. But: the property name
inside the quotes is subject to the same rename rules as INI lines.

## Bare setting commands (shell config shortcuts)

Bare commands like `sbtype sb16` (= `config -set sbtype sb16`) or `sbtype`
(= `config -get sbtype`).

- Shortcuts existed long before 0.81.0.
- 0.81.0 made them toggleable via `[dosbox] shell_config_shortcuts`
  (default `on`). Registration at `src/dosbox.cpp:1026`.
- Syntax unchanged across all four refs.

Migration impact: bare-setting commands continue to work. But: the property
name and value tokens are subject to the same rename/value-map rules.

## `MOUNT`

| Flag | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `-u` | yes | yes | yes | yes | no-op |
| `-t TYPE` | `{dir, overlay, floppy, cdrom}` | same | adds `iso` | adds `hdd` | value-set-expand |
| `-fs TYPE` | IMGMOUNT only | IMGMOUNT only | IMGMOUNT only | yes (`fat`, `iso`, `none`) | added to MOUNT in 0.83.0-rc1 |
| `-freesize` | yes | yes | yes | yes | no-op |
| `-size B,S,H,C` | partial | yes | yes | yes | no-op-ish |
| `-chs C,H,S` | no | no | no | yes | added in 0.83.0-rc1 |
| `-ro` | no | no | yes | yes | added in 0.82.0 |
| `-ide` | no | no | no | yes | added in 0.83.0-rc1 |
| `-label` | yes | yes | yes | yes | no-op |
| `-usecd N` | yes | yes | **removed** | removed | **remove in 0.82.0** |
| `-listcd`, `-cd` | yes | yes | removed | removed | **remove in 0.82.0** |
| `-noioctl`, `-ioctl*` | yes (warn) | yes | yes (warn) | needs implementer review | deprecation |
| `-pr` | yes | yes | yes | yes | no-op |
| `-z LETTER` | yes | yes | yes | yes | no-op |

### Migration rules (MOUNT)

- Strip `-usecd N` (auto-detected now).
- Strip `-listcd` and `-cd`.
- Strip `-noioctl` and `-ioctl*` (already deprecated).
- All other flags pass through.
- Emit comment:
  > `# NOTE: -usecd is no longer needed; CD-ROM is auto-detected.`

## `IMGMOUNT`

- v0.80.0–v0.82.2: standalone command.
- **main**: `IMGMOUNT` is an alias for `MOUNT`. Continues to work unchanged.

Migration: no rewrite needed. Migrator may optionally rewrite `IMGMOUNT` →
`MOUNT` for tidiness (not required).

## `MIXER`

Channel name aliases (still accepted on main as of 0.83.0-rc1):

- `FM` → `OPL` (works with warning)
- `SPKR` → `PCSPEAKER` (works with warning)

Aliases were "restored" in 0.83.0-rc1 (PR #4400) after a brief removal.

`CDDA → CDAUDIO`: the rename predates v0.80.0; not in scope.

Migration: optional polish rewrite `FM` → `OPL`, `SPKR` → `PCSPEAKER`. Not
required.

Other MIXER syntax (`/LISTMIDI`, volume formats, channel toggles) is unchanged
at the user-facing level.

---

# Audio sections

## `[mixer]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `nosound` | bool, false, Always | bool, false, Always | bool, false, **OnlyAtStart** | bool, false, OnlyAtStart | changeability tightened |
| `rate` | int, 48000, set `{8000,11025,16000,22050,32000,44100,48000}` | same | int, 48000, **range 8000–96000** | same | value-set-expand (set→range) |
| `blocksize` | int, 1024/512 (Win/non-Win), set `{128..8192}` | same | int, 1024/512, **range 64–8192** | same | value-set-expand |
| `prebuffer` | int, 25/20, max 100 | same | same | same | no-op |
| `negotiate` | bool, false/true (Win/non-Win) | same | same | same | no-op |
| `compressor` | bool, true | same | same | same | no-op |
| `crossfeed` | str, `off`; set `{off, on, <0–100>}` | str, `off`; set `{off, on, light, normal, strong}` | same | same | value-set-narrow at 0.81.0 (numeric strength removed) |
| `reverb` | str, `off`; set `{off, on, tiny, small, medium, large, huge}` | same | same | same | no-op |
| `chorus` | str, `off`; set `{off, on, light, normal, strong}` | same | same | same | no-op |
| `denoiser` | absent | absent | absent | bool, true | new in 0.83.0-rc1 |

### Migration rules

- `crossfeed = <N>` (v0.80.0 numeric): bucket-map: `0 → off`, `1–25 → light`,
  `26–50 → normal`, `51–100 → strong`. Confidence M (bucketing is editorial).

### Unmigrateable / warning cases

- `crossfeed = <N>` numeric (v0.80.0)
  > `# WARNING: numeric crossfeed strength was removed in 0.81.0; mapped to nearest preset. Check whether 'crossfeed = <preset>' gives the expected behaviour.`

### Code references (main)

- `src/audio/mixer.cpp:3083-3238` — registration
- `src/audio/channel_names.h:10-77` — MIXER channel name aliases

---

## `[midi]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `mididevice` | str, `auto`; `{auto, coremidi(macOS), coreaudio(macOS), win32(Win), oss(non-Win), alsa, fluidsynth, mt32, none}` | same | same | str, **`port`**; `{port, coreaudio(macOS), mt32, soundcanvas, fluidsynth, none}`; deprecated aliases: `{auto, alsa, coremidi, oss, win32} → port` | rename+value-set-narrow + new value `soundcanvas` |
| `midiconfig` | str, `""` | same | same | same | no-op |
| `mpu401` | str, `intelligent` (`{intelligent, uart, none}`) | same | same | same | no-op |
| `raw_midi_output` | absent | bool, false | same | same | new in 0.81.0 |

### Migration rules

`mididevice` value-map:

- `auto → port`
- `alsa → port`
- `coremidi → port`
- `oss → port` (OSS support removed entirely)
- `win32 → port`
- `coreaudio` → keep (macOS-only, still valid)
- `fluidsynth` → keep
- `mt32` → keep
- `none` → keep

### Unmigrateable / warning cases

None — all old values map cleanly. Runtime aliases also work via
`SetDeprecatedWithAlternateValue` at `src/midi/midi.cpp:842-846`.

### Code references (main)

- `src/midi/midi.cpp:793-923` — registration
- `src/midi/midi.cpp:605-606` — `MidiDevicePortPref = "port"`
- `src/midi/midi.cpp:842-846` — deprecated aliases

---

## `[fluidsynth]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `soundfont` | str, `default.sf2`; help marks volume-scaling-after-name as deprecated | str, `default.sf2`; **multi-value form supported** (`name [vol 1–800]`) | same | str, `default.sf2`; multi-value form **removed** (name/path only) | type-change; split |
| `soundfont_dir` | absent | absent | absent | str, `""` | new in 0.83.0-rc1 |
| `soundfont_volume` | absent | absent | absent | int, 100 (1–800) | new in 0.83.0-rc1 (split from `soundfont`) |
| `fsynth_chorus` | str, `auto` (5 custom values) | same | same | same | no-op |
| `fsynth_reverb` | str, `auto` (4 custom values) | same | same | same | no-op |
| `fsynth_filter` | str, `off` | same | same | same | no-op |

### Migration rules

- v0.81.2/v0.82.2 `soundfont = file.sf2 N` → split into `soundfont = file.sf2`
  + `soundfont_volume = N`
- v0.80.0 `soundfont = file.sf2 N` → same split (the volume suffix was already
  deprecated in v0.80.0 help text)

### Unmigrateable / warning cases

None — split is clean.

### Code references (main)

- `src/midi/fluidsynth.cpp:1203-1299` — registration
- `src/midi/fluidsynth.cpp:1223` — `soundfont_dir`
- `src/midi/fluidsynth.cpp:1233` — `soundfont_volume`

---

## `[mt32]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `model` | str, `auto`; set covers cm32l/mt32 variants | adds `cm32ln_100`, `mt32_207`, `mt32_206`, `mt32_203` | same as 0.81.2 | same | value-set-expand at 0.81.0 |
| `romdir` | str, `""` | same | same | same | no-op |
| `mt32_filter` | str, `off` | same | same | same | no-op |

### Migration rules

None — all existing values still valid.

### Unmigrateable / warning cases

None.

### Code references (main)

- `src/midi/mt32.cpp:341-406` — registration
- `src/midi/mt32.cpp:334-339` — alias namespace

---

## `[soundcanvas]`

New section — does not exist before main.

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `soundcanvas_model` | absent | absent | absent | str, `auto` (SC-55 variants) | new |
| `soundcanvas_rom_dir` | absent | absent | absent | str, `""` | new |
| `soundcanvas_filter` | absent | absent | absent | str, `on` | new |

### Migration rules

None. Migrator should not create this section (defaults apply automatically).

### Code references (main)

- `src/midi/soundcanvas.cpp:880-929` — registration

---

## `[sblaster]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `sbtype` | str, `sb16`; `{sb1, sb2, sbpro1, sbpro2, sb16, gb, none}` | same | adds `ess` | same | value-set-expand at 0.82.0 |
| `sbbase` | hex, 0x220 | same | same | same | no-op |
| `irq` | int, 7 | same set (reordered) | same | same | reorder only |
| `dma` | int, 1 | same set (reordered) | same | same | reorder only |
| `hdma` | int, 5 | same | same | same | no-op |
| `sbmixer` | bool, true | same | same | same | no-op |
| `sbwarmup` | int, 100 (0–100) | same | same | same | no-op |
| `oplrate` | int, **Deprecated** | same | same | same | no-op |
| `oplmode` | str, `auto`; `{auto, cms, opl2, dualopl2, opl3, opl3gold, none}` | same | adds `esfm`; `cms` **Deprecated** (split target) | same | value-set-expand + split |
| `opl_fadeout` | absent | str, `off` | same | same | new in 0.81.0 |
| `opl_remove_dc_bias` | absent | absent | bool, false | same | new in 0.82.0 |
| `oplemu` | str, **Deprecated** | same | same | same | no-op |
| `sb_filter` | str, `modern` | same | same | same | no-op |
| `sb_filter_always_on` | bool, false | same | same | same | no-op |
| `opl_filter` | str, `auto` | same | same | same | no-op |
| `cms` | absent | absent | str, `auto` (`{on, off, auto}`) | same | new in 0.82.0 |
| `cms_filter` | str, `on` | same | same | same | no-op |

### Migration rules

- `oplmode = cms` → split into `cms = on` + `oplmode = auto` (or drop
  `oplmode` line; default `auto` picks right OPL chip per `sbtype`). The
  runtime still accepts `oplmode = cms` with warning; explicit rewrite is
  preferred.
- `oplrate` and `oplemu` → drop (deprecated everywhere)

### Unmigrateable / warning cases

None — `oplmode = cms` still works with warning on main.

### Code references (main)

- `src/hardware/audio/soundblaster.cpp:3759-3863` — sblaster core
- `src/hardware/audio/opl.cpp:973-1064` — OPL+CMS settings
- `src/hardware/audio/soundblaster.cpp:3402-3429` — `oplmode = cms` runtime mapping

---

## `[gus]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `gus` | bool, false | same | same | same | no-op |
| `gusbase` | hex, 0x240; `{240, 220, 260, 280, 2a0, 2c0, 2e0, 300}` | same | hex, 0x240; **`{210, 220, 230, 240, 250, 260}`** | same | value-set-narrow at 0.82.0 (silent) |
| `gusirq` | int, 5; `{5, 3, 7, 9, 10, 11, 12}` | same set (reordered) | int, 5; **`{2, 3, 5, 7, 11, 12, 15}`** | same | value-set-narrow at 0.82.0 (silent) |
| `gusdma` | int, 3; `{3, 0, 1, 5, 6, 7}` | same set (reordered) | int, 3; **`{1, 3, 5, 6, 7}`** | same | value-set-narrow at 0.82.0 (silent) |
| `ultradir` | str, `C:\\ULTRASND` | same | same | same | no-op |
| `gus_filter` | str, `off` | same | str, **`on`** | same | default-change at 0.82.0 |

### Migration rules

- All keys unchanged. Value-set narrowings are silent — invalid values fall
  back to default at runtime. The migrator must catch and warn.

### Unmigrateable / warning cases

- `gusbase ∈ {280, 2a0, 2c0, 2e0, 300}`
  > `# WARNING: '[gus] gusbase' value-set narrowed in 0.82.0; check whether 'gusbase = <user-value>' is still valid (accepted: 210, 220, 230, 240, 250, 260).`
- `gusirq ∈ {9, 10}`
  > `# WARNING: '[gus] gusirq' value-set narrowed in 0.82.0; check whether 'gusirq = <user-value>' is still valid (accepted: 2, 3, 5, 7, 11, 12, 15).`
- `gusdma = 0`
  > `# WARNING: '[gus] gusdma = 0' was removed in 0.82.0 (accepted: 1, 3, 5, 6, 7).`

### Code references (main)

- `src/hardware/audio/gus.cpp:1535-1604` — registration

---

## `[innovation]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `sidmodel` | str, `none`; `{auto, 6581, 8580, none}` | same | same | **absent** | remove (collapse to bool) |
| `sidclock` | str, `default`; `{default, c64ntsc, c64pal, hardsid}` | same | same | **absent** | remove |
| `sidport` | hex, 0x280; `{240, 260, 280, 2a0, 2c0}` | same | same | **absent** (fixed to 280) | remove |
| `6581filter` | int, 50 (0–100) | same | same | **renamed** | rename → `innovation_sid_filter` |
| `8580filter` | int, 50 (0–100) | same | same | **absent** | remove |
| `innovation` | absent | absent | absent | bool, false | new in 0.83.0-rc1 |
| `innovation_sid_filter` | absent | absent | absent | int, 50 (0–100) | new in 0.83.0-rc1 (replaces `6581filter`) |
| `innovation_filter` | str, `off` | same | same | same | no-op |

### Migration rules

- `sidmodel = auto` → `innovation = on`
- `sidmodel = 6581` → `innovation = on`
- `sidmodel = 8580` → `innovation = on` + WARNING (8580 no longer emulated)
- `sidmodel = none` → `innovation = off` (or drop)
- `6581filter = N` → `innovation_sid_filter = N`
- `sidclock`, `sidport`, `8580filter` → drop (silently)

### Unmigrateable / warning cases

- `sidmodel = 8580`
  > `# WARNING: only the SID 6581 chip is now emulated; check whether '[innovation] innovation = on' (replacing 'sidmodel = 8580') gives the expected result.`
- `sidport != 280`
  > `# WARNING: '[innovation] sidport' was removed in 0.83.0; the SSI-2001 is now hard-wired to port 280h. Previous 'sidport = <value>' is ignored.`
- `sidclock != default`
  > `# WARNING: '[innovation] sidclock' was removed in 0.83.0; the original SSI-2001 frequency (0.895 MHz) is always used. Previous 'sidclock = <value>' is ignored.`
- `8580filter` present
  > `# WARNING: '[innovation] 8580filter' was removed in 0.83.0 (only the 6581 chip is emulated). Use 'innovation_sid_filter' for 6581 filter strength.`

### Code references (main)

- `src/hardware/audio/innovation.cpp:260-288` — registration

---

## `[speaker]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `pcspeaker` | str, **`discrete`**; `{discrete, impulse, none, off}` | same | str, **`impulse`** | same | default-change at 0.82.0 |
| `pcspeaker_filter` | str, `on` | same | same | same | no-op |
| `zero_offset` | str, **Deprecated** | same | same | same | no-op |
| `tandy` | str, `auto`; `{auto, on, off}` | adds `psg` | same | same | value-set-expand at 0.81.0 |
| `tandy_fadeout` | absent | str, `off` | same | same | new in 0.81.0 |
| `tandy_filter` | str, `on` | same | same | same | no-op |
| `tandy_dac_filter` | str, `on` | same | same | same | no-op |
| `lpt_dac` | str, `none`; `{none, disney, covox, ston1, off}` | same | same | same | no-op |
| `lpt_dac_filter` | str, `on` | same | same | same | no-op |
| `disney` | bool, **Deprecated**, false | same | same | same | deprecated alias for `lpt_dac = disney` |
| `ps1audio` | bool, false | same | same | same | no-op |
| `ps1audio_filter` | str, `on` | same | same | same | no-op |
| `ps1audio_dac_filter` | str, `on` | same | same | same | no-op |

### Migration rules

- `disney = true` → `lpt_dac = disney` (and drop `disney` line)
- `disney = false` → drop (no-op)
- `zero_offset` → drop (deprecated)

### Unmigrateable / warning cases

None.

### Code references (main)

- `src/hardware/audio/speaker.cpp:17-69` — composes section
- `src/hardware/audio/pcspeaker.cpp:62-87` — `pcspeaker`, `pcspeaker_filter`
- `src/hardware/audio/lpt_dac.cpp:151-177` — `lpt_dac`, `disney` deprecation
- `src/hardware/audio/ps1audio.cpp:576-595`
- `src/hardware/audio/tandy_sound.cpp:655-700`

---

## `[imfc]`

New section — does not exist at v0.80.0.

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `imfc` | absent | bool, false | same | same | new in 0.81.0 |
| `imfc_base` | absent | hex, 0x2A20 (`{2A20, 2A30}`) | same | same | new in 0.81.0 |
| `imfc_irq` | absent | int, 3 (`{2..7}`) | same | same | new in 0.81.0 |
| `imfc_filter` | absent | str, `on` | same | same | new in 0.81.0 |

### Migration rules

None. Migrator should not create this section for migrations from v0.80.0.

### Code references (main)

- `src/hardware/audio/imfc.cpp:13441-13486` — registration

---

# I/O and DOS-environment sections

## `[joystick]`

**Section is 100% stable across all four tags.** No migrations.

### Settings table

| Key | v0.80.0 → main |
|---|---|
| `joysticktype` | str, `auto`, `{auto, 2axis, 4axis, 4axis_2, fcs, ch, hidden, disabled}` |
| `timed` | bool, true |
| `autofire` | bool, false |
| `swap34` | bool, false |
| `buttonwrap` | bool, false |
| `circularinput` | bool, false |
| `deadzone` | int, 10 (0–100) |
| `use_joy_calibration_hotkeys` | bool, false |
| `joy_x_calibration` | str, `auto` |
| `joy_y_calibration` | str, `auto` |

### Code references (main)

- `src/hardware/input/joystick.cpp:622-715`

---

## `[serial]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `serial1` | MultiValRemain `" "`, types `{dummy, disabled, mouse, modem, nullmodem, direct}`, default `dummy` | same | same | same registration; **`direct` implementation removed** | no-op at registration; silently broken at runtime for `direct` |
| `serial2` | same shape, default `dummy` | same | same | same | as above |
| `serial3` | same shape, default `disabled` | same | same | same | as above |
| `serial4` | same shape, default `disabled` | same | same | same | as above |
| `phonebookfile` | path, `phonebook.txt`, only_at_start | same | same | same | no-op |

### Migration rules

- `serialN = direct ...` → `serialN = disabled` + WARNING (legacy passthrough
  feature removed; parser still accepts `direct` in the value-set but no port
  is constructed)
- Modem parameters with `baudrate:` (v0.80.0 help text) → verify parser still
  accepts; may need rewrite to `bps:`. **Confidence M**.

### Unmigrateable / warning cases

- `serialN = direct ...`
  > `# WARNING: '[serial] serialN = direct ...' is no longer supported. The legacy serial-port passthrough feature was removed in 0.83.0. Migrated to 'disabled'.`
- Modem `baudrate:` parameter
  > `# WARNING: 'baudrate:' parameter may have been renamed to 'bps:' — verify your modem config string.` (Confidence M.)

### Code references (main)

- `src/hardware/serialport/serialport.cpp:1389-1455` — registration
- `src/hardware/serialport/serialport.cpp:1393-1394` — value-set (note `direct` still listed)
- `src/hardware/serialport/serialport.cpp:1451` — `phonebookfile`

---

## `[mouse]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `mouse_capture` | str, **`onstart`**, `{onclick, onstart, seamless, nomouse}` | str, **`onclick`** (same set) | same | same | default-change at 0.81.0 |
| `mouse_middle_release` | bool, true | same | same | same | no-op |
| `mouse_multi_display_aware` | absent | bool, true | same | same | new in 0.81.0 |
| `mouse_sensitivity` | **MultiVal `","`** (xsens,ysens), only_at_start, default `100` | str, `100`, Always | same | str, `100`, Always | type-change at 0.81.0 (MultiVal → str) |
| `mouse_raw_input` | bool, true | same | same | same | no-op |
| `dos_mouse_driver` | bool, true, only_at_start | same | same | bool, true, **Deprecated** | rename → `builtin_dos_mouse_driver` (with type-change) |
| `builtin_dos_mouse_driver` | absent | absent | absent | str, `on`, `{off, on, no-tsr}` | new in 0.83.0-rc1 |
| `dos_mouse_immediate` | bool, false | same | same | bool, false, **Deprecated** | merged into `builtin_dos_mouse_driver_options` |
| `builtin_dos_mouse_driver_model` | absent | absent | absent | str, `2button`, `{2button, 3button, wheel}` | new |
| `builtin_dos_mouse_driver_move_threshold` | absent | absent | absent | str, `1`, Always | new |
| `builtin_dos_mouse_driver_options` | absent | absent | absent | str, `""`, Always | new |
| `ps2_mouse_model` | str, **`intellimouse`** (build-time `explorer`) | str, **`explorer`** (`{standard, intellimouse, explorer, none}`) | same | same | default-change at 0.81.0 |
| `com_mouse_model` | str, `wheel+msm`, 7-value set | same | same | same | no-op |
| `vmware_mouse` | absent | bool, true | same | same | new in 0.81.0 |
| `virtualbox_mouse` | absent | bool, true | same | same | new in 0.81.0 |

### Migration rules

- `dos_mouse_driver = true` → `builtin_dos_mouse_driver = on`
- `dos_mouse_driver = false` → `builtin_dos_mouse_driver = off`
- `dos_mouse_immediate = true` → append `immediate` to
  `builtin_dos_mouse_driver_options` (was empty → `"immediate"`)
- `dos_mouse_immediate = false` → drop
- `mouse_sensitivity = "100,200"` (v0.80.0 xsens,ysens MultiVal) → preserve as
  is; the comma form is documented. **Confidence M** (parser whitespace
  behaviour not tested).
- Incoming from `[sdl]`:
  - `capture_mouse → mouse_capture`
  - `sensitivity → mouse_sensitivity`
  - `raw_mouse_input → mouse_raw_input`

### Unmigrateable / warning cases

- `mouse_sensitivity = "X,Y"` MultiVal (v0.80.0) on main
  > `# NOTE: '[mouse] mouse_sensitivity' MultiVal form (X,Y) preserved as-is. If only X is honoured, set explicit single value.` (Confidence M.)

### Code references (main)

- `src/hardware/input/mouse_config.cpp:549-804` — registration
- `src/hardware/input/mouse_config.cpp:622-653` — `builtin_dos_mouse_driver`
- `src/hardware/input/mouse_config.cpp:655-657` — `dos_mouse_driver` deprecation
- `src/hardware/input/mouse_config.cpp:702-728` — `builtin_dos_mouse_driver_options`
- `src/hardware/input/mouse_config.cpp:730-732` — `dos_mouse_immediate` deprecation

---

## `[dos]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `xms` | bool, true | same | same | same | no-op |
| `ems` | str, `true` (`{true, emsboard, emm386, false}`) | same | same | str, **`on`** (`{on, emsboard, emm386, off}`) | value-map |
| `umb` | bool, true | same | same | same | no-op |
| `ver` | str, `5.0` | same | same | same | no-op |
| `country` | **int, 0** | str, `auto` | str, `auto` | str, `auto` | type-change at 0.81.0 (int → str) |
| `keyboardlayout` | str, `auto` | str, `auto` | str, `auto` | str, `""`, **Deprecated** | rename → `keyboard_layout` |
| `keyboard_layout` | absent | absent | absent | str, `auto`, OnlyAtStart | new |
| `expand_shell_variable` | **bool, false** | str, `auto` (`{auto, true, false}`) | same | str, `auto` (`{auto, on, off}`) | type-change + value-map |
| `shell_history_file` | absent | path, `shell_history.txt` | same | same | new in 0.81.0 (renamed from older `command_history_file` — no alias in code) |
| `setver_table_file` | absent | path, `""` | same | same | new in 0.81.0 |
| `pcjr_memory_config` | absent | str, `expanded` (`{expanded, standard}`) | same | same | new in 0.81.0 |
| `locale_period` | absent | str, **`modern`** (`{historic, modern}`) | same | str, **`native`** (`{historic, modern, native}`) | new in 0.81.0; default-change + value-set-expand at main |
| `file_locking` | absent | absent | **bool, true** | str, **`auto`** (`{auto, on, off}`) | new in 0.82.0; type-change + default-change at main |
| `stacks` | absent | absent | absent | str, `auto` | new in main |

### Migration rules

- `country = 0` (v0.80.0 int) → `country = auto`
- `country = <nonzero int>` (v0.80.0) → preserve as string. **Confidence M** (need to verify numeric forms still parse on main).
- `keyboardlayout = X` → `keyboard_layout = X`
- `expand_shell_variable = true` → `expand_shell_variable = on`
- `expand_shell_variable = false` → `expand_shell_variable = off`
- `ems = true` → `ems = on`; `ems = false` → `ems = off`; `emsboard`/`emm386` unchanged
- `file_locking = true` (v0.82.2 bool) → `file_locking = on`
- `file_locking = false` → `file_locking = off`
- `locale_period` default changed `modern → native` — preserve explicit value

Also: `command_history_file → shell_history_file` rename happened in v0.81.0
cycle with no deprecation alias in code. Old configs silently lose their
history. The migrator must rewrite even though the runtime won't warn.

### Unmigrateable / warning cases

- Explicit `file_locking = true` from v0.82.2
  > `# NOTE: '[dos] file_locking = on' preserves your previous behaviour. Consider 'auto' (the new default) which only enables file locking under Windows 3.1.`
- `country = <nonzero int>` (v0.80.0)
  > `# WARNING: '[dos] country' was an int in v0.80.0 and a string after. Preserved as-is; verify the numeric country code still parses, or replace with ISO country code or 'auto'.`

### Code references (main)

- `src/dos/dos.cpp:1927-2065` — `init_dos_config_settings`
- `src/dos/dos.cpp:1938-1939` — `ems` `on/off` values
- `src/dos/dos.cpp:2012-2024` — `keyboardlayout` deprecation + `keyboard_layout`
- `src/dos/dos.cpp:2028-2037` — `expand_shell_variable` `on/off`
- `src/dos/dos.cpp:1988-2001` — `locale_period` with `native`
- `src/dos/dos.cpp:2053-2064` — `file_locking` string form
- `src/dos/dos.cpp:1948-1968` — new `stacks`

---

## `[ipx]`

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `ipx` | bool, false | same | same | same | no-op |

### Code references (main)

- `src/hardware/network/ipx.cpp:1295`

---

## `[ethernet]`

Section existed at v0.80.0 (under `#if C_SLIRP`), so it is not new.

### Settings table

| Key | v0.80.0 | v0.81.2 | v0.82.2 | main | Verdict |
|---|---|---|---|---|---|
| `ne2000` | bool, **true** | bool, true | bool, true | bool, **false** | default-change |
| `nicbase` | hex, 0x300 | same | same | same | no-op |
| `nicirq` | int, 3 | same | same | same | no-op |
| `macaddr` | str, `AC:DE:48:88:99:AA` | same | same | same | no-op |
| `tcp_port_forwards` | str, `""` | same | same | same | no-op |
| `udp_port_forwards` | str, `""` | same | same | same | no-op |

### Migration rules

None at the key level. `ne2000` default flipped — preserve explicit values.

### Code references (main)

- `src/network/ethernet.cpp:37-117` — registration
- `src/network/ethernet.cpp:139-147` — section registration

---

## `[webserver]`

New section — does not exist before main.

### Settings table

| Key | main | Verdict |
|---|---|---|
| `webserver_enabled` | bool, false, OnlyAtStart | new-section |
| `webserver_bind_address` | str, `127.0.0.1`, OnlyAtStart | new-section |
| `webserver_port` | int, 8086 (1–65535), OnlyAtStart | new-section |

### Migration rules

None. Migrator should not create this section.

### Code references (main)

- `src/webserver/webserver.cpp:167-188`

---

## `[disknoise]`

New section — does not exist before main.

### Settings table

| Key | main | Verdict |
|---|---|---|
| `hard_disk_noise` | str, `off` (`{off, seek-only, on}`) | new-section |
| `floppy_disk_noise` | str, `off` (`{off, seek-only, on}`) | new-section |

### Migration rules

None. Migrator should not create this section.

### Code references (main)

- `src/audio/disk_noise.cpp:583-614`

---

## `[log]` and `[debug]`

Dynamically built from the `LOG_TYPES` enum (`logfile` plus one bool per
topic). Enum is identical across all four tags. `[debug]` is registered empty.

No migrations.

### Code references (main)

- `src/debugger/debugger_gui.cpp:319-333` — `[log]` build
- `src/misc/logging.h:14-25` — `LOG_TYPES` enum
- `src/debugger/debugger.cpp:3141` — `[debug]` section

---

# Migrator behaviour: comment attachment

When a setting moves from one section to another (cross-section move), the
descriptive comment block immediately preceding the setting moves with it.
Comments that don't describe the next setting stay put.

## Algorithm

Walk backward from the moved setting. Collect contiguous lines into a comment
block. Stop on the first of:

- A real (uncommented) setting line
- A blank line
- A **commented-out setting** — a comment line matching
  `^\s*[;#]\s*<identifier>\s*=`
- The section header `[name]` (top of section)

Lines added to the block:

- Plain comment lines (`#` or `;` followed by free text, not matching the
  `key = value` shape)

So `# this controls the rendering` joins the block;
`# old_key = old_value` breaks it.

## Buffered-streaming model

Maintain a pending-comment buffer while walking lines:

- Section header `[name]` → flush buffer as raw output, emit header
- Blank line → flush buffer as raw output, emit blank
- Commented-out setting → flush buffer as raw output, emit line raw
- Plain comment → push onto buffer
- Real setting → buffer + setting form an "attached unit"; rule dispatch acts
  on the unit as a whole

## Rule actions and the comment block

- **rename / value-map / no-move**: flush buffer as raw, emit rewritten
  setting. Comments stay put.
- **move (cross-section)**: pull the attached unit (buffer + setting) out of
  the source location entirely and queue it for insertion at the target
  section.
- **remove**: flush buffer as raw (the comments describe what is now gone —
  leave them for context), emit WARNING comment + commented-out original.
- **split** (one key → two): flush buffer as raw, emit rewritten primary line
  (e.g. `oplmode = auto`). The secondary new line (e.g. `cms = on`) is
  emitted bare without inheriting the comment.

## Insertion at destination

Queued units append to the end of their target section, before the next
section header or `[autoexec]`, or at EOF. Preserve source ordering between
units sharing a target.

If the target section does not exist in the file, create it before
`[autoexec]`. If there is no `[autoexec]`, append at EOF.

## Inline trailing comments

A trailing comment on a setting's own line (`setting = value  # note`) moves
with the setting line itself. No special handling needed.

## Top-of-section comments

A comment block between the `[section]` header and the first setting under it
follows the same rule. If the first setting gets moved, the comment block
goes with it. The section header itself never moves.

## Pipeline implication

Pure single-pass streaming is not viable, because moves need to land in
sections that may precede the source. Two passes:

1. **Parse pass** — classify lines into units, build:
   - Output structure: ordered list of (section, [units]) plus inter-section text
   - Move queue: list of (target_section, attached_unit)
2. **Emit pass** — serialise the structure, injecting queued units into their
   target sections.

Configs are small; this is memory-cheap and still preserves byte-for-byte for
untouched units.

---

# Cross-cutting summary

## Critical landmines (silent or near-silent changes)

1. **`[cpu] cputype = 386`** (v0.80.0/v0.81.2) — meant today's `386_fast`.
   On main, plain `386` is valid but is a different (slower) core. No
   deprecation alias fires. Migrator MUST rewrite to `386_fast`.

2. **`[cpu] cputype = 486_prefetch`** — alias maps to `486` but loses
   prefetch behaviour. WARNING required.

3. **`[serial] serialN = direct ...`** — value still in registered set on
   main but implementation removed. Parser silently accepts; no port is
   constructed. Migrator MUST rewrite to `disabled` + WARNING.

4. **`[gus] gusbase/gusirq/gusdma`** — value-set narrowings at 0.82.0. No
   deprecation aliases. Invalid values silently fall back to default at
   runtime. Migrator MUST catch and warn.

5. **`[dos] command_history_file → shell_history_file`** — rename in
   v0.81.0 with NO alias in code. Old configs silently lose history.
   Migrator MUST rewrite.

6. **`[composite] brightness/contrast/saturation`** — removed but `[render]`
   has same-named settings with DIFFERENT ranges/meanings. DO NOT migrate
   values. WARNING required.

7. **`[sdl] fullresolution = WxH`** — fixed-resolution fullscreen removed.
   Must downgrade to `fullscreen_mode = standard` + WARNING.

8. **`[innovation] sidmodel = 8580`** — 8580 SID chip no longer emulated;
   collapses to `innovation = on` + WARNING.

## Lossy migrations (require WARNING)

- `cycles = ... <N>%` or `... limit <N>` — percent caps and limits dropped
- `[sdl] host_rate = <Hz>/sdi/vrr` — entire setting removed
- `[sdl] vsync_skip = N` — removed
- `[sdl] vsync = auto/adaptive/yield` — values removed
- `[sdl] waitonerror` — removed
- `[sdl] priority` — removed
- `[mixer] crossfeed = <N>` numeric — preset bucket mapping (M confidence)

## Cross-section moves

- `[sdl] viewport_resolution` → `[render] viewport`
- `[sdl] max_resolution` → `[render] viewport`
- `[dosbox] captures` → `[capture] capture_dir`
- `[sdl] host_rate` → `[dosbox] dos_rate = host` (lossy)
- `[sdl] capture_mouse` → `[mouse] mouse_capture`
- `[sdl] sensitivity` → `[mouse] mouse_sensitivity`
- `[sdl] raw_mouse_input` → `[mouse] mouse_raw_input`

## Splits

- `oplmode = cms` → `cms = on` + `oplmode = auto`
- `soundfont = file.sf2 <N>` → `soundfont = file.sf2` + `soundfont_volume = N`
- `sidmodel = X` → `innovation = on/off` (with WARNING when X = 8580)

## New sections (migrator must not create)

- `[capture]` (0.81.0) — except as target of `captures` move
- `[voodoo]` (0.81.0)
- `[imfc]` (0.81.0)
- `[soundcanvas]` (0.83.0-rc1)
- `[webserver]` (0.83.0-rc1)
- `[disknoise]` (0.83.0-rc1)

## M-confidence items needing verification

1. `[dos] country` numeric→string preservation: do v0.81+ still parse numeric
   country codes?
2. `[serial]` modem params: was `baudrate:` renamed to `bps:`? Parser-tested?
3. `[mouse] mouse_sensitivity` MultiVal (`"X,Y"`) — still parses on main?
4. `[mixer] crossfeed` numeric → preset bucketing thresholds — editorial
   choice, not from code.

## Stable sections (no migrations)

- `[joystick]` — 100% stable across all tags
- `[ipx]`
- `[reelmagic]`
- `[log]`, `[debug]`

## Confidence summary

All rules above are H (verified in both code AND release notes) except where
explicitly marked M or L. The most consequential M items are the four listed
above. Recommend verification before locking the migrator spec.
