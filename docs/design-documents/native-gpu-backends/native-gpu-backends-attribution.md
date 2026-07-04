# GPU backend — attributions and thanks

The native Vulkan render backend (see `native-gpu-backends-plan.md`)
was designed after studying the rendering code of eight open-source
emulators and the official Khronos samples. Every non-trivial design
decision in that plan stands on lessons these projects' developers
learned the hard way, over many years, on hardware and drivers we will
never see. This file records our debt precisely — which ideas,
patterns, and (where licences permit) code we lifted, and from where —
because credit is fair, and because future maintainers deserve the
provenance trail.

**To the developers of Dolphin, PPSSPP, RetroArch, DuckStation, PCSX2,
RPCS3, Cemu, Xenia, and the Khronos Vulkan-Samples: thank you.**

## How this file is maintained

One entry per adopted idea, pattern, or piece of code. Entries are
created as "planned" during design (from the reference study in the
plan's Appendix C) and flipped to "landed" with a commit hash when the
adoption reaches the tree; the update happens in the same commit that
adds the code-level credit (a comment for ideas, additionally an
`SPDX-FileCopyrightText` line for adapted code or data — see the
plan's Attribution policy).

**The credit bar.** We credit *genuinely innovative ideas and
hard-won quirk knowledge* — things a project invented, or paid for in
debugging hours against real drivers. We do **not** credit textbook
techniques that any competent developer learns from introductory
Vulkan/Metal materials, the official Khronos samples, or vendor
documentation: frames-in-flight sync structure, standard swapchain
recreation, storage-mode selection, and the like earn no entry here,
no matter where we happened to read them first. Over-crediting
dilutes the real credits and falsely implies stronger derivation than
exists. (Licence obligations for actually-copied code apply
regardless of this bar.)

Licence classes:

- **adaptable** — GPL-2.0-compatible (Dolphin GPLv2+, PPSSPP GPLv2+,
  RPCS3 GPLv2, Xenia BSD-3-Clause, Khronos Vulkan-Samples
  Apache-2.0): ideas *and*
  code/data may be adapted, with SPDX credit for the latter. Cemu
  (MPL-2.0, verified secondary-licence compatible) is adaptable
  too, with the extra rule that adapted code keeps its MPL
  file-level notice — prefer ideas over code from Cemu.
- **ideas only** — licence-incompatible (RetroArch GPLv3, PCSX2
  GPLv3, DuckStation CC-BY-NC-ND): concepts credited and re-expressed
  in our own words in the plan; copying code is prohibited
  (implementation works from the plan, never from these codebases —
  the plan's licence firewall).

## Dolphin (GPLv2+ — adaptable)

- **MoltenVK vendored-build integration**
  (`Externals/MoltenVK/CMakeLists.txt` + `configure.sh`/`patch.sh` —
  the pinned-tag `ExternalProject` with version-gated
  `fetchDependencies`, a carried-patches directory, and `make macos`)
  — the proven shape for building a pinned MoltenVK from source
  inside a CMake build, adapted for our macOS Vulkan delivery
  (plan decision 16). Adapted code; SPDX credit. *Planned (PR 2).*

Beyond that single item, what Dolphin gave us was something
different: the clearest,
most readable canonical implementation of the standard patterns,
which made it our primary reference reading during the design study.
That service deserves this mention even though it produces no
further itemised credits.

## PPSSPP (GPLv2+ — adaptable)

- **Per-frame present-timing history ring** (`FrameTimeData`,
  `Common/GPU/Vulkan/VulkanRenderManager.cpp`) — the model for our
  pacing logs and feedback records; an artefact of PPSSPP's
  frame-pacing culture, not of any tutorial. Idea/pattern.
  *Planned (PR 6).*
- **Present-timing feedback prior art** — PPSSPP is the only emulator
  we found using the Vulkan timing extensions at all (for
  measurement); their usage shaped our feedback design. Idea.
  *Planned (PR 6).*

## RetroArch (GPLv3 — ideas only)

- **WSI fault-injection testing** (`WSI_HARDENING_TEST` — injecting
  fake `OUT_OF_DATE`/`SURFACE_LOST` to exercise recovery paths) — a
  genuinely inventive testing idea we adopt for our swapchain
  recreation machinery. *Planned (PR 2 tests).*
- **Finite acquire timeouts as driver-wedge protection** (500 ms cap
  where a wait is unavoidable) — hard-won practice; the tutorials all
  teach `UINT64_MAX`. Idea. *Planned (PR 2).*
- **Vulkan GLSL as the single cross-compiled shader source format** —
  the slang-shader ecosystem is RetroArch's invention and the
  precedent that anchored our shader-format decision. Idea.
  *Landed in the plan itself.*
- **Multi-pass shader pipeline architecture prior art**
  (`gfx/drivers_shader/` — per-pass output-size semantics, reflection
  -driven uniform layout, pre-baked sampler grids): the closest
  existing relative of our shader executor, used as a design
  cross-check throughout. Ideas. *Planned (PR 3).*

## DuckStation (CC-BY-NC-ND — ideas only)

- **Scheduled Metal presents** (`presentDrawable:atTime:` with
  host-to-mach time conversion; nil-drawable skip) — the only
  shipping-emulator precedent for display-engine-timed presents on
  macOS. We now reach the same machinery through MoltenVK's
  `VK_GOOGLE_display_timing` rather than a Metal backend, but the
  prior-art credit stands; verified by our own Spikes 4 and 5
  (plan Appendix D). *Planned (PR 6).*
- **Acquire-semaphore pool sizing rule** (acquire semaphores must
  outnumber command buffers by one before safe reuse) — subtle
  correctness knowledge absent from the introductory materials.
  Idea. *Planned (PR 2).*
- **Acquire-after-present overlap** — credited by DuckStation itself
  to DXVK; a niche latency technique, evaluation-gated on our side.
  Idea. *Planned (PR 6).*

## PCSX2 (GPLv3 — ideas only)

- **Pre-Ventura windowed MAILBOX quirk** (macOS remaps/breaks MAILBOX
  in windowed mode before Ventura) — hard-won macOS-version quirk
  knowledge that puts the OS version into our test matrix; still
  relevant through MoltenVK, whose swapchain rides the same
  CAMetalLayer. Idea/workaround. *Planned (PR 2/PR 6 macOS
  testing).*

## RPCS3 (GPLv2 — adaptable)

- **Per-vendor present-timing trust table** (NVIDIA/Intel-ANV report
  honestly; AMD/RADV/MoltenVK do not —
  `rsx/VK/vkutils/swapchain.cpp`) — feeds our feedback validation;
  exactly the kind of knowledge only years of driver triage produce.
  Data + idea; SPDX credit on adaptation. *Planned (PR 6).*
- **NVIDIA minimise quirk** (the driver spams `OUT_OF_DATE` while
  minimised, and recreating the swapchain in that state can crash —
  skip presents, do not recreate). Idea/workaround, with provenance
  recorded per our workaround rule. *Planned (PR 2).*
- **Adaptive acquire-timeout with drain-and-retry** (on timeout,
  drain queued frames and retry rather than wait or fail). Idea.
  *Planned (PR 2).*

## Cemu (MPL-2.0 — adaptable with the file-level notice rule)

No itemised entries yet. Cemu's service to the study (plan
Appendix C §11) was confirmation with weight: the seventh project
with no Vulkan present scheduling; a production
`VK_KHR_present_wait`-for-backpressure pattern that sharpened our
map of how the timing-adjacent extensions get used (measure /
flow-control / schedule); a second production validation of the
deferred shader-cache design; and a third production macOS-on-
MoltenVK precedent. Its Windows-only host-vblank vsync-matching
mode is the closest existing relative of our app-timed scheduler
tier — studied, credited here as prior art in spirit, and not
adopted (our due-time design is platform-neutral and rate-general).

## Xenia (BSD-3-Clause — adaptable)

- **AMD descriptor-pool sampler-capacity quirk**
  (`src/xenia/ui/vulkan/vulkan_presenter.cc` — with immutable
  samplers the pool must still allocate
  `VK_DESCRIPTOR_TYPE_SAMPLER` capacity or allocation fails on AMD;
  observed on Adrenalin 22.3.2, RX Vega 10, validation on) —
  exactly the immutable-sampler descriptor shape our Vulkan
  executor uses; hard-won driver triage of the kind the credit bar
  exists for. Idea/workaround, provenance recorded per our
  workaround rule. *Planned (PR 3).*
- **Windows borderless-fullscreen GDI-copy finding** (IMMEDIATE
  present mode under borderless fullscreen is GDI-copied rather
  than independently flipped — no tearing, no VRR on that path) —
  shapes our VRR guide's fullscreen guidance and the pacing test
  expectations. Idea/quirk knowledge. *Planned (PR 6/PR 7 docs).*

Beyond the itemised entries, Xenia's presenter (by Triang3l) is the
most exhaustively commented WSI code in the study — the eighth
no-scheduling confirmation, and the best written explanation of why
swapchain teardown is fragile. That service deserves this mention.

## Khronos Vulkan-Samples (Apache-2.0)

Below the credit bar by definition: the samples are the official
reference material whose entire purpose is to be followed, so using
their patterns (swapchain recreation, sync pooling,
`swapchain_maintenance1` handling) earns no idea credit. If any
sample code is directly adapted, it receives its Apache-2.0
`SPDX-FileCopyrightText` line at that point and an entry here.

## Khronos Vulkan-Tutorial (Apache-2.0)

Same standing as the samples — official reference material meant to
be followed. One planned adaptation is recorded because actual code
will derive from it:

- **Instance/device initialisation sequence** (the modern Vulkan-Hpp
  RAII tutorial's init chapter) — the licence-clean template for our
  hand-written ~200–400-line device bring-up after vk-bootstrap was
  dropped (plan decision 13). Adapted code; Apache-2.0 SPDX credit
  on landing. *Planned (PR 2).*

## Libraries we build on

Not study subjects but load-bearing dependencies, thanked here all
the same: **MoltenVK** (The Brenwill Workshop Ltd. / Khronos — the
layered Vulkan-over-Metal implementation that let us delete an entire
backend), **Vulkan-Hpp** (Khronos), **glslang** and **SPIRV-Cross**
(Khronos, the latter by Hans-Kristian Arntzen — whose writing on
Vulkan also taught us plenty), and **SDL** (Sam Lantinga and
contributors), which keeps doing our windowing, input, and audio even
as we take the GPU into our own hands. Two libraries were evaluated
in earnest and set aside with thanks rather than used —
**vk-bootstrap** (Charles Giessen) and **Vulkan Memory Allocator**
(AMD GPUOpen): both excellent at what they do; our requirements
turned out too small to need them (plan Appendix E).
