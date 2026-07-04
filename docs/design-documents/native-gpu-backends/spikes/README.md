# Design-phase spikes

Runnable experiments that de-risked the native GPU backend plan (see
`../native-gpu-backends-plan.md`, Appendix D for full results and
interpretation). All were run on an Apple M4 Pro with a 120 Hz
ProMotion display, macOS 15.x era, 2026-07-04/05.

- `sharp450.glsl` — Spikes 1/2 subject: `interpolation/sharp.glsl`
  hand-converted to Vulkan GLSL 4.50 (the shader single-source
  format).
- `lib_spike.cpp` — Spike 2: glslang C++ API + SPIRV-Cross reflection;
  proves the UniformPacker std140 offsets (0/8/16/20/24).
  Build: `clang++ -std=c++17 -I/opt/homebrew/include -L/opt/homebrew/lib
  lib_spike.cpp -lglslang -lglslang-default-resource-limits
  -lspirv-cross-core`
- `metal_spike.mm` — Spike 4: native `presentDrawable:atTime:` at
  70.086 Hz with `addPresentedHandler` feedback. Build line in the
  file header. Output: `metal_spike_output.txt`.
- `vulkan_timing_spike.mm` — Spike 5: the same experiment through
  MoltenVK + `VK_GOOGLE_display_timing`; the results matched Spike 4
  and sealed the Metal-backend deletion. Build line in the file
  header. Output: `vulkan_timing_spike_output.txt`.

These are spikes, not production code: no error recovery beyond
what the measurements needed, deliberate leaks before `_exit()`.
