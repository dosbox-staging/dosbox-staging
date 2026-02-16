// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "setver.h"

#include "more_output.h"

#include "utils/checks.h"
#include "config/config.h"
#include "dosbox_config.h"
#include "config/setup.h"
#include "misc/std_filesystem.h"
#include "utils/string_utils.h"

#include <fstream>
#include <iostream>
#include <regex>

CHECK_NARROWING();

using NameVersionTable = SETVER::NameVersionTable;

static struct {
	SETVER::FakeVersion version_global = {};
	bool is_global_version_set         = false;

	NameVersionTable by_file_name = {
	        // Since MS-DOS 6.22 has some default version table,
	        // we can provide some sane defaults too
	        {"WIN100.BIN", {3, 40}}, // fixes Microsoft Windows 1.x
	        {"WIN200.BIN", {3, 40}}, // fixes Microsoft Windows 2.x
	};

	NameVersionTable by_file_path = {};

} setver_table;

void SETVER::Run()
{
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_SETVER_HELP_LONG"));
		output.Display();
		return;
	}

	// Retrieve all the switches
	const bool has_arg_delete = cmd->FindExistRemoveAll("/d", "/delete");
	const bool has_arg_quiet  = cmd->FindExistRemoveAll("/q", "/quiet");
	// DR-DOS extensions
	const bool has_arg_batch  = cmd->FindExistRemoveAll("/b");
	const bool has_arg_global = cmd->FindExistRemoveAll("/g");
	const bool has_arg_paged  = cmd->FindExistRemoveAll("/p");
	// DOSBox Staging extensions
	const bool has_arg_all = cmd->FindExistRemoveAll("/all");

	// TODO: DR-DOS also provides /x switch to deal with BDOS versions - for
	// now this is not implemented, it is unclear to me what it exactly does

	// Make sure no other switches are supplied
	std::string tmp_str;
	if (cmd->FindStringBeginCaseSensitive("/", tmp_str)) {
		tmp_str = std::string("/") + tmp_str;
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), tmp_str.c_str());
		return;
	}

	auto params = cmd->GetArguments();

	// Handle first parameter being a path to SETVER.EXE database
	const bool is_database_candidate = !params.empty() &&
	                                   IsNameWithPath(params[0]);
	if (is_database_candidate && PreprocessName(params[0])) {
		if (params[0].back() == '\\') {
			if (params[0] != "Z:\\") {
				WriteOut(MSG_Get("PROGRAM_SETVER_WRONG_TABLE"));
				return;
			}
			params.erase(params.begin());
		}
	}

	// Helper lambda for file name preprocessing
	auto preprocess_name = [&](std::string& name,
	                           const bool allow_non_existing_files = false) {
		if (!PreprocessName(name, allow_non_existing_files)) {
			if (IsNameWithPath(name)) {
				WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			} else {
				WriteOut(MSG_Get("SHELL_ILLEGAL_FILE_NAME"));
			}
			return false;
		}
		return true;
	};

	// Preprocess first parameter if name/path is expected
	if (!params.empty() && !has_arg_global) {
		// When removing file from the list, it is possible the disk it
		// was located does not currently exist
		const bool allow_non_existing_files = has_arg_delete;
		if (!preprocess_name(params[0], allow_non_existing_files)) {
			return;
		}
		// It shouldn't be a directory
		if (!params[0].empty() && params[0].back() == '\\') {
			WriteOut(MSG_Get("SHELL_EXPECTED_FILE_NOT_DIR"));
			return;
		}
	}

	// Detect illegal switch combinations
	if ((has_arg_batch || has_arg_paged) && (has_arg_delete || has_arg_global)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH_COMBO"));
		return;
	}
	if (has_arg_all && !has_arg_delete) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH_COMBO"));
		return;
	}

	// Handle cases with no file parameters
	if (params.empty()) {
		if (has_arg_delete && has_arg_global) {
			CommandDeleteGlobalOnly(has_arg_quiet);
		} else if (has_arg_delete && has_arg_all) {
			CommandDeleteAll(has_arg_quiet);
		} else if (has_arg_global || has_arg_delete) {
			WriteOut(MSG_Get("SHELL_MISSING_PARAMETER"));
		} else {
			CommandPrintAll(has_arg_batch, has_arg_paged);
		}
		return;
	}

	// From now on at least 1 parameter is guaranteed to exist

	// Detect illegal switch combinations
	if (has_arg_delete && has_arg_global) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH_COMBO"));
		return;
	}

	// Hande entry deletion
	if (has_arg_delete) {
		if (params.size() > 1) {
			WriteOut(MSG_Get("SHELL_TOO_MANY_PARAMETERS"));
		} else {
			CommandDeletePerFile(params[0], has_arg_quiet);
		}
		return;
	}

	// Hande global version set
	if (has_arg_global) {
		if (params.size() > 1) {
			WriteOut(MSG_Get("SHELL_TOO_MANY_PARAMETERS"));
		} else {
			CommandSet({}, params[0], has_arg_quiet);
		}
		return;
	}

	// Handle per-file version set
	if (has_arg_batch || has_arg_paged) {
		WriteOut(MSG_Get("SHELL_SYNTAX_ERROR"));
	} else if (params.size() > 2) {
		WriteOut(MSG_Get("SHELL_TOO_MANY_PARAMETERS"));
	} else if (params.size() < 2) {
		WriteOut(MSG_Get("SHELL_SYNTAX_ERROR"));
	} else {
		CommandSet(params[0], params[1], has_arg_quiet);
	}
}

bool SETVER::ParseVersion(const std::string& version_str, FakeVersion& version)
{
	version.major = 0;
	version.minor = 0;

	const std::regex expression("^([0-9])(\\.([0-9]{0,2}))?$");
	std::smatch match = {};

	std::regex_match(version_str, match, expression);

	if (match.size() != 4) {
		return false;
	}

	const auto& major_str = match[1].str();
	const auto& minor_str = match[3].str();

	const auto major = parse_int(major_str);
	if (!major) {
		// It would be enough to assert, but PVS-Studio was unhappy
		assert(false);
		return false;
	}

	version.major = static_cast<uint8_t>(*major);

	if (minor_str.empty()) {
		return true;
	}

	const auto minor = parse_int(minor_str);
	if (!minor) {
		// It would be enough to assert, but PVS-Studio was unhappy
		assert(false);
		return false;
	}

	if (minor_str.size() == 1) {
		version.minor = static_cast<uint8_t>(*minor * 10);
	} else {
		version.minor = static_cast<uint8_t>(*minor);
	}

	return true;
}

bool SETVER::PreprocessName(std::string& name, const bool allow_non_existing_files)
{
	// Preprocess file names and relative paths
	trim(name);
	if (IsNameWithPath(name)) {
		char buffer[DOS_PATHLENGTH];
		if (!DOS_Canonicalize(name.c_str(), buffer)) {
			return allow_non_existing_files;
		}
		name = buffer;
		return true;
	}

	if (name.size() > LFN_NAMELENGTH) {
		return false;
	}
	upcase(name);
	return true;
}

bool SETVER::IsNameWithPath(const std::string& name)
{
	// TODO: Use 'contains' after migration to C++20
	return (name.find(':') != std::string::npos) ||
	       (name.find('\\') != std::string::npos);
}

void SETVER::CommandDeletePerFile(const std::string& name, const bool has_arg_quiet)
{
	auto try_delete = [](const std::string& name, NameVersionTable& table) {
		const auto key = FindKeyCaseInsensitive(name, table);
		if (key.empty()) {
			return false;
		}
		table.erase(key);
		return true;
	};

	if (!try_delete(name, setver_table.by_file_name) &&
	    !try_delete(name, setver_table.by_file_path)) {
		if (!has_arg_quiet) {
			WriteOut(MSG_Get("PROGRAM_SETVER_TABLE_ENTRY_NOT_FOUND"));
		}
		return;
	}

	SaveTableToFile();
	if (!has_arg_quiet) {
		WriteOut(MSG_Get("PROGRAM_SETVER_TABLE_ENTRY_REMOVED"));
	}
}

void SETVER::CommandDeleteGlobalOnly(const bool has_arg_quiet)
{
	if (!setver_table.is_global_version_set) {
		if (!has_arg_quiet) {
			WriteOut(MSG_Get("PROGRAM_SETVER_TABLE_ENTRY_NOT_FOUND"));
		}
		return;
	}

	setver_table.is_global_version_set = false;

	SaveTableToFile();
	if (!has_arg_quiet) {
		WriteOut(MSG_Get("PROGRAM_SETVER_TABLE_ENTRY_REMOVED"));
	}
}

void SETVER::CommandDeleteAll(const bool has_arg_quiet)
{
	if (IsTableEmpty()) {
		if (!has_arg_quiet) {
			WriteOut(MSG_Get("PROGRAM_SETVER_TABLE_ALREADY_EMPTY"));
		}
		return;
	}

	setver_table.is_global_version_set = false;
	setver_table.by_file_name.clear();
	setver_table.by_file_path.clear();

	SaveTableToFile();
	if (!has_arg_quiet) {
		WriteOut(MSG_Get("PROGRAM_SETVER_TABLE_CLEARED"));
	}
}

void SETVER::CommandSet(const std::string& name, const std::string& version_str,
                        const bool has_arg_quiet)
{
	FakeVersion version = {};
	if (!ParseVersion(version_str, version)) {
		WriteOut(MSG_Get("PROGRAM_SETVER_INVALID_VERSION"));
		return;
	}

	AddToTable(name, version);

	SaveTableToFile();
	if (!has_arg_quiet) {
		WriteOut(MSG_Get("PROGRAM_SETVER_TABLE_UPDATED"));
	}
}

void SETVER::CommandPrintAll(const bool has_arg_batch, const bool has_arg_paged)
{
	// Nothing to print out if table is empty
	if (IsTableEmpty()) {
		if (!has_arg_batch) {
			WriteOut(MSG_Get("PROGRAM_SETVER_TABLE_EMPTY"));
		}
		return;
	}

	const std::string setver_command = "@Z:\\SETVER.EXE ";

	// Calculate indentation sizes
	size_t indent_size_1 = 0;
	size_t indent_size_2 = 0;
	if (setver_table.is_global_version_set) {
		indent_size_1 = MSG_Get("PROGRAM_SETVER_GLOBAL").length();
	}
	for (const auto& [name, version] : setver_table.by_file_name) {
		indent_size_1 = std::max(indent_size_1, name.length());
	}
	for (const auto& [name, version] : setver_table.by_file_path) {
		indent_size_2 = std::max(indent_size_2, name.length());
	}
	const size_t min_space = 4; // min space between file name and version
	indent_size_1 += min_space;
	indent_size_2 = std::max(indent_size_1, indent_size_2 + min_space);
	if (indent_size_1 + min_space >= indent_size_2) {
		indent_size_1 = indent_size_2;
	}

	// Helper lambda to prepare string with indentation
	auto indent = [](const std::string& in_str, const size_t target_size) {
		assert(target_size > in_str.length());
		std::string indent_str = {};
		indent_str.resize(target_size - in_str.length(), ' ');
		return in_str + indent_str;
	};

	// Prepare display
	MoreOutputStrings output(*this);
	output.SetOptionNoPaging(!has_arg_paged);

	bool is_empty_line_needed = false;

	// Print global version override
	if (setver_table.is_global_version_set) {
		if (has_arg_batch) {
			output.AddString(":: %s\n",
			                 MSG_Get("PROGRAM_SETVER_BATCH_GLOBAL").c_str());

			output.AddString("%s%d.%02d /g /q\n",
			                 setver_command.c_str(),
			                 setver_table.version_global.major,
			                 setver_table.version_global.minor);
		} else {
			const auto tmp_str = indent(MSG_Get("PROGRAM_SETVER_GLOBAL"),
			                            indent_size_1);
			output.AddString("%s%d.%02d\n",
			                 tmp_str.c_str(),
			                 setver_table.version_global.major,
			                 setver_table.version_global.minor);
		}

		is_empty_line_needed = true;
	}

	// Print version override by file name / by file path

	auto print = [&](const NameVersionTable& table,
	                 const size_t indent_size,
	                 const char* batch_comment) {
		if (table.empty()) {
			return;
		}

		if (has_arg_batch) {
			output.AddString(":: %s\n", batch_comment);
		} else if (is_empty_line_needed) {
			output.AddString("\n");
		}

		for (const auto& [name, version] : table) {
			if (has_arg_batch) {
				output.AddString("%s\"%s\" %d.%02d /q\n",
				                 setver_command.c_str(),
				                 name.c_str(),
				                 version.major,
				                 version.minor);
			} else {
				const auto tmp_str = indent(name, indent_size);
				output.AddString("%s%d.%02d\n",
				                 tmp_str.c_str(),
				                 version.major,
				                 version.minor);
			}
		}

		is_empty_line_needed = !table.empty();
	};

	print(setver_table.by_file_name,
	      indent_size_1,
	      MSG_Get("PROGRAM_SETVER_BATCH_BY_FILE_NAME").c_str());

	print(setver_table.by_file_path,
	      indent_size_2,
	      MSG_Get("PROGRAM_SETVER_BATCH_BY_FILE_PATH").c_str());

	// Display the final result

	if (is_empty_line_needed) {
		output.AddString("\n");
	}

	output.Display();
}

void SETVER::AddToTable(const std::string& name, const FakeVersion& version)
{
	if (name.empty()) {
		setver_table.version_global        = version;
		setver_table.is_global_version_set = true;
		return;
	}

	if (IsNameWithPath(name)) {
		setver_table.by_file_path[name] = version;
	} else {
		setver_table.by_file_name[name] = version;
	}
}

std::string SETVER::FindKeyCaseInsensitive(const std::string& key,
                                           const NameVersionTable& table)
{
	for (const auto& [name, version] : table) {
		if (iequals(name, key)) {
			return name;
		}
	}

	return {};
}

bool SETVER::IsTableEmpty()
{
	return !setver_table.is_global_version_set &&
	       setver_table.by_file_name.empty() &&
	       setver_table.by_file_path.empty();
}

void SETVER::OverrideVersion(const std::string& canonical_name, DOS_PSP& psp)
{
	// Check for global version override

	if (setver_table.is_global_version_set) {
		const auto& version = setver_table.version_global;
		psp.SetVersion(version.major, version.minor);
	}

	// Check for version override - first by name with path

	auto try_override = [&](const std::string& name_str,
	                        const NameVersionTable& table) {
		const auto key = FindKeyCaseInsensitive(name_str, table);
		if (key.empty()) {
			return false;
		}
		const auto& version = table.at(key);
		psp.SetVersion(version.major, version.minor);
		return true;
	};

	if (try_override(canonical_name, setver_table.by_file_path)) {
		return;
	}

	// Check for version override by bare name, without path

	if (setver_table.by_file_name.empty()) {
		return;
	}

	const auto position = canonical_name.rfind('\\');
	if (position + 1 >= canonical_name.size()) {
		assert(false);
		return;
	}

	try_override(canonical_name.substr(position + 1), setver_table.by_file_name);
}

std_fs::path SETVER::GetTableFilePath()
{
	// Original SETVER.EXE stores the version table in its own executable;
	// this is not feasible in DOSBox, therefore we are using external file

	const auto section = get_section("dos");
	if (!section) {
		return {};
	}

	const PropPath* file_path = section->GetPath("setver_table_file");
	if (!file_path) {
		return {};
	}

	return file_path->realpath;
}

static std::ostream& operator<<(std::ostream& stream, const SETVER::FakeVersion& version)
{
	assert(version.major <= 9);
	assert(version.minor <= 99);

	stream << static_cast<int>(version.major) << ".";
	if (version.minor > 9) {
		stream << static_cast<int>(version.minor);
	} else {
		stream << "0" << static_cast<int>(version.minor);
	}

	return stream;
}

void SETVER::LoadTableFromFile()
{
	// Do nothing if no file name specified in the configuration
	const auto file_path = GetTableFilePath();
	if (file_path.empty()) {
		return;
	}

	// Helper warning lambdas

	bool already_warned_format  = false;
	bool already_warned_version = false;
	bool already_warned_name    = false;

	auto warn_file_format = [&]() {
		if (already_warned_format) {
			return;
		}
		LOG_WARNING("DOS: SETVER - table file seems to be of extended format, ignoring extra data");
		already_warned_format = true;
	};

	auto warn_version_parse = [&]() {
		if (already_warned_version) {
			return;
		}
		LOG_WARNING("DOS: SETVER - problem parsing DOS version");
		already_warned_version = true;
	};

	auto warn_file_name = [&]() {
		if (already_warned_name) {
			return;
		}
		LOG_WARNING("DOS: SETVER - problem parsing file name");
		already_warned_name = true;
	};

	// If the file does not exist, save default settings there
	if (!std_fs::exists(file_path)) {
		SaveTableToFile();
		return;
	}

	// Clear the table - but store previous as a backup
	const auto backup_table            = setver_table;
	setver_table.is_global_version_set = false;
	setver_table.by_file_name.clear();
	setver_table.by_file_path.clear();

	// Read table from the file
	std::ifstream file(file_path.c_str());
	std::string line = {};
	while (std::getline(file, line)) {
		// Parse line
		auto tokens = split_with_empties(line, '\t');
		// Skip empty lines
		if (tokens.empty()) {
			continue;
		}
		// Skip lines with only a single token, we are not currently
		// using such lines
		if (tokens.size() == 1) {
			warn_file_format();
			continue;
		}
		// Only consider first 2 tokens - ignore the remaining ones,
		// might be needed for some extensions in the future
		if (tokens.size() > 2) {
			warn_file_format();
			tokens.resize(2);
		}
		// First token should be a file name - it is quite likely the
		// disk is not mounted yet, so allow non-existing files
		constexpr bool allow_non_existing_files = true;
		if (!tokens[0].empty() &&
		    !PreprocessName(tokens[0], allow_non_existing_files)) {
			warn_file_name();
			continue;
		}
		// Second token should be a DOS version
		FakeVersion version = {};
		if (!ParseVersion(tokens[1], version)) {
			warn_version_parse();
			continue;
		}
		// Import row to the table
		AddToTable(tokens[0], version);
	}
	if (file.bad()) {
		setver_table = backup_table;
		LOG_WARNING("DOS: SETVER - error reading table file");
	}
	file.close();
}

void SETVER::SaveTableToFile()
{
	// Do nothing if no file name specified in the configuration
	const auto file_path = GetTableFilePath();
	if (file_path.empty()) {
		return;
	}

	// Do not store modifed table if we are in secure mode
	if (control->SecureMode()) {
		static bool already_warned = false;
		if (!already_warned) {
			LOG_WARNING("DOS: SETVER - secure mode, storing table skipped");
			already_warned = true;
		}
		return;
	}

	// Write table into the file
	std::ofstream file(file_path.c_str(), std::ios::trunc);
	if (setver_table.is_global_version_set) {
		file << "\t" << setver_table.version_global << '\n';
	}
	for (const auto& [name, version] : setver_table.by_file_name) {
		file << name << "\t" << version << '\n';
	}
	for (const auto& [name, version] : setver_table.by_file_path) {
		file << name << "\t" << version << '\n';
	}
	if (!file.good()) {
		LOG_WARNING("DOS: SETVER - error saving table file");
	}
	file.close();
}

void SETVER::AddMessages()
{
	MSG_Add("PROGRAM_SETVER_HELP_LONG",
	        "Display or set the DOS version reported to applications.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]setver[reset] \\[/b] [/p]\n"
	        "  [color=light-green]setver[reset] [color=light-cyan]FILE[reset] [color=light-cyan]VERSION[reset] [/q]\n"
	        "  [color=light-green]setver[reset] [color=light-cyan]FILE[reset] /d [/q]\n"
	        "  [color=light-green]setver[reset] [color=light-cyan]VERSION[reset] /g [/q]\n"
	        "  [color=light-green]setver[reset] /d /g [/q]\n"
	        "  [color=light-green]setver[reset] /d /all [/q]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]FILE[reset]     file (optionally with path) to apply the settings to\n"
	        "  [color=light-cyan]VERSION[reset]  DOS version, in [color=white]n[reset].[color=white]nn[reset], [color=white]n[reset].[color=white]n[reset] or [color=white]n[reset] format\n"
	        "  /g       global setting, applied to all executables\n"
	        "  /d       delete entry from version table\n"
	        "  /all     together with /d clears the whole version table\n"
	        "  /b       display the list in a batch file format\n"
	        "  /p       display one page a time\n"
	        "  /q       quiet, skip confirmation messages\n"
	        "  /delete and /quiet have same meaning as /d and /q, respectively\n"
	        "\n"
	        "Notes:\n"
	        "  For persistent version table, specify storage in the configuration file under\n"
	        "  the [dos] section, using the 'setver_table_file = [color=light-cyan]STORAGE[reset]' setting.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]setver[reset] /b              ; displays settings as a batch file\n"
	        "  [color=light-green]setver[reset] [color=light-cyan]RETRO.COM[reset] [color=white]6[reset].[color=white]22[reset]  ; reports DOS version 6.22 for every RETRO.COM file\n"
	        "  [color=light-green]setver[reset] [color=light-cyan]RETRO.COM[reset] /d    ; stop overriding DOS version reported\n");

	MSG_Add("PROGRAM_SETVER_WRONG_TABLE",
	        "Only version table in Z:\\ directory is supported.");
	MSG_Add("PROGRAM_SETVER_INVALID_VERSION", "Invalid DOS version.");

	MSG_Add("PROGRAM_SETVER_TABLE_UPDATED", "Version table updated.");
	MSG_Add("PROGRAM_SETVER_TABLE_CLEARED", "Version table cleared.");
	MSG_Add("PROGRAM_SETVER_TABLE_ALREADY_EMPTY", "Version table already empty.");
	MSG_Add("PROGRAM_SETVER_TABLE_ENTRY_REMOVED",
	        "Entry removed from version table.");
	MSG_Add("PROGRAM_SETVER_TABLE_ENTRY_NOT_FOUND",
	        "Entry not found in version table.");

	MSG_Add("PROGRAM_SETVER_TABLE_EMPTY", "Version table is empty.");
	MSG_Add("PROGRAM_SETVER_GLOBAL", "Global reported version");

	MSG_Add("PROGRAM_SETVER_BATCH_GLOBAL", "rule for every executable");
	MSG_Add("PROGRAM_SETVER_BATCH_BY_FILE_NAME",
	        "rules for matching by file name only");
	MSG_Add("PROGRAM_SETVER_BATCH_BY_FILE_PATH",
	        "rules for matching by file name with path");
}
