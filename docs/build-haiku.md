Haiku builds can be created with Clang or GCC using the Meson buildsystem.

## Build on Haiku

Install dependencies:

``` shell
pkgman install -y meson ccache libpng libsdl2_devel sdl2_net_devel \
                  libogg_devel opus_devel opusfile_devel gcc_syslibs_devel \
                  opus_tools fluidsynth2_devel llvm_clang speexdsp_devel
```

Clone and enter the repository's directory:

``` shell
git clone https://github.com/dosbox-staging/dosbox-staging.git
cd dosbox-staging
```

Build with OpenGL disabled (Haiku supports OpenGL only via software fallback):

``` shell
meson setup -Duse_opengl=false build
ninja -C build
```

Detailed instructions and build options are documented in [BUILD.md](/BUILD.md).

## Run

Edit your configuration file by running: `dosbox -editconf` and make the
following suggested changes (leave all other settings as-is):

``` ini
[sdl]
output = texture
texture_renderer = software

[cpu]
core = normal
```

The state of Haiku's GPU Hardware-acceleration is being discussed here:
https://discuss.haiku-os.org/t/state-of-accelerated-opengl/4163
