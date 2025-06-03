set(disknoises_files 
    fdd_seek1.flac
    fdd_seek2.flac
    fdd_seek3.flac
    fdd_seek4.flac
    fdd_seek5.flac
    fdd_seek6.flac
    fdd_seek7.flac
    fdd_seek8.flac
    fdd_seek9.flac
    hdd_seek1.flac
    hdd_seek2.flac
    hdd_seek3.flac
    hdd_seek4.flac
    hdd_seek5.flac
    hdd_seek6.flac
    hdd_seek7.flac
    hdd_seek8.flac
    hdd_seek9.flac
    hdd_spinup.flac
    hdd_spin.flac
)

list(TRANSFORM disknoises_files PREPEND "disknoises/")