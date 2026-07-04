# Native Vulkan & Metal render backends — plan

## Overview

DOSBox Staging currently offers two render backends: OpenGL (the
flagship, carrying the entire CRT shader system) and the SDL texture
backend (the simple fallback). This project adds two native backends —
**Vulkan** on Linux and Windows, **Metal** on macOS — with the full
shader pipeline, and retires nothing: OpenGL and texture remain as
fallbacks.

**Why now.** OpenGL is a sunsetting API. Apple deprecated it years ago
and has frozen it at 4.1; on Windows and Linux, driver vendors treat
it as legacy while their engineering effort goes into Vulkan, D3D12,
and Metal. Every year we stay OpenGL-only, we depend more heavily on
the least-maintained path through every driver. The native backends
put DOSBox Staging on the APIs that are actually being developed —
and on macOS, on the platform's first-class citizen instead of a
deprecated compatibility layer.

**The flagship feature: first-in-class frame pacing.** DOS video
runs at rates no fixed monitor refreshes at — mode 13h is 70.086 Hz —
and smooth scrolling lives or dies on hitting that cadence exactly.
On VRR displays (G-Sync/FreeSync/ProMotion), the display refreshes
when a frame *arrives*, so whoever controls present timing controls
smoothness. The modern APIs let us hand the display engine a
**timestamp with each frame** — "show this at exactly T" — via
`VK_EXT_present_timing` on Vulkan and `present(atTime:)` on Metal,
with feedback telling us when each frame really reached the glass.
That gives microsecond-class pacing that is immune to emulation-loop
load, plus the measurements to prove it. To the best of our knowledge
(and we surveyed six major emulators' source to check —
[Appendix C §7](#7-present-pacing--the-headline-finding)), **DOSBox
Staging will be the first emulator to ship Vulkan timestamped
presents**. Where the mechanism isn't available, an app-timed
scheduler tier (which also improves the existing OpenGL and SDL
texture backends)
provides the fallback, and the pacing spike in
[Appendix D](#appendix-d--spike-results-2026-07-04) has already
demonstrated the Metal mechanism working on real hardware.

The result is not an incremental port. What this plan builds is a
state-of-the-art GPU presentation pipeline — display-engine-scheduled
presents with measured glass-time feedback, a never-blocking
emulation loop, a single cross-compiled shader source tree, and CI
that renders real frames — designed against the current best practice
of the field and, in its pacing, ahead of it. Vulkan and Metal are
where the industry's driver and tooling investment lives; building on
them now, carefully, gives DOSBox Staging a rendering foundation that
should comfortably serve the project for the next decade.

**Why not SDL's GPU API?** It was the original plan (v1), and it was
abandoned for cause: the API is still young and moving; it is
impossible to build frame-exact presentation on top of it, because
its notion of "good enough" present timing is hard-coded into the
abstraction (the present call is sealed — no native handles, no
timestamps, no feedback); and the extra abstraction layer sits
between us and the debugging tools (validation layers, RenderDoc)
exactly where GPU debugging hurts most.

To be fair to SDL's designers, that hard-coding is a perfectly
reasonable choice *for their audience* — it just isn't us. A modern
game renders frames as fast as it can and shows each one as soon as
it's ready: on a VRR display that is automatically smooth, and on a
fixed 60 Hz display ordinary vsync is exactly right, because the
game's content isn't tied to any particular rate. Emulators are the
odd ones out. Our content has an *exact, non-standard* rate baked
into it (mode 13h is 70.086 Hz, and the games' scrolling was authored
against precisely that), so on VRR we must place each frame at an
exact moment rather than "as soon as possible" — and on a fixed 60 Hz
display, squeezing a 70 Hz stream through means deliberately
choosing *which* frames to drop and when, rather than letting the
driver's queue decide by back-pressure. Neither need exists for
games, so the abstraction never grew the controls for them. Fewer
moving parts, full control, and direct debuggability won. The complete argument is
preserved in
[Appendix A](#appendix-a--architecture-decision-memo-2026-07-03-verbatim).
(The debugger's ImGui overlay still uses the SDL GPU API today in its
own separate window; a follow-up will try it on the plain SDL_Renderer
backend — almost certainly fine for a debugging UI — after which the
GPU API dependency disappears from the tree entirely.)

**What else this buys us:**

- **One shader source tree instead of per-backend maintenance**: all
  shaders move to a single hand-maintained Vulkan GLSL 4.50 format;
  the OpenGL variants are generated offline by a script and checked
  in, so shader authors edit exactly one file per shader.
- **The emulation loop can never again stall on the display** — and
  this matters more in DOSBox than in most programs, because
  presentation lives on the main thread, and the main thread is also
  where CPU emulation and most emulation layers run (moving it
  elsewhere is not realistically on the table). Every millisecond the
  driver blocks us waiting for vblank is a millisecond stolen
  directly from emulation; and the alternative — busy-waiting our way
  to more accurate present timing — would burn even more CPU for it.
  The native backends dissolve the dilemma: presents are submitted
  asynchronously with a timestamp (the display engine does the
  waiting, not us), back-pressure becomes a visible, skippable
  condition instead of a hidden block inside the driver, and heavy
  vsync scenarios drop frames cleanly rather than taxing the
  emulation. Accuracy goes up while main-thread cost goes *down*.
- **Rendering becomes testable in CI**: with native device selection
  we can run the real shader pipeline on Mesa's software Vulkan
  implementation (lavapipe) headlessly — golden-image tests on every
  commit, which the OpenGL backend never had.
- **A cleaner dependency story**: the risky preview-stage libraries
  evaluated along the way were rejected; what remains is glslang,
  SPIRV-Cross, vk-bootstrap, and VMA — all boring, stable, and
  packaging-friendly ([decision 5](#decision-record-all-locked-2026-07-0304) has the distro review).
- **Correct colour management and an HDR path**: Metal gets proper
  Display P3 handling on day one, and the native APIs open the door
  to HDR output later (deferred, but no longer architecturally
  blocked).
- **Documentation as a first-class deliverable**: three kinds. The
  companion `gpu-backend-learning.md` documents every concept,
  experiment, and lesson from other projects' source as the work
  proceeds, one chapter per commit. `gpu-backend-attribution.md`
  publicly thanks the emulator projects and developers whose work
  inspired ours, recording exactly which ideas and code we lifted
  and from where — fairness we owe them, maintained
  commit-by-commit. And the render module itself gets
  **extensive architecture documentation** — beyond a simple README,
  split into parts (general architecture, the shader subsystem, and
  one part per backend) with **rendered architectural diagram
  images**, not just ASCII charts (a standing constraint of the
  [implementation phase](#implementation-order--execution-grade-specs),
  finalised in [PR 7](#pr-7--auto--polish)).

## Plan status

**Starting point: `jn/video-refactoring` tip** (`49d909939`) — the
`UploadFrame`/`PresentFrame` backend contract, source-dimension uploads
with GPU-side doubling, viewport-sized target with clean-present skip,
and target-based captures are the substrate the new backends clone.
Do not start before that branch is merged (or at least stable).

**Plan lineage**: v1 was SDL-GPU-based (`sdl-gpu-backend.md`, now
superseded and deleted); v2 incorporated the first review round; v3
(this) pivots to native backends after the present-timing requirement
was made non-negotiable. [Appendix A](#appendix-a--architecture-decision-memo-2026-07-03-verbatim) is the verbatim architecture
decision memo; [Appendix B](#appendix-b--follow-up-decisions-dependencies-shader-workflow-cache-deferral-reference-study-2026-07-03) records the follow-up decisions; [Appendix C](#appendix-c--reference-study-design-notes-2026-07-04)
holds the reference-study design notes; [Appendix D](#appendix-d--spike-results-2026-07-04) the toolchain spike
results. The learning companion is `gpu-backend-learning.md` (one
chapter per commit — standing process rule).

**Status: PLANNED — all PRs (1–7) specced to execution grade.
Reference study (six emulators + Khronos samples, [Appendix C](#appendix-c--reference-study-design-notes-2026-07-04)) and
toolchain spike ([Appendix D](#appendix-d--spike-results-2026-07-04)) DONE 2026-07-04. Next: joint review of
this document, then implementation. Hard prerequisite:
`jn/video-refactoring` tested and merged. Flesh-out decisions
awaiting John's review are listed in "Decisions made during
flesh-out".**

## Decision record (all locked, 2026-07-03/04)

1. **We build native Vulkan and Metal backends; SDL GPU and dx12 were
   evaluated and rejected** (the full argument is preserved verbatim
   in [Appendix A](#appendix-a--architecture-decision-memo-2026-07-03-verbatim)). Both new backends are committed scope, implemented
   Vulkan-first. The OpenGL backend stays supported for roughly five
   more years as the fallback path, and the `texture` backend stays
   indefinitely — usefully, on Windows it runs on Direct3D 11 through
   SDL_Renderer, which gives us a Microsoft-native fallback without
   writing any D3D code ourselves.
2. **First-class frame pacing is a core requirement of the project,
   not an optimisation.** Where the platform allows it, we use
   display-engine present timing — `VK_EXT_present_timing` on Vulkan
   and `present(atTime:)` on Metal — meaning we attach a target
   timestamp to each present and the display hardware holds the flip
   until exactly that time. Where it isn't available, an app-timed
   deferred-present scheduler serves as the permanent fallback tier.
   The scheduler gets built unconditionally (John's decision — no
   measure-first hedging), partly because it improves the existing
   OpenGL and SDL texture backends on VRR displays immediately — it
   operates above the `RenderBackend` interface, so every backend's
   presents get due-time scheduling.
3. **The `output` setting gains per-API values instead of a generic
   "gpu" value**: `texture`, `texturenb` (kept for compatibility),
   `opengl`, `vulkan` (registered on Linux and Windows), `metal`
   (macOS), and `auto`. The `auto` value resolves to the best native
   API for the platform — `vulkan` on Linux and Windows, `metal` on
   macOS — and falls back through `opengl` to `texture` when the
   preferred backend can't initialise. An explicitly requested value
   that fails at startup falls down the same chain with a logged
   warning. The setting remains `OnlyAtStart`; there is no runtime
   backend switching, same as today.
4. **Shaders move to a single hand-maintained source format: Vulkan
   GLSL 4.50.** Each backend consumes it its own way. The Vulkan
   backend compiles it to SPIR-V at runtime using glslang. The Metal
   backend also compiles at runtime, going one step further through
   SPIRV-Cross to produce Metal Shading Language. The OpenGL backend
   is deliberately kept toolchain-free at runtime: its GLSL 3.30
   variants are produced by an **offline generation script** and
   checked into the repository, with a CI `--check` mode guaranteeing
   they can never go stale relative to the 450 sources. Existing
   user-written 330 shaders keep working on `output = opengl`
   indefinitely; the 450 format is required only for the vulkan and
   metal backends. The GL emission uses SPIRV-Cross's `--flatten-ubo`
   mode — spike-verified in
   [Appendix D](#appendix-d--spike-results-2026-07-04) — which
   matters because it avoids a GL extension that macOS OpenGL lacks
   and keeps uniform upload offset-based and shared across all
   backends.
5. **Dependencies**: glslang, SPIRV-Cross, vk-bootstrap, and VMA are
   added; all four are stable, mature libraries available through
   vcpkg for our own builds. The Linux distro packaging situation was
   reviewed (2026-07-04) with the following outcome: glslang is
   packaged by every major distro and becomes a hard system
   dependency on Linux; VMA is a single self-contained header, so we
   use the system package where one exists (Fedora has one) and fall
   back to a bundled copy of the header elsewhere; vk-bootstrap is
   not packaged by any major distro, but it is a small (~3k line)
   MIT-licensed library explicitly designed for vendoring, so it gets
   bundled under `src/libs/` alongside our other vendored code;
   SPIRV-Cross is poorly packaged as a *library* (Debian ships only
   the CLI tool, Fedora has no official package) — but on Linux we
   only use it for the optional debug-build reflection validator, so
   it becomes a compile-time-optional dependency there and is never
   required for a Linux release build. Its one hard-requirement role
   (MSL generation for Metal) exists only on macOS, where vcpkg/brew
   provide it and distro packaging is irrelevant. In short: nothing
   large ever needs bundling. SDL_shadercross and the whole
   DXC/FXC/DXBC toolchain question disappeared together with SDL GPU
   and dx12.
6. **The on-disk shader cache is deferred to possible future work** —
   not scheduled, not gated on anything. The cache was originally
   justified by DXC's 50–300 ms compile times on the dx12 path; with
   dx12 gone, the remaining compilers (glslang, SPIRV-Cross) cost
   single-digit milliseconds per shader, which is not worth a cache.
   The design sketch is preserved in the "Deferred" section so it
   never has to be re-derived.
7. **Whether `presentation_mode = auto` should map to dos-rate always
   on the native backends is a test-then-decide question.** Once
   presents can no longer block the emulation loop, host-rate mode
   loses its main reason to exist, and dos-rate-always would give VRR
   users perfect pacing by default — but the resulting frame-drop
   pattern on ordinary fixed-rate displays hasn't been verified on
   real hardware yet. Today's vsync-derived auto mapping stays in
   place until that comparison is done ([PR 7](#pr-7--auto--polish)).
8. **vsync semantics on VRR displays**: as long as the DOS refresh
   rate stays inside the display's VRR range, the staging `vsync`
   setting makes no practical difference — every present flips on
   arrival either way. Driver-forced vsync combined with staging
   vsync off is also a valid user configuration, **but only when the
   desktop refresh rate is set above our maximum DOS refresh rate**
   (e.g. a 120 Hz desktop for 70 Hz content) — if the desktop rate
   sits below the DOS rate, the forced vsync throttles or drops our
   presents. This gets documented in the VRR guide rather than
   fought in code. The native present-mode mapping is: vsync on →
   FIFO; vsync off → IMMEDIATE, falling back to MAILBOX and then
   FIFO where IMMEDIATE is unsupported.
9. **The reference-implementation study is done.** Six shipping
   emulators' render backends (Dolphin, PPSSPP, RetroArch,
   DuckStation, PCSX2, RPCS3) plus the official Khronos samples were
   read before designing our own; the distilled design notes with
   file:line references live in [Appendix C](#appendix-c--reference-study-design-notes-2026-07-04), and every
   swapchain/sync/pacing decision in this plan traces back to them.
10. **The learning companion `gpu-backend-learning.md` is a standing
    process rule**: every commit in the series appends a
    textbook-style chapter explaining the concepts the commit
    touches, why the code is shaped the way it is, and where to read
    more. Long, narrative chapters are explicitly welcome — depth is
    a feature of that document, not padding.
11. **Attribution is mandatory, and licence-incompatible code is
    firewalled** (John's rules, 2026-07-04/05): any pattern, vendor
    list, or workaround adopted from the studied projects is credited
    at the adoption site in our code. And from codebases whose
    licences are incompatible with ours — **DuckStation
    (no-derivatives), RetroArch (GPLv3), PCSX2 (GPLv3)** — copying
    code is **explicitly prohibited during implementation**. The
    firewall works like this: everything we learned from those
    projects was distilled into *this document* ([Appendix C](#appendix-c--reference-study-design-notes-2026-07-04)) as
    described designs, in our own words; the implementation phase
    works exclusively from this document and must not open, consult,
    or transcribe those codebases' source. Code and snippets may be
    adapted (with SPDX credit) only from the licence-compatible
    references: Dolphin (GPLv2+), PPSSPP (GPLv2+), RPCS3 (GPLv2), and
    Khronos Vulkan-Samples (Apache-2.0). Every adoption is also
    recorded with a public shout-out in `gpu-backend-attribution.md`,
    maintained commit-by-commit as implementation proceeds. Full
    rules in the ["Attribution policy"](#attribution-policy) section.
12. **Module layout, namespaces, and implementation style** (John's
    decisions, 2026-07-05). The render module is reorganised into
    per-backend and per-subsystem subdirectories — `render/opengl/`,
    `render/vulkan/`, `render/metal/`, `render/sdl/`, and
    `render/shaders/` — each with its own `private/` header directory.
    **Final state matters: existing files move too** (in a dedicated
    move-only commit; no bonus points for avoiding moves), and the
    OpenGL file names are made consistent with the other backends
    (`shader_pipeline.* → opengl_pipeline.*`, `shader.* →
    opengl_shader.*`; the class renames land with [PR 3](#pr-3--shader-toolchain--vulkan-pipeline)'s refactor,
    which guts those classes anyway). Backend-specific file prefixes
    (`vulkan_`, `metal_`, `opengl_`) are kept even inside the
    subdirectories — deliberately, since e.g. a
    `render/vulkan/vulkan.h`-style collision with the SDK's
    `<vulkan/vulkan.h>` must stay impossible. **New code uses
    namespaces** following the `src/audio/clap/` template: the
    namespace matches the deepest directory in UpperCamelCase, and
    class names de-stutter inside it — `Vulkan::Renderer`,
    `Vulkan::Device`, `Vulkan::Swapchain`, `Vulkan::Pipeline`,
    `Metal::Renderer`, `Metal::Pipeline`, `Shaders::PipelineTopology`,
    `Shaders::UniformPacker`, `Shaders::Compiler`,
    `Shaders::SpirvReflection`. Moved-but-unrewritten legacy files
    (GL, SDL, shader manager) are not retro-namespaced. **The
    implementation must be state of the art C++23** within the
    project style guide: `std::expected` for fallible construction
    and operations (fits the no-exceptions rule perfectly),
    `std::span` for buffer and SPIR-V views, monadic
    `std::optional` (`and_then`/`transform`), ranges where they
    clarify, designated initialisers for the many Vulkan info
    structs, `std::to_underlying`, `constexpr` everywhere it's true.
    The style guide still governs naming, includes,
    `CHECK_NARROWING()`, no iostreams, no exceptions beyond RAII —
    but we do not hold back to match deprecated patterns in the old
    code. When modernity and the style guide genuinely conflict:
    ask John.

## Architecture

### Backends

`VulkanRenderer` and `MetalRenderer` implement the existing
`RenderBackend` interface (`src/gui/render/render_backend.h`) —
unchanged. Each owns its SDL window (surface via
`SDL_Vulkan_CreateSurface` / `SDL_Metal_CreateView`); SDL keeps doing
windowing, input, events, audio. The debugger's ImGui-on-SDL-GPU stays
as-is (own window/device, unaffected); a possible later
simplification — moving it to the plain SDL_Renderer ImGui backend —
is recorded under Deferred.

**Vulkan backend** (`render/vulkan/`, namespace `Vulkan` — see the
[target-state listing](#target-state--module-layout-and-architecture) below for the full file breakdown):

- Initialisation goes through vk-bootstrap, which handles the
  instance, validation-layer setup (enabled in debug builds), physical
  device selection, and queue creation. GPU memory is managed by VMA.
  Why libraries for this: both are small, standard, and battle-tested
  to the point of being de-facto parts of the Vulkan ecosystem — VMA
  in particular is shipped by practically every serious Vulkan
  application in existence, and vk-bootstrap encodes the one
  correct way to do the startup ceremony that every application
  otherwise rewrites by hand. This is boilerplate with well-known
  correct answers; reinventing it would add risk and zero value.
- The swapchain runs with two frames in flight (a single-frame
  experiment is planned for VRR latency testing). Each in-flight
  frame owns a bundle of one fence, one acquire semaphore, one
  render-done semaphore, and one command buffer. The recreation state
  machine — which events trigger a swapchain rebuild, and how
  in-flight GPU work is drained safely first — follows the canonical
  pattern distilled from the reference study ([Appendix C §2](#2-swapchain-lifecycle-the-canonical-state-machine)). Present
  modes map from the vsync setting as described in [decision 8](#decision-record-all-locked-2026-07-0304), and
  where the driver offers `VK_EXT_swapchain_maintenance1`, vsync can
  be toggled without rebuilding the swapchain at all ([Appendix C §4](#4-runtime-vsync-switching)).
- **The acquire never blocks.** We call `vkAcquireNextImageKHR` with
  a zero timeout; if no swapchain image is free (`VK_NOT_READY` or
  `VK_TIMEOUT`), the present is simply skipped — counted and
  trace-logged — and the emulation loop continues immediately. This
  is the property that guarantees the display can never stall
  emulation. As a bonus regression signal, the existing
  `adjust_ticks_after_present_frame` compensation should measure
  approximately zero on this backend; if it ever reads non-zero, a
  blocking path has crept back in.
- Uploads use **one host-visible staging slot per in-flight frame**
  (two slots), each sized to the maximum source dimensions at four
  bytes per pixel. Each frame's pixels are written into the current
  frame's slot and copied from there into a source-dimension
  `B8G8R8A8_UNORM` sampled image — our BGRX32 pixels upload directly,
  no swizzling needed. The image is recreated when the source size
  changes, and the doubling flags are handed to the shader pipeline
  exactly as the OpenGL backend does today.
  Why per-frame slots and not a single buffer: with two frames in
  flight, frame N's GPU copy may not have executed yet when the CPU
  writes frame N+1's pixels — a single buffer would be overwritten
  mid-read. The per-frame slot is protected by the frame fence the
  ring already waits on, at zero extra synchronisation cost.
  Why not the emulators' fence-tracked streaming *rings*: those exist
  because a 3D emulator's per-frame transfer volume is unbounded and
  unpredictable — game texture-cache uploads, vertex/index streams,
  uniform data, hundreds of variably-sized transfers per frame — so
  they need dynamic sub-allocation, wrap-around offset tracking, and
  a reclamation deque ([Appendix C §5](#5-upload-path): Dolphin reserves 32 MB for
  this). Our transfer profile is the degenerate case the ring
  machinery generalises: a *known, fixed* number of transfers per
  frame (the source frame, later joined by the OSD overlay) of
  *known, bounded* sizes. Fixed slots per in-flight frame provide the
  same safety with none of the allocator complexity. If the transfer
  profile ever stops being fixed, a ring is the known upgrade path.
- The pipeline executor builds one `VkPipeline` per shader pass
  (vertex plus fragment stage from SPIR-V). Intermediate images are
  created with `COLOR_ATTACHMENT | SAMPLED` usage, in 8-bit or float
  format according to the shader's `float_output` pragma, and a grid
  of samplers covering every filter/wrap combination is created once
  up front. Each pass has one uniform buffer whose contents are
  packed by the shared UniformPacker (std140 offsets computed from
  the parsed shader metadata; SPIRV-Cross reflection double-checks
  those offsets in debug builds — [Appendix C §6](#6-shader-pipeline-executor-retroarch-slang--same-genre-as-ours)). Descriptor sets are
  **static**: written when the pipeline is (re)built and never
  touched per frame — our pass graph is fixed between rebuilds, which
  lets us skip the per-draw descriptor churn the emulators need. The
  final pass renders into a persistent **viewport-sized target
  image** carrying the same `output_stale` clean-present optimisation
  as the OpenGL backend: when nothing changed, presents re-blit the
  cached target instead of re-running the shader chain. Presentation
  itself is a letterbox clear plus a blit from the target to the
  swapchain image, with the OSD compositing point after that blit
  (see ["OSD readiness"](#osd-readiness-design-constraints-only--no-osd-is-implemented) below — `// TODO(osd)` markers go into the
  code at the injection points).
- Captures (`ReadPixelsPostShader`) copy the target image into a
  host-visible buffer and wait on a fence. That wait is a stall, but
  a perfectly acceptable one at screenshot cadence.
- **Present timing**: when `VK_EXT_present_timing` is available, the
  vretrace-driven present is submitted immediately with an absolute
  target timestamp (`tick_boundary + PIC_TickIndex() × 1 ms`), and
  the extension's feedback — the measured time each frame actually
  reached the glass — drives drift correction, missed-window
  detection, and the pacing logs. When the extension is absent, the
  backend runs on the [PR 1](#pr-1--deferred-present-scheduler--pacing-instrumentation) app-timed scheduler tier instead.
- For CI, device selection can be forced to lavapipe (Mesa's software
  Vulkan implementation), which makes real rendered golden-image
  tests of the shader pipeline possible on every commit, with no GPU
  in the runner.

**Metal backend** (`render/metal/`, namespace `Metal`, Objective-C++ —
the repo already contains Obj-C++ in `macos_colorspace.mm`, so there
is precedent). Structurally it mirrors the Vulkan backend but is
considerably simpler, because Metal has no descriptor sets, no
render-pass objects, and no manual image-layout transitions
([Appendix C §8](#8-metal-notes-dolphin)). The window's content is a `CAMetalLayer`; each pass
becomes an `MTLRenderPipelineState`; shaders arrive as MSL generated
at runtime by SPIRV-Cross; uploads go straight into shared-storage
textures on unified-memory hardware, with no staging buffer at all;
and `nextDrawable` returning nil is handled as a skipped present,
mirroring the Vulkan acquire-skip. Scheduled presents use
`present(atTime:)`, with `addPresentedHandler` providing the
glass-time feedback — the exact mechanism proven working on real
hardware in [Appendix D Spike 4](#spike-4--metal-scheduled-presents-on-real-hardware-passed).

Colour management on Metal is a real implementation, not a stub
(investigated 2026-07-05): the GL path tags the **NSWindow** with a
Display P3 colour space (`macos_colorspace.mm`) so macOS
colour-matches our internally-P3-managed output to the display
profile. That mechanism does not cover a `CAMetalLayer`, which Core
Animation colour-matches according to the layer's own `colorspace`
property instead — and which defaults to nil, meaning *no* colour
matching and silently wrong colours on wide-gamut Macs. The Metal
backend therefore implements `SetColorSpace` by assigning the layer's
`colorspace` (`kCGColorSpaceDisplayP3` / `kCGColorSpaceSRGB` per the
`color_space` setting). The shader-side half of colour management
(the image-adjustments pass) is shared code and needs nothing
Metal-specific. On Vulkan (Linux/Windows), the swapchain keeps the
standard `SRGB_NONLINEAR` surface colourspace, matching what the GL
path effectively does on those platforms today.

### OSD readiness (design constraints only — no OSD is implemented)

The future on-screen display shapes several present-path decisions,
so its constraints are recorded here and the code gets explicit
`// TODO(osd): compositing point — see gpu-backend-plan.md` markers
at the injection points (the present functions of the Vulkan and
Metal backends, after the letterbox blit).

How the OSD will work when it arrives: it is **rendered on the CPU
into a viewport-sized RGBA buffer** — which can be large (a 4K or 5K
fullscreen viewport) — then **uploaded every present** and
**alpha-blended over the emulated image**. Planned performance tweak
options: updating the OSD at half the present rate, and/or rendering
it at half resolution with bilinear or nearest-neighbour upscale
during the blend.

What our design must therefore keep possible (and does):

- **A second, viewport-sized streaming upload at present cadence.**
  The per-frame staging-slot architecture accommodates this directly
  — the OSD becomes a second fixed transfer per frame with its own
  per-in-flight-frame slot, sized on demand to the viewport. This is
  exactly why the upload design says "known, fixed number of
  transfers" rather than "one".
- **An alpha-blend draw over the swapchain image.** The swapchain is
  already created with `COLOR_ATTACHMENT` usage (in addition to
  `TRANSFER_DST` for the blit), so a blend pass after the letterbox
  blit needs no swapchain changes. Compositing after the blit also
  means rendered captures (which read the target image) exclude the
  OSD by construction — the long-standing design intent.
- **Half-rate updates**: the OSD texture is persistent, so skipping
  an upload simply re-blends the previous contents — falls out of
  the upload/present split for free.
- **Half-resolution + upscale**: a sampler choice at the blend draw
  (the filter/wrap sampler grid already exists).

Nothing further is built until OSD work actually starts; these
constraints plus the TODO markers are the whole deliverable.

### Shader format (450 spec — spike-refined, Appendix D)

Single file, `#if defined(VERTEX)/#elif defined(FRAGMENT)` sections,
all existing `#pragma` metadata unchanged (parser reused). Binding
model (ours, not SDL GPU's): one descriptor set —

```glsl
layout(set = 0, binding = 0, std140) uniform Uniforms {
        vec2 OUTPUT_SIZE;
        vec2 INPUT_SIZE_0;   // .. INPUT_SIZE_N
        // #pragma parameters follow, in declaration order, all floats
};
layout(set = 0, binding = 1) uniform sampler2D INPUT_TEXTURE_0; // .. N+1
```

- The anonymous block is declared ONCE above the stage sections
  (shared by both stages) — bodies keep referencing `INPUT_SIZE_0`
  etc. unqualified.
- **All UBO members must be float-typed** (vec2/float) — required by
  the GL `--flatten-ubo` emission (spike caveat: no int/float mixing).
- Explicit `layout(location = N)` on all varyings; vertex input stays
  `layout(location = 0) in vec2 a_position`.
- **`sampler` is a reserved word in 450** — the porting recipe renames
  any identifier named `sampler` (hit immediately in `sharp.glsl`).
- Uniform offsets: the **UniformPacker's own std140 computation over
  the canonical member order is the single source of truth on all
  backends** — it must be, because the GL path is toolchain-free at
  runtime and can never ask SPIRV-Cross (std140 is fully
  deterministic, so this is safe). On vulkan/metal, SPIRV-Cross
  reflection validates the computed offsets in debug builds
  (RetroArch-proven reflection, demoted to a checker).

### Offline GL 330 generation (spike-verified pipeline)

`scripts/tools/generate-gl-shaders.{sh,py}`, per shader:

1. `glslang -V -DVERTEX=1 -S vert -o v.spv shader.glsl` and
   `-DFRAGMENT=1 -S frag` (single-file sections work as-is).
2. `spirv-cross --version 330 --no-es --flatten-ubo [--flip-vert-y]
   v.spv/f.spv` → per-stage GLSL 330. UBO becomes
   `uniform vec4 Uniforms[1..k];` — stable name (from the block type),
   set with one `glUniform4fv` from the packed std140 buffer; **no
   UBOs, no `GL_ARB_shading_language_420pack` → macOS-GL-safe.**
3. Post-process: strip `layout(binding = N)` from sampler declarations
   (420pack-only qualifier); samplers bind by preserved name
   (`INPUT_TEXTURE_0`). Optionally strip the harmless 420pack `#ifdef`
   header.
4. Stitch the two stages back into the single-file `VERTEX`/`FRAGMENT`
   format with the `#pragma` comment block re-attached.
5. `--check` mode for CI: regenerate and diff; fail on any change.

Generated files are checked in. Consequence for the GL executor: the
by-name `glUniform*` calls (`Shader::SetUniform*`) are replaced by one
`glUniform4fv(loc("Uniforms"), k, packed)` per pass, sharing the
std140 packer with the native backends. `--flip-vert-y` verified
(appends `gl_Position.y = -gl_Position.y`); whether GL needs it is
decided by the [PR 5](#pr-5--single-source-shader-library) golden tests.

### GL-coupling refactors (verified findings, folded into PR 3)

- `ShaderManager` is welded to GL via its return type: `LoadShader()`
  returns `Shader`, which holds `GLuint program_object` + GL compile/
  uniform code (`private/shader.h:15-17`). Split into `ShaderSource`
  (text + parsed `ShaderInfo`, cached by the manager) and per-backend
  compiled objects owned by each pipeline.
- **`shader_pipeline.cpp` fuses two unrelated concerns in one class**
  — this is the central problem the [PR 3](#pr-3--shader-toolchain--vulkan-pipeline) refactor exists to fix.
  Today's `ShaderPipeline` contains both the backend-agnostic
  mathematics of the pass graph (which passes run, in what order, at
  which output sizes, under the `Previous|Rendered|VideoMode|Viewport`
  semantics, with which staleness rules) *and* the OpenGL execution
  of that graph (FBOs, textures, `glUniform` calls, draw calls) —
  see `ShaderPipeline::Render(const GLuint vao)` taking a raw GL
  handle, and `ShaderPass` mixing agnostic data with GL texture/FBO
  handles. Fused like this, the graph logic cannot be reused by the
  new backends and cannot be unit-tested without a live GL context.
  The split: the mathematics moves to `Shaders::PipelineTopology`
  (`render/shaders/pipeline_topology.*`, pure logic, unit-tested),
  and the GL-executing remainder becomes `render/opengl/
  opengl_pipeline.*` — one of three thin executors alongside
  `Vulkan::Pipeline` and `Metal::Pipeline`.
- Audit backend-type conditionals in `sdl_gui.cpp` (e.g. the macOS
  window-restore GL viewport hack ~`sdl_gui.cpp:2135`) for
  new-backend needs.

### What does not change

It is worth being explicit about the blast radius, because it is
small. The entire render layer upstream of `UploadFrame` is untouched:
the `render.cpp` / `RENDER_*` scanline flow, the source latch, dirty
tracking, pixel-format expansion, and the CPU deinterlacer all work
exactly as they do today, because every backend receives the same
expanded BGRX32 buffer. The `RenderBackend` interface itself is
unchanged until [PR 6](#pr-6--present-timing-the-flagship) adds one optional capability
(`PresentFrameAt`). The raw, upscaled, and video capture paths never
touch a backend and are unaffected. The OpenGL and texture backends
keep working throughout the series and remain as fallbacks
afterwards. The VGA emulation layer is not touched at all, and
capture cadence semantics (captures fire at DOS rate, never at
present rate) are preserved exactly.

## Target state — module layout and architecture

Maintenance note: this section is the source of truth for the
decomposition. Boxes in the diagram map 1:1 to file pairs in the
listing; call/ownership edges live in the "Relations" list below the
diagram (text, so refinements are cheap — ask and it gets
regenerated).

### Directory listing (after PR 7)

Layout per [decision 12](#decision-record-all-locked-2026-07-0304): one subdirectory per backend plus one for the
shared shader subsystem, each with its own `private/` directory for
internal headers; headers consumed across subdirectory boundaries sit
at their subdirectory's top level. Existing files move in a dedicated
move-only commit ([PR 2](#pr-2--vulkan-backend-skeleton-output--vulkan-no-shaders), commit 1). Markers: (NEW n) = created in
PR n; (CHG n) = modified in PR n; (MOVED) = relocated, content
unchanged; unmarked = untouched. Build gating in brackets.

```
src/gui/render/                    — render layer core (backend-independent)
│
├── README.md                      Short index into docs/        (CHG 2..7)
│
├── docs/                          Multi-part architecture documentation
│   │                              (kept current by every PR; finalised
│   │                              in PR 7)
│   │
│   ├── architecture.md            General module architecture (living
│   │                              version of this plan's listing,
│   │                              diagram, and relations)
│   │
│   ├── shaders.md                 Shader subsystem: 450 format,
│   │                              toolchain, topology, offline gen
│   │
│   ├── vulkan.md / metal.md /     Per-backend: swapchain lifecycle,
│   │   opengl.md / sdl.md         sync, present timing, quirks
│   │
│   └── img/                       Rendered diagrams (SVG/PNG) + their
│                                  editable sources
│
├── render.h / render.cpp          Scanline intake, source latch, dirty
│                                  tracking, palette, present-time
│                                  expansion, upload driving (RENDER_* API)
│
├── render_backend.h               RenderBackend abstract interface; grows
│                                  SupportsScheduledPresents() +
│                                  PresentFrameAt(due_us)            (CHG 6)
│
├── frame_dirty_tracker.h          Latch/upload generation tracking
│
├── format_convert.h / .cpp        Pixel-format expanders; gains the row
│                                  expand/double helper hoisted from the
│                                  SDL backend                       (CHG 2)
│
├── deinterlacer.cpp               FMV deinterlacer
│
├── private/
│   └── deinterlacer.h
│
├── shaders/                       — shared shader subsystem
│   │                                (namespace Shaders for new code)
│   │
│   ├── shader_common.h            ShaderInfo, ShaderSource, presets,
│   │                              enums (gains ShaderSource) (MOVED, CHG 3)
│   │
│   ├── shader_manager.h / .cpp    ShaderSource discovery/loading/caching,
│   │                              presets, inventory; per-backend source
│   │                              path resolution (450 vs gl330)
│   │                                                       (MOVED, CHG 3,5)
│   │
│   ├── pipeline_topology.h / .cpp Shaders::PipelineTopology — backend-
│   │                              agnostic pass graph: output sizes,
│   │                              input wiring, staleness rules;
│   │                              unit-tested without any GPU       (NEW 3)
│   │
│   ├── uniform_packer.h / .cpp    Shaders::UniformPacker — canonical
│   │                              std140 offsets + block packing;
│   │                              single source of truth for all
│   │                              three executors                   (NEW 3)
│   │
│   ├── shader_compiler.h / .cpp   Shaders::Compiler — glslang wrapper:
│   │                              450 → SPIR-V [C_VULKAN || C_METAL]
│   │                                                                (NEW 3)
│   │
│   ├── spirv_reflection.h / .cpp  Shaders::SpirvReflection — debug-build
│   │                              validation of the packer
│   │                              [C_VULKAN || C_METAL, optional]   (NEW 3)
│   │
│   ├── auto_shader_switcher.h/.cpp   crt-auto-* → shader resolution
│   │                                                               (MOVED)
│   │
│   ├── auto_image_adjustments.h/.cpp Mode-driven adjustment defaults
│   │                                                               (MOVED)
│   │
│   └── private/
│       └── shader_pragma_parser.h/.cpp  #pragma metadata → ShaderInfo
│                                                                   (MOVED)
│
├── opengl/                        — OpenGL 3.3 backend (legacy code:
│   │                                no namespace)
│   │
│   ├── opengl_renderer.h / .cpp   Backend: owns the GL executor +
│   │                              compiled-program cache; consumes the
│   │                              generated 330 tree      (MOVED, CHG 3,5)
│   │
│   ├── macos_colorspace.h / .mm   NSWindow Display P3 tagging (the GL
│   │                              path's colour management; Metal's
│   │                              equivalent lives in metal_renderer)
│   │                                                               (MOVED)
│   │
│   └── private/
│       │
│       ├── opengl_pipeline.h/.cpp GL pass executor: FBO chain, target
│       │                          FBO, clean-present skip, flattened-
│       │                          uniform upload (renamed from
│       │                          shader_pipeline.*)      (MOVED, CHG 3,5)
│       │
│       └── opengl_shader.h / .cpp GL program compile + uniform upload
│                                  (renamed from shader.*) (MOVED, CHG 3,5)
│
├── vulkan/                        — native Vulkan 1.3 backend
│   │                                (namespace Vulkan) [C_VULKAN]
│   │
│   ├── vulkan_renderer.h / .cpp   Vulkan::Renderer — the backend: owns
│   │                              device/swapchain/executor;
│   │                              non-blocking acquire; scheduled
│   │                              presents                       (NEW 2,6)
│   │
│   └── private/
│       │
│       ├── vulkan_device.h / .cpp Vulkan::Device — instance/device/
│       │                          queues/VMA via vk-bootstrap;
│       │                          validation layers in debug        (NEW 2)
│       │
│       ├── vulkan_swapchain.h/.cpp  Vulkan::Swapchain — swapchain +
│       │                          per-frame ring + recreation state
│       │                          machine + present-timing submits
│       │                                                          (NEW 2,6)
│       │
│       └── vulkan_pipeline.h/.cpp Vulkan::Pipeline — pass executor:
│                                  VkPipelines, dynamic rendering,
│                                  static descriptor sets, target
│                                  image                             (NEW 3)
│
├── metal/                         — native Metal backend (Obj-C++,
│   │                                namespace Metal) [C_METAL]
│   │
│   ├── metal_renderer.h / .mm     Metal::Renderer — the backend:
│   │                              CAMetalLayer (incl. colorspace =
│   │                              Display P3/sRGB), nil-drawable skip,
│   │                              present(atTime:)               (NEW 4,6)
│   │
│   └── private/
│       │
│       └── metal_pipeline.h / .mm Metal::Pipeline — pass executor:
│                                  PSOs, MSL via SPIRV-Cross
│                                  CompilerMSL, target texture      (NEW 4)
│
└── sdl/                           — SDL_Renderer backend (legacy code:
    │                                no namespace)
    │
    └── sdl_renderer.h / .cpp      Backend ("texture"/"texturenb"):
                                   CPU doubling, no shaders; fallback
                                   of last resort          (MOVED, CHG 2)

src/libs/vk-bootstrap/             Vendored (MIT; not distro-packaged;
                                   designed for vendoring)           (NEW 2)

resources/shaders/                 450 sources — the ONLY hand-edited
├── _internal/ crt/                shader files                      (CHG 5)
├── interpolation/ scaler/
│
└── gl330/                         Generated GL 3.30 mirror tree —
                                   never hand-edited; CI --check     (NEW 5)

scripts/tools/generate-gl-shaders.py   Offline 450→330 generation +
                                       --check mode                  (NEW 5)
```

Present scheduler note: [PR 1](#pr-1--deferred-present-scheduler--pacing-instrumentation)'s scheduler state and due-time logic
live in `sdl_gui.cpp`/`dosbox.cpp` (presentation control), not in
this module — see the [PR 1](#pr-1--deferred-present-scheduler--pacing-instrumentation) spec.

### Architecture diagram

One box per class/component; key responsibilities inside; ownership
and call edges in the Relations list below.

```
════════ EMULATION & PRESENTATION CONTROL (outside src/gui/render/) ════════

 ┌─────────────────────────────┐        ┌─────────────────────────────────┐
 │ Main loop (dosbox.cpp)      │        │ GFX presentation (sdl_gui.cpp)  │
 ├─────────────────────────────┤        ├─────────────────────────────────┤
 │ · normal_loop present poll  │◀──────▶│ · GFX_EndUpdate (vretrace)      │
 │ · governor sleep + split    │        │ · GFX_SchedulePresent (PR 1)    │
 │ · PIC_TickIndex due times   │        │ · GFX_MaybePresentFrame         │
 │ · ticks/fast-forward state  │        │ · scheduler state + pacing stats│
 └─────────────────────────────┘        └─────────────────────────────────┘

════════ RENDER LAYER (backend-independent) ════════════════════════════════

 ┌─────────────────────────────┐        ┌─────────────────────────────────┐
 │ render.cpp (RENDER_*)       │        │ «interface» RenderBackend       │
 ├─────────────────────────────┤        ├─────────────────────────────────┤
 │ · scanline intake + latch   │        │ · UploadFrame(BGRX32,…)         │
 │ · FrameDirtyTracker         │        │ · PresentFrame /                │
 │ · FormatConvert expanders   │        │   PresentFrameAt(due) (PR 6)    │
 │ · Deinterlacer (display)    │        │ · SetShader/presets/adjustments │
 │ · palette handling          │        │ · SetVsync · ReadPixelsPostShdr │
 └─────────────────────────────┘        └─────────────────────────────────┘

════════ BACKENDS (implement RenderBackend) ════════════════════════════════

 ┌──────────────┐ ┌───────────────┐ ┌────────────────────┐ ┌─────────────────┐
 │ SdlRenderer  │ │ OpenGlRenderer│ │ Vulkan::Renderer   │ │ Metal::Renderer │
 ├──────────────┤ ├───────────────┤ ├────────────────────┤ ├─────────────────┤
 │ SDL_Renderer │ │ GL 3.3 core   │ │ window+surface     │ │ CAMetalLayer    │
 │ CPU doubling │ │ window+ctx    │ │ acquire(t=0)→skip  │ │ nextDrawable    │
 │ no shaders   │ │ vsync via     │ │ vsync/present modes│ │  nil→skip       │
 │ last-resort  │ │ swap interval │ │ VK_EXT_present_    │ │ present(atTime:)│
 │ fallback     │ │               │ │  timing (PR 6)     │ │ P3 colorspace   │
 └──────────────┘ └───────┬───────┘ └─────┬──────────────┘ └────────┬────────┘
                          │               │                         │
                          │      ┌────────┴──────────┐              │
                          │      │ Vulkan::Device    │              │
                          │      ├───────────────────┤              │
                          │      │ vkb: instance/    │              │
                          │      │ device/queues     │              │
                          │      │ VMA allocator     │              │
                          │      │ validation layers │              │
                          │      └───────────────────┘              │
                          │      ┌───────────────────┐              │
                          │      │ Vulkan::Swapchain │              │
                          │      ├───────────────────┤              │
                          │      │ per-frame ring    │              │
                          │      │ recreation SM     │              │
                          │      │ timed presents    │              │
                          │      └───────────────────┘              │
                          ▼                ▼                        ▼
 ┌────────────────┐ ┌────────────────────────┐ ┌────────────────────────┐
 │ OpenGlPipeline │ │ Vulkan::Pipeline       │ │ Metal::Pipeline        │
 ├────────────────┤ ├────────────────────────┤ ├────────────────────────┤
 │ FBO pass chain │ │ VkPipelines (dynamic   │ │ MTLRenderPipelineStates│
 │ OpenGlShader   │ │  rendering), static    │ │ MSL via CompilerMSL    │
 │  programs      │ │  descriptor sets,      │ │ setFragmentBytes UBOs  │
 │ glUniform4fv   │ │  mapped UBOs,          │ │ sampler states         │
 │  flattened UBO │ │  sampler grid          │ │ target texture +       │
 │ target FBO +   │ │ target image +         │ │  clean-present skip    │
 │  clean-present │ │  clean-present skip    │ │ embedded blit shader   │
 │  skip          │ │                        │ │  for letterbox present │
 └───────┬────────┘ └───────────┬────────────┘ └───────────┬────────────┘
         │                      │                          │
════════ SHARED SHADER SUBSYSTEM (backend-agnostic) ═══════════════════════
         ▼                      ▼                          ▼
 ┌──────────────────┐ ┌───────────────────────────┐ ┌───────────────────────────┐
 │ ShaderManager    │ │ Shaders::PipelineTopology │ │ Shaders::UniformPacker    │
 ├──────────────────┤ ├───────────────────────────┤ ├───────────────────────────┤
 │ source discovery │ │ pass graph: output sizes, │ │ canonical std140 offsets  │
 │ + caching        │ │ input wiring, staleness   │ │ + block packing — source  │
 │ presets,inventory│ │ rules; unit-tested        │ │ of truth for ALL backends │
 │ 450/gl330 path   │ │ without any GPU           │ │                           │
 │ resolution       │ │                           │ │                           │
 └──────────────────┘ └───────────────────────────┘ └───────────────────────────┘
 ┌──────────────────┐ ┌───────────────────────────┐ ┌───────────────────────────┐
 │ShaderPragmaParser│ │ Shaders::Compiler         │ │ Shaders::SpirvReflection  │
 ├──────────────────┤ ├───────────────────────────┤ ├───────────────────────────┤
 │ #pragma comments │ │ [vulkan + metal]          │ │ [vulkan + metal; debug,   │
 │ → ShaderInfo     │ │ glslang: 450 → SPIR-V     │ │  optional] validates the  │
 │                  │ │                           │ │ UniformPacker's offsets   │
 └──────────────────┘ └───────────────────────────┘ └───────────────────────────┘
 ┌──────────────────┐ ┌───────────────────────────┐
 │AutoShaderSwitcher│ │ AutoImageAdjustments      │
 ├──────────────────┤ ├───────────────────────────┤
 │ crt-auto-* mode  │ │ per-mode defaults         │
 │ resolution       │ │                           │
 └──────────────────┘ └───────────────────────────┘
```

### Relations (call/ownership edges)

- `render.cpp` → `RenderBackend.UploadFrame` with the expanded BGRX32
  buffer + doubling flags (once per dirty present; deinterlacer runs
  first, in place).
- GFX presentation (`sdl_gui.cpp`) → `RenderBackend.PresentFrame`
  (poll/scheduler tier) or `.PresentFrameAt` (timed tier, [PR 6](#pr-6--present-timing-the-flagship));
  `GFX_CaptureRenderedImage` → `.ReadPixelsPostShader` (reads the
  target image/FBO).
- Main loop ↔ GFX presentation: `normal_loop` polls
  `GFX_MaybePresentFrame` both modes; `increase_ticks` splits its
  governor sleep around the pending due time; vretrace calls
  `GFX_SchedulePresent`.
- Each backend **owns** its executor (`OpenGlPipeline` /
  `Vulkan::Pipeline` / `Metal::Pipeline`); `Vulkan::Renderer`
  additionally owns `Vulkan::Device` and `Vulkan::Swapchain`.
  `SdlRenderer` owns no shader machinery.
- Executors → `ShaderManager` (get `ShaderSource` by resolved name)
  → `Shaders::PipelineTopology` (pass descriptions from sources +
  sizes + flags) → `Shaders::UniformPacker` (offsets + packed
  bytes). Vulkan/Metal executors additionally → `Shaders::Compiler`
  (SPIR-V) and, in debug, → `Shaders::SpirvReflection`;
  `Metal::Pipeline` runs SPIRV-Cross `CompilerMSL` on the SPIR-V.
  `OpenGlPipeline` compiles the generated 330 text via
  `OpenGlShader` — no runtime toolchain.
- `ShaderManager` → `ShaderPragmaParser` at source load;
  `AutoShaderSwitcher`/`AutoImageAdjustments` drive
  `RenderBackend.SetShader`/adjustment setters on mode changes
  (unchanged mechanism).
- `Vulkan::Swapchain` → `Vulkan::Device` (queues, VMA); executors write
  their final pass into the persistent target (image/FBO/texture)
  that both the present blit and `ReadPixelsPostShader` consume —
  the clean-present skip is a staleness bit on that target, uniform
  across backends.

## PR 1 — deferred-present scheduler + pacing instrumentation
## (EXECUTION-GRADE SPEC)

Backend-independent — it operates above the `RenderBackend`
interface, so it improves the existing OpenGL *and* SDL texture
backends on VRR today; it later becomes the
permanent fallback tier under the native backends. The existing
`GFX_EndUpdate` comment (sdl_gui.cpp:1104-1124) documents today's
1–5 ms present jitter and asks for exactly this work.

### Today's waits, mapped (verified against code)

- **Governor sleep** — `increase_ticks()` (dosbox.cpp:637): when all
  tick work is done and wall clock hasn't reached the next 1 ms
  boundary (`ticks_new <= ticks.last`, line 660), sleeps to the next
  tick boundary via `SDL_DelayPrecise` (line 670) and credits the
  slept time back into `ticks.done` (lines 673-689). This is the
  real-time lock — necessary, stays.
- **Present timing today** — dos-rate presents fire inside
  `GFX_EndUpdate()` (sdl_gui.cpp:1093-1127) from the VGA vretrace
  event, i.e. mid-tick-burst: up to ~1 ms early-biased wall-clock
  jitter (the comment's "1-5 ms"). Host-rate presents are polled from
  `normal_loop`'s empty-PIC-queue branch (dosbox.cpp:602-604) with a
  3 ms early window (`setup_presentation_mode`, sdl_gui.cpp:489-532)
  to hide GL's blocking swap.
- **Tick compensation** — `adjust_ticks_after_present_frame`
  (sdl_gui.cpp:2339-2356) subtracts accumulated present time from
  `ticks.done` via `DOSBOX_SetTicksDone`.

### Design

New state in `sdl.presentation` (gui/private/sdl_gui.h):

```cpp
// Deferred-present scheduler (dos-rate mode)
std::optional<int64_t> pending_present_due_us = {};
```

New/changed functions:

1. **`GFX_SchedulePresent()`** (new, sdl_gui.cpp; declared in
   gui/common.h): computes the wall-clock due time of the current
   emulated moment and arms the pending present:
   `due_us = DOSBOX_GetLastTickStartUs() + llround(PIC_TickIndex() * 1000.0)`.
   If `due_us < now` (emulation running behind) present immediately.
2. **`DOSBOX_GetLastTickStartUs()`** (new, dosbox.cpp; declared in
   dosbox.h): returns `ticks.last * 1000` — the wall-clock µs of the
   tick currently being emulated. (`ticks.last` is the ms tick counter
   the loop advances at dosbox.cpp:706.)
3. **`GFX_EndUpdate()`** (sdl_gui.cpp:1093): the dos-rate branch calls
   `GFX_SchedulePresent()` instead of `GFX_MaybePresentFrame()`.
   Exception: when `DOSBOX_IsPaused()` or fast-forward is active
   (new `DOSBOX_IsFastForwarding()` accessor over `ticks.locked`,
   dosbox.cpp:643/929/942), present immediately — due-times are
   meaningless in both states.
4. **`GFX_MaybePresentFrame()`** (sdl_gui.cpp:2385): dos-rate path
   presents when `pending_present_due_us && now >= due` (clearing the
   pending state), OR when `force_present` (capture sync, line 2393),
   OR when called with no pending arm from the direct repaint paths —
   `GFX_ResetScreen` pause repaint (sdl_gui.cpp:343) and
   `paused_tick` (dosbox.cpp:534) — which keep today's
   present-immediately semantics. Host-rate path unchanged.
5. **`normal_loop`** (dosbox.cpp:602): drop the
   `== PresentationMode::HostRate` gate — poll
   `GFX_MaybePresentFrame()` in both modes (2-5 polls/ms → ~0.2-0.5 ms
   submit granularity for armed presents).
6. **Governor sleep split** — `increase_ticks()` (dosbox.cpp:667-671):
   before sleeping, query `GFX_GetTimeToPendingPresentUs()` (new;
   returns µs-until-due or -1). If `0 <= t < sleep_us`:
   `SDL_DelayPrecise(t)` → `GFX_MaybePresentFrame()` → sleep the
   remainder. The slept-time accounting (lines 673-689) is unchanged
   (measures total elapsed either way).
7. **Instrumentation** (behind `#define DEBUG_PRESENT_PACING 0`,
   per code-style rules): replace the `#if 0` block at
   sdl_gui.cpp:2412-2424 with: rolling present-interval stats
   (last 128 deltas: mean/p95/max vs `frame_time_us`), armed→submit
   latency, immediate-vs-scheduled counts, and the cumulative
   `adjust_ticks` total (expect ≈ 0 on native backends later).

Interactions preserved (verified): captures stay dirty-ungated and
DOS-rate (`force_present` bypasses scheduling); pause repaints present
immediately; `RENDER_MaybeUploadFrame()` stays inside
`GFX_MaybePresentFrame` (2406) so upload cost lands at submit time;
host-rate keeps its 3 ms early window on GL (revisit on native
backends, [PR 7](#pr-7--auto--polish)).

### Commit breakdown (each compiles, `compile-commits.sh` green)

1. `Expose tick-start time and fast-forward state from the main loop`
   — `DOSBOX_GetLastTickStartUs()`, `DOSBOX_IsFastForwarding()` +
   unit test for the due-time computation (pure function).
2. `Schedule dos-rate presents at their emulated due time` — items
   1, 3, 4, 5 above. Manual verify: VRR smooth-scroll (Keen/Pinball
   Dreams class) equal or better; captures byte-identical; pause +
   resize repaint intact; host-rate behaviour unchanged.
3. `Split the governor sleep around pending presents` — item 6 +
   fast-forward rule. Verify: no added stalls at high cycles; turbo
   mode presents sanely.
4. `Add present-pacing instrumentation` — item 7 + learning-doc
   chapter.

## Decisions made during flesh-out (John: review these)

Choices I had to make to reach execution grade; none were discussed
explicitly. Veto/adjust before implementation:

1. **Vulkan 1.3 baseline with dynamic rendering** — no
   `VkRenderPass`/`VkFramebuffer` objects, considerably less code.
   Mesa/NVIDIA/AMD/Intel all ship 1.3 on anything relevant; pre-1.3
   hardware uses `opengl`/`texture`.
2. **Vulkan function loading via SDL** —
   `SDL_Vulkan_LoadLibrary`/`SDL_Vulkan_GetVkGetInstanceProcAddr`
   (SDL already owns the loader); volk only if vk-bootstrap
   integration turns out awkward.
3. **Shader directory layout**: `resources/shaders/**.glsl` become
   the 450 sources; generated GL files live in a parallel
   `resources/shaders/gl330/` tree. `ShaderManager` resolves
   name → path per backend; user shader dirs keep per-backend formats
   (GL users' custom 330 shaders keep working untouched).
4. **The Metal PR lands after the toolchain**, so it ships with full
   pipeline support from day one; its letterbox present uses one tiny
   embedded MSL blit shader (Metal's blit encoder cannot scale — a
   shader-free skeleton is impossible there, unlike Vulkan).
5. **`RenderBackend` grows a scheduled-present capability in [PR 6](#pr-6--present-timing-the-flagship)**:
   `SupportsScheduledPresents()` (default false) +
   `PresentFrameAt(due_time_us)` — the only interface change of the
   whole series.
6. **Swapchain images request `TRANSFER_DST` usage** so [PR 2](#pr-2--vulkan-backend-skeleton-output--vulkan-no-shaders)'s
   shader-free present is pure clear+blit commands.
7. **Uniform update policy**: UBO contents change only at pipeline
   rebuild / settings change (pass output is a pure function of input
   texture + settings — the clean-present invariant); Vulkan uses one
   persistently-mapped UBO per pass, Metal uses
   `setVertexBytes`/`setFragmentBytes` (blocks are tiny).
8. **Lavapipe CI runs under xvfb** — swapchains need a real window
   system; headless-pure Vulkan can't exercise WSI.

## Attribution policy

Crediting where ideas come from is house policy for this series —
both because it's the right thing to do and because future
maintainers deserve a provenance trail.

- **Adapted code or data** — anything that started life in another
  repo (vendor ID lists, quirk tables, workaround logic, struct
  layouts): only from GPL-2.0-compatible sources (Dolphin GPLv2+,
  PPSSPP GPLv2+, RPCS3 GPLv2, Khronos Vulkan-Samples Apache-2.0). The
  file gains an `SPDX-FileCopyrightText` line for the upstream
  project alongside ours — the same convention the repo already uses
  for shader authors (`sharp.glsl` credits Hyllian and jmarsh) — plus
  a short comment at the site naming project + file so the origin is
  findable.
- **Ideas and patterns** (recreation state machines, semaphore-pool
  sizing, feedback-validation approaches): a one-line credit comment
  at the site, e.g. `// Per-vendor feedback trust from RPCS3
  (vkutils/swapchain.cpp): NVIDIA/ANV report present timing
  reliably; AMD/RADV/MoltenVK don't.` Ideas aren't copyrightable, so
  this applies equally to the patterns-only projects (RetroArch
  GPLv3, PCSX2 GPLv3, DuckStation no-derivatives) — credit the idea,
  never transcribe their code.
- **The licence firewall (hard rule for the implementation phase)**:
  the study phase is over, and its findings from the
  licence-incompatible projects live in this document as designs
  described in our own words. During implementation it is
  **prohibited to open, consult, or copy source code from
  DuckStation, RetroArch, or PCSX2** — anyone implementing works from
  this plan, not from those repositories. If the plan turns out to be
  missing a detail that only those codebases can answer, the correct
  move is to extend the *plan* first (a described design, in our own
  words, with the idea credited) and then implement from the updated
  plan — never to code with their source open. Dolphin, PPSSPP,
  RPCS3, and Vulkan-Samples remain fair to consult and adapt from,
  with SPDX credit when code or data is taken.
- **Vendor workarounds specifically** must carry: source project, the
  observed behaviour, and driver/OS versions when known — a
  workaround without provenance becomes unremovable cargo cult.
- The learning doc keeps citing everything as it already does; this
  policy is about the code itself.
- **The credit bar (what qualifies for attribution at all)**: we
  credit *genuinely innovative ideas and hard-won quirk knowledge* —
  things a project invented or paid for in debugging hours (RPCS3's
  vendor trust table, DuckStation's semaphore-sizing rule, RetroArch's
  WSI fault injection). We do **not** credit textbook techniques that
  any competent developer gets from introductory Vulkan/Metal
  materials, official Khronos samples, or vendor documentation
  (frames-in-flight structure, standard swapchain recreation, storage
  -mode selection). The test: *could this have been learned from
  vulkan-tutorial/vkguide, the Khronos samples, or Apple's docs
  alone?* If yes, no credit entry — over-crediting dilutes the real
  credits and falsely implies stronger derivation than exists.
  Licence obligations for actually-copied code apply regardless of
  this bar.
- **Gratitude is part of the policy.** This project owes a real debt
  to the emulator developers whose work we studied — Dolphin, PPSSPP,
  RetroArch, DuckStation, PCSX2, RPCS3, and the Khronos samples
  authors. Every design in this plan stands on lessons they learned
  the hard way, and we say so explicitly: **thank you.**
- **`gpu-backend-attribution.md` is a deliverable**, maintained as
  implementation proceeds (seeded already, alongside this plan). It
  gives public shout-outs to the original developers — or at minimum
  the project — for every idea, pattern, or piece of code we lifted,
  with a clear indication of *what* was taken, from *where*, under
  which licence, and where it landed in our code. The maintenance
  rule: whenever a credit comment or SPDX line is added to the code,
  the corresponding entry in the attribution file is updated in the
  same commit (planned entries from the study are pre-listed and get
  flipped to "landed" with their commit hash).

Known upcoming attribution sites from the study (all clearing the
credit bar): RPCS3's present-timing vendor trust table and
NVIDIA-minimise quirk ([PR 2](#pr-2--vulkan-backend-skeleton-output--vulkan-no-shaders)/
[PR 6](#pr-6--present-timing-the-flagship), GPLv2 — adaptable with SPDX credit); DuckStation's
acquire-semaphore sizing rule and Metal `atTime:` approach ([PR 2](#pr-2--vulkan-backend-skeleton-output--vulkan-no-shaders)/
[PR 6](#pr-6--present-timing-the-flagship), idea credit only); RetroArch's WSI fault-injection testing idea
([PR 2](#pr-2--vulkan-backend-skeleton-output--vulkan-no-shaders) tests, idea credit). The Khronos Vulkan-Samples sit *below*
the credit bar by definition — they are the official reference
material whose entire purpose is to be followed — so using their
patterns needs no idea credit; only directly adapted sample code gets
the Apache-2.0 SPDX line.

## Implementation order — execution-grade specs

Standing constraints for every PR in this series:

- **Licence firewall**: implementation works from this document only.
  Consulting or copying source from DuckStation, RetroArch, or PCSX2
  is prohibited ([decision 11](#decision-record-all-locked-2026-07-0304) and the [Attribution policy](#attribution-policy) have the full
  rule); Dolphin, PPSSPP, RPCS3, and Vulkan-Samples may be consulted
  and adapted from with attribution.
- Every commit compiles individually (`compile-commits.sh` green),
  and every commit appends its learning-doc chapter.
- Every commit that adopts an idea, pattern, or (licence-compatible)
  code from a studied project updates `gpu-backend-attribution.md` in
  the same commit ([Attribution policy](#attribution-policy)).
- New code follows the module layout, namespace, and modern-C++
  conventions of [decision 12](#decision-record-all-locked-2026-07-0304).
- **Architecture documentation is a deliverable, kept current as the
  work proceeds.** The end state is a multi-part documentation set
  under `src/gui/render/docs/`: `architecture.md` (the general module
  architecture — the [target-state listing](#target-state--module-layout-and-architecture),
  diagram, and relations from this plan become its living version),
  `shaders.md` (the shader subsystem: the 450 format, the toolchain,
  the pipeline topology, the offline generation), and one part per
  backend (`vulkan.md`, `metal.md`, `opengl.md`, `sdl.md` — swapchain
  lifecycle, sync structure, present timing, backend quirks).
  Diagrams are produced as **rendered images, not just ASCII charts**
  — with their editable sources checked in next to the exported
  SVG/PNGs so they never become unmodifiable (tool choice at
  implementation; the render must be regenerable). `render/README.md`
  becomes a short index into `docs/`. Each PR updates the parts it
  touches; [PR 7](#pr-7--auto--polish) finalises the set.

### PR 2 — Vulkan backend skeleton (`output = vulkan`, no shaders)

The goal of this PR is feature parity with the `texture` backend, but
running on native Vulkan on Linux and Windows. No shaders are
involved yet — the whole present path is copies, clears, and blits —
which keeps the PR focused on the genuinely hard part: device,
swapchain, and synchronisation lifecycle.

New files: `render/vulkan/vulkan_renderer.{h,cpp}` (the
`RenderBackend` implementation, class `Vulkan::Renderer`),
`render/vulkan/private/vulkan_device.{h,cpp}` (`Vulkan::Device` —
instance, device, queues, and the VMA allocator, built on
vk-bootstrap), and `render/vulkan/private/vulkan_swapchain.{h,cpp}`
(`Vulkan::Swapchain` — the swapchain, the per-frame sync ring, and
the recreation state machine).

Commits:

1. `Reorganise the render module into per-backend subdirectories` —
   the [decision 12](#decision-record-all-locked-2026-07-0304) move-only commit: every existing render file moves
   to its new home (`opengl/`, `sdl/`, `shaders/`, plus the core
   files staying at the top), and the two OpenGL renames happen here
   (`shader_pipeline.* → opengl/private/opengl_pipeline.*`,
   `shader.* → opengl/private/opengl_shader.*`). The only content
   changes permitted are the mechanical ones moves force: include
   paths and the CMake source lists. No logic changes, no class
   renames (those come with [PR 3](#pr-3--shader-toolchain--vulkan-pipeline)'s refactor). Verification: the build
   is green, the unit tests pass, and `git log --follow` tracks every
   file's history across the move.
2. `build: add vk-bootstrap and VMA dependencies` — vk-bootstrap is
   vendored into `src/libs/` (it is not packaged by any major Linux
   distro, but it is a small MIT-licensed library explicitly designed
   for vendoring — [decision 5](#decision-record-all-locked-2026-07-0304)). VMA and `vulkan-headers` come from
   vcpkg for our builds; on system-libs builds VMA uses the distro
   package where one exists and falls back to a bundled copy of its
   single header. There is no Vulkan loader dependency at all,
   because SDL already loads the Vulkan library for us ([decision 2](#decision-record-all-locked-2026-07-0304) in
   the flesh-out list). This commit also adds the `OPT_VULKAN` CMake
   option (default ON for Linux and Windows, OFF for macOS), which
   defines `C_VULKAN` via `dosbox_config.h.in.cmake`, and enables the
   option in the Linux and Windows CI workflows. Verification: the
   project compiles with the option both ON and OFF on all three
   platforms.
3. `Add the vulkan output backend enum and config plumbing` — adds
   `RenderBackendType::Vulkan` to the enum in
   `gui/private/common.h:70`, teaches `configure_renderer()` to parse
   `output = vulkan` (with the value registered in the setting's
   `Set_values` list only on the platforms that support it, plus help
   text), and extends `create_renderer()` with the fallback chain
   vulkan → opengl → texture, following the shape of the existing
   opengl → texture fallback (`sdl_gui.cpp:1523-1570`). At this point
   the backend class doesn't exist yet, so construction fails cleanly
   into the fallback by design. Verification: `output = vulkan` boots
   via the fallback with the expected warning in the log.
4. `Add the Vulkan device and window bring-up` — creates the SDL
   window with the `SDL_WINDOW_VULKAN` flag (following the
   window-properties pattern in `sdl_renderer.cpp:42`) and the
   surface via `SDL_Vulkan_CreateSurface`. vk-bootstrap then builds
   the instance (validation layers enabled in debug builds, with
   their messages routed to `LOG_WARNING`), selects a physical device
   (requirements: Vulkan 1.3, the swapchain extension, and a queue
   family that can do both graphics and present), and creates the
   logical device; the VMA allocator is created last. Destruction
   order is exercised deliberately under validation. Verification:
   the program boots to a black window and exits with a clean
   validation log.
5. `Add the swapchain and per-frame ring` — swapchain creation
   prefers `BGRA8_UNORM`, uses `minImageCount + 1` images for FIFO
   and `max(minImageCount + 2, 3)` for MAILBOX, requests
   `TRANSFER_DST | COLOR_ATTACHMENT` usage (so the shader-free
   present can blit into it), and passes `oldSwapchain` on
   recreation. The sync structure: a ring of two in-flight frames
   (fence plus command pool and buffer each), an acquire-semaphore
   pool sized to frames-in-flight *plus one* (a semaphore cannot be
   reused until its acquire has provably completed — the DuckStation
   sizing rule, [Appendix C §9](#9-duckstation-patterns-only--no-derivatives-licence)), and one render-done semaphore per
   swapchain image. The recreation state machine implements the
   study's canonical pattern ([Appendix C §2](#2-swapchain-lifecycle-the-canonical-state-machine) and [§10](#10-pcsx2-and-rpcs3-added-2026-07-04-new-findings-only)): `OUT_OF_DATE`
   at acquire or present triggers a rebuild; `SUBOPTIMAL` finishes
   the current frame and rebuilds lazily; a zero-size extent
   (minimised window) skips all presentation *without* rebuilding —
   the NVIDIA-minimise quirk makes rebuilding in that state
   dangerous; a redundancy gate skips rebuilds when nothing actually
   changed; and old swapchains are destroyed deferred, once their
   last frame's fence has signalled. Where the driver offers
   `VK_EXT_swapchain_maintenance1` and `surface_maintenance1`, they
   are enabled for per-present fences and recreation-free
   present-mode switching.
6. `Implement frame upload and the blit present path` — the upload
   uses one host-visible VMA staging slot per in-flight frame, grown
   on demand to the source dimensions times four bytes (see the
   architecture section for why per-frame slots rather than a single
   buffer or a streaming ring). `UploadFrame` reuses
   the CPU row-doubling helper that currently lives in `SdlRenderer`
   (hoisting it into `format_convert.{h,cpp}` first, as a small
   preparatory move), copies the result into the input image with
   `vkCmdCopyBufferToImage`, and blits the input image to the
   viewport-sized target image with a linear filter. `PresentFrame`
   then does the non-blocking acquire (zero timeout; `VK_NOT_READY`
   or `VK_TIMEOUT` skips the present and bumps a
   `DEBUG_PRESENT_PACING` counter), clears the swapchain image to
   black for the letterbox, blits the target into the viewport
   rectangle, submits, presents, and routes any `OUT_OF_DATE` result
   into the recreation machinery. The present path gains its
   `// TODO(osd)` compositing-point marker after the letterbox blit
   (see ["OSD readiness"](#osd-readiness-design-constraints-only--no-osd-is-implemented)). Verification: boots to the DOS prompt;
   every video-mode class renders correctly; live resize and
   fullscreen toggling work; the validation log stays clean.
7. `Wire vsync, captures, and the remaining backend interface` —
   `SetVsync` implements the [decision 8](#decision-record-all-locked-2026-07-0304) mapping (using the
   maintenance1 fast path where available, full swapchain recreation
   otherwise); `ReadPixelsPostShader` copies the target image to the
   staging buffer, waits on a fence, and returns a BGRX32
   `RenderedImage` with the same viewport semantics as the OpenGL
   backend; `MakePixel` and the viewport/render-size notification
   methods are filled in; the shader-related interface methods get
   no-op stubs mirroring `SdlRenderer`'s. This commit also audits the
   backend-type conditionals scattered through `sdl_gui.cpp` (such as
   the macOS window-restore hack) for Vulkan relevance. Verification:
   all capture types produce plausible output; pause-plus-resize
   repaints correctly; vsync toggles live.
8. `ci: add a lavapipe smoke test` — a Linux CI job running Mesa's
   lavapipe under `xvfb-run`: boot to the DOS prompt, present at
   least 100 frames, exit cleanly, and assert that the validation
   layer reported nothing. (Golden-image rendering tests arrive
   together with the shader pipeline in [PR 3](#pr-3--shader-toolchain--vulkan-pipeline).)

*Exit criteria: feature parity with the texture backend on Linux and
Windows Vulkan, with a learning-doc chapter per commit.*

### PR 3 — shader toolchain + Vulkan pipeline

This PR brings the shader system to the Vulkan backend, and on the
way performs the refactoring that decouples the shader subsystem from
OpenGL (the coupling documented in ["GL-coupling refactors"](#gl-coupling-refactors-verified-findings-folded-into-pr-3) above).

New files: `render/shaders/shader_compiler.{h,cpp}`
(`Shaders::Compiler`, the glslang wrapper),
`render/shaders/spirv_reflection.{h,cpp}` (`Shaders::SpirvReflection`,
the SPIRV-Cross reflection wrapper, used for debug-build validation
only), `render/shaders/uniform_packer.{h,cpp}`
(`Shaders::UniformPacker` — canonical std140 offset computation and
uniform-block packing, shared by all three executors),
`render/shaders/pipeline_topology.{h,cpp}`
(`Shaders::PipelineTopology` — the pass-graph logic extracted out of
the GL pipeline), `render/vulkan/private/vulkan_pipeline.{h,cpp}`
(`Vulkan::Pipeline`, the executor), plus eight shaders converted to
the 450 format under `resources/shaders/` (temporary duplicates of
their 330 originals, resolved in [PR 5](#pr-5--single-source-shader-library)). The `ShaderPipeline` and
`Shader` class renames (`→ OpenGlPipeline` / `OpenGlShader`, matching
their [PR 2](#pr-2--vulkan-backend-skeleton-output--vulkan-no-shaders) file renames) land with this PR's refactor, which rewrites
both classes anyway.

Commits:

1. `build: add glslang and spirv-cross dependencies` — both come from
   vcpkg for our builds and are linked when `C_VULKAN` or `C_METAL`
   is enabled. On Linux system-libs builds, glslang is a hard
   dependency (packaged by every major distro), while SPIRV-Cross is
   optional: it only backs the debug-build reflection validator
   there, so its absence merely disables that validator ([decision 5](#decision-record-all-locked-2026-07-0304)
   has the full packaging review). Verification: the compile matrix
   passes with every option combination.
2. `Add the runtime GLSL-to-SPIR-V shader compiler` — a thin wrapper
   over the glslang C++ API: one-time `InitializeProcess()`, the
   stock `GetDefaultResources()` limits, per-stage compilation using
   a `#define VERTEX 1` / `#define FRAGMENT 1` preamble (so our
   single-file shader format compiles unchanged), targeting Vulkan
   1.3 / SPIR-V 1.6, with compile errors formatted the same way the
   GL backend reports shader errors today. The entire flow was
   verified in advance by [Appendix D Spike 2](#spike-2--glslang--spirv-cross-library-apis-passed). The debug reflection
   validator that rides along matches uniform blocks by base type and
   member names — never by instance name, because anonymous blocks
   reflect with empty, unstable instance names (a [Spike 2](#spike-2--glslang--spirv-cross-library-apis-passed) finding).
   Unit tests: the spike's `sharp450.glsl` compiles; garbage input is
   rejected with a readable message.
3. `Split shader sources from compiled shader objects` — introduces
   `ShaderSource` (name, source text, parsed `ShaderInfo`) and
   reduces `ShaderManager` to caching sources and presets only (its
   `shader_cache` becomes an `unordered_map<string, ShaderSource>`).
   The GL `Shader` class — which owns an OpenGL program object —
   moves behind `OpenGlRenderer`/its pipeline, with its own compiled-
   program cache. This is a pure refactor with no behaviour change.
   Verification: the unit-test suite passes, and GL golden captures
   taken before and after the commit are identical.
4. `Extract the backend-agnostic pipeline topology` — introduces
   `BuildPipelineTopology(sources, input_size, doubling_flags,
   video_mode, viewport, dedither, adjustments)`, which returns the
   ordered list of `PassDesc` records (pass name, input references,
   resolved output rectangle, filter and wrap modes, float-output
   flag) plus the staleness rules. The GL pipeline is refactored to
   consume this instead of computing sizes itself. The pass-graph
   logic finally becomes unit-testable without any GPU context; the
   tests cover the video-mode × dedither × doubling matrix.
   Verification: GL golden captures remain identical.
5. `Add the Vulkan shader pipeline executor` — for each pass, the
   shaders come from the commit-2 compiler and become one
   `VkPipeline` using dynamic rendering and dynamic viewport/scissor
   state, with vertex input bound to a shared fullscreen-triangle
   vertex buffer carrying `a_position`. There is exactly ONE
   descriptor-set layout in the whole system: binding 0 is the
   uniform buffer, bindings 1..N are combined image samplers.
   Descriptor sets are allocated from a pool sized at pipeline
   rebuild and written once ([flesh-out decision 7](#decisions-made-during-flesh-out-john-review-these) — uniforms only
   change on rebuild or settings changes). Intermediate images are
   VMA-allocated with `COLOR_ATTACHMENT | SAMPLED` usage, 8-bit or
   float per the shader's `float_output` pragma; the sampler grid
   (two filter modes × four wrap modes) is created once. Each pass
   has a persistently-mapped uniform buffer packed by the shared
   UniformPacker, with SPIRV-Cross reflection cross-checking the
   offsets in debug builds. The final pass renders into the target
   image introduced in [PR 2](#pr-2--vulkan-backend-skeleton-output--vulkan-no-shaders), and the `output_stale` clean-present
   semantics are identical to the GL backend's. Verification: the
   validation layer stays silent, and the first lavapipe golden-image
   test — a two-pass chain rendered in CI — lands in this commit.
6. `Port the internal shaders and sharp to the 450 format` — the
   seven internal shaders plus `interpolation/sharp`, converted with
   the [Appendix D](#appendix-d--spike-results-2026-07-04) recipe (the 450 files live alongside their 330
   originals until [PR 5](#pr-5--single-source-shader-library) removes the duplication). `SetShader`,
   `ForceReloadCurrentShader`, the shader-info getters, image
   adjustments, and dedithering strength are all wired up on the
   Vulkan backend. The auto-shader-switcher needs no changes at all —
   it resolves shader *names*, and names are backend-independent.
   Verification: side-by-side vulkan-versus-opengl captures of
   `sharp` are visually identical; adjustments and dedithering
   demonstrably take effect; golden images are recorded for CI.

*Exit criteria: `output = vulkan` running `sharp` matches OpenGL, and
lavapipe golden-image tests are running in CI.*

### PR 4 — Metal backend

Because this PR lands after the shader toolchain exists, the Metal
backend arrives with full pipeline support from its first release —
there is no shader-free skeleton phase as there was for Vulkan
(Metal's blit encoder cannot scale, so a completely shader-free
present isn't even possible there; see [flesh-out decision 4](#decisions-made-during-flesh-out-john-review-these)).

New files: `render/metal/metal_renderer.{h,mm}` (`Metal::Renderer`)
and `render/metal/private/metal_pipeline.{h,mm}`
(`Metal::Pipeline`). Both are Objective-C++; whether ARC is enabled
should match the repo's existing convention — check
`macos_colorspace.mm` before starting.

Commits:

1. `build: add the Metal backend option` — adds `OPT_METAL`
   (macOS-only, default ON), defining `C_METAL`; links the Metal and
   QuartzCore frameworks; registers the `metal` config value with the
   fallback chain metal → opengl → texture; enables the option in the
   macOS CI workflow.
2. `Add the Metal device and layer bring-up` — creates the layer via
   `SDL_Metal_CreateView`, sets up the `MTLDevice` and command queue,
   configures the layer (`pixelFormat = BGRA8Unorm`,
   `displaySyncEnabled` driven by the vsync setting, and
   **`colorspace` driven by the `color_space` setting** — Display P3
   or sRGB via `CGColorSpaceCreateWithName`; this is the Metal
   equivalent of the GL path's NSWindow colour-space tagging and must
   not be left at its nil default, which would disable colour
   matching entirely), tracks drawable size from the viewport
   notifications, and treats a nil return from `nextDrawable` as a
   skipped present with its own counter — the Metal equivalent of the
   Vulkan acquire-skip. `SetColorSpace` is a real implementation on
   this backend, not a stub.
3. `Implement upload, pipeline executor, and present` — uploads go
   into an `MTLTexture` using shared storage on unified-memory
   hardware; the code queries for unified memory rather than assuming
   Apple Silicon, because Intel Macs with discrete GPUs need the
   managed-storage path instead (a PCSX2 lesson, [Appendix C §10](#10-pcsx2-and-rpcs3-added-2026-07-04-new-findings-only)). MSL
   comes from SPIRV-Cross's `CompilerMSL` (entry point `main0`, with
   the texture and buffer indices obtained from its resource-binding
   queries). Each pass becomes an `MTLRenderPipelineState`, keyed by
   pass and target formats; the sampler-state grid mirrors the Vulkan
   one; uniforms are small enough to go through
   `setVertexBytes`/`setFragmentBytes`, so there is no buffer
   management at all ([flesh-out decision 7](#decisions-made-during-flesh-out-john-review-these)). Intermediate textures
   and the viewport-sized target texture mirror the Vulkan
   structure. The letterbox present is a clear plus the one embedded
   blit shader from [decision 4](#decision-record-all-locked-2026-07-0304), followed by `presentDrawable`, with
   the `// TODO(osd)` compositing-point marker after the blit (see
   ["OSD readiness"](#osd-readiness-design-constraints-only--no-osd-is-implemented)); captures read the target texture with
   `waitUntilCompleted`, which is fine at screenshot cadence. Everything shader-side —
   `shader_compiler`, `pipeline_topology`, the UniformPacker, the
   reflection validator — is reused from [PR 3](#pr-3--shader-toolchain--vulkan-pipeline) with zero duplication.
   Verification: parity against the Vulkan backend on the M4 mini;
   golden captures for `sharp`, adjustments, and dedither; both
   storage-mode paths exercised (M-series plus one Intel Mac if one
   is available — flagged as untested otherwise).

*Exit criteria: `output = metal` reaches parity with the Vulkan
backend on macOS.*

### PR 5 — single-source shader library

This PR collapses the shader sources down to the single
hand-maintained 450 tree and switches the OpenGL backend onto the
generated files, ending the temporary duplication [PR 3](#pr-3--shader-toolchain--vulkan-pipeline) introduced.

Commits:

1. `Add the offline GL 3.30 shader generation script` —
   `scripts/tools/generate-gl-shaders.py`, implementing the
   spike-verified pipeline from [Appendix D](#appendix-d--spike-results-2026-07-04) exactly: two glslang
   invocations (one per stage), SPIRV-Cross with `--version 330
   --no-es --flatten-ubo` (plus `--flip-vert-y` if the golden tests
   say the GL path needs it), stripping the `layout(binding = N)`
   qualifiers from sampler declarations (they require a GL extension
   macOS doesn't have), and stitching the two stages back into our
   single-file format with the `#pragma` metadata block re-attached.
   Output is deterministic, a `--check` mode regenerates and diffs
   for CI, and the lint workflow gains the hook.
2. `Switch the OpenGL executor to flattened uniform blocks` — the GL
   pipeline now uploads each pass's uniforms with a single
   `glUniform4fv` call into the flattened `Uniforms[k]` array,
   sourcing the bytes from the same shared UniformPacker the native
   backends use. The old by-name `Shader::SetUniform*` paths are
   deleted. Sampler binding by name at link time stays as it is
   today. Verification: golden captures of the eight already-ported
   shaders, legacy path versus flattened path, must be
   bit-identical.
3. `Convert the bundled shader library to the 450 format` — the
   remaining 13 shaders are converted with the recipe (each gets a
   reserved-word audit — `sampler` bit us on the very first file).
   After this commit, `resources/shaders/` contains only 450 sources,
   the generated `resources/shaders/gl330/` mirror tree is checked
   in ([flesh-out decision 3](#decisions-made-during-flesh-out-john-review-these)), `ShaderManager` resolves shader names
   to the right tree per backend, and the [PR 3](#pr-3--shader-toolchain--vulkan-pipeline) temporary duplicates
   are deleted. Verification: golden captures per shader family on
   all three backends, and the `crt-auto-*` modes working on all
   three.
4. `docs: shader authoring and porting guide` — documents the 450
   format specification, the porting recipe, and the generation
   script's usage; explains the user-shader situation (the 450 format
   is required for the vulkan and metal backends, while existing
   user-written 330 shaders keep working on OpenGL untouched); and
   drafts the release-notes snippet.

*Exit criteria: one hand-maintained shader source tree, with the CI
`--check` guard making stale generated files impossible.*

### PR 6 — present timing (the flagship)

This PR delivers the project's headline feature: presents scheduled
by the display engine itself, at timestamps derived from the emulated
video timing, with measured feedback proving the result. The Metal
half was already proven working on real hardware before this plan was
finalised ([Appendix D Spike 4](#spike-4--metal-scheduled-presents-on-real-hardware-passed)); the Vulkan half is new territory —
no shipping emulator schedules Vulkan presents yet ([Appendix C §7](#7-present-pacing--the-headline-finding)) —
which is why the [PR 1](#pr-1--deferred-present-scheduler--pacing-instrumentation) scheduler remains wired in as the fallback tier
throughout.

Commits:

1. `Add scheduled-present support to the render backend interface` —
   the one interface change of the whole series (flesh-out decision
   5): `SupportsScheduledPresents()` (base implementation returns
   false) and `PresentFrameAt(due_us)`. `GFX_SchedulePresent` gains a
   fast path: when the active backend supports scheduled presents,
   the vretrace-driven present is submitted immediately, carrying its
   target timestamp — bypassing the [PR 1](#pr-1--deferred-present-scheduler--pacing-instrumentation) polling loop entirely —
   and when it doesn't, the scheduler is armed exactly as before.
   The dispatch logic is written so it can be unit-tested without a
   real backend.
2. `Implement VK_EXT_present_timing in the Vulkan backend` — the full
   API surface is pinned in [Appendix D Spike 3](#spike-3--vk_ext_present_timing-api-surface-pinned). Steps: probe
   `VkPhysicalDevicePresentTimingFeaturesEXT` (need `presentTiming` +
   `presentAtAbsoluteTime`) and the surface caps; create the
   swapchain with `VK_SWAPCHAIN_CREATE_PRESENT_TIMING_BIT_EXT`; size
   the feedback queue via
   `vkSetSwapchainPresentTimingQueueSizeEXT` (frames-in-flight +
   margin — unsized queues drop results); calibrate our monotonic
   clock to the swapchain time domain via
   `vkGetCalibratedTimestampsKHR` +
   `VkSwapchainCalibratedTimestampInfoEXT` (also enable
   `VK_KHR_calibrated_timestamps`; recalibrate periodically);
   schedule by chaining `VkPresentTimingsInfoEXT` into
   `VkPresentInfoKHR` with absolute `targetTime`, **scheduled ≥ 2–3
   refresh cycles ahead** ([Spike 4](#spike-4--metal-scheduled-presents-on-real-hardware-passed) lesson); poll
   `vkGetPastPresentationTimingEXT` into a FrameTimeData-style ring
   (PPSSPP pattern), reading the
   `IMAGE_FIRST_PIXEL_VISIBLE` stage as glass time and correlating
   via `VK_KHR_present_id2`; drift correction (slew targets on
   accumulated actual−target drift); missed-window + bogus-feedback
   detection with **per-vendor trust rules** ([Appendix C §10](#10-pcsx2-and-rpcs3-added-2026-07-04-new-findings-only):
   NVIDIA/ANV trustworthy, AMD/RADV/MoltenVK not — validate before
   acting); fallback to the [PR 1](#pr-1--deferred-present-scheduler--pacing-instrumentation) scheduler when absent or lying.
   Verify: RTX 3060 + VRR — logged first-pixel-visible intervals
   track 70.086 Hz; `vulkaninfo` confirms the extension.
3. `Implement scheduled presents in the Metal backend` — host-time →
   mach-absolute-time conversion, `presentDrawable:atTime:`
   (DuckStation-validated shape, designed not copied; **proven
   locally in [Appendix D Spike 4](#spike-4--metal-scheduled-presents-on-real-hardware-passed)**: exact average cadence on the
   M4 Pro, feedback machinery works), `addPresentedHandler` →
   lock-free single-writer feedback ring read from the main thread
   (handlers fire on Metal's internal queue); targets scheduled
   2–3 refresh cycles ahead ([Spike 4](#spike-4--metal-scheduled-presents-on-real-hardware-passed)'s constant-offset lesson);
   same drift/miss accounting. Verify: M4 mini on the VRR panel +
   MBP ProMotion — logged intervals track DOS cadence; check the
   windowed-vs-fullscreen offset difference ([Spike 4](#spike-4--metal-scheduled-presents-on-real-hardware-passed) measured
   windowed only).
4. `Evaluate Win32 exclusive fullscreen and acquire overlap` — two
   candidate improvements from the reference study get measured on
   the RTX 3060 and kept only if they demonstrably improve pacing:
   `VK_EXT_full_screen_exclusive` (guaranteed independent flip on
   Windows, behind an internal default rather than a user setting —
   the config-gated pattern both PCSX2 and RPCS3 use, [Appendix C](#appendix-c--reference-study-design-notes-2026-07-04)
   [§10](#10-pcsx2-and-rpcs3-added-2026-07-04-new-findings-only)), and DuckStation's acquire-the-next-image-right-after-present
   overlap. Whatever doesn't earn its keep is dropped, and the
   findings go into the learning doc either way.
5. `Add the pacing verification report` — the `DEBUG_PRESENT_PACING`
   instrumentation grows into a periodic summary report: presented-
   interval mean/p95/max against the target period, counts of
   scheduled versus app-timed versus immediate presents, accumulated
   drift, and missed windows. This commit also produces the
   manual-matrix run sheet for the release testing. Linux pacing is
   tested by core team members (hardware requirements are listed in
   the [Verification section](#verification)'s inventory).

*Exit criteria: logs on Windows/NVIDIA and on both Macs showing
measured flip intervals tracking the DOS cadence on VRR, and graceful
degradation verified by forcibly disabling the extension.*

### PR 7 — `auto` + polish

The closing PR: the `auto` output value, the deferred
presentation-mode decision, and the documentation sweep.

Commits:

1. `Add the auto output mode` — implements the resolution order from
   [decision 3](#decision-record-all-locked-2026-07-0304): Linux and Windows try `vulkan` first, macOS tries
   `metal`, and everything falls back through `opengl` to `texture`;
   the chosen backend is logged clearly. The *shipped default*
   config value stays unchanged in this release — flipping the
   default to `auto` is a separate release-management decision to be
   taken after the backends have real-world mileage.
2. `Decide presentation_mode auto on native backends` — runs the
   comparison that [decision 7](#decision-record-all-locked-2026-07-0304) deferred, using the VRR panel pinned to
   60 Hz as the fixed-rate testbed: dos-rate's drop-at-acquire
   pattern versus host-rate's sampling pattern, judged with both the
   pacing instrumentation and eyeballs on smooth-scrolling content.
   The winner gets implemented as the native backends' auto mapping.
   If the data supports it, the host-rate early-present window (a
   workaround for OpenGL's blocking swap) is also shrunk toward zero
   on the native backends.
3. `docs: native backends, VRR guide, and release notes` — the
   setting documentation for `vulkan`/`metal`/`auto`; a VRR guide
   covering the recommended configurations (including the
   driver-forced-vsync caveat from [decision 8](#decision-record-all-locked-2026-07-0304) and the NVIDIA
   control-panel DXGI-layering note for windowed VRR on Windows) and
   the compositor ceiling (first-class pacing is a fullscreen
   feature); a known-limitations note that Linux pacing verification
   is in the core team's hands; **the finalised multi-part
   architecture documentation under `render/docs/`** (general +
   shaders + per-backend parts, with rendered diagram images and
   their editable sources — per the standing documentation
   constraint), with `render/README.md` as its index; and the
   learning doc's closing chapter.

*Exit criteria: the series is complete. The only deliberately open
question left behind is when the shipped default flips to `auto`.*

## Deferred — possible future work

- **On-disk shader cache** (design preserved): content-addressed
  `shader-cache/` under the config dir; key = SHA-256(post-preamble
  source, stage, target format, tool versions); value = final
  per-driver artifact (SPIR-V / MSL); atomic temp+rename writes;
  age-based prune. Deferred: remaining compiles are single-digit ms.
  (RetroArch ships exactly this pattern — slang_process.cpp:773-808 —
  validating the design if ever needed.)
- HDR swapchain composition (scRGB/HDR10; CRT shaders above 1.0).
- Host-rate mode simplification/removal if auto→dos-rate-always proves
  out.
- dx12 backend — only if a concrete need appears; DXGI has no
  present-at-time, so pacing can never justify it ([Appendix A](#appendix-a--architecture-decision-memo-2026-07-03-verbatim)).
- SDL GPU — rejected; revisit only if SDL ever exposes present timing;
  the scheduler's due-time is exactly the value that would feed it.
- **Debugger UI off SDL GPU**: the debugger's ImGui currently runs on
  the SDL GPU API (`imgui_impl_sdlgpu3`, own window and device).
  Experiment with switching it to the plain SDL_Renderer ImGui
  backend — easily fast enough for a debugging interface — which
  would remove the last SDL GPU usage from the tree entirely.
- **Asynchronous rendered-capture readback**: `ReadPixelsPostShader`
  could avoid its fence stall by recording the copy, returning a
  pending handle, and delivering the image a frame later — but the
  capture layer's interface is synchronous, rendered screenshots are
  one-off user actions where a millisecond stall is imperceptible,
  and video capture never uses this path (it reads the source latch).
  Revisit only if per-frame rendered captures ever become a feature.

## Verification

- **Unit tests**: pipeline topology matrix (mode × dedither ×
  doubling), reflection/packing cross-check, scheduler due-time math.
- **CI**: lavapipe golden-image render tests (Vulkan); compile matrix
  with backend options ON/OFF; offline-script `--check`;
  `compile-commits.sh` green throughout.
- **Pacing proof** ([PR 6](#pr-6--present-timing-the-flagship)): present-timing feedback logs showing flip
  intervals vs DOS cadence on VRR; skip counters ≈ 0 on VRR;
  per-present block time ≈ 0.
- **Manual matrix per PR**: macOS/Metal; Linux/Vulkan (NVIDIA, AMD,
  Intel — core team members); Windows/Vulkan (NVIDIA RTX 3060
  + the three AMD iGPU boxes; Intel-on-Windows is untested at home —
  minor gap, `opengl`/`texture` fallbacks cover it); VRR and fixed
  displays; both presentation modes; vsync on/off; fullscreen +
  windowed (compositor-ceiling expectations documented; NVCP
  "Vulkan/OpenGL present method → DXGI swapchain" note for windowed
  VRR on NVIDIA/Windows); mode 13h double-scan, text, composite CGA,
  VESA; `sharp`/`crt-auto`/a scaler shader; dedither; adjustments;
  resize + fullscreen + shader switch + pause interactions; all
  capture types; FMV + deinterlacing; upside-down/mirrored check per
  backend.
- **Test hardware inventory** (John, 2026-07-04):
  - Windows: NVIDIA RTX 3060 on a 42–120 Hz VRR monitor — Vulkan +
    `VK_EXT_present_timing` (NVIDIA R595+) directly testable; 70.086
    Hz DOS rates sit comfortably inside the VRR window.
  - Windows/AMD, three GPU generations:
    - Windows 10, Ryzen 9 7900 iGPU (RDNA2-class)
    - Windows 11, Ryzen 7 5700U, Radeon 512SP (Vega 8 — also the
      weak-iGPU performance case the clean-present skip serves)
    - Windows 11, Ryzen 7 8845HS, Radeon 780M (RDNA3)
    AMD hasn't shipped `VK_EXT_present_timing` on Windows yet, so
    these primarily verify the **[PR 1](#pr-1--deferred-present-scheduler--pacing-instrumentation) app-timed fallback tier**, AMD
    driver/WSI behaviour, and Win10-vs-11 compositor differences;
    once AMD ships the extension they become the
    unreliable-feedback-detection bench (RPCS3 trust table flags AMD
    reporting).
  - macOS: M4 Mac mini (same VRR panel) + recent MacBook Pro
    (ProMotion 120 Hz VRR) — Metal + `present(atTime:)` testable on
    both external-VRR and built-in-VRR paths.
  - **Linux: covered by core team members** (no box at John's — 
    lavapipe CI covers functional correctness; real-display testing
    is the team's). Requirements to circulate to whoever volunteers:
    Mesa 26.1+ (RADV/ANV/NVK) or NVIDIA R595+ for
    `VK_EXT_present_timing`; a VRR (FreeSync/G-Sync-compatible)
    display for the pacing verification specifically; confirm with
    `vulkaninfo | grep -i present_timing`. Both X11 and Wayland
    sessions worth a pass (compositor behaviour differs).
  - To confirm per Windows box when convenient:
    `vulkaninfo | grep -i present_timing` (expected on NVIDIA R595+;
    expected absent on the AMD boxes for now).

## Risks

- **`VK_EXT_present_timing` is a first-generation extension.** The
  spec was merged in November 2025; Mesa 26.1 shipped support in May
  2026; NVIDIA supports it from the R595 drivers; AMD and Intel on
  Windows haven't shipped it yet. First-generation driver
  implementations will have bugs, and — as the reference study
  showed — no shipping emulator schedules Vulkan presents with it
  yet, so we will be first to hit whatever is broken. (DuckStation
  does ship the equivalent Metal mechanism, so only the Vulkan half
  is uncharted — [Appendix C §7](#7-present-pacing--the-headline-finding)/[§9](#9-duckstation-patterns-only--no-derivatives-licence).) Mitigations: the [PR 1](#pr-1--deferred-present-scheduler--pacing-instrumentation) app-timed
  scheduler is a full-quality fallback that is always available, and
  the extension's own feedback lets us detect when a driver is lying
  and degrade automatically, with per-vendor trust rules seeding the
  detection.
- **The WSI/sync debugging tail happens on hardware we don't run
  daily.** Swapchain and synchronisation bugs are notoriously
  vendor-flavoured and intermittent. Mitigations: the design follows
  patterns proven across six shipping emulators, validation layers
  (including synchronisation validation) run in all debug builds, and
  the manual test matrix spreads across three vendors and both fixed
  and VRR displays.
- **Generated-code drift**: the MSL and generated-330 shader variants
  could render slightly differently from the hand-written GLSL for
  the CRT shaders. Golden captures guard every cutover. The standard
  is "knowably equivalent" across backends (different GPUs round
  differently); bit-identical output is required only where it is
  achievable — the GL legacy path versus the GL generated path.
- **The compositor ceiling is real and unsolvable from our side**:
  windowed-mode pacing is best-effort on every OS, because desktop
  compositors resample windows at their own rate — [Spike 4](#spike-4--metal-scheduled-presents-on-real-hardware-passed) measured
  this directly on macOS. First-class pacing is a fullscreen
  feature. We document this honestly rather than pretending to solve
  it.
- **Objective-C++ enters the tree** with the Metal backend. The
  surface is small, and the repo already contains Obj-C++
  (`macos_colorspace.mm`), so this is a familiarity cost rather than
  a technical one.

---

# Appendix A — Architecture decision memo (2026-07-03, verbatim)

Preserved verbatim from the plan-review discussion.

## My opinion: switch to native Vulkan + Metal backends. Drop SDL GPU and dx12.

I started this plan on SDL GPU because it's the obvious, low-effort abstraction. Your pacing requirement killed it, and having done the research, I'm now convinced the native route is genuinely better for this project — not just forced. Here's the whole case.

### Why SDL GPU is out

I verified this rather than assumed it: SDL GPU performs the present inside `SDL_SubmitGPUCommandBuffer`, exposes no native swapchain or queue handles, has no timestamp parameter, and has no present-timing work in progress upstream. There is no hook. Getting `VK_EXT_present_timing` into SDL GPU would mean patching SDL or driving a months-long upstream API discussion on someone else's timeline. For a non-negotiable requirement, "wait and hope" isn't a plan. SDL GPU is designed for games that don't care *when* the flip happens; we are precisely the opposite case.

### What present timing actually buys us (and why native is the only way)

`VK_EXT_present_timing` — merged Nov 2025 after five years, feature-complete in [Mesa 26.1 across RADV/NVK/ANV](https://www.phoronix.com/news/Mesa-VK_EXT_present_timing), in NVIDIA's proprietary R595+ on Windows and Linux — and Metal's decade-mature `present(atTime:)` both give the same two-sided contract:

- **Scheduling**: attach a target time to each present; the *display engine* holds the flip until then. Microsecond-class, immune to main-loop load. The vretrace event just submits immediately with `target = tick_boundary + PIC_TickIndex() × 1 ms` — and this actually *deletes* complexity: no deferred-present polling on capable stacks, and the flip hits target even mid-heavy-emulation-burst.
- **Feedback**: timestamps of when frames actually hit glass. This half is underrated — it's what lets us drift-correct DOS's 70.086 Hz against the monitor's clock, detect missed windows, and *prove* our pacing in logs instead of eyeballing smooth scrollers. First-class pacing you can't measure is a vibe, not a feature.

The app-timed scheduler we agreed on stays — demoted to the permanent fallback tier (extension-less Vulkan drivers, OpenGL), and built first since it helps OpenGL immediately.

### The other things native buys — each real on its own

- **Dependency risk drops.** SDL_shadercross — preview software, the riskiest external in the old plan — becomes unnecessary. Native Vulkan eats glslang's SPIR-V directly; native Metal eats SPIRV-Cross's MSL directly; both were already committed for the offline 330 generation. We'd add vk-bootstrap and VMA (boring, stable, ubiquitous). Net ledger: one risky dep out, two dull ones in.
- **Debugging gets honest.** Validation layers (including sync validation) and RenderDoc operate directly on our calls, no abstraction layer between us and the error.
- **CI can finally render.** With native device selection we can force lavapipe (Mesa's software Vulkan) headless — real golden-image tests of the shader pipeline on every commit. Neither the OpenGL backend nor SDL GPU ever gave us that cleanly.
- **The learning goal.** You said you want to genuinely understand SPIR-V, Vulkan, the GPU stack. Owning the swapchain, sync, and pipelines *is* that curriculum; SDL GPU would have taught you SDL GPU's opinions instead.
- **Precedent.** RetroArch, Dolphin, DuckStation — the emulators whose pacing you're aiming at — all own native backends for exactly these reasons. Every subproblem we'll hit has battle-tested reference code in the open.

### What it costs — no sugar-coating

- **Roughly double the backend bring-up.** The Vulkan tax: swapchain lifecycle (`OUT_OF_DATE`/`SUBOPTIMAL`, resize races, minimize, surface loss), semaphore/fence correctness (sync bugs are intermittent and vendor-flavoured — the classic time sink), layout transitions, descriptors. Bounded by our tiny fixed pipeline — realistically ~3–4k lines of Vulkan with vk-bootstrap/VMA, ~1.5–2k of Obj-C++ Metal — but this WSI grind is exactly what SDL GPU had a year of community hardening on, and we're walking away from that.
- **Two native backends to maintain forever**, with driver-quirk triage landing on us.
- **Early-adopter risk.** The extension is *new*. First-generation driver implementations will have bugs; Windows AMD/Intel support hasn't shipped yet. Mitigated by the fallback ladder we need anyway, plus the extension's own feedback telling us when it's lying.
- **The compositor ceiling, on every OS.** Under a desktop compositor a window is treated like a fixed-refresh display — the Khronos blog says so outright. First-class pacing is a **fullscreen** story (direct scanout / independent flip) on Windows, Wayland, and macOS alike. Windowed stays best-effort. Every emulator lives with this; we document it and move on.

### Vulkan vs DX12 on Windows, for our workload

Performance is a non-question — a few fragment passes over a small texture; both APIs are absurdly overqualified. What actually differentiates:

- **Presentation path**: [DXGI flip model](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model) is Windows' native gold standard, but fullscreen/borderless-covering-screen Vulkan reaches the same independent-flip path with working G-Sync, and NVIDIA can layer Vulkan on a DXGI flip swapchain even windowed (control-panel knob). Parity where pacing matters; DX12 slightly ahead in windowed mode — where the compositor caps precision anyway.
- **Present timing**: Vulkan has the extension on Windows (NVIDIA now, others expected). **DX12 has no equivalent at all** — `GetFrameStatistics` feedback and latency-waitable objects, but the present call is still app-timed. On our #1 axis, Vulkan on Windows is *strictly ahead* of Microsoft's own API. That settles it by itself. (The Google AI answer you quoted claiming Present1 takes timing criteria is hallucinated, by the way — `DXGI_PRESENT_PARAMETERS` carries dirty rects, not timestamps. Its wrongness conveniently supports your conclusion.)
- **Driver coverage**: NVIDIA/AMD Vulkan on Windows: excellent. Intel Arc: DX12 and Vulkan co-equal. The weak cohort is old Gen9/Gen11 iGPUs where Intel's Vulkan was the neglected child — and that cohort is already served by our fallbacks. Note the elegant accident: `output = texture` on Windows rides **D3D11 via SDL_Renderer**, so we keep a Microsoft-native fallback without writing one line of D3D.

So dropping dx12 costs: marginally better windowed presentation and old-Intel robustness — both already covered. It saves: a whole backend, the entire DXC/FXC/DXBC packaging question (which dissolves unresolved, pleasingly), and a test-matrix column. Easy call.

### Ripple effects on the plan

- `output`: `texture`, `texturenb`, `opengl`, `vulkan`, `metal`, `auto` (Linux/Windows → `vulkan`, macOS → `metal`, fallback `opengl` → `texture`).
- All shader-side work is **unchanged** — 450 single-source, pragma system, topology extraction, offline 330 generation, GL-coupling cleanup. That was always the majority of the effort, and it transfers 1:1.
- **Shader cache**: with DXC gone, the load-bearing case went with it — glslang + SPIRV-Cross cost single-digit milliseconds per shader. I'd demote it from committed scope to a gated item (build only if mode-switch hitches actually show up in testing). Keeping it as-committed would be me overselling a feature whose justification I already deleted.
- PR sequence: ① app-timed scheduler + pacing instrumentation (helps OpenGL today, permanent fallback tier) → ② native Vulkan skeleton (no shaders; upload/blit/captures/present modes) → ③ shader toolchain + Vulkan pipeline executor + internal shaders → ④ Metal backend → ⑤ single-source library + offline script → ⑥ present-timing PR (`VK_EXT_present_timing` + `present(atTime:)` + drift/feedback instrumentation) → ⑦ `auto` + polish. The learning doc runs through all of it, per commit.
- Fallback ladder at runtime: present-timing extension → app-timed scheduler → (OpenGL/texture paths unchanged).

### Verdict

Native Vulkan + Metal, dx12 dropped, SDL GPU dropped, cache demoted to gated. The price is the WSI/sync grind and owning two backends; the payoff is the flagship feature actually existing, measurably, plus a cleaner dependency story, real CI rendering tests, and the deep-dive you wanted anyway. I'd sign it.

---

# Appendix B — Follow-up decisions: dependencies, shader workflow, cache deferral, reference study (2026-07-03)

Lightly edited from the review discussion into decision-record form;
all details preserved.

## vk-bootstrap and VMA, explained

Vulkan's design philosophy is that the driver does *nothing* implicitly — every application must hand-write two large chunks of pure ceremony that have nothing to do with what the app actually draws:

**The startup ceremony.** Before anything can render: create a `VkInstance` (with the right extensions and validation layers), enumerate the physical GPUs, score and pick one (does it have the required extensions? a queue family that can do graphics *and* present to the surface?), then create the logical device and fetch the queues. This is 500–1,000 lines of code that is *nearly identical in every Vulkan program ever written*, and it's easy to get subtly wrong (querying support in the wrong order, missing a fallback). **vk-bootstrap** is a small MIT-licensed library that does exactly this and nothing more — "give me a Vulkan 1.3 device that can present to this surface with these extensions" in ~20 lines, with the enumeration dance done correctly.

**Memory management.** Vulkan has no "just allocate me a texture". A resource is created, queried for its memory requirements, then allocated from the right *memory type* on the right *heap* (device-local for textures, host-visible for staging uploads...) — and calling `vkAllocateMemory` per resource is not viable, because drivers cap total allocations (commonly 4,096); the expectation is allocating big blocks and sub-allocating within them, respecting alignment rules. **VMA** (Vulkan Memory Allocator, from AMD's GPUOpen) is *the* industry-standard implementation of all that — practically every serious Vulkan application and engine ships it. "Create this image, GPU-only memory" — and it handles type selection and sub-allocation.

The "risky dep out, dull deps in" ledger: SDL_shadercross was **preview software** — API still moving, packaging immature, a real chance of chasing breakage across its releases. vk-bootstrap and VMA are the opposite species: years-stable, MIT, tiny, in vcpkg and every distro, with enormous install bases — "dull" in the best sense. They add code we don't have to write, without adding risk we have to carry. (Both *could* be hand-rolled — the memory needs are a dozen textures and a few buffers — but there's no virtue in rewriting the world's most-rewritten boilerplate.)

## Shader workflow — one correction to the initial understanding

Right for OpenGL; Metal needs **no checked-in variants and no offline step**:

- **Single hand-maintained source**: the Vulkan GLSL 4.50 files. The only thing ever edited by hand.
- **Vulkan**: consumes them at runtime (glslang → SPIR-V). Nothing checked in.
- **Metal**: consumes them at runtime too (glslang → SPIR-V → SPIRV-Cross → MSL). Nothing checked in. The runtime toolchain has to exist anyway for user custom shaders, so checked-in MSL would be redundant repo noise that can also drift with SPIRV-Cross versions.
- **OpenGL**: the *only* backend with an offline step, because it deliberately stays toolchain-free at runtime — the script regenerates the 330 files, committed alongside the 450 change; CI's `--check` mode guarantees they never go stale.

"A shader was touched" therefore means: edit the 450 source → run the script (regenerates GL 330) → commit both → verify on vulkan, metal, and opengl. Testing on all three per shader change is the honest cost of three backends; golden captures make most of it mechanical.

## Cache → "Possible future work — deferred"

Agreed, fully — and cleaner than a "gated item" (a gate implies someone keeps watching it). Milliseconds on first load is nothing; the cache leaves the executing plan, with the design sketch preserved in the deferred section so it doesn't have to be re-derived if reality ever disagrees.

## On the difficulty estimate — revised down after a fair correction

There are stable, shipping, open-source implementations of *precisely* this problem, and studying them before writing a line is cheap, high-value, and belongs in the plan as a formal step. What the study genuinely de-risks is the **design** — swapchain recreation strategy, sync architecture, frames-in-flight structure, WSI event handling, how to shape the per-API device abstraction — which is exactly where the "grind" mistakes described in the memo live. The nuance that stays: reference code doesn't eliminate the *debugging tail* on hardware and drivers not run daily (vendor-flavoured quirks, and first-generation `VK_EXT_present_timing` implementations). That's what the manual test matrix is for. Difficulty revised down, not to zero.

The study list, with licence hygiene noted (patterns are always fair game; *code* only from GPL-2.0-compatible sources):

- **Dolphin** (GPLv2+ — licence-compatible): `VideoBackends/Vulkan` (the `VKSwapChain` and command-buffer manager are clean and readable) and `VideoBackends/Metal` (modern, compact Obj-C++, written recently). Best relevance-to-readability ratio; the primary reference.
- **PPSSPP** (GPLv2+): `VulkanContext` and their frame-pacing work — Henrik Rydgård has thought harder about emulator frame timing than almost anyone.
- **RetroArch** (GPLv3 — read for patterns, don't copy): the most battle-tested Vulkan WSI across weird platforms, *and* the slang shader pipeline is the same shader genre end-to-end.
- **Khronos Vulkan-Samples** (Apache-2.0 — copy-friendly): official swapchain-recreation and present-mode samples, plus whatever present-timing sample material exists by now.
- **DuckStation**: architecturally the closest shape (compact per-API device classes behind one interface), but relicensed to a no-derivatives licence — look-don't-touch, or read only the old GPL-era tree.

Workflow: John checks the repos out locally; Claude inspects them; findings become both a design-notes section in the plan and an early chapter of the learning doc ("how the pros structure a native video backend"), fitting the per-commit education goal.

## Confirmed outcomes

- **Both Vulkan and Metal are committed scope, Vulkan first; SDL GPU
  out.** (John: "full sold".)
- Shader cache moved to deferred future work.
- Reference-implementation study added as a formal plan step (done —
  [Appendix C](#appendix-c--reference-study-design-notes-2026-07-04)).
- Learning doc named `gpu-backend-learning.md`; repo plan doc named
  `gpu-backend-plan.md`.

---

# Appendix C — Reference study: design notes (2026-07-04)

Sources studied locally: Dolphin (GPLv2+, primary), PPSSPP (GPLv2+),
RetroArch (GPLv3 — patterns only), Khronos Vulkan-Samples
(Apache-2.0), DuckStation (CC-BY-NC-ND — **patterns only, no code
copying**). File:line references are into those checkouts.

## 1. Sizing and decomposition

- Dolphin Vulkan backend ≈ 10k LOC: VKTexture 1382, VulkanContext
  1274, StateTracker 907, ObjectCache 812, VKSwapChain 732, VKGfx 730,
  CommandBufferManager 713. Metal backend ≈ 4.6k LOC (MTLStateTracker
  1280, MTLUtil 681, MTLObjectCache 678, MTLGfx 620, MTLTexture 333).
  Both carry full game-rendering state we don't need; our ~3–4k Vulkan
  / ~1.5–2k Metal estimates stand.
- Adopt Dolphin's file shape, minus the game-state parts:
  context/device (vk-bootstrap shrinks this), swapchain, per-frame
  command/sync manager, pipeline executor, upload, readback.

## 2. Swapchain lifecycle (the canonical state machine)

- **Creation**: image count = `minImageCount + 1` clamped to caps
  (Dolphin VKSwapChain.cpp:291; PPSSPP VulkanContext.cpp:1424); avoid
  sRGB formats, prefer plain UNORM (Dolphin); pass `oldSwapchain` and
  let the driver manage the handoff (PPSSPP VulkanContext.cpp:
  1509-1525).
- **Recreation triggers**: `VK_ERROR_OUT_OF_DATE_KHR` at acquire OR
  present; `VK_SUBOPTIMAL_KHR` treated as success-but-recreate-lazily
  (RetroArch converts it to success at present,
  vulkan_common.c:2908-2911; Android prerotate quirk noted at :291);
  window resize; vsync/present-mode change (see [§4](#4-runtime-vsync-switching)).
- **Guards**: skip everything while extent is 0×0 (minimised —
  RetroArch vulkan_common.c:1962-1965); redundancy gate — don't
  recreate if dimensions and swap interval are unchanged (:1982-1987).
- **Teardown discipline**: wait for in-flight work before destroying
  (Dolphin runs `ExecuteCommandBuffer(false, true)` before every
  recreation); RetroArch calls `vkDeviceWaitIdle` *outside* its queue
  lock to dodge a Windows TDR (vulkan_common.c:1836 comment).
  Vulkan-Samples shows the deferred-destruction alternative (old
  swapchain garbage kept per frame until its submit fence signals,
  swapchain_recreation.h:46-56) — cleaner; adopt.
- RetroArch has a WSI fuzzing mode injecting spurious OUT_OF_DATE /
  SURFACE_LOST (`WSI_HARDENING_TEST`, vulkan_common.c:76-100) —
  a testing idea worth stealing for our recreation paths.

## 3. Frames in flight and per-frame sync

- Dolphin: `NUM_FRAMES_IN_FLIGHT = 2`; per-frame bundle = 1 fence,
  acquire semaphore, render-done semaphore, command buffer(s),
  deferred-cleanup list. The dance: acquire(signal sem-A) → record →
  submit(wait sem-A, signal sem-B, fence) → present(wait sem-B).
- PPSSPP: `MAX_INFLIGHT_FRAMES = 3` (throughput-oriented;
  VulkanContext.h:394) — we want latency, so 2, with 1 as a VRR
  experiment (decision already in plan).
- Vulkan-Samples: 2-frame history ring; **semaphore/fence pools**
  recycled rather than re-created (swapchain_recreation.cpp:360-414);
  with `VK_EXT_swapchain_maintenance1`, each present op gets a fence
  so acquire semaphores can be recycled safely (:583-591). Adopt the
  pool + present-fence pattern when maintenance1 is present.
- Everyone blocks in acquire by default (UINT64_MAX). Our non-blocking
  skip uses `timeout = 0` → `VK_NOT_READY`/`VK_TIMEOUT` = skip. When
  we DO wait (captures' force_present), use a finite timeout —
  RetroArch caps waits at 500 ms to avoid wedging on stuck drivers
  (vulkan_common.c:150). Adopt the finite-timeout hygiene.

## 4. Runtime vsync switching

- Dolphin: full swapchain recreation on vsync toggle (present mode is
  immutable per swapchain).
- Vulkan-Samples: with `VK_EXT_surface_maintenance1` +
  `VK_EXT_swapchain_maintenance1`, query compatible present modes and
  switch FIFO↔IMMEDIATE/MAILBOX **without recreation**
  (swapchain_recreation.cpp:62-91, 237-238). Adopt: maintenance1 path
  when available, Dolphin-style recreation as fallback.

## 5. Upload path

- Dolphin streams through a 32 MB fence-tracked ring; that machinery
  exists for unbounded, variably-sized per-frame transfer volumes
  (game texture caches, vertex/index/uniform streams). Our plan: one
  fixed staging slot per in-flight frame (sized to max source dims ×
  4 B), `vkCmdCopyBufferToImage` + the two layout transitions,
  protected by the per-frame fence — the degenerate fixed-profile
  case of their ring, without the allocator. Metal: no staging at all
  (`MTLStorageModeShared`).

## 6. Shader pipeline executor (RetroArch slang — same genre as ours)

- Compilation at preset load only, never per frame; glslang behind a
  **global mutex** with one-time `InitializeProcess()`
  (glslang.cpp:48-62) — glslang is not thread-safe; note for us
  (single-threaded: fine, but keep the init-once).
- glslang invocation: full source string per stage, Vulkan+SPIR-V
  rules, includes forbidden at the glslang level (preprocessed
  before); resource-limits struct is boilerplate to copy once.
- **Uniform offsets come from SPIRV-Cross reflection**, not
  hand-computed std140: reflect UBO/push-constant members, read
  `ubo_offset` per member, round sizes to 16 B
  (slang_process.cpp:250-333). ADOPTED for our executor (canonical
  member order stays as convention; reflection is truth).
- Output-size computation per pass mirrors our `output_size` pragma
  semantics (source/viewport/absolute scale types,
  shader_vulkan.cpp:2384-2434) — direct analog of our
  `pipeline_topology` extraction; good cross-check.
- Pre-baked sampler grid (filters × wraps = 20 samplers,
  shader_vulkan.cpp:549) — same shape as our GL sampler array (2×4).
- Framebuffers/textures per pass recreated on size change with
  deferred disposal (Framebuffer class, shader_vulkan.cpp:490-534).
- They also ship a SHA-256-keyed SPIR-V disk cache
  (slang_process.cpp:773-808) — validates our deferred cache design;
  still deferred.

## 7. Present pacing — the headline finding

- **Nobody schedules presents on Vulkan yet — but DuckStation does on
  Metal.** RetroArch: no display-timing usage at all (pacing =
  present-mode choice + blocking acquire). Dolphin: none (FIFO
  blocking does the pacing). Vulkan-Samples: no present-timing sample
  as of this checkout. DuckStation: no Vulkan timing extensions, but
  its Metal device ships **`presentDrawable:atTime:`** with host-time
  → mach-absolute-time conversion (metal_device.mm:2584-2594) — a
  shipping-emulator precedent for exactly our [PR 6](#pr-6--present-timing-the-flagship) Metal design. Our
  `VK_EXT_present_timing` scheduling remains the novel half.
- PPSSPP is the only one touching the timing extensions, and only for
  **feedback/measurement**: `VK_GOOGLE_display_timing` →
  `vkGetPastPresentationTimingGOOGLE` polled after present
  (VulkanRenderManager.cpp:671-695; FrameTimeData history in
  MiscTypes.h:25-39), and `VK_KHR_present_wait` on a **dedicated
  thread** (:646-669) purely to timestamp when frames hit glass.
  They do NOT compute desired present times.
- Implications: (a) our timestamped-present usage is genuinely novel —
  the [PR 1](#pr-1--deferred-present-scheduler--pacing-instrumentation) scheduler fallback and the feedback-driven verification are
  load-bearing, not optional; (b) PPSSPP's polled-feedback pattern
  (not the thread) is the shape that fits our single-threaded loop —
  `VK_EXT_present_timing`'s feedback is poll-based and needs no
  thread; (c) their FrameTimeData ring is the model for our pacing
  logs.

## 8. Metal notes (Dolphin)

- CAMetalLayer: `setDevice`, `setPixelFormat(BGRA8Unorm)`,
  `setDisplaySyncEnabled` (vsync toggle — no recreation needed!).
- `nextDrawable` is non-blocking and returns nil when none available —
  the exact analog of our Vulkan skip path; handle nil = skip present.
- No descriptor sets, no render-pass objects, no layout transitions,
  unified memory for uploads — the Metal backend is the easy half;
  plan accordingly (Vulkan first hardens the shared structure).

## 9. DuckStation (patterns only — no-derivatives licence)

Architecturally the closest relative: five per-API device classes
behind one `GPUDevice` interface (gpu_device.h, 1048 lines). Sizes:
Vulkan 3679, Metal 2769 (.mm) + 467 (.h), D3D12 2734, OpenGL 1290,
D3D11 1225 LOC — full generic devices; our fixed-pipeline backends
land well under these. Design lessons (to design, not copy):

- **Metal timed presents ship today**: `EndPresent` takes an optional
  `present_time`; when in the future it converts host time to mach
  absolute time and calls `presentDrawable:atTime:` — otherwise plain
  `presentDrawable`. `nextDrawable == nil` → `SkipPresent` (their skip
  path). This validates [PR 6](#pr-6--present-timing-the-flagship)'s Metal half end-to-end.
- **Shader strategy convergence**: despite text-generating shader
  source per API for core rendering, their portable path is
  `CompileGLSLShaderToVulkanSpv()` (glslang) →
  `TranslateVulkanSpvToLanguage()` (SPIRV-Cross) — the same
  glslang + SPIRV-Cross runtime pipeline we chose. Independent
  confirmation of the toolchain.
- **Acquire-semaphore pool sizing**: image-acquire semaphores must
  outnumber command buffers by one (`NUM_IMAGE_ACQUIRE_SEMAPHORES =
  command buffers + 1`, rotated) — a semaphore can't be reused until
  its acquire actually completes. Subtle correctness detail to bake
  into our per-frame ring.
- **Immediate acquire-after-present** (DXVK-inspired): they acquire
  the next image right after `vkQueuePresentKHR` to overlap CPU work
  with present latency. For us, optional — evaluate against the
  timed-present model in [PR 6](#pr-6--present-timing-the-flagship) (pre-acquiring is compatible with FIFO +
  timestamped presents).
- **maintenance1 usage in anger**: `VK_KHR_surface_maintenance1` to
  query image counts per present mode; swapchain maintenance1's
  `ReleaseSwapchainImages`; vsync-mode switch recreates only when the
  mode actually differs. Matches and extends our [§4](#4-runtime-vsync-switching) plan.
- **`VK_EXT_full_screen_exclusive` (Win32)**: they use it for
  guaranteed exclusive-fullscreen control — directly relevant to our
  fullscreen pacing story on Windows; add to the [PR 6](#pr-6--present-timing-the-flagship) evaluation list.
- Present-mode mapping mirrors ours: Disabled → IMMEDIATE, fallback
  MAILBOX, then FIFO; Mailbox → MAILBOX, fallback FIFO.

## 10. PCSX2 and RPCS3 (added 2026-07-04; new findings only)

Licences: **PCSX2 = GPLv3** (patterns only); **RPCS3 = GPLv2**
(compatible with GPL-2.0-or-later; patterns still preferred to keep
our licence flexibility).

- **Vendor-reliability table for present feedback** (RPCS3,
  vkutils/swapchain.cpp:105-134): they flag which drivers report
  present timing truthfully — NVIDIA and Intel-ANV reliable;
  AMD/RADV/MoltenVK not. Direct [PR 6](#pr-6--present-timing-the-flagship) input: our feedback-driven drift
  correction must sanity-check feedback (reject absurd timestamps,
  degrade to open-loop targets per vendor) rather than trusting
  `actualPresentTime` blindly.
- **NVIDIA minimise quirk** (RPCS3, VKPresent.cpp:43-49): on
  minimise, the NVIDIA driver spams `VK_ERROR_OUT_OF_DATE_KHR` and
  recreating the swapchain in that state can crash — reinforce our
  rule: while extent is 0×0, skip presents and do NOT recreate;
  recreate on restore.
- **Adaptive acquire timeout** (RPCS3, VKPresent.cpp:584-605):
  infinite wait when double-buffered, 100 ms + drain-queued-frames +
  immediate retry when triple-buffered — a worked example of "never
  block unboundedly on acquire".
- **Exclusive fullscreen, twice more** (PCSX2 VKSwapChain.cpp:403-430;
  RPCS3 swapchain.cpp:310-324): both gate
  `VK_EXT_full_screen_exclusive` behind config and chain the Win32
  HMONITOR info struct — the pattern shape for our [PR 6](#pr-6--present-timing-the-flagship) evaluation.
- **PCSX2 Metal** (GSDeviceMTL.mm, 2691 lines): no `atTime:` presents
  — DuckStation remains the sole scheduled-present precedent. Useful
  bits: a `UsePresentDrawable` strategy switch (present inside the
  command buffer vs `addScheduledHandler` + `[drawable present]`),
  and a macOS-version quirk (windowed MAILBOX remapped to FIFO before
  Ventura) — macOS version belongs in our test matrix. Storage-mode
  handling differs on Intel-Mac discrete GPUs (shared vs managed) —
  our Metal upload path must query unified memory rather than assume
  Apple Silicon.
- **Image counts**: both prefer triple buffering for MAILBOX
  (`max(minImageCount + 2, 3)`), min+1 for FIFO — matches our [§2](#2-swapchain-lifecycle-the-canonical-state-machine)/[§3](#3-frames-in-flight-and-per-frame-sync)
  numbers.
- Neither touches `VK_EXT_present_timing`/`VK_GOOGLE_display_timing` —
  the [§7](#7-present-pacing--the-headline-finding) conclusion (Vulkan scheduling is novel territory) holds
  across all six studied projects.

---

# Appendix D — Spike results (2026-07-04)

Four spikes, all run locally. [Spike 1](#spike-1--cli-toolchain-round-trip) (CLI toolchain round-trip) below;
[Spike 2](#spike-2--glslang--spirv-cross-library-apis-passed) (library APIs), [Spike 3](#spike-3--vk_ext_present_timing-api-surface-pinned) (`VK_EXT_present_timing` API pinning)
and [Spike 4](#spike-4--metal-scheduled-presents-on-real-hardware-passed) (Metal scheduled presents on real hardware) follow it.

## Spike 1 — CLI toolchain round-trip

Hand-converted `interpolation/sharp.glsl` (330 → 450) and ran the full
round trip. Tools: glslang 16.3.0, SPIRV-Cross 2026-04-30 (Homebrew).

**What works, verbatim commands:**

```
glslang -V -DVERTEX=1   -S vert -o sharp.vert.spv sharp450.glsl
glslang -V -DFRAGMENT=1 -S frag -o sharp.frag.spv sharp450.glsl
spirv-cross --version 330 --no-es --flatten-ubo               sharp.frag.spv
spirv-cross --version 330 --no-es --flatten-ubo --flip-vert-y sharp.vert.spv
spirv-cross --msl sharp.frag.spv
```

- Single-file `VERTEX`/`FRAGMENT` sections compile per stage with
  `-S <stage> -D<SECTION>=1`. The `#pragma` comment block passes
  through untouched (it lives in a comment).
- The 330 output is semantically identical to the hand-written
  original: function names, varying names, sampler names all
  preserved; float constants print in exact form (`2.2` →
  `2.2000000476837158203125`) — cosmetic only.

**Findings that shaped the format spec:**

1. **`sampler` is reserved in 450** — `sharp.glsl`'s
   `texture_linear(in sampler2D sampler, ...)` fails to compile;
   renamed to `tex`. Porting recipe rule #1.
2. **Default UBO emission is macOS-hostile**: plain `--version 330`
   emits a real UBO with `layout(binding = 0)`, which requires
   `GL_ARB_shading_language_420pack` — universally available on
   Linux/Windows GL, **absent on macOS GL**. Rejected.
3. **`--flatten-ubo` is the right GL emission mode**: the UBO becomes
   `uniform vec4 Uniforms[k];` — plain 330, no extensions, name is
   stable (derived from the block *type*, not the SPIR-V-ID instance
   names like `_22`/`_43`, which differ per stage and per edit), and
   the GL executor sets it with a single `glUniform4fv` from the same
   packed std140 buffer the native backends upload. Caveat inherited:
   **UBO members must be all-float** (no int mixing) — fine, ours are
   vec2s and floats; made a format rule.
4. **Sampler binding qualifiers must be stripped for GL**: the
   generated `layout(binding = 1) uniform sampler2D INPUT_TEXTURE_0;`
   is also 420pack-only. The generation script sed-strips
   `layout(binding = N)` from sampler declarations; samplers bind by
   their preserved names at link time, as today.
5. **`--flip-vert-y` works** (appends `gl_Position.y = -gl_Position.y`
   to the vertex main). Whether the GL path needs it is a [PR 5](#pr-5--single-source-shader-library)
   golden-test question, not a blocker.
6. **MSL output is clean and executor-friendly**: combined sampler
   splits into `texture2d<float> tex` + `sampler texSmplr` arguments,
   UBO becomes a `constant Uniforms&` buffer — standard SPIRV-Cross
   MSL binding model, exactly what the Metal executor will consume.

**Porting recipe (330 → 450), validated on sharp.glsl:**

1. `#version 330 core` → `#version 450`.
2. Move all loose uniforms into the one anonymous std140 block
   (canonical order), declared once above the stage sections;
   samplers get `set = 0, binding = 1+`.
3. Add `layout(location = N)` to every varying (matched across
   stages).
4. Rename any identifier called `sampler` (and other 450 keywords:
   `texture` as identifier would clash similarly — audit per shader).
5. Keep the `#pragma` comment block byte-identical.
6. Bodies otherwise untouched.

## Spike 2 — glslang + SPIRV-Cross library APIs (PASSED)

[PR 3](#pr-3--shader-toolchain--vulkan-pipeline) uses the libraries, not the CLI tools; verified with a small C++
program (scratchpad `lib_spike.cpp`) against the Homebrew builds,
on a fragment shader extended with parameter-style members
(`vec2, vec2, float, float, vec2`):

- glslang C++ API works as specced: `TShader` + `setPreamble("#define
  FRAGMENT 1\n")` (the single-file section trick transfers),
  `setEnvClient(Vulkan_1_3)` / `setEnvTarget(Spv_1_6)`,
  `GetDefaultResources()` from `glslang/Public/ResourceLimits.h`
  exists (no hand-copied limits struct needed). Link line:
  `-lglslang -lglslang-default-resource-limits`.
- SPIRV-Cross reflection (`-lspirv-cross-core`) reports member
  offsets **exactly matching the UniformPacker's canonical std140
  math**: 0, 8, 16, 20, 24; struct size 32; sampler binding
  preserved. The packer-as-truth / reflection-as-checker design is
  confirmed with real bytes.
- Gotcha for the debug validator: the **anonymous UBO reflects with
  an empty instance name** — validate by base type and member names,
  never by instance name (which is also SPIR-V-ID-unstable, per
  [Spike 1](#spike-1--cli-toolchain-round-trip)).

## Spike 3 — VK_EXT_present_timing API surface (pinned)

From the released Vulkan-Docs proposal — [PR 6](#pr-6--present-timing-the-flagship) commit 2's "consult the
spec" is now resolved. The shape (names verbatim):

- **Opt-ins**: feature struct `VkPhysicalDevicePresentTimingFeaturesEXT`
  with separate bits `presentTiming`, `presentAtAbsoluteTime`,
  `presentAtRelativeTime` (we need the first two); surface caps via
  `VkPresentTimingSurfaceCapabilitiesEXT`; the swapchain must be
  created with `VK_SWAPCHAIN_CREATE_PRESENT_TIMING_BIT_EXT`.
- **Feedback queue must be sized explicitly**:
  `vkSetSwapchainPresentTimingQueueSizeEXT(device, swapchain, n)` —
  without it, results are dropped. Size ≈ frames-in-flight + margin.
- **Scheduling**: chain `VkPresentTimingsInfoEXT` (one
  `VkPresentTimingInfoEXT` per swapchain: `targetTime` ns,
  `timeDomainId`, flags for absolute-vs-relative and
  nearest-refresh-cycle alignment) into `VkPresentInfoKHR.pNext`.
- **Feedback**: `vkGetPastPresentationTimingEXT` →
  `VkPastPresentationTimingEXT` per present: `presentId` (correlate
  via `VK_KHR_present_id2`), the original `targetTime`, and
  **per-stage timestamps** — stages include
  `VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT`, the literal
  photons time our pacing logs want.
- **Clock mapping**: swapchain-local time domains
  (`vkGetSwapchainTimeDomainPropertiesEXT`) calibrated against host
  clocks via `vkGetCalibratedTimestampsKHR` +
  `VkSwapchainCalibratedTimestampInfoEXT` (so the backend also wants
  `VK_KHR_calibrated_timestamps`). Recalibrate periodically.
- Refresh-cycle duration comes from
  `vkGetSwapchainTimingPropertiesEXT` (`refreshDuration`) — feeds the
  fixed-display quantisation expectations below.

## Spike 4 — Metal scheduled presents on real hardware (PASSED)

Standalone Cocoa/Metal program (scratchpad `metal_spike.mm`): 300
clear-colour frames presented via `presentDrawable:atTime:` at a
70.086 Hz cadence, submitted ~3 ms before each target, actual glass
times collected via `addPresentedHandler`. Hardware: **Apple M4 Pro,
120 Hz ProMotion display, windowed**, macOS 15.x-era. Results:

- **300/300 GPU-completed, 300/300 presented-handlers fired** — the
  scheduling + feedback machinery works end to end.
- **Average cadence is exact**: mean presented interval 14.254 ms vs
  14.268 ms requested. On the (windowed, fixed) 120 Hz grid the
  display engine honoured the timestamps by alternating vblank slots
  in a 2:1 pattern — interval histogram: 203 × ~16.7 ms + 90 ×
  ~8.3 ms. This is the mathematically best realisation of 70.086 Hz
  on a 120 Hz grid.
- **Windowed mode quantises to the refresh grid even on ProMotion
  hardware** — direct empirical confirmation of the compositor
  ceiling on our own kit. Continuous-cadence VRR needs fullscreen;
  the plan's fullscreen-first pacing stance is now measured, not
  argued.
- **Constant ~20 ms lateness vs target** (mean 20.6, p95 24.8):
  submitting only 3 ms ahead can't beat the windowed compositor's
  ~2-frame pipeline. Lesson for [PR 6](#pr-6--present-timing-the-flagship): **schedule targets ≥ 2–3
  refresh cycles ahead** (or accept the constant offset — constant
  latency is harmless for pacing; jitter is the enemy). Expect the
  offset to shrink under fullscreen/direct scanout — verify then.
- Implementation gotchas recorded for [PR 4](#pr-4--metal-backend)/6 (details in the learning
  doc): unbundled binaries must call `[NSApp finishLaunching]` before
  windows will truly present; host the CAMetalLayer as a sublayer of
  a layer-backed view (naive contentView layer replacement gave
  nil-less drawables that never reached glass, plus an AppKit
  teardown crash); presented-handlers fire on Metal's internal queue
  (thread-safe collection required).
