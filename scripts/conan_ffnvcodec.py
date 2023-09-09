from conan import ConanFile
from conan.tools.files import get, copy
from conan.tools.gnu import PkgConfigDeps

class FFNVCodec(ConanFile):
    name = "ffnvcodec"
    version = "11.1.5.2"
    no_copy_source = True

    def source(self):
        get(self, "https://github.com/FFmpeg/nv-codec-headers/releases/download/n11.1.5.2/nv-codec-headers-11.1.5.2.tar.gz", sha256="1442e3159e7311dd71f8fca86e615f51609998939b6a6d405d6683d8eb3af6ee", strip_root=True)

    def package(self):
        copy(self, "*.h", self.source_folder, self.package_folder)

    def pacakge_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
