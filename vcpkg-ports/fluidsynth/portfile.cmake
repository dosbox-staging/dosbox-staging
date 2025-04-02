if("pulseaudio" IN_LIST FEATURES)
    message(
    "${PORT} with pulseaudio feature currently requires the following from the system package manager:
        libpulse-dev pulseaudio
    These can be installed on Ubuntu systems via sudo apt install libpulse-dev pulseaudio"
    )
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO FluidSynth/fluidsynth
    REF "v${VERSION}"
    SHA512 35eaea8c1709ebbd5dee8f3946ab59c39afe31d92b972a44013fa23987aa48936f7d1326d5bda81c6e66f02bf988e48601367d49276a4dd78dbca7a2571f5e57
    HEAD_REF master
    PATCHES
        gentables.patch
)

vcpkg_check_features(
    OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        buildtools  VCPKG_BUILD_MAKE_TABLES
        sndfile     enable-libsndfile
        pulseaudio  enable-pulseaudio
)

set(OPTIONS_TO_ENABLE enable-floats)

set(OPTIONS_TO_DISABLE enable-coverage enable-dbus enable-fpe-check enable-framework enable-jack
    enable-libinstpatch enable-midishare enable-oboe enable-openmp enable-oss enable-pipewire enable-portaudio
    enable-profiling enable-readline enable-sdl2 enable-sdl3 enable-systemd enable-trap-on-fpe enable-ubsan
    enable-dsound enable-wasapi enable-waveout enable-winmidi enable-coreaudio enable-coremidi enable-alsa enable-opensles)

foreach(_option IN LISTS OPTIONS_TO_ENABLE)
    list(APPEND ENABLED_OPTIONS "-D${_option}:BOOL=ON")
endforeach()
    
foreach(_option IN LISTS OPTIONS_TO_DISABLE)
    list(APPEND DISABLED_OPTIONS "-D${_option}:BOOL=OFF")
endforeach()

vcpkg_find_acquire_program(PKGCONFIG)
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        "-DVCPKG_HOST_TRIPLET=${HOST_TRIPLET}"
        ${FEATURE_OPTIONS}
        ${ENABLED_OPTIONS}
        ${DISABLED_OPTIONS}
        "-DPKG_CONFIG_EXECUTABLE=${PKGCONFIG}"
    MAYBE_UNUSED_VARIABLES
        ${OPTIONS_TO_DISABLE}
        VCPKG_BUILD_MAKE_TABLES
        enable-coverage
        enable-framework
        enable-ubsan
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/fluidsynth)

vcpkg_fixup_pkgconfig()

set(tools fluidsynth)
if("buildtools" IN_LIST FEATURES)
    list(APPEND tools make_tables)
endif()
vcpkg_copy_tools(TOOL_NAMES ${tools} AUTO_CLEAN)

vcpkg_copy_pdbs()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
    "${CURRENT_PACKAGES_DIR}/share/man")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
