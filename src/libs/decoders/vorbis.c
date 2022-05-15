/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
 *  Copyright (C) 2018-2021  kcgen <kcgen@users.noreply.github.com>
 *  Copyright (C) 2001-2017  Ryan C. Gordon <icculus@icculus.org>
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

/*
 *  DOSBox Vorbis decoder API implementation
 *  -------------------------------------
 *  It makes use of the stand-alone STB Vorbis library:
 *   - STB: https://github.com/nothings/stb (source)
 *   - STB: https://twitter.com/nothings (website/author info)
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef memcpy
#  undef memcpy
#endif

#include <string.h> /* memcpy */
#include <math.h> /* lroundf */

#include "SDL_sound.h"
#define __SDL_SOUND_INTERNAL__
#include "SDL_sound_internal.h"

#ifdef asset
#  undef assert
#  define assert SDL_assert
#endif

#ifdef memset
#  undef memset
#  define memset SDL_memset
#endif

#define free         SDL_free
#define qsort        SDL_qsort
#define memcmp       SDL_memcmp
#define malloc       SDL_malloc
#define realloc      SDL_realloc
#define dealloca(x)  SDL_stack_free((x))

/* Configure and include stb_vorbis for compiling... */
#define STB_VORBIS_NO_STDIO 1
#define STB_VORBIS_NO_CRT 1
#define STB_VORBIS_NO_PUSHDATA_API 1
#define STB_VORBIS_MAX_CHANNELS 2
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define STB_VORBIS_BIG_ENDIAN 1
#endif

#include "stb_vorbis.h"

#ifdef DEBUG_CHATTER
static const char *vorbis_error_string(const int err)
{
    switch (err)
    {
        case VORBIS__no_error: return NULL;
        case VORBIS_need_more_data: return "VORBIS: need more data";
        case VORBIS_invalid_api_mixing: return "VORBIS: can't mix API modes";
        case VORBIS_outofmem: return "VORBIS: out of memory";
        case VORBIS_feature_not_supported: return "VORBIS: feature not supported";
        case VORBIS_too_many_channels: return "VORBIS: too many channels";
        case VORBIS_file_open_failure: return "VORBIS: failed opening the file";
        case VORBIS_seek_without_length: return "VORBIS: can't seek in unknown length stream";
        case VORBIS_unexpected_eof: return "VORBIS: unexpected eof";
        case VORBIS_seek_invalid: return "VORBIS: invalid seek";
        case VORBIS_invalid_setup: return "VORBIS: invalid setup";
        case VORBIS_invalid_stream: return "VORBIS: invalid stream";
        case VORBIS_missing_capture_pattern: return "VORBIS: missing capture pattern";
        case VORBIS_invalid_stream_structure_version: return "VORBIS: invalid stream structure version";
        case VORBIS_continued_packet_flag_invalid: return "VORBIS: continued packet flag invalid";
        case VORBIS_incorrect_stream_serial_number: return "VORBIS: incorrect stream serial number";
        case VORBIS_invalid_first_page: return "VORBIS: invalid first page";
        case VORBIS_bad_packet_type: return "VORBIS: bad packet type";
        case VORBIS_cant_find_last_page: return "VORBIS: can't find last page";
        case VORBIS_seek_failed: return "VORBIS: seek failed";
        case VORBIS_ogg_skeleton_not_supported: return "VORBIS: multi-track streams are not supported; "
                                                       "consider re-encoding without the Ogg Skeleton bitstream";
        default: break;
    } /* switch */

    return "VORBIS: unknown error";
} /* vorbis_error_string */
#endif

static int VORBIS_init(void)
{
    return 1;  /* always succeeds. */
} /* VORBIS_init */

static void VORBIS_quit(void)
{
    /* it's a no-op. */
} /* VORBIS_quit */

static int VORBIS_open(Sound_Sample *sample, const char *ext)
{
    (void) ext; // deliberately unused, but present for API compliance
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    SDL_RWops *rw = internal->rw;
    int err = 0;
    stb_vorbis *stb = stb_vorbis_open_rwops(rw, 0, &err, NULL);

	if (stb == NULL) {
		SNDDBG(("%s (error code: %d)\n", vorbis_error_string(err), err));
        return 0;
	}
    internal->decoder_private = stb;
    sample->flags = SOUND_SAMPLEFLAG_CANSEEK;
    sample->actual.format = AUDIO_S16SYS; // returns byte-order native to the running architecture
    sample->actual.channels = stb->channels;
    sample->actual.rate = stb->sample_rate;
    const unsigned int num_frames = stb_vorbis_stream_length_in_samples(stb);
    if (!num_frames) {
        internal->total_time = -1;
    }
    else {
        const unsigned int rate = stb->sample_rate;
        internal->total_time = (num_frames / rate) * 1000;
        internal->total_time += (num_frames % rate) * 1000 / rate;
    } /* else */

    return 1; /* we'll handle this data. */
} /* VORBIS_open */


static void VORBIS_close(Sound_Sample *sample)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    stb_vorbis *stb = (stb_vorbis *) internal->decoder_private;
    stb_vorbis_close(stb);
} /* VORBIS_close */


static Uint32 VORBIS_read(Sound_Sample *sample, void* buffer, Uint32 desired_frames)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    stb_vorbis *stb = (stb_vorbis *) internal->decoder_private;
    const int channels = (int) sample->actual.channels;
    const int desired_samples = desired_frames * channels;

    // Note that for interleaved data, you pass in the number of shorts (the
    // size of your array), but the return value is the number of samples per
    // channel, not the total number of samples.

    stb_vorbis_get_error(stb);  /* clear any error state */
    const int decoded_frames = stb_vorbis_get_samples_short_interleaved(stb, channels, (int16_t *) buffer, desired_samples);
    const int err = stb_vorbis_get_error(stb);

    if (decoded_frames == 0) {
        sample->flags |= (err ? (Uint32)SOUND_SAMPLEFLAG_ERROR : (Uint32)SOUND_SAMPLEFLAG_EOF);
    }
    else if (decoded_frames < (int) desired_frames) {
        sample->flags |= (Uint32)SOUND_SAMPLEFLAG_EAGAIN;
    }
    return decoded_frames;
} /* VORBIS_read */


static int VORBIS_rewind(Sound_Sample *sample)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    stb_vorbis *stb = (stb_vorbis *) internal->decoder_private;

    if (!stb_vorbis_seek_start(stb)) {
        SNDDBG(("%s\n", vorbis_error_string(stb_vorbis_get_error(stb))));
        return 0;
    }

    return 1;
} /* VORBIS_rewind */


static int VORBIS_seek(Sound_Sample *sample, Uint32 ms)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    stb_vorbis *stb = (stb_vorbis *) internal->decoder_private;
    const float frames_per_ms = ((float) sample->actual.rate) / 1000.0f;
    const Uint32 frame_offset = lroundf(frames_per_ms * ms);
    const unsigned int sampnum = (unsigned int) frame_offset;

    if (!stb_vorbis_seek(stb, sampnum)) {
        SNDDBG(("%s\n", vorbis_error_string(stb_vorbis_get_error(stb))));
        return 0;
    }
    return 1;
} /* VORBIS_seek */


static const char *extensions_vorbis[] = { "OGG", "OGA", "VORBIS", NULL };
const Sound_DecoderFunctions __Sound_DecoderFunctions_VORBIS =
{
    {
        extensions_vorbis,
        "Ogg Vorbis audio",
        "The DOSBox Staging Team"
    },

    VORBIS_init,       /*   init() method */
    VORBIS_quit,       /*   quit() method */
    VORBIS_open,       /*   open() method */
    VORBIS_close,      /*  close() method */
    VORBIS_read,       /*   read() method */
    VORBIS_rewind,     /* rewind() method */
    VORBIS_seek        /*   seek() method */
};

/* end of SDL_sound_vorbis.c ... */
