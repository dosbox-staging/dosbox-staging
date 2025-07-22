add_subdirectory(disknoises)
add_subdirectory(drives)
add_subdirectory(freedos-cpi)
add_subdirectory(freedos-keyboard)
add_subdirectory(glshaders)
add_subdirectory(mapperfiles)
add_subdirectory(mapping)
add_subdirectory(mapping-freedos.org)
add_subdirectory(mapping-unicode.org)
add_subdirectory(mapping-wikipedia.org)
add_subdirectory(translations)

set(RESOURCE_FILES

  ${RESOURCE_FILES_DISKNOISES}
  ${RESOURCE_FILES_DRIVES}
  ${RESOURCE_FILES_FREEDOS_CPI}
  ${RESOURCE_FILES_FREEDOS_KEYBOARD}
  ${RESOURCE_FILES_GLSHADERS}
  ${RESOURCE_FILES_MAPPERFILES}
  ${RESOURCE_FILES_MAPPING}
  ${RESOURCE_FILES_MAPPING_FREEDOS_ORG}
  ${RESOURCE_FILES_MAPPING_UNICODE_ORG}
  ${RESOURCE_FILES_MAPPING_WIKIPEDIA_ORG}
  ${RESOURCE_FILES_TRANSLATIONS}

  PARENT_SCOPE
)
