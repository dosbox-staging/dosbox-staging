#!/bin/bash

# For redistributable bundle, run ./configure with following args:
# --enable-sdl-static
# --disable-opus-cdda
# --disable-screenshots
# --disable-network

set -x

version=$(git describe --abbrev=5)
#opt=/usr/local/opt
dst=dist/dosbox-staging.app/Contents/

# Generate icon
make -C contrib/icons/ dosbox-staging.icns

# Prepare content
install -d "$dst/MacOS/"
install -d "$dst/Resources/"
install -d "$dst/SharedSupport/"

install        "src/dosbox"                                "$dst/MacOS/"
# install        "$opt/libpng/lib/libpng16.16.dylib"         "$dst/MacOS/"
# install        "$opt/opusfile/lib/libopusfile.0.dylib"     "$dst/MacOS/"
# install        "$opt/sdl2/lib/libSDL2-2.0.0.dylib"         "$dst/MacOS/"
# install        "$opt/sdl2_net/lib/libSDL2_net-2.0.0.dylib" "$dst/MacOS/"
install -m 644 "contrib/macos/Info.plist.template"           "$dst/Info.plist"
install -m 644 "contrib/macos/PkgInfo"                     "$dst/PkgInfo"
install -m 644 "contrib/icons/dosbox-staging.icns"         "$dst/Resources/"
install -m 644 "docs/README.template"                      "$dst/SharedSupport/README"
install -m 644 "COPYING"                                   "$dst/SharedSupport/COPYING"
install -m 644 "README"                                    "$dst/SharedSupport/manual.txt"
install -m 644 "docs/README.video"                         "$dst/SharedSupport/video.txt"
# install -d     "$dst/Contents/_CodeSignature/" # ?

# Fill README template file
sed -i -e "s|%VERSION%|$version|"                     "$dst/Info.plist"
sed -i -e "s|%GIT_COMMIT%|$GITHUB_SHA|"               "$dst/SharedSupport/README"
sed -i -e "s|%GIT_BRANCH%|${GITHUB_REF#refs/heads/}|" "$dst/SharedSupport/README"
sed -i -e "s|%GITHUB_REPO%|$GITHUB_REPOSITORY|"       "$dst/SharedSupport/README"

ln -s /Applications dist/

hdiutil create \
    -volname "dosbox-staging" \
    -srcfolder dist \
    -ov -format UDZO "dosbox-staging $version.dmg"
