set(drives_files 
    y/dos/debug.com
    y/dos/deltree.com
    y/dos/xcopy.exe
    y.conf
)

list(TRANSFORM drives_files PREPEND "drives/")