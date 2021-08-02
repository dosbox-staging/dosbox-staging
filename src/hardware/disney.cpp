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

#include <cassert>
#include <memory>
#include <string.h>

#include "inout.h"
#include "mixer.h"
#include "pic.h"
#include "setup.h"

// Disney Sound Source Constants
constexpr uint16_t DISNEY_BASE = 0x0378;
constexpr uint8_t BUFFER_SAMPLES = 128;
constexpr uint8_t DISNEY_INIT_STATUS = 0x84;

enum DISNEY_STATE { IDLE, RUNNING, FINISHED, ANALYZING };

struct dac_channel {
	uint8_t buffer[BUFFER_SAMPLES] = {};
	uint8_t used = 0; // current data buffer level
	double speedcheck_sum = 0.0;
	double speedcheck_last = 0.0;
	bool speedcheck_failed = false;
	bool speedcheck_init = false;
};

using mixer_channel_ptr_t = std::unique_ptr<MixerChannel, decltype(&MIXER_DelChannel)>;

struct Disney {
	IO_ReadHandleObject read_handler{};
	IO_WriteHandleObject write_handler{};

	// parallel port stuff
	uint8_t data = 0;
	uint8_t status = DISNEY_INIT_STATUS;
	uint8_t control = 0;
	// the D/A channels
	dac_channel da[2] = {};

	Bitu last_used = 0;
	mixer_channel_ptr_t chan{nullptr, MIXER_DelChannel};
	bool stereo = false;

	// For mono-output, the analysis step points the leader to the channel
	// with the most rendered-samples. We use the left channel as a valid
	// place-holder prior to the first analysis. This ensures the
	// leader is always valid, such as in cases where the callback in
	// enabled before the analysis.
	dac_channel *leader = &da[0];

	DISNEY_STATE state = DISNEY_STATE::IDLE;
	uint32_t interface_det = 0;
	uint32_t interface_det_ext = 0;
};

static Disney disney;

static void DISNEY_disable(uint32_t)
{
	if (disney.chan) {
		disney.chan->AddSilence();
		disney.chan->Enable(false);
	}
	disney.last_used = 0;
	disney.state = DISNEY_STATE::IDLE;
	disney.interface_det = 0;
	disney.interface_det_ext = 0;
	disney.stereo = false;
}

static void DISNEY_enable(uint32_t freq)
{
	if (freq < 500 || freq > 100000) {
		// try again..
		disney.state = DISNEY_STATE::IDLE;
		return;
	}
#if 0
	LOG(LOG_MISC,LOG_NORMAL)("DISNEY: enabled at %d Hz in %s", freq,
	                         disney.stereo ? "stereo" : "mono");
#endif
	disney.chan->SetFreq(freq);
	disney.chan->Enable(true);
	disney.state = DISNEY_STATE::RUNNING;
}

// Calculate the frequency from DAC samples and speed parameters
// The maximum possible frequency return is 127000 Hz, which
// occurs when all 128 samples are used and the speedcheck_sum
// has accumulated only one single tick.
static uint32_t calc_frequency(const dac_channel &dac)
{
	if (dac.used <= 1)
		return 0;
	const uint32_t k_samples = 1000 * (dac.used - 1);
	const auto frequency = k_samples / dac.speedcheck_sum;
	return static_cast<uint32_t>(frequency);
}

static void DISNEY_analyze(Bitu channel){
	switch (disney.state) {
	case DISNEY_STATE::RUNNING: // should not get here
		break;
	case DISNEY_STATE::IDLE:
		// initialize channel data
		for (int i = 0; i < 2; i++) {
			disney.da[i].used = 0;
			disney.da[i].speedcheck_sum = 0;
			disney.da[i].speedcheck_failed = false;
			disney.da[i].speedcheck_init = false;
		}
		disney.da[channel].speedcheck_last = PIC_FullIndex();
		disney.da[channel].speedcheck_init = true;

		disney.state = DISNEY_STATE::ANALYZING;
		break;

	case DISNEY_STATE::FINISHED: {
		// The leading channel has the most populated samples
		disney.leader = disney.da[0].used > disney.da[1].used
		                        ? &disney.da[0]
		                        : &disney.da[1];

		// Stereo-mode if both DACs are similarly filled
		const auto st_diff = abs(disney.da[0].used - disney.da[1].used);
		disney.stereo = (st_diff < 5);

		// Run with the greater DAC frequency
		const auto max_freq = std::max(calc_frequency(disney.da[0]),
		                               calc_frequency(disney.da[1]));
		DISNEY_enable(max_freq);
	} break;

	case DISNEY_STATE::ANALYZING: {
		const auto current = PIC_FullIndex();
		dac_channel *cch = &disney.da[channel];

		if (!cch->speedcheck_init) {
			cch->speedcheck_init = true;
			cch->speedcheck_last = current;
			break;
		}
		cch->speedcheck_sum += current - cch->speedcheck_last;
		// LOG_MSG("t=%f",current - cch->speedcheck_last);

		// sanity checks (printer...)
		if ((current - cch->speedcheck_last) < 0.01f || (current - cch->speedcheck_last) > 2)
			cch->speedcheck_failed = true;

		// if both are failed we are back at start
		if (disney.da[0].speedcheck_failed && disney.da[1].speedcheck_failed) {
			disney.state = DISNEY_STATE::IDLE;
			break;
		}

		cch->speedcheck_last = current;

		// analyze finish condition
		if (disney.da[0].used > 30 || disney.da[1].used > 30)
			disney.state = DISNEY_STATE::FINISHED;
	} break;
	}
}

static void disney_write(io_port_t port, uint8_t val, io_width_t)
{
	// LOG_MSG("write disney time %f addr%x val %x",PIC_FullIndex(),port,val);
	disney.last_used = PIC_Ticks;
	switch (port - DISNEY_BASE) {
	case 0: /* Data Port */
	{
		disney.data=val;
		// if data is written here too often without using the stereo
		// mechanism we use the simple DAC machanism.
		if (disney.state != DISNEY_STATE::RUNNING) {
			disney.interface_det++;
			if(disney.interface_det > 5)
				DISNEY_analyze(0);
		}
		if (disney.interface_det > 5) {
			if (disney.da[0].used < BUFFER_SAMPLES) {
				disney.da[0].buffer[disney.da[0].used] = disney.data;
				disney.da[0].used++;
			} // else LOG_MSG("disney overflow 0");
		}
		break;
	}
	case 1:		/* Status Port */
		LOG(LOG_MISC, LOG_NORMAL)("DISNEY:Status write %u", val);
		break;
	case 2:		/* Control Port */
		if ((disney.control & 0x2) && !(val & 0x2)) {
			if (disney.state != DISNEY_STATE::RUNNING) {
				disney.interface_det = 0;
				disney.interface_det_ext = 0;
				DISNEY_analyze(1);
			}

			// stereo channel latch
			if (disney.da[1].used < BUFFER_SAMPLES) {
				disney.da[1].buffer[disney.da[1].used] = disney.data;
				disney.da[1].used++;
			} // else LOG_MSG("disney overflow 1");
		}

		if ((disney.control & 0x1) && !(val & 0x1)) {
			if (disney.state != DISNEY_STATE::RUNNING) {
				disney.interface_det = 0;
				disney.interface_det_ext = 0;
				DISNEY_analyze(0);
			}
			// stereo channel latch
			if (disney.da[0].used < BUFFER_SAMPLES) {
				disney.da[0].buffer[disney.da[0].used] = disney.data;
				disney.da[0].used++;
			} // else LOG_MSG("disney overflow 0");
		}

		if ((disney.control & 0x8) && !(val & 0x8)) {
			// emulate a device with 16-byte sound FIFO
			if (disney.state != DISNEY_STATE::RUNNING) {
				disney.interface_det_ext++;
				disney.interface_det = 0;
				if(disney.interface_det_ext > 5) {
					disney.leader = &disney.da[0];
					DISNEY_enable(7000);
				}
			}
			if (disney.interface_det_ext > 5) {
				if (disney.da[0].used < BUFFER_SAMPLES) {
					disney.da[0].buffer[disney.da[0].used] = disney.data;
					disney.da[0].used++;
				}
			}
		}

//		LOG_WARN("DISNEY:Control write %x",val);
		if (val&0x10) LOG(LOG_MISC,LOG_ERROR)("DISNEY:Parallel IRQ Enabled");
		disney.control=val;
		break;
	}
}

static uint8_t disney_read(io_port_t port, io_width_t)
{
	uint8_t retval;
	switch (port - DISNEY_BASE) {
	case 0: /* Data Port */
		// LOG(LOG_MISC,LOG_NORMAL)("DISNEY:Read from data port");
		return disney.data;
		break;
	case 1:		/* Status Port */
//		LOG(LOG_MISC,"DISNEY:Read from status port %X",disney.status);
		retval = 0x07;//0x40; // Stereo-on-1 and (or) New-Stereo DACs present
		if (disney.interface_det_ext > 5) {
			if (disney.leader && disney.leader->used >= 16){
				retval |= 0x40; // ack
				retval &= ~0x4; // interrupt
			}
		}
		if (!(disney.data&0x80)) retval |= 0x80; // pin 9 is wired to pin 11
		return retval;
		break;
	case 2:		/* Control Port */
		LOG(LOG_MISC,LOG_NORMAL)("DISNEY:Read from control port");
		return disney.control;
		break;
	}
	return 0xff;
}

static void DISNEY_PlayStereo(uint16_t len, const uint8_t *l, const uint8_t *r)
{
	static uint8_t stereodata[BUFFER_SAMPLES * 2];
	for (uint16_t i = 0; i < len; ++i) {
		stereodata[i*2] = l[i];
		stereodata[i*2+1] = r[i];
	}
	disney.chan->AddSamples_s8(len,stereodata);
}

static void DISNEY_CallBack(uint16_t len) {
	if (!len || !disney.chan)
		return;

	// get the smaller used
	Bitu real_used;
	if (disney.stereo) {
		real_used = disney.da[0].used;
		if(disney.da[1].used < real_used) real_used = disney.da[1].used;
	} else
		real_used = disney.leader->used;

	if (real_used >= len) { // enough data for now
		if (disney.stereo) DISNEY_PlayStereo(len, disney.da[0].buffer, disney.da[1].buffer);
		else disney.chan->AddSamples_m8(len,disney.leader->buffer);

		// put the rest back to start
		for (uint8_t i = 0; i < 2; ++i) {
			// TODO for mono only one
			memmove(disney.da[i].buffer, &disney.da[i].buffer[len],
			        BUFFER_SAMPLES /*real_used*/ - len);
			disney.da[i].used -= len;
		}
	// TODO: len > DISNEY
	} else { // not enough data
		if(disney.stereo) {
			uint8_t gapfiller0 = 128;
			uint8_t gapfiller1 = 128;
			if (real_used) {
				gapfiller0 = disney.da[0].buffer[real_used-1];
				gapfiller1 = disney.da[1].buffer[real_used-1];
			};

			memset(disney.da[0].buffer+real_used,
				gapfiller0,len-real_used);
			memset(disney.da[1].buffer+real_used,
				gapfiller1,len-real_used);

			DISNEY_PlayStereo(len, disney.da[0].buffer, disney.da[1].buffer);
			len -= real_used;

		} else { // mono
			uint8_t gapfiller = 128; // Keep the middle
			if (real_used) {
				// fix for some stupid game; it outputs 0 at the end of the stream
				// causing a click. So if we have at least two bytes availible in the
				// buffer and the last one is a 0 then ignore that.
				if(disney.leader->buffer[real_used-1]==0)
					real_used--;
			}
			// do it this way because AddSilence sounds like a gnawing mouse
			if (real_used)
				gapfiller = disney.leader->buffer[real_used-1];
			//LOG_MSG("gapfiller %x, fill len %d, realused %d",gapfiller,len-real_used,real_used);
			memset(disney.leader->buffer+real_used,	gapfiller, len-real_used);
			disney.chan->AddSamples_m8(len, disney.leader->buffer);
		}
		disney.da[0].used =0;
		disney.da[1].used =0;

		//LOG_MSG("disney underflow %d",len - real_used);
	}
	if (disney.last_used+100<PIC_Ticks) {
		// disable sound output
		PIC_AddEvent(DISNEY_disable, 0.0001); // I think we shouldn't delete the
		                                      // mixer while we are inside it
	}
}

static void DISNEY_ShutDown(MAYBE_UNUSED Section *sec)
{
	DEBUG_LOG_MSG("DISNEY: Shutting down");

	// Remove interrupt events
	PIC_RemoveEvents(DISNEY_disable);

	// Stop the game from accessing the IO ports
	disney.read_handler.Uninstall();
	disney.write_handler.Uninstall();

	// Stop and remove the mixer callback
	if (disney.chan) {
		disney.chan->Enable(false);
		disney.chan.reset();
	}

	DISNEY_disable(0);
}

void DISNEY_Init(Section* sec) {
	Section_prop *section = static_cast<Section_prop *>(sec);
	assert(section);
	if (!section->Get_bool("disney"))
		return;

	// Setup the mixer callback
	disney.chan = mixer_channel_ptr_t(MIXER_AddChannel(DISNEY_CallBack,
	                                                   10000, "DISNEY"),
	                                  MIXER_DelChannel);
	assert(disney.chan);

	// Register port handlers for 8-bit IO
	disney.write_handler.Install(DISNEY_BASE, disney_write, io_width_t::byte, 3);
	disney.read_handler.Install(DISNEY_BASE, disney_read, io_width_t::byte, 3);

	// Initialize the Disney states
	disney.status = DISNEY_INIT_STATUS;
	disney.control = 0;
	disney.last_used = 0;
	DISNEY_disable(0);

	sec->AddDestroyFunction(&DISNEY_ShutDown, true);
}
