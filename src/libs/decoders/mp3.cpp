/**
 * This DOSBox mp3 decooder backend is maintained by Kevin R. Croft (krcroft@gmail.com)
 * This decoder makes use of the following single-header public libraries:
 *   - dr_mp3: http://mackron.github.io/dr_mp3.html, by David Reid
 *
 * The upstream SDL2 Sound 1.9.x mp3 decoder is written and copyright by Ryan C. Gordon. (icculus@icculus.org)
 *
 *  This SDL_sound MP3 decoder backend is free software: you can redistribute
 *  it and/or modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This MP3 decoder backend is distributed in the hope that it
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

#include <assert.h>

#include <SDL.h> // provides: SDL_malloc, SDL_realloc, SDL_free, SDL_memcpy, and SDL_memset
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO 1
#define DRMP3_FREE(p)                    SDL_free((p))
#define DRMP3_ASSERT(x)                  assert((x))
#define DRMP3_MALLOC(sz)                 SDL_malloc((sz))
#define DRMP3_REALLOC(p, sz)             SDL_realloc((p), (sz))
#define DRMP3_ZERO_MEMORY(p, sz)         SDL_memset((p), 0, (sz))
#define DRMP3_COPY_MEMORY(dst, src, sz)  SDL_memcpy((dst), (src), (sz))
#include "dr_mp3.h" // provides: drmp3

#include "mp3_seek_table.h" // provides: populate_seek_table and SDL_Sound headers

#include "SDL_sound.h"
#define __SDL_SOUND_INTERNAL__
#include "SDL_sound_internal.h" // provides: Sound_SampleInternal

#define MP3_FAST_SEEK_FILENAME "fastseek.lut"

static size_t mp3_read(void* const pUserData, void* const pBufferOut, const size_t bytesToRead)
{
    Uint8* ptr = static_cast<Uint8*>(pBufferOut);
    Sound_Sample* const sample = static_cast<Sound_Sample*>(pUserData);
    const Sound_SampleInternal* const internal = static_cast<const Sound_SampleInternal*>(sample->opaque);
    SDL_RWops* rwops = internal->rw;
    size_t retval = 0;

    while (retval < bytesToRead)
    {
        const size_t rc = SDL_RWread(rwops, ptr, 1, bytesToRead);
        if (rc == 0) {
            sample->flags |= SOUND_SAMPLEFLAG_EOF;
            break;
        } /* if */
        retval += rc;
        ptr += rc;
    } /* while */

    return retval;
} /* mp3_read */

static drmp3_bool32 mp3_seek(void* const pUserData, const Sint32 offset, const drmp3_seek_origin origin)
{
    const Sint32 whence = (origin == drmp3_seek_origin_start) ? RW_SEEK_SET : RW_SEEK_CUR;
    Sound_Sample* const sample = static_cast<Sound_Sample*>(pUserData);
    Sound_SampleInternal* const internal = static_cast<Sound_SampleInternal*>(sample->opaque);
    return (SDL_RWseek(internal->rw, offset, whence) != -1) ? DRMP3_TRUE : DRMP3_FALSE;
} /* mp3_seek */


static Sint32 MP3_init(void)
{
    return 1;  /* always succeeds. */
} /* MP3_init */


static void MP3_quit(void)
{
    /* it's a no-op. */
} /* MP3_quit */

static void MP3_close(Sound_Sample* const sample)
{
    Sound_SampleInternal* const internal = static_cast<Sound_SampleInternal*>(sample->opaque);
    mp3_t* p_mp3 = static_cast<mp3_t*>(internal->decoder_private);
    if (p_mp3 != nullptr) {
        if (p_mp3->p_dr != nullptr) {
            drmp3_uninit(p_mp3->p_dr);
            SDL_free(p_mp3->p_dr);
        }
        // maps and vector destructors free their memory
        SDL_free(p_mp3);
        internal->decoder_private = nullptr;
    }
} /* MP3_close */

static Uint32 MP3_read(Sound_Sample* const sample, void* buffer, Uint32 desired_frames)
{
    Sound_SampleInternal* const internal = static_cast<Sound_SampleInternal*>(sample->opaque);
    mp3_t* p_mp3 = static_cast<mp3_t*>(internal->decoder_private);

    // LOG_MSG("read-while: num_frames: %u", num_frames);
    return static_cast<Uint32>(drmp3_read_pcm_frames_s16(p_mp3->p_dr,
                                                         static_cast<drmp3_uint64>(desired_frames),
                                                         static_cast<drmp3_int16*>(buffer)));
} /* MP3_read */

static Sint32 MP3_open(Sound_Sample* const sample, const char* const ext)
{
    (void) ext; // deliberately unused
    Sound_SampleInternal* const internal = static_cast<Sound_SampleInternal*>(sample->opaque);
    Sint32 result(0); // assume failure until proven otherwise
    mp3_t* p_mp3 = (mp3_t*) SDL_calloc(1, sizeof (mp3_t));
    if (p_mp3 != nullptr) {
        p_mp3->p_dr = (drmp3*) SDL_calloc(1, sizeof (drmp3));
        if (p_mp3->p_dr != nullptr) {
            result = drmp3_init(p_mp3->p_dr, mp3_read, mp3_seek, sample, nullptr, nullptr);
            if (result == DRMP3_TRUE) {
                SNDDBG(("MP3: Accepting data stream.\n"));
                sample->flags = SOUND_SAMPLEFLAG_CANSEEK;
                sample->actual.channels = p_mp3->p_dr->channels;
                sample->actual.rate = p_mp3->p_dr->sampleRate;
                sample->actual.format = AUDIO_S16SYS;  // returns native byte-order based on architecture
                const Uint64 num_frames = populate_seek_points(internal->rw, p_mp3, MP3_FAST_SEEK_FILENAME); // status will be 0 or pcm_frame_count
                if (num_frames != 0) {
                    const unsigned int rate = p_mp3->p_dr->sampleRate;
                    internal->total_time = ( static_cast<Sint32>(num_frames) / rate) * 1000;
                    internal->total_time += (num_frames % rate) * 1000 / rate;
                    result = 1;
                } else {
                    internal->total_time = -1;
                }
            }
        }
    }

    // Assign our internal decoder to the mp3 object we've just populated
    internal->decoder_private = p_mp3;

    // if anything went wrong then tear down our private structure
    if (result == 0) {
        MP3_close(sample);
    }

    return result;
} /* MP3_open */

static Sint32 MP3_rewind(Sound_Sample* const sample)
{
    Sound_SampleInternal* const internal = static_cast<Sound_SampleInternal*>(sample->opaque);
    mp3_t* p_mp3 = static_cast<mp3_t*>(internal->decoder_private);
    return (drmp3_seek_to_start_of_stream(p_mp3->p_dr) == DRMP3_TRUE);
} /* MP3_rewind */

static Sint32 MP3_seek(Sound_Sample* const sample, const Uint32 ms)
{
    Sound_SampleInternal* const internal = static_cast<Sound_SampleInternal*>(sample->opaque);
    mp3_t* p_mp3 = static_cast<mp3_t*>(internal->decoder_private);
    const float frames_per_ms = sample->actual.rate / 1000.0f;
    const drmp3_uint64 frame_offset = static_cast<drmp3_uint64>(frames_per_ms) * ms;
    const Sint32 result = drmp3_seek_to_pcm_frame(p_mp3->p_dr, frame_offset);
    return (result == DRMP3_TRUE);
} /* MP3_seek */

/* dr_mp3 will play layer 1 and 2 files, too */
static const char* extensions_mp3[] = { "MP3", "MP2", "MP1", nullptr };

extern const Sound_DecoderFunctions __Sound_DecoderFunctions_MP3 = {
    {
        extensions_mp3,
        "MPEG-1 Audio Layer I-III",
        "Ryan C. Gordon <icculus@icculus.org>",
        "https://icculus.org/SDL_sound/"
    },

    MP3_init,       /*   init() method */
    MP3_quit,       /*   quit() method */
    MP3_open,       /*   open() method */
    MP3_close,      /*  close() method */
    MP3_read,       /*   read() method */
    MP3_rewind,     /* rewind() method */
    MP3_seek        /*   seek() method */
}; }
/* end of SDL_sound_mp3.c ... */
