# Repository Guidelines

## Project Structure & Module Organization
`src/` contains the emulator and platform code, split by subsystem (`audio/`, `cpu/`, `dos/`, `gui/`, `hardware/`, `midi/`, `network/`, `shell/`, `utils/`, `platform/`). `src/libs/` holds bundled third-party code. `tests/` contains the GoogleTest suite plus fixtures and test data in `tests/files/`. `resources/` stores runtime assets such as shaders, keyboard mappings, translations, and web assets. `docs/` contains contributor docs plus shared agent rules in `docs/agent-rules/`. `website/` contains the MkDocs site sources. Use `extras/` and `scripts/` for tooling and maintenance tasks, not core runtime code.

## Shared Agent Rules
Treat this file as the entry point, not the full policy set. The canonical shared rules live in `docs/agent-rules/`:

- `code-style.md`: C++ editing rules for `src/`
- `commits.md`: commit structure and prefix rules
- `docs-style.md`: website/docs verification and writing rules
- `mcp.md`: how agents should use the DOSBox MCP bridge

When work touches one of those areas, read the matching rule file before editing.

## Build, Test, and Development Commands
List available presets first with `cmake --list-presets`. Prefer the platform-specific presets documented in `docs/build-*.md`.

- `cmake --preset debug-linux` or `cmake --preset debug-windows`: configure a debug build.
- `cmake --build --preset debug-linux` or `cmake --build --preset debug-windows`: compile the selected preset.
- `ctest -j 8 --preset debug-linux`: run the full test suite for the matching build preset.
- `build/debug-linux/tests/dosbox_tests --gtest_filter=SuiteName.TestName`: run one GoogleTest case directly when you need detailed output.
- `scripts/tools/compile-commits.sh debug-linux --test`: verify each commit in a branch builds cleanly and passes tests.

## Coding Style & Naming Conventions
Formatting is enforced with `.clang-format`; use tabs for indentation and follow K&R-style braces. Run `clang-format -i path/to/file.cpp` on edited C/C++ files. `.clang-tidy` enforces naming: variables, parameters, and static functions use `lower_snake_case`; classes, structs, and enums use `UpperCamelCase`. For detailed C++ rules, use `docs/agent-rules/code-style.md` together with `docs/CONTRIBUTING.md`.

## Testing Guidelines
Unit tests are built by default through CMake and registered with `gtest_discover_tests()`. Add new tests only for confirmed contract behavior. Name test files `*_tests.cpp` and keep test data under `tests/files/` when needed. Run `ctest` against the same preset you built, and prefer targeted `-R` filters before full-suite reruns while iterating.

## Commit & Pull Request Guidelines
Recent history favors short, imperative subjects, with prefixes used selectively. Follow `docs/agent-rules/commits.md` and `docs/CONTRIBUTING.md`: keep commits small, focused, and individually buildable. For large reformats, make a separate `style:` commit first. PRs should include a concise description, related issues, manual test steps, OS coverage notes, and release notes for user-facing changes, matching `.github/pull_request_template.md`.
