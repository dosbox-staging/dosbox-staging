
function(add_install_rules)

  # For now installation is only supported on Linux
  if (DOSBOX_PLATFORM_LINUX)

    set(INSTALL_DIR_DOC       "${CMAKE_INSTALL_DOCDIR}")
    set(INSTALL_DIR_MAN       "${CMAKE_INSTALL_MANDIR}")
    set(INSTALL_DIR_RESOURCES "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}")
    set(INSTALL_DIR_DESKTOP   "${CMAKE_INSTALL_DATADIR}/applications")
    set(INSTALL_DIR_ICONS     "${CMAKE_INSTALL_DATADIR}/icons/hicolor")
    set(INSTALL_DIR_METAINFO  "${CMAKE_INSTALL_DATADIR}/metainfo")

    set(INSTALL_ICON_NAME "org.dosbox_staging.dosbox_staging")

    # Install the application binary
    install(TARGETS dosbox RUNTIME)

    # System manual page
    install(FILES "docs/dosbox.1"
      DESTINATION "${INSTALL_DIR_MAN}/man1")

    # Application menu entry
    install(FILES "contrib/linux/${INSTALL_ICON_NAME}.desktop"
      DESTINATION "${INSTALL_DIR_DESKTOP}")

    # Software component information
    install(FILES "contrib/linux/${INSTALL_ICON_NAME}.metainfo.xml"
      DESTINATION "${INSTALL_DIR_METAINFO}")

    # Scalable icon
    install(FILES "extras/icons/svg/dosbox-staging.svg"
      DESTINATION "${INSTALL_DIR_ICONS}/scalable/apps"
      RENAME "${INSTALL_ICON_NAME}.svg")

    # Bitmap icons
    foreach(PX IN ITEMS 16 22 24 32 48 96 128 256 512 1024)
    install(FILES "extras/icons/png/icon_${PX}.png"
        DESTINATION "${INSTALL_DIR_ICONS}/${PX}x${PX}/apps"
        RENAME "${INSTALL_ICON_NAME}.png")
    endforeach()

    # Bundle the resources
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}${RESOURCE_COPY_PATH}/"
            DESTINATION "${INSTALL_DIR_RESOURCES}")

    # Bundle required licenses
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doc/licenses"
            DESTINATION "${INSTALL_DIR_DOC}")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/doc/LICENSE"
            DESTINATION "${INSTALL_DIR_DOC}")

  endif()

endfunction()
