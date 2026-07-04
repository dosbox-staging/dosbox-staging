# Spike 6 — present-mode sweep (app-timed + VRR)

**The question:** which present mode gives smooth 70.086 Hz on a VRR display
when we pace submission ourselves and let the display flip on arrival — the
"app-timed + VRR" path.

(An earlier version of this spike drove `VK_EXT_present_timing` directly. On the
RTX 3060 / R595 driver that measured **60 Hz + intermittent ~16 ms present
blocking** under FIFO relative-time — absolute-time is unsupported there, so it
can't place frames at 70 Hz. See this file's git history. This version tests the
plain present modes instead, which is what actually-smooth emulators do.)

For each of **IMMEDIATE / MAILBOX / FIFO** the spike:

- paces submission to exactly 70.086 Hz with **`SDL_DelayPrecise`** (which
  avoids Windows' ~15 ms default timer resolution wrecking the pacing),
- measures the **real on-screen flip interval** via **`VK_KHR_present_wait`**
  (`vkWaitForPresentKHR` returns when a `presentId` has actually been presented;
  the CPU timestamp when it returns gives clean, monotonic flip times — the same
  primitive DXVK/Dolphin use to measure present timing),
- measures how long **`vkQueuePresentKHR`** blocks the calling thread.

**Read the result like this:** a mode whose flip interval sits at ~14.268 ms is
doing 70 Hz (VRR engaged). ~16.68 ms (two vblanks on a 120 Hz panel) means it
fell back to 60 Hz. The `-> p50 flip = X ms = Y Hz` line states the verdict per
mode.

## Build

Needs `VCPKG_ROOT` set. From this directory:

```
cmake --preset windows && cmake --build --preset windows
cmake --preset linux   && cmake --build --preset linux
cmake --preset macos   && cmake --build --preset macos    # compiles only
```

## Run

Fullscreen (default); **VRR/G-Sync must be enabled** on the display or every
mode will be fixed-refresh. Pass `--windowed` to compare.

```
build\windows\Release\vulkan_ext_present_timing_spike.exe
./build/linux/vulkan_ext_present_timing_spike
```

## What to send back

The three `=== MODE: ... ===` blocks — especially each mode's
`-> p50 flip = X ms = Y Hz` verdict line and the `vkQueuePresentKHR call` block
times.

## Notes

- `VK_KHR_present_wait` + `present_id` provide the flip times. If a driver lacks
  them the spike still runs and reports block times — judge smoothness by eye.
- Content is a 3-colour cycle; the measurement is the flip numbers, not the
  visuals.
- Spike-grade: minimal error recovery, deliberate leaks before exit.
