// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include "config/config.h"
#include "dos.h"
#include "dos/dos_memory.h"
#include "hardware/memory.h"
#include "misc/support.h"

#include <string_view>

enum class McbFaultStrategy { Deny, Repair, Report, Allow };

// Constants
// ~~~~~~~~~
constexpr uint8_t middle_mcb_type = 0x4d; // 'M', middle or member of a MCB chain
constexpr uint8_t ending_mcb_type = 0x5a; // 'Z', last entry of the MCB chain

// Upper member block starting segment
constexpr uint16_t umb_start_seg = 0x9fff;

static uint16_t allocation_strategy = 0x00;

static auto mcb_fault_strategy = McbFaultStrategy::Repair;

// not based on anything in particular. Player Manager 2 requires ~17 corrections.
constexpr uint16_t max_allowed_faults = 100;

void DOS_SetMcbFaultStrategy(const char * mcb_fault_strategy_pref)
{
	assert(mcb_fault_strategy_pref);
	const std::string_view pref = mcb_fault_strategy_pref;

	if (pref == "deny")
		mcb_fault_strategy = McbFaultStrategy::Deny;
	else if (pref == "repair")
		mcb_fault_strategy = McbFaultStrategy::Repair;
	else if (pref == "report")
		mcb_fault_strategy = McbFaultStrategy::Report;
	else if (pref == "allow")
		mcb_fault_strategy = McbFaultStrategy::Allow;
	else
		// the conf system programmatically guarantees only the above prefs are used
		assertm(false, "Unhandled MCB fault strategy");
}

// returns true if the MCB block needed triaging
static bool triage_block(DOS_MCB &mcb, const uint8_t repair_type)
{
	auto mcb_type_is_valid = [&]() {
		const auto t = mcb.GetType();
		return t == middle_mcb_type || t == ending_mcb_type;
	};
	if (mcb_type_is_valid())
		return false;

	switch (mcb_fault_strategy) {
	case McbFaultStrategy::Deny:
		E_Exit("DOS_MEMORY: Exiting due corrupt MCB chain");
		break;

	case McbFaultStrategy::Repair:
		LOG_INFO("DOS_MEMORY: Repairing MCB block in segment %04xh from type '%02x' to '%02x'",
		         mcb.GetPSPSeg(),
		         mcb.GetType(),
		         repair_type);
		mcb.SetType(repair_type);
		assert(mcb_type_is_valid());
		break;

	case McbFaultStrategy::Report:
		LOG_WARNING("DOS_MEMORY: Reporting MCB block in segment %04xh with corrupt type '%02x'",
		            mcb.GetPSPSeg(),
		            mcb.GetType());
		break;

	case McbFaultStrategy::Allow:
		break;
	}
	return true;
}

static void DOS_CompressMemory()
{
	uint16_t mcb_segment = dos.firstMCB;
	DOS_MCB mcb(mcb_segment);
	DOS_MCB mcb_next(0);

	uint16_t faults = 0;

	while (mcb.GetType() != ending_mcb_type && faults < max_allowed_faults) {
		mcb_next.SetPt((uint16_t)(mcb_segment + mcb.GetSize() + 1));
		if ((mcb.GetPSPSeg() == MCB_FREE) &&
		    (mcb_next.GetPSPSeg() == MCB_FREE)) {
			faults += triage_block(mcb_next, mcb.GetType());
			mcb.SetSize(mcb.GetSize() + mcb_next.GetSize() + 1);
			mcb.SetType(mcb_next.GetType());
		} else {
			mcb_segment+=mcb.GetSize()+1;
			mcb.SetPt(mcb_segment);
		}
	}
}

void DOS_FreeProcessMemory(uint16_t pspseg) {
	uint16_t mcb_segment=dos.firstMCB;
	DOS_MCB mcb(mcb_segment);

	uint16_t faults = 0;
	while (faults < max_allowed_faults) {
		if (mcb.GetPSPSeg() == pspseg) {
			mcb.SetPSPSeg(MCB_FREE);
		}
		if (mcb.GetType() == ending_mcb_type)
			break;
		faults += triage_block(mcb, middle_mcb_type);
		mcb_segment += mcb.GetSize() + 1;
		mcb.SetPt(mcb_segment);
	}

	uint16_t umb_start=dos_infoblock.GetStartOfUMBChain();
	if (umb_start == umb_start_seg) {
		DOS_MCB umb_mcb(umb_start);

		faults = 0;
		while (faults < max_allowed_faults) {
			if (umb_mcb.GetPSPSeg() == pspseg) {
				umb_mcb.SetPSPSeg(MCB_FREE);
			}
			if (umb_mcb.GetType() == ending_mcb_type) {
				break;
			}
			faults += triage_block(umb_mcb, middle_mcb_type);
			umb_start += umb_mcb.GetSize() + 1;
			umb_mcb.SetPt(umb_start);
		}
	} else if (umb_start != 0xffff)
		LOG(LOG_DOSMISC, LOG_ERROR)("Corrupt UMB chain: %x", umb_start);

	DOS_CompressMemory();
}

uint16_t DOS_GetMemAllocStrategy()
{
	return allocation_strategy;
}

bool DOS_SetMemAllocStrategy(const uint16_t strat)
{
	if ((strat & 0x3f) < 3) {
		allocation_strategy = strat;
		return true;
	}
	/* otherwise an invalid allocation strategy was specified */
	return false;
}

bool DOS_AllocateMemory(uint16_t * segment,uint16_t * blocks) {
	DOS_CompressMemory();
	uint16_t bigsize=0;
	uint16_t mem_strat  = allocation_strategy;
	uint16_t mcb_segment=dos.firstMCB;

	uint16_t umb_start=dos_infoblock.GetStartOfUMBChain();
	if (umb_start == umb_start_seg) {
		/* start with UMBs if requested (bits 7 or 6 set) */
		if (mem_strat&0xc0) mcb_segment=umb_start;
	} else if (umb_start != 0xffff)
		LOG(LOG_DOSMISC, LOG_ERROR)("Corrupt UMB chain: %x", umb_start);

	DOS_MCB mcb(0);
	DOS_MCB mcb_next(0);
	DOS_MCB psp_mcb(dos.psp()-1);
	char psp_name[9];
	psp_mcb.GetFileName(psp_name);
	uint16_t found_seg=0,found_seg_size=0;
	for (;;) {
		mcb.SetPt(mcb_segment);
		if (mcb.GetPSPSeg()==MCB_FREE) {
			/* Check for enough free memory in current block */
			uint16_t block_size=mcb.GetSize();			
			if (block_size<(*blocks)) {
				if (bigsize<block_size) {
					/* current block is largest block that was found,
					   but still not as big as requested */
					bigsize=block_size;
				}
			} else if ((block_size==*blocks) && ((mem_strat & 0x3f)<2)) {
				/* MCB fits precisely, use it if search strategy is firstfit or bestfit */
				mcb.SetPSPSeg(dos.psp());
				*segment=mcb_segment+1;
				return true;
			} else {
				switch (mem_strat & 0x3f) {
					case 0: /* firstfit */
						mcb_next.SetPt((uint16_t)(mcb_segment+*blocks+1));
						mcb_next.SetPSPSeg(MCB_FREE);
						mcb_next.SetType(mcb.GetType());
						mcb_next.SetSize(block_size-*blocks-1);
						mcb.SetSize(*blocks);
						mcb.SetType(middle_mcb_type);
						mcb.SetPSPSeg(dos.psp());
						mcb.SetFileName(psp_name);
						//TODO Filename
						*segment=mcb_segment+1;
						return true;
					case 1: /* bestfit */
						if ((found_seg_size==0) || (block_size<found_seg_size)) {
							/* first fitting MCB, or smaller than the last that was found */
							found_seg=mcb_segment;
							found_seg_size=block_size;
						}
						break;
					default: /* everything else is handled as lastfit by dos */
						/* MCB is large enough, note it down */
						found_seg=mcb_segment;
						found_seg_size=block_size;
						break;
				}
			}
		}
		/* Onward to the next MCB if there is one */
		if (mcb.GetType() == ending_mcb_type) {
			if ((mem_strat & 0x80) && (umb_start == umb_start_seg)) {
				/* bit 7 set: try high memory first, then low */
				mcb_segment=dos.firstMCB;
				mem_strat&=(~0xc0);
			} else {
				/* finished searching all requested MCB chains */
				if (found_seg) {
					/* a matching MCB was found (cannot occur for firstfit) */
					if ((mem_strat & 0x3f)==0x01) {
						/* bestfit, allocate block at the beginning of the MCB */
						mcb.SetPt(found_seg);

						mcb_next.SetPt((uint16_t)(found_seg+*blocks+1));
						mcb_next.SetPSPSeg(MCB_FREE);
						mcb_next.SetType(mcb.GetType());
						mcb_next.SetSize(found_seg_size-*blocks-1);

						mcb.SetSize(*blocks);
						mcb.SetType(middle_mcb_type);
						mcb.SetPSPSeg(dos.psp());
						mcb.SetFileName(psp_name);
						//TODO Filename
						*segment=found_seg+1;
					} else {
						/* lastfit, allocate block at the end of the MCB */
						mcb.SetPt(found_seg);
						if (found_seg_size==*blocks) {
							/* use the whole block */
							mcb.SetPSPSeg(dos.psp());
							//Not consistent with line 124. But how many application will use this information ?
							mcb.SetFileName(psp_name);
							*segment = found_seg+1;
							return true;
						}
						*segment = found_seg+1+found_seg_size - *blocks;
						mcb_next.SetPt((uint16_t)(*segment-1));
						mcb_next.SetSize(*blocks);
						mcb_next.SetType(mcb.GetType());
						mcb_next.SetPSPSeg(dos.psp());
						mcb_next.SetFileName(psp_name);
						// Old Block
						mcb.SetSize(found_seg_size-*blocks-1);
						mcb.SetPSPSeg(MCB_FREE);
						mcb.SetType(middle_mcb_type);
					}
					return true;
				}
				/* no fitting MCB found, return size of largest block */
				*blocks=bigsize;
				DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
				return false;
			}
		} else
			mcb_segment += mcb.GetSize() + 1;
	}
	return false;
}


bool DOS_ResizeMemory(uint16_t segment,uint16_t * blocks) {
	if (segment < DOS_MEM_START+1) {
		LOG(LOG_DOSMISC,LOG_ERROR)("Program resizes %X, take care",segment);
	}
      
	DOS_MCB mcb(segment-1);
	if ((mcb.GetType() != middle_mcb_type) && (mcb.GetType() != ending_mcb_type)) {
		DOS_SetError(DOSERR_MCB_DESTROYED);
		return false;
	}

	DOS_CompressMemory();
	uint16_t total=mcb.GetSize();
	DOS_MCB	mcb_next(segment+total);
	if (*blocks<=total) {
		if (*blocks == total) {
			/* Size unchanged */
			mcb.SetPSPSeg(dos.psp());
			return true;
		}
		/* Shrinking MCB */
		DOS_MCB	mcb_new_next(segment+(*blocks));
		mcb.SetSize(*blocks);
		mcb_new_next.SetType(mcb.GetType());
		if (mcb.GetType() == ending_mcb_type) {
			/* Further blocks follow */
			mcb.SetType(middle_mcb_type);
		}

		mcb_new_next.SetSize(total-*blocks-1);
		mcb_new_next.SetPSPSeg(MCB_FREE);
		mcb.SetPSPSeg(dos.psp());
		DOS_CompressMemory();
		return true;
	}
	/* MCB will grow, try to join with following MCB */
	if (mcb.GetType() != ending_mcb_type) {
		if (mcb_next.GetPSPSeg()==MCB_FREE) {
			total+=mcb_next.GetSize()+1;
		}
	}
	if (*blocks<total) {
		if (mcb.GetType() != ending_mcb_type) {
			/* save type of following MCB */
			mcb.SetType(mcb_next.GetType());
		}
		mcb.SetSize(*blocks);
		mcb_next.SetPt((uint16_t)(segment+*blocks));
		mcb_next.SetSize(total-*blocks-1);
		mcb_next.SetType(mcb.GetType());
		mcb_next.SetPSPSeg(MCB_FREE);
		mcb.SetType(middle_mcb_type);
		mcb.SetPSPSeg(dos.psp());
		DOS_CompressMemory();
		return true;
	}

	/* at this point: *blocks==total (fits) or *blocks>total,
	   in the second case resize block to maximum */

	if ((mcb_next.GetPSPSeg() == MCB_FREE) && (mcb.GetType() != ending_mcb_type)) {
		/* adjust type of joined MCB */
		mcb.SetType(mcb_next.GetType());
	}
	mcb.SetSize(total);
	mcb.SetPSPSeg(dos.psp());
	if (*blocks == total) {
		/* block fit exactly */
		mcb.SetPSPSeg(dos.psp());
		return true;
	}

	*blocks=total;	/* return maximum */
	DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
	return false;
}


bool DOS_FreeMemory(uint16_t segment) {
//TODO Check if allowed to free this segment
	if (segment < DOS_MEM_START+1) {
		LOG(LOG_DOSMISC,LOG_ERROR)("Program tried to free %X ---ERROR",segment);
		DOS_SetError(DOSERR_MB_ADDRESS_INVALID);
		return false;
	}
      
	DOS_MCB mcb(segment-1);
	if ((mcb.GetType() != middle_mcb_type) && (mcb.GetType() != ending_mcb_type)) {
		DOS_SetError(DOSERR_MB_ADDRESS_INVALID);
		return false;
	}
	mcb.SetPSPSeg(MCB_FREE);
//	DOS_CompressMemory();
	return true;
}


void DOS_BuildUMBChain(bool umb_active, bool ems_active)
{
	if (umb_active && !is_machine_pcjr_or_tandy()) {
		uint16_t first_umb_seg = 0xd000;
		uint16_t first_umb_size = 0x2000;
		if(ems_active) first_umb_size = 0x1000;

		dos_infoblock.SetStartOfUMBChain(umb_start_seg);
		dos_infoblock.SetUMBChainState(0);		// UMBs not linked yet

		DOS_MCB umb_mcb(first_umb_seg);
		umb_mcb.SetPSPSeg(0);		// currently free
		umb_mcb.SetSize(first_umb_size-1);
		umb_mcb.SetType(ending_mcb_type);

		/* Scan MCB-chain for last block */
		uint16_t mcb_segment=dos.firstMCB;
		DOS_MCB mcb(mcb_segment);
		while (mcb.GetType() != ending_mcb_type) {
			mcb_segment+=mcb.GetSize()+1;
			mcb.SetPt(mcb_segment);
		}

		/* A system MCB has to cover the space between the
		   regular MCB-chain and the UMBs */
		auto cover_mcb = static_cast<uint16_t>(mcb_segment+mcb.GetSize() + 1);
		mcb.SetPt(cover_mcb);
		mcb.SetType(middle_mcb_type);
		mcb.SetPSPSeg(0x0008);
		mcb.SetSize(first_umb_seg-cover_mcb-1);
		mcb.SetFileName("SC      ");

	} else {
		dos_infoblock.SetStartOfUMBChain(0xffff);
		dos_infoblock.SetUMBChainState(0);
	}
}

bool DOS_LinkUMBsToMemChain(uint16_t linkstate) {
	/* Get start of UMB-chain */
	uint16_t umb_start=dos_infoblock.GetStartOfUMBChain();
	if (umb_start != umb_start_seg) {
		if (umb_start!=0xffff) LOG(LOG_DOSMISC,LOG_ERROR)("Corrupt UMB chain: %x",umb_start);
		return false;
	}

	if ((linkstate&1)==(dos_infoblock.GetUMBChainState()&1)) return true;
	
	/* Scan MCB-chain for last block before UMB-chain */
	uint16_t mcb_segment=dos.firstMCB;
	uint16_t prev_mcb_segment=dos.firstMCB;
	DOS_MCB mcb(mcb_segment);
	while ((mcb_segment != umb_start) && (mcb.GetType() != ending_mcb_type)) {
		prev_mcb_segment=mcb_segment;
		mcb_segment+=mcb.GetSize()+1;
		mcb.SetPt(mcb_segment);
	}
	DOS_MCB prev_mcb(prev_mcb_segment);

	switch (linkstate) {
		case 0x0000:	// unlink
		        if ((prev_mcb.GetType() == middle_mcb_type) &&
		            (mcb_segment == umb_start)) {
			        prev_mcb.SetType(ending_mcb_type);
		        }
		        dos_infoblock.SetUMBChainState(0);
		        break;
	        case 0x0001: // link
		        if (mcb.GetType() == ending_mcb_type) {
			        mcb.SetType(middle_mcb_type);
			        dos_infoblock.SetUMBChainState(1);
		        }
		        break;
	        default:
		        LOG_MSG("Invalid link state %x when reconfiguring MCB chain",
		                linkstate);
		        return false;
	        }

	return true;
}

static constexpr uint16_t kilobytes_to_segment(size_t address_kb)
{
	constexpr auto bytes_per_segment = 16;
	constexpr auto bytes_per_kilobyte = 1024;
	return (address_kb * bytes_per_kilobyte) / bytes_per_segment;
}

void DOS_SetupMemory(void) {
	/* Let dos claim a few bios interrupts. Makes DOSBox more compatible with 
	 * buggy games, which compare against the interrupt table. (probably a 
	 * broken linked list implementation) */
	uint16_t ihseg = 0x70;
	uint16_t ihofs = 0xF4;
	real_writeb(ihseg,ihofs,(uint8_t)0xCF);		//An IRET Instruction
	RealSetVec(0x01,RealMake(ihseg,ihofs));		//BioMenace (offset!=4)
	RealSetVec(0x02,RealMake(ihseg,ihofs));		//BioMenace (segment<0x8000)
	RealSetVec(0x03,RealMake(ihseg,ihofs));		//Alien Incident (offset!=0)
	RealSetVec(0x04,RealMake(ihseg,ihofs));		//Shadow President (lower byte of segment!=0)
	RealSetVec(0x0f,RealMake(ihseg,ihofs));		//Always a tricky one (Sound Blaster irq)

	// Create a dummy device MCB with PSPSeg=0x0008
	DOS_MCB mcb_devicedummy((uint16_t)DOS_MEM_START);
	mcb_devicedummy.SetPSPSeg(MCB_DOS);	// Devices
	mcb_devicedummy.SetSize(1);
	mcb_devicedummy.SetType(middle_mcb_type); // More blocks will follow
	//	mcb_devicedummy.SetFileName("SD      ");

	uint16_t mcb_sizes=2;
	// Create a small empty MCB (result from a growing environment block)
	DOS_MCB tempmcb((uint16_t)DOS_MEM_START+mcb_sizes);
	tempmcb.SetPSPSeg(MCB_FREE);
	tempmcb.SetSize(4);
	mcb_sizes+=5;
	tempmcb.SetType(middle_mcb_type);

	// Lock the previous empty MCB
	DOS_MCB tempmcb2((uint16_t)DOS_MEM_START+mcb_sizes);
	tempmcb2.SetPSPSeg(0x40);	// can be removed by loadfix
	tempmcb2.SetSize(16);
	mcb_sizes+=17;
	tempmcb2.SetType(middle_mcb_type);

	if (is_machine_tandy()) {
		/* memory up to 608k available, the rest (to 640k) is used by
			the tandy graphics system's variable mapping of 0xb800 */
		DOS_MCB free_block((uint16_t)DOS_MEM_START+mcb_sizes);
		free_block.SetPSPSeg(MCB_FREE);
		free_block.SetType(ending_mcb_type);
		free_block.SetSize(0x9BFF - DOS_MEM_START - mcb_sizes);
	} else if (is_machine_pcjr()) {
		const auto pcjr_start = DOS_MEM_START + mcb_sizes;
		constexpr auto mcb_entry_size = 1;

		// PCjr video memory uses 32KB shared RAM
		constexpr auto video_memory_start =
			kilobytes_to_segment(PcjrStandardMemorySizeKb - PcjrVideoMemorySizeKb);

		const auto section = get_section("dos");
		assert(section);
		const std::string pcjr_memory_config = section->GetString("pcjr_memory_config");
		if (pcjr_memory_config == "expanded") {
			// With expanded memory, reserve the lower memory up to video memory.
			// This makes application memory contiguous in order to prevent crashes.
			// This is needed to prevent Sierra AGI games from crashing.
			// Further details:
			// https://www.atarimagazines.com/compute/issue58/pcjr_memory.html

			// Space Quest version 1.0x is a special case.
			// It requires an additional 16 KB above the 32KB video memory to be reserved.
			constexpr auto application_segment =
				video_memory_start + kilobytes_to_segment(PcjrVideoMemorySizeKb + 16);

			// The size from the MCB entry itself must be subtracted from the total size.
			const auto reserved_size = application_segment - pcjr_start - mcb_entry_size;
			constexpr auto application_size = umb_start_seg - application_segment - mcb_entry_size;

			DOS_MCB reserved(pcjr_start);
			reserved.SetPSPSeg(MCB_DOS);
			reserved.SetSize(reserved_size);
			reserved.SetType(middle_mcb_type);

			DOS_MCB free_block(application_segment);
			free_block.SetPSPSeg(MCB_FREE);
			free_block.SetSize(application_size);
			free_block.SetType(ending_mcb_type);
		} else {
			// No expanded memory means the lower 96KB is usable for applications
			assert(pcjr_memory_config == "standard");

			DOS_MCB free_block(pcjr_start);
			free_block.SetPSPSeg(MCB_FREE);
			free_block.SetSize(video_memory_start - pcjr_start - mcb_entry_size);
			free_block.SetType(ending_mcb_type);
		}
	} else {
		/* complete memory up to 640k available */
		/* last paragraph used to add UMB chain to low-memory MCB chain */
		DOS_MCB free_block((uint16_t)DOS_MEM_START+mcb_sizes);
		free_block.SetPSPSeg(MCB_FREE);
		free_block.SetType(ending_mcb_type);
		free_block.SetSize(0x9FFE - DOS_MEM_START - mcb_sizes);
	}

	dos.firstMCB=DOS_MEM_START;
	dos_infoblock.SetFirstMCB(DOS_MEM_START);
}
