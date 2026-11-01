# Adaptive CPU cycles

This branch replaces the cycle estimator in `increase_ticks()` with explicit
microsecond accounting and a separate feedback policy in
`src/cpu/cycle_controller.h`. The guest still advances through the existing
1 ms ticks and PIC events. Fixed cycles without `cpu_throttle` are unaffected.

## Measurements

Each host-time observation window records:

- Wall-clock elapsed time, including pacing and scheduling delays.
- Completed guest ticks and their assigned cycle budgets.
- Required main-thread work: wall time minus explicit pacing waits.
- Budgets discarded by the existing halt and simulated I/O-delay accounting.
- Wall-clock debt discarded by the 20 ms catch-up limit.

The load target is a main-thread wall-work budget per unit of guest time,
not a cap on the process CPU percentage reported by macOS or other host tools.
Audio workers can consume CPU concurrently, and explicit SDL pacing waits can
busy-wait even though their elapsed time is excluded from required work.

Accounting spans returns to the outer loop and nested `DOSBOX_RunMachine()`
calls. A guest tick is counted once, when its PIC/CPU budget is exhausted.
The first partial tick after a reset is excluded from completed guest time.
Pause/resume, fast-forward, screen resets, CPU configuration changes, and
cycle/percentage/limit changes invalidate the observations.

The pacer waits to the next absolute millisecond boundary. Its complete wait
duration is excluded from work, including SDL busy-waiting and oversleep.
Presentation time is no longer subtracted wholesale. Texture uploads, shader
submission, capture, and rendering outside the swap/present call are charged
as work. With vsync enabled, the swap/present call is treated as a pacing wait.
With vsync disabled, it is charged as work.

This presentation split is approximate. Drivers can defer rendering work into
the swap/present call, especially `SDL_RenderPresent()`, or block during earlier
rendering calls. The instrumentation cannot distinguish every GPU stall from
intentional display pacing. A separate guest-progress measurement includes
all of this elapsed time, so sustained inability to advance guest time still
causes throttling. This needs verification on real GPU backends and refresh
rates before considering the controller ready for release.

## Feedback policy

The initial tuning values are experimental:

- Observe for at least 100 ms of host time, even under overload.
- Target at most 90% required work per unit of guest time. Legacy percentage
  requests below 90% remain lower targets.
- Hold cycles within a 2% relative dead band around the target.
- Increase by at most 100% per observation window when measured load is less
  than three quarters of the target (67.5% load), and at most 25% when
  approaching capacity. The proportional estimate still caps increases at
  the predicted target; idle/I/O filtering and the other safeguards apply.
- Reduce after two consecutive overloaded windows, by at most 50% per window.
- Treat guest progress below 95% of real time as overload independently of
  measured work. Do not discard extreme sustained overload as interference.
- Prevent increases when more than 20% of the guest cycle budget was removed
  by halt/I/O delays, or when catch-up debt was discarded.
- Respect the configured cycle ceiling and the global maximum. The normal
  auto-cycle floor is 200; explicit smaller ceilings take precedence.

For work-bound execution, the candidate is `cycles * target / measured_load`,
where `measured_load = work_us / completed_guest_us`. The upward/downward bounds
and persistence rules are then applied. Fixed device/rendering overhead need
not scale with cycles: repeated feedback approaches the sustainable budget
without requiring an instruction-cost model. Required work that already
exceeds the target at minimum cycles cannot be fixed by cycle selection.

For fixed cycles with throttling enabled, the configured cycles are the
ceiling. Recovery uses the same gradual upward adjustment. Idle-heavy guests
retain their last estimate until sufficiently active observations return.

## Audio runs concurrently

The mixer and synth workers do not contribute their CPU time to the controller.
Whole-process CPU utilization would mix parallel workloads and give the wrong
capacity estimate. Main-thread device work and blocking on other threads do
consume the measured wall-clock budget.

An independent diagnostic counts sample frames requested by SDL but not
delivered by the final-output queue, excluding intentional pause. This is a
callback delivery shortfall, not a precise count of audible glitches or a
diagnosis of their cause. Startup, host scheduling, expensive plugins/synths,
and emulation failing to supply device data can all affect it. It does not
directly lower cycles: reducing emulation speed cannot fix every audio issue.

## Trace and evaluation

Set `DOSBOX_CYCLE_TRACE=1` in the host environment before launching DOSBox.
Each completed observation window logs current/next cycles, host/guest/work
milliseconds, load, guest rate, active budget fraction, discarded debt, and
audio callback shortfall frames since the previous logged window or reset.
No per-instruction clock reads or audio-callback logging are added.

Example:

```sh
DOSBOX_CYCLE_TRACE=1 build/debug/dosbox --noprimaryconf --nolocalconf \
    --set cpu_cycles=max
```

Compare against a build of `main`, using the same compiler, configuration,
CPU core, workload, renderer, audio settings, and host power conditions.
Evaluate steady throughput, time to settle after workload changes, cycle
variation, guest rate, discarded debt, frame pacing, and audio health.
Trace logging itself can perturb timing; disable it for final performance
measurements.

The deterministic policy tests cover steady load, fixed overhead, capacity
loss/recovery, isolated stalls, sustained overload, idle/I/O-heavy guests,
presentation blocking, limits, and resets. Live validation should also cover:

- Quake/Doom and expensive FPU workloads across supported CPU cores.
- Menus or idle loops followed by busy gameplay, with `cpu_idle` on and off.
- Sound Blaster, OPL, and external synth/plugin loads, including low prebuffer.
- Vsync on/off, host-rate and other presentation modes, different refresh rates,
  texture/OpenGL backends, and heavy shaders.
- Transient host stalls, sustained contention, and thermal/power changes.
- Pause/resume, fast-forward, configuration/core changes, and real/protected
  mode transitions.

Synthetic convergence tests establish the policy's behavior, not a performance
improvement in actual games or on all host platforms.

### Initial local validation

On Apple Silicon, both debug and release emulator builds succeeded. The debug
test executable ran 936 tests: 935 passed and one was skipped, including all
17 controller tests passing.

Headless smoke tests used small SDL software output and synthetic DOS COM
programs timed by the emulated BIOS clock:

- A busy loop in the debug build settled around 64,700 cycles with real-time
  guest progress. This is a workload-specific observation, not a benchmark.
- Release runs with a 60,000-cycle throttle ceiling reached and retained it.
- One injected 80 ms process suspension caused no cycle reduction.
- Repeated process suspensions caused a reduction and subsequent recovery to
  the configured ceiling after the interruptions stopped.
- A halt-heavy guest retained 3,000 cycles instead of raising the budget.
- Fixed cycles without throttling produced no adjustment trace entries.

The release smoke tests used dummy SDL audio and reported no callback delivery
shortfall. Dummy audio does not validate audible output or expensive synths;
headless software rendering does not validate GPU/vsync behavior. Large software
output in the debug build could exceed the timing budget through rendering
overhead alone, which cycle reduction cannot resolve.

### Quake feedback and faster capacity discovery

The initial prototype reduced the user's Quake timedemo result from roughly
215 FPS to 197 FPS. A local DOSBench Quake shareware `demo1` reproduction used
320x200, the automatic/dynamic CPU core, small headless SDL software output,
vsync off, `-nosound -nocdaudio`, and host mixer `nosound=on`. All variants
used the same copied game files and configuration. Controller tracing was
disabled for the measured runs; these results are not directly comparable
to a real GPU backend or the user's original 215 FPS baseline.

Three fresh-process runs per variant gave:

| Policy | Individual FPS | Median FPS |
| --- | --- | --- |
| Original 95% target, 10% upward steps | 202.2, 202.0, 199.8 | 202.0 |
| Experimental 98% target, 10% upward steps | 188.0, 203.6, 205.4 | 203.6 |
| Retained 95% target, faster low-load probing | 206.9, 229.1, 206.8 | 206.9 |

The original trace took approximately six seconds to reach 600,000 cycles
from 3,000, despite initially using little of the work budget. The retained
change allows up to a 50% increase below half the target load, then returns
to 10% tracking steps near capacity. It preserves the 95% target, idle/I/O
filtering, cycle limits, and overload protections. The 98% target experiment
was not retained.

These are small samples with noticeable run-to-run variation, not evidence
that the original peak throughput has been restored. Warm steady-state game
performance, frame pacing, and audible playback still need evaluation.
The updated debug suite passed 937 tests with one skipped, including all
19 controller tests; new coverage checks timely startup discovery and smaller
increases near capacity. The optimized executable was rebuilt for the runs.

### 90% target and more aggressive ramp

The current policy lowers the target to 90% and allows doubling cycles below
67.5% measured load, with up to 25% increases nearer the target. Increases
remain bounded by `target / measured_load`, so these step limits do not force
a jump past the predicted target. The dead band, idle/I/O filtering, overload
persistence, cycle ceilings, and 95% minimum guest-progress guard are unchanged.
The progress guard measures ability to keep guest time advancing in real time;
it is independent of the work-load target.

The same local headless Quake setup described above gave these results, with
controller tracing disabled and three fresh-process runs per policy:

| Policy | Individual FPS | Median FPS |
| --- | --- | --- |
| Previous 95% target, 50%/10% upward steps | 212.7, 210.8, 208.7 | 210.8 |
| Current 90% target, 100%/25% upward steps | 222.4, 231.1, 221.8 | 222.4 |

Both debug and release builds succeeded. The complete debug suite passed
939 tests with one skipped, including all 21 controller tests. The simulated
startup test now requires convergence within one second rather than two;
new tests check the 90% target cap and that aggressive increases remain bounded
by the predicted target. These small benchmark samples do not establish
performance or pacing improvements on every host or renderer.

A separate traced run reached about 678,000 cycles within its first ten
observation windows (approximately one second). Subsequent workload changes
still triggered the existing overload reduction. That trace-enabled run is
excluded from the timing table above.
