// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_STEREO_ON_1_H
#define DOSBOX_STEREO_ON_1_H

#include "dosbox.h"

#include "channel_names.h"
#include "inout.h"
#include "lpt_dac.h"
#include "mixer.h"

class StereoOn1 final : public LptDac {
public:
	StereoOn1()
	        : LptDac(ChannelName::StereoOn1Dac, SampleRateHz,
	                 {ChannelFeature::Stereo})
	{}

	void BindToPort(const io_port_t lpt_port) final;
	void ConfigureFilters(const FilterState state) final;

protected:
	AudioFrame Render() final;
	void WriteData(const io_port_t, const io_val_t value, const io_width_t);
	uint8_t ReadStatus(const io_port_t, const io_width_t);
	void WriteControl(const io_port_t, const io_val_t value, const io_width_t);

private:
	static constexpr auto SampleRateHz = 30000;

	uint8_t stereo_data[2] = {data_reg, data_reg};
};

#endif
