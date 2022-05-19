Name:    dosbox-staging
Version: 0.79.0
Release: 2%{?dist}
Summary: DOS/x86 emulator focusing on ease of use
License: GPLv2+
URL:     https://dosbox-staging.github.io/

Source0: https://github.com/dosbox-staging/dosbox-staging/archive/v%{version}/%{name}-%{version}.tar.gz
Source1: https://github.com/munt/munt/archive/libmt32emu_2_5_3.tar.gz
# Downloaded from: https://wrapdb.mesonbuild.com/v2/mt32emu_2.5.3-1/get_patch
Source2: mt32emu_2.5.3-1_patch.zip

# https://github.com/dosbox-staging/dosbox-staging/commit/5d25187760e595f7e6efa6b639c3945fb4804db1
Patch1: 0001-Add-0.77.0-release-to-metainfo.xml.patch

# This package is a drop-in replacement for dosbox
Provides:  dosbox = %{version}-%{release}
Obsoletes: dosbox < 0.74.4

Provides: bundled(mt32emu) = 2.5.3

BuildRequires: alsa-lib-devel
BuildRequires: desktop-file-utils
BuildRequires: fluidsynth-devel >= 2.2.3
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: git
BuildRequires: gtest-devel
BuildRequires: libappstream-glib
BuildRequires: libatomic
BuildRequires: libpng-devel
BuildRequires: libslirp >= 4.6.1
BuildRequires: make
BuildRequires: meson >= 0.54.2
BuildRequires: opusfile-devel
BuildRequires: SDL2-devel >= 2.0.5
BuildRequires: SDL2_net-devel
BuildRequires: speexdsp-devel > 1.1.9
# mt32emu dependencies:
BuildRequires: cmake

Requires: hicolor-icon-theme
Requires: fluid-soundfont-gm


%description
DOSBox Staging is full x86 CPU emulator (independent of host architecture),
capable of running DOS programs that require real or protected mode.

It features built-in DOS-like shell terminal, emulation of several PC variants
(IBM PC, IBM PCjr, Tandy 1000), CPUs (286, 386, 486, Pentium I), graphic
chipsets (Hercules, CGA, EGA, VGA, SVGA), audio solutions (Sound Blaster,
Gravis UltraSound, Disney Sound Source, Tandy Sound System), CD Digital Audio
emulation (also with audio encoded as FLAC, Opus, OGG/Vorbis, MP3 or WAV),
joystick emulation (supports modern game controllers), serial port emulation,
IPX over UDP, GLSL shaders, and more.

DOSBox Staging is highly configurable, well-optimized and fast enough to run
any old DOS game using modern hardware.


%prep
%autosetup -p1
# mt32emu is not packaged yet; provide sources for Meson wrap dependency:
mkdir -p %{_vpath_srcdir}/subprojects/packagecache/
cp %{SOURCE1} %{_vpath_srcdir}/subprojects/packagecache/
cp %{SOURCE2} %{_vpath_srcdir}/subprojects/packagecache/


%build
%meson -Ddefault_library=static
%meson_build


%install
%meson_install


%check
%meson_test
appstream-util validate-relax --nonet %{buildroot}%{_metainfodir}/*.xml


%files
%license COPYING
%doc AUTHORS README THANKS
%{_bindir}/*
%{_mandir}/man1/*
%{_datadir}/applications/*
%{_datadir}/icons/hicolor/*/apps/dosbox-staging.*
%{_metainfodir}/*


%changelog
* Fri 24 Dec 2021 kcgen <kcgen@users.noreply.github.com>
- 0.78.0-1
- Update to 0.78.0
- Raise minimum Meson version to 0.54.2
- Raise minimum FluidSynth version to 2.2.3
- Raise mt32emu version to 2.5.3 and update download links
- Add libslirp build dependency (new feature)

* Sun Jul 04 2021 Patryk Obara (pbo) <dreamer.tan@gmail.com>
- 0.77.0-2
- Indicate bundled mt32emu library via "Provides" tag
- Raise minimum SDL version to 2.0.5

* Sat Jul 03 2021 Patryk Obara (pbo) <dreamer.tan@gmail.com>
- 0.77.0-1
- Update to 0.77.0
- Replace Autotools with Meson

* Mon Jun 21 2021 Gwyn Ciesla <gwync@protonmail.com> - 0.76.0-3
- Fluidsynth rebuild.

* Tue Jan 26 2021 Patryk Obara (pbo) <dreamer.tan@gmail.com>
- 0.76.0-2
- Tighten dependencies checks

* Mon Jan 25 2021 Patryk Obara (pbo) <dreamer.tan@gmail.com>
- 0.76.0-1
- Update to 0.76.0
- Add fluidsynth-devel build dependency (new feature)
- Add fluid-soundfont-gm runtime dependency (default soundfont)

* Tue Oct 27 2020 Patryk Obara (pbo) <dreamer.tan@gmail.com>
- 0.75.2-1
- Update to 0.75.2

* Thu Oct 01 2020 Patryk Obara (pbo) <dreamer.tan@gmail.com>
- 0.75.1-1
- Initial release.
