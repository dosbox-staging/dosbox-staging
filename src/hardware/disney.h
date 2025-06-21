// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

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
	static constexpr auto DisneySampleRateHz = 7000;
	static constexpr auto MaxFifoSize        = 16;

	// Managed objects
	std::queue<uint8_t> fifo = {};
};

#endif
