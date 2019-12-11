/*
 * DOSBox FLAC decoder is maintained by Kevin R. Croft (krcroft@gmail.com)
 * This decoder makes use of the excellent dr_flac library by David Reid (mackron@gmail.com)
 *
 * Source links
 *   - dr_libs: https://github.com/mackron/dr_libs (source)
 *   - dr_flac: http://mackron.github.io/dr_flac.html (website)
 *
 * The upstream SDL2 Sound 1.9.x FLAC decoder is written and copyright by Ryan C. Gordon. (icculus@icculus.org)
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file is part of the SDL Sound Library.
 *
 *  This SDL_sound Ogg Opus decoder backend is free software: you can redistribute
 *  it and/or modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This  SDL_sound Ogg Opus decoder backend is distributed in the hope that it
 *  will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the SDL Sound Library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include "SDL_sound.h"
#define __SDL_SOUND_INTERNAL__
#include "SDL_sound_internal.h"

#define DR_FLAC_IMPLEMENTATION
// #define DR_FLAC_NO_SIMD 1 /* temporary work-around for https://github.com/mackron/dr_libs/issues/63 */
#define DR_FLAC_NO_STDIO 1
#define DR_FLAC_NO_WIN32_IO 1
#define DR_FLAC_NO_CRC 1
#define DRFLAC_MALLOC(sz) SDL_malloc((sz))
#define DRFLAC_REALLOC(p, sz) SDL_realloc((p), (sz))
#define DRFLAC_FREE(p) SDL_free((p))
#define DRFLAC_COPY_MEMORY(dst, src, sz) SDL_memcpy((dst), (src), (sz))
#define DRFLAC_ZERO_MEMORY(p, sz) SDL_memset((p), 0, (sz))
#include "dr_flac.h"

static size_t flac_read(void* pUserData, void* pBufferOut, size_t bytesToRead)
{
    Uint8 *ptr = (Uint8 *) pBufferOut;
    Sound_Sample *sample = (Sound_Sample *) pUserData;
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    SDL_RWops *rwops = internal->rw;
    size_t retval = 0;

    while (retval < bytesToRead)
    {
        const size_t rc = SDL_RWread(rwops, ptr, 1, bytesToRead);
        if (rc == 0)
        {
            sample->flags |= SOUND_SAMPLEFLAG_EOF;
            break;
        } /* if */
        else
        {
            retval += rc;
            ptr += rc;
        } /* else */
    } /* while */

    return retval;
} /* flac_read */

static drflac_bool32 flac_seek(void* pUserData, int offset, drflac_seek_origin origin)
{
    const int whence = (origin == drflac_seek_origin_start) ? RW_SEEK_SET : RW_SEEK_CUR;
    Sound_Sample *sample = (Sound_Sample *) pUserData;
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    return (SDL_RWseek(internal->rw, offset, whence) != -1) ? DRFLAC_TRUE : DRFLAC_FALSE;
} /* flac_seek */


static int FLAC_init(void)
{
    return 1;  /* always succeeds. */
} /* FLAC_init */


static void FLAC_quit(void)
{
    /* it's a no-op. */
} /* FLAC_quit */

static int FLAC_open(Sound_Sample *sample, const char *ext)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    drflac *dr = drflac_open(flac_read, flac_seek, sample, NULL);

    if (!dr)
    {
        BAIL_IF_MACRO(sample->flags & SOUND_SAMPLEFLAG_ERROR, ERR_IO_ERROR, 0);
        BAIL_MACRO("FLAC: Not a FLAC stream.", 0);
    } /* if */

    SNDDBG(("FLAC: Accepting data stream.\n"));
    sample->flags = SOUND_SAMPLEFLAG_CANSEEK;

    sample->actual.channels = dr->channels;
    sample->actual.rate = dr->sampleRate;
    sample->actual.format = AUDIO_S16SYS; /* returns native byte-order based on architecture */

    const Uint64 frames = (Uint64) dr->totalPCMFrameCount;
    if (frames == 0)
        internal->total_time = -1;
    else
    {
        const Uint32 rate = (Uint32) dr->sampleRate;
        internal->total_time = (frames / rate) * 1000;
        internal->total_time += ((frames % rate) * 1000) / rate;
    } /* else */

    internal->decoder_private = dr;

    return 1;
} /* FLAC_open */



static void FLAC_close(Sound_Sample *sample)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    drflac *dr = (drflac *) internal->decoder_private;
    drflac_close(dr);
} /* FLAC_close */


static Uint32 FLAC_read(Sound_Sample *sample)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    drflac *dr = (drflac *) internal->decoder_private;
    const drflac_uint64 rc = drflac_read_pcm_frames_s16(dr,
                                                        internal->buffer_size / (dr->channels * sizeof(drflac_int16)),
                                                        (drflac_int16 *) internal->buffer);
    return rc * dr->channels * sizeof (drflac_int16);
} /* FLAC_read */


static int FLAC_rewind(Sound_Sample *sample)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    drflac *dr = (drflac *) internal->decoder_private;
    return (drflac_seek_to_pcm_frame(dr, 0) == DRFLAC_TRUE);
} /* FLAC_rewind */

static int FLAC_seek(Sound_Sample *sample, Uint32 ms)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    drflac *dr = (drflac *) internal->decoder_private;
    const float frames_per_ms = ((float) sample->actual.rate) / 1000.0f;
    const drflac_uint64 frame_offset = (drflac_uint64) (frames_per_ms * ((float) ms));
    return (drflac_seek_to_pcm_frame(dr, frame_offset) == DRFLAC_TRUE);
} /* FLAC_seek */


static const char *extensions_flac[] = { "FLAC", "FLA", NULL };
const Sound_DecoderFunctions __Sound_DecoderFunctions_FLAC =
{
    {
        extensions_flac,
        "Free Lossless Audio Codec",
        "Ryan C. Gordon <icculus@icculus.org>",
        "https://icculus.org/SDL_sound/"
    },

    FLAC_init,       /*   init() method */
    FLAC_quit,       /*   quit() method */
    FLAC_open,       /*   open() method */
    FLAC_close,      /*  close() method */
    FLAC_read,       /*   read() method */
    FLAC_rewind,     /* rewind() method */
    FLAC_seek        /*   seek() method */
};

/* end of flac.c ... */
