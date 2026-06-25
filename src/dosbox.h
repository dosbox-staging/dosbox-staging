// SPDX-FileCopyrightText:  2019-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DOSBOX_H
#define DOSBOX_DOSBOX_H

#include "dosbox_config.h"
#include "misc/compiler.h"
#include "misc/messages.h"
#include "misc/types.h"

#include <functional>
#include <memory>

// Project name, lower-case and without spaces
#define DOSBOX_PROJECT_NAME "dosbox-staging"

// Name of the emulator
#define DOSBOX_NAME "DOSBox Staging"

// Development team name
#define DOSBOX_TEAM "The " DOSBOX_NAME " Team"

// Copyright string
#define DOSBOX_COPYRIGHT "(C) " DOSBOX_TEAM

// Website URL
#define DOSBOX_WEBSITE "https://www.dosbox-staging.org"

// Address to report bugs
#define DOSBOX_BUGS_TO "https://github.com/dosbox-staging/dosbox-staging/issues"

// Address of the translation manual
#define DOSBOX_MANUAL_TRANSLATION \
	"https://github.com/dosbox-staging/dosbox-staging/blob/main/docs/TRANSLATING.md"

// Fully qualified application ID for the emulator; see
// https://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names
// for more details
#define DOSBOX_APP_ID "org.dosbox_staging.dosbox_staging"


// Must be called to break out of the emulator loop and exit gracefully from
// all emulation layers.
void DOSBOX_RequestShutdown();

bool DOSBOX_IsShutdownRequested();

//  Pause / resume design overview
//  ==============================
//
//  This is the canonical place to read about how pause hangs together.
//  Per-subsystem comments at the use sites cover the local mechanics; this
//  block covers the cross-cutting reasoning, why the sequence is what it is,
//  and why it is safe.
//
//  Goals
//  -----
//  - Hotkeys (fullscreen toggle, screenshot, mapper, capture start/stop,
//    config reload) work in pause mode.
//
//  - Audio cuts to silence on pause with no audible tail.
//
//  - Bit-identical audio and video capture across pause cycles -- a game
//    played continuously and the same game played with arbitrary
//    pause/unpause activity produce identical capture files.
//
//  - HTTP API stays responsive while paused.
//  - IPX connections survive multi-second pauses.
//  - Quit during pause shuts down cleanly.
//
//  The architectural lever: PIC time freezing
//  ------------------------------------------
//  PIC time (`PIC_Ticks` + `PIC_TickIndex()` → `PIC_FullIndex()`) only
//  advances when `normal_loop` calls `PIC_RunQueue` / `TIMER_AddTick`.
//  `normal_loop` early-returns into `paused_tick` while pause is active,
//  so neither runs. As a consequence:
//
//    * VGA / Voodoo vertical timers don't fire; frame stays frozen.
//
//    * Sound Blaster / GUS / AdLib / PC speaker / Tandy / PS-1 tick
//      handlers (`TIMER_AddTickHandler`) don't fire; emitters don't
//      advance.
//
//    * NE2000 polling stops.
//
//    * ReelMagic MPEG advance stops.
//
//    * IDE delayed commands (`PIC_AddEvent(IDE_DelayedCommand, ...)`) do
//      not complete; their scheduled PIC-time delta is preserved across
//      pause and resumes correctly.
//
//    * Voodoo's `frame_start = PIC_FullIndex()` and its retrace queries
//      use `PIC_FullIndex() - frame_start` — both PIC time; both frozen,
//      so the delta is consistent without any explicit rebase.
//
//    * `PIC_AtomicIndex()` (the thread-safe mirror of `PIC_FullIndex`) is
//      updated only inside `PIC_RunQueue`; it is frozen, too. Anything on
//      another thread that timestamps against it (mixer's `last_mixed`,
//      MT-32's `last_rendered_ms` when using the atomic variant) compares
//      frozen-to-frozen and stays consistent.
//
//  In short: most subsystems pause for free once `normal_loop` parks.
//
//  Pause sequence
//  --------------
//   1. Caller invokes `DOSBOX_Pause(reason)` from the main thread (mapper
//      handler for the hotkey, focus-loss handler for window-inactive).
//      This sets `pause_requested`. It does NOT activate pause yet!
//
//   2. The next `VGA_VerticalTimer` / `Voodoo_VerticalTimer` calls
//      `DOSBOX_TryActivatePauseAtVerticalRetrace`, which flips
//      `pause_active = true`. This aligns activation to a video frame
//      boundary, so the last pre-pause video frame is fully rendered,
//      presented, and captured. Doing it any other way risks a half-rendered
//      frame in the video capture and on screen while pause is active.
//
//   3. Next iteration of `normal_loop` sees `pause_active`, dispatches to
//      `paused_tick`, returns 0.
//
//   4. First entry into `paused_tick` initialises subsystems: mixer to
//      `Paused`, MIDI synths to condition-variable-wait (cv-wait), IMFC main
//      thread to cv-wait, external MIDI muted.
//
//   5. Subsequent `paused_tick` entries just keep the frame presenter and the
//      HTTP bridge running, plus a 1 ms sleep to avoid burning CPU cycles in
//      a busy-wait loop.
//
//  Why the mixer's Paused state must short-circuit BEFORE `mix_samples`
//  -------------------------------------------------------------------
//  Pull-model channels (MT-32, FluidSynth, SoundCanvas) have callbacks
//  that call `audio_frame_fifo.BulkDequeue`, which is a blocking dequeue. If
//  the mixer runs `mix_samples` while the synth render threads are cv-waiting
//  on their pause flag, the FIFO empties and the mixer thread blocks forever
//  on `BulkDequeue` (nobody is refilling). The `MixerState::Paused` branch
//  therefore lives ABOVE the `mix_samples` call site in `mixer_thread_loop`.
//  This is a correctness requirement, not an optimisation.
//
//  Synth render thread pause pattern
//  ---------------------------------
//  Each internal synth (`MidiDeviceMt32`, `MidiDeviceFluidSynth`,
//  `MidiDeviceSoundCanvas`) owns a `pause_flag` (atomic bool) plus a
//  `pause_mutex` / `pause_cv` pair. The render thread cv-waits at the top of
//  its loop while the flag is set. Predicate:
//
//      !pause_flag || !work_fifo.IsRunning()
//
//  The `work_fifo.IsRunning()` disjunct is defence-in-depth for shutdown
//  paths; in practice `DOSBOX_RequestShutdown` force-resumes (see below) so
//  the synth wakes via flag-clear and shuts down normally.
//
//  Synth states at the pause moment and what happens:
//
//   - At top of loop: cv predicate becomes false immediately, thread blocks.
//     No emulated state advances.
//
//   - Mid-`renderFloat` (or equivalent): completes the call uninterruptibly
//     (libmt32emu / FluidSynth / CLAP plugin), then `BulkEnqueue`s. If
//     `audio_frame_fifo` is full, blocks there. If not, returns to top of
//     loop and cv-waits. Either way the synth's internal time cursor does not
//     advance past the in-flight block.
//
//   - Blocked in `BulkEnqueue` (FIFO full): stays blocked for the duration of
//     the pause (benign). Mixer is in `Paused`, not draining. On resume,
//     mixer goes back to On, drains the FIFO, `BulkEnqueue` completes, the
//     synth applies its (pre-pause-timestamped) in-flight event, returns to
//     top, sees the cleared flag, continues.
//
//   - Mid-event-apply (`playMsg` / `playSysex` / CLAP `Process`):
//     completes the call, returns to top of loop, cv-waits. The
//     pre-pause event is fully applied to synth state (correct).
//
//  No state drift accumulates across any number of pause cycles because:
//  pre-pause emitted events are applied on resume, audio in the FIFO is
//  preserved and consumed correctly, and prebuffer depth self-corrects via
//  the natural synth-refill / mixer-drain feedback loop.
//
//  IMFC handles its own emulated Z80: the main thread runs the music-mode
//  dispatch loop and cv-waits between dispatches. The interrupt thread
//  already cv-waits on `m_interruptHandlerRunning` and idles naturally during
//  pause (no DOSBox CPU writes to IMFC ports reach the port handlers while
//  the CPU is parked in `paused_tick`).
//
//  Resume sequence — order matters
//  -------------------------------
//   1. Rebase wall-clock ticks accounting (see below).
//   2. Unmute external MIDI.
//   3. Clear MIDI synth pause flags + notify cvs.
//   4. Clear IMFC pause flag + notify cv.
//   5. Mixer back to its pre-pause state (On or Muted) -- LAST!
//   6. Clear the pause state atomics.
//
//  Why mixer goes LAST: if the mixer were resumed first, it would start
//  draining `audio_frame_fifo` immediately. The synth render threads would
//  still be cv-waiting at the top of their loop and not refilling. The FIFO
//  would briefly underflow until the synths woke. Doing synth resume first
//  means the synths are ready to refill the moment the mixer begins draining.
//
//  For the case where a synth is blocked in `BulkEnqueue` (not on the cv):
//  clearing the synth pause flag is irrelevant to that thread — it is past
//  the cv check. What unblocks it is the mixer draining the FIFO. The
//  ordering above still works: the synth that was on the cv wakes first and
//  proceeds; the synth that was in `BulkEnqueue` only unblocks once the mixer
//  starts draining, which happens last. No deadlock either way.
//
//  Wall-clock baselines: what gets rebased, what gets converted
//  ------------------------------------------------------------
//  PIC time is frozen during pause, but wall-clock keeps ticking on the host.
//  Two distinct fixes are needed:
//
//   - Wall-clock baselines that the CPU tick accounting uses (`ticks.last`,
//     `ticks.added`, `ticks.done`, `ticks.scheduled`) MUST be reset on resume
//     — otherwise `increase_ticks` sees a huge gap and tries to "catch up" by
//     running tens of seconds of emulation in a burst.
//     `rebase_wall_clock_on_resume()` handles this.
//
//   - Wall-clock-based TIMEOUTS that span pause must be converted to PIC
//     time, otherwise they false-fire across the pause. Call sites:
//
//       * `ipx.cpp` server-connect timeout
//       * `ipx.cpp` ping reply timeout
//       * `misc_util.cpp` ENET modem connect timeout
//       * `midi.cpp` sysex pacing baseline (`midi.sysex.start_pic_index`)
//
//  Other apparent wall-clock baselines that turned out to need no fix: the
//  frame presenter's `last_present_time_us` updates naturally because
//  `GFX_MaybePresentFrame` runs every `paused_tick` iteration.
//
//  `last_check_joystick` updates naturally inside `GFX_PollAndHandleEvents`.
//  The mixer's `last_mixed` compares against `PIC_AtomicIndex`; both frozen,
//  both rebased implicitly.
//
//  Host input events that keep flowing during pause
//  ------------------------------------------------
//  `paused_tick` keeps calling `GFX_PollAndHandleEvents` so hotkeys (mapper,
//  fullscreen toggle, screenshot, capture, config reload) and window events
//  remain responsive. Most input subsystems are inherently safe across this:
//
//    * Joystick / gamepad axes are polled per-tick via
//      `SDL_GetJoystickAxis`; no delta accumulator. An axis held at full
//      deflection pre-pause and released during pause reads at its current
//      position on resume.
//    * Keyboard events go through `MAPPER_CheckEvent` -> `KEYBOARD_AddKey`;
//      releases reach the emulated keyboard during `paused_tick`, so
//      press/release pairs stay matched across the pause.
//
//  The mouse needed an explicit drop:
//    * `MOUSE_NotifyEmulatorPaused(true)` is set inside
//      `initialise_pause_on_first_tick` and cleared on resume. Without it,
//      host mouse motion during pause would accumulate into the DOS
//      driver's `pending.x_rel` / `pending.y_rel` and deliver as a single
//      large delta on resume, jumping the in-game cursor. Button releases
//      still pass through unconditionally, matching the existing
//      `MOUSE_EventButton` convention.
//
//  Window / display events that arrive during pause
//  ------------------------------------------------
//  `handle_sdl_windowevent` keeps processing `WINDOW_RESTORED`,
//  `WINDOW_PIXEL_SIZE_CHANGED`, `WINDOW_DISPLAY_CHANGED` during pause, and
//  each of those calls `GFX_ResetScreen` (which mutates `vga.draw` geometry).
//  The frame presenter, meanwhile, keeps showing the pre-pause framebuffer,
//  so for the duration of the pause `vga.draw` briefly describes a different
//  geometry than what's on screen. `DOSBOX_Resume` runs one final
//  `GFX_ResetScreen` after the subsystem resumes so the first post-resume
//  frame uses the freshest viewport / DPI / mode state -- this covers
//  window resize, fullscreen toggle, host DPI change, and moving the window
//  to a different display while paused.
//
//  Mapper UI hotkey works during pause
//  -----------------------------------
//  `MAPPER_Run` used to defer opening the mapper UI via
//  `PIC_AddEvent(MAPPER_RunEvent, 0)` so the binding-dispatch loop in
//  `MAPPER_CheckEvent` could unwind before `MAPPER_DisplayUI` (which can
//  delete the very binding that fired it) ran. PIC events don't fire while
//  paused, which silently broke the mapper hotkey there. The deferral now
//  uses a plain atomic flag drained by `MAPPER_RunPending()` at the bottom
//  of `normal_loop`'s idle branch and at the bottom of `paused_tick` --
//  after `GFX_PollAndHandleEvents` has returned, so binding-object lifetime
//  is still safe. The mapper UI now opens regardless of pause state, and
//  if it opens over an already-paused emulator the audio subsystems stay
//  paused on mapper exit (the eventual user un-pause undoes them).
//
//  Shutdown during pause: force-resume
//  -----------------------------------
//  `DOSBOX_RequestShutdown` force-resumes if currently paused. This is
//  required, not optional. `RWQueue::Stop()` notifies its internal
//  `has_items` / `has_room` condition variables, but it does NOT notify the
//  per-device `pause_cv`. A synth render thread cv-waiting on `pause_cv`
//  would not wake just because `work_fifo.Stop()` ran. Force- resume clears
//  every pause flag and notifies every cv, the cv-waiting synth wakes into
//  normal-running state, and its destructor then runs `work_fifo.Stop()` /
//  `audio_frame_fifo.Stop()` which wake any `BulkEnqueue` / `BulkDequeue`
//  waiters via the normal path.
//
//  `paused_tick` also checks `DOSBOX_IsShutdownRequested()` and returns early
//  so the CPU thread doesn't spin in `paused_tick` while the rest of the
//  emulator is tearing down.
//
//  Pause reason and the WindowInactive → UserRequested upgrade
//  -----------------------------------------------------------
//  Focus-loss handlers call `DOSBOX_Pause(WindowInactive)`. Focus-gain only
//  auto-resumes when the current reason is still `WindowInactive`. If the
//  user explicitly pressed the pause hotkey while the window was inactive,
//  `DOSBOX_Pause(UserRequested)` upgrades the recorded reason; focus-restore
//  then leaves the emulator paused until the user explicitly resumes via the
//  hotkey. This is the only branch of pause state-machine logic that doesn't
//  fall out of pure flag manipulation, and it lives in `DOSBOX_Pause` itself.
//
//  Host filesystem I/O via BIOS/DOS interrupts
//  -------------------------------------------
//  DOS INT 21h and BIOS INT 13h handlers run synchronously on the CPU thread
//  between `cpudecoder` iterations. Pause activation only happens at the top
//  of the next `normal_loop` iteration, never mid-handler. A blocking host
//  `read()` / `write()` always completes and the emulated DOS state is fully
//  consistent before pause kicks in. The only observable effect is that pause
//  may have up to one-I/O-call worth of latency on a slow filesystem
//  (acceptable).
//
//  What is intentionally NOT addressed here
//  ----------------------------------------
//   - PIC scheduling cleanup: `PIC_AddEvent` is used both as the real
//     8259 PIC emulation AND as a general emulator event scheduler.
//     That dual use is real but orthogonal to pause. Detailed audit
//     of the call sites and the migration shape for a future cleanup
//     PR lives in `pic-plan.md` at the repo root.
//
//   - Debugger pause: the debugger's `DOSBOX_SetLoop(&DEBUG_Loop)` path is
//     intentionally independent of this state machine. The debugger
//     wants different semantics: stepping with `DEBUG_Run` actually
//     advances `cpudecoder` (and thereby PIC sub-tick time), while
//     `paused_tick` parks the CPU completely. Routing the debugger
//     through `DOSBOX_Pause` would also need a "force-activate" path,
//     since `VGA_VerticalTimer` doesn't fire while `DEBUG_Loop` is the
//     loop handler and the normal vertical-retrace activation never
//     gets a chance to flip `pause_active`. Audio muting for the
//     debugger is a `// TODO Disable sound` left in `DEBUG_Loop` and
//     is a separate, smaller piece of work to do without touching this
//     state machine.
//
//   - External MIDI long notes: `MIDI_Mute()` sends CC 7 = 0 per channel but
//     does not truncate envelopes on real hardware. Unavoidable.
//
enum class PauseReason : uint8_t {
	None,
	UserRequested,
	WindowInactive,
};

bool DOSBOX_IsPaused();
PauseReason DOSBOX_GetPauseReason();
void DOSBOX_Pause(PauseReason reason);
void DOSBOX_Resume();

// Called from VGA_VerticalTimer / Voodoo_VerticalTimer to flip pause
// from "requested" to "active" on a frame boundary. Aligns pause
// activation to vsync so the last pre-pause frame is fully rendered
// and captured.
//
void DOSBOX_TryActivatePauseAtVerticalRetrace();

// The E_Exit function throws an exception to quit. Call it in unexpected
// circumstances.
[[noreturn]] void E_Exit(const char *message, ...)
        GCC_ATTRIBUTE(__format__(__printf__, 1, 2));

class Section;
class SectionProp;

typedef Bitu (LoopHandler)(void);

void DOSBOX_Init();
void DOSBOX_Destroy();

void DOSBOX_InitModuleConfigsAndMessages();
void DOSBOX_InitModules();
void DOSBOX_DestroyModules();

const char* DOSBOX_GetVersion() noexcept;
const char* DOSBOX_GetDetailedVersion() noexcept;

void DOSBOX_RunMachine();
void DOSBOX_SetLoop(LoopHandler * handler);
void DOSBOX_SetNormalLoop();

void DOSBOX_SetMachineTypeFromConfig(SectionProp& section);

int64_t DOSBOX_GetTicksDone();
void DOSBOX_SetTicksDone(const int64_t ticks_done);
void DOSBOX_SetTicksScheduled(const int64_t ticks_scheduled);

void DOSBOX_Restart();
void DOSBOX_Restart(std::vector<std::string>& parameters);

enum class MachineType {
	// Value not set yet
	None,

	// PC XT with Hercules
	Hercules,

	// PC XT with CGA and monochrome monitor
	CgaMono,
	// PC XT with CGA and color monitor
	CgaColor,
	// IBM PCjr
	Pcjr,
	// Tandy 1000
	Tandy,

	// PC AT with EGA
	Ega,
	// PC AT with VGA or SVGA
	Vga
};

enum class SvgaType {
	// Non-SVGA card
	None,
	// S3 Graphics series (emulated card is mostly Trio64 compatible)
	S3,
	// Tseng Labs ET3000
	TsengEt3k,
	// Tseng Labs ET4000
	TsengEt4k,
	// Paradise Systems PVGA1A
	Paradise
}; 

extern MachineType machine;
extern SvgaType    svga_type;

inline bool is_machine_svga() {
	return svga_type != SvgaType::None;
}

inline bool is_machine_vga_or_better() {
	return machine == MachineType::Vga;
}

inline bool is_machine_ega() {
	return machine == MachineType::Ega;	
}

inline bool is_machine_ega_or_better() {
	return is_machine_ega() || is_machine_vga_or_better();
}

inline bool is_machine_tandy() {
	return machine == MachineType::Tandy;
}

inline bool is_machine_pcjr() {
	return machine == MachineType::Pcjr;
}

inline bool is_machine_pcjr_or_tandy() {
	return is_machine_pcjr() || is_machine_tandy();
}

inline bool is_machine_cga_mono() {
	return machine == MachineType::CgaMono;
}

inline bool is_machine_cga_color() {
	return machine == MachineType::CgaColor;
}

inline bool is_machine_cga()
{
	return is_machine_cga_mono() || is_machine_cga_color();
}

inline bool is_machine_cga_or_better() {
	return is_machine_cga() ||
               is_machine_pcjr_or_tandy() ||
               is_machine_ega_or_better();
}

inline bool is_machine_hercules() {
	return machine == MachineType::Hercules;
}

#ifndef DOSBOX_LOGGING_H
#include "misc/logging.h"
#endif // the logging system.

constexpr auto DefaultMt32RomsDir        = "mt32-roms";
constexpr auto DefaultSoundCanvasRomsDir = "soundcanvas-roms";
constexpr auto DefaultSoundfontsDir      = "soundfonts";
constexpr auto DefaultWebserverDir       = "webserver";
constexpr auto DiskNoisesDir             = "disk-noises";
constexpr auto PluginsDir                = "plugins";
constexpr auto ShaderPresetsDir          = "shader-presets";
constexpr auto ShadersDir                = "shaders";

constexpr auto MicrosInMillisecond = 1000;
constexpr auto BytesPerKilobyte    = 1024;
constexpr auto BytesPerMegabyte = BytesPerKilobyte * 1024;
constexpr int64_t BytesPerGigabyte = BytesPerMegabyte * 1024;

enum class DiskSpeed { Maximum, Fast, Medium, Slow };

#endif // DOSBOX_DOSBOX_H
