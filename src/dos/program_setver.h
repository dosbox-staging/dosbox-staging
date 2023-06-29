/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2023  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_PROGRAM_SETVER_H
#define DOSBOX_PROGRAM_SETVER_H

#include "programs.h"

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

	static void OverrideVersion(const char* name, DOS_PSP& psp);

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
