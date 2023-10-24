/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "program_tree.h"

#include "callback.h"
#include "checks.h"
#include "dos_inc.h"
#include "drives.h"
#include "shell.h"
#include "string_utils.h"

#include "../ints/int10.h"

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

	constexpr bool remove_if_found = true;

	has_option_ascii = cmd->FindExist("/a", remove_if_found);
	has_option_files = cmd->FindExist("/f", remove_if_found);
	// DR-DOS
	has_option_brief = cmd->FindExist("/b", remove_if_found);
	// DR-DOS, pdTree
	has_option_paging = cmd->FindExist("/p", remove_if_found);
	// According to http://help.fdos.org/en/hhstndrd/tree.htm there were
	// plans to implement the following options in FreeDOS:
	// - /DF - display file sizes
	// - /DA - display attributes (it even works, although is undocumented)
	// - /DH - display hidden and system files (normally not shown)
	// - /DR - display results (file and subdirectory count) after each one
	// - /On - sort options, like with DIR command
	has_option_attr   = cmd->FindExist("/da", remove_if_found);
	has_option_size   = cmd->FindExist("/df", remove_if_found);
	has_option_hidden = cmd->FindExist("/dh", remove_if_found);
	if (cmd->FindExist("/on", remove_if_found)) {
		option_sorting = ResultSorting::ByName;
		option_reverse = false;
	}
	if (cmd->FindExist("/o-n", remove_if_found)) {
		option_sorting = ResultSorting::ByName;
		option_reverse = true;
	}
	if (cmd->FindExist("/os", remove_if_found)) {
		option_sorting = ResultSorting::BySize;
		option_reverse = false;
	}
	if (cmd->FindExist("/o-s", remove_if_found)) {
		option_sorting = ResultSorting::BySize;
		option_reverse = true;
	}
	if (cmd->FindExist("/od", remove_if_found)) {
		option_sorting = ResultSorting::ByDateTime;
		option_reverse = false;
	}
	if (cmd->FindExist("/o-d", remove_if_found)) {
		option_sorting = ResultSorting::ByDateTime;
		option_reverse = true;
	}
	if (cmd->FindExist("/oe", remove_if_found)) {
		option_sorting = ResultSorting::ByExtension;
		option_reverse = false;
	}
	if (cmd->FindExist("/o-e", remove_if_found)) {
		option_sorting = ResultSorting::ByExtension;
		option_reverse = true;
	}
	// TODO: consider implementing /DR in some form

	// Make sure no other switches are supplied
	std::string tmp_str = {};
	if (cmd->FindStringBegin("/", tmp_str)) {
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

	if (!has_option_ascii) {
		// If current code page misses one or more characters used to
		// draw the tree, use standard 7-bit ASCII characters

		std::string tmp_str;
		utf8_to_dos("─├│└", tmp_str, UnicodeFallback::Null);
		if (tmp_str.find('\0') != std::string::npos) {
			use_ascii_fallback = true;
		}
	}

	if (has_option_ascii || use_ascii_fallback) {
		str_child  = "|---";
		str_last   = "\\---";
		str_indent = "|   ";
	} else {
		utf8_to_dos("├───", str_child, UnicodeFallback::Null);
		utf8_to_dos("└───", str_last, UnicodeFallback::Null);
		utf8_to_dos("│   ", str_indent, UnicodeFallback::Null);
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

	while (!shutdown_requested && has_next_entry) {
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

		CALLBACK_Idle();
		if (shutdown_requested) {
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
	        "Displays directory tree in a graphical form.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]tree[reset] [color=light-cyan][DIRECTORY][reset] [/a] \\[/b] [/f] [/p] [/da] [/df] [/dh] [/o[color=white]ORDER[reset]]\n"
	        "\n"
	        "Where:\n"
	        "  [color=light-cyan]DIRECTORY[reset] is the name of the directory to display.\n"
	        "  [color=white]ORDER[reset]     is a listing order, one of:\n"
	        "                [color=white]n[reset] (by name, alphabetic),\n"
	        "                [color=white]s[reset] (by size, smallest first),\n"
	        "                [color=white]e[reset] (by extension, alphabetic),\n"
	        "                [color=white]d[reset] (by date/time, oldest first),\n"
	        "            with an optional [color=white]-[reset] prefix to reverse order.\n"
	        "  /a        uses only 7-bit ASCII characters.\n"
	        "  /b        brief display, omits header and footer information.\n"
	        "  /f        also display files.\n"
	        "  /p        display one page a time, shorten output to fit width width.\n"
	        "  /da       display attributes.\n"
	        "  /df       display size for files.\n"
	        "  /dh       also display hidden and system files/directories.\n"
	        "  /o[color=white]ORDER[reset]   orders the list (see above)\n"
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
