// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_XCOPY_H
#define DOSBOX_PROGRAM_XCOPY_H

#include "dos/drives.h"
#include "dos/programs.h"

#include <optional>
#include <string>

inline constexpr DosFilename AllFiles = {.name = "*", .ext = "*"};

struct XCopySnapshotEntry {
	std::string path;
	std::vector<DOS_DTA::Result> files;
};

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

	/// The destination path.
	std::string dest_path = {};

	/// The destination file specification (default *.*).
	DosFilename dest_spec = AllFiles;

	/// Copy entire source subtree.
	bool deep_copy = false;

	/// Copy empty directories (only used when deep_copy is true).
	bool copy_empty_dirs = false;

	/// Displays a prompt before overwriting an existing file
	bool display_overwrite_prompt = false;

	/**
	 * If present, only copy files changed on or after this value, specified
	 * in the FAT directory date format:
	 * <code>
	 * │F│E│D│C│B│A│9│8│7│6│5│4│3│2│1│0│
	 * │ │ │ │ │ │ │ │ │ │ │ └─┴─┴─┴─┴─── Day (1-31)
	 * │ │ │ │ │ │ │ └─┴─┴─┴──────────── Month (1-12)
	 * └─┴─┴─┴─┴─┴─┴─────────────────── Year (less 1980)
	 * </code>
	 */
	std::optional<uint16_t> maybe_min_date = {};

	/// The snapshot of the source tree, built by BuildSourceSnapshot().
	std::vector<XCopySnapshotEntry> src_snapshot = {};

	/// The number of copied files, incremented by CopyToDestination().
	int copied_count = 0;

	static void AddMessages();

	XCopyError Initialize();

	XCopyError InitializeSource(const std::string& value);

	XCopyError InitializeDestination(const std::string& value);

	XCopyError BuildSourceSnapshot();

	XCopyError CopyToDestination();

	XCopyError MakeDirectoryTree(const std::string& path);

	XCopyError CopyFile(uint16_t source_handle, uint16_t dest_handle,
	                    const char* filename);

	std::optional<char> PromptForLetter(const std::string& allowed,
	                                    bool cr_confirms);

	XCopyError FailOnCyclicCopy();

	bool GetDefaultDisplayOverwritePrompt() const;

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
