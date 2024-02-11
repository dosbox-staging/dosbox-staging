Name:    dosbox-staging
Version: 0.80.1
Release: 1%{?dist}
Summary: DOS/x86 emulator focusing on ease of use
License: GPLv2+
URL:     https://dosbox-staging.github.io/

Source0: https://github.com/dosbox-staging/dosbox-staging/archive/v%{version}/%{name}-%{version}.tar.gz

# This package is a drop-in replacement for dosbox
Provides:  dosbox = %{version}-%{release}
Obsoletes: dosbox < 0.74.4

BuildRequires: alsa-lib-devel
BuildRequires: desktop-file-utils
BuildRequires: ffmpeg-free-devel
BuildRequires: fluidsynth-devel >= 2.2.3
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: git
BuildRequires: gmock-devel
BuildRequires: gtest-devel
BuildRequires: iir1-devel
BuildRequires: libappstream-glib
BuildRequires: libatomic
BuildRequires: libpng-devel
BuildRequires: libslirp-devel >= 4.6.1
BuildRequires: make
BuildRequires: meson >= 0.54.2
BuildRequires: mt32emu-devel >= 2.5.3
BuildRequires: opusfile-devel
BuildRequires: SDL2-devel >= 2.0.5
BuildRequires: SDL2_net-devel
BuildRequires: speexdsp-devel > 1.1.9

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


%build
%meson
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
%{_datadir}/%{name}/translations/*
%{_datadir}/%{name}/drives/*
%{_datadir}/%{name}/freedos-cpi/*
%{_datadir}/%{name}/freedos-keyboard/*
%{_datadir}/%{name}/glshaders/*
%{_datadir}/%{name}/mapperfiles/*
%{_datadir}/%{name}/mapping-freedos.org/*
%{_datadir}/%{name}/mapping-unicode.org/*
%{_datadir}/%{name}/mapping-wikipedia.org/*
%{_datadir}/%{name}/mapping/*
%{_metainfodir}/*


%changelog
* Sat Jan 07 2023 rderooy <rderooy@users.noreply.github.com> - 0.80.1-1
- Update to 0.80.1
- Remove included mt32emu as it is now available in the distro repos

* Fri Dec 24 2021 kcgen <kcgen@users.noreply.github.com>
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
