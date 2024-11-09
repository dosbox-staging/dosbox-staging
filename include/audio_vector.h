/*
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#ifndef AUDIO_VECTOR_H
#define AUDIO_VECTOR_H

#include <cstdint>
#include <vector>

#include "config.h"
#include "mixer.h"

// The producer constructs the object and the consumer calls AddSamples()

// Currently only used by SoundBlaster
// Think before using this for other audio devices
// Usage of this class causes more heap allocations than adding samples directly to an RWQueue
// SoundBlaster needs this because it can switch between different formats at runtime

// Suitable where incoming samples are guaranteed to be little-endian,
// such as DOS 16-bit DMA (SB and GUS), as well as raw bin+cue CDDA.
// (Not suitable for synths that output 16-bit samples or the audio
//  codecs, which produce host-endian samples)

class AudioVector
{
public:
	AudioVector(int _num_frames) : num_frames(_num_frames) {}
	virtual ~AudioVector() = default;
	virtual void AddSamples(MixerChannel* channel) = 0;
	int num_frames = 0;
};

class AudioVectorM8 final : public AudioVector
{
public:
	AudioVectorM8(int _num_frames, const uint8_t* _data)
		: AudioVector(_num_frames),
		  data(_data, _data + _num_frames) {}

	void AddSamples(MixerChannel* channel) final
	{
		channel->AddSamples_m8(num_frames, data.data());
	}
private:
	std::vector<uint8_t> data = {};
};

class AudioVectorM8S final : public AudioVector
{
public:
	AudioVectorM8S(int _num_frames, const int8_t* _data)
		: AudioVector(_num_frames),
		  data(_data, _data + _num_frames) {}

	void AddSamples(MixerChannel* channel) final
	{
		channel->AddSamples_m8s(num_frames, data.data());
	}
private:
	std::vector<int8_t> data = {};
};

class AudioVectorS8 final : public AudioVector
{
public:
	AudioVectorS8(int _num_frames, const uint8_t* _data)
		: AudioVector(_num_frames),
		  data(_data, _data + (_num_frames * 2)) {}

	void AddSamples(MixerChannel* channel) final
	{
		channel->AddSamples_s8(num_frames, data.data());
	}
private:
	std::vector<uint8_t> data = {};
};

class AudioVectorS8S final : public AudioVector
{
public:
	AudioVectorS8S(int _num_frames, const int8_t* _data)
		: AudioVector(_num_frames),
		  data(_data, _data + (_num_frames * 2)) {}

	void AddSamples(MixerChannel* channel) final
	{
		channel->AddSamples_s8s(num_frames, data.data());
	}
private:
	std::vector<int8_t> data = {};
};

class AudioVectorS16 final : public AudioVector
{
public:
	AudioVectorS16(int _num_frames, const int16_t* _data)
		: AudioVector(_num_frames),
		  data(_data, _data + (_num_frames * 2)) {}

	void AddSamples(MixerChannel* channel) final
	{
		#if defined(WORDS_BIGENDIAN)
		channel->AddSamples_s16_nonnative(num_frames, data.data());
		#else
		channel->AddSamples_s16(num_frames, data.data());
		#endif
	}
private:
	std::vector<int16_t> data = {};
};

class AudioVectorS16U final : public AudioVector
{
public:
	AudioVectorS16U(int _num_frames, const uint16_t* _data)
		: AudioVector(_num_frames),
		  data(_data, _data + (_num_frames * 2)) {}

	void AddSamples(MixerChannel* channel) final
	{
		#if defined(WORDS_BIGENDIAN)
		channel->AddSamples_s16u_nonnative(num_frames, data.data());
		#else
		channel->AddSamples_s16u(num_frames, data.data());
		#endif
	}
private:
	std::vector<uint16_t> data = {};
};

class AudioVectorM16 final : public AudioVector
{
public:
	AudioVectorM16(int _num_frames, const int16_t* _data)
		: AudioVector(_num_frames),
		  data(_data, _data + _num_frames) {}

	void AddSamples(MixerChannel* channel) final
	{
		#if defined(WORDS_BIGENDIAN)
		channel->AddSamples_m16_nonnative(num_frames, data.data());
		#else
		channel->AddSamples_m16(num_frames, data.data());
		#endif
	}
private:
	std::vector<int16_t> data = {};
};

class AudioVectorM16U final : public AudioVector
{
public:
	AudioVectorM16U(int _num_frames, const uint16_t* _data)
		: AudioVector(_num_frames),
		  data(_data, _data + _num_frames) {}

	void AddSamples(MixerChannel* channel) final
	{
		#if defined(WORDS_BIGENDIAN)
		channel->AddSamples_m16u_nonnative(num_frames, data.data());
		#else
		channel->AddSamples_m16u(num_frames, data.data());
		#endif
	}
private:
	std::vector<uint16_t> data = {};
};

#endif
