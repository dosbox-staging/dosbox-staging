---
description: Rules for verifying and writing mkdocs documentation
globs: website/**/*.md, website/**/*.yml, website/**/*.html
---

# After any docs/website change, run both before presenting results:

1. `cd website && mkdocs build` -- zero warnings required
2. `cd website && ../scripts/linting/verify-markdown.sh` -- zero warnings required

# Writing style

- Use British spelling (colour, centre, favourite, analyse, etc.)

- Write for regular users, not developers — assume no MS-DOS or retro PC experience

- Before adding new formatting or markup patterns, find an existing example in
  the docs and match it

- Most manual pages are divided into two parts: a conversational guide, and a
  reference section (under "Configuration settings")

- We document configuration sections topically; it's okay if settings from the
  same config section are documented in different chapters

- The "Configuration settings" section must copy the setting's description
  from the code verbatim; that's the baseline, then we can enhance it with
  additional info if needed

- Look for opportunities to create cross-references across chapters

- Look for opportunities to include interesting trivia, e.g. games that use a
  given feature (use web search for research).
  
- All referenced games must link to the game's page on Mobygames

# If editing SASS

- Regenerate CSS from `extra-scss/extra.scss` and commit both `.scss` and `.css`

# Version bumps

Update ALL of these together:
- Rename `website/docs/<old>/` directories
- `website/docs/versions.json`
- `mkdocs.yml`: nav paths, redirect targets, exclude globs
- `hooks/offline.py`: `DUMMY_INDEX_PAGES`
