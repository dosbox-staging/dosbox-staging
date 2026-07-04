# PR 2 — Vulkan backend skeleton (execution spec)

Part of the native Vulkan render backend series. Rationale and
decisions live in `native-gpu-backends-plan.md` (“the plan”),
especially the Architecture section, decision 12 (layout/style),
decision 13 (Vulkan-Hpp RAII), decision 5 (dependencies), and
Appendix C §2–§5 (the distilled swapchain/sync patterns). This file
is the implementation contract for PR 2. **If reality disagrees with
this spec, stop and update the spec first.**

**Licence firewall reminder (hard rule):** while implementing this
PR you may consult Dolphin, PPSSPP, RPCS3, and Khronos
Vulkan-Samples/Vulkan-Tutorial (with attribution). You must NOT
open, consult, or copy from DuckStation, RetroArch, or PCSX2 — their
lessons are already distilled into the plan's Appendix C, and you
work from that text alone.

## Goal

`output = vulkan` reaches feature parity with the `texture` backend
on Linux, Windows, and macOS (via MoltenVK): correct image, resize,
fullscreen, vsync switching, captures — no shaders (those are PR 3).
The whole present path is uploads, clears, and blits, keeping the PR
focused on the genuinely hard part: device, swapchain, and
synchronisation lifecycle.

## Prerequisites

- PR 1 merged.
- Dev machine: macOS needs `brew install molten-vk vulkan-headers`
  (already present on John's machine) and, for validation layers,
  the Khronos loader + validation layers (see commit 4's macOS
  loading ladder).

## Success criteria (PR level)

1. `output = vulkan` boots to the DOS prompt on all three platforms;
   every video-mode class renders identically to the `texture`
   backend (same CPU-expanded BGRX32 input, same letterboxing).
2. Live resize, fullscreen toggle, minimise/restore, and vsync
   toggling all work without validation errors or crashes.
3. The emulation loop never blocks on presentation: acquire uses
   zero timeout and skips; `adjust_ticks_after_present_frame`
   accumulation reads ≈ 0 (PR 1 instrumentation shows it).
4. All capture types produce correct output.
5. A lavapipe CI job proves boot + present + clean validation on
   every future commit.
6. Fallback chain works: `output = vulkan` on a Vulkan-less machine
   falls back to opengl → texture with clear log warnings.
7. Every commit compiles and tests green standalone
   (`./scripts/tools/compile-commits.sh $PRESET --test`).

## Standing rules (every commit)

- Learning-doc chapter per commit (decision 10; the device and
  swapchain commits deserve long ones — this is the heart of the
  curriculum).
- Attribution: this PR lands several studied patterns. The specific
  entries in `native-gpu-backends-attribution.md` to flip from
  “Planned” to “landed <hash>” are listed per commit below — same
  commit as the code, with the code-level credit comment (plan's
  Attribution policy has comment-format examples).
- New code style (decision 12): namespace `Vulkan`, de-stuttered
  class names (`Vulkan::Renderer`), files prefixed `vulkan_`,
  C++23 (`std::expected` factories, designated initialisers,
  `std::span`), no exceptions (`VULKAN_HPP_NO_EXCEPTIONS`), UpperCamelCase
  methods, `lower_snake_case` members, unit suffixes, `//` comments,
  `CHECK_NARROWING()` + `"util/checks.h"` in new .cpp files.
- `./scripts/tools/format-commit.sh` before every commit.
- Commit subjects below are pre-agreed; use verbatim.

## Commits

### Commit 1 — `Reorganise the render module into per-backend subdirectories`

The decision-12 move-only commit. **The only content changes allowed
are the ones moves force: include paths, CMake source lists, and
include-guard renames for the two renamed file pairs. No logic
changes, no class renames** (class renames come with PR 3's rewrite).

**Move table** (verified against the current tree; use `git mv`):

| From (src/gui/render/) | To (src/gui/render/) |
| ---------------------- | -------------------- |
| render_backend.h, render.h, render.cpp, frame_dirty_tracker.h, format_convert.h/.cpp, deinterlacer.cpp, private/deinterlacer.h | stay where they are |
| opengl_renderer.h/.cpp | opengl/ |
| macos_colorspace.h/.mm | opengl/ |
| private/shader.h + shader.cpp | opengl/private/opengl_shader.h/.cpp |
| private/shader_pipeline.h + shader_pipeline.cpp | opengl/private/opengl_pipeline.h/.cpp |
| sdl_renderer.h/.cpp | sdl/ |
| private/shader_manager.h + shader_manager.cpp | shaders/shader_manager.h/.cpp |
| private/shader_common.h | shaders/shader_common.h |
| private/shader_pragma_parser.h + shader_pragma_parser.cpp | shaders/private/ |
| private/auto_shader_switcher.h + auto_shader_switcher.cpp | shaders/ (see placement rule) |
| private/auto_image_adjustments.h + auto_image_adjustments.cpp | shaders/ (see placement rule) |

**Placement rule** (from the plan's target-state listing): a header
consumed across subdirectory boundaries sits at its subdirectory's
top level; a header consumed only within its subdirectory goes in
its `private/`. Before moving the last four rows, check consumers:

```sh
grep -rn "auto_shader_switcher.h\|auto_image_adjustments.h\|shader_manager.h\|shader_common.h" src/ tests/
```

Expected outcome: `shader_manager.h` and `shader_common.h` are
consumed by the OpenGL backend and render core → public top level of
`shaders/`; the two auto_* headers are consumed from outside
`shaders/` (render core drives them) → top level too. If the grep
says otherwise, follow the rule, not this table — and update this
spec and the plan's listing in the same commit.

**Steps:**

1. `git mv` per the table; create `opengl/private/`, `sdl/`,
   `shaders/private/` as needed.
2. Rename include guards in the two renamed pairs
   (`DOSBOX_SHADER_H` → `DOSBOX_OPENGL_SHADER_H` style, matching the
   repo's `DOSBOX_MODULENAME_HEADERNAME_H` convention). Moved-but-
   unrenamed files keep their guards.
3. Fix every `#include` that referenced a moved path (the grep above
   plus a clean build finds them all — including `tests/`).
4. Update the source lists in `src/gui/CMakeLists.txt` (and
   `tests/CMakeLists.txt` if it names paths).

**Guardrails:**

- `git status` must show only renames plus the files whose includes
  changed. Verify rename detection:
  `git diff --cached -M90% --stat | grep -c "=>"` — every moved file
  must appear as a rename, not delete+add.
- `git log --follow src/gui/render/opengl/private/opengl_shader.cpp`
  must show the file's full pre-move history.

**Automated verification:** full build + full `ctest` on your
platform preset; then `./scripts/tools/compile-commits.sh $PRESET`
still applies at PR end. Boot probe (`$DOSBOX -conf probe.conf` with
`[autoexec] exit`) on both existing backends.

**Attribution flips:** none.

### Commit 2 — `build: add the Vulkan build plumbing`

**Files:** `vcpkg.json`, `CMakeLists.txt` (top level),
`src/dosbox_config.h.in.cmake`, `src/gui/CMakeLists.txt`,
`.github/workflows/linux.yml`, `.github/workflows/macos.yml`.

**Steps:**

1. `vcpkg.json`: add `"vulkan-headers"` to dependencies, and give
   SDL3 the `vulkan` feature on every platform (it is currently
   Linux-only — without it `SDL_Vulkan_CreateSurface` is compiled
   out of SDL on Windows/macOS):

   ```json
   { "name": "sdl3", "platform": "linux",
     "features": ["alsa", "ibus", "vulkan", "wayland", "x11"] },
   { "name": "sdl3", "platform": "!linux",
     "features": ["vulkan"] },
   "vulkan-headers",
   ```

   MoltenVK has **no vcpkg port** (verified 2026-07-04 against the
   pinned baseline) — it comes from Homebrew on macOS and is
   runtime-loaded, never linked; nothing to add to the manifest.

2. Top-level `CMakeLists.txt`: mirror the `OPT_OPENGL` block
   (~lines 335-338):

   ```cmake
   option(OPT_VULKAN "Enable Vulkan render backend" ON)
   if (OPT_VULKAN)
     set(C_VULKAN ON)
     find_package(VulkanHeaders CONFIG REQUIRED)
   endif()
   ```

   (vcpkg's vulkan-headers exports the `Vulkan::Headers` target —
   headers only, no loader; SDL loads the library at runtime, flesh-
   out decision 2.)

3. `src/dosbox_config.h.in.cmake`: add `#cmakedefine01 C_VULKAN`
   next to `C_OPENGL` (~line 72).

4. `src/gui/CMakeLists.txt`: next to the existing
   `if (C_OPENGL) target_link_libraries(... glad)` block, add
   `if (C_VULKAN) target_link_libraries(dosboxcommon PRIVATE
   Vulkan::Headers)`.

5. CI: option defaults ON, so the standard jobs pick it up with no
   edits. Add `-DOPT_VULKAN=OFF` to the `cmake_flags` of exactly ONE
   existing linux.yml matrix entry (compile-out coverage without a
   new job). In macos.yml, add `molten-vk` to the brew install step
   (needed at runtime from commit 4 onward; harmless now).

**Guardrails:** no source files change in this commit; `C_VULKAN` is
defined but unused — that is intentional.

**Automated verification:**

```sh
cmake --preset $PRESET && cmake --build --preset $PRESET      # ON
cmake --preset $PRESET -DOPT_VULKAN=OFF && cmake --build ...  # OFF
# then reconfigure back ON. Verify vulkan.hpp is reachable:
echo '#include <vulkan/vulkan_raii.hpp>
int main() { return VK_HEADER_VERSION > 0 ? 0 : 1; }' > /tmp/hpp.cpp
# compile it with the include path vcpkg produced (find it under
# build/$PRESET/vcpkg_installed/*/include) — must compile clean.
```

**Attribution flips:** none.

### Commit 3 — `Add the vulkan output backend enum and config plumbing`

**Files:** `src/gui/private/common.h`, `src/gui/sdl_gui.cpp`.

**Steps:**

1. `RenderBackendType` (gui/private/common.h:70) becomes
   `enum class RenderBackendType { OpenGl, Sdl, Vulkan };` (append —
   don't reorder).
2. `configure_renderer()` (sdl_gui.cpp:241-265): add a
   `"vulkan"` → `RenderBackendType::Vulkan` mapping inside
   `#if C_VULKAN`, following the exact shape of the `"opengl"`
   mapping (which sits inside `#if C_OPENGL`).
3. `create_renderer()` (sdl_gui.cpp:1526-1576): add the Vulkan
   branch ahead of the OpenGL one. Until commit 4 lands there is no
   class to construct, so the branch body is, for this commit only:
   log `LOG_WARNING("DISPLAY: Vulkan backend not yet implemented; "
   "falling back to OpenGL output")`, set the backend type to
   OpenGl, and fall through to the existing chain. Mirror the
   existing OpenGL→Sdl fallback's shape, including how it writes the
   effective value back to the config (`set_section_property_value`
   — see the existing handler at ~:1537-1551).
4. The `output` setting registration (sdl_gui.cpp:2545 and the
   values list at ~:2574-2580): add `vulkan` to `Set_values` and the
   help text under `#if C_VULKAN` only, so the value is simply
   invalid where the backend is compiled out.

**Automated verification:**

```sh
cmake --build --preset $PRESET && ctest --preset $PRESET
cat > /tmp/probe.conf <<'EOF'
[sdl]
output = vulkan
[autoexec]
exit
EOF
$DOSBOX -conf /tmp/probe.conf 2>&1 | tee /tmp/log.txt
grep -q "falling back to OpenGL" /tmp/log.txt   # must hit
# and the emulator must still reach the prompt and exit 0.
```

This grep-the-fallback probe is the feedback loop for the whole
commit — do not commit until it passes.

**Attribution flips:** none.

### Commit 4 — `Add the Vulkan device and window bring-up`

**New files:** `src/gui/render/vulkan/vulkan_renderer.h/.cpp`
(class `Vulkan::Renderer`, implements `RenderBackend` — every method
a stub except construction/window for now),
`src/gui/render/vulkan/private/vulkan_device.h/.cpp`
(class `Vulkan::Device`). Gate all of it behind `C_VULKAN` in
`src/gui/CMakeLists.txt` (conditional `target_sources`).

**Design contract (follow exactly):**

- `Vulkan::Device` owns, in declaration order (destruction is the
  reverse — get this order right and RAII does teardown):
  `vk::raii::Context`, `vk::raii::Instance`, debug messenger
  (debug builds), `vk::raii::SurfaceKHR`, `vk::raii::PhysicalDevice`
  (non-owning selection), `vk::raii::Device`, `vk::raii::Queue`
  (graphics+present, one family — plan Architecture).
- Construction is a static factory:
  `static std::expected<Device, std::string> Create(SDL_Window*);`
  built on `VULKAN_HPP_NO_EXCEPTIONS` (define it, plus
  `VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1`, in the vulkan target's
  compile definitions) — every `vk::raii` constructor then returns
  `std::expected`, and errors chain outward with
  `std::unexpected(std::string_view reason)`.
  The init sequence follows the official Khronos Vulkan-Tutorial
  RAII chapter (Apache-2.0). Put the credit at the top of
  vulkan_device.cpp:
  `// Init sequence adapted from the Khronos Vulkan-Tutorial
  (Apache-2.0), https://docs.vulkan.org/tutorial/latest/` plus an
  `SPDX-FileCopyrightText` line for the tutorial authors.
- Window: create with the properties pattern the SDL backend uses
  (sdl/sdl_renderer.cpp:32-42) plus the Vulkan window flag
  (`SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN`). Then
  `SDL_Vulkan_LoadLibrary(nullptr)` BEFORE instance creation, and
  `SDL_Vulkan_GetVkGetInstanceProcAddr()` seeds the Hpp dynamic
  dispatcher (`vk::raii::Context` accepts the function pointer).
  Surface via `SDL_Vulkan_CreateSurface` (on macOS this creates the
  CAMetalLayer).
- Instance extensions: exactly what
  `SDL_Vulkan_GetInstanceExtensions()` returns, PLUS
  `VK_KHR_portability_enumeration` + the
  `eEnumeratePortabilityKHR` instance flag **only if that extension
  is advertised** by `enumerateInstanceExtensionProperties()`.
  Rationale (Spike 5): with the Khronos loader present the flag is
  required to see MoltenVK; when MoltenVK is loaded directly the
  extension does not exist and requesting it fails instance
  creation. Probe, never assume. Same probe-first rule for the
  validation layer (`VK_LAYER_KHRONOS_validation`, debug builds
  only, messages routed to `LOG_WARNING` via a debug-utils
  messenger).
- Physical device selection (our requirements are deliberately
  trivial): device must offer (a) API ≥ 1.3, or API ≥ 1.2 with the
  `VK_KHR_dynamic_rendering` extension (the MoltenVK proviso —
  flesh-out decision 1); (b) `VK_KHR_swapchain`; (c) one queue
  family with graphics + present to our surface. Prefer discrete >
  integrated > CPU when several qualify; log the chosen device name
  and API version. Enable dynamic rendering via
  `vk::PhysicalDeviceDynamicRenderingFeatures` in the device create
  chain (works for both the 1.3-core and extension cases; list the
  extension explicitly when running the 1.2 path).
- `FindMemoryType(type_bits, required_properties)` — the ~15-line
  tutorial-standard helper on `Device`; no allocator library
  (decision 5).
- `Vulkan::Renderer::Create(...)` wires window + Device and returns
  `std::expected`; `create_renderer()`'s commit-3 stub branch is
  replaced by real construction, falling back to OpenGL exactly as
  the stub did when `Create` returns an error.

**macOS loading ladder (write it into a comment in the code):**
`SDL_Vulkan_LoadLibrary(nullptr)` finds, in order: a bundled
`libvulkan.dylib`/`libMoltenVK.dylib` next to the binary or in
`Contents/Frameworks`, then system paths. For dev machines, the
Homebrew Khronos loader + MoltenVK ICD is the recommended setup
(validation layers only work through the loader):

```sh
brew install vulkan-loader vulkan-validationlayers molten-vk
export VK_ICD_FILENAMES="$(brew --prefix molten-vk)/share/vulkan/icd.d/MoltenVK_icd.json"
```

If the default load fails at runtime, retry with explicit well-known
paths (`$(brew --prefix)/lib/libvulkan.dylib`, then
`.../libMoltenVK.dylib`) before falling back to OpenGL with a log
message naming what was tried. Release-bundling of MoltenVK into the
.app (copy dylib + rpath fixup in the packaging job) is deliberately
deferred to PR 7's polish — record a TODO in macos.yml.

**Automated verification:**

```sh
cmake --build --preset $PRESET && ctest --preset $PRESET
$DOSBOX -conf /tmp/probe.conf 2>&1 | tee /tmp/log.txt   # output=vulkan
grep -qi "vulkan.*device\|device.*vulkan" /tmp/log.txt  # device log line
# Window comes up black (no upload path yet) and exits 0 cleanly.
# Debug build + validation layers: zero "Validation Error" lines.
# Deliberate-failure probe: run with VK_ICD_FILENAMES=/dev/null
# (Linux/macOS-with-loader) → must fall back to OpenGL gracefully.
```

**Attribution flips:** the Vulkan-Tutorial init-sequence entry
(“Khronos Vulkan-Tutorial” section) → landed.

### Commit 5 — `Add the swapchain and per-frame ring`

**New files:** `src/gui/render/vulkan/private/vulkan_swapchain.h/.cpp`
(class `Vulkan::Swapchain`),
`tests/vulkan_swapchain_policy_tests.cpp`.

**Design contract:**

- Creation: prefer `B8G8R8A8_UNORM` (avoid sRGB formats — plan
  Appendix C §2); image count `minImageCount + 1` for FIFO,
  `max(minImageCount + 2, 3)` for MAILBOX, clamped to caps; usage
  `TRANSFER_DST | COLOR_ATTACHMENT` (blit target now, OSD blend
  later — flesh-out decision 6); pass `oldSwapchain` on recreation.
- Per-frame ring: `MaxFramesInFlight = 2`. Each in-flight frame
  owns: one fence (created signalled), one command pool + primary
  command buffer. Acquire semaphores live in a pool of
  `MaxFramesInFlight + 1`, rotated — **a semaphore cannot be reused
  until its acquire has provably completed** (credit comment:
  `// Acquire-semaphore pool sized command-buffers+1, after
  DuckStation's sizing rule (idea credit — see
  native-gpu-backends-attribution.md)`). One render-done semaphore
  per swapchain image (indexed by acquired image, not by frame).
- The recreation state machine is pure logic, extracted for tests:

  ```cpp
  // vulkan_swapchain.h — pure, unit-tested
  namespace Vulkan {
  enum class WsiEvent { AcquireOutOfDate, AcquireSuboptimal,
                        PresentOutOfDate, PresentSuboptimal,
                        WindowResized, VsyncChanged, ZeroExtent,
                        ExtentRestored };
  enum class WsiAction { None, SkipPresentation, RebuildNow,
                         RebuildDeferred };
  WsiAction DecideWsiAction(WsiEvent event, bool is_zero_extent,
                            bool anything_changed);
  }
  ```

  Behaviour to encode (plan Appendix C §2/§10): OUT_OF_DATE at
  acquire or present → RebuildNow (but ZeroExtent overrides —
  see next); SUBOPTIMAL → finish frame, RebuildDeferred; zero
  extent (minimised) → SkipPresentation and **never rebuild while
  extent is 0×0** (credit comment:
  `// NVIDIA spams OUT_OF_DATE while minimised and recreating can
  crash — skip and rebuild on restore, after RPCS3's triage (idea
  credit)`); redundancy gate — `anything_changed == false` →
  None; ExtentRestored → RebuildNow.
- Teardown discipline: old swapchains are destroyed deferred, once
  the last frame that used them has its fence signalled (keep a
  small retire list). Device-level waits only in full teardown, with
  a finite timeout on any fence wait (credit comment:
  `// Finite waits everywhere: a wedged driver must not hang the
  emulator — after RetroArch's WSI hardening (idea credit)`).
- Probe and use `VK_EXT_surface_maintenance1` +
  `VK_EXT_swapchain_maintenance1` when offered: per-present fences
  (cleaner semaphore recycling) and recreation-free present-mode
  switching. Both paths must exist; maintenance1 is an optimisation,
  not a requirement.
- Present-mode mapping lives here but is driven by `SetVsync` in
  commit 7: vsync on → FIFO; off → IMMEDIATE, falling back
  MAILBOX → FIFO by availability (decision 8).

**Unit tests** (`vulkan_swapchain_policy_tests.cpp`): full
event × state matrix for `DecideWsiAction` — every rule above is a
test case, including “OUT_OF_DATE while minimised does NOT rebuild”.
This is the self-verify loop for the trickiest logic in the PR;
write the tests first.

**Automated verification:** build + ctest (new tests green) + boot
probe with `output = vulkan` — still black screen, but the log now
shows swapchain creation (format, image count, present mode) and a
clean exit; validation clean in a debug run.

**Attribution flips:** DuckStation “Acquire-semaphore pool sizing
rule” → landed; RPCS3 “NVIDIA minimise quirk” → landed; RPCS3
“Adaptive acquire-timeout” → landed (the finite-timeout hygiene);
RetroArch “Finite acquire timeouts” → landed. (RetroArch's
fault-injection testing idea lands with the policy unit tests —
flip it too and note the tests are our re-expression of the idea.)

### Commit 6 — `Implement frame upload and the blit present path`

**Files:** vulkan_renderer.cpp/.h, vulkan_swapchain.cpp/.h.

**Design contract:**

- **Staging:** one host-visible slot per in-flight frame
  (`HOST_VISIBLE | HOST_COHERENT`, allocated with `FindMemoryType`,
  **persistently mapped at creation, never re-mapped** — decision 5
  footgun rule), grown on demand to
  `max_source_width × max_source_height × 4` bytes.
- **`UploadFrame(pixels, width, height, pitch, first_row, num_rows,
  double_width, double_height, video_mode)`** (signature per
  render_backend.h:96-100): expand rows into the current frame's
  slot with `ExpandBgrx32Row` (format_convert.h — same helper the
  SDL backend uses), honouring the dirty span (`first_row`,
  `num_rows`) so unchanged rows are neither expanded nor copied;
  record `vkCmdCopyBufferToImage` into a source-dimension
  `B8G8R8A8_UNORM` sampled+transfer image (recreated when source
  dimensions change). Layout transitions, explicitly:
  UNDEFINED→TRANSFER_DST before the copy, TRANSFER_DST→TRANSFER_SRC
  before the target blit.
- **Target image:** persistent, viewport-sized, TRANSFER_SRC|DST;
  input image blits into it with `vkCmdBlitImage`, linear filter
  (this is where GPU-side doubling happens: source-dim image →
  viewport-size target). Carries the same `output_stale` /
  clean-present semantics as the GL backend: when nothing changed,
  presents re-blit the cached target.
- **`PresentFrame()`:** acquire with **timeout 0** —
  `VK_NOT_READY`/`VK_TIMEOUT` → skip the present, bump a
  `DEBUG_PRESENT_PACING` skip counter, return (the never-block
  property; plan Architecture). On success: transition swapchain
  image, `vkCmdClearColorImage` to black (letterbox), blit target →
  viewport rectangle, transition to PRESENT_SRC, submit
  (wait acquire-semaphore at TRANSFER stage, signal render-done +
  frame fence), present (wait render-done). Route
  OUT_OF_DATE/SUBOPTIMAL results into `DecideWsiAction`. Add the
  marker comment after the letterbox blit, verbatim:
  `// TODO(osd): compositing point — see native-gpu-backends-plan.md`
- Frame-fence wait at frame start uses a finite timeout (1 s) with a
  logged error path, never `UINT64_MAX`.

**Automated verification:** build + ctest + boot probe with
`output = vulkan`: **the DOS prompt is now visible.** Debug build
run: zero validation errors while booting, resizing (drag corner),
and toggling fullscreen manually. On Linux (or in CI after commit
8): same probe under xvfb + lavapipe.

**Manual verification (required now, matrix at PR end):** DOS prompt
plus one game in mode 13h, one text-mode screen, live resize,
fullscreen toggle, minimise/restore ×5 (the NVIDIA quirk path on
the RTX 3060 box). Compare the image side-by-side against
`output = texture` — identical letterboxing and scaling.

**Attribution flips:** none (upload design is ours; the plan's
Appendix C §5 explains why the emulators' rings don't apply).

### Commit 7 — `Wire vsync, captures, and the remaining backend interface`

**Files:** vulkan_renderer.cpp/.h, vulkan_swapchain.cpp/.h,
`src/gui/sdl_gui.cpp` (audit only).

**Steps:**

1. `SetVsync(bool)` (the effective value arrives pre-resolved —
   sdl_gui owns the windowed/fullscreen/`fullscreen-only` logic):
   maintenance1 path switches present mode without recreation;
   otherwise flag `VsyncChanged` into the recreation machinery.
2. `ReadPixelsPostShader(output_rect_px)`: copy the target image
   region into a host-visible readback buffer, wait its fence
   (finite timeout), return a BGRX32 `RenderedImage` with the same
   field semantics as `OpenGlRenderer::ReadPixelsPostShader` — open
   that implementation (opengl/opengl_renderer.cpp) and mirror its
   `RenderedImage` population exactly (pitch, flip, palette fields).
   A screenshot-cadence fence stall is acceptable by design.
3. `MakePixel(r, g, b)`: copy `SdlRenderer`'s BGRX32 formula.
4. `GetCanvasSizeInPixels`, `NotifyViewportSizeChanged` (resize
   target image + letterbox rects), `NotifyRenderSizeChanged`,
   `NotifyVideoModeChanged`: mirror the SdlRenderer/OpenGlRenderer
   division of labour.
5. `SetColorSpace(ColorSpace)`: on macOS probe
   `VK_EXT_swapchain_colorspace` and select the Display-P3 /
   sRGB colour space at swapchain creation accordingly (plan
   Architecture, macOS bullet); on other platforms sRGB nonlinear
   only for now. If the extension is absent: log once, ignore.
6. Shader-related interface methods (`SetShader`,
   `GetCurrentShaderInfo`, adjustments, dedither, …): no-op stubs
   returning the same defaults `SdlRenderer`'s stubs return —
   diff `sdl/sdl_renderer.cpp` for the authoritative list.
7. Audit backend-type conditionals:
   `grep -n "RenderBackendType::" src/gui/sdl_gui.cpp` — visit each
   hit and decide Vulkan's membership. Known case: the macOS
   window-restore GL viewport hack (~:2136-2144) stays OpenGL-only
   unless the restore test shows Vulkan needs it too (test it; leave
   a one-line comment either way).

**Automated verification:** build + ctest; boot probe; scripted
capture probe — boot, take a rendered screenshot via the capture
hotkey sent programmatically if feasible, else defer to manual; run
the PR 1 pacing instrumentation (`DEBUG_PRESENT_PACING 1` local
build) and confirm the `adjust_ticks` cumulative total ≈ 0 on
vulkan (the never-blocks regression signal), then flip back to 0.

**Manual verification:** all capture types (raw/upscaled/rendered);
pause + resize + repaint; vsync on/off toggle live in both windowed
and fullscreen; `vsync = fullscreen-only` crossing the boundary.

**Attribution flips:** PCSX2 “Pre-Ventura windowed MAILBOX quirk” →
landed if/when the vsync-off MAILBOX fallback gains the macOS-
version note in a comment (add it: MAILBOX-windowed is remapped
before Ventura — relevant through MoltenVK).

### Commit 8 — `ci: add a lavapipe smoke test`

**Files:** `.github/workflows/linux.yml`,
`extras/linux/ubuntu-24.04-packages.txt` (or the job's own apt
step), possibly a small script under `scripts/ci/`.

**Steps:**

1. New job (or a step appended to one existing Linux matrix entry —
   prefer a separate `vulkan-lavapipe` job for signal clarity):
   apt-install `mesa-vulkan-drivers vulkan-validationlayers xvfb`
   on top of the standard package list.
2. Run the boot probe under xvfb with lavapipe forced and validation
   force-enabled through the loader (no code changes needed):

   ```sh
   export VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
   export VK_LOADER_LAYERS_ENABLE='VK_LAYER_KHRONOS_validation'
   cat > probe.conf <<'EOF'
   [sdl]
   output = vulkan
   [autoexec]
   dir
   exit
   EOF
   xvfb-run -a ./build/<preset>/dosbox -conf probe.conf \
       > log.txt 2> errs.txt
   ```

3. Assertions (fail the job on any miss): exit code 0; `log.txt`
   contains the Vulkan device log line (lavapipe/llvmpipe device
   name); `errs.txt` contains no `Validation Error`; the run did
   not fall back (`grep -v "falling back"`).

**Guardrails:** if the ICD json filename differs on the runner
(`ls /usr/share/vulkan/icd.d/`), glob it — do not hardcode blindly.
Keep the job under ~5 minutes; it reuses the build artifact from an
existing job if the workflow structure allows, else builds the
standard release preset.

**Automated verification:** the job itself, green on the PR. Local
dry run possible on any Linux box with the same three packages.

**Attribution flips:** none. (Golden-image rendering tests arrive
with PR 3, reusing `tests/image_compare.{h,cpp}`.)

## Manual test matrix (humans, before merge)

Platforms: Linux (core team — NVIDIA/AMD/Intel, X11 AND Wayland),
Windows (RTX 3060 + the three AMD iGPU boxes, Win10 + Win11),
macOS (M4 mini + MBP). `output = vulkan` throughout; compare against
`texture` for correctness.

| # | Scenario | Pass when |
| --- | -------- | --------- |
| 1 | Boot to prompt, every video-mode class (13h double-scan, text, composite CGA, VESA) | Image identical to `texture` backend |
| 2 | Live resize (drag, maximise, restore) | No crash, no validation error, correct letterbox |
| 3 | Fullscreen toggle ×5, both directions | Same |
| 4 | Minimise ≥10 s, restore (NVIDIA box especially) | No crash; presents resume; no rebuild-while-minimised in logs |
| 5 | vsync on/off live toggle; `fullscreen-only` crossing | Mode switches; maintenance1 path logged where supported |
| 6 | All capture types | Correct images, rendered capture matches screen |
| 7 | Pause + resize + un-pause | Correct repaint |
| 8 | FMV + deinterlacing content | Plays correctly (exercises full-frame uploads) |
| 9 | Fallback: machine/driver without Vulkan (or `VK_DRIVER_FILES=/dev/null`) | Falls back opengl→texture with clear warnings |
| 10 | Weak iGPU (5700U): windowed + fullscreen | No obvious perf regression vs texture backend |
| 11 | PR 1 instrumentation on vulkan | `adjust_ticks` total ≈ 0; skip counter ≈ 0 on VRR |
| 12 | Upside-down/mirrored sanity | Image orientation correct (blit rects, not shaders, on this PR) |

## Exit checklist

- [ ] `./scripts/tools/compile-commits.sh $PRESET --test` green for
      all 8 commits
- [ ] Lavapipe CI job green
- [ ] Matrix filled in (Linux rows may lag — track who owes them)
- [ ] Attribution entries flipped: Vulkan-Tutorial, DuckStation
      sizing rule, RPCS3 ×2, RetroArch ×2, (PCSX2 if landed)
- [ ] Learning-doc chapters for all 8 commits
- [ ] Plan's target-state listing still matches reality — update it
      in the final commit if anything drifted
