// SPDX-License-Identifier: BSD-3-Clause
//
// copyright-holders:Juergen Buchmueller, Manuel Abadia

/**********************************************
    Philips SAA1099 Sound driver
**********************************************/

#ifndef MAME_SOUND_SAA1099_H
#define MAME_SOUND_SAA1099_H

#include "emu.h"

//**************************************************************************
//  INTERFACE CONFIGURATION MACROS
//**************************************************************************

#define MCFG_SAA1099_ADD(_tag, _clock) \
	MCFG_DEVICE_ADD(_tag, SAA1099, _clock)
#define MCFG_SAA1099_REPLACE(_tag, _clock) \
	MCFG_DEVICE_REPLACE(_tag, SAA1099, _clock)


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> saa1099_device

class saa1099_device final : public device_t, public device_sound_interface {
public:
	saa1099_device(const char *tag, device_t *owner,
	               const uint32_t clock, const int rate_divisor);

	saa1099_device(const saa1099_device &) = delete; // prevent copying
	saa1099_device &operator=(const saa1099_device &) = delete; // prevent assignment

	DECLARE_WRITE8_MEMBER( control_w );
	DECLARE_WRITE8_MEMBER( data_w );

	DECLARE_WRITE8_MEMBER( write );

	// protected:
	//  device-level overrides
	void device_start() override;

	// sound stream update overrides
	void sound_stream_update(sound_stream& stream, stream_sample_t** inputs,
	                         stream_sample_t** outputs, int samples) override;

private:
	struct saa1099_channel {
		saa1099_channel() = default;
		uint16_t amplitude[2] = {0, 0}; // amplitude (0x00..0x0f)
		uint8_t envelope[2] = {0, 0}; // envelope (0x00..0x0f or 0x10==off)
		uint8_t octave = 0;           // octave (0x00..0x07)
		uint8_t frequency = 0;        // frequency (0x00..0xff)
		bool freq_enable = 0;         // frequency enable
		bool noise_enable = 0;        // noise enable

		/* vars to simulate the square wave */
		void update_freq(int chip_clock); // based on the octave and
		                                  // frequency registers
		double counter = 0;
		double freq = 0;
		int level = 0;
	};

	struct saa1099_noise {
		saa1099_noise() = default;

		/* vars to simulate the noise generator output */
		double counter = 0;
		double freq = 0;
		uint32_t level = 0xFFFFFFFF; // noise polynomial shifter
	};

	void envelope_w(int ch);

	sound_stream *m_stream;           /* our stream */
	const double m_noise_freqs[3];    /* noise frequencies based on chip-clock */
	int m_noise_params[2];            /* noise generators parameters */
	int m_env_enable[2];              /* envelope generators enable */
	int m_env_reverse_right[2];       /* envelope reversed for right channel */
	int m_env_mode[2];                /* envelope generators mode */
	int m_env_bits[2];                /* non zero = 3 bits resolution */
	int m_env_clock[2];               /* envelope clock mode (non-zero external) */
	int m_env_step[2];                /* current envelope step */
	int m_all_ch_enable;              /* all channels enable */
	int m_sync_state;                 /* sync all channels */
	int m_selected_reg;               /* selected register */
	saa1099_channel m_channels[6];    /* channels */
	saa1099_noise m_noise[2];         /* noise generators */
	double m_sample_rate;
	int m_chip_clock;
};

#endif // MAME_SOUND_SAA1099_H
