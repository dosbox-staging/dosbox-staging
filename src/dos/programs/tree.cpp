// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "tree.h"

#include "cpu/callback.h"
#include "dos/dos.h"
#include "dos/drives.h"
#include "ints/int10.h"
#include "misc/unicode.h"
#include "shell/shell.h"
#include "utils/checks.h"

CHECK_NARROWING();

constexpr auto MaxObjectsInDir = 0xffff; // FAT32 can't store more nevertheless

void TREE::Run()
{
	MoreOutputStrings output(*this);

	// Handle command line
	if (HelpRequested()) {
		output.AddString(MSG_Get("PROGRAM_TREE_HELP_LONG"));
		output.Display();
		return;
	}

	has_option_ascii = false;
	has_option_files = false;

	has_option_ascii = cmd->FindExistRemoveAll("/a");
	has_option_files = cmd->FindExistRemoveAll("/f");
	// DR-DOS
	has_option_brief = cmd->FindExistRemoveAll("/b");
	// DR-DOS, pdTree
	has_option_paging = cmd->FindExistRemoveAll("/p");
	// According to http://help.fdos.org/en/hhstndrd/tree.htm there were
	// plans to implement the following options in FreeDOS:
	// - /DF - display file sizes
	// - /DA - display attributes (it even works, although is undocumented)
	// - /DH - display hidden and system files (normally not shown)
	// - /DR - display results (file and subdirectory count) after each one
	// - /On - sort options, like with DIR command
	has_option_attr   = cmd->FindExistRemoveAll("/da");
	has_option_size   = cmd->FindExistRemoveAll("/df");
	has_option_hidden = cmd->FindExistRemoveAll("/dh");

	int sorting_switch_count = 0;
	if (cmd->FindExistRemoveAll("/on")) {
		option_sorting = ResultSorting::ByName;
		option_reverse = false;
		sorting_switch_count++;
	}
	if (cmd->FindExistRemoveAll("/o-n")) {
		option_sorting = ResultSorting::ByName;
		option_reverse = true;
		sorting_switch_count++;
	}
	if (cmd->FindExistRemoveAll("/os")) {
		option_sorting = ResultSorting::BySize;
		option_reverse = false;
		sorting_switch_count++;
	}
	if (cmd->FindExistRemoveAll("/o-s")) {
		option_sorting = ResultSorting::BySize;
		option_reverse = true;
		sorting_switch_count++;
	}
	if (cmd->FindExistRemoveAll("/od")) {
		option_sorting = ResultSorting::ByDateTime;
		option_reverse = false;
		sorting_switch_count++;
	}
	if (cmd->FindExistRemoveAll("/o-d")) {
		option_sorting = ResultSorting::ByDateTime;
		option_reverse = true;
		sorting_switch_count++;
	}
	if (cmd->FindExistRemoveAll("/oe")) {
		option_sorting = ResultSorting::ByExtension;
		option_reverse = false;
		sorting_switch_count++;
	}
	if (cmd->FindExistRemoveAll("/o-e")) {
		option_sorting = ResultSorting::ByExtension;
		option_reverse = true;
		sorting_switch_count++;
	}
	if (sorting_switch_count > 1) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH_COMBO"));
		return;
	}

	// TODO: consider implementing /DR in some form

	// Make sure no other switches are supplied
	std::string tmp_str = {};
	if (cmd->FindStringBeginCaseSensitive("/", tmp_str)) {
		tmp_str = std::string("/") + tmp_str;
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), tmp_str.c_str());
		return;
	}

	// Check if directory is provided
	const auto params = cmd->GetArguments();
	if (params.size() > 1) {
		WriteOut(MSG_Get("SHELL_TOO_MANY_PARAMETERS"));
		return;
	}

	output.SetOptionNoPaging(!has_option_paging);

	// Get the start directory
	std::string path = {};
	char tmp[DOS_PATHLENGTH + 8]; // extra bytes for drive letter, etc.
	if (params.empty()) {
		if (!DOS_GetCurrentDir(0, tmp)) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			return;
		}

		path = static_cast<char>('A' + DOS_GetDefaultDrive());
		path = path + ":\\" + tmp;
	} else {
		path = params[0];
	}
	if (!DOS_Canonicalize(path.c_str(), tmp)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}
	path = tmp;
	if (path.back() == '\\') {
		path.pop_back();
	}

	// Check if directory exists
	FatAttributeFlags attr = {};
	if (!DOS_GetFileAttr(path.c_str(), &attr) || !attr.directory) {
		WriteOut(MSG_Get("SHELL_DIRECTORY_NOT_FOUND"), path.c_str());
		return;
	}

	// Determine maximum number of columns
	constexpr uint16_t min_columns = 40;
	max_columns = std::max(min_columns, INT10_GetTextColumns());

	// Display volume information
	const auto idx = drive_index(path[0]);
	if (!has_option_brief) {
		const auto label = To_Label(Drives.at(idx)->GetLabel());
		output.AddString(MSG_Get("PROGRAM_TREE_DIRECTORY"), label.c_str());
		output.AddString("\n\n");
		// TODO: display volume serial number in DIR and TREE commands
	}

	// Display the tree
	PreRender();
	MaybeDisplayInfoSpace(output);
	const auto len_limit = max_columns - GetInfoSpaceSize();
	tmp_str              = has_option_paging
	                             ? shorten_path(path, static_cast<uint16_t>(len_limit))
	                             : path;
	output.AddString("%s\n", tmp_str.c_str());
	DisplayTree(output, path + '\\');

	if (!skip_empty_line) {
		output.AddString("\n");
	}

	output.Display();
}

void TREE::PreRender()
{
	bool use_ascii_fallback = false;

	auto to_dos = [](const std::string& in_str) {
		return utf8_to_dos(in_str,
		                   DosStringConvertMode::NoSpecialCharacters,
		                   UnicodeFallback::EmptyString);
	};

	if (!has_option_ascii) {
		// If current code page misses one or more characters used to
		// draw the tree, use standard 7-bit ASCII characters
		use_ascii_fallback = to_dos("─├│└").empty();
	}

	if (has_option_ascii || use_ascii_fallback) {
		str_child  = "|---";
		str_last   = "\\---";
		str_indent = "|   ";
	} else {
		str_child  = to_dos("├───");
		str_last   = to_dos("└───");
		str_indent = to_dos("│   ");

		assert(!str_child.empty());
		assert(!str_last.empty());
		assert(!str_indent.empty());
	}
}

void TREE::MaybeDisplayInfo(MoreOutputStrings& output,
                            const DOS_DTA::Result& entry) const
{
	if (has_option_size && has_option_files) {
		if (entry.IsFile()) {
			output.AddString("%13s ", format_number(entry.size).c_str());
		} else {
			output.AddString("              ");
		}
		if (has_option_attr) {
			output.AddString("  ");
		}
	}
	if (has_option_attr) {
		output.AddString("%c %c%c%c ",
		                 entry.attr.archive ? 'A' : '-',
		                 entry.attr.hidden ? 'H' : '-',
		                 entry.attr.system ? 'S' : '-',
		                 entry.attr.read_only ? 'R' : '-');
	}
}

void TREE::MaybeDisplayInfoSpace(MoreOutputStrings& output) const
{
	if (has_option_size && has_option_files) {
		output.AddString("              ");
		if (has_option_attr) {
			output.AddString("  ");
		}
	}
	if (has_option_attr) {
		output.AddString("      ");
	}
}

uint8_t TREE::GetInfoSpaceSize() const
{
	uint8_t result = 0;
	if (has_option_size && has_option_files) {
		result = static_cast<uint8_t>(result + 14);
		if (has_option_attr) {
			result = static_cast<uint8_t>(result + 2);
		}
	}
	if (has_option_attr) {
		result = static_cast<uint8_t>(result + 6);
	}
	return result;
}

bool TREE::DisplayTree(MoreOutputStrings& output, const std::string& path,
                       const uint16_t depth, const std::string& tree)
{
	auto display_empty = [&] {
		if (skip_empty_line) {
			return;
		}
		MaybeDisplayInfoSpace(output);
		output.AddString("%s\n", tree.c_str());
		skip_empty_line = true;
	};

	auto should_display = [&](const DOS_DTA::Result& result) {
		if (!has_option_hidden && (result.attr.system | result.attr.hidden)) {
			return false;
		}
		if (!result.IsDirectory()) {
			return has_option_files;
		}
		if (result.IsDummyDirectory()) {
			return false;
		}
		return true;
	};

	std::vector<DOS_DTA::Result> dir_contents;
	size_t num_subdirs = 0;

	// Get directory content

	const RealPt save_dta = dos.dta();
	dos.dta(dos.tables.tempdta);

	const auto pattern = path + "*.*";
	FatAttributeFlags flags = {};
	flags.system    = true;
	flags.hidden    = true;
	flags.directory = true;

	bool has_next_entry = DOS_FindFirst(pattern.c_str(), flags._data);
	size_t space_needed = 7; // length of indentation + ellipsis

	while (!DOSBOX_IsShutdownRequested() && has_next_entry) {
		DOS_DTA::Result result = {};

		const DOS_DTA dta(dos.dta());
		dta.GetResult(result);
		assert(!result.name.empty());

		has_next_entry = DOS_FindNext();

		if (!should_display(result)) {
			continue;
		}

		space_needed = std::max(space_needed, result.name.size());

		dir_contents.emplace_back(result);
		if (result.IsDirectory()) {
			++num_subdirs;
		}

		if (dir_contents.size() > MaxObjectsInDir) {
			dos.dta(save_dta);
			output.AddString("\n");
			output.AddString(
			        MSG_Get("PROGRAM_TREE_TOO_MANY_FILES_SUBDIRS"));
			output.AddString("\n");
			return false;
		}
	}

	dos.dta(save_dta);

	// If paging is enabled, chek if we have enough screen horizontal space
	// to display this directory

	space_needed += GetInfoSpaceSize();
	space_needed += tree.size() + str_indent.size();

	if (has_option_paging && space_needed > max_columns) {
		// Not enough space, we can't display this directory
		MaybeDisplayInfoSpace(output);
		output.AddString("%s    ...\n", tree.c_str());
		skip_empty_line = false;
		if (has_option_files) {
			// If listing files, separate directories with empty lines
			display_empty();
		}

		return output.DisplayPartial();
	}

	// Sort the directory, files first

	DOS_Sort(dir_contents, option_sorting, option_reverse, ResultGrouping::FilesFirst);

	// Display directory, dive into subdirectories

	size_t subdir_counter = 0;

	bool is_first_entry = true;
	for (const auto& entry : dir_contents) {
		if (entry.IsDirectory()) {
			if (has_option_files && !subdir_counter && !is_first_entry) {
				MaybeDisplayInfoSpace(output);
				output.AddString("%s%s\n",
				                 tree.c_str(),
				                 str_indent.c_str());
			}
			++subdir_counter;
		}

		MaybeDisplayInfo(output, entry);

		std::string graph = tree;
		if (subdir_counter < num_subdirs) {
			graph += entry.IsDirectory() ? str_child : str_indent;
		} else {
			graph += entry.IsDirectory() ? str_last : "    ";
		}
		output.AddString("%s%s\n", graph.c_str(), entry.name.c_str());
		skip_empty_line = false;

		if (CALLBACK_Idle()) {
			break;
		}

		if (entry.IsDirectory() &&
		    !DisplayTree(output,
		                 path + entry.name + "\\",
		                 static_cast<uint16_t>(depth + 1),
		                 (subdir_counter < num_subdirs) ? tree + str_indent
		                                                : tree + "    ")) {
			return false;
		}

		is_first_entry = false;
	}

	if (is_first_entry && !depth) {
		output.AddString("\n");
		output.AddString(MSG_Get(has_option_files
		                                 ? "SHELL_NO_FILES_SUBDIRS_TO_DISPLAY"
		                                 : "SHELL_NO_SUBDIRS_TO_DISPLAY"));
	} else if (has_option_files) {
		// If listing files, separate directories with empty lines
		display_empty();
	}

	return output.DisplayPartial();
}

void TREE::AddMessages()
{
	MSG_Add("PROGRAM_TREE_HELP_LONG",
	        "Display directory tree in a graphical form.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]tree[reset] [color=light-cyan][DIRECTORY][reset] [/a] \\[/b] [/f] [/p] [/da] [/df] [/dh] [/o[color=white]ORDER[reset]]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]DIRECTORY[reset]  name of the directory to display\n"
	        "  [color=white]ORDER[reset]      listing order, one of:\n"
	        "                 [color=white]n[reset] (by name, alphabetic),\n"
	        "                 [color=white]s[reset] (by size, smallest first),\n"
	        "                 [color=white]e[reset] (by extension, alphabetic),\n"
	        "                 [color=white]d[reset] (by date/time, oldest first),\n"
	        "             with an optional [color=white]-[reset] prefix to reverse order\n"
	        "  /a         use only 7-bit ASCII characters\n"
	        "  /b         brief display, omit header and footer information\n"
	        "  /f         also display files\n"
	        "  /p         display one page a time, shorten output to fit screen width\n"
	        "  /da        display attributes\n"
	        "  /df        display size for files\n"
	        "  /dh        also display hidden and system files/directories\n"
	        "  /o[color=white]ORDER[reset]    order the list (see above)\n"
	        "\n"
	        "Notes:\n"
	        "  If [color=light-cyan]DIRECTORY[reset] is omitted, the current directory is used.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]tree[reset]          ; displays directory tree starting from current directory\n"
	        "  [color=light-green]tree[reset] [color=light-cyan]C:[reset] /f    ; displays C: drive content recursively, with files\n");

	MSG_Add("PROGRAM_TREE_DIRECTORY", " Directory tree for volume %s");

	MSG_Add("PROGRAM_TREE_TOO_MANY_FILES_SUBDIRS",
	        "Too many files or subdirectories.\n");
}
