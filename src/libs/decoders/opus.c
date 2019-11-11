/*
 * This DOSBox Ogg Opus decoder backend is written and copyright 2019 Kevin R Croft (krcroft@gmail.com)
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This decoders makes use of:
 *   - libopusfile, for .opus file handing and frame decoding
 *
 * Source links
 *   - opusfile:   https://github.com/xiph/opusfile
 *   - opus-tools: https://github.com/xiph/opus-tools

 * Documentation references
 *   - Ogg Opus:  https://www.opus-codec.org/docs
 *   - OpusFile:  https://mf4.xiph.org/jenkins/view/opus/job/opusfile-unix/ws/doc/html/index.html
 *
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <opusfile.h>

#include "SDL_sound.h"
#define __SDL_SOUND_INTERNAL__
#include "SDL_sound_internal.h"

// Opus's internal sampling rates to which all encoded streams get resampled
#define OPUS_SAMPLE_RATE 48000
#define OPUS_SAMPLE_RATE_PER_MS 48

static Sint32 opus_init   (void);
static void   opus_quit   (void);
static Sint32 opus_open   (Sound_Sample* sample, const char* ext);
static void   opus_close  (Sound_Sample* sample);
static Uint32 opus_read   (Sound_Sample* sample, void* buffer, Uint32 desired_frames);
static Sint32 opus_rewind (Sound_Sample* sample);
static Sint32 opus_seek   (Sound_Sample* sample, const Uint32 ms);

static const char* extensions_opus[] = { "OPUS", NULL };

const Sound_DecoderFunctions __Sound_DecoderFunctions_OPUS =
{
    {
        extensions_opus,
        "Ogg Opus audio using libopusfile",
        "Kevin R Croft <krcroft@gmail.com>",
        "https://www.opus-codec.org/"
    },

    opus_init,   /*   init() method */
    opus_quit,   /*   quit() method */
    opus_open,   /*   open() method */
    opus_close,  /*  close() method */
    opus_read,   /*   read() method */
    opus_rewind, /* rewind() method */
    opus_seek    /*   seek() method */
};

static Sint32 opus_init(void)
{
    SNDDBG(("Opus init:              done\n"));
    return 1; /* always succeeds. */
} /* opus_init */


static void opus_quit(void){
    SNDDBG(("Opus quit:              done\n"));
} // no-op


/*
 * Read-Write Ops Read Callback Wrapper
 * ------------------------------------
 * OPUS: typedef int(*op_read_func)
 *       void*       _stream  --> The stream to read from
 *       unsigned char* _ptr  --> The buffer to store the data in
 *       int         _nbytes  --> The maximum number of bytes to read.
 *    Returns: The number of bytes successfully read, or a negative value on error.
 *
 * SDL: size_t SDL_RWread
 *      struct SDL_RWops* context --> a pointer to an SDL_RWops structure
 *      void*             ptr     --> a pointer to a buffer to read data into
 *      size_t            size    --> the size of each object to read, in bytes
 *      size_t            maxnum  --> the maximum number of objects to be read
 */
static Sint32 RWops_opus_read(void* stream, unsigned char* ptr, Sint32 nbytes)
{
    const Sint32 bytes_read = SDL_RWread((SDL_RWops*)stream,
                                         (void*)ptr,
                                         sizeof(unsigned char),
                                         (size_t)nbytes);
    SNDDBG(("Opus ops read:          "
            "{wanted: %d, returned: %ld}\n", nbytes, bytes_read));

    return bytes_read;
} /* RWops_opus_read */


/*
 * Read-Write Ops Seek Callback Wrapper
 * ------------------------------------
 *
 * OPUS: typedef int(* op_seek_func)
 *       void*      _stream, --> The stream to seek in
 *       opus_int64 _offset, --> Sets the position indicator for _stream in bytes
 *       int _whence         --> If whence is set to SEEK_SET, SEEK_CUR, or SEEK_END,
 *                               the offset is relative to the start of the stream,
 *                               the current position indicator, or end-of-file,
 *                               respectively
 *    Returns: 0  Success, or -1 if seeking is not supported or an error occurred.
 *      define  SEEK_SET    0
 *      define  SEEK_CUR    1
 *      define  SEEK_END    2
 *
 * SDL: Sint64 SDL_RWseek
 *      SDL_RWops* context --> a pointer to an SDL_RWops structure
 *      Sint64     offset, --> offset, in bytes
 *      Sint32     whence  --> an offset in bytes, relative to whence location; can be negative
 *   Returns the final offset in the data stream after the seek or -1 on error.
 *      RW_SEEK_SET   0
 *      RW_SEEK_CUR   1
 *      RW_SEEK_END   2
 */
static Sint32 RWops_opus_seek(void* stream, const opus_int64 offset, const Sint32 whence)
{
    const Sint64 offset_after_seek = SDL_RWseek((SDL_RWops*)stream, (int)offset, whence);
    SNDDBG(("Opus ops seek:          "
            "{requested offset: %ld, seeked offset: %ld}\n",
            offset, offset_after_seek));
    return (offset_after_seek != -1 ? 0 : -1);
} /* RWops_opus_seek */


/*
 * Read-Write Ops Close Callback Wrapper
 * -------------------------------------
 * OPUS: typedef int(* op_close_func)(void *_stream)
 * SDL:  Sint32 SDL_RWclose(struct SDL_RWops* context)
 */
static Sint32 RWops_opus_close(void* stream)
{
    /* SDL closes this for us */
    // return SDL_RWclose((SDL_RWops*)stream);
    return 0;
} /* RWops_opus_close */


/*
 * Read-Write Ops Tell Callback Wrapper
 * ------------------------------------
 * OPUS: typedef opus_int64(* op_tell_func)(void *_stream)
 * SDL:  Sint64 SDL_RWtell(struct SDL_RWops* context)
 */
static opus_int64 RWops_opus_tell(void* stream)
{
    const Sint64 current_offset = SDL_RWtell((SDL_RWops*)stream);
    SNDDBG(("Opus ops tell:          "
            "%ld\n", current_offset));
    return current_offset;
} /* RWops_opus_tell */


// Populate the opus callback object (in perscribed order), with our callback functions.
static const OpusFileCallbacks RWops_opus_callbacks =
{
    .read =  RWops_opus_read,
    .seek =  RWops_opus_seek,
    .tell =  RWops_opus_tell,
    .close = RWops_opus_close
};

static __inline__ void output_opus_info(const OggOpusFile* of, const OpusHead* oh)
{
#if (defined DEBUG_CHATTER)
    const OpusTags* ot = op_tags(of, -1);
    // Guard
    if (of != NULL && oh != NULL && ot != NULL) {
        SNDDBG(("Opus serial number:     %u\n", op_serialno(of, -1)));
        SNDDBG(("Opus format version:    %d\n", oh->version));
        SNDDBG(("Opus channel count:     %d\n", oh->channel_count ));
        SNDDBG(("Opus seekable:          %s\n", op_seekable(of) ? "True" : "False"));
        SNDDBG(("Opus pre-skip samples:  %u\n", oh->pre_skip));
        SNDDBG(("Opus input sample rate: %u\n", oh->input_sample_rate));
        SNDDBG(("Opus logical streams:   %d\n", oh->stream_count));
        SNDDBG(("Opus vendor:            %s\n", ot->vendor));
        for (int i = 0; i < ot->comments; i++) {
            SNDDBG(("Opus: user comment:     '%s'\n", ot->user_comments[i]));
        }
    }
#endif
} /* output_opus_comments */

/*
 * Opus Open
 * ---------
 *  - Creates a new opus file object by using our our callback structure for all IO operations.
 *  - SDL expects a returns of 1 on success
 */
static Sint32 opus_open(Sound_Sample* sample, const char* ext)
{
    Sint32 rcode;
    Sound_SampleInternal* internal = (Sound_SampleInternal*)sample->opaque;
    OggOpusFile* of = op_open_callbacks(internal->rw, &RWops_opus_callbacks, NULL, 0, &rcode);
    internal->decoder_private = of;
    if (rcode != 0) {
        opus_close(sample);
        SNDDBG(("Opus open error:        "
                "'Could not open opus file: %s'\n", opus_strerror(rcode)));
        BAIL_MACRO("Opus open fatal: 'Not a valid Ogg Opus file'", 0);
    }
    const OpusHead* oh = op_head(of, -1);
    output_opus_info(of, oh);

    sample->actual.rate = OPUS_SAMPLE_RATE;
    sample->actual.channels = (Uint8)(oh->channel_count);
    sample->flags = op_seekable(of) ? SOUND_SAMPLEFLAG_CANSEEK: 0;
    sample->actual.format = AUDIO_S16SYS;
    ogg_int64_t total_time = op_pcm_total(of, -1);        // total PCM samples in the stream
    internal->total_time = total_time == OP_EINVAL ? -1 : // total milliseconds in the stream
                                         (Sint32)( (double)total_time / OPUS_SAMPLE_RATE_PER_MS);
    return 1;
} /* opus_open */


/*
 * Opus Close
 * ----------
 * Free and NULL all heap-allocated codec objects.
 */
static void opus_close(Sound_Sample* sample)
{
    /* From the Opus docs: if opening a stream/file/or using op_test_callbacks() fails
     * then we are still responsible for freeing the OggOpusFile with op_free().
     */
    Sound_SampleInternal* internal = (Sound_SampleInternal*) sample->opaque;
    OggOpusFile* of = internal->decoder_private;
    if (of != NULL) {
        op_free(of);
        internal->decoder_private = NULL;
    }
    return;

} /* opus_close */


/*
 * Opus Read
 * ---------
 */
static Uint32 opus_read(Sound_Sample* sample, void* buffer, Uint32 desired_frames)
{
    int decoded_frames = 0;
    if (desired_frames > 0) {
        Sound_SampleInternal* internal = (Sound_SampleInternal*) sample->opaque;
        OggOpusFile* of = internal->decoder_private;
        decoded_frames = op_read(of, (opus_int16*)buffer, desired_frames * sample->actual.channels, NULL);
        if      (decoded_frames == 0)       { sample->flags |= SOUND_SAMPLEFLAG_EOF; }
        else if (decoded_frames == OP_HOLE) { sample->flags |= SOUND_SAMPLEFLAG_EAGAIN; }
        else if (decoded_frames  < 0)       { sample->flags |= SOUND_SAMPLEFLAG_ERROR; }
    }
    return decoded_frames;
} /* opus_read */


/*
 * Opus Rewind
 * -----------
 * Sets the current position of the stream to 0.
 */
static Sint32 opus_rewind(Sound_Sample* sample)
{
    const Sint32 rcode = opus_seek(sample, 0);
    BAIL_IF_MACRO(rcode < 0, ERR_IO_ERROR, 0);
    return rcode;
} /* opus_rewind */


/*
 * Opus Seek
 * ---------
 * Set the current position of the stream to the indicated
 * integer offset in milliseconds.
 */
static Sint32 opus_seek(Sound_Sample* sample, const Uint32 ms)
{
    Sound_SampleInternal* internal = (Sound_SampleInternal*) sample->opaque;
    OggOpusFile* of = internal->decoder_private;
    int rcode = -1;

    #if (defined DEBUG_CHATTER)
    const float total_seconds = (float)ms/1000;
    uint8_t minutes = total_seconds / 60;
    const float seconds = ((int)total_seconds % 60) + (total_seconds - (int)total_seconds);
    const uint8_t hours = minutes / 60;
    minutes = minutes % 60;
    #endif

    // convert the desired ms offset into OPUS PCM samples
    const ogg_int64_t desired_pcm = ms * OPUS_SAMPLE_RATE_PER_MS;
    rcode = op_pcm_seek(of, desired_pcm);

	if (rcode != 0) {
        SNDDBG(("Opus seek error:        %s\n", opus_strerror(rcode)));
        sample->flags |= SOUND_SAMPLEFLAG_ERROR;
	} else {
        SNDDBG(("Opus seek in file:      "
                "{requested_time: '%02d:%02d:%.2f', becomes_opus_pcm: %ld}\n",
                hours, minutes, seconds, desired_pcm));
    }
    return (rcode == 0);
} /* opus_seek */

/* end of ogg_opus.c ... */
