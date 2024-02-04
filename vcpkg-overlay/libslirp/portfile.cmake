vcpkg_from_gitlab(
    GITLAB_URL https://gitlab.freedesktop.org
    OUT_SOURCE_PATH SOURCE_PATH
    REPO slirp/libslirp
    REF 129077f9870426d1b7b3a8239d8b5a50bee017b4
    SHA512 8223352cf09154a857960b852c9b1a8a01ab0c86c3cc873fc37da337174c59b68b50939cffc018c91f320000da09ce833acf966a3492de1453bc7196d14c1198
    HEAD_REF master
)

if(VCPKG_HOST_IS_WINDOWS)
    vcpkg_acquire_msys(MSYS_ROOT)
    vcpkg_add_to_path("${MSYS_ROOT}/usr/bin")
endif()

vcpkg_configure_meson(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${OPTIONS}
)

vcpkg_install_meson(ADD_BIN_TO_PATH)

vcpkg_fixup_pkgconfig()

vcpkg_copy_pdbs()

file(INSTALL "${SOURCE_PATH}/COPYRIGHT" DESTINATION "${CURRENT_PACKAGES_DIR}/share/libslirp" RENAME copyright)
