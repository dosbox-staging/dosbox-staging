
function(add_to_copy_command SOURCE DESTINATION)
  add_custom_command(
    OUTPUT  ${ASSET_DESTINATION_FILES}
    DEPENDS "${SOURCE}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SOURCE}" "${DESTINATION}"
    APPEND
  )
endfunction()


function(add_copy_assets)

  # Generate list of resource files
  file(GLOB_RECURSE RESOURCE_FILES
       RELATIVE "${CMAKE_SOURCE_DIR}/${RESOURCES_PATH}"
       "${CMAKE_SOURCE_DIR}/${RESOURCES_PATH}/*")
  # Filter out build system and GIT related files
  list(FILTER RESOURCE_FILES EXCLUDE REGEX "(.*/)*meson\.build$")
  list(FILTER RESOURCE_FILES EXCLUDE REGEX "(.*/)*\.git[a-z]+$")

  # Generate list of resource files with destination paths
  foreach(RESOURCE_FILE ${RESOURCE_FILES})
    list(APPEND RESOURCE_DESTINATION_FILES
         "${CMAKE_CURRENT_BINARY_DIR}${RESOURCE_COPY_PATH}/${RESOURCE_FILE}")
  endforeach()

  # Generate list of license files with destination paths
  foreach(LICENSE_FILE ${LICENSE_FILES})
    list(APPEND LICENSE_DESTINATION_FILES
         "${CMAKE_CURRENT_BINARY_DIR}/doc/${LICENSE_FILE}")
  endforeach()
  list(APPEND LICENSE_DESTINATION_FILES ${CMAKE_CURRENT_BINARY_DIR}/doc/LICENSE)

  # Create initial (dummy) command for copying the assets
  set(ASSET_DESTINATION_FILES
      ${LICENSE_DESTINATION_FILES}
      ${RESOURCE_DESTINATION_FILES})
  add_custom_command(OUTPUT ${ASSET_DESTINATION_FILES})

  # Add a copy command for each resource file
  foreach(RESOURCE_FILE ${RESOURCE_FILES})
    add_to_copy_command(
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/${RESOURCE_FILE}"
      "${CMAKE_CURRENT_BINARY_DIR}${RESOURCE_COPY_PATH}/${RESOURCE_FILE}"
    )
  endforeach()

  # Add a copy command for each license file
  foreach(LICENSE_FILE ${LICENSE_FILES})
    add_to_copy_command(
      "${CMAKE_CURRENT_SOURCE_DIR}/${LICENSE_FILE}"
      "${CMAKE_CURRENT_BINARY_DIR}/doc/${LICENSE_FILE}"
    )
  endforeach()

  # Add a copy command for the main license file
  add_to_copy_command(
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    "${CMAKE_CURRENT_BINARY_DIR}/doc/LICENSE"
  )

  # Add a target to copy the assets to the build directory
  add_custom_target(copy_assets ALL DEPENDS ${ASSET_DESTINATION_FILES})
  add_dependencies(dosbox copy_assets)

endfunction()
