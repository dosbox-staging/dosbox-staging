# Building on OpenBSD

OpenBSD builds can be created with Clang using the Meson build system.

Before getting started, you should know that OpenBSD isn't fully supported or
used by the current maintainers. It's been lightly tested on a big-endian
PowerPC system.

## Pre-requisites

### Expand the systems' memory limits

The LLVM link process will map more virtual memory than OpenBSD normally grants
to user processes, so it's recommended to open up these limits by editing
`/etc/login.conf` with your root account:

Change the two default `datasize-` fields to `infinity`, as follows:

```
default:\
  ...
  :datasize-max=infinity:\
  :datasize-cur=infinity:\
  ...
```

This change allowed the link process to successfully run on an Apple Powerbook
G4 with only 2 GB of physical RAM.

### Setup your `bin` path

In order to run Python PIP-installed programs, we link `~/.local/bin` to
`~/bin`, which is in OpenBSD's default user `PATH`:

``` shell
ln -s .local/bin ~/bin
```

### Consider a `wxallowed` mount-point on slow hardware

If your platform is old or slow (such as 32-bit PowerPC, MIPS, or
32-bit ARM SBC), then you might benefit by disabling per-page W^X
permission flagging.

If so:

 1) Read the about the per-page W^X permission build option in the
    `meson_options.txt`, and consider comparing builds.

 2) If you do make a build without per-page W^X (`-Dper_page_w_or_x=disabled`)
    your binary needs to reside within a filesystem mounted with the
    `wxallowed` flag. This is OpenBSD-specific, where as the other BSDs
    don't have this requirement.

    The good news is that OpenBSD mounts `/usr/local` with the `wxallowed`
    flag by default, so the easiest option is to simply place your
    build tree under there, for example: `/usr/local/src/dosbox-staging`.

    Alternatively, you can edit `/etc/fstab` and add the `wxallowed`
    mount flag the filesystem where your build tree resides.

If you're on a fast 64-bit platform or _are_ using per-page W^X, then
your binary will run anywhere and you can skip this.


## Build instructions

Clone and enter the repository:

``` shell
git clone https://github.com/dosbox-staging/dosbox-staging.git
cd dosbox-staging
```

Install dependencies:

``` shell
pkg_add $(cat packages/openbsd.txt)
pip install meson
```

Build:

``` shell
meson setup build
ninja -C build
```

To learn more about Meson's build options, see [BUILD.md](/BUILD.md).

## Run

Your binary is `build/dosbox`, which you can link into your `~/bin/` path:

```shell
ln -s $PWD/build/dosbox ~/bin/dosbox-staging
```
