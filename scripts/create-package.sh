#!/bin/sh

set -e

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2020-2021  Sherman Perry

usage()
{
    printf "%s\n" "\
    Usage: -p <platform> [-h -c <commit> -b <branch> -r <repo> -v <version> -f] BUILD_DIR PACKAGE_DIR
    Where:
        -h          : Show this help message
        -p          : Buld platform. Can be one of linux, macos, msys2, msvc
        -c          : Git commit
        -b          : Git branch
        -r          : Git repository
        -v          : dosbox-staging version
        -f          : force creation if PACKAGE_DIR is not empty
        BUILD_DIR   : Meson build directory
        PACKAGE_DIR : Package directory
    
    Note: On macos, '-v' must be set. On msvc, the environment variable VC_REDIST_DIR must be set."
}

create_parent_dir()
{
    path=$1
    dir=$(dirname "$path")
    if [ "$dir" != "." ]; then
        case $platform in
             msvc) mkdir -p "$dir" ;;
             macos) install -d "${dir}/"
        esac
    fi
}

install_file()
{
    src=$1
    dest=$2
    case $platform in
        linux|msys2) install -DT -m 644 "$src" "$dest" ;;
        msvc) create_parent_dir "$dest" && cp "$src" "$dest" ;;
        macos) create_parent_dir "$dest" && install -m 644 "$src" "$dest" ;;
    esac
}

install_doc()
{
    # Install common documentation files
    case $platform in
        linux)
            install_file docs/README.template "${pkg_dir}/README"
            install_file COPYING              "${pkg_dir}/COPYING"
            install_file README               "${pkg_dir}/doc/manual.txt"
            install_file docs/dosbox.1        "${pkg_dir}/man/dosbox.1"
            readme_tmpl="${pkg_dir}/README"
            ;;
        macos)
            install_file docs/README.template "${macos_dst_dir}/SharedSupport/README"
            install_file COPYING              "${macos_dst_dir}/SharedSupport/COPYING"
            install_file README               "${macos_dst_dir}/SharedSupport/manual.txt"
            install_file docs/README.video    "${macos_dst_dir}/SharedSupport/video.txt"
            readme_tmpl="${macos_dst_dir}/SharedSupport/README"
            ;;
        msys2|msvc)
            install_file COPYING              "${pkg_dir}/COPYING.txt"
            install_file docs/README.template "${pkg_dir}/README.txt"
            install_file docs/README.video    "${pkg_dir}/doc/video.txt"
            install_file README               "${pkg_dir}/doc/manual.txt"
            readme_tmpl="${pkg_dir}/README.txt"
            ;;
    esac
    # Fill template variables in README.template
    if [ -n "$git_commit" ]; then 
        sed -i -e "s|%GIT_COMMIT%|$git_commit|" "$readme_tmpl"
    fi
    if [ -n "$git_branch" ]; then 
        sed -i -e "s|%GIT_BRANCH%|$git_branch|" "$readme_tmpl"
    fi
    if [ -n "$git_repo" ]; then
        sed -i -e "s|%GITHUB_REPO%|$git_repo|"  "$readme_tmpl"
    fi
}

install_translation()
{
    lng_dir=${pkg_dir}/translations
    if [ "$platform" = "macos" ]; then
        lng_dir=${macos_dst_dir}/Resources/translations
    fi
    # Prepare translation files
    #
    # Note:
    #   We conciously drop the dialect postfix because no dialects are available.
    #   (US was the default DOS dialect and therefore is the default for 'en').
    #   Dialect translations will be added if/when they're available.
    #
    for source in $(find contrib/translations -name '*.lng'); do
        target=$(basename "$source" | tr '[:upper:]' '[:lower:]')
        install_file "$source" "$lng_dir/${target#??_}"
    done
}

pkg_linux()
{
    # Print shared object dependencies
    ldd "${build_dir}/dosbox"
    install -DT "${build_dir}/dosbox" "${pkg_dir}/dosbox"

    install -DT contrib/linux/dosbox-staging.desktop "${pkg_dir}/desktop/dosbox-staging.desktop"
    DESTDIR="$(realpath "$pkg_dir")" make -C contrib/icons/ install datadir=
}

pkg_macos()
{
    # Note, this script assumes macos builds have x86_64 and ARM64 subdirectories

    # Print shared object dependencies
    otool -L "${build_dir}/dosbox-arm64/dosbox"
    python3 scripts/verify-macos-dylibs.py "${build_dir}/dosbox-arm64/dosbox"

    # Create universal binary from both architectures
    mkdir dosbox-universal
    lipo dosbox-x86_64/dosbox dosbox-arm64/dosbox -create -output dosbox-universal/dosbox

    # Generate icon
    make -C contrib/icons/ dosbox-staging.icns

    install -d   "${macos_dst_dir}/MacOS/"
    install      dosbox-universal/dosbox           "${macos_dst_dir}/MacOS/"
    install_file contrib/macos/Info.plist.template "${macos_dst_dir}/Info.plist"
    install_file contrib/macos/PkgInfo             "${macos_dst_dir}/PkgInfo"
    install_file contrib/icons/dosbox-staging.icns "${macos_dst_dir}/Resources/"

    sed -i -e "s|%VERSION%|${dbox_version}|"       "${macos_dst_dir}/Info.plist"
}

pkg_msys2()
{
    install -DT "${build_dir}/dosbox.exe" "${pkg_dir}/dosbox.exe"

    # Discover and copy required dll files
    ntldd -R "${pkg_dir}/dosbox.exe" \
        | sed -e 's/^[[:blank:]]*//g' \
        | cut -d ' ' -f 3 \
        | grep -E -i '(mingw|clang)(32|64)' \
        | sed -e 's|\\|/|g' \
        | xargs cp --target-directory="${pkg_dir}/"
}

pkg_msvc()
{
    # Get the release dir name from $build_dir
    release_dir=$(basename "${build_dir}")

    # Copy binary
    cp "${build_dir}/dosbox.exe"  "${pkg_dir}/dosbox.exe"

    # Copy dll files
    cp "${build_dir}"/*.dll                  "${pkg_dir}/"
    cp "src/libs/zmbv/${release_dir}"/*.dll  "${pkg_dir}/"

    # Copy VC redistributable files
    cp docs/vc_redist.txt                  "${pkg_dir}/doc/vc_redist.txt"
    cp "$VC_REDIST_DIR/msvcp140.dll"       "${pkg_dir}/"
    cp "$VC_REDIST_DIR/vcruntime140.dll"   "${pkg_dir}/"
    cp "$VC_REDIST_DIR/vcruntime140_1.dll" "${pkg_dir}/" || true # might be missing, depending on arch
}

# Get GitHub CI environment variables if available. The CLI options
# '-c', '-b', '-r' will override these if set.
git_commit=$GITHUB_SHA
git_branch=${GITHUB_REF#refs/heads/}
git_repo=$GITHUB_REPOSITORY

while getopts 'p:c:b:r:v:hf' c
do
    case $c in
        h) print_usage="true" ;;
        f) force_pkg="true" ;;
        p) platform=$OPTARG ;;
        c) git_commit=$OPTARG ;;
        b) git_branch=$OPTARG ;;
        r) git_repo=$OPTARG ;;
        v) dbox_version=$OPTARG ;;
        *) true ;; # keep shellcheck happy
    esac
done

shift "$((OPTIND - 1))"

build_dir=$1
pkg_dir=$2

if [ "$print_usage" = "true" ]; then
    usage
    exit 0
fi

p=$platform
case $p in
    linux|macos|msys2|msvc) true ;;
    *) platform="unsupported" ;;
esac

if [ "$platform" = "unsupported" ]; then
    echo "Platform not set or unsupported"
    usage
    exit 1
fi

if [ ! -d "$build_dir" ]; then
    echo "Build directory not set, or does not exist"
    usage
    exit 1
fi

if [ -z "$pkg_dir" ]; then
    echo "Package directory not set"
    usage
    exit 1
fi

if [ "$platform" = "macos" ]; then 
    if [ -z "$dbox_version" ]; then
        echo "Dosbox version required on MacOS"
        usage
        exit 1
    fi
    macos_dst_dir=${pkg_dir}/dist/dosbox-staging.app/Contents
fi

if [ "$platform" = "msvc" ] && [ -z "$VC_REDIST_DIR" ]; then
    echo "VC_REDIST_DIR environment variable not set"
    usage
    exit 1
fi

if [ -f "$pkg_dir" ]; then
    echo "PACKAGE_DIR must not be a file"
    usage
    exit 1
fi

mkdir -p "$pkg_dir"

if [ -z "$(find ${pkg_dir} -maxdepth 0 -empty)" ] && [ "$force_pkg" != "true" ]; then
    echo "PACKAGE_DIR must be empty. Use '-f' to foce creation anyway"
    usage
    exit 1
fi

set -x

install_doc
install_translation

case $platform in
    linux) pkg_linux ;;
    macos) pkg_macos ;;
    msys2) pkg_msys2 ;;
    msvc)  pkg_msvc  ;;
    *)     echo "Oops."; usage; exit 1 ;;
esac
