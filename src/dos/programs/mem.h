// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_MEM_H
#define DOSBOX_PROGRAM_MEM_H

#include "dos/programs.h"

#include "dos/programs/more_output.h"

#include <optional>
#include <vector>

class MEM final : public Program {
public:
	MEM()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::Misc,
		               HELP_CmdType::Program,
		               "MEM"};
	}

	void Run() override;

private:
	static void AddMessages();

	static const std::string Indentation;

	static constexpr size_t BytesInKb    = 1024;
	static constexpr size_t HmaSizeBytes = 64 * BytesInKb;

	static constexpr size_t McbSizeBytes = RealSegmentSize;

	struct McbChainInfoEntry {
		uint16_t mcb_segment = 0;

		uint8_t type         = 0;
		size_t size_bytes    = 0;
		uint16_t psp_segment = 0;

		std::string file_name = {};

		bool reserved = false;

		bool is_free() const
		{
			return psp_segment == MCB_FREE;
		}
		bool is_dos() const
		{
			return !reserved && (psp_segment == MCB_DOS);
		}
		bool is_reserved() const
		{
			return reserved;
		}
	};
	using McbChainInfo = std::vector<McbChainInfoEntry>;

	// Order is important for display purposes, so no 'unordered_map'
	using PspInfo = std::map<uint16_t, std::string>;
	using EnvInfo = std::map<uint16_t, std::string>;

	struct MemoryInfo {

		size_t total_bytes = 0;
		size_t free_bytes  = 0;

		size_t largest_free_block = 0;

		McbChainInfo mcb_chain_info = {};

		struct {
			bool is_available = false;

			size_t total_bytes = 0;
			size_t free_bytes  = 0;

			size_t largest_free_block = 0;

			McbChainInfo mcb_chain_info = {};

		} umb = {};

		PspInfo psp_info = {};
		EnvInfo env_info = {};
	};

	struct BiosMemoryMapEntry {
		uint64_t base   = 0;
		uint64_t length = 0;
		uint32_t type   = 0;
	};
	using BiosMemoryMap = std::vector<BiosMemoryMapEntry>;

	struct XmsInfo {
		bool is_available = false;

		uint16_t api_segment = 0;
		uint16_t api_offset  = 0;

		uint8_t version_major = 0;
		uint8_t version_minor = 0;

		uint8_t driver_revision_major = 0;
		uint8_t driver_revision_minor = 0;

		// Shouldn't happen inside DOSBox Staging, but there is still a
		// theoretical possibility the size calculation will fail
		std::optional<size_t> total_bytes = {};

		size_t free_bytes = 0;

		size_t largest_free_block = 0;

		struct {
			bool is_available = false;

			size_t free_bytes = 0;
		} hma = {};
	};

	struct EmsInfo {

		bool is_available = false;

		uint8_t version_major = 0;
		uint8_t version_minor = 0;

		std::optional<size_t> total_bytes = {};
		size_t free_bytes                 = 0;
	};

	struct EmsExtraInfo {

		std::optional<uint16_t> frame_segment = {};
		std::optional<uint16_t> open_handles  = {};
		std::optional<uint16_t> total_handles = {};

		// Order is important for display purposes, so no 'unordered_map'
		std::map<uint16_t, uint16_t> handle_pages    = {};
		std::map<uint16_t, std::string> handle_names = {};
	};

	// Report display functions
	std::string DisplaySummary(MoreOutputStrings& output) const;
	std::string DisplayClassify(MoreOutputStrings& output) const;
	std::string DisplayDebug(MoreOutputStrings& output) const;
	std::string DisplayFree(MoreOutputStrings& output) const;
	std::string DisplayModule(MoreOutputStrings& output,
	                          const std::string& module_name) const;
	std::string DisplayXms(MoreOutputStrings& output) const;
	std::string DisplayEms(MoreOutputStrings& output) const;

	void DisplayEmsHandleTable(MoreOutputStrings& output,
	                           const EmsExtraInfo& info) const;
	void DisplayEmsValues(MoreOutputStrings& output,
	                      const EmsInfo& ems,
	                      const EmsExtraInfo& info) const;

	// Helper display functions
	using ValueList = std::vector<std::pair<std::string, std::string>>;
	void DisplayValues(MoreOutputStrings& output, const ValueList& values) const;
	static std::string SanitizeNameForDisplay(const std::string& name);

	// Helper conversion functions
	static std::string ToBytesString(const size_t value);
	static std::string ToKbString(const size_t value);
	static std::string ToBytesKbString(const size_t value);
	static std::string InBrackets(const std::string& input);

	struct McbNameType {
		std::string file_name = {};
		std::string type      = {};
	};
	static McbNameType GetMcbNameType(const MemoryInfo& info,
	                                  const McbChainInfoEntry& entry);

	// Info retrieval - Conventional, UMB
	MemoryInfo GetMemoryInfo() const;
	// Helper functions
	void ReadBasicMemoryInfo(MemoryInfo& memory) const;
	void ReadPspStructuresInfo(MemoryInfo& memory) const;
	static McbChainInfo GetMcbChainInfo(const uint16_t start_segment);

	// Info retrieval - Extended and HMA
	XmsInfo GetXmsInfo() const;

	// Info retrieval - Expanded
	EmsInfo GetEmsInfo() const;
	EmsExtraInfo GetEmsExtraInfo(const EmsInfo& ems) const;

	// Info retrieval - memory map according to BIOS
	static BiosMemoryMap GetBiosMemoryMap();
};

#endif // DOSBOX_PROGRAM_MEM_H
