/*
 * Implementation of the standard functions in direct.h
 *
 * opendir(), readdir(), closedir() and rewinddir().
 *
 * 06/17/2000 by Mike Haaland <mhaaland@hypertech.com>
 */

#include "dirent.h"
#include "io.h"

#ifdef WIN32

/** open the current directory and return a structure
 *  to be used in subsequent readdir() and closedir()
 *  calls. 
 *
 *  returns NULL if one error. 
 */
DIR * opendir(const char *dirname) {
    
    static DIR dir;

    /* Stash the directory name */
    strcpy(dir.pathName,dirname);
	strcat(dir.pathName,"*.*");

    /* set the handle to invalid and set the firstTime flag */
    dir.handle    = INVALID_HANDLE_VALUE;
    dir.firstTime = TRUE;

    /* Change the current directory to the one requested */
    return (access(dirname,0) ? NULL : &dir);
}

/** Close the current directory - return 0 if success */
int	closedir(DIR *dirp) {
    /* reset ourselves to the first file in the directory
     *
     * We just close the current handle and reset for the
     * next readdir call
     */
    int result = 1;
    
    if (dirp->handle != INVALID_HANDLE_VALUE)
    {
        result = FindClose(dirp->handle);
        dirp->handle = INVALID_HANDLE_VALUE;
    }
    
    return (result == 0) ? 1 : 0;
}

/** get the next entry in the directory */
struct dirent *	readdir(DIR *dirp) {
    static struct dirent d;

    if (TRUE == dirp->firstTime)
    {
        /** Get the first entry in the directory */
        dirp->handle = FindFirstFile(dirp->pathName, &dirp->findFileData);
        dirp->firstTime = FALSE;
        if (INVALID_HANDLE_VALUE == dirp->handle)
        {
            return NULL;
        }
    }
    else
    {
        int result = FindNextFile(dirp->handle, &dirp->findFileData);
        if (0 == result )
        {
            return NULL;
        }
    }
    /* we have a valid FIND_FILE_DATA, copy the filename */
    memset(&d,'\0', sizeof(struct dirent));

    strcpy(d.d_name,dirp->findFileData.cFileName);
    d.d_namlen = strlen(d.d_name);  

    return &d;
}

/** reopen the current directory */
void rewinddir(DIR *dirp) {
     closedir(dirp);
     dirp->firstTime = TRUE;
}

#endif
