#!/bin/sh

# SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
# SPDX-License-Identifier: GPL-2.0-or-later

set -e

usage()
{
    printf "%s\n" "\
    Usage: -p <platform> [-h -c <commit> -b <branch> -r <repo> -v <version> -f] BUILD_DIR PACKAGE_DIR
    Where:
        -h          : Show this help message
        -p          : Build platform. Can be one of windows, macos, linux
        -c          : Git commit
        -b          : Git branch
        -r          : Git repository
        -v          : DOSBox Staging version
        -f          : Force creation if PACKAGE_DIR is not empty
        BUILD_DIR   : Build directory
        PACKAGE_DIR : Package directory

    Notes: 
      - On 'macos', '-v' must be set.
      - On 'windows', the VC_REDIST_DIR env var must be set."
}

create_parent_dir()
{
    path=$1
    dir=$(dirname "$path")
    if [ "$dir" != "." ]; then
        case $platform in
             windows) mkdir -p "$dir" ;;
             macos) install -d "${dir}/"
        esac
    fi
}

install_file()
{
    src=$1
    dest=$2
    case $platform in
        linux)   install -DT -m 644 "$src" "$dest" ;;
        windows) create_parent_dir "$dest" && cp "$src" "$dest" ;;
        macos)   create_parent_dir "$dest" && install -m 644 "$src" "$dest" ;;
    esac
}

install_doc()
{
    # Install common documentation files
    case $platform in
        linux)
            for filename in ${build_dir}/doc/licenses/*; do
            	install_file "${filename}" "${pkg_dir}/doc/licenses/$(basename ${filename})"
            done
            install_file "${build_dir}/doc/LICENSE" "${pkg_dir}/LICENSE"
            install_file docs/dosbox.1              "${pkg_dir}/man/dosbox.1"
            install_file docs/README.template       "${pkg_dir}/README"
            readme_tmpl="${pkg_dir}/README"
            ;;
        macos)
            for filename in ${build_dir}/licenses/*; do
            	install_file "${filename}" "${macos_content_dir}/doc/licenses/$(basename ${filename})"
            done
            install_file "${build_dir}/LICENSE" "${macos_content_dir}/SharedSupport/LICENSE"
            install_file docs/README.template       "${macos_content_dir}/SharedSupport/README"
            readme_tmpl="${macos_content_dir}/SharedSupport/README"
            ;;
        windows)
            for filename in ${build_dir}/../doc/licenses/*; do
            	install_file "${filename}" "${pkg_dir}/doc/licenses/$(basename ${filename})"
            done
            install_file "${build_dir}/../doc/LICENSE" "${pkg_dir}/LICENSE.txt"
            install_file docs/README.template       "${pkg_dir}/README.txt"
            readme_tmpl="${pkg_dir}/README.txt"
            ;;
    esac
    # Fill template variables in README.template
    if [[ "$git_branch" == "refs/tags/"* ]] && [[ "$git_branch" != *"-"* ]]; then
        version_tag=`echo $git_branch | awk '{print substr($0,11);exit}'`
        package_information="release $version_tag"
    elif [[ "$git_branch" == "release/"* ]]; then
        version_tag=`git describe --tags | cut -f1 -d"-"`
        package_information="release $version_tag"
    elif [ -n "$git_branch" ] && [ -n "$git_commit" ]; then
        package_information="a development branch named $git_branch with commit $git_commit"
    else
        package_information="a development branch"
    fi

    if [ -n "$package_information" ]; then
        if [ "$(uname)" = "Darwin" ]; then
            sed -i '' -e "s|%PACKAGE_INFORMATION%|$package_information|" "$readme_tmpl"
        else
            sed -i -e "s|%PACKAGE_INFORMATION%|$package_information|" "$readme_tmpl"
        fi
    fi

    if [ -n "$git_repo" ]; then
        if [ "$(uname)" = "Darwin" ]; then
            sed -i '' -e "s|%GITHUB_REPO%|$git_repo|" "$readme_tmpl"
        else
            sed -i -e "s|%GITHUB_REPO%|$git_repo|" "$readme_tmpl"
        fi
    fi
}

install_resources()
{
    case $platform in
    macos)
        local src_dir=${build_dir}/../Resources
        local dest_dir=${macos_content_dir}/Resources
        ;;
    windows)
        local src_dir=${build_dir}/../Resources
        local dest_dir=${pkg_dir}/resources
        ;;
    *)
        local src_dir=${build_dir}/resources
        local dest_dir=${pkg_dir}/resources
        ;;
    esac

    find $src_dir -type f |
        while IFS= read -r src; do
            install_file "$src" "$dest_dir/${src#*$src_dir/}"
        done
}

pkg_linux()
{
    # Print shared object dependencies
    # ldd crashes with a malloc error on the s390x platform, so always pass
    ldd "${build_dir}/dosbox" || true
    install -DT "${build_dir}/dosbox" "${pkg_dir}/dosbox"

    install -DT extras/linux/org.dosbox_staging.dosbox_staging.desktop "${pkg_dir}/desktop/org.dosbox_staging.dosbox_staging.desktop"

    install -DT extras/linux/install-icons.sh "${pkg_dir}/install-icons.sh"

    DESTDIR="$(realpath "$pkg_dir")" make -C extras/icons/ install datadir=
}

pkg_macos()
{
    # Note, this script assumes macos builds have x86_64 and arm64 subdirectories

    # Create universal binary from both architectures
    mkdir dosbox-universal
    lipo dosbox-x86_64/dosbox dosbox-arm64/dosbox -create -output dosbox-universal/dosbox
    lipo dosbox-x86_64/crashpad_handler dosbox-arm64/crashpad_handler -create -output dosbox-universal/crashpad_handler

    install -d   "${macos_content_dir}/MacOS/"
    install      dosbox-universal/dosbox                "${macos_content_dir}/MacOS/"
    install      dosbox-universal/crashpad_handler      "${macos_content_dir}/MacOS/"
    install_file extras/macos/Info.plist.template       "${macos_content_dir}/Info.plist"
    install_file extras/macos/PkgInfo                   "${macos_content_dir}/PkgInfo"
    install_file extras/icons/macos/dosbox-staging.icns "${macos_content_dir}/Resources/"

    sed -i '' -e "s|%VERSION%|${dbox_version}|"       "${macos_content_dir}/Info.plist"

	# Install start commands
	start_command="Start DOSBox Staging.command"
	start_logging_command="Start DOSBox Staging (logging).command"
	install -m 755 "extras/macos/${start_command}"         "${macos_dist_dir}/${start_command}"
	install -m 755 "extras/macos/${start_logging_command}" "${macos_dist_dir}/${start_logging_command}"

	# Hide command extensions in Finder
	file_attr="00 00 00 00 00 00 00 00 00 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
	xattr -x -w com.apple.FinderInfo "$file_attr" "${macos_dist_dir}/${start_command}"
	xattr -x -w com.apple.FinderInfo "$file_attr" "${macos_dist_dir}/${start_logging_command}"

	# Set up visual appearance of the root folder of the DMG image
	install_file extras/macos/background/background.tiff "${macos_dist_dir}/.hidden/background.tiff"
	install_file extras/macos/DS_Store "${macos_dist_dir}/.DS_Store"
}

pkg_windows()
{
    # Get the release dir name from $build_dir
    release_dir=$(basename -- "$(dirname -- "${build_dir}")")/$(basename -- "${build_dir}")

    # Copy binary
    cp "${build_dir}/dosbox.exe"  "${pkg_dir}/dosbox.exe"

    # Copy dll files
    cp "${build_dir}"/*.dll                  "${pkg_dir}/"

    # Copy MSVC C++ redistributable files
    cp docs/vc_redist.txt                    "${pkg_dir}/doc/vc_redist.txt"
    cp "$VC_REDIST_DIR"/*.dll                "${pkg_dir}/"
}

# Get GitHub CI environment variables if available. The CLI options
# '-c', '-b', '-r' will override these if set.
if [ -n "${GITHUB_REPOSITORY:-}" ]; then
    git_commit=`echo ${GITHUB_SHA} | awk '{print substr($0,1,9);exit}'`
    git_branch=${GITHUB_REF#refs/heads/}
    git_repo=${GITHUB_REPOSITORY}
else
    git_commit=$(git rev-parse --short HEAD || echo '')
    git_branch=$(git rev-parse --abbrev-ref HEAD || echo '')
    git_repo=$(basename "$(git rev-parse --show-toplevel)" || echo '')
fi

print_usage="false"
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
    linux|macos|windows) true ;;
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
        echo "DOSBox Staging version required on macOS"
        usage
        exit 1
    fi
    macos_dist_dir="${pkg_dir}/dist"
    macos_content_dir="${macos_dist_dir}/DOSBox Staging.app/Contents"
fi

if [ "$platform" = "windows" ] && [ -z "$VC_REDIST_DIR" ]; then
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

if [ -z "$(find "${pkg_dir}" -maxdepth 0 -empty)" ] && [ "$force_pkg" != "true" ]; then
    echo "PACKAGE_DIR must be empty. Use '-f' to force creation anyway"
    usage
    exit 1
fi

set -x

install_doc
install_resources

case $platform in
    windows) pkg_windows ;;
    macos)   pkg_macos   ;;
    linux)   pkg_linux   ;;
    *)     echo "Unsupported platform."; usage; exit 1 ;;
esac
