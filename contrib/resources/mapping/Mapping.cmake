set(mapping_files
    ASCII.TXT
    CAPITAL_SMALL.TXT
    DECOMPOSITION.TXT
    MAIN.TXT
)

list(TRANSFORM mapping_files PREPEND "mapping/")