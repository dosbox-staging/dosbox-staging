// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_XCOPY_H
#define DOSBOX_PROGRAM_XCOPY_H

#include "dos/programs.h"

#include <string>

enum class [[nodiscard]] XCopyError : uint8_t {
	NoError       = 0,
	FileNotFound  = 1,
	Interrupted   = 2,
	Failure       = 4,
	CriticalError = 5
};

class XCOPY final : public Program {
public:
	XCOPY()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::File,
		               HELP_CmdType::Program,
		               "XCOPY"};
	}
	void Run() override;

private:
	/// The number of copied files, incremented by CopyToDestination().
	int copied_count = 0;

	static void AddMessages();

	void AfterRun(XCopyError error);

	template <typename... Args>
	void WriteErr(const std::string& format, const Args&... args)
	{
		// TODO: The original XCOPY implementation writes certain
		//	 messages to stderr. Forward to stdout for now.
		WriteOut(format, args...);
	}
};

#endif
