#!/bin/sh

conan profile detect
conan export scripts/conan_ffnvcodec.py
conan export scripts/conan_ffmpeg.py
conan install . --output-folder=conan --build=missing
