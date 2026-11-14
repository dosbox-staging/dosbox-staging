---
description: C++ code style rules
globs: src/**/*.{cpp,h}
---

# General principles

- Simplest, most minimal solution wins; don't overengineer or future-proof
- Prefer composition over inheritance; prefer immutability
- Small, focused, pure functions; single responsibility per component
- Look for existing patterns in the codebase first — don't reinvent the wheel
- Self-documenting code; no restating the obvious in comments
- Only add abstractions when justified by current needs
- Prefer loose coupling and high cohesion
- Keep testability in mind
- Clarity and maintainability first; only optimise with measurements
- When in doubt, underengineer

# Scope

These rules apply to `src/` only. Vendored code in `src/libs/` follows its own
conventions — do not reformat it.

# After any code change, verify:

1. `clang-format -i <changed files>` or `./scripts/tools/format-commit.sh`

# Language

- C++23; use the standard library over C-style alternatives
- Never use C++ iostreams
- Never use C string functions — only `std::string` and `std::string_view`
- Never write `using namespace std;`
- Avoid exceptions except for signalling RAII construction failures
- Avoid complex template metaprogramming and complex macros
- Use RAII and smart pointers for resource management
- Prefer `constexpr`, `static_assert`, lambdas, range-based loops
- Prefer pre-increment/decrement (`++i`, `--i`)
- Use namespaces for grouping, not name prefixes
- Restrict SDL to rendering, window management, OpenGL, input, and OS-level
  audio/MIDI — check if C++ stdlib or SDL already provides what you need
  before adding custom code

# Naming

- `UpperCamelCase` for types, free functions, methods, enums, constants
- `lower_snake_case` for variables, arguments, struct members, static functions
- Don't uppercase acronyms: `PngWriter`, not `PNGWriter`
- No Hungarian notation
- Always include unit suffixes: `delay_ms`, `cutoff_freq_hz`
- Old code uses `MODULENAME_FunctionName` for public interfaces — don't replace
  these with namespaces

# Types

- Prefer `auto`
- Prefer plain `int` for numbers; don't micro-optimise integer sizes
- Use `int64_t` for large numbers rather than `uint32_t`
- Use `intXX_t`/`uintXX_t` only for binary protocols; `size_t` only for stdlib interfaces
- Prefer enum classes
- Prefer `std::optional` over sentinel values or out-parameters
- Default to `std::vector`; use `std::unordered_map` over `std::map` unless ordering is needed
- Always initialise variables and struct members
- Maintain `const`-correctness throughout

# Includes & headers

- Sort includes per Google C++ Style Guide
- Header guards: `DOSBOX_HEADERNAME_H` or `DOSBOX_MODULENAME_HEADERNAME_H`
- New code: add `CHECK_NARROWING()` and include `"util/checks.h"`

# Comments

- Use `//` for block comments
- End-of-line comments only for tabular data
- Debug logging: wrap with `#if 0` / `#endif` or per-topic define switches
