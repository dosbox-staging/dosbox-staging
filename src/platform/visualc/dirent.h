

/*
 * Defines and structures used to implement the
 * functionality standard in direct.h for:
 *
 * opendir(), readdir(), closedir() and rewinddir().
 *
 * 06/17/2000 by Mike Haaland <mhaaland@hypertech.com>
 */
#ifndef _DIRENT_H_
#define _DIRENT_H_

#ifdef _MSC_VER


#ifdef  __cplusplus
extern "C" {
#endif

#include <windows.h>
#include <direct.h>
#include <sys/types.h>

#if !defined(__GNUC__)
/* Convienience macros used with stat structures */
#define S_ISDIR(x) (x & _S_IFDIR)
#define S_ISREG(x) (x & _S_IFREG)
#endif


/* Structure to keep track of the current directory status */
typedef struct my_dir {
    HANDLE          handle;
    WIN32_FIND_DATA findFileData;
    BOOLEAN         firstTime;
    char            pathName[MAX_PATH];
} DIR;

/* Standard directory name entry returned by readdir() */
struct dirent {
  char d_namlen;
  char d_name[MAX_PATH];
};

/* function prototypes */
int		        closedir(DIR *dirp);
DIR *		    opendir(const char *dirname);
struct dirent *	readdir(DIR *dirp);
void		    rewinddir(DIR *dirp);

#ifdef  __cplusplus
}
#endif

#endif
#endif

