# Run-Tests

- [Run-Tests](#run-tests)
  - [Introduction](#introduction)
  - [Test-case Specification](#test-case-specification)
    - [Minimal Test-case Example](#minimal-test-case-example)
    - [Thorough Test-Case Example](#thorough-test-case-example)
  - [Installing Binary Dependencies](#installing-binary-dependencies)
  - [Source Control](#source-control)

---

## Introduction

Launches tests-cases found in the `contrib/test-cases/`, where each
test-case is a sub-directory.

## Test-case Specification

Each test-case is a *sub-directory* consisting of one or more *batch files*
that will be executed in DOSBox. The sub-directory should cover a single
theme of testing and be named accordingly; long descriptive names are fine.
Dashes should be used instead of spaces, and lower-case is prefered for
consistency. Some examples:

- `sdl-video-modes`
- `adlib-opl-types`
- `cdda-mp3-decode`

A test-case sub-directory contains:

- one or more DOS batch files, lower-case, and named in 8.3 notation. For
  example: `vga.bat`, `adlib-1.bat`, and `longname.bat`.
- (optional) correspondingly-named `*.conf` DOSBox conf files, tailored for
  the specific batch-file. For example: `vga.conf`, `adlib-1.conf`, and `longname.conf`.
- (optional) correspondingly-named `*.vars` files.
  For example: `vga.vars`, `adlib-1.vars`, and `longname.vars`
  
  If present, a vars-file may contain the following in `KEY=value` notation
  (default values are shown and used if a *.vars file is not present):
  - `RUNNING_AFTER=true` (or `false`), if the batch file is expected to be
    running after RUNTIME.
  - `RUNTIME=15`, the maximum number of seconds for which the test should
    be run. Reasonable values start around 5 (to allow DOSBox some time to start
    through to hundreds of seconds if the test needs that long to exercise the
    desired functionality.
  - `GRACETIME=3`, the number of seconds we should wait to kill the process,
    after first trying to elegantly stop the process.
  - `ARGS=""`, any additional arguments to be passed to the batch file
- (optional) `dosbox.conf`, a standard DOSBox conf that will be applied in
  common to all of the tests-cases batch files.
- (optional) any additional files or subdirectories needed to support the
  batch files such as a game or application directory.

### Minimal Test-case Example

This example test-case consists of trivial one-line batch file that launches
a (hypothetical) tool called `testvesa.exe` that sets and holds a VESA
video-mode indefinitely.  We're specifically interested in exercising
DOSBox's `vesa_nolfb` video emulation.

We would create the following directories and files:

- **Directory**: `contrib/test-cases/vesa-nolfb`
- **Batch file**: `contrib/test-cases/vesa-nolfb/run.bat`,
  containing one line: `testvesa.exe`
- **Conf file**: `contrib/test-cases/vesa-nolfb/run.conf`,
  containing the following:
  
  ``` text
  [dosbox]
  machine  = vesa_nolfb
  ```
  
- The `run.vars` file is absent because we are happy with the defaults.
  That is, we expect `testvesa.exe` to still be running after 15 seconds of
  runtime.  If `testvesa.exe` happens to fail (or DOSBox fails to set the
  video mode), then it will terminate before the expected 15s
  and the run-tests script will consider the test a failure.

### Thorough Test-Case Example

This examples tests DOSBox's `vgaonly` mode using *good* and *bad*
permutations.  We would create the following directory and files:

**Directory**: `contrib/test-cases/vgaonly`

**Batch files**:

- `contrib/test-cases/vgaonly/good.bat`
- `contrib/test-cases/vgaonly/bad.bat`

**Conf files**:

- `contrib/test-cases/vgaonly/dosbox.conf` containing:
  
  ``` text
  [dosbox]
  machine  = vgaonly
  ```

- `contrib/test-cases/vgaonly/good.conf` is not present
  (we don't need any customizations beyond our common `dosbox.conf`)
- `contrib/test-cases/vgaonly/bad.conf` is also not present for
  the same reason.

The good batch-file will run a valid program (such as a game), and we expect
that game to still be running after 15 seconds. The bad batch-file will attempt
to set an invalid mode, and we expect DOSBox to error-out and soon after
starting.  If the bad-batch file is still running after 10 seconds then something
has gone wrong (so would consider that a failure and want the test terminated).

**Vars files**:

- `contrib/test-cases/vgaonly/good.vars` is not present, we are
  happy with the defaults.
- `contrib/test-cases/vgaonly/bad.vars` would contain:
  
  ``` ini
  RUNNING_AFTER=false
  RUNTIME=10
  ```

## Installing Binary Dependencies

Binaries that support these tests (such as the hypothetical `testvesa.exe`)
are not present in source-control, therefore they must be downloaded,
generated (perhaps compiled), or derived (such as encoding an mp3 from a wav)
using a setup Bash script, in the form of `setup-*.sh`.

These `setup-*.sh` scripts can exist inside a given test-case directory,
however in the case where a DOS program can support more than one theme of
testing, we have opted to download and extract them in the `contrib/test-cases/common`
directory and use relative symlinks to those directories in the specific test-case
directory.

If you're working with the source tree, simply launch
`contrib/test-cases/common/setup-tests.sh` to setup the existing binary packages and
work with them, or add your own `setup-*.sh` script in `contrib/test-cases/common`.

## Source Control

The `contrib/test-cases/*` sub-directories are largely excluded in the repository's
root `.gitignore` because testing may generate ephemeral files or biniaries not suitable
for tracking in SCM, Therefore, please explicitly add any source files, text files,
scripts, or documentation that've written in your test-case sub-directory to ensure it's
part of your commit/PR.

For example, if you've added a directory symlink to `../common/<some-package>`, then
please explicitly add your symlink.
