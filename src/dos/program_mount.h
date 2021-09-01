#ifndef DOSBOX_PROGRAM_MOUNT_H
#define DOSBOX_PROGRAM_MOUNT_H

#include "programs.h"

class MOUNT final : public Program {
    public:
        void Move_Z(char new_z);
        void ListMounts();
        void Run(void);
};

void MOUNT_ProgramStart(Program **make);

#endif // DOSBOX_PROGRAM_MOUNT_H
