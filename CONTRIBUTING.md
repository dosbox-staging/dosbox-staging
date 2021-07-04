# Contributing to dosbox-staging

Thank you for your interest in contributing to dosbox-staging! There are many
ways to participate, and we appreciate all of them. This document is a bit long,
so here are links to the major sections:

- [1. Feature requests and bug reports](#feature-requests-and-bug-reports)
- [2. Find something to work on](#find-something-to-work-on)
- [3. Contributing code](#contributing-code)
  - [3.1. Coding style](#coding-style)
    - [3.1.1. Language](#language)
    - [3.1.2. Code Formatting](#code-formatting)
    - [3.1.3. Additional Style Rules](#additional-style-rules)
  - [3.2. Submitting patches / Pull Requests](#submitting-patches--pull-requests)
    - [3.2.1. Commit messages](#commit-messages)
    - [3.2.2. Commit messages for patches authored by someone else](#commit-messages-for-patches-authored-by-someone-else)
- [4. Tools](#tools)
  - [4.1. Using clang-format](#using-clang-format)
  - [4.2. Summarize warnings](#summarize-warnings)

# Feature requests and bug reports

If you find a [feature request][enhancement_label], you're interested in,
leave a comment, or an emote - it will help us to decide where to focus our
development effort.

Report bugs via our [bugtracker][issues]. Issues and requests reported
via comments on social media or private messages are very likely going to be
lost.

[enhancement_label]: https://github.com/dosbox-staging/dosbox-staging/issues?q=is%3Aissue+is%3Aopen+label%3Aenhancement
[issues]: https://github.com/dosbox-staging/dosbox-staging/issues

# Find something to work on

There's plenty of tasks to work on all around, here are some ideas:

- Test with your DOS games.
- Package dosbox-staging for your OS.
- Improve documentation (in the repo, or in the
  [project wiki](https://github.com/dosbox-staging/dosbox-staging/wiki)).
- Promote the project in gaming communities :)
- Provide translations (!)
- Look at warning in code (build with `-Wall -Weffc++` to see the long list
  of potential code improvements), and try to eliminate them.
- Look through our static analysis reports, pick an issue, investigate if the
  problem is true-positive or false-positive and if the code can be improved.
  See artifacts/reports in: [Clang][clanga], [Coverity][coverity],
  [PVS][pvs], [LGTM].
- Look at our groomed list of features we want implemented in the
  [Backlog](https://github.com/dosbox-staging/dosbox-staging/projects/3) - if
  the issue is not assigned to anyone, then it's for the pickings! Leave a
  comment saying that you're working on it.
- Find next release project in our
  [projects list ](https://github.com/dosbox-staging/dosbox-staging/projects)
  and pick one of tasks in "To do" column.
- Peruse through the list of
  [open bug reports](https://github.com/dosbox-staging/dosbox-staging/issues?q=is%3Aissue+is%3Aopen+label%3Abug)
  and try to reproduce or fix them.

Or just send us a Pull Request with your improvement
(read: ["Submitting patches"](#submitting-patches-pull-requests)).

If you plan to work on a new, bigger feature - then it might be good idea to
discuss it with us early, e.g. by creating a new bugtracker issue.

[coverity]: https://scan.coverity.com/projects/dosbox-staging/
[lgtm]:     https://lgtm.com/projects/g/dosbox-staging/dosbox-staging/
[clanga]:   https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22Code+analysis%22
[pvs]:      https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22PVS-Studio+analysis%22

# Contributing code

## Coding style

These rules apply to code in `src/` and `include/` directories.
They do not apply to code in `src/libs/` directory (libraries in there
have their own coding conventions).

Rules outlined below apply to new code landing in master branch.
Do not do mass reformatting or renaming of existing code.

### Language

We use C-like C++17. To clarify:

- Avoid designing your code in complex object-oriented style.
  This does not mean "don't use classes", it means "don't use stuff like
  multiple inheritance, overblown class hierarchies, operator overloading,
  iostreams for stdout/stderr, etc, etc".
- C++17 has rich STL library, use it (responsibly - sometimes using
  C standard library makes more sense).
- Use modern C++ features like `constexpr`, `static_assert`, managed pointers,
  lambda expressions, for-each loops, etc.
- Avoid using exceptions. C++ exceptions are trickier than you think.
  No, you won't get it right. Or person touching the code after you won't get
  it right. Or person using your exception-ridden interface won't get it right.
  Let errors like `std::logic_error` or `std::bad_alloc` terminate the
  process, so it's easier to notice during testing and can be fixed early.
- Avoid complex template metaprogramming. Simple templates are fine.
- Avoid complex macros. If possible, write a `constexpr` function or simple
  template instead.
- Never write `using namespace std;`. We don't want any confusion about what
  comes from STL and what's project-specific.
- Before using some platform-specific API - check if SDL2 provides
  cross-platform interface for it. It probably does.

### Code Formatting

For new code follow K&R style - see [Linux coding style] for examples and some
advice on good C coding style.

Following all the details of formatting style is tedious, that's why we use
custom clang-format ruleset to make it crystal clear, skip to
["Using clang-format"](#using-clang-format) section to learn how.

[Linux coding style]:https://www.kernel.org/doc/html/latest/process/coding-style.html

### Additional Style Rules

1. Sort includes according to: [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html#Names_and_Order_of_Includes)
2. NEVER use Hungarian notation.
3. Most old code uses naming convention `MODULENAME_FunctionName` for public
   module interfaces. Do NOT replace it with namespaces.
4. Utility function names and helper functions (generic functionalities, not
   tied to any specific emulation area, such as functions in
   `include/support.h`) do not need to follow this convention.
   Using `snake_case` names for such functions is recommended.
5. Do NOT use that convention for static functions (single file scope), class
   names, methods, enums, or other constructs.
6. Class names and method names can use `CamelCase` or `snake_case`.
   Be consistent, DON'T mix styles like this: `get_DefaultValue_Str` (bleh).
7. Using `snake_case` for variable names, parameter names, struct fields, and
   class fields is recommended.
8. Use header guards in format: `DOSBOX_HEADERNAME_H` or
   `DOSBOX_MODULENAME_HEADERNAME_H`.

## Submitting Patches / Pull Requests

Submissions via GitHub PRs are recommended. If you can't use GitHub for
whatever reason, then submitting patches (generated with `git-format-patch`)
via email is also possible - contact maintainer about the details.

Code submitted as raw source files (not patches), files attached to issue
comments, forum posts, diffs in non-git format, etc will be promptly ignored.

### Commit messages

Read [How to Write a Git Commit Message]. Then read it again, and follow
"the seven rules" :)

The only exception to these rules are commits landing from our upstream project,
so occasionally you might find a commit originating from `svn/trunk` branch,
that does not follow them.  There are no other exceptions.

[How to Write a Git Commit Message]:https://chris.beams.io/posts/git-commit/

### Commit messages for patches authored by someone else

- If possible, preserve code formatting used by original author
- Record the correct author name, date when original author wrote the patch
  (if known), and sign it, e.g:

  ```
  $ git commit --amend --author="Original Author <mail-or-identifier>"
  $ git commit --amend --date="15-05-2003 11:45"
  $ git commit --amend --signoff
  ```

- Record the source of the patch so future programmers can find the context
  and discussion surrounding it. Use following trailer:

  ```
  Imported-from: <url-or-specific-identifier>
  ```

For an example of commit, that followed all of these rules, see:

    $ git log -1 ffe3c5ab7fb5e28bae78f07ea987904f391a7cf8

# Tools

## Using clang-format

It's usually distributed with the Clang compiler, and can be integrated with
[many programming environments](https://releases.llvm.org/10.0.0/tools/clang/docs/ClangFormat.html).

Outside of your editor, code can be formatted by invoking the tool directly:

    $ clang-format -i file.cpp

But it's better not to re-format the whole file at once - you can target
specific line ranges (run `clang-format --help` to learn how), or use our
helper script to format C/C++ code touched by your latest commit:

    $ git commit -m "Edit some C++ code"
    $ ./scripts/format-commit.sh

Run `./scripts/format-commit.sh --help` to learn about available options.



### Vim integration

Download `clang-format.py` file somewhere, and make it executable:

    $ curl "https://raw.githubusercontent.com/llvm/llvm-project/main/clang/tools/clang-format/clang-format.py" > ~/.vim/clang-format.py
    $ chmod +x ~/.vim/clang-format.py

Then add following lines to your `.vimrc` file:

    " Integrate clang-format tool
    map <C-K> :py3f ~/.vim/clang-format.py<cr>
    imap <C-K> <c-o>:py3f ~/.vim/clang-format.py<cr>

Read documentation inside `clang-format.py` file in case your OS is missing
python3 support.

### MSVC integration

[ClangFormat extension on VisualStudio Marketplace](https://marketplace.visualstudio.com/items?itemName=LLVMExtensions.ClangFormat)

## Summarize warnings

Out gating mechanism tracks the number of warnings per OS/compiler.
To see a summary of warnings in your build, do a clean build and then
use script `./scripts/count-warnings.py`:

    make -j "$(nproc)" |& tee build.log
    ./scripts/count-warnings.py build.log

Run `./scripts/count-warnings.py --help` to learn about available options.
