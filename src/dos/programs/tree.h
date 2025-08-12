// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_TREE_H
#define DOSBOX_PROGRAM_TREE_H

#include "dos/programs.h"

#include "more_output.h"

class TREE final : public Program {
public:
	TREE()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::File,
		               HELP_CmdType::Program,
		               "TREE"};
	}
	void Run() override;

private:
	void PreRender();
	bool DisplayTree(MoreOutputStrings& output, const std::string& path,
	                 const uint16_t depth = 0, const std::string& tree = "");

	void MaybeDisplayInfo(MoreOutputStrings& output,
	                      const DOS_DTA::Result& entry) const;
	void MaybeDisplayInfoSpace(MoreOutputStrings& output) const;

	uint8_t GetInfoSpaceSize() const;

	bool has_option_ascii  = false;
	bool has_option_brief  = false;
	bool has_option_files  = false;
	bool has_option_paging = false;
	bool has_option_attr   = false;
	bool has_option_size   = false;
	bool has_option_hidden = false;

	ResultSorting option_sorting = ResultSorting::None;
	bool option_reverse = false;

	bool skip_empty_line = false;

	uint16_t max_columns = 0;

	// Strings for drawing the directory tree
	std::string str_child  = {}; // child node here
	std::string str_last   = {}; // last child node
	std::string str_indent = {}; // indentation only, no child node

	static void AddMessages();
};

#endif
