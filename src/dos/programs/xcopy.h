// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_XCOPY_H
#define DOSBOX_PROGRAM_XCOPY_H

#include "dos/drives.h"
#include "dos/programs.h"

#include <optional>
#include <string>

inline constexpr DosFilename AllFiles = {.name = "*", .ext = "*"};

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
	/// The source path.
	std::string src_path = {};

	/// The source file specification.
	DosFilename src_spec = AllFiles;

	/// The snapshot of the source tree, built by BuildSourceSnapshot().
	std::vector<DOS_DTA::Result> src_snapshot = {};

	/// The number of copied files, incremented by CopyToDestination().
	int copied_count = 0;

	static void AddMessages();

	XCopyError Initialize();

	XCopyError InitializeSource(const std::string& value);

	XCopyError BuildSourceSnapshot();

	XCopyError CopyToDestination();

	XCopyError CopyFile(uint16_t source_handle, uint16_t dest_handle,
	                    const char* filename);

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
