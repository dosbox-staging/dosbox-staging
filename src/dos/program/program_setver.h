// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_SETVER_H
#define DOSBOX_PROGRAM_SETVER_H

#include "dos/programs.h"

#include <map>

class SETVER final : public Program {
public:
	SETVER()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::Misc,
		               HELP_CmdType::Program,
		               "SETVER"};
	}
	void Run() override;

	static void LoadTableFromFile();
	static void SaveTableToFile();

	static void OverrideVersion(const std::string& canonical_name, DOS_PSP& psp);

	struct FakeVersion {
		uint8_t major = 0;
		uint8_t minor = 0;
	};

	using NameVersionTable = std::map<std::string, SETVER::FakeVersion>;

private:
	static bool ParseVersion(const std::string& version_str, FakeVersion& version);
	static bool PreprocessName(std::string& name,
	                           const bool allow_non_existing_files = false);
	static bool IsNameWithPath(const std::string& name);

	void CommandDeletePerFile(const std::string& name, const bool has_arg_quiet);
	void CommandDeleteGlobalOnly(const bool has_arg_quiet);
	void CommandDeleteAll(const bool has_arg_quiet);
	void CommandSet(const std::string& name, const std::string& version_str,
	                const bool has_arg_quiet);
	void CommandPrintAll(const bool has_arg_batch, const bool has_arg_paged);

	static void AddToTable(const std::string& name, const FakeVersion& version);
	static std::string FindKeyCaseInsensitive(const std::string& key,
	                                          const NameVersionTable& table);
	static bool IsTableEmpty();

	static std_fs::path GetTableFilePath();

	void AddMessages();
};

#endif
