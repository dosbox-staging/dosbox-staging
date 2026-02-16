// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2017-2020 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MAME_EMU_H
#define DOSBOX_MAME_EMU_H

#include "dosbox.h"

#include <cmath>
#if defined(_MSC_VER) && (_MSC_VER  <= 1500) 
#include <SDL.h>
#else
#include <stdint.h>
#endif
#include <float.h>
#include <stdlib.h>
#include <memory>

#if C_DEBUGGER
#include <stdio.h>
#include <stdarg.h>
#endif

typedef int16_t stream_sample_t;

typedef uint8_t u8;
typedef uint32_t u32;

class device_t;

#define NAME( _ASDF_ ) 0
#define BIT( _INPUT_, _BIT_ ) ( ( _INPUT_) >> (_BIT_)) & 1

#define ATTR_UNUSED
#define DECLARE_READ8_MEMBER(name)      u8     name( int, int)
#define DECLARE_WRITE8_MEMBER(name)     void   name( int, int, u8 data)
#define READ8_MEMBER(name)              u8     name( int, int)
#define WRITE8_MEMBER(name)				void   name([[maybe_unused]] int offset, [[maybe_unused]] int space, [[maybe_unused]] u8 data)

class device_sound_interface {
public:
	struct sound_stream {
		void update() {}
	};

	sound_stream temp = {};

	virtual ~device_sound_interface() = default;

	sound_stream *stream_alloc([[maybe_unused]] int whatever, [[maybe_unused]] int channels, [[maybe_unused]] int size)
	{
		return &temp;
	}

	virtual void sound_stream_update(sound_stream &stream,
	                                 stream_sample_t **inputs,
	                                 stream_sample_t **outputs,
	                                 int samples) = 0;
};

struct attotime {
	int whatever;

	static attotime from_hz([[maybe_unused]] int hz) {
		return attotime();
	}
};

typedef int device_type;

class device_t {
	u32 clockRate = 0;
public:
	const char *shortName = nullptr;
	struct machine_t {
		int describe_context() const {
			return 0;
		}
	};

	machine_t machine() const {
		return machine_t();
	}

	//int offset, space;

	u32 clock() const {
		return clockRate;
	}

	void logerror([[maybe_unused]] const char* format, ...) {
#if C_DEBUGGER
		char buf[512*2];
		va_list msg;
		va_start(msg,format);
		vsprintf(buf,format,msg);
		va_end(msg);
		LOG(LOG_MISC,LOG_NORMAL)("%s",buf);
#endif
	}

	static int tag() {
		return 0;
	}

	virtual void device_start() {
	}

	void save_item(int, [[maybe_unused]] int blah= 0) {
	}

	device_t(const char *short_name,
	         [[maybe_unused]] device_t *owner,
	         u32 _clock)
	        : clockRate(_clock),
	          shortName(short_name)
	{}

	virtual ~device_t() = default;

	// prevent copying and assignment
	device_t(const device_t &) = delete;
	device_t &operator=(const device_t &) = delete;
};

#define auto_alloc_array_clear(m, t, c) calloc(c, sizeof(t) )
#define auto_alloc_clear(m, t) static_cast<t*>( calloc(1, sizeof(t) ) )

#endif
