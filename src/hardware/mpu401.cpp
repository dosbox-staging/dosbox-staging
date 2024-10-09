/*
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

#include <cstring>

#include "cpu.h"
#include "inout.h"
#include "math_utils.h"
#include "midi.h"
#include "pic.h"
#include "setup.h"

static void MPU401_Event(uint32_t);
static void MPU401_Reset();
static void MPU401_ResetDone(uint32_t);
static void MPU401_EOIHandler(uint32_t val = 0);
static void MPU401_EOIHandlerDispatch();

constexpr uint8_t MPU401_VERSION = 0x15;
constexpr uint8_t MPU401_REVISION = 0x01;
constexpr uint8_t MPU401_QUEUE = 32;
constexpr double MPU401_EIO_DELAY = 0.06; // real delay is possibly a bit longer
constexpr double MPU401_TIMECONSTANT = (60000000 / 1000.0);
constexpr double MPU401_RESETBUSY = 14.0;

enum MpuMode { M_UART, M_INTELLIGENT };
enum MpuDataType { T_OVERFLOW, T_MARK, T_MIDI_SYS, T_MIDI_NORM, T_COMMAND };

static void MPU401_WriteData(io_port_t port, io_val_t value, io_width_t);

// Messages sent to MPU-401 from host
// constexpr uint8_t MSG_OVERFLOW = 0xf8; // unused
// constexpr uint8_t MSG_MARK = 0xfc; // unused

// Messages sent to host from MPU-401
//  constexpr uint8_t MSG_MPU_OVERFLOW = 0xf8; // unused
constexpr uint8_t MSG_MPU_COMMAND_REQ = 0xf9;
constexpr uint8_t MSG_MPU_END = 0xfc;
constexpr uint8_t MSG_MPU_CLOCK = 0xfd;
constexpr uint8_t MSG_MPU_ACK = 0xfe;
constexpr uint8_t MSG_MPU_RESET = 0xff;

struct MpuTrack {
	uint8_t counter  = 0;
	uint8_t value[8] = {};
	uint8_t sys_val  = 0;
	uint8_t vlength  = 0;
	uint8_t length   = 0;
	MpuDataType type = T_MIDI_NORM;
};

struct MpuState {
	bool conductor = false;
	bool cond_req  = false;
	bool cond_set  = false;

	bool block_ack = false;
	bool playing   = false;
	bool reset     = false;

	bool wsd       = false;
	bool wsm       = false;
	bool wsd_start = false;

	bool irq_pending     = false;
	bool send_now        = false;
	bool eoi_scheduled   = false;
	int8_t data_onoff    = 0;
	uint8_t command_byte = 0;
	uint8_t cmd_pending  = 0;

	uint8_t tmask = 0;
	uint8_t cmask = 0;
	uint8_t amask = 0;

	uint16_t midi_mask = 0;
	uint16_t req_mask  = 0;

	uint8_t channel  = 0;
	uint8_t old_chan = 0;
};

struct MpuClock {
	uint8_t timebase = 0;

	uint8_t tempo      = 0;
	uint8_t tempo_rel  = 0;
	uint8_t tempo_grad = 0;

	uint8_t cth_rate      = 0;
	uint8_t cth_counter   = 0;
	uint8_t cth_savecount = 0;

	bool clock_to_host = false;
};

struct Mpu {
	bool is_intelligent = false;
	MpuMode mode        = M_UART;

	// Princess Maker 2 wants it on irq 9
	uint8_t irq = 9;

	uint8_t queue[MPU401_QUEUE] = {};
	uint8_t queue_pos           = 0;
	uint8_t queue_used          = 0;

	MpuTrack playbuf[8] = {};
	MpuTrack condbuf    = {};

	MpuState state = {};

	MpuClock clock = {};
};

static Mpu mpu = {};

static void QueueByte(uint8_t data)
{
	if (mpu.state.block_ack) {
		mpu.state.block_ack = false;
		return;
	}
	if (!mpu.queue_used && mpu.is_intelligent) {
		mpu.state.irq_pending = true;
		PIC_ActivateIRQ(mpu.irq);
	}
	if (mpu.queue_used < MPU401_QUEUE) {
		assert(mpu.queue_pos <= MPU401_QUEUE);
		uint8_t pos = mpu.queue_used + mpu.queue_pos;
		pos -= (pos >= MPU401_QUEUE) ? MPU401_QUEUE : 0;
		mpu.queue_pos -= (mpu.queue_pos >= MPU401_QUEUE) ? MPU401_QUEUE : 0;
		mpu.queue_used++;
		assert(pos < MPU401_QUEUE);
		mpu.queue[pos] = data;
	} else
		LOG(LOG_MISC, LOG_NORMAL)("MPU401:Data queue full");
}

static void ClrQueue()
{
	mpu.queue_used = 0;
	mpu.queue_pos = 0;
}

static uint8_t MPU401_ReadStatus(io_port_t, io_width_t)
{
	uint8_t ret = 0x3f; // Bits 6 and 7 clear
	if (mpu.state.cmd_pending)
		ret |= 0x40;
	if (!mpu.queue_used)
		ret |= 0x80;
	return ret;
}

static void send_all_notes_off()
{
	for (auto channel = FirstMidiChannel; channel <= LastMidiChannel; ++channel) {
		MIDI_RawOutByte(MidiStatus::ControlChange | channel);
		MIDI_RawOutByte(MidiChannelMode::AllNotesOff);
		MIDI_RawOutByte(0);
	}
}

static void MPU401_WriteCommand(io_port_t, const io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	if (mpu.mode == M_UART && val != MSG_MPU_RESET)
		return;
	if (mpu.state.reset) {
		if (mpu.state.cmd_pending || val != MSG_MPU_RESET) {
			mpu.state.cmd_pending = val + 1;
			return;
		}
		PIC_RemoveEvents(MPU401_ResetDone);
		mpu.state.reset = false;
	}
	if (val <= 0x2f) {
		// MIDI stop, start, continue
		switch (val & 3) {
		case 1:
			MIDI_RawOutByte(MidiStatus::Stop);
			mpu.clock.cth_savecount = mpu.clock.cth_counter;
			break;

		case 2:
			MIDI_RawOutByte(MidiStatus::Start);
			mpu.clock.cth_counter = mpu.clock.cth_savecount = 0;
			break;
		case 3:
			MIDI_RawOutByte(MidiStatus::Continue);
			mpu.clock.cth_counter = mpu.clock.cth_savecount;
			break;
		}

		if (val & 0x20)
			LOG(LOG_MISC, LOG_ERROR)("MPU-401:Unhandled Recording Command %u", val);
		switch (val & 0xc) {
		case 0x4: // Stop
			if (mpu.state.playing && !mpu.clock.clock_to_host)
				PIC_RemoveEvents(MPU401_Event);
			mpu.state.playing = false;
			send_all_notes_off();
			break;
		case 0x8: // Play
			LOG(LOG_MISC, LOG_NORMAL)("MPU-401:Intelligent mode playback started");
			if (!mpu.state.playing && !mpu.clock.clock_to_host)
				PIC_AddEvent(MPU401_Event,
				             MPU401_TIMECONSTANT /
				                     (mpu.clock.tempo *
				                      mpu.clock.timebase));
			mpu.state.playing = true;
			ClrQueue();
			break;
		}
	} else if (val >= 0xa0 && val <= 0xa7) { // Request play counter
		if (mpu.state.cmask & (1 << (val & 7))) {
			QueueByte(mpu.playbuf[val & 7].counter);
		}
	} else if (val >= 0xd0 && val <= 0xd7) { // Send data
		mpu.state.old_chan = mpu.state.channel;
		mpu.state.channel = val & 7;
		mpu.state.wsd = true;
		mpu.state.wsm = false;
		mpu.state.wsd_start = true;
	} else
		switch (val) {
		case 0xdf: // Send system message
			mpu.state.wsd = false;
			mpu.state.wsm = true;
			mpu.state.wsd_start = true;
			break;

		case 0x8e: // Conductor
			mpu.state.cond_set = false;
			break;
		case 0x8f: mpu.state.cond_set = true; break;
		case 0x94: // Clock to host
			if (mpu.clock.clock_to_host && !mpu.state.playing)
				PIC_RemoveEvents(MPU401_Event);
			mpu.clock.clock_to_host = false;
			break;
		case 0x95:
			if (!mpu.clock.clock_to_host && !mpu.state.playing)
				PIC_AddEvent(MPU401_Event,
				             MPU401_TIMECONSTANT /
				                     (mpu.clock.tempo *
				                      mpu.clock.timebase));
			mpu.clock.clock_to_host = true;
			break;
			// Internal timebase
		case 0xc2: mpu.clock.timebase = 48; break;
		case 0xc3: mpu.clock.timebase = 72; break;
		case 0xc4: mpu.clock.timebase = 96; break;
		case 0xc5: mpu.clock.timebase = 120; break;
		case 0xc6: mpu.clock.timebase = 144; break;
		case 0xc7: mpu.clock.timebase = 168; break;
		case 0xc8: mpu.clock.timebase = 192; break;
		// Commands with data byte
		case 0xe0:
		case 0xe1:
		case 0xe2:
		case 0xe4:
		case 0xe6:
		case 0xe7:
		case 0xec:
		case 0xed:
		case 0xee:
		case 0xef: mpu.state.command_byte = val; break;
		// Commands 0xa# returning data
		case 0xab: // Request and clear recording counter
			QueueByte(MSG_MPU_ACK);
			QueueByte(0);
			return;
		case 0xac: // Request version
			QueueByte(MSG_MPU_ACK);
			QueueByte(MPU401_VERSION);
			return;
		case 0xad: // Request revision
			QueueByte(MSG_MPU_ACK);
			QueueByte(MPU401_REVISION);
			return;
		case 0xaf: // Request tempo
			QueueByte(MSG_MPU_ACK);
			QueueByte(mpu.clock.tempo);
			return;
		case 0xb1: // Reset relative tempo
			mpu.clock.tempo_rel = 40;
			break;
		case 0xb9: // Clear play map
		case 0xb8: // Clear play counters
			send_all_notes_off();
			for (uint8_t i = 0; i < 8; ++i) {
				mpu.playbuf[i].counter = 0;
				mpu.playbuf[i].type = T_OVERFLOW;
			}
			mpu.condbuf.counter = 0;
			mpu.condbuf.type = T_OVERFLOW;
			if (!(mpu.state.conductor = mpu.state.cond_set))
				mpu.state.cond_req = 0;
			mpu.state.amask = mpu.state.tmask;
			mpu.state.req_mask = 0;
			mpu.state.irq_pending = true;
			break;
		case MSG_MPU_RESET:
			LOG(LOG_MISC, LOG_NORMAL)("MPU-401:Reset %u", val);
			PIC_AddEvent(MPU401_ResetDone, MPU401_RESETBUSY);
			mpu.state.reset = true;
			if (mpu.mode == M_UART) {
				MPU401_Reset();
				return; // do not send ack in UART mode
			}
			MPU401_Reset();
			break;
		case 0x3f: // UART mode
			LOG(LOG_MISC, LOG_NORMAL)("MPU-401:Set UART mode %u", val);
			mpu.mode = M_UART;
			break;
		default:;
			// LOG(LOG_MISC,LOG_NORMAL)("MPU-401:Unhandled command %X",val);
		}
	QueueByte(MSG_MPU_ACK);
}

static uint8_t MPU401_ReadData(io_port_t, io_width_t)
{
	uint8_t ret = MSG_MPU_ACK;
	if (mpu.queue_used) {
		mpu.queue_pos -= (mpu.queue_pos >= MPU401_QUEUE) ? MPU401_QUEUE : 0;
		ret = mpu.queue[mpu.queue_pos];
		mpu.queue_pos++;
		mpu.queue_used--;
	}
	if (!mpu.is_intelligent) {
		return ret;
	}

	if (mpu.queue_used == 0)
		PIC_DeActivateIRQ(mpu.irq);

	if (ret >= 0xf0 && ret <= 0xf7) { // MIDI data request
		mpu.state.channel = ret & 7;
		mpu.state.data_onoff = 0;
		mpu.state.cond_req = false;
	}
	if (ret == MSG_MPU_COMMAND_REQ) {
		mpu.state.data_onoff = 0;
		mpu.state.cond_req = true;
		if (mpu.condbuf.type != T_OVERFLOW) {
			mpu.state.block_ack = true;
			MPU401_WriteCommand(0x331, mpu.condbuf.value[0],
			                    io_width_t::byte);
			if (mpu.state.command_byte)
				MPU401_WriteData(0x330, mpu.condbuf.value[1],
				                 io_width_t::byte);
		}
		mpu.condbuf.type = T_OVERFLOW;
	}
	if (ret == MSG_MPU_END || ret == MSG_MPU_CLOCK || ret == MSG_MPU_ACK) {
		mpu.state.data_onoff = -1;
		MPU401_EOIHandlerDispatch();
	}
	return ret;
}

static void MPU401_WriteData(io_port_t, io_val_t value, io_width_t)
{
	auto val = check_cast<uint8_t>(value);
	if (mpu.mode == M_UART) {
		// Always write the byte to device
		MIDI_RawOutByte(val);

		// In UART mode, the software communicates directly with the
		// MIDI device (sending it 16-bit MIDI words via the UART), which
		// can include the reset message. This is slightly different than
		// resetting the MPU (which reverts it back to intelligent mode,
		// amung other things). We can detect this in UART mode and apply
		// it generally, in addition to how the device handles it.
		// https://www.midi.org/specifications-old/item/table-1-summary-of-midi-message
		if (val == MSG_MPU_RESET) {
			MIDI_Reset();
		}
		return;
	}
	// 0xe# command data
	switch (mpu.state.command_byte) {
	case 0x00: break;
	case 0xe0: // Set tempo
		mpu.state.command_byte = 0;
		// range clamp of true MPU-401 (always between 4 and 250)
		val = clamp(val, static_cast<uint8_t>(4), static_cast<uint8_t>(250));
		mpu.clock.tempo = val;
		return;
	case 0xe1: // Set relative tempo
		mpu.state.command_byte = 0;
		if (val != 0x40) // default value
			LOG(LOG_MISC, LOG_ERROR)("MPU-401:Relative tempo change not implemented");
		return;
	case 0xe7: // Set internal clock to host interval
		mpu.state.command_byte = 0;
		mpu.clock.cth_rate = val >> 2;
		return;
	case 0xec: // Set active track mask
		mpu.state.command_byte = 0;
		mpu.state.tmask = val;
		return;
	case 0xed: // Set play counter mask
		mpu.state.command_byte = 0;
		mpu.state.cmask = val;
		return;
	case 0xee: // Set 1-8 MIDI channel mask
		mpu.state.command_byte = 0;
		mpu.state.midi_mask &= 0xff00;
		mpu.state.midi_mask |= val;
		return;
	case 0xef: // Set 9-16 MIDI channel mask
		mpu.state.command_byte = 0;
		mpu.state.midi_mask &= 0x00ff;
		mpu.state.midi_mask |= ((uint16_t)val) << 8;
		return;
	// case 0xe2:	//Set graduation for relative tempo
	// case 0xe4:	//Set metronome
	// case 0xe6:	//Set metronome measure length
	default: mpu.state.command_byte = 0; return;
	}
	static Bitu length, cnt, posd;
	if (mpu.state.wsd) { // Directly send MIDI message
		if (mpu.state.wsd_start) {
			mpu.state.wsd_start = 0;
			cnt = 0;
			switch (val & 0xf0) {
			case 0xc0:
			case 0xd0:
				mpu.playbuf[mpu.state.channel].value[0] = val;
				length = 2;
				break;
			case 0x80:
			case 0x90:
			case 0xa0:
			case 0xb0:
			case 0xe0:
				mpu.playbuf[mpu.state.channel].value[0] = val;
				length = 3;
				break;
			case 0xf0:
				LOG(LOG_MISC, LOG_ERROR)("MPU-401:Illegal WSD byte");
				mpu.state.wsd = 0;
				mpu.state.channel = mpu.state.old_chan;
				return;
			default: // MIDI with running status
				cnt++;
				MIDI_RawOutByte(
				        mpu.playbuf[mpu.state.channel].value[0]);
			}
		}
		if (cnt < length) {
			MIDI_RawOutByte(val);
			cnt++;
		}
		if (cnt == length) {
			mpu.state.wsd = 0;
			mpu.state.channel = mpu.state.old_chan;
		}
		return;
	}
	if (mpu.state.wsm) { // Directly send system message
		if (val == MidiStatus::EndOfExclusive) {
			MIDI_RawOutByte(MidiStatus::EndOfExclusive);
			mpu.state.wsm = 0;
			return;
		}
		if (mpu.state.wsd_start) {
			mpu.state.wsd_start = 0;
			cnt = 0;
			switch (val) {
			case 0xf2: length = 3; break;
			case 0xf3: length = 2; break;
			case 0xf6: length = 1; break;
			case 0xf0: length = 0; break;
			default: length = 0;
			}
		}
		if (!length || cnt < length) {
			MIDI_RawOutByte(val);
			cnt++;
		}
		if (cnt == length)
			mpu.state.wsm = 0;
		return;
	}
	if (mpu.state.cond_req) { // Command
		switch (mpu.state.data_onoff) {
		case -1: return;
		case 0: // Timing byte
			mpu.condbuf.vlength = 0;
			if (val < 0xf0) {
				mpu.state.data_onoff++;
			} else {
				mpu.state.data_onoff = -1;
				MPU401_EOIHandlerDispatch();
				return;
			}
			// A timing value of 0 means send it now!
			mpu.state.send_now = (val == 0);
			mpu.condbuf.counter = val;
			break;
		case 1: // Command byte #1
			mpu.condbuf.type = T_COMMAND;
			if (val == 0xf8 || val == 0xf9)
				mpu.condbuf.type = T_OVERFLOW;

			mpu.condbuf.value[mpu.condbuf.vlength] = val;
			mpu.condbuf.vlength++;

			if ((val & 0xf0) != 0xe0)
				MPU401_EOIHandlerDispatch();
			else
				mpu.state.data_onoff++;
			break;
		case 2: // Command byte #2
			mpu.condbuf.value[mpu.condbuf.vlength] = val;
			mpu.condbuf.vlength++;
			MPU401_EOIHandlerDispatch();
			break;
		}
		return;
	}
	// Data
	switch (mpu.state.data_onoff) {
	case -1: return;
	case 0: // Timing byte
		if (val < 0xf0)
			mpu.state.data_onoff = 1;
		else {
			mpu.state.data_onoff = -1;
			MPU401_EOIHandlerDispatch();
			return;
		}
		// A timing value of 0 means send it now!
		mpu.state.send_now = (val == 0);
		mpu.playbuf[mpu.state.channel].counter = val;
		break;
	case 1: // MIDI
		mpu.playbuf[mpu.state.channel].vlength++;
		posd = mpu.playbuf[mpu.state.channel].vlength;
		if (posd == 1) {
			switch (val & 0xf0) {
			case 0xf0: // System message or mark
				if (val > 0xf7) {
					mpu.playbuf[mpu.state.channel].type = T_MARK;
					mpu.playbuf[mpu.state.channel].sys_val = val;
					length = 1;
				} else {
					LOG(LOG_MISC, LOG_ERROR)("MPU-401:Illegal message");
					mpu.playbuf[mpu.state.channel].type = T_MIDI_SYS;
					mpu.playbuf[mpu.state.channel].sys_val = val;
					length = 1;
				}
				break;
			case 0xc0:
			case 0xd0: // MIDI Message
				mpu.playbuf[mpu.state.channel].type = T_MIDI_NORM;
				length = mpu.playbuf[mpu.state.channel].length = 2;
				break;
			case 0x80:
			case 0x90:
			case 0xa0:
			case 0xb0:
			case 0xe0:
				mpu.playbuf[mpu.state.channel].type = T_MIDI_NORM;
				length = mpu.playbuf[mpu.state.channel].length = 3;
				break;
			default: // MIDI data with running status
				posd++;
				mpu.playbuf[mpu.state.channel].vlength++;
				mpu.playbuf[mpu.state.channel].type = T_MIDI_NORM;
				length = mpu.playbuf[mpu.state.channel].length;
				break;
			}
		}
		if (!(posd == 1 && val >= 0xf0))
			mpu.playbuf[mpu.state.channel].value[posd - 1] = val;
		if (posd == length)
			MPU401_EOIHandlerDispatch();
	}
}

static void MPU401_IntelligentOut(uint8_t chan)
{
	uint8_t val = 0;
	switch (mpu.playbuf[chan].type) {
	case T_OVERFLOW: break;
	case T_MARK:
		val = mpu.playbuf[chan].sys_val;
		if (val == 0xfc) {
			MIDI_RawOutByte(val);
			mpu.state.amask &= ~(1 << chan);
			mpu.state.req_mask &= ~(1 << chan);
		}
		break;
	case T_MIDI_NORM:
		for (uint8_t i = 0; i < mpu.playbuf[chan].vlength; ++i)
			MIDI_RawOutByte(mpu.playbuf[chan].value[i]);
		break;
	default: break;
	}
}

static void UpdateTrack(uint8_t chan)
{
	MPU401_IntelligentOut(chan);
	if (mpu.state.amask & (1 << chan)) {
		mpu.playbuf[chan].vlength = 0;
		mpu.playbuf[chan].type = T_OVERFLOW;
		mpu.playbuf[chan].counter = 0xf0;
		mpu.state.req_mask |= (1 << chan);
	} else {
		if (mpu.state.amask == 0 && !mpu.state.conductor)
			mpu.state.req_mask |= (1 << 12);
	}
}

static void UpdateConductor()
{
	if (mpu.condbuf.value[0] == 0xfc) {
		mpu.condbuf.value[0] = 0;
		mpu.state.conductor = false;
		mpu.state.req_mask &= ~(1 << 9);
		if (mpu.state.amask == 0)
			mpu.state.req_mask |= (1 << 12);
		return;
	}
	mpu.condbuf.vlength = 0;
	mpu.condbuf.counter = 0xf0;
	mpu.state.req_mask |= (1 << 9);
}

static void MPU401_Event(io_val_t)
{
	if (mpu.mode == M_UART)
		return;

	const auto event_delay = MPU401_TIMECONSTANT /
	                         (mpu.clock.tempo * mpu.clock.timebase);
	if (mpu.state.irq_pending) {
		PIC_AddEvent(MPU401_Event, event_delay);
		return;
	}

	if (mpu.state.playing) {
		// Decrease counters
		for (uint8_t i = 0; i < 8; ++i) {
			if (mpu.state.amask & (1 << i)) {
				auto &counter = mpu.playbuf[i].counter;
				if (counter) {
					--counter;
				}
				if (!counter) {
					UpdateTrack(i);
				}
			}
		}
		if (mpu.state.conductor) {
			auto &counter = mpu.condbuf.counter;
			if (counter) {
				--counter;
			}
			if (!counter) {
				UpdateConductor();
			}
		}
	}
	if (mpu.clock.clock_to_host) {
		mpu.clock.cth_counter++;
		if (mpu.clock.cth_counter >= mpu.clock.cth_rate) {
			mpu.clock.cth_counter = 0;
			mpu.state.req_mask |= (1 << 13);
		}
	}
	if (!mpu.state.irq_pending && mpu.state.req_mask)
		MPU401_EOIHandler();

	PIC_AddEvent(MPU401_Event, event_delay);
}

static void MPU401_EOIHandlerDispatch()
{
	if (mpu.state.send_now) {
		mpu.state.eoi_scheduled = true;
		PIC_AddEvent(MPU401_EOIHandler, MPU401_EIO_DELAY);
	} else if (!mpu.state.eoi_scheduled)
		MPU401_EOIHandler();
}

// Updates counters and requests new data on "End of Input"
static void MPU401_EOIHandler(io_val_t)
{
	mpu.state.eoi_scheduled = false;
	if (mpu.state.send_now) {
		mpu.state.send_now = false;
		if (mpu.state.cond_req)
			UpdateConductor();
		else
			UpdateTrack(mpu.state.channel);
	}
	mpu.state.irq_pending = false;
	if (!mpu.state.req_mask)
		return;
	uint8_t i = 0;
	do {
		if (mpu.state.req_mask & (1 << i)) {
			QueueByte(0xf0 + i);
			mpu.state.req_mask &= ~(1 << i);
			break;
		}
	} while ((i++) < 16);
}

static void MPU401_ResetDone(uint32_t)
{
	mpu.state.reset = false;
	if (mpu.state.cmd_pending) {
		MPU401_WriteCommand(0x331, mpu.state.cmd_pending - 1,
		                    io_width_t::byte);
		mpu.state.cmd_pending = 0;
	}
}
static void MPU401_Reset()
{
	MIDI_Reset();
	PIC_DeActivateIRQ(mpu.irq);
	mpu.mode = (mpu.is_intelligent ? M_INTELLIGENT : M_UART);
	PIC_RemoveEvents(MPU401_Event);
	PIC_RemoveEvents(MPU401_EOIHandler);
	mpu.state.eoi_scheduled = false;
	mpu.state.wsd = false;
	mpu.state.wsm = false;
	mpu.state.conductor = false;
	mpu.state.cond_req = false;
	mpu.state.cond_set = false;
	mpu.state.playing = false;
	mpu.state.irq_pending = false;
	mpu.state.cmask = 0xff;
	mpu.state.amask = mpu.state.tmask = 0;
	mpu.state.midi_mask = 0xffff;
	mpu.state.data_onoff = -1;
	mpu.state.command_byte = 0;
	mpu.state.block_ack = false;
	mpu.clock.tempo = 100;
	mpu.clock.timebase = 120;
	mpu.clock.tempo_rel = 40;
	mpu.clock.tempo_grad = 0;
	mpu.clock.clock_to_host = false;
	mpu.clock.cth_rate = 60;
	mpu.clock.cth_counter = 0;
	mpu.clock.cth_savecount = 0;
	ClrQueue();
	mpu.state.req_mask = 0;
	mpu.condbuf.counter = 0;
	mpu.condbuf.type = T_OVERFLOW;
	for (uint8_t i = 0; i < 8; ++i) {
		mpu.playbuf[i].type = T_OVERFLOW;
		mpu.playbuf[i].counter = 0;
	}
}

class MPU401 final : public Module_base {
private:
	IO_ReadHandleObject ReadHandler[2]   = {};
	IO_WriteHandleObject WriteHandler[2] = {};
	bool is_installed                    = false;

public:
	MPU401(Section* configuration) : Module_base(configuration)
	{
		Section_prop* section = dynamic_cast<Section_prop*>(configuration);
		if (!section) {
			return;
		}

		const std::string mpu_choice = section->Get_string("mpu401");

		if (const auto has_bool = parse_bool_setting(mpu_choice);
		    has_bool && *has_bool == false) {
			return;
		}

		constexpr io_port_t port_0x330 = 0x330;
		constexpr io_port_t port_0x331 = 0x331;

		WriteHandler[0].Install(port_0x330, &MPU401_WriteData, io_width_t::byte);
		WriteHandler[1].Install(port_0x331, &MPU401_WriteCommand, io_width_t::byte);
		ReadHandler[0].Install(port_0x330, &MPU401_ReadData, io_width_t::byte);
		ReadHandler[1].Install(port_0x331, &MPU401_ReadStatus, io_width_t::byte);

		mpu = Mpu{};
		mpu.is_intelligent = (mpu_choice == "intelligent");
		if (mpu.is_intelligent) {
			// Set IRQ and unmask it(for timequest/princess maker 2)
			PIC_SetIRQMask(mpu.irq, false);
			MPU401_Reset();
		}

		LOG_MSG("MPU-401: Running in %s mode on ports %xh and %xh",
		        mpu.is_intelligent ? "intelligent" : "UART",
		        port_0x330,
		        port_0x331);

		is_installed = true;
	}
	~MPU401()
	{
		if (!is_installed) {
			return;
		}

		LOG_MSG("MPU-401: Shutting down");

		if (mpu.is_intelligent) {
			PIC_SetIRQMask(mpu.irq, true);
		}
		for (auto& handler : WriteHandler) {
			handler.Uninstall();
		}
		for (auto& handler : ReadHandler) {
			handler.Uninstall();
		}
		is_installed = false;
	}
};

static std::unique_ptr<MPU401> mpu401 = nullptr;

static Section_prop* get_midi_section()
{
	assert(control);

	auto sec = static_cast<Section_prop*>(control->GetSection("midi"));
	assert(sec);

	return sec;
}

void mpu401_destroy([[maybe_unused]] Section* sec)
{
	mpu401 = {};
}

void MPU401_Destroy()
{
	mpu401_destroy(get_midi_section());
}

void mpu401_init([[maybe_unused]] Section* sec)
{
	mpu401 = std::make_unique<MPU401>(get_midi_section());

	constexpr auto ChangeableAtRuntime = true;

	get_midi_section()->AddDestroyFunction(&mpu401_destroy, ChangeableAtRuntime);
}

void MPU401_Init()
{
	mpu401_init(get_midi_section());
}
