from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.apple import is_apple_os
from conan.tools.build import cross_building
from conan.tools.env import Environment, VirtualBuildEnv, VirtualRunEnv
from conan.tools.files import (
    apply_conandata_patches, chdir, copy, export_conandata_patches, get, rename,
    replace_in_file, rm, rmdir
)
from conan.tools.gnu import Autotools, AutotoolsDeps, AutotoolsToolchain, PkgConfigDeps
from conan.tools.layout import basic_layout
from conan.tools.microsoft import check_min_vs, is_msvc, unix_path
from conan.tools.scm import Version
import os
import glob
import shutil
import re

required_conan_version = ">=1.57.0"


class FFMpegConan(ConanFile):
    name = "ffmpeg"
    version = "6.0"
    url = "https://github.com/conan-io/conan-center-index"
    description = "A complete, cross-platform solution to record, convert and stream audio and video"
    # https://github.com/FFmpeg/FFmpeg/blob/master/LICENSE.md
    license = ("LGPL-2.1-or-later", "GPL-2.0-or-later")
    homepage = "https://ffmpeg.org"
    topics = ("multimedia", "audio", "video", "encoder", "decoder", "encoding", "decoding",
              "transcoding", "multiplexer", "demultiplexer", "streaming")
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "avdevice": [True, False],
        "avcodec": [True, False],
        "avformat": [True, False],
        "swresample": [True, False],
        "swscale": [True, False],
        "postproc": [True, False],
        "avfilter": [True, False],
        "with_asm": [True, False],
        "with_zlib": [True, False],
        "with_bzip2": [True, False],
        "with_lzma": [True, False],
        "with_libiconv": [True, False],
        "with_freetype": [True, False],
        "with_openjpeg": [True, False],
        "with_openh264": [True, False],
        "with_opus": [True, False],
        "with_vorbis": [True, False],
        "with_zeromq": [True, False],
        "with_sdl": [True, False],
        "with_libx264": [True, False],
        "with_libx265": [True, False],
        "with_libvpx": [True, False],
        "with_libmp3lame": [True, False],
        "with_libfdk_aac": [True, False],
        "with_libwebp": [True, False],
        "with_ssl": [False, "openssl", "securetransport"],
        "with_libalsa": [True, False],
        "with_pulse": [True, False],
        "with_vaapi": [True, False],
        "with_vdpau": [True, False],
        "with_vulkan": [True, False],
        "with_xcb": [True, False],
        "with_appkit": [True, False],
        "with_avfoundation": [True, False],
        "with_coreimage": [True, False],
        "with_audiotoolbox": [True, False],
        "with_videotoolbox": [True, False],
        "with_programs": [True, False],
        "with_nvenc": [True, False],
        "disable_everything": [True, False],
        "disable_all_encoders": [True, False],
        "disable_encoders": [None, "ANY"],
        "enable_encoders": [None, "ANY"],
        "disable_all_decoders": [True, False],
        "disable_decoders": [None, "ANY"],
        "enable_decoders": [None, "ANY"],
        "disable_all_hardware_accelerators": [True, False],
        "disable_hardware_accelerators": [None, "ANY"],
        "enable_hardware_accelerators": [None, "ANY"],
        "disable_all_muxers": [True, False],
        "disable_muxers": [None, "ANY"],
        "enable_muxers": [None, "ANY"],
        "disable_all_demuxers": [True, False],
        "disable_demuxers": [None, "ANY"],
        "enable_demuxers": [None, "ANY"],
        "disable_all_parsers": [True, False],
        "disable_parsers": [None, "ANY"],
        "enable_parsers": [None, "ANY"],
        "disable_all_bitstream_filters": [True, False],
        "disable_bitstream_filters": [None, "ANY"],
        "enable_bitstream_filters": [None, "ANY"],
        "disable_all_protocols": [True, False],
        "disable_protocols": [None, "ANY"],
        "enable_protocols": [None, "ANY"],
        "disable_all_devices": [True, False],
        "disable_all_input_devices": [True, False],
        "disable_input_devices": [None, "ANY"],
        "enable_input_devices": [None, "ANY"],
        "disable_all_output_devices": [True, False],
        "disable_output_devices": [None, "ANY"],
        "enable_output_devices": [None, "ANY"],
        "disable_all_filters": [True, False],
        "disable_filters": [None, "ANY"],
        "enable_filters": [None, "ANY"],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "avdevice": True,
        "avcodec": True,
        "avformat": True,
        "swresample": True,
        "swscale": True,
        "postproc": True,
        "avfilter": True,
        "with_asm": True,
        "with_zlib": True,
        "with_bzip2": True,
        "with_lzma": True,
        "with_libiconv": True,
        "with_freetype": True,
        "with_openjpeg": True,
        "with_openh264": True,
        "with_opus": True,
        "with_vorbis": True,
        "with_zeromq": False,
        "with_sdl": False,
        "with_libx264": True,
        "with_libx265": True,
        "with_libvpx": True,
        "with_libmp3lame": True,
        "with_libfdk_aac": True,
        "with_libwebp": True,
        "with_ssl": "openssl",
        "with_libalsa": True,
        "with_pulse": True,
        "with_vaapi": True,
        "with_vdpau": True,
        "with_vulkan": True,
        "with_xcb": True,
        "with_appkit": True,
        "with_avfoundation": True,
        "with_coreimage": True,
        "with_audiotoolbox": True,
        "with_videotoolbox": True,
        "with_programs": True,
        "with_nvenc": True,
        "disable_everything": False,
        "disable_all_encoders": False,
        "disable_encoders": None,
        "enable_encoders": None,
        "disable_all_decoders": False,
        "disable_decoders": None,
        "enable_decoders": None,
        "disable_all_hardware_accelerators": False,
        "disable_hardware_accelerators": None,
        "enable_hardware_accelerators": None,
        "disable_all_muxers": False,
        "disable_muxers": None,
        "enable_muxers": None,
        "disable_all_demuxers": False,
        "disable_demuxers": None,
        "enable_demuxers": None,
        "disable_all_parsers": False,
        "disable_parsers": None,
        "enable_parsers": None,
        "disable_all_bitstream_filters": False,
        "disable_bitstream_filters": None,
        "enable_bitstream_filters": None,
        "disable_all_protocols": False,
        "disable_protocols": None,
        "enable_protocols": None,
        "disable_all_devices": False,
        "disable_all_input_devices": False,
        "disable_input_devices": None,
        "enable_input_devices": None,
        "disable_all_output_devices": False,
        "disable_output_devices": None,
        "enable_output_devices": None,
        "disable_all_filters": False,
        "disable_filters": None,
        "enable_filters": None,
    }

    @property
    def _settings_build(self):
        return getattr(self, "settings_build", self.settings)

    @property
    def _dependencies(self):
        return {
            "avformat": ["avcodec"],
            "avdevice": ["avcodec", "avformat"],
            "avfilter": ["avformat"],
            "with_bzip2": ["avformat"],
            "with_ssl": ["avformat"],
            "with_zlib": ["avcodec"],
            "with_lzma": ["avcodec"],
            "with_libiconv": ["avcodec"],
            "with_openjpeg": ["avcodec"],
            "with_openh264": ["avcodec"],
            "with_vorbis": ["avcodec"],
            "with_opus": ["avcodec"],
            "with_libx264": ["avcodec"],
            "with_libx265": ["avcodec"],
            "with_libvpx": ["avcodec"],
            "with_libmp3lame": ["avcodec"],
            "with_libfdk_aac": ["avcodec"],
            "with_libwebp": ["avcodec"],
            "with_freetype": ["avfilter"],
            "with_zeromq": ["avfilter", "avformat"],
            "with_libalsa": ["avdevice"],
            "with_xcb": ["avdevice"],
            "with_pulse": ["avdevice"],
            "with_sdl": ["with_programs"],
        }

    @property
    def _version_supports_vulkan(self):
        return Version(self.version) >= "4.3.0"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        if not self.settings.os in ["Linux", "FreeBSD"]:
            del self.options.with_vaapi
            del self.options.with_vdpau
            del self.options.with_vulkan
            del self.options.with_xcb
            del self.options.with_libalsa
            del self.options.with_pulse
        if self.settings.os != "Macos":
            del self.options.with_appkit
        if self.settings.os not in ["Macos", "iOS", "tvOS"]:
            del self.options.with_coreimage
            del self.options.with_audiotoolbox
            del self.options.with_videotoolbox
        if not is_apple_os(self):
            del self.options.with_avfoundation
        if not self._version_supports_vulkan:
            self.options.rm_safe("with_vulkan")

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def layout(self):
        basic_layout(self, src_folder="src")

    def requirements(self):
        if self.options.with_zlib:
            self.requires("zlib/1.2.13")
        if self.options.with_bzip2:
            self.requires("bzip2/1.0.8")
        if self.options.with_lzma:
            self.requires("xz_utils/5.4.2")
        if self.options.with_libiconv:
            self.requires("libiconv/1.17")
        if self.options.with_freetype:
            self.requires("freetype/2.13.0")
        if self.options.with_openjpeg:
            self.requires("openjpeg/2.5.0")
        if self.options.with_openh264:
            self.requires("openh264/2.3.1")
        if self.options.with_vorbis:
            self.requires("vorbis/1.3.7")
        if self.options.with_opus:
            self.requires("opus/1.3.1")
        if self.options.with_zeromq:
            self.requires("zeromq/4.3.4")
        if self.options.with_sdl:
            self.requires("sdl/2.26.5")
        if self.options.with_libx264:
            self.requires("libx264/cci.20220602")
        if self.options.with_libx265:
            self.requires("libx265/3.4")
        if self.options.with_libvpx:
            self.requires("libvpx/1.11.0")
        if self.options.with_libmp3lame:
            self.requires("libmp3lame/3.100")
        if self.options.with_libfdk_aac:
            self.requires("libfdk_aac/2.0.2")
        if self.options.with_libwebp:
            self.requires("libwebp/1.3.1")
        if self.options.with_ssl == "openssl":
            self.requires("openssl/[>=1.1 <4]")
        if self.options.get_safe("with_libalsa"):
            self.requires("libalsa/1.2.7.2")
        if self.options.get_safe("with_xcb") or self.options.get_safe("with_vaapi"):
            self.requires("xorg/system")
        if self.options.get_safe("with_pulse"):
            self.requires("pulseaudio/14.2")
        if self.options.get_safe("with_vaapi"):
            self.requires("vaapi/system")
        if self.options.get_safe("with_vdpau"):
            self.requires("vdpau/system")
        if self._version_supports_vulkan and self.options.get_safe("with_vulkan"):
            self.requires("vulkan-loader/1.3.239.0")
        if self.options.with_nvenc:
            self.requires("ffnvcodec/11.1.5.2")

    def validate(self):
        if self.options.with_ssl == "securetransport" and not is_apple_os(self):
            raise ConanInvalidConfiguration(
                "securetransport is only available on Apple")

        for dependency, features in self._dependencies.items():
            if not self.options.get_safe(dependency):
                continue
            used = False
            for feature in features:
                used = used or self.options.get_safe(feature)
            if not used:
                raise ConanInvalidConfiguration("FFmpeg '{}' option requires '{}' option to be enabled".format(
                    dependency, "' or '".join(features)))

    def build_requirements(self):
        if self.settings.arch in ("x86", "x86_64"):
            self.tool_requires("yasm/1.3.0")
        if not self.conf.get("tools.gnu:pkg_config", check_type=str):
            self.tool_requires("pkgconf/1.9.3")
        if self._settings_build.os == "Windows":
            self.win_bash = True
            if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                self.tool_requires("msys2/cci.latest")

    def source(self):
        get(self, "https://ffmpeg.org/releases/ffmpeg-6.0.tar.bz2", sha256="47d062731c9f66a78380e35a19aac77cebceccd1c7cc309b9c82343ffc430c3d", strip_root=True)

    @property
    def _target_arch(self):
        # Taken from acceptable values https://github.com/FFmpeg/FFmpeg/blob/0684e58886881a998f1a7b510d73600ff1df2b90/configure#L5010
        if str(self.settings.arch).startswith("armv8"):
            return "aarch64"
        elif self.settings.arch == "x86":
            return "i686"
        return str(self.settings.arch)

    @property
    def _target_os(self):
        if self.settings.os == "Windows":
            return "mingw32" if self.settings.compiler == "gcc" else "win32"
        elif is_apple_os(self):
            return "darwin"

        # Taken from https://github.com/FFmpeg/FFmpeg/blob/0684e58886881a998f1a7b510d73600ff1df2b90/configure#L5485
        # This is the map of Conan OS settings to FFmpeg acceptable values
        return {
            "AIX": "aix",
            "Android": "android",
            "FreeBSD": "freebsd",
            "Linux": "linux",
            "Neutrino": "qnx",
            "SunOS": "sunos",
        }.get(str(self.settings.os), "none")

    @property
    def _default_compilers(self):
        if self.settings.compiler == "gcc":
            return {"cc": "gcc", "cxx": "g++"}
        elif self.settings.compiler in ["clang", "apple-clang"]:
            return {"cc": "clang", "cxx": "clang++"}
        elif is_msvc(self):
            return {"cc": "cl.exe", "cxx": "cl.exe"}
        return {}

    def _create_toolchain(self):
        tc = AutotoolsToolchain(self)
        # Custom configure script of ffmpeg understands:
        # --prefix, --bindir, --datadir, --docdir, --incdir, --libdir, --mandir
        # Options --datadir, --docdir, --incdir, and --mandir are not injected by AutotoolsToolchain  but their default value
        # in ffmpeg script matches expected conan install layout.
        # Several options injected by AutotoolsToolchain are unknown from this configure script and must be pruned.
        # This must be done before modifying tc.configure_args, because update_configre_args currently removes
        # duplicate configuration keys, even when they have different values, such as list of encoder flags.
        # See https://github.com/conan-io/conan-center-index/issues/17140 for further information.
        tc.update_configure_args({
            "--sbindir": None,
            "--includedir": None,
            "--oldincludedir": None,
            "--datarootdir": None,
            "--build": None,
            "--host": None,
            "--target": None,
        })
        return tc

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()
        if not cross_building(self):
            env = VirtualRunEnv(self)
            env.generate(scope="build")

        def opt_enable_disable(what, v):
            return "--{}-{}".format("enable" if v else "disable", what)

        def opt_append_disable_if_set(args, what, v):
            if v:
                args.append(f"--disable-{what}")

        tc = self._create_toolchain()

        args = [
            "--pkg-config-flags=--static",
            "--disable-doc",
            opt_enable_disable("cross-compile", cross_building(self)),
            opt_enable_disable("asm", self.options.with_asm),
            # Libraries
            opt_enable_disable("shared", self.options.shared),
            opt_enable_disable("static", not self.options.shared),
            opt_enable_disable("pic", self.options.get_safe("fPIC", True)),
            # Components
            opt_enable_disable("avdevice", self.options.avdevice),
            opt_enable_disable("avcodec", self.options.avcodec),
            opt_enable_disable("avformat", self.options.avformat),
            opt_enable_disable("swresample", self.options.swresample),
            opt_enable_disable("swscale", self.options.swscale),
            opt_enable_disable("postproc", self.options.postproc),
            opt_enable_disable("avfilter", self.options.avfilter),

            # Dependencies
            opt_enable_disable("bzlib", self.options.with_bzip2),
            opt_enable_disable("zlib", self.options.with_zlib),
            opt_enable_disable("lzma", self.options.with_lzma),
            opt_enable_disable("iconv", self.options.with_libiconv),
            opt_enable_disable("libopenjpeg", self.options.with_openjpeg),
            opt_enable_disable("libopenh264", self.options.with_openh264),
            opt_enable_disable("libvorbis", self.options.with_vorbis),
            opt_enable_disable("libopus", self.options.with_opus),
            opt_enable_disable("libzmq", self.options.with_zeromq),
            opt_enable_disable("sdl2", self.options.with_sdl),
            opt_enable_disable("libx264", self.options.with_libx264),
            opt_enable_disable("libx265", self.options.with_libx265),
            opt_enable_disable("libvpx", self.options.with_libvpx),
            opt_enable_disable("libmp3lame", self.options.with_libmp3lame),
            opt_enable_disable("libfdk-aac", self.options.with_libfdk_aac),
            opt_enable_disable("libwebp", self.options.with_libwebp),
            opt_enable_disable("openssl", self.options.with_ssl == "openssl"),
            opt_enable_disable("alsa", self.options.get_safe("with_libalsa")),
            opt_enable_disable(
                "libpulse", self.options.get_safe("with_pulse")),
            opt_enable_disable("vaapi", self.options.get_safe("with_vaapi")),
            opt_enable_disable("vdpau", self.options.get_safe("with_vdpau")),
            opt_enable_disable("libxcb", self.options.get_safe("with_xcb")),
            opt_enable_disable(
                "libxcb-shm", self.options.get_safe("with_xcb")),
            opt_enable_disable(
                "libxcb-shape", self.options.get_safe("with_xcb")),
            opt_enable_disable(
                "libxcb-xfixes", self.options.get_safe("with_xcb")),
            opt_enable_disable("appkit", self.options.get_safe("with_appkit")),
            opt_enable_disable(
                "avfoundation", self.options.get_safe("with_avfoundation")),
            opt_enable_disable(
                "coreimage", self.options.get_safe("with_coreimage")),
            opt_enable_disable(
                "audiotoolbox", self.options.get_safe("with_audiotoolbox")),
            opt_enable_disable(
                "videotoolbox", self.options.get_safe("with_videotoolbox")),
            opt_enable_disable("securetransport",
                               self.options.with_ssl == "securetransport"),
            opt_enable_disable("nvenc", self.options.with_nvenc),
            "--disable-cuda",  # FIXME: CUDA support
            "--disable-cuvid",  # FIXME: CUVID support
            # Licenses
            opt_enable_disable("nonfree", self.options.with_libfdk_aac or (self.options.with_ssl and (
                self.options.with_libx264 or self.options.with_libx265 or self.options.postproc))),
            opt_enable_disable(
                "gpl", self.options.with_libx264 or self.options.with_libx265 or self.options.postproc)
        ]

        # Individual Component Options
        opt_append_disable_if_set(args, "everything", self.options.disable_everything)
        opt_append_disable_if_set(args, "encoders", self.options.disable_all_encoders)
        opt_append_disable_if_set(args, "decoders", self.options.disable_all_decoders)
        opt_append_disable_if_set(args, "hwaccels", self.options.disable_all_hardware_accelerators)
        opt_append_disable_if_set(args, "muxers", self.options.disable_all_muxers)
        opt_append_disable_if_set(args, "demuxers", self.options.disable_all_demuxers)
        opt_append_disable_if_set(args, "parsers", self.options.disable_all_parsers)
        opt_append_disable_if_set(args, "bsfs", self.options.disable_all_bitstream_filters)
        opt_append_disable_if_set(args, "protocols", self.options.disable_all_protocols)
        opt_append_disable_if_set(args, "devices", self.options.disable_all_devices)
        opt_append_disable_if_set(args, "indevs", self.options.disable_all_input_devices)
        opt_append_disable_if_set(args, "outdevs", self.options.disable_all_output_devices)
        opt_append_disable_if_set(args, "filters", self.options.disable_all_filters)

        args.extend(self._split_and_format_options_string(
            "enable-encoder", self.options.enable_encoders))
        args.extend(self._split_and_format_options_string(
            "disable-encoder", self.options.disable_encoders))
        args.extend(self._split_and_format_options_string(
            "enable-decoder", self.options.enable_decoders))
        args.extend(self._split_and_format_options_string(
            "disable-decoder", self.options.disable_decoders))
        args.extend(self._split_and_format_options_string(
            "enable-hwaccel", self.options.enable_hardware_accelerators))
        args.extend(self._split_and_format_options_string(
            "disable-hwaccel", self.options.disable_hardware_accelerators))
        args.extend(self._split_and_format_options_string(
            "enable-muxer", self.options.enable_muxers))
        args.extend(self._split_and_format_options_string(
            "disable-muxer", self.options.disable_muxers))
        args.extend(self._split_and_format_options_string(
            "enable-demuxer", self.options.enable_demuxers))
        args.extend(self._split_and_format_options_string(
            "disable-demuxer", self.options.disable_demuxers))
        args.extend(self._split_and_format_options_string(
            "enable-parser", self.options.enable_parsers))
        args.extend(self._split_and_format_options_string(
            "disable-parser", self.options.disable_parsers))
        args.extend(self._split_and_format_options_string(
            "enable-bsf", self.options.enable_bitstream_filters))
        args.extend(self._split_and_format_options_string(
            "disable-bsf", self.options.disable_bitstream_filters))
        args.extend(self._split_and_format_options_string(
            "enable-protocol", self.options.enable_protocols))
        args.extend(self._split_and_format_options_string(
            "disable-protocol", self.options.disable_protocols))
        args.extend(self._split_and_format_options_string(
            "enable-indev", self.options.enable_input_devices))
        args.extend(self._split_and_format_options_string(
            "disable-indev", self.options.disable_input_devices))
        args.extend(self._split_and_format_options_string(
            "enable-outdev", self.options.enable_output_devices))
        args.extend(self._split_and_format_options_string(
            "disable-outdev", self.options.disable_output_devices))
        args.extend(self._split_and_format_options_string(
            "enable-filter", self.options.enable_filters))
        args.extend(self._split_and_format_options_string(
            "disable-filter", self.options.disable_filters))

        if self._version_supports_vulkan:
            args.append(opt_enable_disable("vulkan", self.options.get_safe("with_vulkan")))
        if is_apple_os(self):
            # relocatable shared libs
            args.append("--install-name-dir=@rpath")
        args.append(f"--arch={self._target_arch}")
        if self.settings.build_type == "Debug":
            args.extend([
                "--disable-optimizations",
                "--disable-mmx",
                "--disable-stripping",
                "--enable-debug",
            ])
        if not self.options.with_programs:
            args.append("--disable-programs")
        # since ffmpeg"s build system ignores CC and CXX
        compilers_from_conf = self.conf.get("tools.build:compiler_executables", default={}, check_type=dict)
        buildenv_vars = VirtualBuildEnv(self).vars()
        nm = buildenv_vars.get("NM")
        if nm:
            args.append(f"--nm={unix_path(self, nm)}")
        ar = buildenv_vars.get("AR")
        if ar:
            args.append(f"--ar={unix_path(self, ar)}")
        if self.options.with_asm:
            asm = compilers_from_conf.get("asm", buildenv_vars.get("AS"))
            if asm:
                args.append(f"--as={unix_path(self, asm)}")
        strip = buildenv_vars.get("STRIP")
        if strip:
            args.append(f"--strip={unix_path(self, strip)}")
        cc = compilers_from_conf.get("c", buildenv_vars.get("CC", self._default_compilers.get("cc")))
        if cc:
            args.append(f"--cc={unix_path(self, cc)}")
        cxx = compilers_from_conf.get("cpp", buildenv_vars.get("CXX", self._default_compilers.get("cxx")))
        if cxx:
            args.append(f"--cxx={unix_path(self, cxx)}")
        ld = buildenv_vars.get("LD")
        if ld:
            args.append(f"--ld={unix_path(self, ld)}")
        ranlib = buildenv_vars.get("RANLIB")
        if ranlib:
            args.append(f"--ranlib={unix_path(self, ranlib)}")
        # for some reason pkgconf from conan can't find .pc files on Linux in the context of ffmpeg configure...
        if self._settings_build.os != "Linux":
            pkg_config = self.conf.get("tools.gnu:pkg_config", default=buildenv_vars.get("PKG_CONFIG"), check_type=str)
            if pkg_config:
                args.append(f"--pkg-config={unix_path(self, pkg_config)}")
        if is_msvc(self):
            args.append("--toolchain=msvc")
            if not check_min_vs(self, "190", raise_invalid=False):
                # Visual Studio 2013 (and earlier) doesn't support "inline" keyword for C (only for C++)
                tc.extra_defines.append("inline=__inline")
        if cross_building(self):
            args.append(f"--target-os={self._target_os}")
            if is_apple_os(self) and self.options.with_audiotoolbox:
                args.append("--disable-outdev=audiotoolbox")

        if tc.cflags:
            args.append("--extra-cflags={}".format(" ".join(tc.cflags)))
        if tc.ldflags:
            args.append("--extra-ldflags={}".format(" ".join(tc.ldflags)))
        tc.configure_args.extend(args)
        tc.generate()

        if is_msvc(self):
            # Custom AutotoolsDeps for cl like compilers
            # workaround for https://github.com/conan-io/conan/issues/12784
            includedirs = []
            defines = []
            libs = []
            libdirs = []
            linkflags = []
            cxxflags = []
            cflags = []
            for dependency in self.dependencies.values():
                deps_cpp_info = dependency.cpp_info.aggregated_components()
                includedirs.extend(deps_cpp_info.includedirs)
                defines.extend(deps_cpp_info.defines)
                libs.extend(deps_cpp_info.libs + deps_cpp_info.system_libs)
                libdirs.extend(deps_cpp_info.libdirs)
                linkflags.extend(deps_cpp_info.sharedlinkflags + deps_cpp_info.exelinkflags)
                cxxflags.extend(deps_cpp_info.cxxflags)
                cflags.extend(deps_cpp_info.cflags)

            env = Environment()
            env.append("CPPFLAGS", [f"-I{unix_path(self, p)}" for p in includedirs] + [f"-D{d}" for d in defines])
            env.append("_LINK_", [lib if lib.endswith(".lib") else f"{lib}.lib" for lib in libs])
            env.append("LDFLAGS", [f"-LIBPATH:{unix_path(self, p)}" for p in libdirs] + linkflags)
            env.append("CXXFLAGS", cxxflags)
            env.append("CFLAGS", cflags)
            env.vars(self).save_script("conanautotoolsdeps_cl_workaround")
        else:
            deps = AutotoolsDeps(self)
            deps.generate()

        deps = PkgConfigDeps(self)
        deps.generate()

    def _split_and_format_options_string(self, flag_name, options_list):
        if not options_list:
            return []

        def _format_options_list_item(flag_name, options_item):
            return f"--{flag_name}={options_item}"

        def _split_options_string(options_string):
            return list(filter(None, "".join(options_string.split()).split(",")))

        options_string = str(options_list)
        return [_format_options_list_item(flag_name, item) for item in _split_options_string(options_string)]

    def build(self):
        if self.options.with_libx264:
            # ffmepg expects libx264.pc instead of x264.pc
            with chdir(self, self.generators_folder):
                shutil.copy("x264.pc", "libx264.pc")
        autotools = Autotools(self)
        autotools.configure()
        autotools.make()

    def package(self):
        copy(self, "LICENSE.md", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        autotools = Autotools(self)
        autotools.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "share"))
        if is_msvc(self):
            if self.options.shared:
                # ffmpeg created `.lib` files in the `/bin` folder
                for fn in os.listdir(os.path.join(self.package_folder, "bin")):
                    if fn.endswith(".lib"):
                        rename(self, os.path.join(self.package_folder, "bin", fn),
                               os.path.join(self.package_folder, "lib", fn))
                rm(self, "*.def", os.path.join(self.package_folder, "lib"))
            else:
                # ffmpeg produces `.a` files that are actually `.lib` files
                with chdir(self, os.path.join(self.package_folder, "lib")):
                    for lib in glob.glob("*.a"):
                        rename(self, lib, lib[3:-2] + ".lib")

    def _read_component_version(self, component_name):
        # since 5.1, major version may be defined in version_major.h instead of version.h
        component_folder = os.path.join(self.package_folder, "include", f"lib{component_name}")
        version_file_name = os.path.join(component_folder, "version.h")
        version_major_file_name = os.path.join(component_folder, "version_major.h")
        pattern = f"define LIB{component_name.upper()}_VERSION_(MAJOR|MINOR|MICRO)[ \t]+(\\d+)"
        version = dict()
        for file in (version_file_name, version_major_file_name):
            if os.path.isfile(file):
                with open(file, "r", encoding="utf-8") as f:
                    for line in f:
                        match = re.search(pattern, line)
                        if match:
                            version[match[1]] = match[2]
        if "MAJOR" in version and "MINOR" in version and "MICRO" in version:
            return f"{version['MAJOR']}.{version['MINOR']}.{version['MICRO']}"
        return None

    def _set_component_version(self, component_name):
        version = self._read_component_version(component_name)
        if version is not None:
            self.cpp_info.components[component_name].set_property("component_version", version)
            # TODO: to remove once support of conan v1 dropped
            self.cpp_info.components[component_name].version = version
        else:
            self.output.warning(f"cannot determine version of lib{component_name} packaged with ffmpeg!")

    def package_info(self):
        if self.options.with_programs:
            if self.options.with_sdl:
                self.cpp_info.components["programs"].requires = [
                    "sdl::libsdl2"]

        if self.options.avdevice:
            self.cpp_info.components["avdevice"].set_property(
                "pkg_config_name", "libavdevice")
            self.cpp_info.components["avdevice"].libs = ["avdevice"]
            self.cpp_info.components["avdevice"].requires = ["avutil"]
            if self.options.avfilter:
                self.cpp_info.components["avdevice"].requires.append(
                    "avfilter")
            if self.options.swscale:
                self.cpp_info.components["avdevice"].requires.append("swscale")
            if self.options.avformat:
                self.cpp_info.components["avdevice"].requires.append(
                    "avformat")
            if self.options.avcodec:
                self.cpp_info.components["avdevice"].requires.append("avcodec")
            if self.options.swresample:
                self.cpp_info.components["avdevice"].requires.append(
                    "swresample")
            if self.options.postproc:
                self.cpp_info.components["avdevice"].requires.append(
                    "postproc")
            self._set_component_version("avdevice")

        if self.options.avfilter:
            self.cpp_info.components["avfilter"].set_property(
                "pkg_config_name", "libavfilter")
            self.cpp_info.components["avfilter"].libs = ["avfilter"]
            self.cpp_info.components["avfilter"].requires = ["avutil"]
            if self.options.swscale:
                self.cpp_info.components["avfilter"].requires.append("swscale")
            if self.options.avformat:
                self.cpp_info.components["avfilter"].requires.append(
                    "avformat")
            if self.options.avcodec:
                self.cpp_info.components["avfilter"].requires.append("avcodec")
            if self.options.swresample:
                self.cpp_info.components["avfilter"].requires.append(
                    "swresample")
            if self.options.postproc:
                self.cpp_info.components["avfilter"].requires.append(
                    "postproc")
            self._set_component_version("avfilter")

        if self.options.avformat:
            self.cpp_info.components["avformat"].set_property(
                "pkg_config_name", "libavformat")
            self.cpp_info.components["avformat"].libs = ["avformat"]
            self.cpp_info.components["avformat"].requires = ["avutil"]
            if self.options.avcodec:
                self.cpp_info.components["avformat"].requires.append("avcodec")
            if self.options.swresample:
                self.cpp_info.components["avformat"].requires.append(
                    "swresample")
            self._set_component_version("avformat")

        if self.options.avcodec:
            self.cpp_info.components["avcodec"].set_property(
                "pkg_config_name", "libavcodec")
            self.cpp_info.components["avcodec"].libs = ["avcodec"]
            self.cpp_info.components["avcodec"].requires = ["avutil"]
            if self.options.swresample:
                self.cpp_info.components["avcodec"].requires.append(
                    "swresample")
            self._set_component_version("avcodec")

        if self.options.swscale:
            self.cpp_info.components["swscale"].set_property(
                "pkg_config_name", "libswscale")
            self.cpp_info.components["swscale"].libs = ["swscale"]
            self.cpp_info.components["swscale"].requires = ["avutil"]
            self._set_component_version("swscale")

        if self.options.swresample:
            self.cpp_info.components["swresample"].set_property(
                "pkg_config_name", "libswresample")
            self.cpp_info.components["swresample"].libs = ["swresample"]
            self.cpp_info.components["swresample"].requires = ["avutil"]
            self._set_component_version("swresample")

        if self.options.postproc:
            self.cpp_info.components["postproc"].set_property(
                "pkg_config_name", "libpostproc")
            self.cpp_info.components["postproc"].libs = ["postproc"]
            self.cpp_info.components["postproc"].requires = ["avutil"]
            self._set_component_version("postproc")

        self.cpp_info.components["avutil"].set_property(
            "pkg_config_name", "libavutil")
        self.cpp_info.components["avutil"].libs = ["avutil"]
        self._set_component_version("avutil")

        if self.settings.os in ("FreeBSD", "Linux"):
            self.cpp_info.components["avutil"].system_libs = [
                "pthread", "m", "dl"]
            if self.options.swresample:
                self.cpp_info.components["swresample"].system_libs = ["m"]
            if self.options.swscale:
                self.cpp_info.components["swscale"].system_libs = ["m"]
            if self.options.postproc:
                self.cpp_info.components["postproc"].system_libs = ["m"]
            if self.options.get_safe("fPIC"):
                if self.settings.compiler in ("gcc", "clang"):
                    # https://trac.ffmpeg.org/ticket/1713
                    # https://ffmpeg.org/platform.html#Advanced-linking-configuration
                    # https://ffmpeg.org/pipermail/libav-user/2014-December/007719.html
                    self.cpp_info.components["avcodec"].exelinkflags.append(
                        "-Wl,-Bsymbolic")
                    self.cpp_info.components["avcodec"].sharedlinkflags.append(
                        "-Wl,-Bsymbolic")
            if self.options.avformat:
                self.cpp_info.components["avformat"].system_libs = ["m"]
            if self.options.avfilter:
                self.cpp_info.components["avfilter"].system_libs = [
                    "m", "pthread"]
            if self.options.avdevice:
                self.cpp_info.components["avdevice"].system_libs = ["m"]
        elif self.settings.os == "Windows":
            if self.options.avcodec:
                self.cpp_info.components["avcodec"].system_libs = ["mfplat", "mfuuid", "strmiids"]
            if self.options.avdevice:
                self.cpp_info.components["avdevice"].system_libs = [
                    "ole32", "psapi", "strmiids", "uuid", "oleaut32", "shlwapi", "gdi32", "vfw32"]
            self.cpp_info.components["avutil"].system_libs = [
                "user32", "bcrypt"]
            self.cpp_info.components["avformat"].system_libs = ["secur32"]
        elif is_apple_os(self):
            if self.options.avdevice:
                self.cpp_info.components["avdevice"].frameworks = [
                    "CoreFoundation", "Foundation", "CoreGraphics"]
            if self.options.avfilter:
                self.cpp_info.components["avfilter"].frameworks = [
                    "CoreGraphics"]
            if self.options.avcodec:
                self.cpp_info.components["avcodec"].frameworks = [
                    "CoreFoundation", "CoreVideo", "CoreMedia"]
            if self.settings.os == "Macos":
                if self.options.avdevice:
                    self.cpp_info.components["avdevice"].frameworks.append(
                        "OpenGL")
                if self.options.avfilter:
                    self.cpp_info.components["avfilter"].frameworks.append(
                        "OpenGL")

        if self.options.avdevice:
            if self.options.get_safe("with_libalsa"):
                self.cpp_info.components["avdevice"].requires.append(
                    "libalsa::libalsa")
            if self.options.get_safe("with_xcb"):
                self.cpp_info.components["avdevice"].requires.append(
                    "xorg::xcb")
            if self.options.get_safe("with_pulse"):
                self.cpp_info.components["avdevice"].requires.append(
                    "pulseaudio::pulseaudio")
            if self.options.get_safe("with_appkit"):
                self.cpp_info.components["avdevice"].frameworks.append(
                    "AppKit")
            if self.options.get_safe("with_avfoundation"):
                self.cpp_info.components["avdevice"].frameworks.append(
                    "AVFoundation")
            if self.options.get_safe("with_audiotoolbox"):
                self.cpp_info.components["avdevice"].frameworks.append(
                    "CoreAudio")

        if self.options.avcodec:
            if self.options.with_zlib:
                self.cpp_info.components["avcodec"].requires.append(
                    "zlib::zlib")
            if self.options.with_lzma:
                self.cpp_info.components["avcodec"].requires.append(
                    "xz_utils::xz_utils")
            if self.options.with_libiconv:
                self.cpp_info.components["avcodec"].requires.append(
                    "libiconv::libiconv")
            if self.options.with_openjpeg:
                self.cpp_info.components["avcodec"].requires.append(
                    "openjpeg::openjpeg")
            if self.options.with_openh264:
                self.cpp_info.components["avcodec"].requires.append(
                    "openh264::openh264")
            if self.options.with_vorbis:
                self.cpp_info.components["avcodec"].requires.append(
                    "vorbis::vorbis")
            if self.options.with_opus:
                self.cpp_info.components["avcodec"].requires.append(
                    "opus::opus")
            if self.options.with_libx264:
                self.cpp_info.components["avcodec"].requires.append(
                    "libx264::libx264")
            if self.options.with_libx265:
                self.cpp_info.components["avcodec"].requires.append(
                    "libx265::libx265")
            if self.options.with_libvpx:
                self.cpp_info.components["avcodec"].requires.append(
                    "libvpx::libvpx")
            if self.options.with_libmp3lame:
                self.cpp_info.components["avcodec"].requires.append(
                    "libmp3lame::libmp3lame")
            if self.options.with_libfdk_aac:
                self.cpp_info.components["avcodec"].requires.append(
                    "libfdk_aac::libfdk_aac")
            if self.options.with_libwebp:
                self.cpp_info.components["avcodec"].requires.append(
                    "libwebp::libwebp")
            if self.options.with_nvenc:
                self.cpp_info.components["avcodec"].requires.append(
                    "ffnvcodec::ffnvcodec")
            if self.options.get_safe("with_audiotoolbox"):
                self.cpp_info.components["avcodec"].frameworks.append(
                    "AudioToolbox")
            if self.options.get_safe("with_videotoolbox"):
                self.cpp_info.components["avcodec"].frameworks.append(
                    "VideoToolbox")

        if self.options.avformat:
            if self.options.with_bzip2:
                self.cpp_info.components["avformat"].requires.append(
                    "bzip2::bzip2")
            if self.options.with_zeromq:
                self.cpp_info.components["avformat"].requires.append(
                    "zeromq::libzmq")
            if self.options.with_ssl == "openssl":
                self.cpp_info.components["avformat"].requires.append(
                    "openssl::ssl")
            elif self.options.with_ssl == "securetransport":
                self.cpp_info.components["avformat"].frameworks.append(
                    "Security")

        if self.options.avfilter:
            if self.options.with_freetype:
                self.cpp_info.components["avfilter"].requires.append(
                    "freetype::freetype")
            if self.options.with_zeromq:
                self.cpp_info.components["avfilter"].requires.append(
                    "zeromq::libzmq")
            if self.options.get_safe("with_appkit"):
                self.cpp_info.components["avfilter"].frameworks.append(
                    "AppKit")
            if self.options.get_safe("with_coreimage"):
                self.cpp_info.components["avfilter"].frameworks.append(
                    "CoreImage")
            if Version(self.version) >= "5.0" and is_apple_os(self):
                self.cpp_info.components["avfilter"].frameworks.append("Metal")

        if self.options.get_safe("with_vaapi"):
            self.cpp_info.components["avutil"].requires.extend(
                ["vaapi::vaapi", "xorg::x11"])

        if self.options.get_safe("with_vdpau"):
            self.cpp_info.components["avutil"].requires.append("vdpau::vdpau")

        if self._version_supports_vulkan and self.options.get_safe("with_vulkan"):
            self.cpp_info.components["avutil"].requires.append(
                "vulkan-loader::vulkan-loader")
