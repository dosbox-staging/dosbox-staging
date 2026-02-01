// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mem.h"

#include "cpu/callback.h"
#include "cpu/cpu.h"
#include "cpu/registers.h"
#include "hardware/port.h"
#include "ints/bios.h"
#include "ints/ems.h"
#include "more_output.h"
#include "shell/shell.h"
#include "utils/checks.h"
#include "utils/math_utils.h"

CHECK_NARROWING();

const std::string MEM::Indentation = "  ";

// References:
// - FreeDOS implementation source code, https://gitlab.com/FreeDOS/base/mem/

void MEM::Run()
{
	MoreOutputStrings output(*this);

	if (HelpRequested()) {
		output.AddString(MSG_Get("PROGRAM_MEM_HELP_LONG"));
		output.Display();
		return;
	}

	constexpr bool RemoveIfFound = true;

	std::string module_name = {};

	// MS-DOS compatible options
	const auto has_option_paging   = cmd->FindExistRemoveAll("/p", "/page");
	const auto has_option_classify = cmd->FindExistRemoveAll("/c", "/classify");
	const auto has_option_debug    = cmd->FindExistRemoveAll("/d", "/debug");
	const auto has_option_free     = cmd->FindExistRemoveAll("/f", "/free");
	// Module can be specified after colon or as a separate argument
	const auto has_option_module = cmd->FindExistRemoveAll("/m", "/module");
	const auto has_option_module_colon =
	        cmd->FindStringBegin("/m:", module_name, RemoveIfFound) ||
	        cmd->FindStringBegin("/module:", module_name, RemoveIfFound);
	// FreeDOS extesions
	const auto has_option_xms = cmd->FindExistRemoveAll("/x", "/xms");
	const auto has_option_ems = cmd->FindExistRemoveAll("/e", "/ems");

	// Check that only one report is selected
	const std::vector<bool> all_selected = {has_option_classify,
	                                        has_option_debug,
	                                        has_option_free,
	                                        has_option_module,
	                                        has_option_module_colon,
	                                        has_option_xms,
	                                        has_option_ems};

	const auto num_selected = std::ranges::count_if(all_selected.begin(),
	                                                all_selected.end(),
	                                                [](bool v) { return v; });

	std::string tmp = {};
	if (num_selected > 1 || cmd->FindStringBegin("/m:", tmp) ||
	    cmd->FindStringBegin("/module:", tmp)) {

		// Either multiple different switches were selected or multiple
		// modules were given using '/m:MODULE' or '/module:MODULE'
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH_COMBO"));
		return;
	}

	// Check that no unknown switches were given
	if (cmd->FindStringBegin("/", tmp)) {
		tmp = std::string("/") + tmp;
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), tmp.c_str());
		return;
	}

	// Check the extracted module name after colon is not empty
	if (has_option_module_colon && module_name.empty()) {
		WriteOut(MSG_Get("SHELL_MISSING_PARAMETER"));
		return;
	}

	// Get the module name if needed and not already known
	const auto remaining_args = cmd->GetArguments();
	if (has_option_module) {
		if (remaining_args.size() == 1) {
			module_name = remaining_args[0];
		} else if (remaining_args.empty()) {
			WriteOut(MSG_Get("SHELL_MISSING_PARAMETER"));
			return;
		} else {
			WriteOut(MSG_Get("SHELL_TOO_MANY_PARAMETERS"));
			return;
		}
	} else if (!remaining_args.empty()) {
		WriteOut(MSG_Get("SHELL_TOO_MANY_PARAMETERS"));
		return;
	}

	// Command syntax is correct, do the processing
	output.SetOptionNoPaging(!has_option_paging);

	// Contrary to the original MS-DOS MEM.EXE, we do not display summary
	// under other reports; this is a conscious decision, the summary
	// clutters the output with information which is often not needed.

	std::string error_string = {};
	if (has_option_classify) {
		error_string = DisplayClassify(output);
	} else if (has_option_debug) {
		error_string = DisplayDebug(output);
	} else if (has_option_free) {
		error_string = DisplayFree(output);
	} else if (has_option_module || has_option_module_colon) {
		error_string = DisplayModule(output, module_name);
	} else if (has_option_xms) {
		error_string = DisplayXms(output);
	} else if (has_option_ems) {
		error_string = DisplayEms(output);
	} else {
		assert(num_selected == 0);
		error_string = DisplaySummary(output);
	}

	if (error_string.empty()) {
		output.AddString("\n");
		output.Display();
	} else {
		WriteOut(error_string);
	}
}

std::string MEM::DisplaySummary(MoreOutputStrings& output) const
{
	// Rounding values to kilobytes is not realized the same way as in
	// MS-DOS; the original MEM.EXE can display 634 KB free and 7 KB used
	// conventional memory, which does not sum to 640 KB - so let's not
	// copy the behavior.

	const auto memory = GetMemoryInfo();

	const auto xms = GetXmsInfo();
	const auto ems = GetEmsInfo();

	const auto& hma = xms.hma;
	const auto& umb = memory.umb;

	output.AddString(MSG_Get("PROGRAM_MEM_SUMMARY_TABLE_HEADER") + "\n");
	output.AddString(MSG_Get("PROGRAM_MEM_SUMMARY_TABLE_HORIZONTAL_LINE") + "\n");

	const auto row_format = MSG_Get("PROGRAM_MEM_SUMMARY_TABLE_ROW_FORMAT");

	auto display_row = [&output, &row_format](const std::string& type_string_id,
	                                          const size_t free_bytes,
	                                          const std::optional<size_t> total_bytes) {
		const std::string value_total = total_bytes
		                                  ? ToKbString(*total_bytes)
		                                  : "";
		const std::string value_used  = total_bytes
		                                  ? ToKbString(*total_bytes - free_bytes)
		                                  : "";
		const std::string value_free  = ToKbString(free_bytes);

		output.AddString(row_format + "\n",
		                 MSG_Get(type_string_id).c_str(),
		                 value_total.c_str(),
		                 value_used.c_str(),
		                 value_free.c_str());
	};

	display_row("PROGRAM_MEM_TYPE_CONVENTIONAL",
	            memory.free_bytes,
	            memory.total_bytes);

	// We do not calculate the reserved memory size. It is not clear how the
	// MS-DOS MEM.EXE calculates it (FreeDOS MEM.EXE sometimes gives
	// different value, at least when running under Windows 3.1x), and
	// DR-DOS MEM.EXE does not print it at all.
	// Plus, the value is probably not usable for anything.

	if (umb.is_available) {
		display_row("PROGRAM_MEM_TYPE_UMB", umb.free_bytes, umb.total_bytes);
	}
	if (hma.is_available) {
		display_row("PROGRAM_MEM_TYPE_HMA", hma.free_bytes, HmaSizeBytes);
	}
	if (xms.is_available) {
		display_row("PROGRAM_MEM_TYPE_XMS", xms.free_bytes, xms.total_bytes);
	}
	if (ems.is_available) {
		display_row("PROGRAM_MEM_TYPE_EMS", ems.free_bytes, ems.total_bytes);
	}

	output.AddString(MSG_Get("PROGRAM_MEM_SUMMARY_TABLE_HORIZONTAL_LINE") + "\n");

	const size_t free_under_1M  = memory.free_bytes + umb.free_bytes;
	const size_t total_under_1M = memory.total_bytes + umb.total_bytes;
	display_row("PROGRAM_MEM_TYPE_UNDER_1MB", free_under_1M, total_under_1M);

	output.AddString("\n");

	// We do not display the total amount of memory, as this might be hard
	// to calculate; for example EMS can be simulated on top of XMS, or can
	// be a distinct hardware RAM, and it's hard to detect which is true.

	ValueList values = {};

	auto get_memsize_value = [](const size_t value) {
		return format_str("%4s %8s %s)",
		                  ToKbString(value).c_str(),
		                  (std::string("(") + ToBytesString(value)).c_str(),
		                  MSG_Get("PROGRAM_MEM_BYTES").c_str());
	};

	// It seems the value we calculates is the same as the largest
	// executable size calculated by DR-DOS MEM.EXE, but often higher than
	// the largest executable calculated by MS-DOS MEM.EXE.
	// Not sure which tool is correct (or maybe both are wrong?), or how the
	// value should be calculated - therefore value is shown as the largest
	// available block of conventional memory and described this way.

	const auto label_largest = MSG_Get("PROGRAM_MEM_LABEL_LARGEST");
	const auto value_largest = get_memsize_value(memory.largest_free_block);
	values.emplace_back(label_largest, value_largest);

	if (umb.is_available) {
		const auto label_largest_umb = MSG_Get("PROGRAM_MEM_LABEL_LARGEST_UMB");
		const auto value_largest_umb = get_memsize_value(umb.largest_free_block);
		values.emplace_back(label_largest_umb, value_largest_umb);
	}

	DisplayValues(output, values);
	return {};
}

std::string MEM::DisplayClassify(MoreOutputStrings& output) const
{
	const auto memory = GetMemoryInfo();
	const auto& umb   = memory.umb;

	const auto this_psp = psp->GetSegment();

	const auto row_format = MSG_Get("PROGRAM_MEM_CLASSIFY_TABLE_ROW_FORMAT") + "\n";

	auto display_info = [&output, &memory, &umb, &row_format, &this_psp](
	                            const uint16_t segment, const std::string& name) {
		size_t total_memory = 0;
		size_t total_umb    = 0;
		for (const auto& entry : memory.mcb_chain_info) {
			if (entry.psp_segment == segment && !entry.is_reserved()) {
				total_memory += entry.size_bytes + McbSizeBytes;
			}
		}
		for (const auto& entry : umb.mcb_chain_info) {
			if (entry.psp_segment == segment && !entry.is_reserved()) {
				total_umb += entry.size_bytes + McbSizeBytes;
			}
		}
		output.AddString(Indentation);
		output.AddString(row_format,
		                 SanitizeNameForDisplay(name).c_str(),
		                 (segment == this_psp) ? '*' : ' ',
		                 segment,
		                 ToBytesKbString(total_memory + total_umb).c_str(),
		                 ToBytesKbString(total_memory).c_str(),
		                 ToBytesKbString(total_umb).c_str());
	};

	output.AddString(MSG_Get("PROGRAM_MEM_CLASSIFY_TITLE"));
	output.AddString("\n\n");

	output.AddString(Indentation);
	output.AddString(MSG_Get("PROGRAM_MEM_CLASSIFY_TABLE_HEADER") + "\n");
	output.AddString(Indentation);
	output.AddString(MSG_Get("PROGRAM_MEM_CLASSIFY_TABLE_HORIZONTAL_LINE") + "\n");

	for (const auto& [segment, name] : memory.psp_info) {
		display_info(segment, name);
	}

	output.AddString(Indentation);
	output.AddString(MSG_Get("PROGRAM_MEM_CLASSIFY_TABLE_HORIZONTAL_LINE") + "\n");
	display_info(MCB_DOS, "DOS");
	display_info(MCB_FREE, MSG_Get("PROGRAM_MEM_CLASSIFY_FREE"));

	output.AddString("\n");
	output.AddString(Indentation + MSG_Get("PROGRAM_MEM_ASTERISK") + "\n");

	return {};
}

std::string MEM::DisplayDebug(MoreOutputStrings& output) const
{
	const auto memory = GetMemoryInfo();
	const auto& umb   = memory.umb;

	const auto this_psp = psp->GetSegment();

	const auto row_format = MSG_Get("PROGRAM_MEM_DEBUG_TABLE_ROW_FORMAT") + "\n";

	auto display_mcb_chain = [&output, &memory, &this_psp, &row_format](
	                                 const McbChainInfo& chain_info) {

		output.AddString(Indentation);
		output.AddString(MSG_Get("PROGRAM_MEM_DEBUG_TABLE_HEADER") + "\n");
		output.AddString(Indentation);
		output.AddString(MSG_Get("PROGRAM_MEM_DEBUG_TABLE_HORIZONTAL_LINE") + "\n");

		bool found_this_psp = false;
		for (const auto& entry : chain_info) {
			const auto mcb_info = GetMcbNameType(memory, entry);

			const bool is_this_psp = (entry.psp_segment == this_psp);
			found_this_psp = found_this_psp || is_this_psp;

			output.AddString(Indentation);
			output.AddString(format_str(
			        row_format,
			        entry.mcb_segment,
			        (entry.psp_segment == this_psp) ? '*' : ' ',
			        ToBytesKbString(entry.size_bytes + McbSizeBytes).c_str(),
			        SanitizeNameForDisplay(mcb_info.file_name).c_str(),
			        entry.psp_segment,
			        mcb_info.type.c_str()));
		}

		if (found_this_psp) {
			output.AddString("\n");
			output.AddString(Indentation);
			output.AddString(MSG_Get("PROGRAM_MEM_ASTERISK") + "\n");
		}
	};

	output.AddString(MSG_Get("PROGRAM_MEM_DEBUG_TITLE_CONVENTIONAL"));
	output.AddString("\n\n");

	// TODO: MS-DOS, DR-DOS, and FreeDOS implementations also show the
	// interrupt vector table area, BIOS memory area, and DOS memory area
	// before the 1st MCB. Consider showing these too.

	display_mcb_chain(memory.mcb_chain_info);

	if (umb.is_available) {
		output.AddString("\n\n");
		output.AddString(MSG_Get("PROGRAM_MEM_DEBUG_TITLE_UPPER"), 1u);
		output.AddString("\n\n");

		display_mcb_chain(umb.mcb_chain_info);
	}

	return {};
}

std::string MEM::DisplayFree(MoreOutputStrings& output) const
{
	const auto memory = GetMemoryInfo();
	const auto& umb   = memory.umb;

	const auto this_psp = psp->GetSegment();

	const auto row_format = MSG_Get("PROGRAM_MEM_FREE_TABLE_ROW_FORMAT") + "\n";

	auto display_free = [&output, &row_format, &this_psp](
	                            const McbChainInfo& chain_info) {
		size_t totalFree = 0;
		output.AddString(Indentation);
		output.AddString(MSG_Get("PROGRAM_MEM_FREE_TABLE_HEADER") + "\n");
		output.AddString(Indentation);
		output.AddString(
		        MSG_Get("PROGRAM_MEM_FREE_TABLE_HORIZONTAL_LINE") + "\n");

		bool found_this_psp = false;
		for (const auto& entry : chain_info) {
			if (!entry.is_free() && this_psp != entry.psp_segment) {
				continue;
			}

			const size_t size = entry.size_bytes + McbSizeBytes;
			const bool is_this_psp = (entry.psp_segment == this_psp);
			found_this_psp = found_this_psp || is_this_psp;

			output.AddString(Indentation);
			output.AddString(row_format,
			                 entry.mcb_segment,
			                 is_this_psp ? '*' : ' ',
			                 ToBytesKbString(size).c_str());
			totalFree += size;
		}
		output.AddString(Indentation);
		output.AddString(MSG_Get("PROGRAM_MEM_FREE_TABLE_UNDERLINE") + "\n");
		output.AddString(Indentation);
		output.AddString(MSG_Get("PROGRAM_MEM_FREE_TABLE_SUMMARY") + "\n",
		                 ToBytesKbString(totalFree).c_str());

		if (found_this_psp) {
			output.AddString("\n");
			output.AddString(Indentation);
			output.AddString(MSG_Get("PROGRAM_MEM_ASTERISK") + "\n");
		}
	};

	output.AddString(MSG_Get("PROGRAM_MEM_FREE_TITLE_CONVENTIONAL"));
	output.AddString("\n\n");
	display_free(memory.mcb_chain_info);
	if (umb.is_available) {
		output.AddString("\n\n");
		output.AddString(MSG_Get("PROGRAM_MEM_FREE_TITLE_UPPER"));
		output.AddString("\n\n");
		display_free(umb.mcb_chain_info);
	}

	return {};
}

std::string MEM::DisplayModule(MoreOutputStrings& output,
                               const std::string& module_name) const
{
	const auto memory = GetMemoryInfo();
	const auto& umb   = memory.umb;

	// Find PSPs matching the module
	std::vector<std::pair<uint16_t, std::string>> matching_psp_blocks = {};
	for (const auto& [psp_segment, file_name] : memory.psp_info) {
		if (iequals(file_name, module_name)) {
			matching_psp_blocks.emplace_back(psp_segment, file_name);
		}
	}

	if (matching_psp_blocks.empty()) {
		auto module_name_upcase = module_name;
		upcase(module_name_upcase);
		return format_str(MSG_Get("PROGRAM_MEM_ERROR_NO_MODULE"),
		                  module_name_upcase.c_str());
	}

	const auto row_format = MSG_Get("PROGRAM_MEM_MODULE_TABLE_ROW_FORMAT") + "\n";

	bool first = true;
	for (const auto& entry : matching_psp_blocks) {
		// Workaround for older compilers, not allowing local bindings
		// to be captured by lambda
		const auto& psp_segment = entry.first;
		const auto& file_name   = entry.second;

		if (!first) {
			output.AddString("\n");
		}
		first = false;

		output.AddString(MSG_Get("PROGRAM_MEM_MODULE_TITLE"),
		                 SanitizeNameForDisplay(file_name).c_str(),
		                 psp_segment);
		output.AddString("\n\n");

		size_t total_size = 0;
		auto display_size = [&output, &total_size, &row_format, &memory, &psp_segment]
	                                    (const McbChainInfo& chain_info) {

			for (const auto& entry : chain_info) {

				if (entry.psp_segment != psp_segment) {
					continue;
				}

				const auto mcb_info = GetMcbNameType(memory, entry);
				const auto current_size = entry.size_bytes +
				                          McbSizeBytes;

				output.AddString(Indentation);
				output.AddString(row_format,
				                 entry.mcb_segment,
				                 ToBytesKbString(current_size).c_str(),
				                 mcb_info.type.c_str());
				total_size += current_size;
			}
		};

		output.AddString(Indentation);
		output.AddString(MSG_Get("PROGRAM_MEM_MODULE_TABLE_HEADER") + "\n");
		output.AddString(Indentation);
		output.AddString(
		        MSG_Get("PROGRAM_MEM_MODULE_TABLE_HORIZONTAL_LINE") + "\n");

		display_size(memory.mcb_chain_info);
		display_size(umb.mcb_chain_info);

		output.AddString(Indentation);
		output.AddString(MSG_Get("PROGRAM_MEM_MODULE_TABLE_UNDERLINE") + "\n");
		output.AddString(Indentation);
		output.AddString(MSG_Get("PROGRAM_MEM_MODULE_TABLE_SUMMARY") + "\n",
		                 ToBytesKbString(total_size).c_str());
	}

	return {};
}

std::string MEM::DisplayXms(MoreOutputStrings& output) const
{
	const auto xms = GetXmsInfo();

	const auto& hma = xms.hma;

	if (!xms.is_available) {
		return MSG_Get("PROGRAM_MEM_ERROR_NO_XMS");
	}

	output.AddString(MSG_Get("PROGRAM_MEM_XMS_TITLE"));
	output.AddString("\n\n");

	ValueList values = {};

	const auto label_version = MSG_Get("PROGRAM_MEM_XMS_LABEL_VERSION");
	const auto label_driver  = MSG_Get("PROGRAM_MEM_XMS_LABEL_DRIVER");

	const auto value_version = format_str("%u.%02u",
	                                      xms.version_major,
	                                      xms.version_minor);
	const auto value_driver  = format_str("%u.%02u",
                                              xms.driver_revision_major,
                                              xms.driver_revision_minor);

	values.emplace_back(Indentation + label_version, value_version);
	values.emplace_back(Indentation + label_driver, value_driver);
	values.emplace_back();

	const auto label_hma = MSG_Get("PROGRAM_MEM_XMS_LABEL_HMA");
	const auto value_hma = hma.is_available
	                             ? ToBytesString(hma.free_bytes) + " " +
	                                       InBrackets(ToKbString(hma.free_bytes))
	                             : MSG_Get("PROGRAM_MEM_XMS_HMA_NOT_AVAILABLE");

	values.emplace_back(Indentation + label_hma, value_hma);
	values.emplace_back();

	if (xms.total_bytes.has_value()) {
		const auto label_total = MSG_Get("PROGRAM_MEM_XMS_LABEL_TOTAL");
		const auto value_total = ToBytesString(*(xms.total_bytes)) + " " +
		                         InBrackets(ToKbString(*(xms.total_bytes)));

		values.emplace_back(Indentation + label_total, value_total);
	}

	const auto label_free    = MSG_Get("PROGRAM_MEM_XMS_LABEL_FREE");
	const auto label_largest = MSG_Get("PROGRAM_MEM_XMS_LABEL_LARGEST");

	const auto value_free = ToBytesString(xms.free_bytes) + " " +
	                        InBrackets(ToKbString(xms.free_bytes));
	const auto value_largest = ToBytesString(xms.largest_free_block) + " " +
	                           InBrackets(ToKbString(xms.largest_free_block));

	values.emplace_back(Indentation + label_free, value_free);
	values.emplace_back(Indentation + label_largest, value_largest);

	DisplayValues(output, values);
	return {};
}

std::string MEM::DisplayEms(MoreOutputStrings& output) const
{
	const auto ems = GetEmsInfo();

	if (!ems.is_available) {
		return MSG_Get("PROGRAM_MEM_ERROR_NO_EMS");
	}

	output.AddString(MSG_Get("PROGRAM_MEM_EMS_TITLE"));
	output.AddString("\n\n");

	const auto info = GetEmsExtraInfo(ems);

	DisplayEmsHandleTable(output, info);
	DisplayEmsValues(output, ems, info);
	return {};
}

void MEM::DisplayEmsHandleTable(MoreOutputStrings& output,
                                const EmsExtraInfo& info) const
{
	auto get_name = [&info](const uint16_t handle) -> std::string {
		if (handle == 0) {
			return "SYSTEM";
		} else if (!info.handle_names.contains(handle)) {
			return {};
		}

		return SanitizeNameForDisplay(info.handle_names.at(handle));
	};

	if (!info.handle_pages.empty()) {

		output.AddString(Indentation);
		output.AddString(MSG_Get("PROGRAM_MEM_EMS_TABLE_HEADER") + "\n");
		output.AddString(Indentation);
		output.AddString(MSG_Get("PROGRAM_MEM_EMS_TABLE_HORIZONTAL_LINE") + "\n");

		const auto row_format = MSG_Get("PROGRAM_MEM_EMS_TABLE_ROW_FORMAT") +
		                        "\n";
		for (const auto& [handle, pages] : info.handle_pages) {
			output.AddString(Indentation);
			output.AddString(
			        format_str(row_format,
			                   handle,
			                   get_name(handle).c_str(),
			                   ToBytesString(pages).c_str(),
			                   ToKbString(pages * EmsPageSize).c_str()));
		}

		output.AddString("\n");
	}
}

void MEM::DisplayEmsValues(MoreOutputStrings& output,
                           const EmsInfo& ems,
                           const EmsExtraInfo& info) const
{
	ValueList values = {};

	const auto label_version = MSG_Get("PROGRAM_MEM_EMS_LABEL_VERSION");
	const auto value_version = format_str("%u.%02u",
	                                      ems.version_major,
	                                      ems.version_minor);

	values.emplace_back(Indentation + label_version, value_version);
	values.emplace_back();

	if (info.frame_segment.has_value()) {
		const auto label_segment = MSG_Get("PROGRAM_MEM_EMS_LABEL_SEGMENT");
		const auto value_segment = format_str("%04Xh", *info.frame_segment);

		values.emplace_back(Indentation + label_segment, value_segment);
		values.emplace_back();
	}

	if (info.total_handles.has_value() && info.open_handles.has_value()) {
		const auto label_total_handles = MSG_Get(
		        "PROGRAM_MEM_EMS_LABEL_HANDLES_TOTAL");
		const auto label_free_handles = MSG_Get(
		        "PROGRAM_MEM_EMS_LABEL_HANDLES_FREE");

		const auto free_handles = *info.total_handles - *info.open_handles;

		const auto value_total_handles = std::to_string(*info.total_handles);
		const auto value_free_handles = std::to_string(free_handles);

		values.emplace_back(Indentation + label_total_handles,
		                    value_total_handles);
		values.emplace_back(Indentation + label_free_handles,
		                    value_free_handles);
		values.emplace_back();
	}

	if (ems.total_bytes.has_value()) {
		const auto label_total = MSG_Get("PROGRAM_MEM_EMS_LABEL_TOTAL");
		const auto value_total = ToBytesString(*ems.total_bytes) + " " +
		                         InBrackets(ToKbString(*ems.total_bytes));
		values.emplace_back(Indentation + label_total, value_total);
	}

	const auto label_free = MSG_Get("PROGRAM_MEM_EMS_LABEL_FREE");
	const auto value_free = ToBytesString(ems.free_bytes) + " " +
	                        InBrackets(ToKbString(ems.free_bytes));
	values.emplace_back(Indentation + label_free, value_free);

	DisplayValues(output, values);
}

std::string MEM::SanitizeNameForDisplay(const std::string& name)
{
	auto result = name;

	for (auto& character : result) {
		if (character == 0) {
			character = ' ';
		} else if (is_control_ascii(character)) {
			character = '?';
		}
	}

	return result;
}

void MEM::DisplayValues(MoreOutputStrings& output, const ValueList& values) const
{
	constexpr size_t Spacing = 3;

	size_t maxLabelSize = 0;
	for (const auto& [label, value] : values) {
		maxLabelSize = std::max(maxLabelSize, label.size());
	}

	for (const auto& [label, value] : values) {
		size_t separatorSize = Spacing + maxLabelSize - label.size();

		output.AddString(label);
		output.AddString(std::string(separatorSize, ' '));
		output.AddString(value);
		output.AddString("\n");
	}
}

std::string MEM::ToBytesString(const size_t value)
{
	return format_number(value);
}

std::string MEM::ToKbString(const size_t value)
{
	return format_number((value + BytesInKb / 2) / BytesInKb) + "K";
}

std::string MEM::ToBytesKbString(const size_t value)
{
	return format_str("%7s  %6s",
	                  ToBytesString(value).c_str(),
	                  InBrackets(ToKbString(value)).c_str());
}

std::string MEM::InBrackets(const std::string& input)
{
	return std::string("(") + input + std::string(")");
}

MEM::McbNameType MEM::GetMcbNameType(const MemoryInfo& info,
                                     const McbChainInfoEntry& entry)
{
	if (entry.is_free()) {
		return {{}, MSG_Get("PROGRAM_MEM_MCB_TYPE_FREE")};
	} else if (entry.is_dos()) {
		return {{}, MSG_Get("PROGRAM_MEM_MCB_TYPE_SYSTEM")};
	} else if (entry.is_reserved()) {
		return {{}, MSG_Get("PROGRAM_MEM_MCB_TYPE_RESERVED")};
	}

	std::string name = {};
	std::string type = {};

	bool name_via_env = false;
	bool name_via_psp = false;

	name = entry.file_name;
	if (info.env_info.contains(entry.mcb_segment)) {
		name         = info.env_info.at(entry.mcb_segment);
		name_via_env = true;
	} else if (name.empty() && info.psp_info.contains(entry.psp_segment)) {
		name         = info.psp_info.at(entry.psp_segment);
		name_via_psp = true;
	}

	if (name_via_env) {
		type = MSG_Get("PROGRAM_MEM_MCB_TYPE_ENVIRONMENT");
	} else if (name_via_psp) {
		type = MSG_Get("PROGRAM_MEM_MCB_TYPE_DATA");
	} else if (!entry.file_name.empty()) {
		type = MSG_Get("PROGRAM_MEM_MCB_TYPE_PROGRAM");
	}

	return {name, type};
}

MEM::MemoryInfo MEM::GetMemoryInfo() const
{
	MemoryInfo memory = {};

	ReadBasicMemoryInfo(memory);
	ReadPspStructuresInfo(memory);

	return memory;
}

void MEM::ReadBasicMemoryInfo(MemoryInfo& memory) const
{
	auto& umb = memory.umb;

	memory.total_bytes = mem_readw(BIOS_MEMORY_SIZE) * 1024;

	const auto this_psp = psp->GetSegment();

	auto calculate_free_size = [&this_psp](const McbChainInfo& mcb_chain_info,
	                                       size_t& free_size,
	                                       size_t& largest_free_block) {
		size_t cumulated_free = 0;
		for (const auto& entry : mcb_chain_info) {

			// Entries belonging to this MEM.EXE command shall be
			// considered s free memory
			const bool is_free = entry.is_free() ||
			                     (this_psp == entry.psp_segment);

			if (is_free) {
				free_size += entry.size_bytes;
				if (cumulated_free > 0) {
					free_size += McbSizeBytes;
					cumulated_free += McbSizeBytes;
				}
				cumulated_free += entry.size_bytes;
				largest_free_block = std::max(cumulated_free,
				                              largest_free_block);
			} else {
				cumulated_free = 0;
			}
		}
	};

	auto calculate_umb_size = [](const McbChainInfo& mcb_chain_info,
	                             size_t& total_size) {
		for (const auto& entry : mcb_chain_info) {
			if (!entry.is_reserved()) {
				total_size += entry.size_bytes + McbSizeBytes;
			}
		}
	};

	auto detect_reserved_mcb = [](McbChainInfoEntry& first) {
		if (!first.is_dos()) {
			return;
		}
		const auto segment_after_start = first.mcb_segment + 1;
		if ((segment_after_start * RealSegmentSize) % BytesInKb == 0) {
			first.reserved = true;
		}
	};

	// It would be better to detect the MSC chain start segments using the
	// official APIs, but since our DOS only supports on UMB chain this
	// could be tricky to test.

	memory.mcb_chain_info = GetMcbChainInfo(dos.firstMCB);
	calculate_free_size(memory.mcb_chain_info,
	                    memory.free_bytes,
	                    memory.largest_free_block);

	const auto first_umb_mcb = dos_infoblock.GetStartOfUMBChain();
	umb.is_available         = (first_umb_mcb != 0xffff);
	if (umb.is_available) {
		umb.mcb_chain_info = GetMcbChainInfo(first_umb_mcb);
		calculate_free_size(umb.mcb_chain_info,
		                    umb.free_bytes,
		                    umb.largest_free_block);

		// Detect reserved areas, marked with dummy DOS segments
		if (!umb.mcb_chain_info.empty()) {
			detect_reserved_mcb(umb.mcb_chain_info[0]);
		}

		calculate_umb_size(umb.mcb_chain_info, umb.total_bytes);
	}
}

void MEM::ReadPspStructuresInfo(MemoryInfo& memory) const
{
	const auto& umb = memory.umb;

	auto get_psp_info = [&memory](const McbChainInfo& chain_info) {
		for (const auto& entry : chain_info) {
			if (entry.is_free() || entry.is_dos() ||
			    entry.is_reserved() || entry.file_name.empty()) {
				continue;
			}

			// At least under Windows 3.x it is normal that entries
			// are overwritten while processing the MCB chain
			memory.psp_info[entry.psp_segment] = entry.file_name;
		}
	};

	auto get_env_info = [&memory](const McbChainInfo& chain_info) {
		for (const auto& entry : chain_info) {
			if (entry.is_free() || entry.is_dos() ||
			    entry.is_reserved() || entry.file_name.empty()) {
				continue;
			}

			DOS_PSP psp(entry.psp_segment);
			const auto environment = psp.GetEnvironment();
			if (environment == 0) {
				continue;
			}

			// At least under Windows 3.x it is normal that entries
			// are overwritten while processing the MCB chain
			memory.env_info[environment - 1] = entry.file_name;
		}
	};
	get_psp_info(memory.mcb_chain_info);
	get_psp_info(umb.mcb_chain_info);

	get_env_info(memory.mcb_chain_info);
	get_env_info(umb.mcb_chain_info);
}

MEM::McbChainInfo MEM::GetMcbChainInfo(const uint16_t start_segment)
{
	McbChainInfo chain_info = {};

	uint16_t mcb_segment = start_segment;

	while (true) {
		DOS_MCB mcb(mcb_segment);
		if (mcb.GetType() != 'M' && mcb.GetType() != 'Z') {
			LOG_WARNING("DOS: MEM - invalid type in MCB segment %04Xh, chain broken",
			            mcb_segment);
			break;
		}

		chain_info.emplace_back();
		chain_info.back().mcb_segment = mcb_segment;

		chain_info.back().type        = mcb.GetType();
		chain_info.back().size_bytes  = mcb.GetSize() * RealSegmentSize;
		chain_info.back().psp_segment = mcb.GetPSPSeg();

		char buffer[9];
		mcb.GetFileName(&buffer[0]);

		chain_info.back().file_name = std::string(buffer);
		if (mcb.GetType() == 'Z') {
			// End of the chain
			break;
		}

		mcb_segment += static_cast<uint16_t>(mcb.GetSize() + 1);
	}

	return chain_info;
}

MEM::XmsInfo MEM::GetXmsInfo() const
{
	XmsInfo xms = {};
	auto& hma   = xms.hma;

	// Determine if we have Extended (XMS) memory
	reg_ax = 0x4300;
	CALLBACK_RunRealInt(0x2f);
	xms.is_available = (reg_al == 0x80);
	if (!xms.is_available) {
		return {};
	}

	// Get Extended (XMS) memory API address
	reg_ax = 0x4310;
	CALLBACK_RunRealInt(0x2f);
	xms.api_segment = SegValue(es);
	xms.api_offset  = reg_bx;
	if (xms.api_segment == 0) {
		LOG_WARNING("DOS: MEM - XMS API segment is NULL");
		return {};
	}

	// Get the Extended (XMS) memory driver version
	reg_ah = 0x00;
	CALLBACK_RunRealFar(xms.api_segment, xms.api_offset);
	xms.version_major         = bcd_to_decimal(reg_ah);
	xms.version_minor         = bcd_to_decimal(reg_al);
	xms.driver_revision_major = bcd_to_decimal(reg_bh);
	xms.driver_revision_minor = bcd_to_decimal(reg_bl);

	if (xms.version_major < 2) {
		LOG_WARNING("DOS: MEM - XMS version 1.x not supported");
		return {};
	}

	// Get the High (HMA) memory information
	hma.is_available = (reg_dx == 0x0001);
	if (hma.is_available) {
		constexpr uint16_t FreeHmaUnsupported = 0xffff;

		reg_ax = 0x4a01;
		reg_bx = FreeHmaUnsupported;
		CALLBACK_RunRealInt(0x2f);
		if (reg_bx == FreeHmaUnsupported) {
			hma.is_available = false;
		} else {
			hma.free_bytes = reg_bx;
		}
	}

	// Get the free Extended (XMS) memory info
	if (xms.version_major >= 3 && is_cpu_386_or_better()) {
		reg_ah = 0x88;
		CALLBACK_RunRealFar(xms.api_segment, xms.api_offset);
		if (reg_bl == 0) {
			xms.free_bytes         = reg_edx * BytesInKb;
			xms.largest_free_block = reg_eax * BytesInKb;
		}
	} else {
		reg_ah = 0x08;
		CALLBACK_RunRealFar(xms.api_segment, xms.api_offset);
		if (reg_bl == 0) {
			xms.free_bytes         = reg_dx * BytesInKb;
			xms.largest_free_block = reg_ax * BytesInKb;
		}
	}

	// Get the total Extended (XMS) memory info
	for (const auto& entry : GetBiosMemoryMap()) {
		if (entry.type != 1) {
			// Not normally available to the guest OS
			continue;
		}

		constexpr uint64_t XmsBase = BytesInKb * BytesInKb;
		if (entry.base + entry.length < XmsBase) {
			// The entry does not cover the XMS memory
			continue;
		}

		if (!xms.total_bytes.has_value()) {
			xms.total_bytes = 0;
		}

		xms.total_bytes = *xms.total_bytes + entry.length;
		if (entry.base < XmsBase) {
			xms.total_bytes = *xms.total_bytes + (entry.base - XmsBase);
		}
	}

	// Get the total Extended (XMS) memory info - fallback method
	if (!xms.total_bytes.has_value()) {
		reg_ax = 0xe801;
		CALLBACK_RunRealInt(0x15);
		if (!(cpu_regs.flags & FLAG_CF)) {
			xms.total_bytes = (reg_ax + reg_bx) * BytesInKb;
		}
	}

	// Get the total Extended (XMS) memory info - fallback method using CMOS
	if (!xms.total_bytes.has_value()) {
		// Check that the machine is not XT
		if (real_readb(0xf000, 0xfffe) == 0xfc) {

			IO_WriteB(0x70, 0x18);
			const auto high_part = IO_ReadB(0x71) << 18;
			IO_WriteB(0x70, 0x17);
			const auto low_part = IO_ReadB(0x71) << 10;

			xms.total_bytes = low_part + high_part;
		}
	}

	// Reduce reported XMS size by HMA
	if (xms.total_bytes.has_value() && xms.hma.is_available) {
		if (*xms.total_bytes <= HmaSizeBytes) {
			xms.total_bytes = 0;
		} else {
			xms.total_bytes = *xms.total_bytes - HmaSizeBytes;
		}
	}

	// Sanity check - if not passed, the total memory size is wrong
	if (xms.total_bytes.has_value()) {
		if (*xms.total_bytes < xms.free_bytes) {
			LOG_WARNING("DOS: MEM - invalid total/free XMS memory size");
			xms.total_bytes = {};
		}
	}

	return xms;
}

MEM::EmsInfo MEM::GetEmsInfo() const
{
	// Check if expanded memory driver is available
	uint16_t handle = 0;
	if (!DOS_OpenFile(EmsDeviceName.c_str(), 0, &handle)) {
		// No Expanded (EMS) memory driver available
		return {};
	}
	DOS_CloseFile(handle);

	// Check that memory driver is working properly
	reg_ah = 0x40;
	CALLBACK_RunRealInt(0x67);
	if (reg_ah != 0x00) {
		return {};
	}

	EmsInfo ems = {};

	// Get the Expanded (EMS) memory driver version
	reg_ah = 0x46;
	CALLBACK_RunRealInt(0x67);
	if (reg_ah != 0x00) {
		return {};
	}

	ems.version_major = high_nibble(reg_al);
	ems.version_minor = low_nibble(reg_al);

	// Get the free/total Expanded (EMS) memory info
	reg_ah = 0x42;
	CALLBACK_RunRealInt(0x67);
	if (reg_ah != 0x00) {
		return {};
	}

	ems.free_bytes  = reg_bx * EmsPageSize;
	ems.total_bytes = reg_dx * EmsPageSize;

	// Sanity check - if not passed, the total memory size is wrong
	if (*ems.total_bytes < ems.free_bytes) {
		LOG_WARNING("DOS: MEM - invalid total/free EMS memory size");
		ems.total_bytes = {};
	}

	// We can now declare EMS is operable
	ems.is_available = true;

	return ems;
}

MEM::EmsExtraInfo MEM::GetEmsExtraInfo(const EmsInfo& ems) const
{
	if (!ems.is_available) {
		return {};
	}

	EmsExtraInfo info = {};

	// Get the frame segment
	reg_ah = 0x41;
	CALLBACK_RunRealInt(0x67);
	if (reg_ah == 0x00) {
		info.frame_segment = reg_bx;
	}

	// Get the open handles count
	reg_ah = 0x4b;
	CALLBACK_RunRealInt(0x67);
	if (reg_ah == 0x00) {
		info.open_handles = reg_bx;
	}

	// Get the total handles count
	reg_ax = 0x5402;
	CALLBACK_RunRealInt(0x67);
	if (reg_ah == 0x00) {
		info.total_handles = reg_bx;
	}

	// Sanity check - if not passed, the total handles number is wrong
	if (info.open_handles.has_value() && info.total_handles.has_value() &&
	    (*info.total_handles < *info.open_handles)) {
		LOG_WARNING("DOS: MEM - invalid total/open EMS handles");
		info.total_handles = {};
	}

	// Get the allocated pages for each handle
	for (uint16_t handle = 0; handle <= UINT8_MAX; ++handle) {
		reg_ah = 0x4c;
		reg_dx = handle;
		CALLBACK_RunRealInt(0x67);
		if (reg_ah == 0x00) {
			info.handle_pages[handle] = reg_bx;
		}
		if (info.total_handles.has_value() &&
		    info.handle_pages.size() >= *info.total_handles) {
			break;
		}
	}

	// Get the name for each open handle
	uint16_t segment = 0;
	uint16_t blocks  = 1;
	if (!DOS_AllocateMemory(&segment, &blocks)) {
		LOG_WARNING("DOS: MEM - unable to allocate DOS memory");
		return info;
	}

	for (const auto& [handle, pages] : info.handle_pages) {

		reg_ax = 0x5300;
		reg_dx = handle;
		CPU_SetSegGeneral(es, segment);
		reg_di = 0;
		CALLBACK_RunRealInt(0x67);
		if (reg_ah != 0x00) {
			continue;
		}

		for (uint8_t idx = 0; idx < 8; ++idx) {
			const auto byte = real_readb(segment, idx);
			info.handle_names[handle].push_back(byte);
		}
	}

	DOS_FreeMemory(segment);

	return info;
}

MEM::BiosMemoryMap MEM::GetBiosMemoryMap()
{
	BiosMemoryMap memory_map = {};

	if (!is_cpu_386_or_better()) {
		return memory_map;
	}

	uint16_t segment = 0;
	uint16_t blocks  = 2;
	if (!DOS_AllocateMemory(&segment, &blocks)) {
		LOG_WARNING("DOS: MEM - unable to allocate DOS memory");
		return memory_map;
	}

	constexpr uint32_t SmapMagicValue = 0x534d4150;
	constexpr uint64_t MaxEntries     = 100;

	for (uint32_t idx = 0; idx <= MaxEntries; ++idx) {
		if (idx == MaxEntries) {
			LOG_WARNING("DOS: MEM - too many entries in the BIOS memory map");
			memory_map.clear();
			break;
		}

		reg_eax = 0xe820;
		reg_ebx = idx;
		reg_ecx = 20;
		reg_edx = SmapMagicValue;
		CPU_SetSegGeneral(es, segment);
		reg_di = 0;
		CALLBACK_RunRealInt(0x15);
		if ((cpu_regs.flags & FLAG_CF) || reg_eax != SmapMagicValue) {
			break;
		}

		memory_map.emplace_back();
		memory_map.back().base   = real_readq(segment, 0);
		memory_map.back().length = real_readq(segment, 8);
		memory_map.back().type   = real_readd(segment, 16);
	}

	DOS_FreeMemory(segment);
	return memory_map;
}

void MEM::AddMessages()
{
	MSG_Add("PROGRAM_MEM_HELP_LONG",
	        "Display the amount of used and free memory.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]mem[reset] [/p] [/c | /d | /f | /x | /e]\n"
	        "  [color=light-green]mem[reset] [/p] /m [color=light-cyan]MODULE[reset]\n"
	        "  [color=light-green]mem[reset] [/p] /m:[color=light-cyan]MODULE[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  /p or /page      display one page a time\n"
	        "  /c or /classify  display memory usage per module\n"
	        "  /d or /debug     display detailed memory usage information according to MCB\n"
	        "                   (Memory Control Block) and PSP (Program Segment Prefix)\n"
	        "                   structures\n"
	        "  /f or /free      display free memory segments\n"
	        "  /m or /module    display memory usage of the specified [color=light-cyan]MODULE[reset]\n"
	        "  /x or /xms       display Extended Memory (XMS) usage\n"
	        "  /e or /ems       display Expanded Memory (EMS) usage\n"
	        "\n"
	        "Notes:\n"
	        "  - If no report is selected, a brief summary is displayed.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]mem[reset]\n");

	// Summary report

	MSG_Add("PROGRAM_MEM_SUMMARY_TABLE_HEADER",
	        "[color=white]Memory Type           Total        Used         Free[reset]");
	MSG_Add("PROGRAM_MEM_SUMMARY_TABLE_HORIZONTAL_LINE",
	        "----------------   ----------   ----------   ----------");
	MSG_Add("PROGRAM_MEM_SUMMARY_TABLE_ROW_FORMAT",
	        "[color=light-cyan]%-16s[reset]   %10s   %10s   %10s");

	MSG_Add("PROGRAM_MEM_TYPE_CONVENTIONAL", "Conventional");
	MSG_Add("PROGRAM_MEM_TYPE_UMB", "Upper (UMB)");
	MSG_Add("PROGRAM_MEM_TYPE_HMA", "High (HMA)");
	MSG_Add("PROGRAM_MEM_TYPE_XMS", "Extended (XMS)");
	MSG_Add("PROGRAM_MEM_TYPE_EMS", "Expanded (EMS)");
	MSG_Add("PROGRAM_MEM_TYPE_UNDER_1MB", "Total under 1 MB");

	MSG_Add("PROGRAM_MEM_LABEL_LARGEST",
		"Largest free Conventional Memory block");
	MSG_Add("PROGRAM_MEM_LABEL_LARGEST_UMB",
	        "Largest free Upper Memory (UMB) block");

	MSG_Add("PROGRAM_MEM_BYTES", "bytes");

	// Classification report

	MSG_Add("PROGRAM_MEM_CLASSIFY_TITLE", "Modules using memory below 1 MB:");
	MSG_Add("PROGRAM_MEM_CLASSIFY_TABLE_HEADER",
	        "[color=white]Name        PSP         Total      =   Conventional  +   Upper (UMB)[reset]");
	MSG_Add("PROGRAM_MEM_CLASSIFY_TABLE_HORIZONTAL_LINE",
	        "--------   -----   ---------------   ---------------   ---------------");
	MSG_Add("PROGRAM_MEM_CLASSIFY_TABLE_ROW_FORMAT",
	        "%-8s%c  %04Xh   %15s   %s   %15s");

	MSG_Add("PROGRAM_MEM_CLASSIFY_FREE", "free");

	// Debug report

	MSG_Add("PROGRAM_MEM_DEBUG_TITLE_CONVENTIONAL",
	        "Conventional Memory MCB chain:");
	MSG_Add("PROGRAM_MEM_DEBUG_TITLE_UPPER",
		"Upper Memory MCB chain #%u:");

	MSG_Add("PROGRAM_MEM_DEBUG_TABLE_HEADER",
	        "[color=white]Segment        Size        Name       PSP   Type[reset]");
	MSG_Add("PROGRAM_MEM_DEBUG_TABLE_HORIZONTAL_LINE",
	        "-------   ---------------  --------  -----  -------------");
	MSG_Add("PROGRAM_MEM_DEBUG_TABLE_ROW_FORMAT",
	        " %04Xh %c  %15s  %-8s  %04Xh  %s");

	// Free memory report

	MSG_Add("PROGRAM_MEM_FREE_TITLE_CONVENTIONAL",
	        "Free segments in Conventional Memory:");
	MSG_Add("PROGRAM_MEM_FREE_TITLE_UPPER",
	        "Free segments in Upper Memory (UMB):");

	MSG_Add("PROGRAM_MEM_FREE_TABLE_HEADER",
	        "[color=white]Segment         Size[reset]");
	MSG_Add("PROGRAM_MEM_FREE_TABLE_HORIZONTAL_LINE",
		"-------   ---------------");
	MSG_Add("PROGRAM_MEM_FREE_TABLE_ROW_FORMAT",
		" %04Xh %c  %15s");
	MSG_Add("PROGRAM_MEM_FREE_TABLE_UNDERLINE",
		"          ---------------");
	MSG_Add("PROGRAM_MEM_FREE_TABLE_SUMMARY",
		"Total:    %s");

	// Module memory usage report

	MSG_Add("PROGRAM_MEM_MODULE_TITLE",
	        "Module '%s' (PSP segment %04Xh) uses the following memory:");

	MSG_Add("PROGRAM_MEM_MODULE_TABLE_HEADER",
	        "[color=white]Segment         Size        Type[reset]");
	MSG_Add("PROGRAM_MEM_MODULE_TABLE_HORIZONTAL_LINE",
	        "-------   ---------------   -------------");
	MSG_Add("PROGRAM_MEM_MODULE_TABLE_ROW_FORMAT",
		" %04Xh    %s   %s");
	MSG_Add("PROGRAM_MEM_MODULE_TABLE_UNDERLINE",
		"          ---------------");
	MSG_Add("PROGRAM_MEM_MODULE_TABLE_SUMMARY",
		"Total:    %s");

	// XMS memory report

	MSG_Add("PROGRAM_MEM_MCB_TYPE_FREE",        "(free)");
	MSG_Add("PROGRAM_MEM_MCB_TYPE_SYSTEM",      "System data");
	MSG_Add("PROGRAM_MEM_MCB_TYPE_RESERVED",    "Reserved area");
	MSG_Add("PROGRAM_MEM_MCB_TYPE_PROGRAM",     "Program");
	MSG_Add("PROGRAM_MEM_MCB_TYPE_ENVIRONMENT", "Environment");
	MSG_Add("PROGRAM_MEM_MCB_TYPE_DATA",        "Data");

	MSG_Add("PROGRAM_MEM_XMS_TITLE",
	        "Detailed Extended Memory (XMS) information:");

	MSG_Add("PROGRAM_MEM_XMS_LABEL_VERSION", "XMS version");
	MSG_Add("PROGRAM_MEM_XMS_LABEL_DRIVER",  "Driver revision");
	MSG_Add("PROGRAM_MEM_XMS_LABEL_HMA",     "High Memory (HMA)");
	MSG_Add("PROGRAM_MEM_XMS_LABEL_TOTAL",   "Total XMS memory");
	MSG_Add("PROGRAM_MEM_XMS_LABEL_FREE",    "Free XMS memory");
	MSG_Add("PROGRAM_MEM_XMS_LABEL_LARGEST", "Largest free XMS block");

	MSG_Add("PROGRAM_MEM_XMS_HMA_FREE", "free");
	MSG_Add("PROGRAM_MEM_XMS_HMA_NOT_AVAILABLE", "not available");

	// EMS memory report

	MSG_Add("PROGRAM_MEM_EMS_TITLE",
	        "Detailed Expanded Memory (EMS) information:");

	MSG_Add("PROGRAM_MEM_EMS_TABLE_HEADER",
	        "[color=white]Handle   Name         Pages         Size[reset]");
	MSG_Add("PROGRAM_MEM_EMS_TABLE_HORIZONTAL_LINE",
	        "------   --------   -------   ----------");
	MSG_Add("PROGRAM_MEM_EMS_TABLE_ROW_FORMAT",
		"   %3d   %-8s   %7s   %10s");

	MSG_Add("PROGRAM_MEM_EMS_LABEL_VERSION",       "EMS version");
	MSG_Add("PROGRAM_MEM_EMS_LABEL_SEGMENT",       "Frame segment");
	MSG_Add("PROGRAM_MEM_EMS_LABEL_HANDLES_TOTAL", "Total handles");
	MSG_Add("PROGRAM_MEM_EMS_LABEL_HANDLES_FREE",  "Free handles");
	MSG_Add("PROGRAM_MEM_EMS_LABEL_TOTAL",         "Total EMS memory");
	MSG_Add("PROGRAM_MEM_EMS_LABEL_FREE",          "Free EMS memory");

	// Common messages

	MSG_Add("PROGRAM_MEM_ASTERISK", "* - the currently running MEM command");

	// Error messages

	MSG_Add("PROGRAM_MEM_ERROR_NO_MODULE", "No module '%s' in memory.\n");
	MSG_Add("PROGRAM_MEM_ERROR_NO_XMS", "No Extended Memory (XMS) found.\n");
	MSG_Add("PROGRAM_MEM_ERROR_NO_EMS", "No Expanded Memory (EMS) found.\n");
}
