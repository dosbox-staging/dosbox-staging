/*
 * DOSBox WAV decoder is maintained by Kevin R. Croft (krcroft@gmail.com)
 * This decoder makes use of the excellent dr_wav library by David Reid (mackron@gmail.com)
 *
 * Source links
 *   - dr_libs: https://github.com/mackron/dr_libs (source)
 *   - dr_wav: http://mackron.github.io/dr_wav.html (website)
 *
 * Please see the file LICENSE.txt in the source's root directory.
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

/* Map dr_wav's memory routines to SDL's */
#define DRWAV_FREE(p)                   SDL_free((p))
#define DRWAV_MALLOC(sz)                SDL_malloc((sz))
#define DRWAV_REALLOC(p, sz)            SDL_realloc((p), (sz))
#define DRWAV_ZERO_MEMORY(p, sz)        SDL_memset((p), 0, (sz))
#define DRWAV_COPY_MEMORY(dst, src, sz) SDL_memcpy((dst), (src), (sz))

#define DR_WAV_NO_STDIO
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

static size_t wav_read(void* pUserData, void* pBufferOut, size_t bytesToRead)
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
} /* wav_read */

static drwav_bool32 wav_seek(void* pUserData, int offset, drwav_seek_origin origin)
{
    const int whence = (origin == drwav_seek_origin_start) ? RW_SEEK_SET : RW_SEEK_CUR;
    Sound_Sample *sample = (Sound_Sample *) pUserData;
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    return (SDL_RWseek(internal->rw, offset, whence) != -1) ? DRWAV_TRUE : DRWAV_FALSE;
} /* wav_seek */


static int WAV_init(void)
{
    return 1;  /* always succeeds. */
} /* WAV_init */


static void WAV_quit(void)
{
    /* it's a no-op. */
} /* WAV_quit */

static void WAV_close(Sound_Sample *sample)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    drwav *dr = (drwav *) internal->decoder_private;
    if (dr != NULL) {
        (void) drwav_uninit(dr);
        SDL_free(dr);
        internal->decoder_private = NULL;
    }
    return;
} /* WAV_close */

static int WAV_open(Sound_Sample *sample, const char *ext)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    drwav* dr = SDL_malloc(sizeof(drwav));
    drwav_result result = drwav_init_ex(dr, wav_read, wav_seek, NULL, sample, NULL, 0, NULL);
    internal->decoder_private = dr;

    if (result == DRWAV_TRUE) {
        SNDDBG(("WAV: Codec accepted the data stream.\n"));
        sample->flags = SOUND_SAMPLEFLAG_CANSEEK;
        sample->actual.rate = dr->sampleRate;
        sample->actual.format = AUDIO_S16SYS;
        sample->actual.channels = dr->channels;

        const Uint64 frames = (Uint64) dr->totalPCMFrameCount;
        if (frames == 0)
            internal->total_time = -1;
        else {
            const Uint32 rate = (Uint32) dr->sampleRate;
            internal->total_time = (frames / rate) * 1000;
            internal->total_time += ((frames % rate) * 1000) / rate;
        }  /* else */

    } /* if result != DRWAV_TRUE */
    else {
        SNDDBG(("WAV: Codec could not parse the data stream.\n"));
        WAV_close(sample);
    }
    return result;
} /* WAV_open */


static Uint32 WAV_read(Sound_Sample *sample)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    drwav *dr = (drwav *) internal->decoder_private;
    const drwav_uint64 frames_read = drwav_read_pcm_frames_s16(dr,
                                         internal->buffer_size / (dr->channels * sizeof(drwav_int16)),
                                         (drwav_int16 *) internal->buffer);
    return frames_read * dr->channels * sizeof (drwav_int16);
} /* WAV_read */


static int WAV_rewind(Sound_Sample *sample)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    drwav *dr = (drwav *) internal->decoder_private;
    return (drwav_seek_to_pcm_frame(dr, 0) == DRWAV_TRUE);
} /* WAV_rewind */

static int WAV_seek(Sound_Sample *sample, Uint32 ms)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    drwav *dr = (drwav *) internal->decoder_private;
    const float frames_per_ms = ((float) sample->actual.rate) / 1000.0f;
    const drwav_uint64 frame_offset = (drwav_uint64) (frames_per_ms * ((float) ms));
    return (drwav_seek_to_pcm_frame(dr, frame_offset) == DRWAV_TRUE);
} /* WAV_seek */

static const char *extensions_wav[] = { "WAV", "W64", NULL };

const Sound_DecoderFunctions __Sound_DecoderFunctions_WAV =
{
    {
        extensions_wav,
        "WAV Audio Codec",
        "Kevin R. Croft <krcroft@gmail.com>",
        "github.com/mackron/dr_libs/blob/master/dr_wav.h"
    },

    WAV_init,       /*   init() method */
    WAV_quit,       /*   quit() method */
    WAV_open,       /*   open() method */
    WAV_close,      /*  close() method */
    WAV_read,       /*   read() method */
    WAV_rewind,     /* rewind() method */
    WAV_seek        /*   seek() method */
};
/* end of wav.c ... */
