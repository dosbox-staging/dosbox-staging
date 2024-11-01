/*
 *  Copyright (C) 2022-2024  The DOSBox Staging Team
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

#include "dosbox.h"

#include <algorithm>
#include <cstring>
#include <memory>

#include "dma.h"
#include "mem.h"
#include "inout.h"
#include "pic.h"
#include "paging.h"
#include "setup.h"

std::unique_ptr<DmaController> primary   = {};
std::unique_ptr<DmaController> secondary = {};

#define EMM_PAGEFRAME4K ((0xE000 * 16) / dos_pagesize)
uint32_t ems_board_mapping[LINK_START];

// Constants
constexpr uint8_t SecondaryMin = 4;
constexpr uint8_t SecondaryMax = 7;

constexpr uint16_t NULL_PAGE = 0xffff;
static uint32_t dma_wrapping = NULL_PAGE; // initial value

static void UpdateEMSMapping()
{
	/* if EMS is not present, this will result in a 1:1 mapping */
	Bitu i;
	for (i = 0; i < 0x10; i++) {
		ems_board_mapping[EMM_PAGEFRAME4K + i] =
		        paging.firstmb[EMM_PAGEFRAME4K + i];
	}
}

// Generic function to read or write a block of data to or from memory.
// Don't use this directly; call two helpers: DMA_BlockRead or DMA_BlockWrite
static void perform_dma_io(const DMA_DIRECTION direction, const PhysPt spage,
                           PhysPt mem_address, void* const data_start,
                           const size_t num_words, const uint8_t is_dma16)
{
	assert(is_dma16 == 0 || is_dma16 == 1);

	const auto highpart_addr_page = spage >> 12;

	// Maybe move the mem_address into the 16-bit range
	mem_address <<= is_dma16;

	// The data pointer will be incremented per transfer
	auto data_pt = reinterpret_cast<uint8_t*>(data_start);

	// Convert from DMA 'words' to actual bytes, no greater than 64 KB
	auto remaining_bytes = check_cast<uint16_t>(num_words << is_dma16);
	do {
		// Find the right EMS page that contains the current address
		auto page = highpart_addr_page + (mem_address >> 12);
		if (page < EMM_PAGEFRAME4K) {
			page = paging.firstmb[page];
		} else if (page < EMM_PAGEFRAME4K + 0x10) {
			page = ems_board_mapping[page];
		} else if (page < LINK_START) {
			page = paging.firstmb[page];
		}

		// Calculate the offset within the page
		const auto pos_in_page       = mem_address & (dos_pagesize - 1);
		const auto bytes_to_page_end = check_cast<uint16_t>(
		        dos_pagesize - pos_in_page);
		const auto chunk_start = check_cast<PhysPt>(page * dos_pagesize +
		                                            pos_in_page);

		// Determine how many bytes to transfer within this page
		const auto chunk_bytes = std::min(remaining_bytes, bytes_to_page_end);

		// Copy the data from the page address into the data pointer
		if (direction == DMA_DIRECTION::READ) {
			for (auto i = 0; i < chunk_bytes; ++i) {
				data_pt[i] = phys_readb(chunk_start + i);
			}
		}

		// Copy the data from the data pointer into the page address
		else if (direction == DMA_DIRECTION::WRITE) {
			for (auto i = 0; i < chunk_bytes; ++i) {
				phys_writeb(chunk_start + i, data_pt[i]);
			}
		}

		mem_address += chunk_bytes;
		data_pt += chunk_bytes;
		remaining_bytes -= chunk_bytes;
	} while (remaining_bytes);
}

void TANDYSOUND_ShutDown(Section* = nullptr);

static bool activate_primary()
{
	assert(!primary);
	constexpr uint8_t primary_index = 0;
	primary = std::make_unique<DmaController>(primary_index);
	return true;
}

static bool activate_secondary()
{
	assert(!secondary);

	// The secondary controller and Tandy Sound device conflict in
	// their use of the 0xc0 IO ports. This is a unique architectural
	// conflict, so we explicitly shutdown the TandySound device (if
	// it happens to be running) to meet this request.
	//
	TANDYSOUND_ShutDown();

	constexpr uint8_t secondary_index = 1;
	secondary = std::make_unique<DmaController>(secondary_index);
	return true;
}

constexpr uint8_t is_primary(const uint8_t channel_num)
{
	return (channel_num < SecondaryMin);
}

constexpr uint8_t is_secondary(const uint8_t channel_num)
{
	return (channel_num >= SecondaryMin && channel_num <= SecondaryMax);
}

constexpr uint8_t to_secondary_num(const uint8_t channel_num)
{
	assert(channel_num >= SecondaryMin);
	assert(channel_num <= SecondaryMax);
	return (static_cast<uint8_t>(channel_num - SecondaryMin));
}

DmaChannel* DMA_GetChannel(const uint8_t channel_num)
{
	if (is_primary(channel_num) && (primary || activate_primary())) {
		return primary->GetChannel(channel_num);
	}
	if (is_secondary(channel_num) && (secondary || activate_secondary())) {
		return secondary->GetChannel(to_secondary_num(channel_num));
	}
	return nullptr;
}

void DMA_ShutdownSecondaryController()
{
	secondary = {};
}

static DmaChannel* GetChannelFromPort(const io_port_t port)
{
	uint8_t num = UINT8_MAX;
	switch (port) {
	/* read DMA page register */
	case 0x81: num = 2; break;
	case 0x82: num = 3; break;
	case 0x83: num = 1; break;
	case 0x87: num = 0; break;
	case 0x89: num = 6; break;
	case 0x8a: num = 7; break;
	case 0x8b: num = 5; break;
	case 0x8f: num = 4; break;
	default:
		LOG_WARNING("DMA: Attempted to lookup DMA channel from invalid port %04x",
		            port);
	}
	return DMA_GetChannel(num);
}

static void DMA_Write_Port(const io_port_t port, const io_val_t value, io_width_t)
{
	const auto val = check_cast<uint16_t>(value);
	// LOG(LOG_DMACONTROL,LOG_ERROR)("Write %" sBitfs(X) " %"
	// sBitfs(X),port,val);
	if (port < 0x10) {
		/* write to the first DMA controller (channels 0-3) */
		primary->WriteControllerReg(port, val, io_width_t::byte);
	} else if (port >= 0xc0 && port <= 0xdf) {
		/* write to the second DMA controller (channels 4-7) */
		secondary->WriteControllerReg((port - 0xc0) >> 1, val, io_width_t::byte);
	} else {
		UpdateEMSMapping();
		auto channel = GetChannelFromPort(port);
		if (channel) {
			channel->SetPage(check_cast<uint8_t>(val));
		}
	}
}

static uint16_t DMA_Read_Port(const io_port_t port, const io_width_t width)
{
	// LOG(LOG_DMACONTROL,LOG_ERROR)("Read %" sBitfs(X),port);
	if (port < 0x10) {
		/* read from the first DMA controller (channels 0-3) */
		return primary->ReadControllerReg(port, width);
	} else if (port >= 0xc0 && port <= 0xdf) {
		/* read from the second DMA controller (channels 4-7) */
		return secondary->ReadControllerReg((port - 0xc0) >> 1, width);
	} else {
		const auto channel = GetChannelFromPort(port);
		if (channel) {
			return channel->page_num;
		}
	}
	return 0;
}

void DmaController::WriteControllerReg(const io_port_t reg,
                                       const io_val_t value, io_width_t)
{
	auto val         = check_cast<uint16_t>(value);
	DmaChannel* chan = nullptr;
	switch (reg) {
	/* set base address of DMA transfer (1st byte low part, 2nd byte high
	 * part) */
	case 0x0:
	case 0x2:
	case 0x4:
	case 0x6:
		UpdateEMSMapping();
		chan     = GetChannel((uint8_t)(reg >> 1));
		flipflop = !flipflop;
		if (flipflop) {
			chan->base_addr = (chan->base_addr & 0xff00) | val;
			chan->curr_addr = (chan->curr_addr & 0xff00) | val;
		} else {
			chan->base_addr = (chan->base_addr & 0x00ff) | (val << 8);
			chan->curr_addr = (chan->curr_addr & 0x00ff) | (val << 8);
		}
		break;
	/* set DMA transfer count (1st byte low part, 2nd byte high part) */
	case 0x1:
	case 0x3:
	case 0x5:
	case 0x7:
		UpdateEMSMapping();
		chan     = GetChannel((uint8_t)(reg >> 1));
		flipflop = !flipflop;
		if (flipflop) {
			chan->base_count = (chan->base_count & 0xff00) | val;
			chan->curr_count = (chan->curr_count & 0xff00) | val;
		} else {
			chan->base_count = (chan->base_count & 0x00ff) | (val << 8);
			chan->curr_count = (chan->curr_count & 0x00ff) | (val << 8);
		}
		break;
	case 0x8: /* Comand reg not used */ break;
	case 0x9: /* Request registers, memory to memory */
		// TODO Warning?
		break;
	case 0xa: /* Mask Register */
		if ((val & 0x4) == 0) {
			UpdateEMSMapping();
		}
		chan = GetChannel(val & 3);
		chan->SetMask((val & 0x4) > 0);
		break;
	case 0xb: /* Mode Register */
		UpdateEMSMapping();
		chan                 = GetChannel(val & 3);
		chan->is_autoiniting = (val & 0x10) > 0;
		chan->is_incremented = (val & 0x20) > 0;
		// TODO Maybe other bits?
		break;
	case 0xc: /* Clear Flip/Flip */ flipflop = false; break;
	case 0xd: /* Clear/Reset all channels */
		for (uint8_t ct = 0; ct < 4; ct++) {
			chan = GetChannel(ct);
			chan->SetMask(true);
			chan->has_reached_terminal_count = false;
		}
		flipflop = false;
		break;
	case 0xe: /* Clear Mask register */
		UpdateEMSMapping();
		for (uint8_t ct = 0; ct < 4; ct++) {
			chan = GetChannel(ct);
			chan->SetMask(false);
		}
		break;
	case 0xf: /* Multiple Mask register */
		UpdateEMSMapping();
		for (uint8_t ct = 0; ct < 4; ct++) {
			chan = GetChannel(ct);
			chan->SetMask(val & 1);
			val >>= 1;
		}
		break;
	}
}

uint16_t DmaController::ReadControllerReg(const io_port_t reg, io_width_t)
{
	DmaChannel* chan = nullptr;
	uint16_t ret     = 0;
	switch (reg) {
	/* read base address of DMA transfer (1st byte low part, 2nd byte high
	 * part) */
	case 0x0:
	case 0x2:
	case 0x4:
	case 0x6:
		chan     = GetChannel((uint8_t)(reg >> 1));
		flipflop = !flipflop;
		if (flipflop) {
			return chan->curr_addr & 0xff;
		} else {
			return (chan->curr_addr >> 8) & 0xff;
		}
	/* read DMA transfer count (1st byte low part, 2nd byte high part) */
	case 0x1:
	case 0x3:
	case 0x5:
	case 0x7:
		chan     = GetChannel((uint8_t)(reg >> 1));
		flipflop = !flipflop;
		if (flipflop) {
			return chan->curr_count & 0xff;
		} else {
			return (chan->curr_count >> 8) & 0xff;
		}
	case 0x8: /* Status Register */
		ret = 0;
		for (uint8_t ct = 0; ct < 4; ct++) {
			chan = GetChannel(ct);
			if (chan->has_reached_terminal_count) {
				ret |= 1 << ct;
			}
			chan->has_reached_terminal_count = false;
			if (chan->has_raised_request) {
				ret |= 1 << (4 + ct);
			}
		}
		return ret;
	default:
		LOG(LOG_DMACONTROL, LOG_NORMAL)
		("Trying to read undefined DMA port %x", reg);
		break;
	}
	return 0xffff;
}

DmaChannel::DmaChannel(const uint8_t num, const bool is_dma_16bit)
        : chan_num(num),
          is_16bit(is_dma_16bit ? 0x1 : 0x0)
{
	if (num == 4) {
		return;
	}
	assert(is_masked);
	assert(is_incremented);
}

void DmaChannel::DoCallback(const DMAEvent event) const
{
	if (callback) {
		callback(this, event);
	}
}

void DmaChannel::SetMask(const bool _mask)
{
	is_masked = _mask;
	DoCallback(is_masked ? DMA_MASKED : DMA_UNMASKED);
}

void DmaChannel::RegisterCallback(const DMA_Callback _cb)
{
	callback = _cb;
	SetMask(is_masked);
	if (callback) {
		RaiseRequest();
	} else {
		ClearRequest();
	}
}

void DmaChannel::ReachedTerminalCount()
{
	has_reached_terminal_count = true;
	DoCallback(DMA_REACHED_TC);
}

void DmaChannel::SetPage(const uint8_t val)
{
	page_num = val;
	page_base = (page_num >> is_16bit) << (16 + is_16bit);
}

void DmaChannel::RaiseRequest()
{
	has_raised_request = true;
}

void DmaChannel::ClearRequest()
{
	has_raised_request = false;
}

size_t DmaChannel::Read(const size_t words, uint8_t* const dest_buffer)
{
	return ReadOrWrite(DMA_DIRECTION::READ, words, dest_buffer);
}

size_t DmaChannel::Write(const size_t words, uint8_t* const src_buffer)
{
	return ReadOrWrite(DMA_DIRECTION::WRITE, words, src_buffer);
}

size_t DmaChannel::ReadOrWrite(const DMA_DIRECTION direction,
                               const size_t words, uint8_t* const buffer)
{
	auto want     = check_cast<uint16_t>(words);
	uint16_t done = 0;
	curr_addr &= dma_wrapping;

	// incremented per transfer
	auto curr_buffer = buffer;
again:
	Bitu left = (curr_count + 1);
	if (want < left) {
		perform_dma_io(direction, page_base, curr_addr, curr_buffer, want, is_16bit);
		done += want;
		curr_addr += want;
		curr_count -= want;
	} else {
		perform_dma_io(direction, page_base, curr_addr, curr_buffer, left, is_16bit);
		curr_buffer += left << is_16bit;
		want -= left;
		done += left;
		ReachedTerminalCount();
		if (is_autoiniting) {
			curr_count = base_count;
			curr_addr  = base_addr;
			if (want) {
				goto again;
			}
			UpdateEMSMapping();
		} else {
			curr_addr += left;
			curr_count = 0xffff;
			is_masked  = true;
			UpdateEMSMapping();
			DoCallback(DMA_MASKED);
		}
	}
	return done;
}

bool DmaChannel::HasReservation() const
{
	return (reservation_callback && !reservation_owner.empty());
}

void DmaChannel::EvictReserver()
{
	assert(HasReservation());

	reservation_callback(nullptr);

	reservation_callback = {};
	reservation_owner    = {};
}

void DmaChannel::ReserveFor(const std::string& new_owner,
                            const DMA_ReservationCallback new_cb)
{
	assert(new_cb);
	assert(!new_owner.empty());

	if (HasReservation()) {
		LOG_MSG("DMA: %s is replacing %s on %d-bit DMA channel %u",
		        new_owner.c_str(),
		        reservation_owner.c_str(),
		        is_16bit == 1 ? 16 : 8,
		        chan_num);
		EvictReserver();
	}
	Reset();
	reservation_callback = new_cb;
	reservation_owner    = new_owner;
}

void DmaChannel::Reset()
{
	// Defaults at the time of initialization
	page_base  = 0;
	curr_addr = 0;

	base_addr  = 0;
	base_count = 0;
	curr_count = 0;

	page_num = 0;

	is_incremented             = true;
	is_autoiniting             = false;
	is_masked                  = true;
	has_reached_terminal_count = false;
	has_raised_request         = false;

	callback             = {};
	reservation_callback = {};
	reservation_owner    = {};
}

void DmaChannel::LogDetails() const
{
	LOG_DEBUG(
	        "DMA[%u]: %s %d-bit DMA has base count:%4u, "
	        "current count:%4u, is auto-init: %d, is masked: %d, "
	        " has reached TC: %d, has raised IRQ request: %d",
	        chan_num,
	        reservation_owner.c_str(),
	        is_16bit == 1 ? 16 : 8,
	        base_count,
	        curr_count,
	        is_autoiniting,
	        is_masked,
	        has_reached_terminal_count,
	        has_raised_request);
}

DmaChannel::~DmaChannel()
{
	if (HasReservation()) {
		LOG_MSG("DMA: Shutting down %s on %d-bit DMA channel %d",
		        reservation_owner.c_str(),
		        is_16bit == 1 ? 16 : 8,
		        chan_num);
		EvictReserver();
	}
}

DmaController::DmaController(const uint8_t controller_index)
        : index(controller_index)
{
	// first or second DMA controller
	assert(index == 0 || index == 1);

	constexpr auto n = static_cast<uint8_t>(ARRAY_LEN(dma_channels));

	for (uint8_t i = 0; i < n; ++i) {
		const auto channel_num = static_cast<uint8_t>(i + index * n);
		const auto is_16bit    = (index == 1);
		dma_channels[i] = std::make_unique<DmaChannel>(channel_num, is_16bit);
	}

	for (io_port_t i = 0; i < 0x10; ++i) {
		const io_width_t width = (i < 8) ? io_width_t::word
		                                 : io_width_t::byte;

		// Install handler for primary DMA controller ports
		if (index == 0) {
			io_write_handlers[i].Install(i, DMA_Write_Port, width);
			io_read_handlers[i].Install(i, DMA_Read_Port, width);
		}
		// Install handler for secondary DMA controller ports
		else if (IS_EGAVGA_ARCH) {
			assert(index == 1);
			const auto dma_port = static_cast<io_port_t>(0xc0 + i * 2);
			io_write_handlers[i].Install(dma_port, DMA_Write_Port, width);
			io_read_handlers[i].Install(dma_port, DMA_Read_Port, width);
		}
	}
	// Install handlers for ports 0x81-0x83,0x87 (on the primary)
	if (index == 0) {
		io_write_handlers[0x10].Install(0x81, DMA_Write_Port, io_width_t::byte, 3);
		io_read_handlers[0x10].Install(0x81, DMA_Read_Port, io_width_t::byte, 3);
		io_write_handlers[0x11].Install(0x87, DMA_Write_Port, io_width_t::byte, 1);
		io_read_handlers[0x11].Install(0x87, DMA_Read_Port, io_width_t::byte, 1);
	}
	// Install handlers for ports 0x89-0x8b,0x8f (on the secondary)
	else if (IS_EGAVGA_ARCH) {
		assert(index == 1);
		io_write_handlers[0x10].Install(0x89, DMA_Write_Port, io_width_t::byte, 3);
		io_read_handlers[0x10].Install(0x89, DMA_Read_Port, io_width_t::byte, 3);
		io_write_handlers[0x11].Install(0x8f, DMA_Write_Port, io_width_t::byte, 1);
		io_read_handlers[0x11].Install(0x8f, DMA_Read_Port, io_width_t::byte, 1);
	}

	LOG_MSG("DMA: Initialised %s controller",
	        (index == 0 ? "primary" : "secondary"));
}

DmaController::~DmaController()
{
	LOG_MSG("DMA: Shutting down %s controller",
	        (index == 0 ? "primary" : "secondary"));

	// Deregister the controller's IO handlers
	for (auto& rh : io_read_handlers) {
		rh.Uninstall();
	}
	for (auto& wh : io_write_handlers) {
		wh.Uninstall();
	}

	for (auto& channel : dma_channels) {
		channel = {};
	}
}

DmaChannel* DmaController::GetChannel(const uint8_t chan) const
{
	constexpr auto n = ARRAY_LEN(dma_channels);
	return (chan < n) ? dma_channels[chan].get() : nullptr;
}

void DmaController::ResetChannel(const uint8_t channel_num) const
{
	if (auto c = GetChannel(channel_num); c) {
		c->Reset();
	}
}

void DMA_ResetChannel(const uint8_t channel_num)
{
	if (is_primary(channel_num) && primary) {
		primary->ResetChannel(channel_num);
	} else if (is_secondary(channel_num) && secondary) {
		secondary->ResetChannel(to_secondary_num(channel_num));
	}
}

void DMA_SetWrapping(const uint32_t wrap)
{
	dma_wrapping = wrap;
}

void DMA_Destroy(Section* /*sec*/)
{
	primary   = {};
	secondary = {};
}
void DMA_Init(Section* sec)
{
	DMA_SetWrapping(0xffff);
	sec->AddDestroyFunction(&DMA_Destroy);
	Bitu i;
	for (i = 0; i < LINK_START; i++) {
		ems_board_mapping[i] = i;
	}
}
