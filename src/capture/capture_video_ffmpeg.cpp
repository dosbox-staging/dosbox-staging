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

constexpr int ScaledWidth = 1600;
constexpr int ScaledHeight = 1200;

constexpr AVPixelFormat OutputFormat = AV_PIX_FMT_YUV420P;

FfmpegEncoder::FfmpegEncoder()
{
	video_scaler.thread  = std::thread(&FfmpegEncoder::ScaleVideo, this);
	audio_encoder.thread = std::thread(&FfmpegEncoder::EncodeAudio, this);
	video_encoder.thread = std::thread(&FfmpegEncoder::EncodeVideo, this);
	muxer.thread         = std::thread(&FfmpegEncoder::Mux, this);
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

void FfmpegEncoder::CaptureVideoAddFrame(const RenderedImage& image,
                                         const float frames_per_second)
{
	static int64_t pts = 0;
	const int rounded_fps = iroundf(frames_per_second);
	if (!video_encoder.is_initalised) {
		if (!video_encoder.Init(image, rounded_fps)) {
			LOG_ERR("FFMPEG: Video encoder failed to init");
			video_encoder.Free();
			return;
		}

		bool init_muxer = false;
		if (!muxer.is_initalised && audio_encoder.is_initalised) {
			init_muxer = muxer.Init(video_encoder, audio_encoder);
			if (!init_muxer) {
				LOG_ERR("FFMPEG: Failed to init muxer");
				muxer.Free();
				video_encoder.Free();
				return;
			}
		}

		mutex.lock();
		video_encoder.is_initalised = true;
		pts = 0;

		muxer.is_initalised = muxer.is_initalised || init_muxer;
		mutex.unlock();
		waiter.notify_all();

	} else if (video_encoder.width != image.params.width ||
	           video_encoder.height != image.params.height ||
	           video_encoder.pixel_format != image.params.pixel_format ||
	           video_encoder.frames_per_second != rounded_fps ||
	           video_encoder.pixel_aspect_ratio !=
	                   image.params.video_mode.pixel_aspect_ratio) {
		CaptureVideoFinalise();
		if (!video_encoder.Init(image, rounded_fps)) {
			LOG_ERR("FFMPEG: Video encoder failed to init");
			video_encoder.Free();
			return;
		}

		mutex.lock();
		video_encoder.is_initalised = true;
		pts = 0;
		mutex.unlock();
		waiter.notify_all();
	}

	RenderedImage copy = image.deep_copy();
	copy.pts = pts++;
	if (!video_scaler.queue.MaybeEnqueue(std::move(copy))) {
		copy.free();
	}
}

void FfmpegEncoder::CaptureVideoAddAudioData(const uint32_t sample_rate,
                                             const uint32_t num_sample_frames,
                                             const int16_t* sample_frames)
{
	if (!audio_encoder.is_initalised) {
		if (!audio_encoder.Init(sample_rate)) {
			LOG_ERR("FFMPEG: Failed to init audio encoder");
			audio_encoder.Free();
			return;
		}

		bool init_muxer = false;
		if (!muxer.is_initalised && video_encoder.is_initalised) {
			init_muxer = muxer.Init(video_encoder, audio_encoder);
			if (!init_muxer) {
				LOG_ERR("FFMPEG: Failed to init muxer");
				muxer.Free();
				audio_encoder.Free();
				return;
			}
		}

		mutex.lock();
		audio_encoder.is_initalised = true;
		muxer.is_initalised         = muxer.is_initalised || init_muxer;
		mutex.unlock();
		waiter.notify_all();

	} else if (audio_encoder.sample_rate != sample_rate) {
		CaptureVideoFinalise();
		if (!audio_encoder.Init(sample_rate)) {
			LOG_ERR("FFMPEG: Failed to init audio encoder");
			audio_encoder.Free();
			return;
		}

		mutex.lock();
		audio_encoder.is_initalised = true;
		mutex.unlock();
		waiter.notify_all();
	}

	const size_t num_samples = num_sample_frames * SamplesPerFrame;
	std::vector<int16_t> audio_data(sample_frames, sample_frames + num_samples);
	audio_encoder.queue.BulkEnqueue(audio_data, num_samples);
}

void FfmpegEncoder::StopQueues()
{
	std::unique_lock<std::mutex> lock(mutex);

	// Set these first so none of the threads start another iteration
	// Before the encoders get re-initalised
	audio_encoder.is_initalised = false;
	video_encoder.is_initalised = false;
	muxer.is_initalised         = false;

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

	// At least the muxer queue has been measured
	// to have stale items left in it if it is never started
	// due to one of the encoders failing to init.
	// Explicitly clear the queues out so we don't have to worry
	// about stale work with incorrect video/audio specs for the next
	// recording.
	video_scaler.queue.Clear();
	audio_encoder.queue.Clear();
	video_encoder.queue.Clear();
	muxer.queue.Clear();

	muxer.Free();
	audio_encoder.Free();
	video_encoder.Free();
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

bool FfmpegMuxer::Init(FfmpegVideoEncoder& video_encoder,
                       FfmpegAudioEncoder& audio_encoder)
{
	const int32_t output_file_index = get_next_capture_index(CaptureType::VideoMp4);
	const std::string output_file_path =
	        generate_capture_filename(CaptureType::VideoMp4, output_file_index)
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
	                                             video_encoder.codec);
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
	                                             audio_encoder.codec);
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

bool FfmpegAudioEncoder::Init(const uint32_t sample_rate)
{
	codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!codec) {
		LOG_ERR("FFMPEG: Failed to find audio codec");
		return false;
	}

	codec_context = avcodec_alloc_context3(codec);
	if (!codec_context) {
		LOG_ERR("FFMPEG: Failed to allocate audio context");
		return false;
	}

	codec_context->sample_fmt     = AV_SAMPLE_FMT_FLTP;
	codec_context->sample_rate    = static_cast<int>(sample_rate);
	codec_context->channel_layout = AV_CH_LAYOUT_STEREO;

	if (avcodec_open2(codec_context, codec, nullptr) < 0) {
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

	// Resampler used to convert from interleaved 16 bit signed int
	// To planar floating point format
	// Can resample the audio rate in the same function call if needed
	// (currently no resampling) I believe there is an extra step or two
	// required for actual resampling (converting timing and such)
	constexpr int log_offset    = 0;
	constexpr void* log_context = nullptr;
	resampler_context           = swr_alloc_set_opts(nullptr,
                                               AV_CH_LAYOUT_STEREO,
                                               AV_SAMPLE_FMT_FLTP,
                                               codec_context->sample_rate,
                                               AV_CH_LAYOUT_STEREO,
                                               AV_SAMPLE_FMT_S16,
                                               codec_context->sample_rate,
                                               log_offset,
                                               log_context);
	if (!resampler_context) {
		LOG_ERR("FFMPEG: Failed to allocate audio resampler");
		return false;
	}
	if (swr_init(resampler_context) < 0) {
		LOG_ERR("FFMPEG: Failed to init audio resampler");
		return false;
	}

	this->sample_rate = sample_rate;
	return true;
}

void FfmpegAudioEncoder::Free()
{
	if (resampler_context) {
		swr_free(&resampler_context);
		resampler_context = nullptr;
	}
	if (frame) {
		av_frame_free(&frame);
		frame = nullptr;
	}
	if (codec_context) {
		avcodec_free_context(&codec_context);
		codec_context = nullptr;
	}
}

bool FfmpegVideoEncoder::Init(const RenderedImage& image, const int frames_per_second)
{
	this->width              = image.params.width;
	this->height             = image.params.height;
	this->pixel_format       = image.params.pixel_format;
	this->frames_per_second  = frames_per_second;
	this->pixel_aspect_ratio = image.params.video_mode.pixel_aspect_ratio;
	codec = avcodec_find_encoder_by_name("libx264");
	if (!codec) {
		LOG_ERR("FFMPEG: Failed to find libx264 encoder");
		return false;
	}
	codec_context = avcodec_alloc_context3(codec);
	if (!codec_context) {
		LOG_ERR("FFMPEG: Failed to allocate video context");
		return false;
	}

	codec_context->width                   = ScaledWidth;
	codec_context->height                  = ScaledHeight;
	codec_context->time_base.num           = 1;
	codec_context->time_base.den           = frames_per_second;
	codec_context->framerate.num           = frames_per_second;
	codec_context->framerate.den           = 1;
	codec_context->pix_fmt                 = OutputFormat;
	#if 0
	codec_context->sample_aspect_ratio.num = static_cast<int>(
	        pixel_aspect_ratio.Num());
	codec_context->sample_aspect_ratio.den = static_cast<int>(
	        pixel_aspect_ratio.Denom());
	#endif

	if (avcodec_open2(codec_context, codec, nullptr) < 0) {
		LOG_ERR("FFMPEG: Failed to open video context");
		return false;
	}

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

void FfmpegEncoder::ScaleVideo()
{
	for (;;) {
		std::unique_lock<std::mutex> lock(mutex);
		waiter.wait(lock, [this] {
			return video_encoder.is_initalised || is_shutting_down;
		});
		if (is_shutting_down) {
			return;
		}
		video_scaler.is_working = true;
		lock.unlock();

		while (auto image = video_scaler.queue.Dequeue()) {
			const auto& image_params = image->params;
			if (image_params.width != 320 || image_params.height != 200 || image_params.pixel_format != PixelFormat::XRGB8888_Packed32) {
				LOG_ERR("FFMPEG: Wrong format");
				image->free();
				continue;
			}

			AVFrame* frame = av_frame_alloc();
			if (!frame) {
				LOG_ERR("FFMPEG: Failed to allocate video frame");
				image->free();
				continue;
			}
			frame->width               = ScaledWidth;
			frame->height              = ScaledHeight;
			frame->format              = static_cast<int>(OutputFormat);
			frame->pts                 = image->pts;
			// 0 means auto-align based on current CPU
			constexpr int memory_alignment = 0;
			if (av_frame_get_buffer(frame, memory_alignment) < 0) {
				LOG_ERR("FFMPEG: Failed to get video frame buffer");
				av_frame_free(&frame);
				image->free();
				continue;
			}

			uint8_t* y_row = frame->data[0];
			uint8_t* cr_row = frame->data[2];
			uint8_t* cb_row = frame->data[1];
			int y_pitch = frame->linesize[0];
			int cr_pitch = frame->linesize[2];
			int cb_pitch = frame->linesize[1];
			constexpr int HorizonatalScale = 5;
			constexpr int VerticalScale = 6;
			uint8_t* row = image->image_data;
			for (uint16_t y = 0; y < image_params.height; ++y) {
				for (uint16_t x = 0; x < image_params.width; ++x) {
					uint32_t src_pixel = read_unaligned_uint32_at(row, x);
					float red = static_cast<float>((src_pixel >> 16) & 0xFF);
					float green = static_cast<float>((src_pixel >> 8) & 0xFF);
					float blue = static_cast<float>(src_pixel & 0xFF);

					float yf = (0.257f * red) + (0.504f * green) + (0.098f * blue) + 16.0f;
					uint8_t y_out = static_cast<uint8_t>(clamp(yf, 0.0f, 255.0f));
					memset(y_row + (x * HorizonatalScale), y_out, HorizonatalScale);

					if (y % 2 == 0 && x % 2 == 0) {
						float crf = (0.439f * red) - (0.368f * green) - (0.071f * blue) + 128.0f;
						float cbf = -(0.148f * red) - (0.291f * green) + (0.439f * blue) + 128.0f;
						uint8_t cr_out = static_cast<uint8_t>(clamp(crf, 0.0f, 255.0f));
						uint8_t cb_out = static_cast<uint8_t>(clamp(cbf, 0.0f, 255.0f));
						memset(cr_row + (x / 2) * HorizonatalScale, cr_out, HorizonatalScale);
						memset(cb_row + (x / 2) * HorizonatalScale, cb_out, HorizonatalScale);
					}
				}

				for (int i = 1; i < VerticalScale; ++i) {
					uint8_t* prev_row = y_row;
					y_row += y_pitch;
					memcpy(y_row, prev_row, ScaledWidth);
				}
				if (y % 2 == 0) {
					for (int i = 1; i < VerticalScale; ++i) {
						uint8_t* prev_row = cr_row;
						cr_row += cr_pitch;
						memcpy(cr_row, prev_row, ScaledWidth / 2);
					}
					for (int i = 1; i < VerticalScale; ++i) {
						uint8_t* prev_row = cb_row;
						cb_row += cb_pitch;
						memcpy(cb_row, prev_row, ScaledWidth / 2);
					}
					cr_row += cr_pitch;
					cb_row += cb_pitch;
				}

				y_row += y_pitch;
				row += image->pitch;
			}
			image->free();

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
			return video_encoder.is_initalised || is_shutting_down;
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
			return muxer.is_initalised || is_shutting_down;
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
			return audio_encoder.is_initalised || is_shutting_down;
		});
		if (is_shutting_down) {
			return;
		}
		audio_encoder.is_working = true;
		lock.unlock();

		// Number of samples per channel (frames) the AVFrame has been
		// allocated to hold
		const int frame_capacity = audio_encoder.frame->nb_samples;

		// 2 samples per frame. Number of int16_ts requested from the
		// queue.
		const size_t sample_capacity = static_cast<size_t>(frame_capacity) *
		                               SamplesPerFrame;

		for (;;) {
			audio_encoder.queue.BulkDequeue(audio_data, sample_capacity);
			if (audio_data.empty()) {
				assert(!audio_encoder.queue.IsRunning());
				break;
			}
			if (av_frame_make_writable(audio_encoder.frame) < 0) {
				LOG_ERR("FFMPEG: Failed to make audio frame writable");
				continue;
			}
			const int received_frames = static_cast<int>(
			                                    audio_data.size()) /
			                            static_cast<int>(SamplesPerFrame);
			const uint8_t* audio_ptr = reinterpret_cast<uint8_t*>(
			        audio_data.data());
			const int converted_frames =
			        swr_convert(audio_encoder.resampler_context,
			                    audio_encoder.frame->data,
			                    frame_capacity,
			                    &audio_ptr,
			                    received_frames);
			if (converted_frames < 0) {
				LOG_ERR("FFMPEG: Failed to convert audio frame");
				continue;
			}
			assert(converted_frames == received_frames);
			audio_encoder.frame->nb_samples = converted_frames;
			audio_encoder.frame->pts += converted_frames;
			if (avcodec_send_frame(audio_encoder.codec_context,
			                       audio_encoder.frame) < 0) {
				LOG_ERR("FFMPEG: Failed to send audio frame");
			}
			send_packets_to_muxer(audio_encoder.codec_context,
			                      MuxerAudioStreamIndex,
			                      muxer.queue);

			// The encoder must receive full capacity except for the
			// last frame. If we hit this, the queue should be
			// stopped anyway.
			if (converted_frames != frame_capacity) {
				assert(!audio_encoder.queue.IsRunning());
				break;
			}
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
