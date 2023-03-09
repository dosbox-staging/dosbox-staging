# Building on Nix

The Nix Ecosystem insists on the purity of its builds, therefore compiling 
Dosbox Staging on it requires slightly different inputs so meson gets the 
appropriate environment variables from pkg-config.

## Installing the Dependencies under Nix

Nix has an unique toolset where you don't have to install it's packages to run 
them. However, if you still want to, instead of using `nix-env` to install your 
dependencies like your traditional package manager, we recommend you to do the 
following:

### Install dependencies with Home Manager (Recommended)

1. Install Home-Manager: <https://github.com/nix-community/home-manager#installation>.
2. After following the guide add the following packages on home.nix:

    ``` shell
    home.packages = [ pkg-config gcc_multi cmake ccache SDL2 SDL2_image SDL2_net 
			fluidsynth glib gtest libGL libGLU libjack2 libmt32emu libogg
			libpng libpulseaudio libslirp libsndfile meson ninja opusfile
			libselinux speexdsp stdenv alsa-lib xorg.libXi irr1 ]
    ```

3. Then rebuild with the command `home-manager switch`

### Install dependencies in a standard Nix configuration

1. Add the following packages on configuration.nix:

    ``` shell
environment.systemPackages = [ 
                  pkg-config gcc_multi cmake ccache SDL2 SDL2_image SDL2_net 
                  fluidsynth glib gtest libGL libGLU libjack2 libmt32emu libogg
                  libpng libpulseaudio libslirp libsndfile meson ninja opusfile
                  libselinux speexdsp stdenv alsa-lib xorg.libXi irr1 
                  ]; 
    ```


## Build 

After you `git clone https://github.com/dosbox-staging/dosbox-staging.git` and 
finish setting it up, run the following command on the terminal, with or without 
the dependencies installed:

``` shell
nix-shell -p meson ninja gcc_multi cmake ccache SDL2 SDL2_image SDL2_net gtest \
fluidsynth glib libGL libGLU libjack2 libmt32emu libogg libpng libpulseaudio \
libslirp libsndfile opusfile libselinux speexdsp stdenv alsa-lib xorg.libXi \
irr1 pkg-config --run 'meson setup build'
```

Once it's finished setting up the build, then run:

``` shell
nix-shell -p meson ninja gcc_multi cmake ccache SDL2 SDL2_image SDL2_net gtest \
fluidsynth glib libGL libGLU libjack2 libmt32emu libogg libpng libpulseaudio \
libslirp libsndfile opusfile libselinux speexdsp stdenv alsa-lib xorg.libXi \
irr1 pkg-config --run 'meson compile -C build'
```

After that, you should be all set! Your binary will be located at `build/dosbox`!

See more build options in [BUILD.md](/BUILD.md).