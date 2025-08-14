# Contributing to DOSBox Staging

Thank you for your interest in contributing to DOSBox Staging! There are many
ways to participate, and we appreciate all of them.

- [Feature requests and bug reports](#feature-requests-and-bug-reports)
- [Find something to work on](#find-something-to-work-on)
- [Contributing code](#contributing-code)
  - [Language standard](#language-standard)
  - [Code formatting](#code-formatting)
  - [Code style](#code-style)
  - [Code style examples](#code-style-examples)
  - [Submitting code changes](#submitting-code-changes)
- [Tools](#tools)
  - [compile-commits](#compile-commits)
  - [clang-format](#clang-format)
  - [count-warnings](#count-warnings)


## Feature requests and bug reports

If you find a [feature request][enhancement_label] you're interested in, leave
a comment—this will help us decide where to focus our development effort.

Report bugs via our [bugtracker][issues]. Issues and requests reported via
comments on social media or private messages are very likely going to be lost.

[enhancement_label]: https://github.com/dosbox-staging/dosbox-staging/issues?q=is%3Aissue+is%3Aopen+label%3Aenhancement
[issues]: https://github.com/dosbox-staging/dosbox-staging/issues


## Find something to work on

There are plenty of tasks to work on all around. Here are some ideas:

- Test DOSBox Staging with your DOS games.

- Improve our documentation (in the repo, or in the [project
  wiki](https://github.com/dosbox-staging/dosbox-staging/wiki)).

- Promote the project in gaming communities 😎

- Provide translations (refer to our [translations guide](https://github.com/dosbox-staging/dosbox-staging/blob/main/resources/translations/README.md)).

- Look at warnings in the code (build with `-Wall -Weffc++` to see the long
  list of potential code improvements), and try to eliminate them.

- Peruse the list of [open bug
  reports](https://github.com/dosbox-staging/dosbox-staging/issues?q=is%3Aissue+is%3Aopen+label%3Abug)
  and try to reproduce or fix them. If the issue is not assigned to anyone,
  then it's for the picking! Leave a comment saying that you're working on
  it.

- Look through our static analysis reports, pick an issue, investigate if the
  problem is true-positive or false-positive and if the code can be improved.
  See the [PVS-Studio analysis][pvs] artifacts and reports.

- Check out the reports tagged with ["good first
  issue"](https://github.com/dosbox-staging/dosbox-staging/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22)—these
  are suitable for people getting familiar with the project.

Or just send us a Pull Request with your improvement (but please read
[Submitting code changes](#submitting-code-changes) first).

If you plan to work on a new, bigger feature, then it might be a good idea to
discuss it with us early, e.g., by creating a new issue ticket.

[pvs]: https://github.com/dosbox-staging/dosbox-staging/actions/workflows/pvs-studio.yml


## Contributing code

These rules apply to code in `src/` and `include/` directories. They don't
apply to vendored libraries in the `src/libs/` directory (these have their own
coding conventions).

The rules outlined below apply to new code landing in the `main` branch.

### Language standard

We use C++20 while avoiding the more complex areas of C++ and object-oriented
design. To clarify:

- Avoid designing your code in a complex object-oriented style. This does not
  mean "don't use classes"; it means "don't use stuff like multiple
  inheritance, overblown class hierarchies, operator overloading, iostreams
  for stdout/stderr, etc".

- C++20 has a [rich standard library](https://en.cppreference.com/w/cpp/20),
  use it. We use [STL
  containers](https://en.cppreference.com/w/cpp/container),
  [std::filesystem](https://en.cppreference.com/w/cpp/filesystem), and various
  [concurrency constructs](https://en.cppreference.com/w/cpp/thread) in our
  new code. All new code must use the C++ standard library instead of C
  arrays, C filesystem functions, and OS-specific concurrency APIs if possible.

- Before starting to use platform-specific, OS-level APIs, check if the C++
  standard library or SDL provides a cross-platform implementation already.
  They probably do. If that's not the case, look into using battle-tested
  libraries instead of adding lots of platform-specific custom code.

- In general, try to use well-established libraries for common tasks. Don't
  reinvent the wheel unnecessarily.

- Our use of SDL is generally restricted to the texture renderer fallback
  option, and interfacing with OS-specific APIs that aren't covered by the C++
  standard. So that means window management, OpenGL context creation, input
  handling, and interfacing with OS-level audio and MIDI APIs. We should avoid
  using SDL's thread, timer, filesystem and similar APIs when equivalent
  functionality is available in the C++ standard library (we prefer to use
  `std::thread`, `std::mutex`, `std::lock_guard`, `std::filesystem`,
  `std::atomic`, etc.)

- The use of SDL-specific types should be restricted to a minimum. We don't
  want them to "infect" the entire codebase (ideally, they should be only used
  in a few files that directly interface with SDL).

- Prefer [RAII](https://en.cppreference.com/w/cpp/language/raii) patterns for
  resource lifecycle management when possible. This means using [smart
  pointers](https://en.cppreference.com/w/cpp/memory) (`unique_ptr`,
  `shared_ptr`, `weak_ptr`, etc.) Using raw C pointers is heavily
  discouraged and should be only used as a last resort when interfacing with
  legacy DOSBox code.

- Use modern C++ features like `constexpr`, `static_assert`, lambda
  expressions, for-each and range-expression loops, etc.

- But do not use C++ iostreams at all.

- Do not use C string functions and low-level C string manipulation in new
  code. Only use `std::string` and `std::string_view`.

- Avoid using exceptions. C++ exceptions are trickier than you think.
  No, you won't get it right. Or the person touching the code after you won't
  get it right. Or the person using your exception-ridden interface won't get
  it right. 😅 Let errors like `std::logic_error` or `std::bad_alloc`
  terminate the process, so it's easier to notice during testing and can be
  fixed early. The single good use case for exceptions is signalling
  construction failures in RAII-style lifecycle management. That's vastly
  preferable to two-stage initialisation (i.e., to have a separate `Init()`
  method you need to call after object construction). Other than that, don't
  throw exceptions anywhere else.

- Don't micro-optimise, and avoid writing "clever code". Always optimise for
  clarity of intent, maintainability, and readability in the first round,
  and only deviate from that if warranted due to performance considerations
  backed by real-world measurements. _Do not attempt to "optimise"
  anything before proving you are indeed dealing with a bottleneck, and even
  then, only optimise things that cause measurable, user-observable issues
  during real-world usage (e.g., speeding something up 50-fold in a
  synthetic micro-benchmark might or might not matter during real usage)._

- Avoid complex template metaprogramming. Simple templates are fine.

- Avoid complex macros. If possible, write a `constexpr` function or a simple
  template instead.

- Never write `using namespace std;`. We don't want any confusion about what
  comes from STL and what's project-specific.

- Prefer using namespaces in new code instead of name prefixes. Namespaces are
  also handy for grouping strings and constants.


### Code formatting

For new code, follow K&R style—see [Linux coding style] for examples and some
advice on good C coding style.

Following all the details of formatting style is tedious; that's why we use a
custom [clang-format](https://clang.llvm.org/docs/ClangFormat.html) ruleset to
make it crystal clear. See the [clang-format](#clang-format) section to learn
how.

[Linux coding style]:https://www.kernel.org/doc/html/latest/process/coding-style.html


### Code style

1. Use `auto` as much as you can.

2. Try hard to be `const`-correct as much as possible.

3. Keep the `const`s in sync in the function/method declarations and
   definitions.

4. Prefer to use enum classes.

5. Always initialise all variables and struct members.

6. Struct-nesting is encouraged as logical groupings improve readability.

7. **NEVER** use Hungarian notation!

8. But always include unit of measurement suffixes (e.g., `delay_ms` instead
   of just `delay`, `cutoff_freq_hz` instead of just `cutoff_freq`, etc.)

9. Prefer using plain `int`s for numbers. Do not micro-optimise the size of
the integer and avoid using unsigned types. Prefer using `int64_t` over
`uint32_t` if you need to represent big numbers. Only use `intXX_t`/`uintXX_t`
when dealing with binary protocols, and `size_t` only if it makes interfacing
with standard library functions less cumbersome.

10. Most old code uses the naming convention `MODULENAME_FunctionName` for
    public module interfaces. Do **NOT** replace it with namespaces.

11. Generally, we only use `UpperCamelCase` and `lower_snake_case`. Function
    arguments, variable names, struct members, and static functions are
    `lower_snake_case`—everything else is `UpperCamelCase`. See the code
    example below for concrete examples.

12. Don't uppercase acronyms, so use `PngWriter` instead of `PNGWriter`.

13. Prefer to use pre-increment/decrement whenever possible, so `++i` and `--i`.

14. Sort includes according to [Google C++ Style
    Guide](https://google.github.io/styleguide/cppguide.html#Names_and_Order_of_Includes).

15. Enable narrowing checks in new code and when you rework a file by adding
    `CHECK_NARROWING()` and including `"util/checks.h"` (just search for
    `CHECK_NARROWING())`.

16. Use header guards in the format: `DOSBOX_HEADERNAME_H` or
    `DOSBOX_MODULENAME_HEADERNAME_H`.

17. Use end-of-line comments very sparingly (only for tabular data), and use
    `//` for block comments.

18. Surround debug logging with `#if 0` & `#endif` pairs instead of commenting
    the log statements out, or better yet, introduce define switches per topic
    (e.g., `#define DEBUG_VGA_DRAW`).


### Code style examples

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

Check out these more recently introduced files for further examples:

- [src/audio/clap/*](https://github.com/dosbox-staging/dosbox-staging/tree/main/src/audio/clap) (all files)
- [src/midi/midi_soundcanvas.cpp](https://github.com/dosbox-staging/dosbox-staging/blob/main/src/midi/midi_soundcanvas.cpp)
- [src/midi/midi_soundcanvas.h](https://github.com/dosbox-staging/dosbox-staging/blob/main/src/midi/midi_soundcanvas.h)
- [src/capture/image/*](https://github.com/dosbox-staging/dosbox-staging/tree/main/src/capture/image) (all files)


### Submitting code changes

You'll need to submit code changes via GitHub PRs. The key points

1. Prefer small, focused commits with clearly defined scope. This aids
   bisecting when tracking down bugs and regressions. The smaller commits, the
   better!

2. Including style changes in functional code change commits is fine as long
   as they only affect 10-20 lines of code. When reformatting larger code
   blocks, always create a separate reformat commit first, followed by the
   actual code change in a second commit. The reformat commit's title should
   be prefixed with `style:` (e.g., `style: Reformat midi_fluidsynth.cpp`).

3. Make sure that all your commits can be compiled individually. This is very
   important for bisecting, and we strictly enforce it. You can just run
   `scripts/tools/compile-commits.sh` to compile all commits of the PR you're
   working on.


#### Commit messages

Read [How to Write a Git Commit Message]. Then read it again, and follow "the
seven rules" 😎

The only exception to these rules is commits landing from our upstream
project, so occasionally you might find a commit originating from the
`svn/trunk` branch that does not follow them.  There are no other exceptions.

[How to Write a Git Commit Message]:https://chris.beams.io/posts/git-commit/


#### Commit prefixes

We use a limited subset of commit prefixes to help separate functional
code changes from non-functional ones:

- `build:` — Changes to the build scripts.
- `ci:` — Changes related to the GitHub Actions workflows.
- `docs:` — Changes to the documentation, including changing config setting
  descriptions.
- `style:` — Reformats or style changes with zero functional changes.
- `website:` — Changes to the MkDocs sources the website is generated from.

The use of the `fix:`, `feat:`/`feature:`, `refactor:`, and `test:` prefixes
is discouraged as they don't add much value, only noise, and it's often not
clear-cut whether a change is a fix, a new feature, or a refactoring job.


#### Commit messages for patches authored by someone else

- If possible, preserve the code formatting used by the original author in
  your first import commit. Style changes and any other code changes should be
  in subsequent commits.

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

For an example of a commit that followed all of these rules, see commit
[ffe3c5ab](https://github.com/dosbox-staging/dosbox-staging/commit/ffe3c5ab7fb5e28bae78f07ea987904f391a7cf8):

    $ git log -1 ffe3c5ab7fb5e28bae78f07ea987904f391a7cf8


## Tools

### compile-commits

Make sure that all your commits can be compiled individually. This is very
important for bisecting, and we strictly enforce it. You can simply just run
`scripts/tools/compile-commits.sh` to compile all commits of the PR you're
working on.


### clang-format

It's usually distributed with the Clang compiler and can be integrated with
[many programming
environments](https://releases.llvm.org/10.0.0/tools/clang/docs/ClangFormat.html).

Outside of your editor, code can be formatted by invoking the tool directly:

    $ clang-format -i file.cpp

But it's better not to re-format the whole file at once—you can target
specific line ranges (run `clang-format --help` to learn how), or use our
helper script to format C/C++ code touched by your latest commit:

    $ git commit -m "Edit some C++ code"
    $ ./scripts/tools/format-commit.sh

Run `./scripts/tools/format-commit.sh --help` to learn about available options.


#### Vim integration

Download `clang-format.py` file somewhere, and make it executable:

    $ curl "https://raw.githubusercontent.com/llvm/llvm-project/main/clang/tools/clang-format/clang-format.py" > ~/.vim/clang-format.py
    $ chmod +x ~/.vim/clang-format.py

Then add the following lines to your `.vimrc` file:

    " Integrate clang-format tool
    map <C-K> :py3f ~/.vim/clang-format.py<cr>
    imap <C-K> <c-o>:py3f ~/.vim/clang-format.py<cr>

Read the documentation inside `clang-format.py` file in case your OS is
missing Python 3 support.


#### MSVC integration

[ClangFormat extension on VisualStudio Marketplace](https://marketplace.visualstudio.com/items?itemName=LLVMExtensions.ClangFormat)


### count-warnings

Our quality gating mechanism tracks the number of warnings per OS/compiler. To see a
summary of warnings in your build, do a clean build and then use the script
`./scripts/ci/count-warnings.py`:

    make -j "$(nproc)" |& tee build.log
    ./scripts/ci/count-warnings.py build.log

Run `./scripts/ci/count-warnings.py --help` to learn about available options.
