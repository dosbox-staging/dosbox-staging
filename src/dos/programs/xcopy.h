// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_XCOPY_H
#define DOSBOX_PROGRAM_XCOPY_H

#include "dos/drives.h"
#include "dos/programs.h"

#include <optional>
#include <string>

inline constexpr DosFilename AllFiles = {.name = "*", .ext = "*"};

enum class XcopyExitCode : uint8_t {
	FileNotFoundError   = 1,
	CtrlBreakIssued     = 2,
	InitializationError = 4,
	DiskWriteError      = 5
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

	static void AddMessages();

	[[nodiscard]] bool Initialize();

	[[nodiscard]] bool InitializeSource(const std::string& value);

	std::optional<std::vector<DOS_DTA::Result>> BuildSourceSnapshot();

	std::pair<bool, int> CopyToDestination(const std::vector<DOS_DTA::Result>& files);

	bool CopyFile(uint16_t source_handle, uint16_t dest_handle,
	              const char* filename);

	template <typename... Args>
	void WriteErr(const std::string& format, const Args&... args)
	{
		// TODO: The original XCOPY implementation writes certain
		//	 messages to stderr. Forward to stdout for now.
		WriteOut(format, args...);
	}
};

#endif
