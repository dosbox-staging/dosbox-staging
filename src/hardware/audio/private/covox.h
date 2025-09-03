// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_COVOX_H
#define DOSBOX_COVOX_H

#include "dosbox.h"

#include "lpt_dac.h"

#include "audio/channel_names.h"
#include "audio/mixer.h"
#include "hardware/port.h"

class Covox final : public LptDac {
public:
	Covox() : LptDac(ChannelName::CovoxDac, UseMixerRate) {}
	void BindToPort(const io_port_t lpt_port) override;
	void ConfigureFilters(const FilterState state) override;

protected:
	AudioFrame Render() override;

private:
	void WriteData(const io_port_t, const io_val_t value, const io_width_t);
	uint8_t ReadStatus(const io_port_t, const io_width_t);
	void WriteControl(const io_port_t, const io_val_t, const io_width_t) {}
};

#endif // DOSBOX_COVOX_H
