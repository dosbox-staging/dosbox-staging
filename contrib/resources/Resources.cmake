include(${CMAKE_CURRENT_LIST_DIR}/disknoises/DiskNoises.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/drives/Drives.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/freedos-cpi/FreedosCpi.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/freedos-keyboard/FreedosKeyboard.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/glshaders/GlShaders.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/mapperfiles/Mapperfiles.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/mapping/Mapping.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/mapping-freedos.org/MappingFreedos.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/mapping-unicode.org/MappingUnicode.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/mapping-wikipedia.org/MappingWikipedia.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/translations/Translations.cmake)

set(DOSBOX_RESOURCE_FILES
    ${disknoises_files}
    ${drives_files}
    ${freedoscpi_files}
    ${freedoskeyboard_files}
    ${glshaders_files}
    ${mapperfile_files}
    ${mapping_files}
    ${mappingfreedos_files}
    ${mappingunicode_files}
    ${mappingwikipedia_files}
    ${translations_files})

set(DOSBOX_RESOURCE_FILES_ROOT ${DOSBOX_RESOURCE_FILES})
list(TRANSFORM DOSBOX_RESOURCE_FILES_ROOT PREPEND "contrib/resources/")