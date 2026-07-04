# PR 6 — present timing, the flagship (execution spec)

Part of the native Vulkan render backend series. Rationale in
`native-gpu-backends-plan.md`: decision 2 (timing is a core
requirement), the Architecture present-timing bullet, Appendix C §7
(nobody schedules Vulkan presents yet — we are first) and §10
(vendor trust), Appendix D Spike 3 (the pinned
`VK_EXT_present_timing` API surface — treat it as the authoritative
name list), Spike 4 (schedule-ahead lesson), and Spike 5 (the
MoltenVK/`VK_GOOGLE_display_timing` path, proven; the spike source
`spikes/vulkan_timing_spike.mm` is ours — adapt freely).
**If reality disagrees with this spec — including any struct or
function name that shifted between the spike-era spec and the
shipping headers — stop, pin the real name against the installed
`vulkan-headers`, and update Appendix D + this spec first.**

## Goal

Presents scheduled by the display engine at timestamps derived from
emulated video timing, with measured glass-time feedback proving the
result in logs. Two dialects behind one internal design —
`VK_EXT_present_timing` (new Mesa/NVIDIA) and
`VK_GOOGLE_display_timing` (MoltenVK/macOS) — with the PR 1
app-timed scheduler as the always-available fallback tier, and
automatic degradation when a driver lies.

## Prerequisites

- PRs 1–3 merged (PR 5 is independent of this PR; either order
  works — resolve rebase conflicts in whoever lands second).
- Hardware for the verification half: RTX 3060 + VRR (Windows,
  NVIDIA R595+ driver — confirm with
  `vulkaninfo | grep -i present_timing` first), M4 Mac mini + MBP.
  Without at least one of these, the code can land but the PR must
  not merge — the exit criteria are measurement-based.

## Success criteria (PR level)

1. On Windows/NVIDIA + VRR fullscreen: logged first-pixel-visible
   intervals track 70.086 Hz — mean within ±0.2% of 14.268 ms,
   interval p95 jitter < 1 ms over ≥ 1000 frames of scrolling
   content.
2. On both Macs (MoltenVK/GOOGLE dialect): same measurement
   standard fullscreen; windowed shows the documented compositor
   quantisation (Spike 4/5 behaviour) — expected, not a failure.
3. Forcibly disabling the extension (debug knob) degrades cleanly to
   the PR 1 scheduler with a single clear log line — no crashes, no
   pacing collapse.
4. Bogus feedback (per-vendor trust rules) is detected and logged,
   and drift correction refuses to act on it.
5. `adjust_ticks_after_present_frame` cumulative total remains ≈ 0
   on the vulkan backend (the never-blocks invariant survived).
6. The dispatch logic (scheduled-present fast path vs armed
   scheduler) is unit-tested without a real backend.
7. Every commit standalone-green.

## Standing rules

Same as previous PRs. Attribution flips are significant in this PR —
listed per commit. New pacing knowledge (what first-generation
drivers actually did) is exactly what the learning doc exists for:
write it down as you hit it.

## Commits

### Commit 1 — `Add scheduled-present support to the render backend interface`

**Files:** `src/gui/render/render_backend.h`, `src/gui/sdl_gui.cpp`,
`src/gui/private/common.h`, `tests/present_dispatch_tests.cpp`.

**Steps:**

1. The only interface change of the whole series (flesh-out
   decision 5), added to `RenderBackend`:

   ```cpp
   // Display-engine-scheduled presents (PR 6). Backends that can
   // attach a target timestamp to a present override both.
   virtual bool SupportsScheduledPresents() { return false; }
   virtual void PresentFrameAt([[maybe_unused]] const int64_t due_time_us)
   {
           PresentFrame();
   }
   ```

2. Extract the dispatch decision as a pure function in
   `gui/private/common.h` (unit-testable — same pattern as PR 1's
   due-time helper):

   ```cpp
   enum class PresentDispatch { SubmitTimedNow, ArmScheduler,
                                PresentImmediately };
   PresentDispatch DecidePresentDispatch(bool backend_schedules,
                                         bool is_paused,
                                         bool is_fast_forwarding,
                                         bool is_running_behind);
   ```

   Truth table (write the tests first): paused or fast-forwarding →
   `PresentImmediately` (due times meaningless — PR 1 rule);
   running behind → `PresentImmediately`; backend schedules →
   `SubmitTimedNow` (vretrace submits immediately, timestamp
   attached — no polling latency at all); otherwise →
   `ArmScheduler` (the PR 1 path, unchanged).

3. `GFX_SchedulePresent()` consumes the decision: `SubmitTimedNow`
   calls `backend->PresentFrameAt(due_us)` right from the vretrace
   event (upload first — keep `RENDER_MaybeUploadFrame` ordering
   consistent with the poll path); the other outcomes behave exactly
   as PR 1 left them.

**Automated verification:** `ctest -R present_dispatch` (full truth
table); full ctest; boot probes on vulkan (still scheduler tier —
nothing implements the capability yet) and opengl.

**Attribution flips:** none.

### Commit 2 — `Implement VK_EXT_present_timing in the Vulkan backend`

**Files:** `vulkan/private/vulkan_swapchain.*`,
`vulkan/private/vulkan_device.*`, `vulkan/vulkan_renderer.*`, new
`vulkan/private/vulkan_present_timing.h/.cpp` (the dialect-neutral
feedback ring + drift logic, so commit 3 reuses it),
`tests/present_timing_logic_tests.cpp`.

**API surface** (pinned by Appendix D Spike 3 — verify each name
against the installed vulkan-headers before use, per the header
rule above):

- Feature probe: `VkPhysicalDevicePresentTimingFeaturesEXT` —
  require `presentTiming` AND `presentAtAbsoluteTime`; surface caps
  via `VkPresentTimingSurfaceCapabilitiesEXT`.
- Swapchain opt-in: `VK_SWAPCHAIN_CREATE_PRESENT_TIMING_BIT_EXT` at
  creation (recreation path must preserve it).
- **Feedback queue MUST be sized** or results silently drop:
  `vkSetSwapchainPresentTimingQueueSizeEXT(device, swapchain,
  frames_in_flight + 4)`.
- Clock calibration: enable `VK_KHR_calibrated_timestamps`; map our
  monotonic µs clock to the swapchain time domain
  (`vkGetSwapchainTimeDomainPropertiesEXT` →
  `vkGetCalibratedTimestampsKHR` +
  `VkSwapchainCalibratedTimestampInfoEXT`); recalibrate every ~5 s
  and on swapchain recreation; store offset + drift slope.
- Scheduling: chain `VkPresentTimingsInfoEXT` (one
  `VkPresentTimingInfoEXT`: absolute `targetTime` in the calibrated
  domain, `timeDomainId`) into `VkPresentInfoKHR.pNext` via
  `vk::StructureChain`. **Target = the due time itself — never add
  artificial lead.** We cannot submit before the frame exists, so
  glass time lands at `due + pipeline_depth` as a **constant
  offset** (Spike 4 measured 2–3 cycles windowed; expect ~1 cycle
  under fullscreen direct scanout). That constant is benign —
  jitter is the enemy — and inflating targets to "give the display
  engine lead" would buy nothing except 2–3 frames of deliberate
  latency. The offset is measured and reported (commit 5), never
  chased.
- Feedback: poll `vkGetPastPresentationTimingEXT` each present;
  correlate via `VK_KHR_present_id2`; read the
  `IMAGE_FIRST_PIXEL_VISIBLE` stage as glass time.

**Dialect-neutral core** (`vulkan_present_timing.*` — design it so
commit 3 plugs in):

- `FrameTimeRecord { present_id, target_us, actual_glass_us }` in a
  fixed ring (size 128) — modelled on PPSSPP's FrameTimeData
  (credit comment at the ring:
  `// Feedback-history ring after PPSSPP's FrameTimeData
  (Common/GPU/Vulkan/VulkanRenderManager.cpp) — idea credit, see
  native-gpu-backends-attribution.md`).
- Drift correction, pure and unit-tested:
  `int64_t ComputeTargetAdjustmentUs(span<const FrameTimeRecord>)`.
  The offset `actual − target` has two components: a **constant
  level** (the present pipeline's depth — leave it alone, it is
  latency, not error) and a **slope** (the DOS clock and the
  display clock drifting apart — this is what gets corrected).
  Correct on the slope/trend of the offset over the ring, clamped
  to ±0.5 refresh cycles per step, slewing future targets so the
  clocks converge without stair-stepping. Unit tests must include:
  constant large offset → zero adjustment; slowly growing offset →
  slew; clamp behaviour.
- Missed-window detection: `(actual − target)` deviating from the
  **rolling median offset** by more than one refresh cycle → count
  + trace log. (Absolute thresholds are wrong here — under a
  windowed compositor the constant offset alone exceeds any sane
  absolute threshold.)
- **Feedback validation with per-vendor trust priors** (adapted from
  RPCS3 — GPLv2, code/data adaptation allowed WITH SPDX):
  `SPDX-FileCopyrightText` line for RPCS3 in the file carrying the
  vendor table + site comment naming
  `rsx/VK/vkutils/swapchain.cpp`. Priors: NVIDIA/Intel-ANV trusted;
  AMD/RADV/MoltenVK validate-before-trust. Validation regardless of
  prior: reject non-monotonic or absurd timestamps (glass time
  before submit, > 1 s in the future); after 8 consecutive rejects,
  stop feeding drift correction and log the degradation once.
- Tier fallback: extension absent, feature bits missing,
  calibration failure, or sustained bogus feedback → run the PR 1
  scheduler tier (the `SupportsScheduledPresents()` answer flips to
  false at init; runtime degradation routes `PresentFrameAt` to the
  scheduler path). One log line states the active tier at startup
  and on every change — the manual matrix greps for it.
- **Debug force-tier knob** (needed by the matrix): environment
  variable `DOSBOX_PRESENT_TIMING=ext|google|scheduler|off`
  (debug builds honour it; release ignores it). Document in the
  file-top comment.

**Unit tests:** drift EMA (converges on constant offset, clamps
steps, ignores untrusted records), missed-window classification,
feedback validation (each reject rule), tier-degradation state
machine.

**Automated verification:** ctest; lavapipe job still green
(lavapipe has no present_timing → exercises the absent-extension
path — assert the “tier: app-timed scheduler” log line in the CI
probe); boot probe on dev machine.

**Manual verification (hardware, now):** RTX 3060: `vulkaninfo |
grep -i present_timing` confirms; run scrolling content fullscreen
VRR with `DEBUG_PRESENT_PACING 1`: glass-interval mean/p95 as per
success criterion 1; pull the knob through all tiers.

**Attribution flips:** PPSSPP “Per-frame present-timing history
ring” → landed; PPSSPP “Present-timing feedback prior art” →
landed; RPCS3 “Per-vendor present-timing trust table” → landed
(with SPDX).

### Commit 3 — `Implement the VK_GOOGLE_display_timing dialect`

**Files:** vulkan_present_timing.*, vulkan_swapchain.*.

**Steps** (Spike 5 proved this end to end — adapt from
`spikes/vulkan_timing_spike.mm`):

1. Probe order: `VK_EXT_present_timing` first; absent → probe
   `VK_GOOGLE_display_timing` (device extension on MoltenVK; entry
   points via the device dispatcher).
2. Scheduling: chain `VkPresentTimesInfoGOOGLE`
   (`presentID`, `desiredPresentTime`) into the present. On
   MoltenVK the time domain is mach-absolute nanoseconds ==
   `clock_gettime_nsec_np(CLOCK_UPTIME_RAW)` (Spike 5-confirmed;
   no calibrated-timestamps ceremony needed on this path — convert
   our µs clock once, assert the relationship in a debug check).
   Same 2–3-cycle lead rule.
3. Feedback: `vkGetPastPresentationTimingGOOGLE` into the SAME ring
   (`actualPresentTime` per `presentID`); refresh period from
   `vkGetRefreshCycleDurationGOOGLE` (drives lead + missed-window
   thresholds). MoltenVK sits in the validate-before-trust bucket —
   Spike 5's feedback was complete and sane, so expect validation
   to pass and trust to be earned quickly; keep the checks anyway.
4. Same drift/miss/degradation logic — zero new policy code; that
   was the point of the dialect-neutral core.

**Automated verification:** ctest; boot probe on the Mac shows
“tier: VK_GOOGLE_display_timing” log line; knob forces each tier.

**Manual verification:** M4 mini on the VRR panel + MBP ProMotion:
fullscreen glass-interval measurements per success criterion 2
(Spike 5 already proved windowed quantisation — reproduce it once,
document, move on); external-VRR vs built-in-VRR both covered.

**Attribution flips:** DuckStation “Scheduled Metal presents” →
landed (prior-art credit; we reach the same machinery through
MoltenVK — the entry text already says so).

### Commit 4 — `Evaluate Win32 exclusive fullscreen and acquire overlap`

Two candidate improvements, measured on the RTX 3060, kept only if
they demonstrably improve pacing. **Protocol per candidate:** 3 runs
× 1000 frames of 70.086 Hz scrolling content, fullscreen VRR,
metrics from the pacing report (interval p95 error, missed windows).
Keep if p95 improves ≥ 10% consistently; otherwise delete the code
(keep the learning-doc write-up either way — negative results are
results).

1. `VK_EXT_full_screen_exclusive`: behind an internal default-off
   constant (not a user setting) — the config-gated pattern both
   PCSX2 and RPCS3 use (idea credit comment; Appendix C §10 has the
   described pattern: chain the Win32 HMONITOR full-screen-exclusive
   info structs into swapchain creation, handle
   `VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT` as a rebuild
   event in `DecideWsiAction`).
2. Acquire-after-present overlap (DXVK-originated, credited via
   DuckStation): immediately after `vkQueuePresentKHR`, acquire the
   next image so the wait overlaps present latency. Note: with
   timed presents the CPU is idle-by-design between frames, so the
   expected gain is small — that is what the measurement is for.

**Automated verification:** ctest + boot probes with each candidate
toggled; no validation errors in either state.

**Attribution flips:** DuckStation “Acquire-after-present overlap”
→ landed-or-rejected (update the entry with the measured outcome);
PCSX2/RPCS3 exclusive-fullscreen config-gating idea — covered by
the existing RPCS3/PCSX2 entries' scope; extend the RPCS3 entry
with the outcome if adopted.

### Commit 5 — `Add the pacing verification report`

**Files:** sdl_gui.cpp / vulkan_present_timing.* instrumentation,
`docs/design-documents/native-gpu-backends/pacing-run-sheet.md`
(new).

1. Grow `DEBUG_PRESENT_PACING` into a periodic summary (every 512
   presents and at shutdown): presented-interval mean/p95/max vs
   target period; **glass-vs-due offset mean/p95 (the constant
   pipeline latency — watch that it stays constant; a drifting
   level means the slope corrector is misbehaving)**; counts per
   tier (scheduled/app-timed/immediate/
   skipped); accumulated drift correction; missed windows; feedback
   rejects; calibration offset drift. One block, grep-friendly
   prefix `PACING:`.
2. The run sheet: a checklist document for release testing derived
   from the manual matrix below — one row per hardware box from the
   plan's Verification inventory, the exact conf + env + content to
   run, the log lines to capture, and the pass thresholds. Linux
   rows carry the core-team hardware requirements (Mesa 26.1+ /
   NVIDIA R595+, VRR display, `vulkaninfo` check) verbatim from the
   plan.

**Automated verification:** report renders sanely in a probe run
(grep `PACING:`); flip define back to 0; full ctest.

**Attribution flips:** none.

## Manual test matrix (humans, before merge)

| # | Box | Scenario | Pass when |
| --- | --- | -------- | --------- |
| 1 | RTX 3060 + VRR, fullscreen | 70.086 Hz scroller, EXT tier | Criterion 1 numbers in PACING report |
| 2 | RTX 3060, windowed AND borderless + NVCP DXGI-swapchain knob | Same content | Documented behaviour; numbers recorded for the VRR guide. Borderless expectation (Xenia's finding, plan Appendix C §12): IMMEDIATE may be GDI-copied — no independent flip/VRR on that path |
| 3 | RTX 3060 | `DOSBOX_PRESENT_TIMING=scheduler` | Clean tier log; PR 1-grade pacing; no crash |
| 4 | AMD boxes ×3 (no EXT on Windows yet) | Same content | Auto-selects scheduler tier with one clear log line; pacing sane |
| 5 | M4 mini + external VRR, fullscreen | GOOGLE tier | Criterion 2 numbers |
| 6 | MBP ProMotion built-in, fullscreen + windowed | GOOGLE tier | Fullscreen tracks; windowed quantises (expected) — both recorded |
| 7 | Any | vsync on + timed presents | FIFO + timestamps coexist; no starvation |
| 8 | Any | 60 Hz fixed (VRR pinned) | Clean degradation of 70 Hz content; drop pattern regular, no drift runaway |
| 9 | Linux core team (NVIDIA R595+ / Mesa 26.1+ + VRR) | Criterion-1 protocol | Numbers reported back; X11 and Wayland both |
| 10 | Any | Long soak (30 min scroller) | Drift correction stable; no monotonic target creep; miss count not growing |

## Exit checklist

- [ ] compile-commits green for all 5 commits
- [ ] PACING report numbers for boxes 1, 5, 6 pasted into the PR
- [ ] Degradation demonstrated (knob + AMD boxes + lavapipe CI line)
- [ ] Commit-4 evaluation outcomes recorded (kept or deleted, with
      numbers) in the learning doc
- [ ] Attribution flips: PPSSPP ×2, RPCS3 (SPDX), DuckStation ×2
- [ ] Run sheet committed; Linux rows assigned to named volunteers
