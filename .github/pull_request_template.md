# Description

_Describe a summary of your changes clearly and concisely, including motivation and context._
_Breaking changes need extra explanation on backward compatibility considerations._

_Feel free to include additional details, but please respect the reviewer's time and keep it brief._


## Related issues

_Related issues can be listed here (remove the section if not applicable.)_


# Release notes

_If this is a user-facing change that should be mentioned in the release notes, please provide a draft of the notes here._

_It doesn't have to be perfect; the person writing the release notes will expand this if needed._

_(Remove the section if no notes are needed.)_


# Manual testing

_Please describe the tests that you ran to verify your changes._

_Provide clear instructions so the reviewers can reproduce and verify your results._

_Include relevant details for your test configuration, operating system, etc._

_Ideally, you would also test your changes with a [sanitizer build](https://github.com/dosbox-staging/dosbox-staging/blob/main/docs/build-linux.md#make-a-sanitizer-build) and confirm no issues were found._

_Non-trivial changes **must be** tested on all three OSes we support. If you can't test on a particular OS, ask the team or contributors for help._

The change has been manually tested on:

- [ ] Windows
- [ ] macOS
- [ ] Linux


# Checklist

_Please tick the items as you have addressed them. Don't remove items; leave the ones that are not applicable unchecked._

I have:

- [ ] followed the project's [contributing guidelines](https://github.com/dosbox-staging/dosbox-staging/blob/master/docs/CONTRIBUTING.md) and [code of conduct](https://github.com/dosbox-staging/dosbox-staging/blob/master/docs/CODE_OF_CONDUCT.md).
- [ ] performed a self-review of my code.
- [ ] commented on the particularly hard-to-understand areas of my code.
- [ ] split my work into well-defined, bisectable commits, and I [named my commits well](https://github.com/dosbox-staging/dosbox-staging/blob/main/docs/CONTRIBUTING.md#commit-messages).
- [ ] applied the appropriate labels (bug, enhancement, refactoring, documentation, etc.)
- [ ] [checked](https://github.com/dosbox-staging/dosbox-staging/blob/main/scripts/compile_commits.sh) that all my commits can be built.
- [ ] my change has been manually tested on Windows, macOS, and Linux.
- [ ] confirmed that my code does not cause performance regressions (e.g., by running the Quake benchmark).
- [ ] added unit tests where applicable to prove the correctness of my code and to avoid future regressions.
- [ ] provided the release notes draft (for significant user-facing changes).
- [ ] made corresponding changes to the documentation or the website according to the [documentation guidelines](https://github.com/dosbox-staging/dosbox-staging/blob/main/DOCUMENTATION.md).
- [ ] [locally verified](https://github.com/dosbox-staging/dosbox-staging/blob/main/DOCUMENTATION.md#previewing-documentation-changes-locally) my website or documentation changes.

