set(translations_files
    br.lng
    de.lng
    en.lng
    es.lng
    fr.lng
    it.lng
    nl.lng
    pl.lng
    ru.lng
)

list(TRANSFORM translations_files PREPEND "translations/")