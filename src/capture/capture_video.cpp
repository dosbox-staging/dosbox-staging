// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "capture.h"

#include "private/capture_video.h"
#include "video/zmbv.h"

#include <cassert>
#include <cmath>
#include <cstring>

#include "hardware/memory.h"
#include "misc/support.h"
#include "utils/math_utils.h"

static constexpr auto NumSampleFramesInBuffer = 16 * 1024;

static constexpr auto SampleFrameSizeBytes = 4;
static constexpr auto NumAudioChannels = 2;

// The mixer delivers captured audio to us in producer-paced bursts, while
// `capture_video_add_frame()` runs per emulated video frame; these are two
// independent clocks. Dumping every sample buffered since the last frame into
// a single `01wb` chunk therefore can yields wildly uneven per-frame audio.
//
// Instead, we meter a fixed per-frame target out of the buffer and hold this
// many sample frames in reserve, so short-term producer jitter is absorbed by
// the reserve and the emitted chunks stay uniform. The cost is a constant
// audio-behind-video interleave offset of roughly this many frames, which
// demuxers reconcile from the stream's sample/frame counts. Sized well above
// the mixer blocksize (1024 by default) and well below the 16K buffer
// capacity so we neither underrun on jitter nor risk the overflow drop in
// `capture_video_add_audio_data()`.
static constexpr int AudioPrebufferFrames = 4 * 1024;

// Captured-audio resync (surplus shedding)
// ========================================
//
// A host too slow to emulate in real time makes the mixer (paced by the wall
// clock, to feed the audio device) produce more audio than the video timeline
// (paced by emulated time) can hold. The excess piles up in the buffer above
// the prebuffer set-point. We periodically "shed" that surplus so the
// captured audio stays locked to the video (this is a "controlled skip
// forward") and crossfade each cut to avoid audible clicks.

// Minimum spacing between sheds, in video frames. Every shed is a crossfade
// seam; shedding on every frame (typically up to 50 to 70 times per second)
// sounds choppy, so we let the surplus accumulate and cut less often. So we
// do larger but fewer skips which sound better. A minimum spacing of 7 frames
// gives the best overall results that works well enough for fast-paced music,
// too (we could use a higher value for slower paces music).
static constexpr int AudioResyncIntervalVideoFrames = 7;

// Don't shed a surplus smaller than this. Keeps normal producer jitter (which
// the prebuffer already absorbs) from triggering needless cuts, so captures
// on a fast-enough host stay pristine. ~21 ms at 48 kHz.
static constexpr int AudioResyncMinSurplusFrames = 1024;

// Frames blended across each cut to smooth the skip-forward. ~2.7 ms for 48
// kHz audio in 70 Hz VGA modes.
static constexpr int AudioResyncCrossfadeFrames = 128;

static constexpr auto AviHeaderSize = 500;

static struct {
	FILE* handle = nullptr;

	uint32_t frames          = 0;
	VideoCodec* codec        = nullptr;
	int width                = 0;
	int height               = 0;
	PixelFormat pixel_format = {};
	float frames_per_second  = 0.0f;

	uint32_t written           = 0;
	uint32_t buf_size          = 0;
	std::vector<uint8_t> buf   = {};
	std::vector<uint8_t> index = {};
	uint32_t index_used        = 0;

	struct {
		// Unit model: this buffer and every count derived from it are
		// measured in audio *sample frames* -- one L+R pair of
		// `NumAudioChannels` samples. `SampleFrameSizeBytes` converts a
		// frame count to bytes; names ending in `VideoFrames` count
		// emulated video frames instead.
		int16_t buf[NumSampleFramesInBuffer][NumAudioChannels] = {};

		uint32_t sample_rate         = 0;
		uint32_t num_buffered_frames = 0;
		uint32_t bytes_written       = 0;

		// Fractional per-frame emit target; carries the sub-frame
		// remainder (and any unmet demand from an underrun) between
		// video frames. See `AudioPrebufferFrames`.
		double frame_credit = 0.0;

		// False until the reserve has first filled to
		// `AudioPrebufferFrames`; gates the start of metered output.
		bool is_primed = false;

		// Video frames since the last surplus shed; gates the resync
		// cadence. See `AudioResyncIntervalVideoFrames`.
		int num_video_frames_since_resync = 0;
	} audio = {};
} video = {};

static ZMBV_FORMAT to_zmbv_format(const PixelFormat format)
{
	switch (format) {
	case PixelFormat::Indexed8: return ZMBV_FORMAT::BPP_8;
	case PixelFormat::RGB555_Packed16: return ZMBV_FORMAT::BPP_15;
	case PixelFormat::RGB565_Packed16: return ZMBV_FORMAT::BPP_16;

	// ZMBV internally maps from 24-bit to 32-bit (let ZMBV manage itself).
	case PixelFormat::BGR24_ByteArray: return ZMBV_FORMAT::BPP_24;

	case PixelFormat::BGRX32_ByteArray: return ZMBV_FORMAT::BPP_32;
	default: assertm(false, "Invalid pixel_format value"); break;
	}
	return ZMBV_FORMAT::NONE;
}

// Returns number of bytes needed to represent the given PixelFormat
static uint8_t to_bytes_per_pixel(const PixelFormat format)
{
	// PixelFormat's underly value is colour depth in number of bits
	const float num_bits = static_cast<uint8_t>(format);

	// Use float-ceil to handle non-power-of-two bit quantities
	constexpr uint8_t bits_per_byte = 8;
	const auto num_bytes = iround(ceilf(num_bits / bits_per_byte));

	assert(num_bytes >= 1 && num_bytes <= 4);
	return static_cast<uint8_t>(num_bytes);
}

static uint8_t to_bytes_per_pixel(const ZMBV_FORMAT format)
{
	return ZMBV_ToBytesPerPixel(format);
}

static void add_avi_chunk(const char* tag, const uint32_t size,
                          const void* data, const uint32_t flags)
{
	uint8_t chunk[8] = {};

	chunk[0] = tag[0];
	chunk[1] = tag[1];
	chunk[2] = tag[2];
	chunk[3] = tag[3];

	host_writed(&chunk[4], size);

	// Write the actual data
	fwrite(chunk, 1, 8, video.handle);

	auto writesize = (size + 1) & ~1;
	fwrite(data, 1, writesize, video.handle);

	auto pos = video.written + 4;
	video.written += writesize + 8;

	if (video.index_used + 16 >= video.index.size()) {
		video.index.resize(video.index.size() + 16 * 4096);
	}

	auto index = video.index.data() + video.index_used;
	video.index_used += 16;

	index[0] = tag[0];
	index[1] = tag[1];
	index[2] = tag[2];
	index[3] = tag[3];

	host_writed(index + 4, flags);
	host_writed(index + 8, pos);
	host_writed(index + 12, size);
}

// Writes the first `num_sample_frames` sample frames from the audio buffer as
// a single `01wb` AVI chunk, then shifts any remaining frames down to the
// front of the buffer. A no-op when `num_sample_frames` is zero.
static void write_audio_chunk(const int num_sample_frames)
{
	assert(num_sample_frames <=
	       static_cast<int>(video.audio.num_buffered_frames));

	if (num_sample_frames == 0) {
		return;
	}

	add_avi_chunk("01wb",
	              num_sample_frames * SampleFrameSizeBytes,
	              video.audio.buf,
	              0);

	video.audio.bytes_written += num_sample_frames * SampleFrameSizeBytes;

	// Slide the frames we didn't write down to the front so the next chunk
	// starts at index 0.
	const int num_remaining_frames = static_cast<int>(
	                                         video.audio.num_buffered_frames) -
	                                 num_sample_frames;

	if (num_remaining_frames > 0) {
		memmove(video.audio.buf,
		        video.audio.buf[num_sample_frames],
		        num_remaining_frames * SampleFrameSizeBytes);
	}
	video.audio.num_buffered_frames = num_remaining_frames;
}

// Sheds `num_frames_to_drop` sample frames from the front of the audio buffer
// to pull the captured audio back in step with the video timeline (see
// `AudioResyncIntervalVideoFrames`). Excising a run of frames outright would
// click, so we instead crossfade the retained head into the audio just past
// the cut: over `num_crossfade_frames` frames we fade the kept audio out
// (weight `1 - blend_weight`) while fading in the audio `num_frames_to_drop`
// frames later (weight `blend_weight`). `blend_weight` runs
// `(frame + 1) / (num_crossfade_frames + 1)` -- the `+ 1`s keep it strictly
// between 0 and 1 so neither source is fully cut off at a seam. Then the tail
// is shifted down. Net effect: a smooth skip-forward of `num_frames_to_drop`
// frames.
static void drop_audio_surplus_with_crossfade(const int num_frames_to_drop)
{
	if (num_frames_to_drop == 0) {
		return;
	}

	// Keep the fade region inside the dropped run so the fade-out and
	// fade-in sources never overlap.
	const int num_crossfade_frames = (AudioResyncCrossfadeFrames < num_frames_to_drop)
	                                       ? AudioResyncCrossfadeFrames
	                                       : num_frames_to_drop;

	// The dropped run plus the run we blend into must both be present.
	assert(static_cast<int>(video.audio.num_buffered_frames) >=
	       num_frames_to_drop + num_crossfade_frames);

	for (int frame = 0; frame < num_crossfade_frames; ++frame) {
		const auto blend_weight = static_cast<float>(frame + 1) /
		                          static_cast<float>(num_crossfade_frames + 1);

		for (int channel = 0; channel < NumAudioChannels; ++channel) {
			const auto fade_out_sample = static_cast<float>(
			        video.audio.buf[frame][channel]);

			const auto fade_in_sample = static_cast<float>(
			        video.audio.buf[num_frames_to_drop + frame][channel]);

			video.audio.buf[frame][channel] = clamp_to_int16(
			        iroundf(fade_out_sample * (1.0f - blend_weight) +
			                fade_in_sample * blend_weight));
		}
	}

	// Slide the untouched tail up so it sits directly behind the blended
	// head, closing the gap the skipped frames left behind.
	const int tail_start_frame = num_frames_to_drop + num_crossfade_frames;
	const int num_tail_frames = static_cast<int>(video.audio.num_buffered_frames) -
	                            tail_start_frame;

	if (num_tail_frames > 0) {
		memmove(video.audio.buf[num_crossfade_frames],
		        video.audio.buf[tail_start_frame],
		        num_tail_frames * SampleFrameSizeBytes);
	}

	video.audio.num_buffered_frames -= num_frames_to_drop;
}

void capture_video_finalise()
{
	if (!video.handle) {
		return;
	}
	if (video.codec) {
		video.codec->FinishVideo();
	}

	// Flush all audio still held back by the per-frame prebuffer so the
	// stream contains every produced sample. Must run before the header is
	// built below, which reads `bytes_written` as the audio stream length.
	write_audio_chunk(video.audio.num_buffered_frames);

	uint8_t avi_header[AviHeaderSize];
	uint32_t header_pos = 0;

#define AVIOUT4(_S_) \
	memcpy(&avi_header[header_pos], _S_, 4); \
	header_pos += 4;
#define AVIOUTw(_S_) \
	host_writew(&avi_header[header_pos], _S_); \
	header_pos += 2;
#define AVIOUTd(_S_) \
	host_writed(&avi_header[header_pos], _S_); \
	header_pos += 4;

	// Try and write an avi header
	AVIOUT4("RIFF"); // Riff header
	AVIOUTd(AviHeaderSize + video.written - 8 + video.index_used);
	AVIOUT4("AVI ");
	AVIOUT4("LIST");

	const auto main_list = header_pos;
	// TODO size of list
	AVIOUTd(0);
	AVIOUT4("hdrl");

	AVIOUT4("avih");
	// # of bytes to follow
	AVIOUTd(56);
	/* Microseconds per frame */
	AVIOUTd((uint32_t)(1000000 / video.frames_per_second));
	AVIOUTd(0);
	AVIOUTd(0);            /* PaddingGranularity (whatever that might be) */
	AVIOUTd(0x110);        /* Flags,0x10 has index, 0x100 interleaved */
	AVIOUTd(video.frames); /* TotalFrames */
	AVIOUTd(0);            /* InitialFrames */
	AVIOUTd(2);            /* Stream count */
	AVIOUTd(0);            /* SuggestedBufferSize */
	AVIOUTd(static_cast<uint32_t>(video.width));  /* Width */
	AVIOUTd(static_cast<uint32_t>(video.height)); /* Height */
	AVIOUTd(0); /* TimeScale:  Unit used to measure time */
	AVIOUTd(0); /* DataRate:   Data rate of playback     */
	AVIOUTd(0); /* StartTime:  Starting time of AVI data */
	AVIOUTd(0); /* DataLength: Size of AVI data chunk    */

	// Video stream list
	AVIOUT4("LIST");
	AVIOUTd(4 + 8 + 56 + 8 + 40); /* Size of the list */
	AVIOUT4("strl");

	// Video stream header
	AVIOUT4("strh");
	AVIOUTd(56);        /* # of bytes to follow */
	AVIOUT4("vids");    /* Type */
	AVIOUT4(CODEC_4CC); /* Handler */
	AVIOUTd(0);         /* Flags */
	AVIOUTd(0);         /* Reserved, MS says: wPriority, wLanguage */
	AVIOUTd(0);         /* InitialFrames */
	AVIOUTd(1000000);   /* Scale */

	/* Rate: Rate/Scale == samples/second */
	AVIOUTd((uint32_t)(1000000 * video.frames_per_second));
	AVIOUTd(0);            /* Start */
	AVIOUTd(video.frames); /* Length */
	AVIOUTd(0);            /* SuggestedBufferSize */
	AVIOUTd(~0);           /* Quality */
	AVIOUTd(0);            /* SampleSize */
	AVIOUTd(0);            /* Frame */
	AVIOUTd(0);            /* Frame */

	// The video stream format
	AVIOUT4("strf");
	AVIOUTd(40);                                  /* # of bytes to follow */
	AVIOUTd(40);                                  /* Size */
	AVIOUTd(static_cast<uint32_t>(video.width));  /* Width */
	AVIOUTd(static_cast<uint32_t>(video.height)); /* Height */
	//		OUTSHRT(1); OUTSHRT(24);     /* Planes, Count */
	AVIOUTd(0);
	AVIOUT4(CODEC_4CC); /* Compression */
	AVIOUTd(video.width * video.height *
	        4); /* SizeImage (in
	                                                           bytes?) */
	AVIOUTd(0); /* XPelsPerMeter */
	AVIOUTd(0); /* YPelsPerMeter */
	AVIOUTd(0); /* ClrUsed: Number of colors used */
	AVIOUTd(0); /* ClrImportant: Number of colors important */

	// Audio stream list
	AVIOUT4("LIST");
	AVIOUTd(4 + 8 + 56 + 8 + 16); /* Length of list in bytes */
	AVIOUT4("strl");

	// The audio stream header
	AVIOUT4("strh");
	AVIOUTd(56); /* # of bytes to follow */
	AVIOUT4("auds");
	AVIOUTd(0); /* Format (Optionally) */
	AVIOUTd(0); /* Flags */
	AVIOUTd(0); /* Reserved, MS says: wPriority, wLanguage */
	AVIOUTd(0); /* InitialFrames */
	AVIOUTd(4); /* Scale */

	/* Rate, actual rate is scale/rate */
	AVIOUTd(video.audio.sample_rate * SampleFrameSizeBytes);
	AVIOUTd(0); /* Start */

	if (!video.audio.sample_rate) {
		video.audio.sample_rate = 1;
	}

	AVIOUTd(video.audio.bytes_written / SampleFrameSizeBytes); /* Length */
	AVIOUTd(0);               /* SuggestedBufferSize */
	AVIOUTd(~0);              /* Quality */
	AVIOUTd(SampleFrameSizeBytes); /* SampleSize */
	AVIOUTd(0);               /* Frame */
	AVIOUTd(0);               /* Frame */

	// The audio stream format
	AVIOUT4("strf");
	AVIOUTd(16);                      /* # of bytes to follow */
	AVIOUTw(1);                       /* Format, WAVE_ZMBV_FORMAT_PCM */
	AVIOUTw(2);                       /* Number of channels */
	AVIOUTd(video.audio.sample_rate); /* SamplesPerSec */
	AVIOUTd(video.audio.sample_rate * SampleFrameSizeBytes); /* AvgBytesPerSec*/
	AVIOUTw(4);                                         /* BlockAlign */
	AVIOUTw(16);                                        /* BitsPerSample */

	int nmain = header_pos - main_list - 4;

	// Finish stream list, i.e. put number of bytes in the list to
	// proper pos
	int njunk = AviHeaderSize - 8 - 12 - header_pos;
	AVIOUT4("JUNK");
	AVIOUTd(njunk);

	// Fix the size of the main list
	header_pos = main_list;
	AVIOUTd(nmain);
	header_pos = AviHeaderSize - 12;
	AVIOUT4("LIST");

	// Length of list in bytes
	AVIOUTd(video.written + 4);
	AVIOUT4("movi");

	// First add the index table to the end
	memcpy(video.index.data(), "idx1", 4);
	host_writed(video.index.data() + 4, video.index_used - 8);
	fwrite(video.index.data(), 1, video.index_used, video.handle);

	fseek(video.handle, 0, SEEK_SET);
	fwrite(&avi_header, 1, AviHeaderSize, video.handle);

	fclose(video.handle);
	delete video.codec;
	video.handle = nullptr;
}

// Buffers freshly captured audio delivered by the capture pipeline until
// `capture_video_add_frame()` meters it back out one video frame at a time. The
// buffer is fixed-size: if it is already full we copy only what fits and drop
// the remainder -- a last-resort overflow valve that the resync shedding (see
// the `AudioResync*` constants) is designed to keep us clear of.
void capture_video_add_audio_data(const uint32_t sample_rate,
                                  const uint32_t num_sample_frames,
                                  const int16_t* sample_frames)
{
	if (!video.handle) {
		return;
	}

	// Take the smaller of the buffer's free space and the incoming count;
	// any excess beyond the free space is dropped.
	auto frames_left = NumSampleFramesInBuffer - video.audio.num_buffered_frames;
	if (frames_left > num_sample_frames) {
		frames_left = num_sample_frames;
	}

	memcpy(&video.audio.buf[video.audio.num_buffered_frames],
	       sample_frames,
	       frames_left * SampleFrameSizeBytes);

	video.audio.num_buffered_frames += frames_left;
	video.audio.sample_rate = sample_rate;
}

static void create_avi_file(const uint16_t width, const uint16_t height,
                            const PixelFormat pixel_format,
                            const float frames_per_second, ZMBV_FORMAT format)
{
	video.handle = CAPTURE_CreateFile(CaptureType::Video);
	if (!video.handle) {
		return;
	}
	video.codec = new VideoCodec();
	if (!video.codec->SetupCompress(width, height)) {
		return;
	}

	video.buf_size = video.codec->NeededSize(width, height, format);
	video.buf.resize(video.buf_size);

	video.index.resize(16 * 4096);
	video.index_used = 8;

	video.width             = width;
	video.height            = height;
	video.pixel_format      = pixel_format;
	video.frames_per_second = frames_per_second;

	for (auto i = 0; i < AviHeaderSize; ++i) {
		fputc(0, video.handle);
	}

	video.frames  = 0;
	video.written = 0;

	video.audio.num_buffered_frames           = 0;
	video.audio.bytes_written                 = 0;
	video.audio.frame_credit                  = 0.0;
	video.audio.is_primed                     = false;
	video.audio.num_video_frames_since_resync = 0;
}

// Performs some transforms on the passed down rendered image to make sure
// we're capturing the raw output, then passes the result in the same
// byte-order to the encoder. Endianness varies per pixel format (see
// PixelFormat in video.h for details); the ZMBV encoder handles all that
// detail.
//
// We always write non-double-scanned and non-pixel-doubled frames in raw
// video capture mode :
//
// Double scanning and pixel doubling can be either "baked-in" or performed in
// post-processing. Ignoring the `double_height` and `double_width` flags
// takes care of the post-processing variety, but for the "baked-in" variants
// we need to skip every second row or pixel.
//
// So, for example, the 320x200 13h VGA mode is always written as 320x200 in
// raw capture mode regardless of the state of the width & height doubling
// flags, and irrespective of whether the passed down image is 320x200 or
// 320x400 with "baked-in" double scanning.
//
// Composite modes are special because they're rendered at 2x the width of the
// video mode to allow enough vertical resolution to represent composite
// artifacts (so 320x200 is rendered as 640x200, and 640x200 as 1280x200).
// These are written as-is, otherwise we'd be losing information.
//
static void compress_raw_frame(const RenderedImage& image)
{
	const auto& src = image.params;
	auto src_row    = image.image_data;

	// To reconstruct the raw image, we must skip every second row
	// when dealing with "baked-in" double scanning.
	const auto raw_height = (src.height / (src.rendered_double_scan ? 2 : 1));
	const auto src_pitch = (image.pitch * (src.rendered_double_scan ? 2 : 1));

	// To reconstruct the raw image, we must skip every second pixel
	// when dealing with "baked-in" pixel doubling.
	const auto raw_width = (src.width / (src.rendered_pixel_doubling ? 2 : 1));

	const auto pixel_skip_count = (src.rendered_pixel_doubling ? 1 : 0);

	auto compress_row = [&](const uint8_t* row_buffer) {
		video.codec->CompressLines(1, &row_buffer);
	};

	const auto src_bpp = to_bytes_per_pixel(src.pixel_format);
	const auto dest_bpp = to_bytes_per_pixel(to_zmbv_format(src.pixel_format));

	// Maybe compress the source rows straight away without copying. Note
	// that this is a shortcut scenario; hard-code it to false to exercise
	// the rote version below.

	const auto can_use_src_directly = (src_bpp == dest_bpp &&
	                                   pixel_skip_count == 0);
	if (can_use_src_directly) {
		for (auto i = 0; i < raw_height; ++i, src_row += src_pitch) {
			compress_row(src_row);
		}
		return;
	}

	// Otherwise we need to arrange the source bytes before compressing
	assert(!can_use_src_directly);

	// Grow the target row buffer if needed
	const auto dest_row_bytes = check_cast<uint16_t>(src.width * dest_bpp);
	static std::vector<uint8_t> dest_row = {};
	if (dest_row.size() < dest_row_bytes) {
		dest_row.resize(dest_row_bytes, 0);
	}

	const auto src_advance = src_bpp * (pixel_skip_count + 1);

	for (auto i = 0; i < raw_height; ++i, src_row += src_pitch) {
		auto src_pixel  = src_row;
		auto dest_pixel = dest_row.data();

		for (auto j = 0; j < raw_width; ++j, src_pixel += src_advance) {
			std::memcpy(dest_pixel, src_pixel, src_bpp);
			dest_pixel += dest_bpp;
		}
		compress_row(dest_row.data());
	}
}

void capture_video_add_frame(const RenderedImage& image, const float frames_per_second)
{
	const auto& src = image.params;
	assert(src.width <= RenderMaxWidth);

	// To reconstruct the raw image, we must skip every second row when
	// dealing with "baked-in" double scanning.
	const auto raw_width = check_cast<uint16_t>(
	        src.width / (src.rendered_pixel_doubling ? 2 : 1));

	// To reconstruct the raw image, we must skip every second pixel
	// when dealing with "baked-in" pixel doubling.
	const auto raw_height = check_cast<uint16_t>(
	        src.height / (src.rendered_double_scan ? 2 : 1));

	// Disable capturing if any of the test fails
	if (video.handle && (video.width != raw_width || video.height != raw_height ||
	                     video.pixel_format != src.pixel_format ||
	                     video.frames_per_second != frames_per_second)) {
		capture_video_finalise();
	}

	const auto zmbv_format = to_zmbv_format(src.pixel_format);

	if (!video.handle) {
		create_avi_file(raw_width,
		                raw_height,
		                src.pixel_format,
		                frames_per_second,
		                zmbv_format);
	}
	if (!video.handle) {
		return;
	}

	const auto codec_flags = (video.frames % 300 == 0) ? 1 : 0;

	static uint8_t palette_data[NumVgaColors * 4] = {};

	for (auto i = 0; i < NumVgaColors; ++i) {
		const auto color = image.palette[i];

		palette_data[i * 4]     = color.red;
		palette_data[i * 4 + 1] = color.green;
		palette_data[i * 4 + 2] = color.blue;
	}

	if (!video.codec->PrepareCompressFrame(codec_flags,
	                                       zmbv_format,
	                                       palette_data,
	                                       video.buf.data(),
	                                       video.buf_size)) {
		return;
	}

	compress_raw_frame(image);

	const auto written = video.codec->FinishCompressFrame();
	if (written < 0) {
		return;
	}

	add_avi_chunk("00dc", written, video.buf.data(), codec_flags & 1 ? 0x10 : 0x0);
	video.frames++;

	// Meter audio out evenly across video frames instead of dumping every
	// sample that arrived since the last frame into one `01wb` chunk. We
	// hold back a reserve (`AudioPrebufferFrames`) and emit a fixed
	// per-frame target, so bursty producer output is smoothed into uniform
	// chunks.
	if (!video.audio.is_primed) {
		if (video.audio.num_buffered_frames < AudioPrebufferFrames) {
			// Still building the reserve; nothing to emit yet.
			return;
		}
		video.audio.is_primed = true;
	}

	// One video frame spans `1 / frames_per_second` seconds, which at the
	// capture sample rate is this many audio sample frames. Accumulate it
	// as a running credit and emit whole frames only, carrying the
	// fractional remainder forward so the audio rate stays exact over time.
	video.audio.frame_credit += static_cast<double>(video.audio.sample_rate) /
	                            video.frames_per_second;

	int num_audio_frames_to_write = ifloor(video.audio.frame_credit);

	// Underrun (reserve exhausted by a long producer stall): emit whatever
	// is available rather than padding with silence, keeping the captured
	// stream bit-identical to what was produced. The unmet demand stays in
	// `frame_credit` and drains once the producer catches up.
	if (num_audio_frames_to_write >
	    static_cast<int>(video.audio.num_buffered_frames)) {
		num_audio_frames_to_write = static_cast<int>(
		        video.audio.num_buffered_frames);
	}

	video.audio.frame_credit -= num_audio_frames_to_write;
	write_audio_chunk(num_audio_frames_to_write);

	// Shed accumulated surplus to keep the captured audio locked to the
	// (emulated-time) video timeline. See the `AudioResync*` constants.
	++video.audio.num_video_frames_since_resync;

	// The surplus is whatever sits above the steady-state reserve; only this
	// excess is eligible to be shed (the reserve itself is never touched).
	const int num_surplus_frames =
	        (video.audio.num_buffered_frames > AudioPrebufferFrames)
	                ? static_cast<int>(video.audio.num_buffered_frames -
	                                   AudioPrebufferFrames)
	                : 0;

	// Force an early shed well before the buffer's hard capacity (at around
	// ~75% capacity) so a very slow host can't overrun it (and hit the drop
	// in `capture_video_add_audio_data()`) between scheduled sheds.
	const auto is_buffer_near_full = video.audio.num_buffered_frames >
	                                 (NumSampleFramesInBuffer * 3) / 4;

	const auto is_resync_due = video.audio.num_video_frames_since_resync >=
	                           AudioResyncIntervalVideoFrames;

	if (num_surplus_frames >= AudioResyncMinSurplusFrames &&
	    (is_resync_due || is_buffer_near_full)) {

		[[maybe_unused]] const int num_video_frames_waited =
		        video.audio.num_video_frames_since_resync;

		drop_audio_surplus_with_crossfade(num_surplus_frames);
		video.audio.num_video_frames_since_resync = 0;

		LOG_DEBUG("CAPTURE: Audio resync after %d frames%s -- shed %d sample frames, buffer now %u (target %d)",
		          num_video_frames_waited,
		          is_buffer_near_full ? " (buffer pressure)" : "",
		          num_surplus_frames,
		          video.audio.num_buffered_frames,
		          AudioPrebufferFrames);
	}
}
