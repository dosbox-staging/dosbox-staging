# Spike 6 — VK_EXT_present_timing on desktop

The desktop analogue of Spike 5 (MoltenVK + `VK_GOOGLE_display_timing`).
It presents 300 clear-colour frames at a 70.086 Hz cadence through native
Vulkan with `VK_EXT_present_timing`, schedules each present with a **relative**
target time (the frame period, a plain ns duration), and reads back per-present
feedback (the best stage the surface offers) — proving (or disproving) tier-1
pacing on real hardware.

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
- **First-run finding (RTX 3060 / R595, 4K):** absolute-time scheduling and
  the first-pixel-visible stage are reported *unsupported* on that surface, and
  only a swapchain-local time domain is offered. So the spike uses
  **relative-time scheduling** (needs no host-clock calibration) and the
  guaranteed **`QUEUE_OPERATIONS_END`** stage (GPU-work-done, not scanout). The
  plan's flagship "absolute timestamp + photons-on-glass" is therefore not what
  NVIDIA's first-gen driver actually offers — see Appendix D Spike 3.

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

- `feedback interval [ms]` + the histogram — the pacing result (want a sharp
  peak at 14.268 ms, or the 2:1 refresh-grid split Spike 5 saw windowed).
- `interval - period [ms]` — jitter around the target period (want ~0). Note
  the `stage:` line — `queue_operations_end` means GPU-work-done, not true
  glass, so treat it as a proxy.
- `vkQueuePresentKHR call [ms]` — **the blocking check**. Near-zero = present
  is non-blocking on this driver; ~13 ms = it blocks (the NVIDIA-Windows FIFO
  concern), which the plan must account for.
- The `[1]`..`[9]` progress lines — if it fails, the last line printed says
  exactly where.

## Notes / known simplifications

- Relative-time scheduling: `targetTime` = the frame period (ns), so no
  time-domain calibration is needed to schedule. Feedback stage times are in
  the swapchain-local domain; the intervals reported assume that domain counts
  nanoseconds (holds on the drivers seen so far — if the numbers look wildly
  off, that assumption broke and we add calibration).
- No CPU-side pacing sleep: the 2-frame fence ring throttles submission, so any
  cadence measured is the driver's relative-time pacing, not our timer.
- No validation layers by default (keep the run robust on a bare box); add
  `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` in the environment to enable.
- Spike-grade: minimal error recovery, deliberate leaks before exit.
