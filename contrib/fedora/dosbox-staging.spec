Name:    dosbox-staging
Version: 0.75.1
Release: 1%{?dist}
Summary: DOS/x86 emulator focusing on ease of use
License: GPLv2+
URL:     https://dosbox-staging.github.io/
Source:  https://github.com/dosbox-staging/dosbox-staging/archive/v%{version}/%{name}-%{version}.tar.gz

# This package is a drop-in replacement for dosbox
Provides: dosbox

BuildRequires: gcc-c++
BuildRequires: gcc
BuildRequires: automake
BuildRequires: alsa-lib-devel
BuildRequires: libpng-devel
BuildRequires: SDL2-devel
BuildRequires: SDL2_net-devel
BuildRequires: opusfile-devel
BuildRequires: librsvg2-tools
BuildRequires: desktop-file-utils

Requires: hicolor-icon-theme


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
%autosetup -n %{name}-%{version}


%build
./autogen.sh
%configure \
        CPPFLAGS="-DNDEBUG" \
        CFLAGS="${CFLAGS/-O2/-O3}" \
        CXXFLAGS="${CXXFLAGS/-O2/-O3}"
# binary
%{__make} %{_smp_mflags}
# icons
%{__make} -C contrib/icons hicolor


%install
make install DESTDIR=%{buildroot}

pushd contrib/icons/hicolor
install -p -m 0644 -Dt %{buildroot}%{_datadir}/icons/hicolor/16x16/apps    16x16/apps/%{name}.png
install -p -m 0644 -Dt %{buildroot}%{_datadir}/icons/hicolor/22x22/apps    22x22/apps/%{name}.png
install -p -m 0644 -Dt %{buildroot}%{_datadir}/icons/hicolor/24x24/apps    24x24/apps/%{name}.png
install -p -m 0644 -Dt %{buildroot}%{_datadir}/icons/hicolor/32x32/apps    32x32/apps/%{name}.png
install -p -m 0644 -Dt %{buildroot}%{_datadir}/icons/hicolor/scalable/apps scalable/apps/%{name}.svg
popd

desktop-file-install \
        --dir=%{buildroot}%{_datadir}/applications \
        contrib/linux/%{name}.desktop


%files
%license COPYING
%doc AUTHORS README THANKS
%{_bindir}/*
%{_mandir}/man1/*
%{_datadir}/applications/*
%{_datadir}/icons/hicolor/*/apps/dosbox-staging.*


%changelog
* Thu Oct 01 2020 Patryk Obara (pbo) <dreamer.tan@gmail.com>
- 0.75.1-1
- Initial release.
