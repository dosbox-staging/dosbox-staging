/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2023  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include "shell.h"

#include "../dos/program_more_output.h"
#include "checks.h"

CHECK_NARROWING();


void DOS_Shell::CMD_DIR()
{
	std::string variable = {};
	if (GetEnvStr("DIRCMD", variable)) {
		cmd->AddEnvArguments(variable);
	}

	bool has_option_1000s_separator    = cmd->FindRemoveExist("/c");
	bool has_option_no_1000s_separator = cmd->FindRemoveExist("/-c");
	bool has_option_wide_by_column     = cmd->FindRemoveExist("/d");
	bool has_option_bare               = cmd->FindRemoveExist("/b");
	bool has_option_lowercase          = cmd->FindRemoveExist("/l");
	bool has_option_list_far_right     = cmd->FindRemoveExist("/n");
	bool has_option_paging             = cmd->FindRemoveExist("/p");
	bool has_option_in_subdirectories  = cmd->FindRemoveExist("/s");
	bool has_option_wide_by_row        = cmd->FindRemoveExist("/w");

	if (cmd->FindRemoveExist("/wp", "/pw")) {
		has_option_paging      = true;
		has_option_wide_by_row = true;
	}

	std::string attributes_str = {};
	bool has_option_attributes = cmd->FindRemoveStringBegin("/a", attributes_str);
	std::string sorting_str    = {};
	bool has_option_sorting    = cmd->FindRemoveStringBegin("/o", sorting_str);

	// Make sure no other switches are supplied
	if (!CheckAllSwitchesHandled()) {
		return; // DOS error already printed by check routine
	}

/* XXX
	// Sorting flags
	bool option_reverse          = false;
	ResultSorting option_sorting = ResultSorting::None;
	if (ScanCMDBool(args, "ON")) {
		option_sorting = ResultSorting::ByName;
		option_reverse = false;
	}
	if (ScanCMDBool(args, "O-N")) {
		option_sorting = ResultSorting::ByName;
		option_reverse = true;
	}
	if (ScanCMDBool(args, "OD")) {
		option_sorting = ResultSorting::ByDateTime;
		option_reverse = false;
	}
	if (ScanCMDBool(args,"O-D")) {
		option_sorting = ResultSorting::ByDateTime;
		option_reverse = true;
	}
	if (ScanCMDBool(args, "OE")) {
		option_sorting = ResultSorting::ByExtension;
		option_reverse = false;
	}
	if (ScanCMDBool(args,"O-E")) {
		option_sorting = ResultSorting::ByExtension;
		option_reverse = true;
	}
	if (ScanCMDBool(args, "OS")) {
		option_sorting = ResultSorting::BySize;
		option_reverse = false;
	}
	if (ScanCMDBool(args,"O-S")) {
		option_sorting = ResultSorting::BySize;
		option_reverse = true;
	}

	const std::string pattern = to_search_pattern(args);

	// Make a full path in the args
	char path[DOS_PATHLENGTH];
	if (!DOS_Canonicalize(pattern.c_str(), path)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}

	// Prepare display engine
	MoreOutputStrings output(*this);
	output.SetOptionNoPaging(!has_option_paging);

	// DIR cmd in DOS and cmd.exe format 'Directory of <path>'
	// accordingly:
	// - only directory part of pattern passed as an argument
	// - do not append '\' to the directory name
	// - for root directories/drives: append '\' to the name
	char *last_dir_sep = strrchr(path, '\\');
	if (last_dir_sep == path + 2)
		*(last_dir_sep + 1) = '\0';
	else
		*last_dir_sep = '\0';

	const char drive_letter = path[0];
	const auto drive_idx = drive_index(drive_letter);
	const bool print_label  = (drive_letter >= 'A') && Drives.at(drive_idx);

	if (!has_option_bare) {
		if (print_label) {
			const auto label = To_Label(Drives.at(drive_idx)->GetLabel());
			output.AddString(MSG_Get("SHELL_CMD_DIR_VOLUME"),
			                 drive_letter,
			                 label.c_str());
		}
		// TODO: display volume serial number in DIR and TREE commands
		output.AddString(MSG_Get("SHELL_CMD_DIR_INTRO"), path);
		output.AddString("\n");
	}

	const bool is_root = strnlen(path, sizeof(path)) == 3;

	// Command uses dta so set it to our internal dta
	const RealPt save_dta = dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());

	bool ret = DOS_FindFirst(pattern.c_str(), FatAttributeFlags::NotVolume);
	if (!ret) {
		if (!has_option_bare)
			output.AddString(MSG_Get("SHELL_FILE_NOT_FOUND"),
			                 pattern.c_str());
		dos.dta(save_dta);
		output.Display();
		return;
	}

	std::vector<DOS_DTA::Result> dir_contents;

	do { // File name and extension
		DOS_DTA::Result result = {};
		dta.GetResult(result);

		// Skip non-directories if option AD is present,
		// or skip dirs in case of A-D
		if (has_option_all_dirs && !result.IsDirectory()) {
			continue;
		} else if (has_option_all_files && result.IsDirectory()) {
			continue;
		}

		dir_contents.emplace_back(result);

	} while (DOS_FindNext());

	DOS_Sort(dir_contents, option_sorting, option_reverse);

	size_t byte_count   = 0;
	uint32_t file_count = 0;
	uint32_t dir_count  = 0;
	size_t wide_count   = 0;

	for (const auto& entry : dir_contents) {
		// Skip listing . and .. from toplevel directory, to simulate
		// DIR output correctly.
		// Bare format never lists .. nor . as directories.
		if (is_root || has_option_bare) {
			if (entry.IsDummyDirectory()) {
				continue;
			}
		}

		if (entry.IsDirectory()) {
			dir_count += 1;
		} else {
			file_count += 1;
			byte_count += entry.size;
		}

		// 'Bare' format: just the name, one per line, nothing else
		//
		if (has_option_bare) {
			output.AddString("%s\n", entry.name.c_str());
			continue;
		}

		// 'Wide list' format: using several columns
		//
		if (has_option_wide) {
			if (entry.IsDirectory()) {
				const int length = static_cast<int>(entry.name.length());
				output.AddString("[%s]%*s",
				                 entry.name.c_str(),
				                 (14 - length),
				                 "");
			} else {
				output.AddString("%-16s", entry.name.c_str());
			}
			wide_count += 1;
			if (!(wide_count % 5)) {
				// TODO: should auto-adapt to screen width
				output.AddString("\n");
			}
			continue;
		}

		// default format: one detailed entry per line
		//
		const auto year   = static_cast<uint16_t>((entry.date >> 9) + 1980);
		const auto month  = static_cast<uint8_t>((entry.date >> 5) & 0x000f);
		const auto day    = static_cast<uint8_t>(entry.date & 0x001f);
		const auto hour   = static_cast<uint8_t>((entry.time >> 5) >> 6);
		const auto minute = static_cast<uint8_t>((entry.time >> 5) & 0x003f);

		output.AddString("%-8s %-3s   %21s %s %s\n",
		                 entry.GetBareName().c_str(),
		                 entry.GetExtension().c_str(),
		                 entry.IsDirectory()
		                         ? "<DIR>"
		                         : format_number(entry.size).c_str(),
		                 format_date(year, month, day),
		                 format_time(hour, minute, 0, 0));
	}

	// Additional newline in case last line in 'Wide list` format was not
	// wrapped automatically
	if (has_option_wide && (wide_count % 5)) {
		// TODO: should auto-adapt to screen width
		output.AddString("\n");
	}

	// Show the summary of results
	if (!has_option_bare) {
		output.AddString(MSG_Get("SHELL_CMD_DIR_BYTES_USED"),
		                 file_count,
		                 format_number(byte_count).c_str());

		uint8_t drive = dta.GetSearchDrive();
		size_t free_space = 1024 * 1024 * 100;
		if (Drives.at(drive)) {
			uint16_t bytes_sector;
			uint8_t  sectors_cluster;
			uint16_t total_clusters;
			uint16_t free_clusters;
			Drives.at(drive)->AllocationInfo(&bytes_sector,
			                                 &sectors_cluster,
			                                 &total_clusters,
			                                 &free_clusters);
			free_space = bytes_sector;
			free_space *= sectors_cluster;
			free_space *= free_clusters;
		}

		output.AddString(MSG_Get("SHELL_CMD_DIR_BYTES_FREE"),
		                 dir_count,
		                 format_number(free_space).c_str());
	}
	dos.dta(save_dta);
	output.Display();
	*/
}
