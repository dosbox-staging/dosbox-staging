set(FILES

    y/dos/debug.com
    y/dos/deltree.com
    y/dos/xcopy.exe
    y.conf
)

list(TRANSFORM FILES PREPEND "drives/")
set(RESOURCE_FILES_DRIVES ${FILES} PARENT_SCOPE)
