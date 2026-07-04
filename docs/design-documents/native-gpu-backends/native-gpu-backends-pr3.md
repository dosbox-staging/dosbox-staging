# PR 3 — shader toolchain + Vulkan pipeline (execution spec)

Part of the native Vulkan render backend series. Rationale in
`native-gpu-backends-plan.md`: the Shader-format section (450 spec),
“GL-coupling refactors”, decision 4 (single-source shaders),
decision 5 (glslang; no runtime SPIRV-Cross), Appendix C §6
(RetroArch's shader executor — ideas only, licence firewall), and
Appendix D Spikes 1–2 (the recipe and the library APIs — both
already proven; `spikes/lib_spike.cpp` and `spikes/sharp450.glsl`
are runnable evidence you may read and adapt, they are ours).
**If reality disagrees with this spec, stop and update the spec
first.**

**Licence firewall reminder:** RetroArch's shader pipeline is the
same genre as ours — that is exactly why you must NOT open it. Its
distilled lessons are in Appendix C §6. Dolphin/PPSSPP/RPCS3/
Khronos material stays fair game with attribution.

## Goal

`output = vulkan` runs the full shader system: the glslang-based
450→SPIR-V compiler, the backend-agnostic pass-graph topology
(finally unit-testable), the Vulkan pass executor, and eight shaders
ported to 450 (the seven `_internal/` shaders + `interpolation/
sharp`). On the way, the shader subsystem is decoupled from OpenGL
(the `ShaderManager`/`Shader`/`ShaderPipeline` fusion documented in
the plan). Exit: vulkan rendering `sharp` matches OpenGL, and
lavapipe golden-image tests run in CI.

## Prerequisites

- PR 2 merged (vulkan backend boots, blit path works, lavapipe CI
  job exists).
- Local tools for reference (not build deps): `glslang` and
  `spirv-cross` CLIs (brew) for eyeballing intermediate output.

## Success criteria (PR level)

1. `sharp` on vulkan is visually identical to `sharp` on opengl
   (side-by-side captures) and matches a recorded lavapipe golden.
2. GL rendering is bit-identical before vs after the refactor
   commits (3–4) — the refactor must not change GL behaviour.
3. `Shaders::PipelineTopology` has unit tests covering the
   mode × dedither × doubling × output-size matrix, no GPU needed.
4. `Shaders::UniformPacker` offsets are pinned by golden tests
   (Spike 2 values); no runtime reflection anywhere
   (SPIRV-Cross must NOT appear in the link line).
5. Image adjustments and dedithering demonstrably work on vulkan.
6. Every commit standalone-green; validation stays silent.

## Standing rules

Same as PR 2 (learning-doc chapter per commit, attribution flips in
the same commit, decision-12 style — namespace `Shaders` for the new
shared components — format-commit.sh, CHECK_NARROWING).

## Commits

### Commit 1 — `build: add the glslang dependency`

**Files:** `vcpkg.json`, top-level `CMakeLists.txt`,
`src/gui/CMakeLists.txt`.

1. Add `"glslang"` to vcpkg.json (port verified to exist at the
   pinned baseline). Link it only when `C_VULKAN`:
   inside the `OPT_VULKAN` block add
   `find_package(glslang CONFIG REQUIRED)`; in gui/CMakeLists link
   the targets vcpkg's usage file names — check
   `build/$PRESET/vcpkg_installed/*/share/glslang/usage` for the
   authoritative list (expected: `glslang::glslang` plus the
   default-resource-limits target; older layouts also ship
   `glslang::SPIRV`). Do NOT link SPIRV-Cross — it has no runtime
   role (decision 5).
2. Note for distro packagers goes into the docs later (PR 5);
   nothing to do here beyond a comment next to find_package naming
   glslang a hard dependency of the vulkan backend.

**Automated verification:** configure+build with OPT_VULKAN ON and
OFF; `grep -i spirv-cross build/$PRESET/CMakeCache.txt` → no hits.

**Attribution flips:** none.

### Commit 2 — `Add the runtime GLSL-to-SPIR-V shader compiler`

**New files:** `src/gui/render/shaders/shader_compiler.h/.cpp`
(class `Shaders::Compiler`),
`src/gui/render/shaders/uniform_packer.h/.cpp`
(class `Shaders::UniformPacker`),
`tests/shader_compiler_tests.cpp`, `tests/uniform_packer_tests.cpp`.

**Compiler contract** (Spike 2 proved every line of this — reuse
`spikes/lib_spike.cpp` as the working reference):

```cpp
namespace Shaders {

enum class Stage { Vertex, Fragment };

class Compiler {
public:
        // 450 single-file source in, SPIR-V out. Uses a
        // per-stage preamble ("#define VERTEX 1\n" /
        // "#define FRAGMENT 1\n") so our single-file section
        // format compiles unchanged. Targets Vulkan 1.3 /
        // SPIR-V 1.6. Errors come back formatted the same way
        // the GL backend reports shader errors today (grep
        // opengl_shader.cpp for the format).
        static std::expected<std::vector<uint32_t>, std::string>
        Compile(std::string_view source, Stage stage);
};

} // namespace Shaders
```

Implementation notes: one-time `glslang::InitializeProcess()`
guarded by a function-local static (glslang is not thread-safe and
must be initialised once — Appendix C §6; we are single-threaded,
but keep the init-once shape); resource limits from
`GetDefaultResources()` (`glslang/Public/ResourceLimits.h` — no
hand-copied limits table); `setEnvClient(EShClientVulkan,
EShTargetVulkan_1_3)` / `setEnvTarget(EShTargetSpv,
EShTargetSpv_1_6)`.

**UniformPacker contract:** canonical member order is
`OUTPUT_SIZE`, `INPUT_SIZE_0..N`, then `#pragma` parameters in
declaration order; member types are float and vec2 ONLY (format
rule from the plan — enforced here with a clear error). std140
rules implemented: float aligns 4 / size 4; vec2 aligns 8 / size 8;
offset = align-up(cursor, alignment). The packer exposes
`OffsetOf(name)`, `BlockSizeBytes()`, and
`PackInto(std::span<std::byte>, values...)`.

**Unit tests (the feedback loop for this commit):**

- Golden offsets — the Spike 2 block (`vec2, vec2, float, float,
  vec2`) MUST produce offsets 0, 8, 16, 20, 24 and block size 32.
  These numbers were recorded from SPIRV-Cross reflection over real
  compiled SPIR-V (Appendix D Spike 2); they pin the packer forever.
- Alignment edges: `float, vec2` → 0, 8 (vec2 aligns up);
  `vec2, float, float` → 0, 8, 12.
- Compiler: `spikes/sharp450.glsl` compiles in both stages
  (copy it into `tests/shaders450_data/` as test data); garbage
  input returns a readable error mentioning the line number;
  an identifier named `sampler` fails with glslang's reserved-word
  error (documents the porting rule).

**Automated verification:** `ctest --preset $PRESET -R
"shader_compiler|uniform_packer"` green; full build both OPT states.

**Attribution flips:** none.

### Commit 3 — `Split shader sources from compiled shader objects`

Pure refactor; zero behaviour change. This commit also performs the
class renames that PR 2's file renames prepared
(`Shader` → `OpenGlShader`, `ShaderPipeline` → `OpenGlPipeline`) —
the classes are being rewritten here anyway.

**Steps:**

1. In `shaders/shader_common.h` introduce
   `struct ShaderSource { std::string name; std::string source_text;
   ShaderInfo info; };`
2. `ShaderManager` (shaders/shader_manager.h/.cpp): `LoadShader` →
   `GetShaderSource`, returning the cached `ShaderSource`; its
   `shader_cache` becomes
   `std::unordered_map<std::string, ShaderSource>`. The manager no
   longer includes any GL header — that is the acceptance test of
   the decoupling (`grep -i "gl\|glad" shaders/shader_manager.*` →
   only false positives allowed).
3. GL compilation moves behind the OpenGL backend: `OpenGlShader`
   (renamed class) keeps `BuildShaderProgram` +
   `SetUniform*`; `OpenGlPipeline` owns a compiled-program cache
   keyed by shader name, compiling from `ShaderSource.source_text`
   on demand.
4. Chase every caller (`grep -rn "LoadShader\|ShaderPipeline\|
   class Shader" src/ tests/`).

**Guardrails:** no parsing, no pass logic, no uniform changes in
this commit. If you find yourself editing `SetPassOutputSizes`, you
have drifted into commit 4 — back out.

**Automated verification:** full ctest; boot probes on opengl AND
vulkan AND texture; **GL golden capture check** — before starting
the commit, capture `sharp` + one CRT shader on a static screen
(Ctrl+F5), rebuild after the change, recapture, `cmp` the PNGs —
byte-identical required.

**Attribution flips:** none.

### Commit 4 — `Extract the backend-agnostic pipeline topology`

**New files:** `src/gui/render/shaders/pipeline_topology.h/.cpp`
(namespace `Shaders`), `tests/pipeline_topology_tests.cpp`.

**Contract** (shape per the plan; refine against reality — the
authoritative current logic lives in `OpenGlPipeline`:
`CreatePipeline` (~:228) and `SetPassOutputSizes` (:286-329)):

```cpp
namespace Shaders {

struct PassDesc {
        std::string name = {};
        // Inputs by pass index; SourceImage (-1) = the uploaded
        // frame. Mirrors the Previous|Rendered|VideoMode|Viewport
        // input/output semantics of the pragma format.
        std::vector<int> input_pass_indices = {};
        DosBox::Rect output_rect_px = {};
        TextureFilterMode filter    = {};
        // wrap mode, float_output flag, per-pass ShaderInfo ref
};

struct PipelineTopology {
        std::vector<PassDesc> passes = {};
        // staleness rules: which passes re-run on which change
        // classes (source dirty / adjustments / viewport) — move
        // the output_stale decision inputs here
};

PipelineTopology BuildPipelineTopology(/* sources, input size,
        doubling flags, video mode, viewport rect, dedither
        strength, adjustments enabled */);

} // namespace Shaders
```

The exact parameter list must be derived from what
`OpenGlPipeline::CreatePipeline` actually consumes — move the
*decisions* (which passes run, in what order, at what sizes, with
which staleness rules) and leave GL resource creation (FBOs,
textures, program binds) behind. `OpenGlPipeline` is then refactored
to consume the returned `PipelineTopology`.

**Unit-test matrix** (this is the point of the commit — the graph
logic becomes testable without a GPU): video modes {320×200 mode 13h
with double-scan flags, 720×400 text, 640×480 VESA} × dedither
{off, on} × doubling {none, width, height, both} × adjustments
{off, on} × shader output-size classes as exercised by the real
`ShaderInfo` of `sharp` and one multi-pass internal chain. Assert:
pass count, each pass's resolved output rect, input wiring, and the
staleness classification. Use real parsed `ShaderInfo` from the
bundled shader sources (the pragma parser is already unit-tested —
reuse its fixtures pattern).

**Automated verification:** new tests green; full ctest; the GL
golden capture check again (byte-identical before/after).

**Attribution flips:** RetroArch “Multi-pass shader pipeline
architecture prior art” → landed (idea-credit comment at the top of
pipeline_topology.cpp: per-pass output-size semantics and pre-baked
sampler grids are the genre conventions RetroArch's slang pipeline
established — no code was consulted, per the licence firewall).

### Commit 5 — `Add the Vulkan shader pipeline executor`

**New files:**
`src/gui/render/vulkan/private/vulkan_pipeline.h/.cpp`
(class `Vulkan::Pipeline`), `tests/vulkan_golden_tests.cpp`,
golden PNGs under `tests/vulkan_golden_data/`.

**Design contract:**

- Exactly ONE `vk::raii::DescriptorSetLayout` in the whole system:
  binding 0 = uniform buffer (vertex+fragment stages), bindings
  1..N = combined image samplers (N = max inputs any pass uses).
- Per pass: one `vk::raii::Pipeline` (dynamic rendering — colour
  attachment format chosen per the pass's `float_output`:
  `B8G8R8A8_UNORM` or `R16G16B16A16_SFLOAT`; dynamic viewport +
  scissor), one persistently-mapped UBO sized by
  `UniformPacker::BlockSizeBytes()`, one descriptor set allocated
  from a pool sized at pipeline (re)build and **written once** —
  never updated per frame (flesh-out decision 7: uniforms change
  only on rebuild/settings changes; the clean-present invariant).
- Vertex input: replicate the GL quad geometry semantics —
  read `OpenGlPipeline`'s vertex setup (`a_position` contents,
  winding, coordinate space) and reproduce it with one shared
  vertex buffer; the 450 shaders keep
  `layout(location = 0) in vec2 a_position` (format spec).
- Intermediate images: `COLOR_ATTACHMENT | SAMPLED`, allocated via
  `Device::FindMemoryType`, recreated when the topology's sizes
  change, deferred-destroyed like swapchains.
- Sampler grid: filters {nearest, linear} × wraps {the four modes
  the pragma format supports} created once up front.
- Final pass renders into PR 2's target image; `output_stale`
  clean-present semantics identical to GL (drive both backends from
  the same `PipelineTopology` staleness data).
- Uniform upload: `UniformPacker::PackInto` directly into the
  mapped UBO; offsets are the packer's — there is no reflection.

**Golden-image tests** (the CI feedback loop; reuses
`tests/image_compare.{h,cpp}` — see its header for the
write-reference-on-first-run workflow):

- The test creates a HEADLESS Vulkan context: instance with **no
  WSI extensions, no surface, no swapchain** — golden tests
  exercise `Vulkan::Pipeline` into an offscreen target image and
  read it back; the swapchain is deliberately out of scope (it is
  exercised by the PR 2 smoke job).
- `GTEST_SKIP()` cleanly when no Vulkan implementation is present,
  so contributor machines without Vulkan never break.
- Determinism policy: references are recorded on **lavapipe** (the
  CI device). The test inspects the device name: on
  llvmpipe/lavapipe it compares with tolerance 0.0; on any other
  device (e.g. a dev Mac on MoltenVK) it uses tolerance 0.02 —
  cross-GPU rounding differs and “knowably equivalent” is the
  standard (plan Risks).
- Cases in this commit: `sharp` over a synthetic checkerboard at a
  non-integer scale, and one two-pass chain
  (`integer-upscale` feeding `image-adjustments`).
- CI wiring: the PR 2 lavapipe job additionally runs
  `ctest -R vulkan` with `VK_DRIVER_FILES` set (goldens + policy
  tests), not just the boot smoke.

**Automated verification:** `ctest -R "vulkan"` locally
(MoltenVK path) and in the lavapipe job; validation layers silent
during the golden run (debug build).

**Attribution flips:** none new.

### Commit 6 — `Port the internal shaders and sharp to the 450 format`

**Files:** eight new 450 sources under
`resources/shaders/vulkan450/` mirroring the existing subpaths
(temporary tree — PR 5 inverts the layout and deletes it);
`shaders/shader_manager.cpp` (per-backend source-path resolution);
`vulkan_renderer.cpp` (SetShader + friends);
`src/gui/render/render.cpp` (setting help text).

**Steps:**

1. Port with the Appendix D recipe, per shader, in this order —
   `interpolation/sharp` first (the spike already converted it:
   diff yours against `spikes/sharp450.glsl`), then the seven
   `_internal/` shaders. Recipe (per file):
   `#version 330 core` → `#version 450`; loose uniforms → the ONE
   anonymous std140 block in canonical order, declared once above
   the stage sections; samplers → `set = 0, binding = 1+`;
   `layout(location = N)` on every varying, matched across stages;
   reserved-word audit — run
   `grep -nwE "sampler|texture|buffer|coherent" <file>` and rename
   collisions (rule #1 from the spike: `sampler` bit immediately);
   `#pragma` comment block byte-identical; bodies otherwise
   untouched.
2. Per-shader verification loop (do NOT batch — port, verify,
   next): compile both stages through `Shaders::Compiler` (extend
   the compiler unit test's data-driven list with each newly ported
   file — the test enumerates `resources/shaders/vulkan450/`);
   then eyeball on screen.
3. `ShaderManager` resolves per backend: vulkan → `vulkan450/` tree,
   opengl/texture-era paths unchanged. User shader directories are
   NOT dual-tree: a user file is assumed to be in the active
   backend's format (plan Open question 1's default — pending
   John's veto).
4. Wire on the vulkan backend: `SetShader` (compile all passes; on
   ANY user-shader compile failure log a warning naming the 450
   requirement + the porting guide and fall back to `sharp`,
   mirroring today's broken-shader recovery in render.cpp
   ~:903-917), `ForceReloadCurrentShader`, shader-info getters,
   `EnableImageAdjustments`/`SetImageAdjustmentSettings`,
   `SetDeditheringStrength`. The auto-shader-switcher needs zero
   changes (it resolves names; names are backend-independent).
5. Update the `shader` setting help text (render.cpp:1677 —
   currently “Shaders are only supported in the OpenGL output
   mode”) to name both backends.

**Automated verification:** compiler unit test now covers all eight
450 files; goldens extended to the dedither chain + adjustments
(record on lavapipe); full ctest; boot probe with
`shader = crt-auto` on vulkan (auto-switcher path).

**Manual verification:** side-by-side vulkan vs opengl captures of
`sharp` — visually identical; adjustments sliders and dedither
demonstrably take effect on vulkan; a deliberately-broken user
shader produces the warning + sharp fallback.

**Attribution flips:** none.

## Manual test matrix (humans, before merge)

`output = vulkan` unless stated; macOS + Windows NVIDIA minimum,
Linux rows to the core team.

| # | Scenario | Pass when |
| --- | -------- | --------- |
| 1 | `sharp` vs opengl side-by-side, mode 13h + text | Visually identical |
| 2 | `crt-auto` family on 13h/text/VESA | Correct shader selected (log), renders like opengl |
| 3 | Image adjustments on/off live | Takes effect, clean-present still skips when static |
| 4 | Dedither strength sweep | Takes effect |
| 5 | Shader switch at runtime ×10 (incl. under pause) | No leak growth (watch RSS), no validation errors |
| 6 | Broken/330 user shader on vulkan | Warning + `sharp` fallback (Open question 1 default) |
| 7 | GL regression pass | opengl backend: same content set, before/after PR captures identical |
| 8 | Resize + fullscreen + shader switch interleaved | Stable, letterbox correct |

## Exit checklist

- [ ] compile-commits green for all 6 commits
- [ ] Lavapipe job runs goldens + policy tests, green
- [ ] GL byte-identical capture evidence attached for commits 3–4
- [ ] Attribution: RetroArch pipeline-prior-art entry flipped
- [ ] Learning-doc chapters (topology extraction and the executor
      deserve long ones)
- [ ] `shader` help text updated; Open-question-1 default noted in
      the PR description for John's sign-off
