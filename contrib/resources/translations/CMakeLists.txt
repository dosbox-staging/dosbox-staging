set(FILES

    br.lng
    de.lng
    es.lng
    fr.lng
    it.lng
    nl.lng
    pl.lng
    ru.lng
)

list(TRANSFORM FILES PREPEND "translations/")
set(RESOURCE_FILES_TRANSLATIONS ${FILES} PARENT_SCOPE)
