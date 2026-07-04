# PR 4 — Metal backend (DELETED — tombstone)

This PR no longer exists and has no implementation work. The native
Metal backend was removed from the plan before implementation began:
macOS runs the same Vulkan backend on MoltenVK, with timed presents
through `VK_GOOGLE_display_timing` (plan decision 1, Appendix E,
sealed empirically by Spike 5 in Appendix D).

The PR number is retained as a tombstone so cross-references,
attribution entries, and the learning doc's history stay valid.

macOS-specific work that survives lands inside the Vulkan PRs:

- MoltenVK build/CI/dev-setup wiring and the loading ladder —
  [PR 2](native-gpu-backends-pr2.md), commits 2 and 4.
- The `VK_GOOGLE_display_timing` dialect —
  [PR 6](native-gpu-backends-pr6.md), commit 3.
- MoltenVK `.app` bundling for release —
  [PR 7](native-gpu-backends-pr7.md).

Do not implement anything from this file.
