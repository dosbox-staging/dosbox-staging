---
description: Git commit rules
globs: "**/*"
---

# Commits

- All commits must compile individually (this is enforced)
- Prefixes: `build:`, `ci:`, `docs:`, `style:`, `website:`
- Do NOT use `fix:`, `feat:`, `refactor:`, `test:` prefixes
- Style-only changes in their own `style:` commits; okay to include in
  functional commits if affecting ≤10-20 lines
- Documentation content: `docs:` prefix
- Website layout/theme: `website:` prefix
- Formatting-only changes: separate commit
- Never add Co-Authored-By lines to commits
- Verify all commits compile: `scripts/tools/compile-commits.sh`
