/*
 *  Copyright (C) 2002-2022  The DOSBox Team
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

#include "opl.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sys/types.h>

#include "../capture/capture.h"
#include "cpu.h"
#include "mapper.h"
#include "mem.h"
#include "setup.h"
#include "support.h"

#ifdef _MSC_VER
#	pragma pack(1)
#endif

#define HW_OPL2     0
#define HW_DUALOPL2 1
#define HW_OPL3     2

static std::unique_ptr<OPL> opl = {};

Timer::Timer(int16_t micros)
        : clock_interval(micros * 0.001) // interval in milliseconds
{
	SetCounter(0);
}

// Update returns with true if overflow
// Properly syncs up the start/end to current time and changing intervals
bool Timer::Update(const double time)
{
	if (enabled && (time >= trigger)) {
		// How far into the next cycle
		const double deltaTime = time - trigger;
		// Sync start to last cycle
		const auto counter_mod = fmod(deltaTime, counter_interval);

		start   = time - counter_mod;
		trigger = start + counter_interval;
		// Only set the overflow flag when not masked
		if (!masked)
			overflow = true;
	}
	return overflow;
}

void Timer::Reset()
{
	// On a reset make sure the start is in sync with the next cycle
	overflow = false;
}

void Timer::SetCounter(const uint8_t val)
{
	counter = val;
	// Interval for next cycle
	counter_interval = (256 - counter) * clock_interval;
}

void Timer::SetMask(const bool set)
{
	masked = set;
	if (masked)
		overflow = false;
}

void Timer::Stop()
{
	enabled = false;
}

void Timer::Start(const double time)
{
	// Only properly start when not running before
	if (!enabled) {
		enabled  = true;
		overflow = false;
		// Sync start to the last clock interval
		const auto clockMod = fmod(time, clock_interval);

		start = time - clockMod;
		// Overflow trigger
		trigger = start + counter_interval;
	}
}

struct RawHeader {
	uint8_t id[8];         // 0x00, "DBRAWOPL"
	uint16_t version_high; // 0x08, size of the data following the m
	uint16_t version_low;  // 0x0a, size of the data following the m
	uint32_t commands;     // 0x0c, uint32_t amount of command/data pairs
	uint32_t milliseconds; // 0x10, uint32_t Total milliseconds of data in
	                       // this chunk
	uint8_t hardware;      // 0x14, uint8_t Hardware Type
	                       // 0=opl2,1=dual-opl2,2=opl3
	uint8_t format; // 0x15, uint8_t Format 0=cmd/data interleaved, 1 maybe
	                // all cdms, followed by all data
	uint8_t compression;     // 0x16, uint8_t Compression Type, 0 = No
	                         // Compression
	uint8_t delay256;        // 0x17, uint8_t Delay 1-256 msec command
	uint8_t delay_shift8;    // 0x18, uint8_t (delay + 1)*256
	uint8_t conv_table_size; // 0x191, uint8_t Raw Conversion Table size
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#	pragma pack()
#endif

// The Raw Tables is < 128 and is used to convert raw commands into a full
// register index. When the high bit of a raw command is set it indicates the
// cmd/data pair is to be sent to the 2nd port. After the conversion table the
// raw data follows immediatly till the end of the chunk.

// Table to map the opl register to one <127 for dro saving
class Capture {
public:
	bool DoWrite(const io_port_t reg_full, const uint8_t val)
	{
		const auto reg_mask = reg_full & 0xff;

		// Check the raw index for this register if we actually have to
		// save it
		if (handle) {
			// Check if we actually care for this to be logged,
			// else just ignore it
			uint8_t raw = to_raw[reg_mask];
			if (raw == 0xff) {
				return true;
			}
			// Check if this command will not just replace the same
			// value in a reg that doesn't do anything with it
			if ((*cache)[reg_full] == val)
				return true;

			// Check how much time has passed
			uint32_t passed = PIC_Ticks - lastTicks;
			lastTicks       = PIC_Ticks;
			header.milliseconds += passed;

			// if ( passed > 0 ) LOG_MSG( "Delay %d", passed ) ;

			// If we passed more than 30 seconds since the last
			// command, we'll restart the the capture
			if (passed > 30000) {
				CloseFile();
				goto skipWrite;
			}
			while (passed > 0) {
				if (passed < 257) { // 1-256 millisecond delay
					AddBuf(delay256,
					       check_cast<uint8_t>(passed - 1));
					passed = 0;
				} else {
					const auto shift = (passed >> 8);
					passed -= shift << 8;
					AddBuf(delay_shift8,
					       check_cast<uint8_t>(shift - 1));
				}
			}
			AddWrite(reg_full, val);
			return true;
		}
	skipWrite:
		// Not yet capturing to a file here. Check for commands that
		// would start capturing, if it's not one of them return.

		// Note on in any channel
		const auto note_on = reg_mask >= 0xb0 && reg_mask <= 0xb8 &&
		                     (val & 0x020);

		// Percussion mode enabled and a note on in any percussion
		// instrument
		const auto percussion_on = reg_mask == 0xbd && ((val & 0x3f) > 0x20);

		if (!(note_on || percussion_on))
			return true;

		handle = CAPTURE_CreateFile(CaptureType::RawOplStream);
		if (!handle)
			return false;

		InitHeader();

		// Prepare space at start of the file for the header
		fwrite(&header, 1, sizeof(header), handle);
		// Write the Raw To Reg table
		fwrite(&to_reg, 1, raw_used, handle);
		// Write the cache of last commands
		WriteCache();
		// Write the command that triggered this
		AddWrite(reg_full, val);

		// Init the timing information for the next commands
		lastTicks  = PIC_Ticks;
		startTicks = PIC_Ticks;
		return true;
	}

	Capture(RegisterCache *_cache) : header(), cache(_cache)
	{
		LOG_MSG("CAPTURE: Preparing to capture raw OPL output; capturing will start when OPL output starts");
		MakeTables();
	}

	virtual ~Capture()
	{
		CloseFile();
		LOG_MSG("CAPTURE: Stopped capturing raw OPL output");
	}

	// prevent copy
	Capture(const Capture &) = delete;

	// prevent assignment
	Capture &operator=(const Capture &) = delete;

private:
	uint8_t to_reg[127];  // 127 entries to go from raw data to registers
	uint8_t raw_used = 0; // How many entries in the ToPort are used
	uint8_t to_raw[256];  // 256 entries to go from port index to raw data
	                      //
	uint8_t delay256     = 0;
	uint8_t delay_shift8 = 0;

	RawHeader header;

	FILE *handle = nullptr;  // File used for writing
	                         //
	uint32_t startTicks = 0; // Start used to check total raw length on end
	uint32_t lastTicks  = 0; // Last ticks when last last cmd was added
	uint8_t buf[1024];       // 16 added for delay commands and what not
	uint32_t bufUsed = 0;

	RegisterCache *cache;

	void MakeEntry(const uint8_t reg, uint8_t &raw)
	{
		to_reg[raw] = reg;
		to_raw[reg] = raw;
		++raw;
	}

	void MakeTables()
	{
		uint8_t index = 0;
		memset(to_reg, 0xff, sizeof(to_reg));
		memset(to_raw, 0xff, sizeof(to_raw));

		// Select the entries that are valid and the index is the
		// mapping to the index entry
		MakeEntry(0x01, index); // 0x01: Waveform select
		MakeEntry(0x04, index); // 104: Four-Operator Enable
		MakeEntry(0x05, index); // 105: OPL3 Mode Enable
		MakeEntry(0x08, index); // 08: CSW / NOTE-SEL
		MakeEntry(0xbd, index); // BD: Tremolo Depth / Vibrato Depth /
		                        // Percussion Mode / BD/SD/TT/CY/HH On

		// Add the 32 byte range that hold the 18 operators
		for (uint8_t i = 0; i < 24; ++i) {
			if ((i & 7) < 6) {
				MakeEntry(0x20 + i, index); // 20-35: Tremolo /
				                            // Vibrato / Sustain
				                            // / KSR / Frequency
				                            // Multiplication Facto
				MakeEntry(0x40 + i, index); // 40-55: Key Scale
				                            // Level / Output Level
				MakeEntry(0x60 + i, index); // 60-75: Attack
				                            // Rate / Decay Rate
				MakeEntry(0x80 + i, index); // 80-95: Sustain
				                            // Level / Release
				                            // Rate
				MakeEntry(0xe0 + i, index); // E0-F5: Waveform
				                            // Select
			}
		}

		// Add the 9 byte range that hold the 9 channels
		for (uint8_t i = 0; i < 9; ++i) {
			MakeEntry(0xa0 + i, index); // A0-A8: Frequency Number
			MakeEntry(0xb0 + i, index); // B0-B8: Key On / Block
			                            // Number / F-Number(hi
			                            // bits)
			MakeEntry(0xc0 + i, index); // C0-C8: FeedBack Modulation
			                            // Factor / Synthesis Type
		}

		// Store the amount of bytes the table contains
		raw_used = index;

		//	assert( raw_used <= 127 );
		delay256     = raw_used;
		delay_shift8 = raw_used + 1;
	}

	void ClearBuf()
	{
		fwrite(buf, 1, bufUsed, handle);
		header.commands += bufUsed / 2;
		bufUsed = 0;
	}

	void AddBuf(const uint8_t raw, const uint8_t val)
	{
		buf[bufUsed++] = raw;
		buf[bufUsed++] = val;
		if (bufUsed >= sizeof(buf)) {
			ClearBuf();
		}
	}

	void AddWrite(const io_port_t reg_full, const uint8_t val)
	{
		const uint8_t reg_mask = reg_full & 0xff;
		//  Do some special checks if we're doing opl3 or dualopl2
		// commands Although you could pretty much just stick to always
		// doing opl3 on the player side

		// Enabling opl3 4op modes will make us go into opl3 mode
		if (header.hardware != HW_OPL3 && reg_full == 0x104 && val &&
		    (*cache)[0x105]) {
			header.hardware = HW_OPL3;
		}

		// Writing a keyon to a 2nd address enables dual opl2 otherwise
		// Maybe also check for rhythm
		if (header.hardware == HW_OPL2 && reg_full >= 0x1b0 &&
		    reg_full <= 0x1b8 && val) {
			header.hardware = HW_DUALOPL2;
		}

		uint8_t raw = to_raw[reg_mask];
		if (raw == 0xff)
			return;
		if (reg_full & 0x100)
			raw |= 128;

		AddBuf(raw, val);
	}

	void WriteCache()
	{
		// Check the registers to add
		for (uint16_t i = 0; i < 256; ++i) {
			auto val = (*cache)[i];
			// Silence the note on entries
			if (i >= 0xb0 && i <= 0xb8)
				val &= ~0x20;
			if (i == 0xbd)
				val &= ~0x1f;
			if (val)
				AddWrite(i, val);

			val = (*cache)[0x100 + i];

			if (i >= 0xb0 && i <= 0xb8)
				val &= ~0x20;
			if (val)
				AddWrite(0x100 + i, val);
		}
	}

	void InitHeader()
	{
		memset(&header, 0, sizeof(header));
		memcpy(header.id, "DBRAWOPL", 8);

		header.version_low     = 0;
		header.version_high    = 2;
		header.delay256        = delay256;
		header.delay_shift8    = delay_shift8;
		header.conv_table_size = raw_used;
	}

	void CloseFile()
	{
		if (handle) {
			ClearBuf();

			// Endianize the header and write it to beginning of the
			// file
			header.version_high = host_to_le(header.version_high);
			header.version_low  = host_to_le(header.version_low);
			header.commands     = host_to_le(header.commands);
			header.milliseconds = host_to_le(header.milliseconds);

			fseek(handle, 0, SEEK_SET);
			fwrite(&header, 1, sizeof(header), handle);
			fclose(handle);

			handle = nullptr;
		}
	}
};

Chip::Chip() : timer0(80), timer1(320) {}

bool Chip::Write(const io_port_t reg, const uint8_t val)
{
	// if(reg == 0x02 || reg == 0x03 || reg == 0x04)
	// LOG(LOG_MISC,LOG_ERROR)("write adlib timer %X %X",reg,val);
	switch (reg) {
	case 0x02:
		timer0.Update(PIC_FullIndex());
		timer0.SetCounter(val);
		return true;
	case 0x03:
		timer1.Update(PIC_FullIndex());
		timer1.SetCounter(val);
		return true;
	case 0x04:
		// Reset overflow in both timers
		if (val & 0x80) {
			timer0.Reset();
			timer1.Reset();
		} else {
			const auto time = PIC_FullIndex();
			if (val & 0x1)
				timer0.Start(time);
			else
				timer0.Stop();

			if (val & 0x2)
				timer1.Start(time);
			else
				timer1.Stop();

			timer0.SetMask((val & 0x40) > 0);
			timer1.SetMask((val & 0x20) > 0);
		}
		return true;
	}
	return false;
}

uint8_t Chip::Read()
{
	const auto time(PIC_FullIndex());
	uint8_t ret = 0;

	// Overflow won't be set if a channel is masked
	if (timer0.Update(time)) {
		ret |= 0x40;
		ret |= 0x80;
	}
	if (timer1.Update(time)) {
		ret |= 0x20;
		ret |= 0x80;
	}
	return ret;
}

void OPL::Init(const uint16_t sample_rate)
{
	newm = 0;
	OPL3_Reset(&oplchip, sample_rate);

	ms_per_frame = millis_in_second / sample_rate;

	memset(cache, 0, ARRAY_LEN(cache));

	switch (mode) {
	case Mode::Opl3: break;
	case Mode::Opl3Gold:
		adlib_gold = std::make_unique<AdlibGold>(sample_rate);
		break;
	case Mode::Opl2: break;
	case Mode::DualOpl2:
		// Setup opl3 mode in the hander
		WriteReg(0x105, 1);
		// Also set it up in the cache so the capturing will start opl3
		CacheWrite(0x105, 1);
		break;
	}
}

void OPL::WriteReg(const io_port_t selected_reg, const uint8_t val)
{
	OPL3_WriteRegBuffered(&oplchip, selected_reg, val);
	if (selected_reg == 0x105)
		newm = selected_reg & 0x01;
}

io_port_t OPL::WriteAddr(const io_port_t port, const uint8_t val)
{
	io_port_t addr = val;
	if ((port & 2) && (addr == 0x05 || newm)) {
		addr |= 0x100;
	}
	return addr;
}

AudioFrame OPL::RenderFrame()
{
	static int16_t buf[2] = {};
	OPL3_GenerateStream(&oplchip, buf, 1);

	AudioFrame frame = {};
	if (adlib_gold) {
		adlib_gold->Process(buf, 1, &frame[0]);
	} else {
		frame.left  = buf[0];
		frame.right = buf[1];
	}
	return frame;
}

void OPL::RenderUpToNow()
{
	const auto now = PIC_FullIndex();

	// Wake up the channel and update the last rendered time datum.
	if (channel->WakeUp()) {
		last_rendered_ms = now;
		return;
	}
	// Keep rendering until we're current
	while (last_rendered_ms < now) {
		last_rendered_ms += ms_per_frame;
		fifo.emplace(RenderFrame());
	}
}

void OPL::AudioCallback(const uint16_t requested_frames)
{
	assert(channel);

	//if (fifo.size())
	//	LOG_MSG("OPL: Queued %2lu cycle-accurate frames", fifo.size());

	auto frames_remaining = requested_frames;

	// First, send any frames we've queued since the last callback
	while (frames_remaining && fifo.size()) {
		channel->AddSamples_sfloat(1, &fifo.front()[0]);
		fifo.pop();
		--frames_remaining;
	}
	// If the queue's run dry, render the remainder and sync-up our time datum
	while (frames_remaining) {
		const auto frame = RenderFrame();
		channel->AddSamples_sfloat(1, &frame[0]);
		--frames_remaining;
	}
	last_rendered_ms = PIC_FullIndex();
}

void OPL::CacheWrite(const io_port_t port, const uint8_t val)
{
	// capturing?
	if (capture)
		capture->DoWrite(port, val);

	// Store it into the cache
	cache[port] = val;
}

void OPL::DualWrite(const uint8_t index, const uint8_t port, const uint8_t value)
{
	// Make sure you don't use opl3 features
	// Don't allow write to disable opl3
	if (port == 5)
		return;

	// Only allow 4 waveforms
	auto val = value;
	if (port >= 0xe0)
		val &= 3;

	// Write to the timer?
	if (chip[index].Write(port, val))
		return;

	// Enabling panning
	if (port >= 0xc0 && port <= 0xc8) {
		val &= 0x0f;
		val |= index ? 0xA0 : 0x50;
	}
	const auto full_port = check_cast<io_port_t>(port + (index ? 0x100 : 0u));
	WriteReg(full_port, val);
	CacheWrite(full_port, val);
}

void OPL::AdlibGoldControlWrite(const uint8_t val)
{
	switch (ctrl.index) {
	case 0x04:
		adlib_gold->StereoControlWrite(StereoProcessorControlReg::VolumeLeft,
		                               val);
		break;
	case 0x05:
		adlib_gold->StereoControlWrite(StereoProcessorControlReg::VolumeRight,
		                               val);
		break;
	case 0x06:
		adlib_gold->StereoControlWrite(StereoProcessorControlReg::Bass, val);
		break;
	case 0x07:
		adlib_gold->StereoControlWrite(StereoProcessorControlReg::Treble, val);
		break;

	case 0x08:
		adlib_gold->StereoControlWrite(StereoProcessorControlReg::SwitchFunctions,
		                               val);
		break;

	case 0x09: // Left FM Volume
		ctrl.lvol = val;
		goto setvol;
	case 0x0a: // Right FM Volume
		ctrl.rvol = val;
	setvol:
		if (ctrl.mixer) {
			// Dune CD version uses 32 volume steps in an apparent
			// mistake, should be 128
			channel->SetAppVolume(
			        {static_cast<float>(ctrl.lvol & 0x1f) / 31.0f,
			         static_cast<float>(ctrl.rvol & 0x1f) / 31.0f});
		}
		break;

	case 0x18: // Surround
		adlib_gold->SurroundControlWrite(val);
	}
}

uint8_t OPL::AdlibGoldControlRead()
{
	switch (ctrl.index) {
	case 0x00: // Board Options
		return 0x50; // 16-bit ISA, surround module, no
		             // telephone/CDROM
		// return 0x70; // 16-bit ISA, no
		             // telephone/surround/CD-ROM

	case 0x09: // Left FM Volume
		return ctrl.lvol;
	case 0x0a: // Right FM Volume
		return ctrl.rvol;
	case 0x15: // Audio Relocation
		return 0x388 >> 3; // Cryo installer detection
	}
	return 0xff;
}

void OPL::PortWrite(const io_port_t port, const io_val_t value, const io_width_t)
{
	RenderUpToNow();

	const auto val = check_cast<uint8_t>(value);

	if (port & 1) {
		switch (mode) {
		case Mode::Opl3Gold:
			if (port == 0x38b) {
				if (ctrl.active) {
					AdlibGoldControlWrite(val);
					break;
				}
			}
			[[fallthrough]];
		case Mode::Opl2:
		case Mode::Opl3:
			if (!chip[0].Write(reg.normal, val)) {
				WriteReg(reg.normal, val);
				CacheWrite(reg.normal, val);
			}
			break;
		case Mode::DualOpl2:
			// Not a 0x??8 port, then write to a specific port
			if (!(port & 0x8)) {
				uint8_t index = (port & 2) >> 1;
				DualWrite(index, reg.dual[index], val);
			} else {
				// Write to both ports
				DualWrite(0, reg.dual[0], val);
				DualWrite(1, reg.dual[1], val);
			}
			break;
		}
	} else {
		// Ask the handler to write the address
		// Make sure to clip them in the right range
		switch (mode) {
		case Mode::Opl2:
			reg.normal = WriteAddr(port, val) & 0xff;
			break;
		case Mode::Opl3Gold:
			if (port == 0x38a) {
				if (val == 0xff) {
					ctrl.active = true;
					break;
				} else if (val == 0xfe) {
					ctrl.active = false;
					break;
				} else if (ctrl.active) {
					ctrl.index = val & 0xff;
					break;
				}
			}
			[[fallthrough]];
		case Mode::Opl3:
			reg.normal = WriteAddr(port, val) & 0x1ff;
			break;
		case Mode::DualOpl2:
			// Not a 0x?88 port, when write to a specific side
			if (!(port & 0x8)) {
				uint8_t index   = (port & 2) >> 1;
				reg.dual[index] = val & 0xff;
			} else {
				reg.dual[0] = val & 0xff;
				reg.dual[1] = val & 0xff;
			}
			break;
		default:
			// TODO CMS and None must be removed as they're not real OPL modes
			break;
		}
	}
}

uint8_t OPL::PortRead(const io_port_t port, const io_width_t)
{
	// roughly half a micro (as we already do 1 micro on each port read and
	// some tests revealed it taking 1.5 micros to read an adlib port)
	auto delaycyc = (CPU_CycleMax / 2048);
	if (GCC_UNLIKELY(delaycyc > CPU_Cycles))
		delaycyc = CPU_Cycles;

	CPU_Cycles -= delaycyc;
	CPU_IODelayRemoved += delaycyc;

	switch (mode) {
	case Mode::Opl2:
		// We allocated 4 ports, so just return -1 for the higher ones
		if (!(port & 3))
			// Make sure the low bits are 6 on opl2
			return chip[0].Read() | 0x6;
		else
			return 0xff;

	case Mode::Opl3Gold:
		if (ctrl.active) {
			if (port == 0x38a)
				return 0; // Control status, not busy
			else if (port == 0x38b)
				return AdlibGoldControlRead();
		}
		[[fallthrough]];
	case Mode::Opl3:
		// We allocated 4 ports, so just return -1 for the higher ones
		if (!(port & 3))
			return chip[0].Read();
		else
			return 0xff;

	case Mode::DualOpl2:
		// Only return for the lower ports
		if (port & 1)
			return 0xff;
		// Make sure the low bits are 6 on opl2
		return chip[(port >> 1) & 1].Read() | 0x6;

	default:
		// TODO CMS and None must be removed as they're not real OPL modes
		break;
	}
	return 0;
}

// Save the current state of the operators as instruments in an Reality AdLib
// Tracker (RAD) file
#if 0
static void SaveRad()
{
	char b[16 * 1024];
	int w = 0;

	FILE *handle = CAPTURE_CreateFile(CaptureType::RadOplInstruments);
	if (!handle)
		return;

	// Header
	fwrite("RAD by REALiTY!!", 1, 16, handle);
	b[w++] = 0x10; // version
	b[w++] = 0x06; // default speed and no description
	               //
	// Write 18 instuments for all operators in the cache
	for (int i = 0; i < 18; i++) {
		const uint8_t *set  = opl->cache + (i / 9) * 256;
		const auto offset   = ((i % 9) / 3) * 8 + (i % 3);
		const uint8_t *base = set + offset;

		b[w++] = 1 + i; // instrument number
		b[w++] = base[0x23];
		b[w++] = base[0x20];
		b[w++] = base[0x43];
		b[w++] = base[0x40];
		b[w++] = base[0x63];
		b[w++] = base[0x60];
		b[w++] = base[0x83];
		b[w++] = base[0x80];
		b[w++] = set[0xc0 + (i % 9)];
		b[w++] = base[0xe3];
		b[w++] = base[0xe0];
	}
	b[w++] = 0; // instrument 0, no more instruments following
	b[w++] = 1; // 1 pattern following

	// Zero out the remaining part of the file a bit to make rad happy
	for (int i = 0; i < 64; i++)
		b[w++] = 0;

	fwrite(b, 1, w, handle);
	fclose(handle);
}
#endif

static void OPL_SaveRawEvent(const bool pressed)
{
	if (!pressed)
		return;
	//	SaveRad();return;

	if (!opl) {
		LOG_WARNING("OPL: Can't capture the OPL stream because the OPL device is unavailable");
		return;
	}

	// Are we already recording? If so, close the stream
	if (opl->capture) {
		opl->capture.reset();
	}
	// Otherwise start a new recording
	else {
		opl->capture = std::make_unique<Capture>(&opl->cache);
	}
}

static std::string opl_mode_to_string(const Mode mode)
{
	switch (mode) {
	case Mode::Opl2:     return "OPL2";
	case Mode::DualOpl2: return "DualOPL2";
	case Mode::Opl3:     return "OPL3";
	case Mode::Opl3Gold: return "OPL3Gold";
	}
	return "Unknown";
}

OPL::OPL(Section *configuration, const OplMode oplmode)
{
	assert(oplmode != OplMode::Cms);
	assert(oplmode != OplMode::None);

	switch (oplmode) {
	case OplMode::Opl2:     mode = Mode::Opl2;     break;
	case OplMode::DualOpl2: mode = Mode::DualOpl2; break;
	case OplMode::Opl3:     mode = Mode::Opl3;     break;
	case OplMode::Opl3Gold: mode = Mode::Opl3Gold; break;
	default: break;
	}

	Section_prop *section = static_cast<Section_prop *>(configuration);
	const auto base = static_cast<uint16_t>(section->Get_hex("sbbase"));

	ctrl.mixer = section->Get_bool("sbmixer");

	std::set channel_features = {ChannelFeature::Sleep,
	                             ChannelFeature::ReverbSend,
	                             ChannelFeature::ChorusSend,
	                             ChannelFeature::Synthesizer};

	const auto dual_opl = mode != Mode::Opl2;

	if (dual_opl)
		channel_features.emplace(ChannelFeature::Stereo);

	const auto mixer_callback = std::bind(&OPL::AudioCallback,
	                                      this,
	                                      std::placeholders::_1);

	// Register the Audio channel
	channel = MIXER_AddChannel(mixer_callback, use_mixer_rate, "OPL", channel_features);

	// Used to be 2.0, which was measured to be too high. Exact value
	// depends on card/clone.
	//
	// Please don't touch this value *EVER* again as many people fine-tune
	// their mixer volumes per game, so changing this would break their
	// settings. The value cannot be "improved"; there's simply no
	// universally "good" setting that would work well in all games in
	// existence.
	constexpr auto opl_volume_scale_factor = 1.5f;
	channel->Set0dbScalar(opl_volume_scale_factor);

	Init(check_cast<uint16_t>(channel->GetSampleRate()));

	using namespace std::placeholders;

	const auto read_from = std::bind(&OPL::PortRead, this, _1, _2);
	const auto write_to  = std::bind(&OPL::PortWrite, this, _1, _2, _3);

	// 0x388-0x38b ports (read/write)
	constexpr io_port_t port_0x388 = 0x388;
	WriteHandler[0].Install(port_0x388, write_to, io_width_t::byte, 4);
	ReadHandler[0].Install(port_0x388, read_from, io_width_t::byte, 4);

	// 0x220-0x223 ports (read/write)
	if (dual_opl) {
		WriteHandler[1].Install(base, write_to, io_width_t::byte, 4);
		ReadHandler[1].Install(base, read_from, io_width_t::byte, 4);
	}
	// 0x228-0x229 ports (write)
	WriteHandler[2].Install(base + 8u, write_to, io_width_t::byte, 2);

	// 0x228 port (read)
	ReadHandler[2].Install(base + 8u, read_from, io_width_t::byte, 1);

	MAPPER_AddHandler(OPL_SaveRawEvent, SDL_SCANCODE_UNKNOWN, 0, "caprawopl", "Rec. OPL");

	LOG_MSG("OPL: Running %s on ports %xh and %xh",
	        opl_mode_to_string(mode).c_str(),
	        base,
	        port_0x388);
}

OPL::~OPL()
{
	LOG_MSG("OPL: Shutting down %s", opl_mode_to_string(mode).c_str());

	// Stop playback
	if (channel) {
		channel->Enable(false);
	}

	// Stop the game from accessing the IO ports
	for (auto& rh : ReadHandler) {
		rh.Uninstall();
	}
	for (auto& wh : WriteHandler) {
		wh.Uninstall();
	}

	// Deregister the mixer channel, after which it's cleaned up
	assert(channel);
	MIXER_DeregisterChannel(channel);
}

void OPL_ShutDown([[maybe_unused]] Section* sec)
{
	opl = {};
}

void OPL_Init(Section* sec, const OplMode oplmode)
{
	assert(sec);
	opl = std::make_unique<OPL>(sec, oplmode);

	constexpr auto changeable_at_runtime = true;
	sec->AddDestroyFunction(&OPL_ShutDown, changeable_at_runtime);
}
