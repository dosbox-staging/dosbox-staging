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

#ifndef DOSBOX_DISNEY_H
#define DOSBOX_DISNEY_H

#include "dosbox.h"

#include <queue>

#include "inout.h"
#include "lpt_dac.h"
#include "mixer.h"

class Disney final : public LptDac {
public:
	Disney();
	~Disney() final;

	void BindToPort(const io_port_t lpt_port) final;
	void ConfigureFilters(const FilterState state) final;

protected:
	AudioFrame Render() final;

private:
	bool IsFifoFull() const;
	void WriteData(const io_port_t, const io_val_t value, const io_width_t);
	uint8_t ReadStatus(const io_port_t, const io_width_t);
	void WriteControl(const io_port_t, const io_val_t value, const io_width_t);

	// The DSS is an LPT DAC with a 16-level FIFO running at 7kHz
	static constexpr auto dss_7khz_rate = 7000;
	static constexpr uint8_t max_fifo_size = 16;

	// Managed objects
	std::queue<uint8_t> fifo = {};
};

#endif
