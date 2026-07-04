# GPU backend learning companion

This document grows alongside the native GPU backend work
(see `native-gpu-backends-plan.md`). The deal: **every commit in the series
appends a chapter** explaining the concepts that commit touches, why
the code looks the way it does, and where to read more. It is written
to be read like a textbook — in order the first time, then dipped into
as reference. Chapters are allowed to be long: depth and narrative
are features here, not padding — especially for experiments and for
what we dig out of other projects' source. Part I covers the foundations you need before the first
line of backend code; Part II records what we learned from studying
existing implementations, from the spikes, and from the ecosystem
review that reshaped the plan. Per-commit chapters follow from
Part III onwards.

A note on reading order: the chapters are a chronological record,
and the plan changed underneath them as experiments and reviews came
in — most dramatically in Chapter 7, where the native Metal backend
is deleted. Earlier chapters are left saying what we believed when
they were written, with bracketed notes marking the spots that later
chapters supersede. Watching the design bend under evidence is part
of the curriculum.

---

## Part I — Foundations

### Chapter 1: How a frame actually reaches your screen

It is easy to spend years doing OpenGL and never once think about what
happens after `SwapBuffers`. The new backends force us to care, so
let's build the full picture once, properly.

#### The swapchain

Your GPU cannot draw directly "to the screen". The screen is fed by a
piece of scanout hardware — the *display engine* — which continuously
reads some region of video memory, top to bottom, and turns it into
the signal your monitor receives. Drawing into the exact memory being
scanned out produces tearing: the top half of the screen shows the old
frame, the bottom half the new one, with a visible seam wherever the
scanout beam happened to be when you wrote.

The fix is double buffering, and its modern generalisation is the
**swapchain**: a small ring of images (typically 2–3) shared between
your application and the display engine. At any moment, one image is
being scanned out; you draw into another; a third might be queued,
waiting its turn. "Presenting" means handing your finished image to
the display engine and asking for the next free one.

OpenGL hides all of this behind `SwapBuffers` — the driver owns the
ring, and when you outrun it, the driver simply blocks your thread
inside some innocent-looking GL call until an image frees up. You
cannot ask "would this block?"; you find out by blocking. Vulkan and
Metal invert this: the swapchain is an object you create, its images
are textures you render into, and the two halves of presentation —
*acquire* an image, *present* a finished one — are explicit calls with
inspectable results. All the waiting is in the acquire, and Vulkan
lets you pass a timeout — including zero, meaning "if nothing is free,
tell me and I'll skip". That single capability is why our emulation
loop will never again stall on the display.

#### Present modes: FIFO, IMMEDIATE, MAILBOX

The *present mode* decides what the display engine does when frames
arrive faster than the monitor consumes them.

- **FIFO** (what "vsync on" means in Vulkan): presented images wait in
  a queue; the display takes exactly one per refresh. Never tears. If
  you produce faster than the display drains, the queue fills up and
  back-pressure reaches you — in our design, as a failed acquire we
  can skip on.
- **IMMEDIATE**: your new frame replaces the one being scanned out,
  right now, mid-scanline. Never waits, may tear.
- **MAILBOX**: a one-slot mailbox. Presenting replaces whatever is
  already queued if the display hasn't taken it yet. Never blocks,
  never tears; excess frames are silently dropped. Think of it as
  "vsync without back-pressure". Not universally supported, which is
  why it's a fallback in our mapping, not a primary.

#### Variable refresh rate changes the rules

A classic monitor refreshes on a fixed clock — 60 Hz, come what may.
A VRR display (G-Sync, FreeSync, adaptive sync) instead starts a
refresh **when a frame arrives**, within its supported range (say
48–144 Hz). This is a profound inversion: the display no longer has a
schedule you must hit; *your presents are the schedule*.

For DOSBox this is the perfect match. Mode 13h runs at 70.086 Hz — a
rate no fixed monitor refreshes at — and on VRR, if we present a
frame every 14.268 ms, the monitor genuinely refreshes at 70.086 Hz
and Commander Keen scrolls exactly as smoothly as it did on a real
VGA CRT. The whole pacing effort in this project reduces to one
sentence: *submit presents at precisely the emulated cadence, and let
the VRR display follow.*

Two boundary conditions to keep in mind. Above the VRR range (say the
DOS rate exceeded a 60 Hz-max panel), FIFO throttles you and IMMEDIATE
tears — our skip-on-acquire design degrades this to clean frame drops
instead. And below the range, the panel re-shows frames on its own;
nothing for us to do.

#### The compositor ceiling

One more actor sits between you and the glass: the desktop compositor
(DWM on Windows, the Wayland compositor, WindowServer on macOS). When
your output is a window among windows, the compositor samples your
latest frame on *its* schedule and composites it into the desktop —
your careful 70.086 Hz cadence gets resampled to the compositor's
fixed rhythm, and no API can fully prevent it. When your window covers
the whole screen, all three platforms have a fast path (independent
flip / direct scanout) that hands your swapchain straight to the
display engine, compositor bypassed.

The honest consequence, which every emulator lives with: **first-class
pacing is a fullscreen feature.** Windowed mode is best-effort, and we
document rather than fight it.

#### Frames in flight, and why fewer is better for us

Because the GPU runs asynchronously, the CPU can record and submit
frame N+1 while the GPU still renders frame N. "Frames in flight" is
how far ahead we allow that to run. Games want 2–3 for throughput —
bubbles in the pipeline cost them frame rate. We want the opposite:
our GPU work is trivial (a few shader passes over a tiny texture), and
every queued frame is added latency between the emulated moment and
the photons. We default to 2 and will experiment with 1 on VRR; the
per-frame fence/semaphore structure that implements this is Chapter 4
material.

### Chapter 2: The pacing problem

#### Where DOSBox's timing comes from

The emulator is real-time-locked: one emulated millisecond per wall
millisecond, enforced by the main loop's tick governor. Within a tick,
though, emulation runs as a *burst* — a fast host chews through the
whole millisecond's work in a fraction of it, then sleeps until the
next tick boundary. Events scheduled inside the tick (like the VGA
vertical retrace that ends a frame) therefore *execute* earlier than
the wall-clock moment they represent, by up to a millisecond.

Today's dos-rate presentation fires inside that vretrace event, so its
wall-clock timing inherits the burst jitter — the existing code
comments describe 1–5 ms of it. On VRR, present-time jitter *is*
frame-interval jitter on the glass. VRR absorbs a lot, but "absorbs a
lot" is not "microsecond-accurate", and the code comment at
`GFX_EndUpdate` explicitly wishes for better. That wish is this
project.

#### Two ways to present at the right time

**App-timed** (PR 1, the fallback tier): don't present inside the
vretrace event; instead compute the *due* wall-clock time of that
emulated moment — the tick's base time plus the fraction of the tick
already emulated (`PIC_TickIndex()`) — and submit the present when the
clock actually reaches it. The main loop already polls several times
per millisecond, so this lands within ~0.2–0.5 ms of the target,
without threads or sleeps. It also helps the existing OpenGL and SDL
texture backends today — it sits above the backend interface, so
every backend's presents get the due-time treatment — which is why it
comes first.

**Display-engine-timed** (PR 6, the flagship): attach the target
timestamp to the present itself and let the *display engine* hold the
flip until that exact time. This is `VK_EXT_present_timing` on Vulkan
(spec-merged November 2025 after five years of gestation) and
`present(atTime:)` on Metal (a decade old and rock solid). Accuracy
becomes microsecond-class and — the subtle win — *independent of main
loop load*: the vretrace event can submit immediately, mid-burst, and
the flip still lands on target.

Both extensions also give **feedback**: the actual time each frame hit
the glass. Feedback is what turns pacing from a vibe into a
measurement — we can log the real flip intervals, detect missed
windows, and correct drift between the emulated clock and the
display's. A little humbling discovery from the reference study
(Chapter 4): no shipping emulator schedules presents this way yet;
PPSSPP uses the older timing extensions for measurement only. We are
early adopters, which is why the app-timed tier stays load-bearing.

#### Latency accounting

A frame's journey: emulated vretrace → (wait until due) → submit →
GPU renders (trivial for us) → display engine flips at the target →
scanout. With frames-in-flight kept low and presents scheduled to the
emulated cadence, the end-to-end latency is nearly constant — which
matters as much as the average: consistent latency reads as "solid",
oscillating latency reads as judder, even at the same mean.

### Chapter 3: The shader toolchain

#### One source, three consumers

The project keeps a single hand-maintained shader format: **Vulkan
GLSL, `#version 450`**. Three backends consume it three ways:

- **Vulkan**: GLSL → SPIR-V at runtime, via glslang.
- **Metal**: GLSL → SPIR-V → MSL at runtime, via glslang +
  SPIRV-Cross.
- **OpenGL**: GLSL 3.30 files *generated offline* by a script and
  checked into the repo, so the GL backend keeps needing no toolchain
  at runtime.

*(Superseded in Chapter 7: the Metal consumer was deleted along with
the Metal backend. macOS now runs the Vulkan backend on MoltenVK,
which consumes the same SPIR-V as any Vulkan driver — no MSL is ever
generated by us, and SPIRV-Cross's only remaining role is the offline
OpenGL generation.)*

#### What SPIR-V actually is

SPIR-V is a binary intermediate representation for shaders — think of
it as the "object code" of the GPU world, except portable. Where GLSL
is text parsed by each vendor's compiler (with each vendor's parser
bugs — a genuine plague of the OpenGL era), SPIR-V is a simple,
well-specified list of typed instructions in static single assignment
form. The driver still compiles it to actual GPU machine code, but the
error-prone front-end work — parsing, name resolution, constant
folding — happens once, in tooling we control.

Two properties matter to us daily. First, SPIR-V preserves *names and
layout metadata* (unless you strip them), which is what lets tools
reflect over a compiled shader — "this uniform block has a member
`OUTPUT_SIZE` at byte offset 0" — and what lets SPIRV-Cross
reconstruct readable source in other languages. Second, it is the
common interchange point of the whole ecosystem: glslang compiles *to*
it, SPIRV-Cross translates *from* it, Vulkan consumes it natively.

#### The two GLSL dialects

Vulkan GLSL (`#version 450` under Vulkan rules) differs from the
OpenGL 3.30 GLSL our shaders were written in, in ways that are almost
entirely mechanical:

- Loose `uniform vec2 OUTPUT_SIZE;` declarations do not exist; all
  non-sampler uniforms live in **uniform blocks** with explicit
  `std140` layout and a `set`/`binding` address.
- Samplers also get explicit `set`/`binding` addresses.
- Varyings need explicit `layout(location = N)` so the stages link by
  number, not by name.
- A few new reserved words appear — the spike (Chapter 5) immediately
  tripped over `sampler` used as a parameter name.

The bodies — the actual mathematics of a CRT shader — do not change.
That's why converting 21 shaders is a recipe, not a rewrite.

#### std140, in one paragraph

`std140` is the standardised memory layout for uniform blocks: scalars
align to 4 bytes, `vec2` to 8, `vec3`/`vec4` to 16, arrays round each
element up to 16. It exists so the CPU can compute member offsets
without asking the driver. We go one step safer: the executor gets its
offsets from **SPIRV-Cross reflection** over the compiled SPIR-V (the
compiler tells us where it actually put things), and our canonical
member order is just a convention we cross-check in debug builds.
RetroArch has run this exact scheme for years.

#### glslang and SPIRV-Cross

**glslang** is Khronos's reference GLSL front-end: source string in,
SPIR-V out. We use it at runtime (compilation happens only on shader
switches, costing single-digit milliseconds) and in the offline
generation script. One operational note from the reference study: it
has one-time global initialisation and is not thread-safe — trivial
for our single-threaded use, but worth knowing.

**SPIRV-Cross** translates SPIR-V *back* into high-level languages —
MSL for Metal, GLSL 3.30 for our OpenGL backend — and doubles as our
reflection library. Its GLSL output has knobs that mattered enough to
spike-test; Chapter 5 has the receipts, including the `--flatten-ubo`
mode that turned out to be the key to macOS compatibility.

---

## Part II — Field notes

### Chapter 4: How the pros build native backends

Before writing our own Vulkan and Metal code, we read six shipping
emulators' render backends (Dolphin, PPSSPP, RetroArch, DuckStation,
PCSX2, RPCS3 — licences vary; the plan records which are
patterns-only) and the official Khronos samples. This chapter is the
narrative version; the plan's Appendix C has the file:line
references. Everything we end up adopting gets credited at the
adoption site in our code too — the plan's attribution policy makes
that a series-wide rule, in the same spirit as the upstream-author
SPDX credits our shader files already carry.

#### The shape everyone converges on

Strip away each project's game-rendering machinery and the same
skeleton appears: a *context* (instance, device, queues), a
*swapchain* object owning recreation logic, a *per-frame ring* of
command buffers and sync primitives, an *upload* path, and a *pipeline
cache/executor*. Dolphin's Vulkan backend spells this out most
readably (~10k lines with all the game-state we don't need; our
fixed-function needs pencil out at a third of that). Its Metal backend
is less than half the size of the Vulkan one — a preview of how the
second backend will feel after the first hardens the shared structure.

#### Six projects, six personalities

Reading that much code back to back, each project develops a distinct
character, and knowing them helps you know *which* reference to open
when a given problem bites:

- **Dolphin** is the textbook. Clean class boundaries, honest names,
  a `VKSwapChain` you can read top to bottom in one sitting. When we
  need the canonical shape of anything, we open Dolphin first.
- **PPSSPP** is the timing nerd. It's the only project that even
  *measures* presentation properly, and its code around frame timing
  reads like someone who has spent years being personally offended by
  micro-stutter. Its per-frame timing-history ring is the direct
  model for our pacing logs.
- **RetroArch** is the scarred veteran. Its WSI code is not pretty,
  but every ugly branch has a comment naming the vendor and the year
  it earned its place. It even ships a fuzzing mode that injects fake
  swapchain failures to test its own recovery — the single best
  testing idea we found anywhere.
- **DuckStation** is the architect. Five APIs behind one compact
  interface, bitfield-packed pipeline state keys, and the only
  shipping `presentDrawable:atTime:` in the fleet. The closest thing
  to a blueprint of what we're building.
- **PCSX2** is the craftsman's Metal shop — the renderer people point
  to when they say Metal emulation done right — full of small,
  hard-won macOS specifics (like MAILBOX quietly not working windowed
  before Ventura).
- **RPCS3** is the field medic. It emulates a real GPU under hostile
  conditions and its swapchain code is mostly triage: which vendors
  lie about present timing, which drivers spam errors when minimised,
  how long you may safely wait for anything. Its per-vendor trust
  table goes straight into our feedback validation.

#### The swapchain dance, and its traps

The canonical loop: acquire an image (a semaphore will signal when
it's really ready), record commands, submit them (waiting on that
semaphore, signalling a second one, and signalling a fence for the
CPU), then present (waiting on the second semaphore). Two frames of
these primitives, used round-robin, with the fence as the CPU's
throttle.

A short digression on why the *acquire semaphore* is the subtlest
object in that list, because it will matter when you read our ring
code. A fence tells the CPU when work finished; a semaphore
synchronises GPU-internal stages, and the CPU can never query it.
When `vkAcquireNextImageKHR` returns, the image is *not* ready — the
call only promises that the semaphore you handed it will signal when
the display engine is truly done with the image. That means you
cannot reuse an acquire semaphore until you *know* its signal was
consumed by a submit that has itself provably completed — knowledge
you only get transitively, through a fence. Get this wrong and
nothing fails on your machine; it fails on someone else's, months
later, as a once-a-week flicker. This is why DuckStation sizes its
acquire-semaphore pool to command buffers *plus one*, and why the
Khronos sample recycles semaphores through explicit pools tied to
present-completion fences. Our ring copies that discipline exactly.

The traps are all in *recreation*. A resize, a vsync toggle, or the
driver simply declaring the swapchain `OUT_OF_DATE` means tearing the
ring down and rebuilding it — while the GPU may still be using it.
The field wisdom we're adopting: treat `SUBOPTIMAL` as "finish this
frame, rebuild lazily"; skip everything while the window is 0×0
(minimised); gate recreation behind an "anything actually changed?"
check; hand the old swapchain to the new one's create-info and defer
destruction until its last frame's fence signals. RetroArch — the most
platform-scarred of the three — adds two memorable habits: never wait
on the device while holding your queue lock (a Windows timeout-
detection story lies behind that comment), and put a *finite* timeout
on any blocking acquire so a wedged driver can't hang you forever.
They even have a fuzzing mode that injects fake `OUT_OF_DATE` errors
to exercise the recovery paths — an idea worth stealing for tests.

One genuinely modern improvement the samples taught us:
`VK_EXT_swapchain_maintenance1` lets a swapchain switch present modes
(vsync on/off) *without* recreation, and gives each present its own
fence so semaphores can be recycled correctly. We use it when
present, and fall back to Dolphin-style full recreation when not.

#### Uploads and descriptors: where we get to be simpler

The emulators stream large, varied texture traffic, so they build
fence-tracked ring buffers (Dolphin reserves 32 MB) and reset whole
descriptor pools every frame. Our workload is one small texture,
70 times a second, through a fixed pass graph. That means: a single
persistent staging buffer instead of a ring, and **static descriptor
sets** rewritten only when the pipeline is rebuilt — no per-frame
descriptor churn at all. Reading the pros is partly about learning
which of their complexities we get to *not* have.

#### The pacing surprise

The headline finding: **nobody schedules presents on Vulkan yet —
but DuckStation already does on Metal.** Dolphin and RetroArch pace
purely by present-mode choice and blocking acquires. PPSSPP — the
project with the deepest pacing culture — uses
`VK_GOOGLE_display_timing` and `VK_KHR_present_wait`, but only to
*measure* when frames hit the glass (they keep a tidy ring of
per-frame timing records that our pacing logs will imitate), not to
request flip times. DuckStation, though, takes an optional present
time, converts it from host time to mach absolute time, and calls
`presentDrawable:atTime:` — precisely the Metal half of our design,
shipping in a popular emulator today. So the Metal path has a proven
shape to follow, while our `VK_EXT_present_timing` scheduling is
genuinely new territory. That cuts both ways: no Vulkan pattern to
crib, and a real chance first-generation drivers have bugs no one has
hit — which is exactly why the app-timed scheduler tier and the
feedback-driven verification are load-bearing parts of the plan, not
nice-to-haves.

DuckStation earned its place in the study in other ways too. Its
five-API `GPUDevice` layer is the closest published relative of our
`RenderBackend` shape, and despite text-generating most of its core
shaders per language, its portable path runs glslang → SPIR-V →
SPIRV-Cross at runtime — an independent confirmation of the exact
toolchain we picked. It also contributed two fine-grained correctness
patterns to our design notes: image-acquire semaphores must outnumber
command buffers by one before they can be recycled safely, and
acquiring the next swapchain image immediately after presenting (a
DXVK trick) overlaps CPU work with present latency.

The last two additions to the study, PCSX2 and RPCS3, mostly
confirmed the picture — and added one finding that will matter a lot
in the present-timing work: RPCS3 maintains a per-vendor list of
which drivers *report present timing truthfully* (NVIDIA and Intel
yes; AMD's drivers and MoltenVK no). Feedback you cannot trust is
worse than no feedback, so our drift-correction loop will validate
what the driver tells it before acting on it. RPCS3 also documents,
in a weary code comment, that NVIDIA's driver floods you with
`OUT_OF_DATE` errors while a window is minimised and crashes if you
take the bait and recreate — the sixth project to teach us that
swapchain code earns its scars one vendor at a time.

#### Metal, mercifully

Dolphin's Metal backend reads like a holiday after the Vulkan one: no
descriptor sets, no render-pass objects, no image layout transitions,
unified memory instead of staging buffers, and vsync toggled by a
property on the layer. Even the failure modes rhyme with our design —
`nextDrawable` simply returns nil when nothing is available, the exact
analogue of our skip-on-acquire. The plan's "Vulkan first" order is
partly this: solve the hard shape once, then enjoy the easy port.

*(Superseded in Chapter 7: the easy port never happens — the Metal
backend was deleted in favour of running the Vulkan backend on
MoltenVK. Dolphin's Metal observations stay useful all the same,
because Metal is what MoltenVK speaks underneath: `nextDrawable`'s
nil is still where our skipped presents ultimately come from on
macOS.)*

### Chapter 5: The toolchain spike, step by step

Theory is nice; we ran the pipeline. Subject: `interpolation/sharp.glsl`,
hand-converted to `#version 450`. Tools: glslang 16.3.0 and SPIRV-Cross
(2026-04-30) from Homebrew.

#### The conversion

Following the recipe from Chapter 3: uniforms into one anonymous
`std140` block (`set = 0, binding = 0`), the sampler to
`binding = 1`, `layout(location)` on the varyings, `#pragma` comment
block untouched. First compile attempt:

```
ERROR: sharp450.glsl:51: '' : syntax error, unexpected SAMPLER
```

Lesson one, immediately: `sampler` became a reserved word in 450, and
the original shader has a function parameter named exactly that.
Renamed to `tex`; the recipe gained a rule. This is why we spike.

#### The commands that work

```
glslang -V -DVERTEX=1   -S vert -o sharp.vert.spv sharp450.glsl
glslang -V -DFRAGMENT=1 -S frag -o sharp.frag.spv sharp450.glsl
```

Our single-file `VERTEX`/`FRAGMENT` section trick survives intact —
one file, compiled twice with a define and a forced stage.

#### The macOS plot twist

The obvious GL emission, `spirv-cross --version 330 --no-es`, produces
a faithful shader — names preserved, mathematics identical — but
declares the uniform block as a real UBO with a
`layout(binding = 0)` qualifier. That qualifier needs the
`GL_ARB_shading_language_420pack` extension: ancient and universal on
Linux and Windows… and **not present in Apple's OpenGL**. Our GL
backend must keep working on macOS for years yet.

The fix was hiding in SPIRV-Cross's option list: `--flatten-ubo`
emits the whole block as

```glsl
uniform vec4 Uniforms[1];
```

— a plain 3.30 uniform array, no extensions, set with a single
`glUniform4fv` call whose data is *bit-identical to the std140 buffer
the native backends upload*. One packing function serves all three
backends. Even better, the flattened name comes from the block *type*
(`Uniforms`), which is stable — unlike the per-stage instance names
(`_22`, `_43`) SPIR-V invents for anonymous blocks. Two footnotes: the
flatten mode forbids mixing floats and integers in one block (fine —
ours is all floats, now a format rule), and the generated sampler
declarations still carry a 420pack-only `layout(binding = 1)` that the
generation script must strip, binding samplers by name instead.

#### The rest of the round trip

`--flip-vert-y` does exactly what it promises (appends
`gl_Position.y = -gl_Position.y;` — whether GL needs it awaits the
golden tests), and the MSL output is textbook SPIRV-Cross: the
combined sampler splits into a `texture2d<float>` plus `sampler` pair,
the block becomes a `constant Uniforms&` buffer argument. Both exactly
what the Metal executor will consume. One cosmetic curiosity: float
constants come out in their exact decimal form — `2.2` becomes
`2.2000000476837158203125` — which is the same bits, printed honestly.

Verdict: every open toolchain question from the plan now has an
evidence-backed answer, and the offline generation script's design
wrote itself.

---

### Chapter 6: Three more spikes — libraries, a pinned extension, and photons on demand

While the plan sat in review, three more open questions looked cheap
enough to answer with experiments rather than assumptions. Two were
quick confirmations. The third turned into a small saga with a very
satisfying ending.

#### The library spike: same tools, different door

Chapter 5 proved the toolchain using the command-line tools; the real
backends will use the *libraries*, and "the CLI works" does not
automatically mean "the C++ API works, ships usable headers on this
platform, and behaves the same". Twenty minutes of C++ later: it does.
glslang's modern API even removed a famous annoyance — where projects
like RetroArch hand-copy a hundred-line resource-limits table into
their source, current glslang simply exports `GetDefaultResources()`.
Our single-file `VERTEX`/`FRAGMENT` trick maps onto `setPreamble()`,
and the whole compile is a dozen lines.

The more valuable half was reflection. We extended the test shader's
uniform block with parameter-style members — two `vec2`s, two lone
`float`s, then another `vec2`, deliberately chosen so the offsets are
non-trivial — and asked SPIRV-Cross where the compiler actually put
them: 0, 8, 16, 20, 24, total size 32. Exactly what the
UniformPacker's canonical std140 arithmetic predicts. The
packer-as-truth design now rests on measured bytes, not on our reading
of the spec. One gotcha surfaced for free: an *anonymous* uniform
block reflects with an empty instance name, so the debug validator
must identify blocks by base type and member names — instance names
are both empty and (as Chapter 5 showed with `_22`/`_43`) unstable
anyway.

#### Pinning the extension: no more "consult the spec later"

The plan's PR 6 carried one embarrassing placeholder: "struct names
shifted during the extension's five-year gestation; consult the final
spec." The Khronos registry blocks robots, but the Vulkan-Docs
repository doesn't, and the released proposal document pins
everything. The API is richer than our sketch assumed, in three ways
that change the implementation:

First, capabilities are fine-grained: *supporting the extension* and
*supporting absolute-time scheduling* are separate feature bits
(`presentTiming` vs `presentAtAbsoluteTime`), and the swapchain itself
must opt in at creation time with a flag. Second — the trap we're
glad to know about now — feedback is collected into an internal queue
whose size *you* must set (`vkSetSwapchainPresentTimingQueueSizeEXT`);
leave it unsized and your timing results silently vanish. Third, the
feedback is per-*stage*: among the queryable stages is
`IMAGE_FIRST_PIXEL_VISIBLE` — not "we handed the frame to the
compositor", but "the display hardware began emitting the first
pixel". That is the photon timestamp, and it is what our pacing logs
will call the truth. Clock mapping between our monotonic clock and
the swapchain's time domain goes through the calibrated-timestamps
mechanism, which is its own small ceremony but a well-trodden one.

#### The Metal spike: measuring photons on a Tuesday

The flagship claim of this whole project is that a display engine will
hold a flip until a time *we* choose. That claim was, until today,
supported by documentation and one emulator's source code. It felt
wrong to start a multi-month implementation without having ever
watched it happen. So: a 250-line standalone Cocoa program — a window,
a `CAMetalLayer`, 300 clear-colour frames presented with
`presentDrawable:atTime:` on a 70.086 Hz schedule, and
`addPresentedHandler` collecting when each frame really reached the
glass.

It crashed immediately, of course. Exit code 139, before a single
line of output — which itself was a lesson (`printf` buffers to
pipes; a segfault eats the buffer; unbuffer stdout in spike code).
With markers added, everything *looked* fine — window up, "Apple M4
Pro" reported, 120 Hz ProMotion display detected — yet all 300 frames
went unpresented, and the exit crash remained. Two classic unbundled-
Cocoa traps, worth recording forever: first, a program without an app
bundle must call `[NSApp finishLaunching]` before its windows truly
join the window server — order a window front before that and you get
a convincing ghost that never composites. Second, replacing an
`NSWindow` content view's backing layer with a `CAMetalLayer` is the
fragile way to host one; adding it as a *sublayer of a layer-backed
view* is the robust pattern (and the AppKit teardown crash left with
the fix — though the spike now ends with `_exit()` anyway, because a
spike owes the OS nothing).

Then it ran, and the numbers are worth framing. All 300 frames
presented, with feedback. The mean interval between actual glass
times: **14.254 ms against a requested 14.268 ms** — the display
engine reproduced a 70.086 Hz cadence on hardware that has no such
refresh rate, by doing precisely what theory says it should: on the
120 Hz grid it alternated one-vblank and two-vblank gaps, 90 frames
at ~8.3 ms and 203 at ~16.7 ms, the best integer realisation of
14.268 ms that an 8.33 ms lattice permits. The scheduling API isn't
just "supported" — it is doing arithmetic on our behalf.

Two further findings, both of which upgraded the plan. The intervals
quantised to the grid *at all* because a **windowed** surface on
macOS presents through the compositor at a fixed rate — even on a
ProMotion panel. Our "first-class pacing is a fullscreen feature"
stance stopped being an argument from documentation and became a
local measurement. And every frame landed a constant ~20 ms after its
requested time: we submitted only 3 ms ahead, and a windowed
compositor pipeline is about two frames deep — you cannot hit a
target the pipeline can't reach. The fix is simply to schedule
targets two or three refresh cycles ahead; and the deeper point is
one every emulator developer eventually internalises: **constant
latency is benign — jitter is the enemy.** A steady 20 ms offset
disappears into the audio/video sync budget; a wobbling one is what
your eyes report as stutter.

One spike, three design inputs, and the flagship feature has now been
seen working with our own eyes, before a single line of production
code exists. That is what spikes are for.

### Chapter 7: The review that deleted a backend

By the end of Chapter 6 the plan looked finished. Seven PRs specced
to execution grade, a reference study across six emulators, four
green spikes, the flagship mechanism watched working on real
hardware. Then John did something uncomfortable and correct: before
letting a single line of implementation start, he took three of the
plan's most structural decisions and audited them against the
freshest official guidance — vulkan.org, the new Khronos tutorial,
the MoltenVK project, the Slang initiative — on the suspicion that
we had missed something important.

He was right on two of the three. One of the misses was big enough
to delete an entire backend from the plan.

This chapter tells that story, because the *way* it happened teaches
as much as the outcomes: how reference-driven design quietly embeds
a bias, how to fact-check a technology claim with primary artifacts,
and how a two-hundred-line experiment can settle an argument that
documentation alone never would.

#### The blind spot in "study the masters"

Chapter 4 was built on a real strength: before designing anything,
we read six shipping emulators' render backends and adopted the
patterns they had bled for. What nobody warned us about is the
failure mode of that method. All six are raw-C-API Vulkan codebases
whose foundations were laid in Vulkan's first years; zero of them
use Vulkan-Hpp, zero use vk-bootstrap, and their idioms crystallised
long before the current official guidance existed. When all your
references agree with each other, it is tempting to read that as
confirmation. Sometimes it only means they share a birthday.

Meanwhile, the official Khronos tutorial — rewritten and moved to
docs.vulkan.org — teaches, today: **Vulkan-Hpp with the RAII layer**,
modern C++, **Slang as the primary shading language**, a Vulkan 1.4
baseline, dynamic rendering, timeline semaphores. That is a different
world from the one our references live in. And since we are writing
a brand-new backend in 2026, not maintaining a 2016 one, the burden
of proof sits on us to justify *diverging* from current guidance —
not the other way round.

So each of the three challenges got the same treatment: establish
the facts from primary sources first, run an experiment where words
weren't enough, then decide. Two decisions flipped; one survived
with a better-documented reason. All three outcomes are recorded in
the plan (decisions 1, 13, 14 and Appendix E); what follows is the
reasoning at full length.

#### Vulkan-Hpp, and the dependency that dissolved

Vulkan-Hpp is the official C++ projection of the Vulkan API: the
same concepts, but with scoped enums instead of bare integers,
lifetime-owning RAII wrapper classes, and — the detail that decided
everything — configurable error handling. The plan's style decision
(decision 12) had already mandated C++23 with `std::expected` for
fallible construction, in a codebase that forbids exceptions. Define
`VULKAN_HPP_NO_EXCEPTIONS` under C++23 and Vulkan-Hpp's `vk::raii`
construction functions return exactly
`std::expected<vk::raii::Device, vk::Result>`. We had, in effect,
already designed Vulkan-Hpp's error model by hand and were about to
re-implement it around raw C handles. Discovering that the official
binding *natively speaks the idiom your style guide chose* ends the
argument.

Two supporting facts made it free. First, `vulkan.hpp` and
`vulkan_raii.hpp` ship *inside the vulkan-headers package* we need
anyway — verified locally before deciding, because "zero new
dependencies" is exactly the kind of claim that dies on contact with
a package manager. Second, pNext chains. Vulkan bolts extension
features onto calls by threading `void*`-linked structs — the single
most error-prone construct in the raw API, where a wrong `sType` or
a broken link is a runtime validation error at best. Our
present-timing code is pNext-heavy by nature. Vulkan-Hpp's
`vk::StructureChain` makes those chains a compile-time-checked type.
The one real cost is compile time (vulkan.hpp is a famously heavy
header), bounded by the fact that only a handful of translation
units in one backend directory will ever include it.

Why doesn't any studied emulator use it? Because they predate its
maturity, carry years of working raw-C code, and migrating a working
backend to a new binding is all risk and no feature. None of that
applies to an empty directory. This is the reference-study bias in
one sentence: *the masters' code records their constraints along
with their wisdom, and you have to know which is which.*

Adopting Hpp then knocked over a domino nobody had aimed at:
**vk-bootstrap left the plan.** It had been in since Appendix B as
one of the two "dull, stable" boilerplate-eaters, and everything
Appendix B says about it remains true. But look at what the
boilerplate actually is for us: our device requirements are trivial
(Vulkan 1.3-class, a swapchain, one queue family that can do
graphics and present — no device scoring, no optional-feature
ladders, no exotic queues). Against requirements that simple, the
official tutorial's init sequence — Apache-2.0, adaptable with
credit — plus Hpp's RAII types shrink "the 500–1000 lines
vk-bootstrap exists to eat" down to 200–400 lines we fully
understand and own. A dependency is an answer to a question; when
the question dissolves, the answer must be allowed to leave. It
does, with thanks (the attribution file says so in so many words).

**VMA fell in the same review**, for a reason worth internalising
as a general skill: *know your allocation profile before adopting an
allocator.* VMA earns its industry-standard status on workloads
with **many, churny, heterogeneous** allocations — streaming texture
caches, per-frame buffer storms, thousands of live resources
bumping against the driver's total-allocation ceiling (commonly
4,096), fragmentation over hours of play. Now count our resources:
one input image, half a dozen intermediates, a target image, two
staging slots, a vertex buffer, a few tiny uniform buffers — about
fifteen allocations, all long-lived, rebuilt only when the video
mode or viewport changes. That is not a small version of VMA's
problem; it is the *absence* of VMA's problem. Manual allocation at
this scale is a ~15-line `findMemoryType` helper (the
tutorial-standard loop over `memoryTypeBits` and property flags) and
one `vkAllocateMemory` per resource. The one genuine footgun in
manual land got promoted to a plan rule: per-frame staging memory is
`HOST_VISIBLE | HOST_COHERENT`, **mapped once at creation and left
mapped forever** — map/unmap per frame is the classic beginner tax,
and persistent mapping is both legal and universally recommended.

#### MoltenVK: the argument we expected to win

The second challenge was the uncomfortable one: *why are we writing
a native Metal backend at all, when MoltenVK exists?* The plan's
implicit answer had been quality — surely a translation layer can't
be trusted with the flagship feature, microsecond present timing.
We went in expecting to defend the Metal backend. The evidence
executed it instead.

What MoltenVK actually is, first, because "translation layer"
undersells it: a **Khronos-maintained** implementation of Vulkan
(1.4 in current releases) layered over Metal, developed by The
Brenwill Workshop — the reference implementation of the Vulkan
Portability initiative, not a community shim. The facts that
mattered to us specifically:

- **`VK_GOOGLE_display_timing` — the older, Google-authored cousin
  of `VK_EXT_present_timing` — has been implemented in MoltenVK
  since 2021**, mapped internally onto the same
  `presentDrawable:atTime:` machinery our Spike 4 validated. Same
  contract as the EXT extension: a desired present time in, measured
  actual glass times back.
- Its most demanding known user is **Psychtoolbox-3** — the
  standard scientific toolkit for visual psychophysics, where
  stimulus onset timing *is the experiment's validity* and
  presentation error is measured in microseconds. Psychtoolbox
  drives macOS through Vulkan/MoltenVK precisely for this timing
  path. There is no harsher production test of a present-timing
  implementation.
- **PPSSPP and RetroArch ship their macOS Vulkan backends on
  MoltenVK in production.** The quirk tail we feared is being walked
  daily by two of the projects we studied.
- Colour management goes through `VK_EXT_swapchain_colorspace`
  (Display P3 without writing a line of Metal), and if raw Metal
  access is ever truly needed, `VK_EXT_metal_objects` exports the
  underlying `CAMetalLayer`, drawables, and textures — a real escape
  hatch, verified to exist rather than assumed.

Against that: what would a hand-written Metal backend buy? A second
pass-graph executor, a second WSI implementation, Objective-C++ in
the tree, runtime MSL generation (keeping SPIRV-Cross as a runtime
dependency), an embedded-MSL bootstrap shader, and a permanent
second column in every test matrix — to reach *the same
`atTime:` call* MoltenVK reaches for us. The deletion list writes
itself; the only honest question left was whether the timing quality
truly survives the layer. Documentation said yes. Psychtoolbox's
existence said yes. We measured anyway.

#### Spike 5: trust, but measure

Deleting a planned backend on the strength of other people's claims
would have been exactly the kind of decision this project tries not
to make. So Spike 5 re-ran Spike 4's experiment — same M4 Pro, same
120 Hz ProMotion panel, same 70.086 Hz cadence, same 300 frames —
with one variable changed: instead of native Metal, a raw-C-API
Vulkan program (`spikes/vulkan_timing_spike.mm`) linked directly
against MoltenVK, scheduling every present with
`VK_GOOGLE_display_timing`'s `desiredPresentTime` and reading actual
glass times back with `vkGetPastPresentationTimingGOOGLE`.

Two integration lessons surfaced before the first frame, both worth
their line in this book. Linking MoltenVK directly (no Khronos
loader in between) means *loader-level* extensions don't exist:
requesting `VK_KHR_portability_enumeration` — which every "Vulkan on
macOS" tutorial tells you to enable — fails instance creation with
`VK_ERROR_EXTENSION_NOT_PRESENT`, because that extension is
implemented by the loader, not the driver. Request only what the
implementation itself provides (`VK_KHR_surface`,
`VK_EXT_metal_surface`). And this particular Homebrew MoltenVK build
exposed **Vulkan 1.2** — a useful reminder that the macOS feature
baseline is set by *which MoltenVK we ship*, not by the hardware or
macOS version; pinning a modern MoltenVK in the build is part of the
plan's dependency work.

Then the run. All 300 presents produced feedback entries — complete
per-present observability, `desiredPresentTime` and
`actualPresentTime` for every `presentID`, plus
`vkGetRefreshCycleDurationGOOGLE` correctly reporting the panel's
8.333 ms cycle. And the histogram is the whole verdict, side by
side with Spike 4:

|                    | Spike 4 (native Metal) | Spike 5 (MoltenVK)     |
|--------------------|------------------------|------------------------|
| feedback coverage  | 300/300                | 300/300                |
| mean interval      | 14.254 ms              | 14.275 ms              |
| target period      | 14.268 ms              | 14.268 ms              |
| interval histogram | 90 × 8.3 + 203 × 16.7  | 90 × 8.3 + 207 × 16.7  |

The same 2:1 alternation of one-vblank and two-vblank gaps — the
mathematically best realisation of 70.086 Hz on a 120 Hz grid —
reproduced through the translation layer to within a fraction of a
percent on the mean. MoltenVK isn't approximating the mechanism; it
is *the same mechanism*, one function call further away. (Lateness
versus target also behaved exactly as Chapter 6's schedule-ahead
lesson predicts: submitting 10 ms ahead instead of Spike 4's 3 ms
cut the constant windowed-compositor offset from ~21 ms to ~13 ms.
The two-to-three-cycles-ahead rule stands.)

That table is what sealed decision 1. The plan's PR 4 — the entire
native Metal backend — became a tombstone the same day, and the
timing design collapsed to one backend with two extension dialects:
`VK_EXT_present_timing` where new Mesa/NVIDIA drivers offer it,
`VK_GOOGLE_display_timing` on MoltenVK, and the PR 1 app-timed
scheduler underneath both. Two dialects, one backend — versus one
dialect, two backends. Written down like that, it is hard to
remember why a Metal backend ever felt necessary.

What did we give up? Xcode's Metal debugger now shows MoltenVK's
generated Metal rather than hand-written Metal — but it still works,
*and* the Vulkan validation layers run on top of MoltenVK, so macOS
debugging arguably gained a toolset. The Vulkan "portability subset"
carve-outs are real but irrelevant to a five-pass fragment pipeline
(they concern geometry shaders, triangle fans, exotic border
colours). The honest residual cost is being downstream of a
translation layer's release cadence — mitigated by who maintains it
and who ships on it.

#### Slang, and the name that almost misled us

The third challenge came armed with what looked like a decisive
fact. The official tutorial teaches Slang — NVIDIA's shading
language, fifteen years in development, now a Khronos initiative —
as *the* shader language for new Vulkan work. And RetroArch's entire
shader ecosystem is famously built on ".slang" files… so the retro
community has already migrated, and our Vulkan GLSL 4.50 choice
swims against the tide?

Fact-check first, and always with a primary artifact, not a search
summary. We opened libretro's actual `stock.slang`: `#version 450`,
`layout(push_constant)`, `layout(std140, set = 0, binding = 0)`,
`#pragma stage vertex` — that is **Vulkan GLSL with a pragma
convention**, compiled by RetroArch through glslang with Vulkan
rules. The name is a libretro coinage from 2016, years before
Khronos adopted the (unrelated) Slang language. The "ecosystem
evidence" for Slang was actually ecosystem evidence *for our exact
format* — the strongest confirmation the 450 decision has received.
Names lie; artifacts don't.

With that corrected, the real Slang evaluation was straightforward,
and it is a study in how the right recommendation for one audience
is wrong for another. For the tutorial's audience — new codebases,
no legacy shaders, no third-party shader ecosystem — Slang is
genuinely the better language: modules, generics, real IDE support,
one source for every API. Our situation differs in three specific,
factual ways. **Conversion asymmetry**: our 21 shaders are GLSL;
porting them to 450 is a mechanical recipe (spike-proven, bodies
untouched), while porting to Slang's HLSL-like syntax is the full
rewrite we already rejected once. **Runtime packaging**: user custom
shaders mean we ship a compiler; glslang is small, static-linkable,
and packaged by every Linux distro, while libslang is
dynamic-library-only, tens of megabytes, and packaged by essentially
no distro — the exact bundling situation our dependency policy
avoids. **The ecosystem writes GLSL** — including, as just
established, the part of it with "slang" in the filename.

Slang was therefore declined — but declined *properly*: the shader
front-end in our architecture is one pluggable component
(`Shaders::Compiler` is the only class that knows GLSL exists, and
everything downstream speaks SPIR-V), so if the retro-shader world
ever genuinely migrates, swapping compilers is a one-component
change. A decision with a recorded escape route ages better than a
conviction.

#### The ledger

Worth tallying what one afternoon of adversarial review deleted
from a finished plan: one entire native backend (executor, WSI,
Objective-C++, runtime MSL generation, a test-matrix dimension),
one bootstrap library, one allocator library, and SPIRV-Cross's
last runtime role. The runtime dependency list ended the day at:
**glslang, plus MoltenVK on macOS**. The plan got *simpler* by
getting reviewed — not the usual direction reviews push plans — and
every deletion traces to evidence: a header that ships where we
thought it didn't, a production system harsher than our use case, a
histogram matching to the frame, a `.slang` file that turned out to
be GLSL.

The process lesson is the one to keep. Locked decisions rot, even
good ones, because the ecosystem moves while you plan — and the
cheapest possible moment to find out is the gap between "plan
finished" and "implementation started". An adversarial pass against
the *current* official guidance, run by someone actively trying to
prove the plan wrong, cost a few hours and removed several
thousand lines of future code. The masters taught us the patterns;
the review taught us which patterns had expired. Both lessons were
necessary, and neither could have replaced the other.

## Part III — Per-commit chapters

*(Begins with PR 1. Each commit in the series appends its chapter
here: what the commit does, the concepts behind it, and why the code
is shaped the way it is.)*

---

## Reading list

### Vulkan
- [Vulkan Tutorial](https://vulkan-tutorial.com) — the classic
  step-by-step; read "Drawing a triangle" and "Swap chain recreation".
- [vkguide.dev](https://vkguide.dev) — more modern architecture
  advice, closer to how real engines structure things.
- [Khronos Vulkan Guide](https://github.com/KhronosGroup/Vulkan-Guide)
  — the official map of the ecosystem.
- [Khronos Vulkan-Samples](https://github.com/KhronosGroup/Vulkan-Samples)
  — `samples/api/swapchain_recreation` is our canonical reference for
  WSI handling (Apache-2.0, copy-friendly).
- [The official Khronos Vulkan tutorial](https://docs.vulkan.org/tutorial/latest/)
  — the current official on-ramp (Vulkan-Hpp RAII, dynamic rendering,
  Slang); the guidance Chapter 7 audited the plan against, and the
  licence-clean template for our device initialisation.
- [Vulkan-Hpp](https://github.com/KhronosGroup/Vulkan-Hpp) — the
  official C++ binding; our binding layer. Read the RAII programming
  guide and the `VULKAN_HPP_NO_EXCEPTIONS` section.
- [MoltenVK](https://github.com/KhronosGroup/MoltenVK) — the layered
  Vulkan-over-Metal implementation our macOS path runs on; the user
  guide documents its configuration knobs and extension support.
- [vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap) and
  [VMA](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
  — both evaluated and set aside with thanks (Chapter 7): our init
  and allocation needs turned out too small for them. Still the
  right first stop if yours aren't.
- Hans-Kristian Arntzen's blog ([themaister.net](https://themaister.net))
  — the best writing on Vulkan synchronisation and on render-graph
  thinking; he also wrote SPIRV-Cross and RetroArch's Vulkan driver.

### Present timing and pacing
- [VK_EXT_present_timing: the Journey to State-of-the-Art Frame Pacing in Vulkan](https://www.khronos.org/blog/vk-ext-present-timing-the-journey-to-state-of-the-art-frame-pacing-in-vulkan)
  — the Khronos blog announcing the extension; required reading.
- [Phoronix: Mesa 26.1 present-timing support](https://www.phoronix.com/news/Mesa-VK_EXT_present_timing)
  and [the merge story](https://www.phoronix.com/news/VK_EXT_present_timing-Merged).
- [VK_EXT_present_timing reference](https://vkdoc.net/extensions/VK_EXT_present_timing).
- "The Elusive Frame Timing" — Alen Ladavac, GDC 2018 talk; the
  clearest explanation of why frame pacing is hard (find it on the GDC
  Vault or YouTube).
- [Microsoft: DXGI flip model](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model)
  — how Windows presentation actually works; context for the
  compositor ceiling.
- PPSSPP's `VulkanRenderManager.cpp` — the only shipping emulator
  code using the Vulkan timing extensions (for measurement).

### SPIR-V and the shader toolchain
- [SPIR-V Guide](https://github.com/KhronosGroup/SPIRV-Guide) — gentle
  official introduction.
- [SPIR-V specification](https://registry.khronos.org/SPIR-V/) — for
  when the guide isn't enough.
- [glslang](https://github.com/KhronosGroup/glslang) and
  [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) READMEs —
  both are better documentation than they look.
- [GL_KHR_vulkan_glsl extension text](https://github.com/KhronosGroup/GLSL/blob/main/extensions/khr/GL_KHR_vulkan_glsl.txt)
  — the precise definition of "Vulkan GLSL vs OpenGL GLSL".
- [LearnOpenGL: Advanced GLSL](https://learnopengl.com/Advanced-OpenGL/Advanced-GLSL)
  — readable std140 layout rules.
- RetroArch's `gfx/drivers_shader/` — the closest existing relative of
  our shader pipeline (GPLv3: read for patterns, don't copy).

### Metal

*(The native Metal backend was deleted — Chapter 7 — but Metal is
what MoltenVK speaks underneath, and Spike 4 was pure Metal, so this
list stays for macOS debugging.)*

- [Metal by Example](https://metalbyexample.com) — the friendliest
  Metal introduction.
- [Metal Shading Language Specification](https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf).
- Apple's `CAMetalLayer` and `MTLDrawable` documentation — small API
  surface, worth reading in full; `present(atTime:)` lives here.
- Dolphin's `Source/Core/VideoBackends/Metal/` — a complete, modern,
  compact Obj-C++ backend (GPLv2+, licence-compatible).
- DuckStation's `src/util/metal_device.mm` — the shipping
  `presentDrawable:atTime:` usage (no-derivatives licence: read to
  understand, never copy).

### Testing
- [lavapipe / llvmpipe](https://docs.mesa3d.org/drivers/llvmpipe.html)
  — Mesa's software Vulkan implementation; our ticket to real
  golden-image rendering tests in CI.
- [Vulkan validation layers](https://github.com/KhronosGroup/Vulkan-ValidationLayers)
  — always-on in debug builds; read the synchronisation-validation
  section.
- [RenderDoc](https://renderdoc.org) — frame capture and debugging for
  both Vulkan and (via Xcode alternatives) general GPU work.
