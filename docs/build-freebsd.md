# Building on FreeBSD

FreeBSD builds can be created with Clang using the Meson build system.

Before getting started, you should know that FreeBSD isn't fully supported or
used by the current maintainers. It's been lightly tested on a big-endian
PowerPC system.

## Pre-requisites

### Setup your `bin` path

In order to run Python PIP-installed programs, we link `~/.local/bin` to
`~/bin`, which is in FreeBSD's default user `PATH`:

``` shell
ln -s .local/bin ~/bin
```

## Build instructions

Clone and enter the repository:

``` shell
git clone https://github.com/dosbox-staging/dosbox-staging.git
cd dosbox-staging
```

Install dependencies:

``` shell
$ su (enter root password)
# pkg install `cat packages/freebsd.txt`
# exit (back to your user account)
$ pip install meson
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
