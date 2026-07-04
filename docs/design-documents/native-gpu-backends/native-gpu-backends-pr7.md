# PR 7 — `auto` + polish (execution spec)

Part of the native Vulkan render backend series. Rationale in
`native-gpu-backends-plan.md`: decision 3 (`auto` resolution order),
decision 7 (presentation_mode auto is test-then-decide), decision 8
(vsync/VRR semantics for the guide), and the standing
architecture-documentation constraint in “Implementation order”.
**If reality disagrees with this spec, stop and update the spec
first.**

## Goal

The closing PR: the `auto` output value with its fallback chain, the
deferred presentation-mode decision resolved by measurement, MoltenVK
bundled into the macOS release artifact, and the documentation
sweep — the multi-part architecture docs under `src/gui/render/docs/`
finalised, the user-facing VRR guide, and release notes.

## Prerequisites

- PRs 1–3, 5, 6 merged. The pacing instrumentation from PR 6 is the
  measuring instrument for commit 2's decision.

## Success criteria (PR level)

1. `output = auto` resolves vulkan → opengl → texture on every
   platform, logs the chosen backend clearly, and survives a
   no-Vulkan machine (forced-failure probe).
2. The shipped default `output` value is UNCHANGED in this release
   (flipping to `auto` is a separate release-management decision —
   the deliberately-open question the series leaves behind).
3. The presentation_mode-auto decision is made from recorded
   measurements (not vibes) and implemented; the numbers are in the
   PR.
4. macOS release artifacts run `output = vulkan` out of the box on a
   clean machine (pinned loader + MoltenVK bundled per decision 16;
   no Homebrew required).
5. `src/gui/render/docs/` exists per the standing constraint:
   architecture.md, shaders.md, vulkan.md, opengl.md, sdl.md, with
   rendered diagram images AND their editable sources; README.md is
   a short index. Docs build + markdown lint clean.
6. Every commit standalone-green.

## Commits

### Commit 1 — `Add the auto output mode`

**Files:** `src/gui/sdl_gui.cpp`, `src/gui/render/render.cpp` (help
text only if it references output values).

**Steps:**

1. `configure_renderer()` (sdl_gui.cpp:241-265): `"auto"` maps to a
   new `RenderBackendType::Auto` (or an `is_auto` flag — pick
   whichever keeps `create_renderer()` simplest; the enum value is
   cleaner given the existing switch shape).
2. `create_renderer()` (sdl_gui.cpp:1526-1576): auto tries Vulkan
   first on every platform (macOS lands on MoltenVK), then OpenGL,
   then texture — reusing the per-backend construction/fallback
   code the chain already has; the difference between `auto` and an
   explicit `vulkan` is only the log tone (auto: informational
   "selected"; explicit: warning "falied, falling back"). Write the
   effective backend back to the runtime config the same way the
   existing fallbacks do.
3. `output` setting: add `auto` to the values + help text
   (unconditionally — auto is valid even in a build with
   OPT_VULKAN=OFF; it just starts its chain at opengl there). The
   default stays as-is (success criterion 2).
4. Log line, exactly one, at selection:
   `DISPLAY: Output backend 'vulkan' selected (output = auto)`.

**Automated verification:** boot probe `output = auto` → grep the
selection line; forced-failure probe (`VK_DRIVER_FILES=/dev/null` on
loader platforms) → auto lands on opengl without warnings-as-errors;
build with `-DOPT_VULKAN=OFF` → auto lands on opengl; full ctest.

### Commit 2 — `Decide presentation_mode auto on native backends`

The decision-7 experiment, run and then implemented.

**Protocol (record everything in the learning doc):**

- Testbed: the VRR panel pinned to 60 Hz (fixed-rate case — the
  case host-rate exists for) plus native VRR (the case dos-rate is
  obviously right for).
- Content: 70.086 Hz smooth scroller (Keen-class) + one 60 Hz VESA
  title.
- Arms: `presentation_mode = dos-rate` vs `host-rate`, vulkan
  backend, fullscreen, vsync on and off.
- Metrics: PR 6 PACING report (interval regularity of what reaches
  glass, skip/drop counts and their pattern regularity) + eyeball
  judgement of the drop cadence (a regular 7-of-8 beat vs irregular
  stutter).
- Decision rule: if dos-rate's drop-at-acquire pattern on fixed
  60 Hz is subjectively no worse than host-rate's resampling AND
  the metrics don't contradict, auto maps to dos-rate always on
  scheduled-present backends (host-rate stays selectable); if it is
  worse, keep today's vsync-derived mapping and record why.
- Implement the winner in `setup_presentation_mode`
  (sdl_gui.cpp:489-532) gated on
  `backend->SupportsScheduledPresents()`; leave the non-native
  mapping untouched either way.
- If the data supports it, shrink the host-rate early-present window
  (the 3 ms GL-blocking-swap workaround, :527) toward zero on the
  vulkan backend — measure with the armed→submit latency stat
  before and after.

**Automated verification:** ctest + boot probes in both modes on
vulkan and opengl (mapping still resolves; no mode is
unreachable).

**Manual verification:** the protocol itself is the verification;
paste the comparison table into the PR.

### Commit 3 — `build: bundle the Vulkan loader and MoltenVK into the macOS app`

Assembles the decision-16 vendored stack into the release `.app` —
release artifacts depend on nothing outside the bundle (LunarG's
recommended posture for Apple platforms: ship well-tested pinned
versions of both the loader and MoltenVK; no system-wide loader).

**Steps:**

1. In the macOS packaging step (macos.yml, the universal-build job),
   place into the .app:
   - `Contents/Frameworks/libvulkan.1.dylib` — the vcpkg-built
     loader; vcpkg builds are per-arch, so `lipo -create` the two
     arch builds in the universal job, mirroring how the main
     binary is merged;
   - `Contents/Frameworks/libMoltenVK.dylib` — from the PR 2
     vendored source build (`make macos` emits a universal dylib,
     no lipo needed; take it from either arch runner's build);
   - `Contents/Resources/vulkan/icd.d/MoltenVK_icd.json` — with its
     `library_path` pointing at the bundled dylib
     (`../../../Frameworks/libMoltenVK.dylib`); the loader searches
     the bundle's `Resources/vulkan/icd.d` on macOS.
2. Wire the PR 2 loading ladder's bundle branch:
   `SDL_Vulkan_LoadLibrary()` gets the explicit
   `Contents/Frameworks/libvulkan.1.dylib` path when running from a
   bundle. Confirm by log line on a clean-Mac run — the ladder logs
   which path loaded.
3. Licences: MoltenVK and the Vulkan loader are both Apache-2.0 —
   add both licence texts to the bundle's licence set via the
   existing mechanism (`cmake/add_copy_assets.cmake` copies
   `licenses/`).
4. Sign/notarise: both dylibs signed with the same identity as the
   app — add them to the codesign invocation's inputs (follow how
   other bundled dylibs from vcpkg are handled in the same job; the
   job already injects external deps).

**Automated verification:** CI artifact from the PR run, installed
on a Mac with NO Homebrew Vulkan anything (or brew's copies renamed
away): `output = vulkan` boots and logs the bundled-loader path;
`output = auto` selects vulkan; `codesign --verify --deep` passes;
`vulkaninfo` is NOT required to exist on the machine.

### Commit 4 — `docs: native backends, VRR guide, and release notes`

**Files:** website docs (user-facing), `src/gui/render/docs/*`
(developer-facing), `src/gui/render/README.md`, release notes
snippet, learning-doc closing chapter.

**User-facing (obeys `.claude/rules/docs-style.md`: British
spelling, verbatim setting descriptions as the baseline, Mobygames
links for referenced games, cross-references):**

1. Setting documentation for `output = vulkan` / `auto` in the
   manual's reference section (copy the setting's help text from
   the code verbatim, then enhance).
2. The VRR guide chapter: recommended configurations per display
   class; the decision-8 driver-forced-vsync caveat (desktop rate
   must exceed the DOS rate); the NVIDIA control-panel
   “Vulkan/OpenGL present method → DXGI swapchain” note for
   windowed VRR on Windows; the Windows borderless-fullscreen
   ceiling (IMMEDIATE presents may be GDI-copied rather than
   independently flipped — no tearing/VRR on that path; Xenia's
   finding, plan Appendix C §12, credited — flip the attribution
   entry with this commit); the compositor ceiling stated honestly
   (first-class pacing is a fullscreen feature — cite the measured
   Spike 4/5 behaviour); a known-limitations note that Linux pacing
   verification is in the core team's hands.
3. Release-notes snippet: the flagship framing (first emulator to
   ship Vulkan timestamped presents — keep the "to the best of our
   knowledge" qualifier from the plan), the new output values, the
   shader single-source change and what it means for custom-shader
   authors (link the porting guide from PR 5).

**Developer-facing (`src/gui/render/docs/`, the standing
constraint):**

1. `architecture.md`: the living version of the plan's target-state
   listing, diagram, and relations — port them, then make them true
   (the listing was maintained per-PR; reconcile any drift).
2. `shaders.md` exists since PR 5 — final pass.
3. `vulkan.md` (swapchain lifecycle + sync ring + present-timing
   tiers + MoltenVK specifics + quirk log), `opengl.md`, `sdl.md`.
4. **Diagrams as rendered images with editable sources committed
   next to them** (`docs/img/`). Tool: pick one that renders
   deterministically from text sources in-repo (Mermaid via `mmdc`,
   Graphviz, or D2 — whichever the implementing session can run;
   the requirement is regenerability, so commit the source AND a
   `make`-style regeneration note in the img/ README, and treat the
   plan's ASCII diagram as the content spec).
5. `README.md` becomes a ten-line index into `docs/`.

**Automated verification:** `cd website && mkdocs build` zero
warnings; `scripts/linting/verify-markdown.sh` zero warnings;
diagram regeneration command runs clean from a fresh checkout;
learning-doc closing chapter present.

## Manual test matrix (humans, before merge)

| # | Scenario | Pass when |
| --- | -------- | --------- |
| 1 | `output = auto` on every hardware box in the inventory | Correct backend selected + logged on each |
| 2 | Auto on the no-Vulkan case (driver-less VM or env-forced) | Silent, sensible opengl fallback |
| 3 | Presentation-mode comparison protocol (commit 2) | Table completed; decision implemented matches it |
| 4 | macOS artifact on a clean Mac (no Homebrew) | vulkan boots out of the box; codesign/notarisation pass |
| 5 | VRR guide walkthrough | A team member follows the guide cold on their hardware and gets the promised behaviour |
| 6 | Docs render | Website chapter + render/docs images render correctly |
| 7 | Full regression sweep (the plan's Verification manual matrix, one last full pass on all backends) | No regressions vs the per-PR runs |

## Exit checklist

- [ ] compile-commits green for all 4 commits
- [ ] Presentation-mode decision table in the PR; plan's decision 7
      marked resolved (edit the plan in the same commit)
- [ ] macOS bundle verified Homebrew-free
- [ ] `src/gui/render/docs/` complete: 5 parts + img/ sources +
      README index
- [ ] Website build + markdown lint clean
- [ ] Learning doc closing chapter; attribution file final pass
      (every non-tombstone entry either landed or explicitly
      dropped-with-reason — including Xenia's borderless-GDI entry,
      flipped by commit 4)
- [ ] The one deliberately open question — flipping the shipped
      default to `auto` — recorded in the release notes draft as
      future work
