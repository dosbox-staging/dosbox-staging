#!/usr/bin/env bash

# SPDX-FileCopyrightText:  2024-2024 The DOSBox Staging Team
# SPDX-License-Identifier: GPL-2.0-or-later

# Set Bash's unofficial strict mode
# See: http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail

is_system_debian_based() {
    if [[ ! -f /etc/debian_version ]]; then
        echo "This is not a Debian-based system"
        exit 1
    fi
}

cd_to_repo_root() {
    cd "$(git rev-parse --show-toplevel)"
}

is_source_clean() {
    if [[ -n "$(git status --ignored --porcelain)" ]]; then
        echo "The repository contains ignored or modified files, clean it with:"
        echo "git clean -ffdx"
        exit 1
    fi
}

copy_debian_metadata_to_root() {
    # Avoid polluting the repo root with Debian's metadata, so we copy
    # it to the root at the time of use; then delete it when we're done.
    cp -rf extras/linux/debian .
    trap 'rm -rf debian' EXIT
}

setup_common_variables() {
    local DEBIAN_VERSION=11

    cp -rv extras/linux/debian .

    PACKAGE="$(grep Package: debian/control | sed 's/.*: //')"

    PACKAGE_VERSION="$(./scripts/get-version.sh version-and-hash)"

    # Debianize the package version by replacing the first dash after
    # the numbered tag with a tilde. Subsequence dashes are ok.
    PACKAGE_VERSION=${PACKAGE_VERSION/-/\~}

    # Debian includes a "-rev" postfix version to let the package
    # maintainer release multiple versions of the same upstream source.
    # Because the project always increments the tag's minor version and/or
    # commit hash, our revision is always 1.
    PACKAGE_REVISION=1

    DEPENDENCIES="$(cat packages/debian-${DEBIAN_VERSION}-development.txt | xargs)"
}

check_package_dependencies() {
    local to_install=
    for package in ${DEPENDENCIES}; do
        if ! dpkg-query -W -f='${Status}' "$package" 2>/dev/null | grep -q "ok installed"; then
            echo "Package $package is not installed. Adding to install list."
            to_install="$to_install $package"
        fi
    done
    if [[ -n ${to_install:-} ]]; then
        echo "Install missing packages with:"
        echo "sudo apt-get install -y $to_install"
        exit 1
    fi
}

generate_mandatory_debian_change_record() {
    # Start with a new changelog for the current tag/commit and point the user
    # to the commit history for the repos entire history.
    rm -f debian/changelog

    export EDITOR=true
    dch \
        --create \
        --controlmaint \
        --package "${PACKAGE}" \
        --newversion "${PACKAGE_VERSION}-${PACKAGE_REVISION}" \
        "Commit history https://github.com/dosbox-staging/dosbox-staging/commits/main/"
}

populate_build_depdencies() {
    # The dependencies need to be in comma separated format
    CSV_DEPENDENCIES="${DEPENDENCIES// /,}"

    # The builder needs to know what version of deb-helper we're compatible with.
    DEBHELPER="debhelper-compat (= 11)"

    # Insert the dependencies and helper version into the control file
    sed -i \
        "s/Build-Depends:.*/Build-Depends: ${CSV_DEPENDENCIES},${DEBHELPER}/" \
        debian/control
}

create_original_source_tarball() {
    tar cz . > "../${PACKAGE}_${PACKAGE_VERSION}.orig.tar.gz"
}

build_package() {
    debuild -us -uc
}


# Run the functions in order:

is_system_debian_based

cd_to_repo_root

is_source_clean

copy_debian_metadata_to_root

setup_common_variables

check_package_dependencies

generate_mandatory_debian_change_record

populate_build_depdencies

create_original_source_tarball

build_package

