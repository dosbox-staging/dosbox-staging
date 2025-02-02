# Contributing to DOSBox Staging

Thank you for your interest in contributing to DOSBox Staging! There are many
ways to participate, and we appreciate all of them. This document is a bit long,
so here are links to the major sections:

- [1. Feature requests and bug reports](#feature-requests-and-bug-reports)
- [2. Find something to work on](#find-something-to-work-on)
- [3. Contributing code](#contributing-code)
  - [3.1. Coding style](#coding-style)
    - [3.1.1. Language](#language)
    - [3.1.2. Code formatting](#code-formatting)
    - [3.1.3. Additional style rules](#additional-style-rules)
    - [3.1.4. Code style example](#code-style-example)
  - [3.2. Submitting patches and pull requests](#submitting-patches-and-pull-requests)
    - [3.2.1. Commit messages](#commit-messages)
    - [3.2.2. Commit messages for patches authored by someone else](#commit-messages-for-patches-authored-by-someone-else)
- [4. Tools](#tools)
  - [4.1. Using clang-format](#using-clang-format)
  - [4.2. Summarise warnings](#summarize-warnings)

## Feature requests and bug reports

If you find a [feature request][enhancement_label] you're interested in,
leave a comment or an emote—it will help us decide where to focus our
development effort.

Report bugs via our [bugtracker][issues]. Issues and requests reported
via comments on social media or private messages are very likely going to be
lost.

[enhancement_label]: https://github.com/dosbox-staging/dosbox-staging/issues?q=is%3Aissue+is%3Aopen+label%3Aenhancement
[issues]: https://github.com/dosbox-staging/dosbox-staging/issues

## Find something to work on

There are plenty of tasks to work on all around. Here are some ideas:

- Test DOSBox Staging with your DOS games.
- Package DOSBox Staging for your OS.
- Improve our documentation (in the repo, or in the
  [project wiki](https://github.com/dosbox-staging/dosbox-staging/wiki)).
- Promote the project in gaming communities 😎
- Provide translations.
- Look at warnings in the code (build with `-Wall -Weffc++` to see the long list
  of potential code improvements), and try to eliminate them.
- Look through our static analysis reports, pick an issue, investigate if the
  problem is true-positive or false-positive and if the code can be improved.
  See artifacts and reports in: [Clang][clanga], [PVS][pvs], [LGTM].
- Look at our groomed list of features we want to be implemented in the
  [Backlog](https://github.com/dosbox-staging/dosbox-staging/projects/3)—if
  the issue is not assigned to anyone, then it's for the pickings! Leave a
  comment saying that you're working on it.
- Find the next release project in our
  [projects list](https://github.com/dosbox-staging/dosbox-staging/projects)
  and pick one of the tasks in the "To do" column.
- Peruse the list of
  [open bug reports](https://github.com/dosbox-staging/dosbox-staging/issues?q=is%3Aissue+is%3Aopen+label%3Abug)
  and try to reproduce or fix them.
- Check out the list of reports tagged with ["good first issue"](https://github.com/dosbox-staging/dosbox-staging/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22)—these are suitable for people getting familiar with the project.

Or just send us a Pull Request with your improvement
(but please read ["Submitting patches"](#submitting-patches-pull-requests) first).

If you plan to work on a new, bigger feature, then it might be a good idea to
discuss it with us early, e.g., by creating a new issue ticket.

[lgtm]:     https://lgtm.com/projects/g/dosbox-staging/dosbox-staging/
[clanga]:   https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22Code+analysis%22
[pvs]:      https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22PVS-Studio+analysis%22

## Contributing code

### Coding style

These rules apply to code in `src/` and `include/` directories.
They do not apply to code in `src/libs/` directory (libraries in there
have their own coding conventions).

The rules outlined below apply to new code landing in the main branch.
Do not do mass reformatting or renaming of existing code.

#### Language

We use C-like C++20. To clarify:

- Avoid designing your code in a complex object-oriented style.
  This does not mean "don't use classes", it means "don't use stuff like
  multiple inheritance, overblown class hierarchies, operator overloading,
  iostreams for stdout/stderr, etc".
- Don't micro-optimise and avoid writing "clever code". Always optimise for clarity of intent,
  maintainability, and readability in the first round, and only deviate from that if
  warranted due to performance considerations backed by actual real-world measurements.
- C++20 has a rich STL library, use it (responsibly—sometimes using
  C standard library makes more sense).
- Use modern C++ features like `constexpr`, `static_assert`, managed pointers,
  lambda expressions, for-each and range-expression loops, `std::array`
  instead of fixed C arrays, etc.
- Avoid using exceptions. C++ exceptions are trickier than you think.
  No, you won't get it right. Or the person touching the code after you won't get
  it right. Or the person using your exception-ridden interface won't get it right. 😅
  Let errors like `std::logic_error` or `std::bad_alloc` terminate the
  process, so it's easier to notice during testing and can be fixed early.
- Avoid complex template metaprogramming. Simple templates are fine.
- Avoid complex macros. If possible, write a `constexpr` function or a simple
  template instead.
- Never write `using namespace std;`. We don't want any confusion about what
  comes from STL and what's project-specific.
- Using namespaces is fine for grouping strings or constants but should be used sparingly.
- Before using some platform-specific API, check if SDL2 provides
  a cross-platform interface for it. It probably does.

#### Code formatting

For new code follow K&R style—see [Linux coding style] for examples and some
advice on good C coding style.

Following all the details of formatting style is tedious, that's why we use
a custom clang-format ruleset to make it crystal clear. Skip to
["Using clang-format"](#using-clang-format) section to learn how.

[Linux coding style]:https://www.kernel.org/doc/html/latest/process/coding-style.html

#### Additional style rules

1. Use `auto` as much as you can.
2. We try hard to be `const`-correct as much as possible.
3. Keep the `const`s in sync in the function/method declarations and definitions.
4. Prefer to use enum classes.
5. Always initialise all variables and struct members.
6. Struct-nesting is encouraged as logical groupings improve readability.
7. **NEVER** use Hungarian notation!
8. But always include unit of measurement suffixes (e.g., `delay_ms` instead
9. Prefer using `intXX_t`/`uintXX_t` types and `size_t` instead of plain `int`.
10. Most old code uses the naming convention `MODULENAME_FunctionName` for public
   module interfaces. Do **NOT** replace it with namespaces.
11. Generally, we only use `UpperCamelCase` and `lower_snake_case`. Function
arguments, variable names, struct members, and static functions are
`lower_snake_case`—everything else is `UpperCamelCase`. See the code example
below for concrete examples.
12. Don't uppercase acronyms, so use `PngWriter` instead of `PNGWriter`.
13. Prefer to us pre-increment/decrement whenever possible, so `++i` and `--i`.
14. Sort includes according to [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html#Names_and_Order_of_Includes).
of just `delay`, `cutoff_freq_hz` instead of just `cutoff_freq`, etc.).
15. Enable narrowing checks in new code and when you rework a file by adding
`CHECK_NARROWING()` and including `"checks.h"` (just search for
`CHECK_NARROWING())`.
16. Use header guards in the format: `DOSBOX_HEADERNAME_H` or `DOSBOX_MODULENAME_HEADERNAME_H`.
17. Use end-of-line comments very sparingly and use `//` for block comments.
18. Surround debug logging with `#if 0` & `#endif` pairs instead of commenting the log statements out, or better yet, introduce define switches per topic (e.g., `#define DEBUG_VGA_DRAW`).

#### Code style example

```cpp
enum class CaptureState { Off, Pending, InProgress };

// Static function
static int32_t get_next_capture_index(const CaptureType type);

// Public function prefixed by `MODULENAME_`
void CAPTURE_AddFrame(const RenderedImage& image, const float frames_per_second);

// Static inline struct; always initialise members
static struct {
    std_fs::path path     = {};
    bool path_initialised = false;

    struct {
        CaptureState audio = {};
        CaptureState midi  = {};
        CaptureState video = {};
    } state = {};
} capture = {};

// Public struct; always initialise members
struct State {
    CaptureState raw      = {};
    CaptureState upscaled = {};
    CaptureState rendered = {};
    CaptureState grouped  = {};
};

// Class definition
class PngWriter {
public:
    PngWriter() = default;
    ~PngWriter();

    // Use const args whenever possible
    bool InitRgb888(FILE* fp, const uint16_t width, const uint16_t height,
                    const Fraction& pixel_aspect_ratio,
                    const VideoMode& video_mode);

    void WriteRow(std::vector<uint8_t>::const_iterator row);

    // Mark methods as const whenever possible
    bool IsValid() const;

private:
    // Always initialise members
    State state = {};

    std_fs::path rendered_path = {};
}
```

### Submitting patches and pull requests

Submissions via GitHub PRs are recommended. If you can't use GitHub for
whatever reason, then submitting patches (generated with `git-format-patch`)
via email is also possible—contact a maintainer about the details.

Code submitted as raw source files (not patches), files attached to issue
comments, forum posts, diffs in non-git format, etc., will be promptly ignored.

#### Commit messages

Read [How to Write a Git Commit Message]. Then read it again, and follow
"the seven rules" 😎

The only exception to these rules are commits landing from our upstream project,
so occasionally you might find a commit originating from the `svn/trunk` branch
that does not follow them.  There are no other exceptions.

[How to Write a Git Commit Message]:https://chris.beams.io/posts/git-commit/

#### Commit messages for patches authored by someone else

- If possible, preserve code formatting used by the original author.
- Record the correct author name, date when the original author wrote the patch
  (if known), and sign it, e.g:

  ```
  $ git commit --amend --author="Original Author <mail-or-identifier>"
  $ git commit --amend --date="15-05-2003 11:45"
  $ git commit --amend --signoff
  ```

- Record the source of the patch so future programmers can find the context
  and discussion surrounding it. Use the following trailer:

  ```
  Imported-from: <url-or-specific-identifier>
  ```

For an example of a commit that followed all of these rules, see commit [ffe3c5ab](https://github.com/dosbox-staging/dosbox-staging/commit/ffe3c5ab7fb5e28bae78f07ea987904f391a7cf8):

    $ git log -1 ffe3c5ab7fb5e28bae78f07ea987904f391a7cf8

## Tools

### Using clang-format

It's usually distributed with the Clang compiler and can be integrated with
[many programming environments](https://releases.llvm.org/10.0.0/tools/clang/docs/ClangFormat.html).

Outside of your editor, code can be formatted by invoking the tool directly:

    $ clang-format -i file.cpp

But it's better not to re-format the whole file at once—you can target
specific line ranges (run `clang-format --help` to learn how), or use our
helper script to format C/C++ code touched by your latest commit:

    $ git commit -m "Edit some C++ code"
    $ ./scripts/format-commit.sh

Run `./scripts/format-commit.sh --help` to learn about available options.


#### Vim integration

Download `clang-format.py` file somewhere, and make it executable:

    $ curl "https://raw.githubusercontent.com/llvm/llvm-project/main/clang/tools/clang-format/clang-format.py" > ~/.vim/clang-format.py
    $ chmod +x ~/.vim/clang-format.py

Then add the following lines to your `.vimrc` file:

    " Integrate clang-format tool
    map <C-K> :py3f ~/.vim/clang-format.py<cr>
    imap <C-K> <c-o>:py3f ~/.vim/clang-format.py<cr>

Read the documentation inside `clang-format.py` file in case your OS is missing Python 3 support.

#### MSVC integration

[ClangFormat extension on VisualStudio Marketplace](https://marketplace.visualstudio.com/items?itemName=LLVMExtensions.ClangFormat)

### Summarise warnings

Out gating mechanism tracks the number of warnings per OS/compiler.
To see a summary of warnings in your build, do a clean build and then
use the script `./scripts/count-warnings.py`:

    make -j "$(nproc)" |& tee build.log
    ./scripts/count-warnings.py build.log

Run `./scripts/count-warnings.py --help` to learn about available options.
