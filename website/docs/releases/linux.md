---
hide:
  - footer
---

# Linux releases

## Current stable version

<section class="release-downloads" markdown>

[Download DOSBox Staging 0.82.1 (x86_64)][0_82_1]
<br>
<small>
sha256: TODO<wbr>TODO

</small>

</section>


Our pre-compiled builds run on most desktop Linux distributions (x86\_64 only for
now). They only depend on the C/C++, ALSA, and OpenGL system libraries.

Please run the `install-icons.sh` script included with the release to install
the application icons.

Check out the [0.82.1 release notes](release-notes/0.82.1.md) to learn
about the changes and improvements introduced in this release.

If you're new to DOSBox Staging, we highly recommend following the
[Getting started guide](../getting-started/index.md).


## Hardware requirements

From the x86 family of processors, a processor with SSSE3 (Supplemental
Streaming SIMD Extensions 3) is required.


## External repository packages

DOSBox Staging is also packaged by external teams, as listed below.
These packages may have variations in configuration file locations,
filesystem or network restrictions, feature exclusions, and other
differences compared to the project-released tarball.

To understand these potential differences, we recommend referring to the
repository's documentation and, if uncertain, comparing against the
project-released tarball.

The DOSBox Staging team does not track or document these differences.
For issues specific to these packages, please contact the respective
repository owners.


### Containerised packages

[![Download from Flathub](https://flathub.org/assets/badges/flathub-badge-en.png){ class=linux-badge width=35% align=left }][flathub]

[flathub]:https://flathub.org/apps/details/io.github.dosbox-staging


### Fedora repository package

    sudo dnf install dosbox-staging

### Gentoo repository package

    emerge games-emulation/dosbox-staging

### Ubuntu and Mint repository package

Available via [Personal Package Archive](https://launchpad.net/~feignint/+archive/ubuntu/dosbox-staging):

    sudo add-apt-repository ppa:feignint/dosbox-staging
    sudo apt-get update
    sudo apt install dosbox-staging

### Arch and Manjaro repository package

Available via [Arch User Repository](https://aur.archlinux.org/packages/dosbox-staging).

### NixOS repository package

Available via NixOS or Home Manager. Add the following to your `configuration.nix` file:

For NixOS:

    environment.systemPackages = with pkgs; [
      dosbox-staging
    ];

For Home Manager:

    home.packages = with pkgs; [
      dosbox-staging
    ];

Then rebuild your system with: `nixos-rebuild switch`

### Other repository packages

[![Packaging status](https://repology.org/badge/vertical-allrepos/dosbox-staging.svg){ width=230 }][other-repos]

[other-repos]:https://repology.org/project/dosbox-staging/versions

## Steam

You can easily configure your DOS games on Steam to use DOSBox Staging via
[Boxtron](https://github.com/dreamer/boxtron) (a community-developed
Steam Play compatibility tool for DOS games).

Boxtron will automatically use `dosbox` if found in your path, or can be
configured to use a specific binary by editing the file
`~/.config/boxtron.conf` and overriding [dosbox.cmd][boxtron-conf]:

    cmd = ~/path-to-dosbox-staging/dosbox

[boxtron-conf]:https://github.com/dreamer/boxtron/wiki/Configuration#dosboxcmd


## RetroPie package

You can easily configure your DOS games on
[Retropie](https://retropie.org.uk/) to use DOSBox Staging via
[RetroPie-Setup](https://github.com/RetroPie/RetroPie-Setup) (select
**Optional Packages** --> **DOSBox Staging**).


## Development snapshot builds

You can always see what's cooking on the main branch! :sunglasses: :beer:

These [snapshot builds](development-builds.md) might be slow or unstable as they
are designed with developers and testers in mind.


## Older releases

- [Download DOSBox Staging 0.82.0 (x86_64)][0_82_0]
  <br>
  <small>
  sha256: fd491de6e989da2b34be743fd419735b<wbr>efc80e6f507ec15b9b3ea6164624dabf
  </small>

- [Download DOSBox Staging 0.81.2 (x86_64)][0_81_2]
  <br>
  <small>
  sha256: c47f1767ae1371666f40e3a4e13272da<wbr>5c5a98c9c6f355b4fb82bac0d3911a68
  </small>

- [DOSBox Staging 0.81.1 (x86_64)][0_81_1]
  <br>
  <small>
  sha256: 5aee92774569cf1e39ade3fccff03994<wbr>464d17b396b0ae98360af61e9d37cba7
  </small>

- [DOSBox Staging 0.81.0 (tar.xz)][0_81_0]
  <br>
  <small>
  sha256: 034b08a941a7fd0279a81b10af620999<wbr>c569f7e81b786e7f4b59a0b94e46d399
  </small>

- [DOSBox Staging 0.80.1 (x86_64)][0_80_1]
  <br>
  <small>
  sha256: 12582a6496b1a276cd239e6b3d21ddfc<wbr>d51fd8f9e40a1ebbc0a3800e0636190a
  </small>

- [DOSBox Staging 0.80.0 (x86_64)][0_80_0]
  <br>
  <small>
  sha256: 3022bdd405dc1106007c3505e6a5d083<wbr>de982d516c9bce499e2c4a02a697a1bd
  </small>

- [DOSBox Staging 0.79.1 (x86_64)][0_79_1]
  <br>
  <small>
  sha256: aebf8619bb44934f18d0e219d50c4e2c<wbr>03b179c37daa67a9b800e7bd3aefc262
  </small>

- [DOSBox Staging 0.78.1 (x86_64)][0_78_1]
  <br>
  <small>
  sha256: 8bd2a247ca960f6059276db2b0331f85<wbr>3e16bc8a090722b15f567782542b5fba
  </small>

- [DOSBox Staging 0.78.0 (x86_64)][0_78_0]
  <br>
  <small>
  sha256: 085e7cbe350546b3f25b0400c872a276<wbr>6f9a49d16a5ca8d17a0a93aad6e37709
  </small>

- [DOSBox Staging 0.77.1 (x86_64)][0_77_1]
  <br>
  <small>
  sha256: e2d475e4b1f80881ccafc4502b3884c0<wbr>96b51aa2fc2cfe89bb6c2b8ebfb7cc76
  </small>

- [DOSBox Staging 0.76.0 (x86_64)][0_77_0]
  <br>
  <small>
  sha256: f8401bcd473d5b664eeb3a90e4dbb4bb<wbr>f0cef5339adba867f361c00b7de9b2fe
  </small>

- [DOSBox Staging 0.76.0 (x86_64)][0_76_0]
  <br>
  <small>
  sha256: b14de58ba0f5dd192398cda58fa439b1<wbr>5512f50d1c88b5ded6f300d4a9212852
  </small>

- [DOSBox Staging 0.75.2 (x86_64)][0_75_2]
  <br>
  <small>
  sha256: 0325a1860aea95e8117aa49b041bfd62<wbr>8ab20531a3abc7b0a67aff4c47049465
  </small>

- [DOSBox Staging 0.75.1 (x86_64)][0_75_1]
  <br>
  <small>
  sha256: aef22e5ddf93ff826fc2d48a4c8c0b40<wbr>97d3455525b40be5b3fb443935929c70
  </small>

- [DOSBox Staging 0.75.0 (x86_64)][0_75_0]
  <br>
  <small>
  sha256: a28d8ba0481722c8343b7532299c7b9b<wbr>b9e491c6832d9d05dd4704939287f776
  </small>

- [DOSBox Staging 0.75.0-rc1 (x86_64)][0_75_0_rc1]
  <br>
  <small>
  sha256: 594ba45280af240cb18b3882f7ffa711<wbr>69a697eb362b7d7a76c8ccda2b940e84
  </small>

[0_82_1]: https://github.com/dosbox-staging/dosbox-staging/releases/download/v0.82.1/dosbox-staging-linux-x86_64-v0.82.1.tar.xz
[0_82_0]: https://github.com/dosbox-staging/dosbox-staging/releases/download/v0.82.0/dosbox-staging-linux-x86_64-v0.82.0.tar.xz
[0_81_2]: https://github.com/dosbox-staging/dosbox-staging/releases/download/v0.81.2/dosbox-staging-linux-v0.81.2.tar.xz
[0_81_1]: https://github.com/dosbox-staging/dosbox-staging/releases/download/v0.81.1/dosbox-staging-linux-v0.81.1.tar.xz
[0_81_0]: https://github.com/dosbox-staging/dosbox-staging/releases/download/v0.81.0/dosbox-staging-linux-v0.81.0.tar.xz
[0_80_1]: https://github.com/dosbox-staging/dosbox-staging/releases/download/v0.80.1/dosbox-staging-linux-v0.80.1.tar.xz
[0_80_0]: https://github.com/dosbox-staging/dosbox-staging/releases/download/v0.80.0/dosbox-staging-linux-v0.80.0.tar.xz
[0_79_1]: https://github.com/dosbox-staging/dosbox-staging/releases/download/v0.79.1/dosbox-staging-linux-v0.79.1.tar.xz
[0_78_1]: https://github.com/dosbox-staging/dosbox-staging/releases/download/v0.78.1/dosbox-staging-linux-v0.78.1.tar.xz
[0_78_0]: https://github.com/dosbox-staging/dosbox-staging/releases/download/v0.78.0/dosbox-staging-linux-v0.78.0.tar.xz
[0_77_1]: https://github.com/dosbox-staging/dosbox-staging/releases/download/v0.77.1/dosbox-staging-linux-v0.77.1.tar.xz
[0_77_0]: https://github.com/dosbox-staging/dosbox-staging/releases/download/v0.77.0/dosbox-staging-linux-v0.77.0.tar.xz
[0_76_0]: https://github.com/dosbox-staging/dosbox-staging/releases/download/v0.76.0/dosbox-staging-linux-v0.76.0.tar.xz
[0_75_2]: https://github.com/dosbox-staging/dosbox-staging/releases/download/v0.75.2/dosbox-staging-linux-v0.75.2.tar.xz
[0_75_1]: https://github.com/dosbox-staging/dosbox-staging/releases/download/v0.75.1/dosbox-staging-linux-v0.75.1.tar.xz
[0_75_0]: https://github.com/dosbox-staging/dosbox-staging/releases/download/v0.75.0/dosbox-staging-linux-v0.75.0.tar.xz
[0_75_0_rc1]: https://github.com/dosbox-staging/dosbox-staging/releases/download/v0.75.0-rc1/dosbox-staging-linux-v0.75.0-rc1.tar.xz


## Building from source

Of course, you can always [build DOSBox Staging straight from the source][1].

Send us patches if you improve something! :smile:

[1]:https://github.com/dosbox-staging/dosbox-staging

