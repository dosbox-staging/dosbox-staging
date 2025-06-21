// SPDX-FileSPDText:X Identifier: GPL-2.0-or-later
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_LPT_DAC_H
#define DOSBOX_LPT_DAC_H

#include "dosbox.h"

#include <queue>
#include <set>
#include <string_view>

#include "inout.h"
#include "lpt.h"
#include "mixer.h"
#include "rwqueue.h"

// Provides mandatory scafolding for derived LPT DAC devices
class LptDac {
public:
	LptDac(const std::string_view name, const int channel_rate_hz,
	       std::set<ChannelFeature> extra_features = {});

	virtual ~LptDac();

	// public interfaces
	virtual void ConfigureFilters(const FilterState state) = 0;
	virtual void BindToPort(const io_port_t lpt_port)      = 0;

	bool TryParseAndSetCustomFilter(const std::string& filter_choice);
	void PicCallback(const int requested_frames);

	LptDac() = delete;

	// prevent copying
	LptDac(const LptDac&) = delete;

	// prevent assignment
	LptDac& operator=(const LptDac&) = delete;

	RWQueue<AudioFrame> output_queue{1};
	MixerChannelPtr channel = {};
	float frame_counter = 0.0f;

protected:
	// Base LPT DAC functionality
	virtual AudioFrame Render() = 0;
	void RenderUpToNow();

	double last_rendered_ms = 0.0;
	double ms_per_frame     = 0.0;

	std::string dac_name = {};

	// All LPT devices support data write, status read, and control write
	void BindHandlers(const io_port_t lpt_port, const io_write_f write_data,
	                  const io_read_f read_status,
	                  const io_write_f write_control);

	IO_WriteHandleObject data_write_handler    = {};
	IO_ReadHandleObject status_read_handler    = {};
	IO_WriteHandleObject control_write_handler = {};

	uint8_t data_reg = Mixer_GetSilentDOSSample<uint8_t>();

	LptStatusRegister status_reg   = {};
	LptControlRegister control_reg = {};

private:
	int frames_rendered_this_tick = 0;
};


#endif
