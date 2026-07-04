# Native GPU backend — design document set

Everything about the native Vulkan render backend project lives in
this directory. Reading order depends on who you are:

**Implementing a PR?** Read, in order:

1. `native-gpu-backends-plan.md` — at minimum: Overview, Decision
   record, Architecture, the standing constraints under
   “Implementation order”, the Attribution policy (the **licence
   firewall** is a hard rule), and your PR's design section.
2. `native-gpu-backends-prN.md` — your PR's implementation
   contract: step-level instructions, guardrails, per-commit
   automated verification, and the human test matrix. This is what
   you follow. If reality disagrees with it, **stop and update the
   spec (and the plan) first** — never improvise around a mismatch.
3. The standing per-commit duties: a learning-doc chapter in
   `native-gpu-backends-learning.md` and attribution flips in
   `native-gpu-backends-attribution.md`, both in the same commit as
   the code they describe.

**Reviewing or catching up?** `native-gpu-backends-plan.md` top to
bottom; the appendices hold the full decision history (A–B), the
eight-emulator reference study (C), the spike results (D), and the
ecosystem review that deleted the Metal backend (E).

**Learning?** `native-gpu-backends-learning.md` is the textbook
companion — foundations, field notes from the reference study, the
spike sagas, and one chapter per landed commit.

## Files

- `native-gpu-backends-plan.md` — the plan: decisions, architecture,
  per-PR design sections, appendices. The single source of truth.
- `native-gpu-backends-pr1.md` … `pr7.md` — per-PR implementation
  contracts (pr4 is a tombstone: the Metal backend was deleted in
  favour of MoltenVK).
- `native-gpu-backends-learning.md` — the educational companion;
  grows one chapter per commit (standing rule).
- `native-gpu-backends-attribution.md` — public thanks and the
  provenance trail for every adopted idea; maintained
  commit-by-commit.
- `spikes/` — runnable design-phase experiments and their outputs
  (toolchain round-trip, library APIs, native-Metal and MoltenVK
  timed presents). Ours; adapt freely.
