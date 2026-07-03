#ifndef DOSBOX_PROGRAM_APPEND_H
#define DOSBOX_PROGRAM_APPEND_H

#include "dos/programs.h"

class APPEND final : public Program {
public:
	APPEND()
	{
		help_detail = {HELP_Filter::All,
		               HELP_Category::Misc,
		               HELP_CmdType::Program,
		               "APPEND"};
	}
	void Run() override;
};

#endif
