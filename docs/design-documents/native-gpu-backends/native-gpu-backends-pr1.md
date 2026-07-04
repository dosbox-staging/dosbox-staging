# PR 1 — deferred-present scheduler + pacing instrumentation (execution spec)

Part of the native Vulkan render backend series. Rationale,
decisions, and study references live in `native-gpu-backends-plan.md`
(“the plan”); this file is the implementation contract for PR 1.
Follow it commit by commit. **If reality disagrees with this spec at
any point, stop and update the spec first** (plan rule: extend the
plan, then implement from it — never improvise around a mismatch).

All `file:line` anchors below were verified on the
`jn/video-refactoring` tip (2026-07-04). Line numbers may drift a few
lines; the named functions and quoted code are the real anchors.

## Goal

Dos-rate presents currently fire *inside* the VGA vretrace event
(`GFX_EndUpdate`, `src/gui/sdl_gui.cpp:1093-1127`), i.e. mid
tick-burst, up to ~1 ms early with burst-dependent jitter (the
comment block at lines 1099–1124 documents this and asks for exactly
this work). After this PR, a dos-rate present is *armed* at vretrace
with a wall-clock due time derived from the emulated moment
(`tick start + PIC_TickIndex() × 1 ms`) and *submitted* when the
clock reaches it — landing within ~0.2–0.5 ms of the target via the
main loop's existing polling, with no new threads and no busy-waits.

This is backend-independent: it sits above `RenderBackend`, improves
the existing OpenGL and SDL texture backends on VRR displays today,
and later becomes the permanent fallback tier below the native
present-timing path (plan decision 2). It ships unconditionally — no
config gate (John's locked decision).

## Prerequisites

- `jn/video-refactoring` merged (or this branch based on its tip).
- No Vulkan work is involved; this PR touches only
  `src/dosbox.cpp`, `src/gui/sdl_gui.cpp`, the two gui common
  headers, `include/dosbox.h`-equivalent (wherever
  `DOSBOX_GetTicksDone` is declared — dosbox.h:104-105), and tests.

## Success criteria (PR level)

1. On a VRR display, smooth-scrolling content at DOS rates is equal
   or better than before (never worse) — human-verified, matrix below.
2. Captures are byte-identical to pre-PR captures for the same
   content (capture cadence and dirty-ungating unchanged).
3. Pause repaint, window-resize repaint, fast-forward, and host-rate
   behaviour are unchanged.
4. No added emulation stalls at high cycle counts (governor split
   must not eat CPU budget).
5. Instrumentation exists (`DEBUG_PRESENT_PACING`) that measures
   present intervals and armed→submit latency, proving item 1 with
   numbers, and it is compiled out by default.
6. Every commit compiles and passes tests standalone
   (`./scripts/tools/compile-commits.sh <preset> --test`).

## Standing rules (every commit)

- Learning doc: append a chapter to
  `native-gpu-backends-learning.md` in the same commit (short is
  fine for mechanical commits; the standing rule is plan decision 10).
- Style: `.claude/rules/code-style.md` applies; run
  `./scripts/tools/format-commit.sh` before committing; new code uses
  `CHECK_NARROWING()` + `"util/checks.h"`.
- Commit subjects: imperative, no `fix:`/`feat:` prefixes (repo
  rule); the subjects below are pre-agreed — use them verbatim.
- No attribution entries expected in this PR (nothing here derives
  from the studied emulators); if that changes, update
  `native-gpu-backends-attribution.md` in the same commit.

## Shared implementation notes (read before commit 1)

- **Clock discipline.** `ticks.last` (dosbox.cpp, advanced at ~:706)
  is in *milliseconds* on the same monotonic clock the rest of the
  loop uses (`GetTicks()` family). All new state is in
  *microseconds* on that same clock. Find the µs helper the file
  already uses (`sdl.presentation.last_present_time_us` is set from
  it) and use exactly that one — never mix clock sources.
- **`SDL_DelayPrecise` takes NANOSECONDS.** The existing call site
  (dosbox.cpp:670) multiplies µs by 1000. Keep that convention; a
  missing `* 1000` here produces a “works but paces at 1000×” bug
  that eyeballs won't catch — the unit is the guardrail.
- **Pause is a five-state FSM** (dosbox.cpp: `Running`,
  `UserPausePending`, `UserPaused`, `AutoPausePending`,
  `AutoPaused`). `DOSBOX_IsPaused()` (dosbox.cpp:243) is true only
  in the two settled paused states. During *pending* states the
  audio fade is still running and emulation still produces frames —
  scheduling must behave normally there. Do not add special-casing
  for pending states.
- **Poll calls vs direct calls.** After this PR,
  `GFX_MaybePresentFrame()` is called in dos-rate mode both by
  pollers (which must present only when a pending present is due)
  and by direct repaint paths — `GFX_ResetScreen` pause repaint
  (sdl_gui.cpp:341-343) and `paused_tick` (dosbox.cpp:528-549) —
  which must keep today's present-immediately semantics. These are
  distinguished **by pause state, not by a parameter**: both direct
  paths only run while `DOSBOX_IsPaused()` is true, and pollers only
  run while it is false. The dos-rate logic is therefore exactly:

  ```cpp
  // dos-rate path inside GFX_MaybePresentFrame()
  const auto force_present = CAPTURE_IsCapturingPostRenderImage(); // :2396
  if (force_present || DOSBOX_IsPaused()) {
          // captures stay dirty-ungated and DOS-rate; pause
          // repaints keep present-immediately semantics
          sdl.presentation.pending_present_due_us = {};
          // ... fall through to the existing present code
  } else if (sdl.presentation.pending_present_due_us &&
             now_us >= *sdl.presentation.pending_present_due_us) {
          sdl.presentation.pending_present_due_us = {};
          // ... fall through to the existing present code
  } else {
          return; // nothing due — poll again later
  }
  ```

- **Re-arming while armed** (next vretrace before the previous due
  fired) cannot happen at sane DOS rates given 0.2–0.5 ms poll
  granularity; if it ever does, overwriting the pending due time
  (dropping the older frame) is the correct behaviour. One comment
  at the overwrite site, no extra machinery.

## Commits

### Commit 1 — `Expose tick-start time and fast-forward state from the main loop`

**Files:** `src/dosbox.cpp`, the header declaring
`DOSBOX_GetTicksDone` (dosbox.h:104-105), `src/gui/private/common.h`,
`tests/present_scheduling_tests.cpp` (new), `tests/CMakeLists.txt`.

**Steps:**

1. In dosbox.cpp, next to `DOSBOX_GetTicksDone` (:109), add:

   ```cpp
   int64_t DOSBOX_GetLastTickStartUs()
   {
           return ticks.last * 1000;
   }

   bool DOSBOX_IsFastForwarding()
   {
           return ticks.locked;
   }
   ```

   Declare both in the same header as `DOSBOX_GetTicksDone`.
   (`ticks.locked` is the turbo/fast-forward flag — set/cleared in
   `DOSBOX_UnlockSpeed`, dosbox.cpp:929/:942, consumed at :643.)

2. In `src/gui/private/common.h`, add the pure due-time helper (this
   header is test-includable — precedent:
   `tests/shader_pragma_parser_tests.cpp` includes a private
   header):

   ```cpp
   // Wall-clock µs when the current emulated moment is due on
   // screen: the start of the tick being emulated plus the fraction
   // of it already emulated. Pure; unit-tested.
   inline int64_t ComputePresentDueTimeUs(const int64_t last_tick_start_us,
                                          const double tick_index)
   {
           return last_tick_start_us + llround(tick_index * 1000.0);
   }
   ```

3. New `tests/present_scheduling_tests.cpp` (gtest; register in the
   `add_executable(dosbox_tests ...)` list in `tests/CMakeLists.txt`,
   keeping the list alphabetical). Cases, at minimum:
   - `tick_index == 0.0` → returns `last_tick_start_us`;
   - `0.5` → `+500`;
   - rounding: `0.0004` → `+0`, `0.0006` → `+1`, `0.9995` → `+1000`
     (llround semantics — document that a due time may land on the
     next tick boundary; that is correct);
   - large clock values (days of uptime): no overflow at
     `int64_t` ms×1000.

**Guardrails:** No call sites change in this commit. Nothing reads
the new functions yet — that is intentional (each commit compiles
and tests standalone).

**Automated verification:**

```sh
cmake --build --preset $PRESET
ctest --preset $PRESET -R present_scheduling
./scripts/tools/format-commit.sh --verify
```

All green before committing. (`$PRESET`: pick your platform's from
`cmake --list-presets`, e.g. `release-macos-arm64`.)

**Manual verification:** none needed.

### Commit 2 — `Schedule dos-rate presents at their emulated due time`

**Files:** `src/gui/sdl_gui.cpp`, `src/gui/private/sdl_gui.h`,
`src/gui/private/common.h`, `src/gui/common.h`, `src/dosbox.cpp`.

**Steps:**

1. State — in the `presentation` sub-struct of the `sdl` struct
   (`src/gui/private/sdl_gui.h:146-153`), add:

   ```cpp
   // Deferred-present scheduler (dos-rate mode): wall-clock µs when
   // the armed present is due; empty = nothing armed
   std::optional<int64_t> pending_present_due_us = {};
   ```

2. `GFX_SchedulePresent()` — new function in sdl_gui.cpp, declared
   in `src/gui/private/common.h` next to `GFX_EndUpdate` (:110 — it
   is module-internal; the *public* `gui/common.h` is only for
   functions dosbox.cpp calls):

   ```cpp
   void GFX_SchedulePresent()
   {
           if (DOSBOX_IsPaused() || DOSBOX_IsFastForwarding()) {
                   // Due times are meaningless when wall-clock and
                   // emulated time are decoupled
                   sdl.presentation.pending_present_due_us = {};
                   GFX_MaybePresentFrame();
                   return;
           }
           const auto due_us = ComputePresentDueTimeUs(
                   DOSBOX_GetLastTickStartUs(), PIC_TickIndex());
           if (due_us <= /* now, µs, same clock */) {
                   // Emulation is running behind; present immediately
                   sdl.presentation.pending_present_due_us = {};
                   GFX_MaybePresentFrame();
           } else {
                   sdl.presentation.pending_present_due_us = due_us;
           }
   }
   ```

   (`PIC_TickIndex()` is inline in `src/hardware/pic.h:36-39`.)

3. `GFX_EndUpdate()` (sdl_gui.cpp:1093): in the
   `PresentationMode::DosRate` branch, replace the
   `GFX_MaybePresentFrame()` call (:1126) with
   `GFX_SchedulePresent()`. Leave the explanatory comment block but
   update it: the 1–5 ms jitter description becomes historical —
   rewrite it to describe the armed/due scheme (this comment is
   load-bearing documentation; keep its length).

4. `GFX_MaybePresentFrame()` (sdl_gui.cpp:2385): implement the
   dos-rate gating exactly as given in “Poll calls vs direct calls”
   above. The host-rate path is untouched. `RENDER_MaybeUploadFrame()`
   (:2409) stays where it is — upload cost lands at submit time, not
   at arm time.

5. `normal_loop` (dosbox.cpp:586-604): remove the
   `== PresentationMode::HostRate` gate (:602) so the empty-queue
   branch polls `GFX_MaybePresentFrame()` in both presentation
   modes. This is the poller: 2–5 polls/ms → ~0.2–0.5 ms submit
   granularity.

6. `GFX_GetTimeToPendingPresentUs()` — new, sdl_gui.cpp, declared in
   the **public** `src/gui/common.h` (:45 area — dosbox.cpp will call
   it in commit 3): returns `-1` when nothing is armed, else
   `max<int64_t>(0, due - now)`. Added now so commit 3 is a pure
   consumer; it is dead code for exactly one commit (acceptable —
   note it in the commit message body).

**Guardrails:**

- Do NOT touch: `setup_presentation_mode` (:489-532), the host-rate
  3 ms early window (:527), `adjust_ticks_after_present_frame`
  (:2339-2356), capture code, or any `RenderBackend`.
- The pause-FSM note applies: no special handling for
  `*PausePending` states.
- Keep the change surgical — the diff should be ~60–90 lines. If it
  grows past that, you are refactoring; stop and reread the spec.

**Automated verification:**

```sh
cmake --build --preset $PRESET && ctest --preset $PRESET
# Boot-and-exit probe (real window; texture + opengl + both modes):
cat > /tmp/probe.conf <<'EOF'
[sdl]
presentation_mode = dos-rate
[autoexec]
exit
EOF
$DOSBOX -conf /tmp/probe.conf   # must reach the DOS prompt and exit 0
# then again with presentation_mode = host-rate, and with
# output = texture. Grep the log for errors each time.
```

($DOSBOX = the binary under `build/<preset>/`.) A clean boot-exit in
both modes on both backends is the automated bar for this commit;
the behavioural proof arrives with the commit-4 instrumentation and
the human matrix.

**Manual verification (quick pass now, full matrix at PR end):**
VRR smooth-scroll spot check (Commander Keen 4 or Pinball Dreams —
equal or better than a pre-PR build); pause + un-pause repaints; one
screenshot capture (Ctrl+F5 by default) diffed byte-identical
against a pre-PR capture of the same still screen (`cmp` the PNGs).

### Commit 3 — `Split the governor sleep around pending presents`

**Files:** `src/dosbox.cpp`.

**Steps:**

1. In `increase_ticks()` (dosbox.cpp:637), the governor sleeps to
   the next tick boundary at :670
   (`SDL_DelayPrecise(sleep_us * 1000)`). Before that call, query
   `GFX_GetTimeToPendingPresentUs()`; when `0 <= t_us < sleep_us`:

   ```cpp
   SDL_DelayPrecise(static_cast<uint64_t>(t_us) * 1000); // ns!
   GFX_MaybePresentFrame();
   // Recompute the remainder from the clock (the present itself
   // took time) and finish the sleep if any remains
   ```

   Otherwise the existing single sleep runs unchanged.

2. Do not touch the slept-time accounting (:673-689) — it measures
   total elapsed time and is correct for the split case as-is.
   Verify by reading it, and say so in the commit message body.

3. Fast-forward: while `ticks.locked` the function returns early
   (:643) and never reaches the sleep — no code needed; verify turbo
   still presents sanely (it presents immediately via
   `GFX_SchedulePresent`'s fast-forward branch from commit 2).

**Guardrails:** The split must only ever *shorten* the pre-present
sleep, never lengthen total sleep. If `t_us == 0`, present first,
then sleep the full remainder.

**Automated verification:** build + full `ctest` + the commit-2
boot probe (both modes). Add a temporary high-cycles smoke: boot
with `cycles = max` in `[cpu]` and `exit`; confirm no hang and
prompt reached.

**Manual verification:** turbo (Alt+F12 by default) held during a
scroller: speed-up works, no present starvation; high fixed cycles
on a weak-ish machine profile: no audible/visible stall regression.

### Commit 4 — `Add present-pacing instrumentation`

**Files:** `src/gui/sdl_gui.cpp`, learning doc.

**Steps:**

1. Add `#define DEBUG_PRESENT_PACING 0` near the top of sdl_gui.cpp
   (per code-style rule: per-topic define switches).
2. Replace the `#if 0` block in `GFX_MaybePresentFrame`
   (:2416-2427) with a `#if DEBUG_PRESENT_PACING` block collecting:
   - rolling ring of the last 128 presented intervals (µs), reported
     as mean / p95 / max against
     `sdl.presentation.frame_time_us`;
   - armed→submitted latency (due_us vs actual submit time) — the
     direct measure of scheduler accuracy;
   - counters: scheduled presents, immediate (behind/pause/turbo)
     presents, forced (capture) presents;
   - cumulative `adjust_ticks_after_present_frame` total (expect
     ≈ 0 on the native backends later — the regression signal from
     the plan's Architecture section).
   Report via the same `LOG_TRACE`-style call the old block used,
   every 128 presents.
3. Learning-doc chapter for the PR (this is the natural commit to
   carry the long chapter: the pacing problem recap, the
   arm/poll/split design, and first measured numbers).

**Guardrails:** `DEBUG_PRESENT_PACING` must be `0` in the committed
state. The instrumentation must compile in both states — build once
with it flipped to `1` locally (that is part of verification), flip
back to `0`, rebuild, then commit.

**Automated verification:**

```sh
# with DEBUG_PRESENT_PACING 1:
$DOSBOX -conf /tmp/probe-longer.conf 2>&1 | grep -c "PACING"
# probe-longer.conf: autoexec runs a ~10 s busy loop before exit so
# ≥128 presents accumulate; assert at least one stats line and
# armed→submit p95 < 1000 µs in the printed stats. Then flip back
# to 0, rebuild, verify zero PACING lines, run format + full ctest.
```

**Manual verification:** with instrumentation on and a VRR display,
confirm mean interval ≈ `frame_time_us` (±1%) in mode 13h content.

## Manual test matrix (humans, before merge)

Run on: Windows/RTX 3060 + VRR (primary), macOS M4 (secondary);
`output = opengl` and `output = texture`; windowed and fullscreen.

| # | Scenario | Steps | Pass when |
| --- | -------- | ----- | --------- |
| 1 | VRR smooth scroll | Keen 4 / Pinball Dreams, dos-rate, vsync off, fullscreen on VRR | Scroll ≥ as smooth as pre-PR build (A/B same machine) |
| 2 | Fixed-rate display | Same content, 60 Hz monitor or VRR pinned 60 | No regression vs pre-PR |
| 3 | Captures | Ctrl+F5 on a static screen, pre- and post-PR builds | PNGs byte-identical (`cmp`) |
| 4 | Pause/repaint | Pause, resize window, un-pause | Image repaints correctly at every step |
| 5 | Host-rate mode | `presentation_mode = host-rate`, scroller | Behaviour unchanged vs pre-PR |
| 6 | Fast-forward | Hold turbo in a scroller | Speeds up, presents keep flowing, returns to normal cleanly |
| 7 | High cycles | `cycles = max`, demanding game | No new stutter/stalls |
| 8 | Vsync on | `vsync = on`, dos-rate, fullscreen | No present starvation (GL blocking swap interacts with due times) |

Record results in the PR description. Scenario 8 is the one most
likely to surface a problem (a blocking `SwapBuffers` inside a
due-time submit); if it stalls, the correct reaction is to present
*early* by the measured swap-block time on GL — but measure first,
and bring the numbers to review.

## Exit checklist

- [ ] All four commits individually green:
      `./scripts/tools/compile-commits.sh $PRESET --test`
- [ ] Instrumentation numbers pasted into the PR (armed→submit p95,
      interval mean vs target)
- [ ] Manual matrix table filled in
- [ ] Learning-doc chapters present for all commits
- [ ] `DEBUG_PRESENT_PACING` committed as `0`
