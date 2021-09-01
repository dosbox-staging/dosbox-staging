#ifndef DOSBOX_PROGRAM_IMGMOUNT_H
#define DOSBOX_PROGRAM_IMGMOUNT_H

#include "programs.h"

class IMGMOUNT final : public Program {
    public:
        void Run(void);
};

void IMGMOUNT_ProgramStart(Program **make);

#endif // DOSBOX_PROGRAM_IMGMOUNT_H
