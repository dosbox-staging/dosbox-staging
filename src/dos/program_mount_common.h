#ifndef DOSBOX_PROGRAM_MOUNT_COMMON_H
#define DOSBOX_PROGRAM_MOUNT_COMMON_H

#include "dosbox.h"

#if defined(WIN32)
#ifndef S_ISDIR
#define S_ISDIR(m) (((m)&S_IFMT)==S_IFDIR)
#endif
#endif

extern Bitu ZDRIVE_NUM;

const char *UnmountHelper(char umount);

#endif // DOSBOX_PROGRAM_MOUNT_COMMON_H
