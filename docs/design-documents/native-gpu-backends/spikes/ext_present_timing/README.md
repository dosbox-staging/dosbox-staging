# Spike 6 — VK_EXT_present_timing on desktop

The desktop analogue of Spike 5 (MoltenVK + `VK_GOOGLE_display_timing`).
It presents 300 clear-colour frames at a 70.086 Hz cadence through native
Vulkan with `VK_EXT_present_timing`, schedules each present with an absolute
target timestamp, and reads back the actual **first-pixel-visible** glass time
from past-presentation feedback — proving (or disproving) tier-1 pacing on
real hardware.

It also times `vkAcquireNextImageKHR` and `vkQueuePresentKHR` to answer the
open question of whether present blocks the calling thread on NVIDIA-Windows
(the "emulation never stalls on the display" invariant).

## Status

- **API-current**: written against Vulkan-Headers **1.4.350** (spec v3). The
  shipping API differs from the proposal pinned in the plan's Appendix D
  Spike 3 — notably `vkGetPastPresentationTimingEXT` now takes info/properties
  structs with a nested `VkPresentStageTimeEXT` array, and feedback carries a
  `uint64_t presentId` paired with `VK_KHR_present_id2`.
- **Compile-verified on macOS only.** It cannot *run* on macOS — MoltenVK does
  not implement `VK_EXT_present_timing`, so it exits cleanly at step `[3]`.
- **Runs on**: Linux (Mesa 26.1+ RADV/ANV/NVK, or NVIDIA R595+) and Windows
  (NVIDIA R595+). AMD/Intel on Windows do not expose the extension yet.

## Build

Needs `VCPKG_ROOT` set. From this directory:

```
cmake --preset linux   && cmake --build --preset linux
cmake --preset windows && cmake --build --preset windows
cmake --preset macos   && cmake --build --preset macos    # compiles only
```

## Run

Fullscreen is required for real pacing numbers (a windowed compositor
quantises to its own refresh grid — Spike 4's lesson). Default is fullscreen;
pass `--windowed` to compare.

```
./build/linux/vulkan_ext_present_timing_spike
build\windows\Release\vulkan_ext_present_timing_spike.exe
```

First confirm the extension is actually present:
`vulkaninfo | grep -i present_timing`.

## What to capture and send back

The whole stdout, but especially the final report block:

- `glass vs target [ms]` — scheduling accuracy (want a tight, ~constant offset).
- `glass interval [ms]` + the histogram — pacing jitter (want a sharp peak at
  14.268 ms, or the 2:1 refresh-grid split Spike 5 saw windowed).
- `vkQueuePresentKHR call [ms]` — **the blocking check**. Near-zero = present
  is non-blocking on this driver; ~13 ms = it blocks (the NVIDIA-Windows FIFO
  concern), which the plan must account for.
- The `[1]`..`[9]` progress lines — if it fails, the last line printed says
  exactly where.

## Notes / known simplifications

- Uses a host-clock swapchain time domain (CLOCK_MONOTONIC on Linux,
  QueryPerformanceCounter on Windows) directly, so no
  `vkGetCalibratedTimestampsKHR` dance. If the driver exposes no host time
  domain, it exits at `[8]` — tell me and I'll add calibration.
- No validation layers by default (keep the run robust on a bare box); add
  `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` in the environment to enable.
- Spike-grade: minimal error recovery, deliberate leaks before exit.
