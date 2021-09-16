/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *  Copyright (C) 2017-2020  The DOSBox Team
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
#include <memory.h>

#if C_DEBUG
#include <stdio.h>
#include <stdarg.h>
#endif

typedef Bit16s stream_sample_t;

typedef Bit8u u8;
typedef Bit32u u32;

class device_t;
struct machine_config;

#define NAME( _ASDF_ ) 0
#define BIT( _INPUT_, _BIT_ ) ( ( _INPUT_) >> (_BIT_)) & 1

#define ATTR_UNUSED
#define DECLARE_READ8_MEMBER(name)      u8     name( int, int)
#define DECLARE_WRITE8_MEMBER(name)     void   name( int, int, u8 data)
#define READ8_MEMBER(name)              u8     name( int, int)
#define WRITE8_MEMBER(name)				void   name(MAYBE_UNUSED int offset, MAYBE_UNUSED int space, MAYBE_UNUSED u8 data)

#define DECLARE_DEVICE_TYPE(Type, Class) \
		extern const device_type Type; \
		class Class;

#define DEFINE_DEVICE_TYPE(Type, Class, ShortName, FullName)		\
	const device_type Type = 0;

class device_sound_interface {
public:
	struct sound_stream {
		void update() {}
	};

	sound_stream temp;

	device_sound_interface(const machine_config & /* mconfig */, MAYBE_UNUSED device_t &_device)
	        : temp()
	{}

	virtual ~device_sound_interface() = default;

	sound_stream *stream_alloc(MAYBE_UNUSED int whatever, MAYBE_UNUSED int channels, MAYBE_UNUSED int size)
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

	static attotime from_hz(MAYBE_UNUSED int hz) {
		return attotime();
	}
};

struct machine_config {
};

typedef int device_type;

class device_t {
	u32 clockRate;
public:
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

	void logerror(MAYBE_UNUSED const char* format, ...) {
#if C_DEBUG
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

	void save_item(int, MAYBE_UNUSED int blah= 0) {
	}

	device_t(const machine_config & /* mconfig */, MAYBE_UNUSED device_type type, MAYBE_UNUSED const char *tag, MAYBE_UNUSED device_t *owner, u32 _clock) : clockRate( _clock ) {
	}

	virtual ~device_t() {
	}
};

#define auto_alloc_array_clear(m, t, c) calloc(c, sizeof(t) )
#define auto_alloc_clear(m, t) static_cast<t*>( calloc(1, sizeof(t) ) )

#endif
