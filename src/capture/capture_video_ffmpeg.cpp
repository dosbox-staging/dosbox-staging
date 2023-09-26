/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
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

#include "capture.h"
#include "capture_video.h"
#include "checks.h"
#include "math_utils.h"
#include "timer.h"
#include "image/image_decoder.h"

// GCC and Clang define this macro
#ifdef __SSE2__
#define HAVE_SSE2
#endif

// MSVC is a pain in the ass and needs all this:
// https://learn.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=msvc-170

// This macro means SSE2 support but is only defined for 32 bit
#if _M_IX86_FP == 2
#define HAVE_SSE2
#endif

// All 64 bit x86 CPUs support SSE2
#ifdef _M_X64
#define HAVE_SSE2
#endif

// Unless you're on this weird emulated ARM target that defines x86 macros for some reason
#ifdef _M_ARM64EC
#undef HAVE_SSE2
#endif

#ifdef HAVE_SSE2
#include <emmintrin.h>
#endif

#if C_FFMPEG

CHECK_NARROWING();

// Disable deprecated warnigns coming from ffmpeg libraries.
// I'm trying to support as many versions as possible.
// Some of the CI machines don't support the new functions.
// And some give deprecated warnings on the new functions.
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#else
// This also works on Clang
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

// These get added to an array in muxer.format_context->streams
// In the order they get initalized with avformat_new_stream()
constexpr int MuxerVideoStreamIndex = 0;
constexpr int MuxerAudioStreamIndex = 1;

// Always stereo audio
constexpr size_t SamplesPerFrame = 2;

// Used by av_frame_get_buffer
// 0 means auto-align based on current CPU
constexpr int MemoryAlignment = 0;

FfmpegEncoder::FfmpegEncoder(const Section_prop* secprop)
{
	if (secprop->Get_string("ffmpeg_container") == std::string("mp4")) {
		muxer.container = CaptureType::VideoMp4;
	} else {
		muxer.container = CaptureType::VideoMkv;
	}
	if (secprop->Get_string("ffmpeg_audio_codec") == std::string("flac")) {
		audio_encoder.requested_codec = AudioCodec::FLAC;
	} else {
		audio_encoder.requested_codec = AudioCodec::AAC;
	}
	std::string quality = secprop->Get_string("ffmpeg_quality");
	if (quality == "lossless") {
		video_encoder.crf = 0;
	} else if (quality == "medium") {
		video_encoder.crf = 23;
	} else if (quality == "low") {
		video_encoder.crf = 30;
	} else {
		// Default to high
		video_encoder.crf = 18;
	}
	video_encoder.max_vertical_resolution = secprop->Get_int("ffmpeg_resolution");

	video_scaler.thread  = std::thread(&FfmpegEncoder::ScaleVideo, this);
	set_thread_name(video_scaler.thread, "dosbox:scaler");

	audio_encoder.thread = std::thread(&FfmpegEncoder::EncodeAudio, this);
	set_thread_name(audio_encoder.thread, "dosbox:audioenc");

	video_encoder.thread = std::thread(&FfmpegEncoder::EncodeVideo, this);
	set_thread_name(video_encoder.thread, "dosbox:videoenc");

	muxer.thread         = std::thread(&FfmpegEncoder::Mux, this);
	set_thread_name(audio_encoder.thread, "dosbox:muxer");
}

FfmpegEncoder::~FfmpegEncoder()
{
	mutex.lock();
	is_shutting_down = true;
	mutex.unlock();
	waiter.notify_all();

	StopQueues();

	if (video_scaler.thread.joinable()) {
		video_scaler.thread.join();
	}
	if (audio_encoder.thread.joinable()) {
		audio_encoder.thread.join();
	}
	if (video_encoder.thread.joinable()) {
		video_encoder.thread.join();
	}
	if (muxer.thread.joinable()) {
		muxer.thread.join();
	}
}

static AVFrame *init_audio_frame(const AVCodecContext* codec_context, const int64_t pts)
{
	AVFrame* frame = av_frame_alloc();
	if (!frame) {
		LOG_ERR("FFMPEG: Failed to allocate audio frame");
		return nullptr;
	}

	frame->format = static_cast<int>(codec_context->sample_fmt);
	frame->nb_samples = codec_context->frame_size;
	frame->sample_rate = codec_context->sample_rate;
	frame->pts = pts;
	frame->channel_layout = codec_context->channel_layout;

	if (av_frame_get_buffer(frame, MemoryAlignment) < 0) {
		LOG_ERR("FFMPEG: Failed to get audio frame buffer");
		av_frame_free(&frame);
		return nullptr;
	}

	frame->nb_samples = 0;

	return frame;
}

bool FfmpegEncoder::InitEverything()
{
	if (!video_encoder.Init()) {
		LOG_ERR("FFMPEG: Failed to init video encoder");
		return false;
	}
	if (!audio_encoder.Init()) {
		LOG_ERR("FFMPEG: Failed to init audio encoder");
		return false;
	}
	if (!muxer.Init(video_encoder, audio_encoder)) {
		LOG_ERR("FFMPEG: Failed to init muxer");
		return false;
	}
	main_thread_audio_frame = init_audio_frame(audio_encoder.codec_context, 0);
	if (!main_thread_audio_frame) {
		LOG_ERR("FFMPEG: Failed to initalise audio frame");
		return false;
	}
	main_thread_video_pts = 0;
	mutex.lock();
	worker_threads_are_initalised = true;
	mutex.unlock();
	waiter.notify_all();
	return true;
}

void FfmpegEncoder::FreeEverything()
{
	muxer.Free();
	audio_encoder.Free();
	video_encoder.Free();
	if (main_thread_audio_frame) {
		av_frame_free(&main_thread_audio_frame);
		main_thread_audio_frame = nullptr;
	}
}

bool FfmpegVideoEncoder::UpdateSettingsIfNeeded(uint16_t width, uint16_t height, Fraction pixel_aspect_ratio, int frames_per_second)
{
	if (this->width != width || this->height != height || this->pixel_aspect_ratio != pixel_aspect_ratio || this->frames_per_second != frames_per_second) {
		this->width = width;
		this->height = height;
		this->pixel_aspect_ratio = pixel_aspect_ratio;
		this->frames_per_second = frames_per_second;
		return true;
	}
	return false;
}

void FfmpegEncoder::CaptureVideoAddFrame(const RenderedImage& image,
                                         const float frames_per_second)
{
	const int rounded_fps = iroundf(frames_per_second);
	const bool video_settings_changed = video_encoder.UpdateSettingsIfNeeded(image.params.width, image.params.height, image.params.video_mode.pixel_aspect_ratio, rounded_fps);

	if (worker_threads_are_initalised) {
		if (video_settings_changed) {
			CaptureVideoFinalise();
		}
	} else if (audio_encoder.ready_for_init) {
		if (!InitEverything()) {
			FreeEverything();
		}
	}

	video_encoder.ready_for_init = true;

	if (!worker_threads_are_initalised) {
		return;
	}

	VideoScalerWork work;
	work.pts = main_thread_video_pts++;
	work.image = image.deep_copy();
	if (!video_scaler.queue.MaybeEnqueue(std::move(work))) {
		work.image.free();
	}
}

static void write_audio_to_frame(const int16_t* audio_data, AVFrame *frame, const uint32_t num_frames)
{
	switch(frame->format) {
	case AV_SAMPLE_FMT_FLTP: {
		float* left_channel = reinterpret_cast<float*>(frame->data[0]) + frame->nb_samples;
		float* right_channel = reinterpret_cast<float*>(frame->data[1]) + frame->nb_samples;
		for (uint32_t frame = 0; frame < num_frames; ++frame) {
			const size_t sample = static_cast<size_t>(frame) * SamplesPerFrame;
			left_channel[frame] = static_cast<float>(audio_data[sample]) / 32768.0f;
			right_channel[frame] = static_cast<float>(audio_data[sample + 1]) / 32768.0f;
		}
		break;
	}
	case AV_SAMPLE_FMT_S16: {
		int16_t* dest = reinterpret_cast<int16_t*>(frame->data[0]) + (static_cast<size_t>(frame->nb_samples) * SamplesPerFrame);
		memcpy(dest, audio_data, num_frames * SamplesPerFrame * sizeof(int16_t));
		break;
	}
	default: {
		assertm(false, "Invalid audio sample format");
	}
	}
	frame->nb_samples += static_cast<int>(num_frames);
}

void FfmpegEncoder::CaptureVideoAddAudioData(const uint32_t sample_rate,
                                             const uint32_t num_sample_frames,
                                             const int16_t* sample_frames)
{
	const bool sample_rate_changed = audio_encoder.sample_rate != sample_rate;
	audio_encoder.sample_rate = sample_rate;

	if (worker_threads_are_initalised) {
		if (sample_rate_changed) {
			CaptureVideoFinalise();
		}
	} else if (video_encoder.ready_for_init) {
		if (!InitEverything()) {
			FreeEverything();
		}
	}

	audio_encoder.ready_for_init = true;

	if (!worker_threads_are_initalised) {
		return;
	}

	uint32_t remaining = num_sample_frames;
	while (remaining > 0) {
		const uint32_t space_left = static_cast<uint32_t>(audio_encoder.codec_context->frame_size) - static_cast<uint32_t>(main_thread_audio_frame->nb_samples);
		const uint32_t num_frames_to_write = std::min(space_left, remaining);
		write_audio_to_frame(sample_frames, main_thread_audio_frame, num_frames_to_write);
		if (main_thread_audio_frame->nb_samples == audio_encoder.codec_context->frame_size) {
			const int64_t pts = main_thread_audio_frame->pts + main_thread_audio_frame->nb_samples;
			audio_encoder.queue.Enqueue(std::move(main_thread_audio_frame));
			main_thread_audio_frame = init_audio_frame(audio_encoder.codec_context, pts);
		}
		remaining -= num_frames_to_write;
		sample_frames += num_frames_to_write * SamplesPerFrame;
	}
}

void FfmpegEncoder::StopQueues()
{
	// CaptureVideoAddAudioData() gathers up the data into an AVFrame
	// It enqueues once it reaches capacity (usually around 1024 audio frames per AVFrame)
	// The last (and only the last) AVFrame may be less than capacity.
	// Go ahead and enqueue that partially filled AVFrame here.
	if (main_thread_audio_frame) {
		assert(worker_threads_are_initalised);
		if (main_thread_audio_frame->nb_samples > 0) {
			audio_encoder.queue.Enqueue(std::move(main_thread_audio_frame));
		} else {
			av_frame_free(&main_thread_audio_frame);
		}
		main_thread_audio_frame = nullptr;
	}

	std::unique_lock<std::mutex> lock(mutex);

	// Set this first so none of the threads start another iteration
	// Before the encoders get re-initalised
	worker_threads_are_initalised = false;

	audio_encoder.ready_for_init = false;
	video_encoder.ready_for_init = false;

	video_scaler.queue.Stop();
	waiter.wait(lock, [this] {
		return !video_scaler.is_working;
	});

	audio_encoder.queue.Stop();
	video_encoder.queue.Stop();
	waiter.wait(lock, [this] {
		return !audio_encoder.is_working && !video_encoder.is_working;
	});
	muxer.queue.Stop();
	waiter.wait(lock, [this] { return !muxer.is_working; });

	lock.unlock();

	assert(video_scaler.queue.IsEmpty());
	assert(video_encoder.queue.IsEmpty());
	assert(audio_encoder.queue.IsEmpty());
	assert(muxer.queue.IsEmpty());

	FreeEverything();
}

void FfmpegEncoder::StartQueues()
{
	video_scaler.queue.Start();
	audio_encoder.queue.Start();
	video_encoder.queue.Start();
	muxer.queue.Start();
}

void FfmpegEncoder::CaptureVideoFinalise()
{
	StopQueues();
	StartQueues();
}

bool FfmpegMuxer::Init(const FfmpegVideoEncoder& video_encoder,
                       const FfmpegAudioEncoder& audio_encoder)
{
	const int32_t output_file_index = get_next_capture_index(container);
	const std::string output_file_path =
	        generate_capture_filename(container, output_file_index)
	                .string();

	// Only one of these needs to be specified. We're using filename.
	constexpr AVOutputFormat* oformat = nullptr;
	constexpr char* format_name       = nullptr;
	avformat_alloc_output_context2(&format_context,
	                               oformat,
	                               format_name,
	                               output_file_path.c_str());
	if (!format_context) {
		LOG_ERR("FFMPEG: Failed to allocate format context");
		return false;
	}
	AVStream* video_stream = avformat_new_stream(format_context,
	                                             video_encoder.av_codec);
	if (!video_stream) {
		LOG_ERR("FFMPEG: Failed to create video stream");
		return false;
	}
	video_stream->time_base = video_encoder.codec_context->time_base;
	video_stream->sample_aspect_ratio = video_encoder.codec_context->sample_aspect_ratio;

	if (avcodec_parameters_from_context(video_stream->codecpar,
	                                    video_encoder.codec_context) < 0) {
		LOG_ERR("FFMPEG: Failed to copy video codec parameters to stream");
		return false;
	}

	AVStream* audio_stream = avformat_new_stream(format_context,
	                                             audio_encoder.av_codec);
	if (!audio_stream) {
		LOG_ERR("FFMPEG: Failed to create audio stream");
		return false;
	}
	audio_stream->time_base.num = 1;
	audio_stream->time_base.den = audio_encoder.codec_context->sample_rate;

	if (avcodec_parameters_from_context(audio_stream->codecpar,
	                                    audio_encoder.codec_context) < 0) {
		LOG_ERR("FFMPEG: Failed to copy video codec parameters to stream");
		return false;
	}

	if (avio_open(&format_context->pb, output_file_path.c_str(), AVIO_FLAG_WRITE) <
	    0) {
		LOG_ERR("FFMPEG: Failed to create capture file '%s'",
		        output_file_path.c_str());
		return false;
	}

	if (avformat_write_header(format_context, nullptr) < 0) {
		LOG_ERR("FFMPEG: Failed to write header");
		return false;
	}

	assert(video_stream == format_context->streams[MuxerVideoStreamIndex]);
	assert(audio_stream == format_context->streams[MuxerAudioStreamIndex]);

	LOG_MSG("FFMPEG: Video capture started to '%s'", output_file_path.c_str());

	return true;
}

void FfmpegMuxer::Free()
{
	if (format_context) {
		avio_close(format_context->pb);
		format_context->pb = nullptr;
		avformat_free_context(format_context);
		format_context = nullptr;
	}
}

static bool codec_supports_format(const AVCodec* codec, const AVSampleFormat requested)
{
	const AVSampleFormat* format = codec->sample_fmts;
	while (*format != -1) {
		if (*format == requested) {
			return true;
		}
		++format;
	}
	return false;
}

// From testing on my system, flac supports S16 (interleaved int16_t) and does not support float
// AAC only supports FLTP (floating point planar format, not interleaved)
// Try S16 first because that is what comes in as input
// Re-vist this when Dosbox changes audio formats to float
static AVSampleFormat find_best_audio_format(const AVCodec *codec)
{
	if (codec_supports_format(codec, AV_SAMPLE_FMT_S16)) {
		return AV_SAMPLE_FMT_S16;
	}
	if (codec_supports_format(codec, AV_SAMPLE_FMT_FLTP)) {
		return AV_SAMPLE_FMT_FLTP;
	}
	return AV_SAMPLE_FMT_NONE;
}

bool FfmpegAudioEncoder::Init()
{
	switch (requested_codec) {
	case AudioCodec::AAC:
		av_codec = avcodec_find_encoder_by_name("aac");
		break;
	case AudioCodec::FLAC:
		av_codec = avcodec_find_encoder_by_name("flac");
		break;
	default:
		av_codec = nullptr;
	}

	if (!av_codec) {
		LOG_ERR("FFMPEG: Failed to find audio codec");
		return false;
	}

	const AVSampleFormat format = find_best_audio_format(av_codec);
	if (format == AV_SAMPLE_FMT_NONE) {
		// If we hit this from some new audio codec, it means we need to write
		// a new conversion routine in write_audio_to_frame().
		LOG_ERR("FFMPEG: No conversion routine for audio codec's supported format");
		return false;
	}

	codec_context = avcodec_alloc_context3(av_codec);
	if (!codec_context) {
		LOG_ERR("FFMPEG: Failed to allocate audio context");
		return false;
	}

	codec_context->sample_fmt     = format;
	codec_context->sample_rate    = static_cast<int>(sample_rate);
	codec_context->channel_layout = AV_CH_LAYOUT_STEREO;

	if (avcodec_open2(codec_context, av_codec, nullptr) < 0) {
		LOG_ERR("FFMPEG: Failed to open audio context");
		return false;
	}

	frame = av_frame_alloc();
	if (!frame) {
		LOG_ERR("FFMPEG: Failed to allocate audio frame");
		return false;
	}
	frame->format         = static_cast<int>(codec_context->sample_fmt);
	frame->nb_samples     = codec_context->frame_size;
	frame->sample_rate    = codec_context->sample_rate;
	frame->pts            = 0;
	frame->channel_layout = codec_context->channel_layout;
	// 0 means auto-align based on current CPU
	constexpr int memory_alignment = 0;
	if (av_frame_get_buffer(frame, memory_alignment) < 0) {
		LOG_ERR("FFMPEG: Failed to get audio frame buffer");
		return false;
	}

	return true;
}

void FfmpegAudioEncoder::Free()
{
	if (frame) {
		av_frame_free(&frame);
		frame = nullptr;
	}
	if (codec_context) {
		avcodec_free_context(&codec_context);
		codec_context = nullptr;
	}
}

bool FfmpegVideoEncoder::Init()
{
	av_codec = avcodec_find_encoder_by_name("libx264");
	if (!av_codec) {
		LOG_ERR("FFMPEG: Failed to find libx264 encoder");
		return false;
	}
	codec_context = avcodec_alloc_context3(av_codec);
	if (!codec_context) {
		LOG_ERR("FFMPEG: Failed to allocate video context");
		return false;
	}

	int scale_factor = std::max(max_vertical_resolution / height, 1);

	// Round down to nearest multiple of 2
	// Scaling alogirthim is much faster this way when converting to YuV420
	if (scale_factor > 1) {
		scale_factor -= scale_factor % 2;
	}

	codec_context->width         = width * scale_factor;
	codec_context->height        = height * scale_factor;
	codec_context->time_base.num = 1;
	codec_context->time_base.den = frames_per_second;
	if (scale_factor == 1) {
		codec_context->pix_fmt = AV_PIX_FMT_YUV444P;
	} else {
		codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
	}
	codec_context->sample_aspect_ratio.num = static_cast<int>(
	        pixel_aspect_ratio.Num());
	codec_context->sample_aspect_ratio.den = static_cast<int>(
	        pixel_aspect_ratio.Denom());

	// This flag is required for Matroska (MKV)
	// It's also required for MP4 when using FLAC
	// It doesn't seem to have any adverse side effects so just turn it on all the time.
	codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	AVDictionary *options = nullptr;

	// Constant rate factor determining video quality. 0 means lossless.
	av_dict_set(&options, "crf", std::to_string(crf).c_str(), 0);

	// Encoding speed vs compression rate. Does not affect quality.
	av_dict_set(&options, "preset", "medium", 0);

	if (avcodec_open2(codec_context, av_codec, &options) < 0) {
		LOG_ERR("FFMPEG: Failed to open video context");
		return false;
	}

	av_dict_free(&options);

	return true;
}

void FfmpegVideoEncoder::Free()
{
	if (codec_context) {
		avcodec_free_context(&codec_context);
		codec_context = nullptr;
	}
}

static void send_packets_to_muxer(AVCodecContext* context, int stream_index,
                                  RWQueue<AVPacket*>& queue)
{
	int packet_received;
	do {
		AVPacket* packet = av_packet_alloc();
		if (!packet) {
			LOG_ERR("FFMPEG: Failed to allocate audio packet");
			return;
		}
		packet_received = avcodec_receive_packet(context, packet);
		if (packet_received == 0) {
			packet->stream_index = stream_index;
			[[maybe_unused]] bool packet_queued = queue.Enqueue(std::move(packet));
			assert(packet_queued);
		} else {
			av_packet_free(&packet);
		}
	} while (packet_received == 0);

	// These are two legitimate errors that don't need to be logged.
	// EAGAIN meaning there's no more packets for us right now.
	// AVERROR_EOF happens once when we flush the encoders.
	if (packet_received != AVERROR(EAGAIN) && packet_received != AVERROR_EOF) {
		LOG_ERR("FFMPEG: Receive packet failed");
	}
}

#ifdef HAVE_SSE2

static void scale_image(const RenderedImage& image, AVFrame* frame)
{
	size_t horizontal_scale = static_cast<size_t>(frame->width) / image.params.width;
	size_t vertical_scale = static_cast<size_t>(frame->height) / image.params.height;

	size_t scaled_width = image.params.width * horizontal_scale;

	size_t uv_horizontal_scale = horizontal_scale;
	size_t uv_vertical_scale = vertical_scale;
	size_t uv_width = scaled_width;
	if (frame->format == AV_PIX_FMT_YUV420P) {
		uv_horizontal_scale /= 2;
		uv_vertical_scale /= 2;
		uv_width /= 2;
	}

	uint8_t* y_row = frame->data[0];
	uint8_t* cr_row = frame->data[2];
	uint8_t* cb_row = frame->data[1];
	int y_pitch = frame->linesize[0];
	int cr_pitch = frame->linesize[2];
	int cb_pitch = frame->linesize[1];

	uint8_t* src = image.image_data;
	for (uint16_t y = 0; y < image.params.height; ++y) {
		for (uint16_t x = 0; x < image.params.width; x += 4) {
			__m128i temp;

			__m128i in = _mm_loadu_si128(reinterpret_cast<__m128i*>(src + (x * 4)));

			temp = _mm_srli_epi32(in, 16);
			temp = _mm_and_si128(temp, _mm_set1_epi32(0xFF));
			__m128 red = _mm_cvtepi32_ps(temp);

			temp = _mm_srli_epi32(in, 8);
			temp = _mm_and_si128(temp, _mm_set1_epi32(0xFF));
			__m128 green = _mm_cvtepi32_ps(temp);

			temp = _mm_and_si128(in, _mm_set1_epi32(0xFF));
			__m128 blue = _mm_cvtepi32_ps(temp);

			__m128 red_temp;
			__m128 green_temp;
			__m128 blue_temp;

			red_temp = _mm_mul_ps(_mm_set1_ps(0.257f), red);
			green_temp = _mm_mul_ps(_mm_set1_ps(0.504f), green);
			blue_temp = _mm_mul_ps(_mm_set1_ps(0.098f), blue);
			__m128 yf = _mm_add_ps(red_temp, green_temp);
			yf = _mm_add_ps(yf, blue_temp);
			yf = _mm_add_ps(yf, _mm_set1_ps(16.0f));

			red_temp = _mm_mul_ps(_mm_set1_ps(0.439f), red);
			green_temp = _mm_mul_ps(_mm_set1_ps(0.368f), green);
			blue_temp = _mm_mul_ps(_mm_set1_ps(0.071f), blue);
			__m128 crf = _mm_sub_ps(red_temp, green_temp);
			crf = _mm_sub_ps(crf, blue_temp);
			crf = _mm_add_ps(crf, _mm_set1_ps(128.0f));

			red_temp = _mm_mul_ps(_mm_set1_ps(0.148f), red);
			red_temp = _mm_sub_ps(_mm_setzero_ps(), red_temp);
			green_temp = _mm_mul_ps(_mm_set1_ps(0.291f), green);
			blue_temp = _mm_mul_ps(_mm_set1_ps(0.439f), blue);
			__m128 cbf = _mm_sub_ps(red_temp, green_temp);
			cbf = _mm_add_ps(cbf, blue_temp);
			cbf = _mm_add_ps(cbf, _mm_set1_ps(128.0f));

			__m128i y_out = _mm_cvtps_epi32(yf);
			y_out = _mm_packs_epi32(y_out, y_out);
			y_out = _mm_packus_epi16(y_out, y_out);
			y_out = _mm_unpacklo_epi8(y_out, y_out);

			__m128i cr_out = _mm_cvtps_epi32(crf);
			cr_out = _mm_packs_epi32(cr_out, cr_out);
			cr_out = _mm_packus_epi16(cr_out, cr_out);

			__m128i cb_out = _mm_cvtps_epi32(cbf);
			cb_out = _mm_packs_epi32(cb_out, cb_out);
			cb_out = _mm_packus_epi16(cb_out, cb_out);

			_mm_storeu_si64(y_row + (x * 2), y_out);
			_mm_storeu_si32(cr_row + x, cr_out);
			_mm_storeu_si32(cb_row + x, cb_out);
		}

		for (size_t i = 1; i < vertical_scale; ++i) {
			uint8_t* prev_row = y_row;
			y_row += y_pitch;
			memcpy(y_row, prev_row, scaled_width);
		}
		for (size_t i = 1; i < uv_vertical_scale; ++i) {
			uint8_t* prev_row = cr_row;
			cr_row += cr_pitch;
			memcpy(cr_row, prev_row, uv_width);
		}
		for (size_t i = 1; i < uv_vertical_scale; ++i) {
			uint8_t* prev_row = cb_row;
			cb_row += cb_pitch;
			memcpy(cb_row, prev_row, uv_width);
		}

		y_row += y_pitch;
		cr_row += cr_pitch;
		cb_row += cb_pitch;

		src += image.pitch;
	}
}

#else

static void scale_image(const RenderedImage& image, AVFrame* frame)
{
	size_t horizontal_scale = static_cast<size_t>(frame->width) / image.params.width;
	size_t vertical_scale = static_cast<size_t>(frame->height) / image.params.height;

	size_t scaled_width = image.params.width * horizontal_scale;

	size_t uv_horizontal_scale = horizontal_scale;
	size_t uv_vertical_scale = vertical_scale;
	size_t uv_width = scaled_width;
	if (frame->format == AV_PIX_FMT_YUV420P) {
		uv_horizontal_scale /= 2;
		uv_vertical_scale /= 2;
		uv_width /= 2;
	}

	uint8_t* y_row = frame->data[0];
	uint8_t* cr_row = frame->data[2];
	uint8_t* cb_row = frame->data[1];
	int y_pitch = frame->linesize[0];
	int cr_pitch = frame->linesize[2];
	int cb_pitch = frame->linesize[1];

	ImageDecoder image_decoder;
	image_decoder.Init(image, 0);
	for (uint16_t y = 0; y < image.params.height; ++y) {
		for (uint16_t x = 0; x < image.params.width; ++x) {
			Rgb888 src_pixel = image_decoder.GetNextPixelAsRgb888();
			float red = static_cast<float>(src_pixel.red);
			float green = static_cast<float>(src_pixel.green);
			float blue = static_cast<float>(src_pixel.blue);

			float yf = (0.257f * red) + (0.504f * green) + (0.098f * blue) + 16.0f;
			float crf = (0.439f * red) - (0.368f * green) - (0.071f * blue) + 128.0f;
			float cbf = -(0.148f * red) - (0.291f * green) + (0.439f * blue) + 128.0f;

			uint8_t y_out = static_cast<uint8_t>(clamp(yf, 0.0f, 255.0f));
			uint8_t cr_out = static_cast<uint8_t>(clamp(crf, 0.0f, 255.0f));
			uint8_t cb_out = static_cast<uint8_t>(clamp(cbf, 0.0f, 255.0f));

			memset(y_row + (x * horizontal_scale), y_out, horizontal_scale);
			memset(cr_row + (x * uv_horizontal_scale), cr_out, uv_horizontal_scale);
			memset(cb_row + (x * uv_horizontal_scale), cb_out, uv_horizontal_scale);
		}

		for (size_t i = 1; i < vertical_scale; ++i) {
			uint8_t* prev_row = y_row;
			y_row += y_pitch;
			memcpy(y_row, prev_row, scaled_width);
		}
		for (size_t i = 1; i < uv_vertical_scale; ++i) {
			uint8_t* prev_row = cr_row;
			cr_row += cr_pitch;
			memcpy(cr_row, prev_row, uv_width);
		}
		for (size_t i = 1; i < uv_vertical_scale; ++i) {
			uint8_t* prev_row = cb_row;
			cb_row += cb_pitch;
			memcpy(cb_row, prev_row, uv_width);
		}

		y_row += y_pitch;
		cr_row += cr_pitch;
		cb_row += cb_pitch;
		image_decoder.AdvanceRow();
	}
}

#endif

void FfmpegEncoder::ScaleVideo()
{
	for (;;) {
		std::unique_lock<std::mutex> lock(mutex);
		waiter.wait(lock, [this] {
			return worker_threads_are_initalised || is_shutting_down;
		});
		if (is_shutting_down) {
			return;
		}
		video_scaler.is_working = true;
		lock.unlock();

		while (auto optional = video_scaler.queue.Dequeue()) {
			RenderedImage& image = optional->image;

			AVFrame* frame = av_frame_alloc();
			if (!frame) {
				LOG_ERR("FFMPEG: Failed to allocate video frame");
				image.free();
				continue;
			}

			frame->width               = video_encoder.codec_context->width;
			frame->height              = video_encoder.codec_context->height;
			frame->format              = static_cast<int>(video_encoder.codec_context->pix_fmt);
			frame->pts                 = optional->pts;
			frame->sample_aspect_ratio = video_encoder.codec_context->sample_aspect_ratio;
			if (av_frame_get_buffer(frame, MemoryAlignment) < 0) {
				LOG_ERR("FFMPEG: Failed to get video frame buffer");
				av_frame_free(&frame);
				image.free();
				continue;
			}

			int64_t start = GetTicksUs();
			scale_image(image, frame);
			LOG_MSG("FFMPEG: Scaler: %ld", GetTicksUsSince(start));
			image.free();

			[[maybe_unused]] bool frame_queued = video_encoder.queue.Enqueue(std::move(frame));
			assert(frame_queued);
		}

		lock.lock();
		video_scaler.is_working = false;
		lock.unlock();
		waiter.notify_all();
	}
}

void FfmpegEncoder::EncodeVideo()
{
	for (;;) {
		std::unique_lock<std::mutex> lock(mutex);
		waiter.wait(lock, [this] {
			return worker_threads_are_initalised|| is_shutting_down;
		});
		if (is_shutting_down) {
			return;
		}
		video_encoder.is_working = true;
		lock.unlock();

		// Dequeue returns a false std::optional when the queue is stopped.
		while (auto optional = video_encoder.queue.Dequeue()) {
			AVFrame* frame = *optional;
			assert(frame->width == video_encoder.codec_context->width);
			assert(frame->height == video_encoder.codec_context->height);
			assert(frame->format == video_encoder.codec_context->pix_fmt);

			if (avcodec_send_frame(video_encoder.codec_context, frame) < 0) {
				LOG_ERR("FFMPEG: Failed to send video frame");
			}
			av_frame_free(&frame);
			send_packets_to_muxer(video_encoder.codec_context,
			                      MuxerVideoStreamIndex,
			                      muxer.queue);
		}

		// Queue has been stopped. Flush the encoder by sending nullptr
		// frame.
		assert(!video_encoder.queue.IsRunning());
		avcodec_send_frame(video_encoder.codec_context, nullptr);
		send_packets_to_muxer(video_encoder.codec_context,
		                      MuxerVideoStreamIndex,
		                      muxer.queue);

		lock.lock();
		video_encoder.is_working = false;
		lock.unlock();
		waiter.notify_all();
	}
}

void FfmpegEncoder::Mux()
{
	for (;;) {
		std::unique_lock<std::mutex> lock(mutex);
		waiter.wait(lock, [this] {
			return worker_threads_are_initalised || is_shutting_down;
		});
		if (is_shutting_down) {
			return;
		}
		muxer.is_working = true;
		lock.unlock();

		while (auto optional = muxer.queue.Dequeue()) {
			AVPacket* packet = *optional;
			AVRational encoder_time_base;
			if (packet->stream_index == MuxerVideoStreamIndex) {
				encoder_time_base = video_encoder.codec_context->time_base;
			} else {
				encoder_time_base = audio_encoder.codec_context->time_base;
			}
			av_packet_rescale_ts(packet,
			                     encoder_time_base,
			                     muxer.format_context
			                             ->streams[packet->stream_index]
			                             ->time_base);
			if (av_interleaved_write_frame(muxer.format_context,
			                               packet) < 0) {
				LOG_ERR("FFMPEG: Muxer failed to write frame");
			}
			av_packet_free(&packet);
		}

		// Pass nullptr to drain the muxer's internal buffer
		av_interleaved_write_frame(muxer.format_context, nullptr);
		av_write_trailer(muxer.format_context);
		lock.lock();
		muxer.is_working = false;
		lock.unlock();
		waiter.notify_all();
	}
}

void FfmpegEncoder::EncodeAudio()
{
	std::vector<int16_t> audio_data;

	for (;;) {
		std::unique_lock<std::mutex> lock(mutex);
		waiter.wait(lock, [this] {
			return worker_threads_are_initalised || is_shutting_down;
		});
		if (is_shutting_down) {
			return;
		}
		audio_encoder.is_working = true;
		lock.unlock();

		while (auto optional = audio_encoder.queue.Dequeue()) {
			AVFrame* frame = *optional;
			if (avcodec_send_frame(audio_encoder.codec_context, frame) < 0) {
				LOG_ERR("FFMPEG: Failed to send audio frame");
			}
			av_frame_free(&frame);
			send_packets_to_muxer(audio_encoder.codec_context,
			                      MuxerAudioStreamIndex,
			                      muxer.queue);
		}

		// Send nullptr frame to flush the encoder.
		avcodec_send_frame(audio_encoder.codec_context, nullptr);
		send_packets_to_muxer(audio_encoder.codec_context,
		                      MuxerAudioStreamIndex,
		                      muxer.queue);

		lock.lock();
		audio_encoder.is_working = false;
		lock.unlock();
		waiter.notify_all();
	}
}

#endif // C_FFMPEG
