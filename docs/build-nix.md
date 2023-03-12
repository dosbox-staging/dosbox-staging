# Building on Nix0S

Compiling code on Nix requires slightly different inputs from other linux distros,
since the appropriate environment variables for pkg-config are only avaliable 
under the `nix-shell` command.

## Installing the dependencies under Nix

Nix has a unique toolset where you are not required to install the dependencies 
to run them. However, if you still want to, we recommend you to do the following:

### Install dependencies with Home Manager (Recommended)

1. Install Home-Manager: <https://github.com/nix-community/home-manager#installation>.
2. After following the installation guide, add the following packages on your home.nix:

    ``` shell
    home.packages = [ pkg-config gcc_multi cmake ccache SDL2 SDL2_image SDL2_net 
            fluidsynth glib gtest libGL libGLU libjack2 libmt32emu libogg
            libpng libpulseaudio libslirp libsndfile meson ninja opusfile
            libselinux speexdsp stdenv alsa-lib xorg.libXi irr1 ]
    ```

3. Rebuild with the command `home-manager switch`

### Install dependencies in a standard Nix configuration

1. Add the following packages to configuration.nix:

    ``` shell
    environment.systemPackages = [ 
            pkg-config gcc_multi cmake ccache SDL2 SDL2_image SDL2_net 
            fluidsynth glib gtest libGL libGLU libjack2 libmt32emu libogg
            libpng libpulseaudio libslirp libsndfile meson ninja opusfile
            libselinux speexdsp stdenv alsa-lib xorg.libXi irr1 ];
    ```

## Build 

After you `git clone https://github.com/dosbox-staging/dosbox-staging.git` and 
finish setting it up, run the following command from the terminal, with or without 
the dependencies installed:

``` shell
nix-shell -p $(cat packages/NixOS.txt) --run 'meson setup build'
```

Once it's finished setting up the build, run:

``` shell
nix-shell -p $(cat packages/NixOS.txt) --run 'meson compile -C build'
```

After that, you should be all set! Your binary will be located in `build/dosbox`!

See more build options in [BUILD.md](/BUILD.md).
