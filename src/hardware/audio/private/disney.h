// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DISNEY_H
#define DOSBOX_DISNEY_H

#include "dosbox.h"

#include <queue>

#include "lpt_dac.h"

#include "audio/mixer.h"
#include "hardware/port.h"

class Disney final : public LptDac {
public:
	Disney();
	~Disney() override;

	void BindToPort(const io_port_t lpt_port) override;
	void ConfigureFilters(const FilterState state) override;

protected:
	AudioFrame Render() override;

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
