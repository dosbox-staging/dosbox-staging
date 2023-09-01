/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#ifndef DOSBOX_CAPTURE_VIDEO_H
#define DOSBOX_CAPTURE_VIDEO_H

#include "render.h"
#include "rwqueue.h"

class VideoEncoder {
public:
	virtual void CaptureVideoAddFrame(const RenderedImage& image,
	                                  const float frames_per_second) = 0;

	virtual void CaptureVideoAddAudioData(const uint32_t sample_rate,
	                                      const uint32_t num_sample_frames,
	                                      const int16_t* sample_frames) = 0;

	virtual void CaptureVideoFinalise() = 0;

	virtual ~VideoEncoder() = default;
};

class ZMBVEncoder : public VideoEncoder {
public:
	void CaptureVideoAddFrame(const RenderedImage& image,
	                          const float frames_per_second) override;

	void CaptureVideoAddAudioData(const uint32_t sample_rate,
	                              const uint32_t num_sample_frames,
	                              const int16_t* sample_frames) override;

	void CaptureVideoFinalise() override;
};

#if C_FFMPEG

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

struct FfmpegVideoEncoder {
	RWQueue<RenderedImage> queue{256};
	std::thread thread = {};

	// FFmpeg pointers. Initialsed in main thread. Used by worker thread.
	const AVCodec* codec          = nullptr;
	AVCodecContext* codec_context = nullptr;
	AVFrame* frame                = nullptr;
	SwsContext* rescaler_contex   = nullptr;

	// Accessed only in main thread, used to check if needs re-init
	// If one of these changes, create a new file
	Fraction pixel_aspect_ratio = {};
	PixelFormat pixel_format    = {};
	int frames_per_second       = 0;
	uint16_t width              = 0;
	uint16_t height             = 0;

	// Synchronization flags, guarded by mutex

	// Main thread waits for worker to finish working.
	bool is_working = false;

	// Worker thread waits on main thread to initalise.
	bool is_initalised = false;

	// Init + free called by main thread only.
	bool Init(const uint16_t width, const uint16_t height,
	          const PixelFormat pixel_format, const int frames_per_second,
	          const Fraction pixel_aspect_ratio);
	void Free();
};

struct FfmpegAudioEncoder {
	RWQueue<int16_t> queue{480000};
	std::thread thread = {};

	// FFmpeg pointers. Initialsed in main thread. Used by worker thread.
	const AVCodec* codec          = nullptr;
	AVCodecContext* codec_context = nullptr;
	AVFrame* frame                = nullptr;
	SwrContext* resampler_context = nullptr;

	// Accessed only in main thread, used to check if needs re-init
	// If sample rate changes, create a new file
	uint32_t sample_rate = 0;

	// Synchronization flags, guarded by mutex

	// Main thread waits for worker to finish working.
	bool is_working = false;

	// Worker thread waits on main thread to initalise.
	bool is_initalised = false;

	// Init + free called by main thread only.
	bool Init(const uint32_t sample_rate);
	void Free();
};

struct FfmpegMuxer {
	RWQueue<AVPacket*> queue{1024};
	std::thread thread = {};

	// FFmpeg pointers. Initialsed in main thread. Used by worker thread.
	AVFormatContext* format_context = nullptr;

	// Synchronization flags, guarded by mutex

	// Main thread waits for worker to finish working
	bool is_working = false;

	// Worker thread waits on main thread to initalise.
	bool is_initalised = false;

	// Init + free called by main thread only.
	// Muxer requires both video and audio encoders to be initalised first.
	bool Init(FfmpegVideoEncoder& video_encoder,
	          FfmpegAudioEncoder& audio_encoder);
	void Free();
};

class FfmpegEncoder : public VideoEncoder {
public:
	FfmpegEncoder();
	~FfmpegEncoder();
	FfmpegEncoder(const FfmpegEncoder&)            = delete;
	FfmpegEncoder& operator=(const FfmpegEncoder&) = delete;

	void CaptureVideoAddFrame(const RenderedImage& image,
	                          const float frames_per_second) override;

	void CaptureVideoAddAudioData(const uint32_t sample_rate,
	                              const uint32_t num_sample_frames,
	                              const int16_t* sample_frames) override;

	void CaptureVideoFinalise() override;

private:
	std::mutex mutex                 = {};
	std::condition_variable waiter   = {};
	FfmpegVideoEncoder video_encoder = {};
	FfmpegAudioEncoder audio_encoder = {};
	FfmpegMuxer muxer                = {};

	// Guarded by mutex, only set in destructor
	bool is_shutting_down = false;

	void EncodeVideo();
	void EncodeAudio();
	void Mux();
	void StopQueues();
	void StartQueues();
};

#endif // C_FFMPEG
#endif // DOSBOX_CAPTURE_VIDEO_H
