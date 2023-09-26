/*
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
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
	        : LptDac(ChannelName::StereoOn1Dac, ston1_max_30_khz,
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
	static constexpr uint16_t ston1_max_30_khz = 30000u;

	uint8_t stereo_data[2] = {data_reg, data_reg};
};

#endif
