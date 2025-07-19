/*
 *  Copyright (C) 2022 Jon Dennis
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

//
// This file contains the reelmagic MPEG player code...
//

#include "reelmagic.h"

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "channel_names.h"
#include "dos_system.h"
#include "logging.h"
#include "mixer.h"
#include "player.h"
#include "rwqueue.h"
#include "setup.h"
#include "timer.h"

// bring in the MPEG-1 decoder library...
#define PL_MPEG_IMPLEMENTATION
#include "mpeg_decoder.h"

// global config
static ReelMagic_PlayerConfiguration _globalDefaultPlayerConfiguration;

constexpr unsigned int common_magic_key   = 0x40044041;
constexpr unsigned int thehorde_magic_key = 0xC39D7088;

static unsigned int _initialMagicKey = common_magic_key;
static unsigned int _magicalFcodeOverride = 0; // 0 = no override

//
// Internal class utilities...
//
namespace {
// XXX currently duplicating this in realmagic_*.cpp files to avoid header pollution... TDB if this
// is a good idea...
struct RMException : ::std::exception {
	std::string _msg = {};
	RMException(const char* fmt = "General ReelMagic Exception", ...)
	{
		va_list vl;
		va_start(vl, fmt);
		_msg.resize(vsnprintf(&_msg[0], 0, fmt, vl) + 1);
		va_end(vl);
		va_start(vl, fmt);
		vsnprintf(&_msg[0], _msg.size(), fmt, vl);
		va_end(vl);
		LOG(LOG_REELMAGIC, LOG_ERROR)("%s", _msg.c_str());
	}
	~RMException() noexcept override = default;
	const char* what() const noexcept override
	{
		return _msg.c_str();
	}
};

class AudioFifo {
private:
	plm_t* mpeg_stream        = {};
	plm_samples_t* mp2_buffer = {};
	int sample_rate           = 0;
	uint16_t num_inspected    = 0;

public:
	AudioFifo() = default;

	AudioFifo(plm_t* plm) : mpeg_stream(plm)
	{
		assert(mpeg_stream);
		assert(mpeg_stream->audio_decoder);
		assert(mpeg_stream->audio_decoder->buffer);

		// Prevent the decoder from muxing audio from multiple active
		// players into the same MP2 buffer. This is needed for games
		// that hold multiple players, like Flash Traffic.
		mpeg_stream->audio_decoder->buffer->load_callback = nullptr;

		SetSampleRate(plm_get_samplerate(mpeg_stream));
	}

	int GetSampleRate() const
	{
		return sample_rate;
	}
	void SetSampleRate(const int rate)
	{
		// MPEG-1 Layer II Audio supports 32, 44.1, and 48 KHz frame rates
		assert(rate == 32000 || rate == 44100 || rate == 48000);
		sample_rate = rate;
	}

	const float* PopFrame()
	{
		constexpr uint16_t max_frames = PLM_AUDIO_SAMPLES_PER_FRAME;

		// A helper to get the audio frame at the current position.
		auto at_pos = [&]() {
			assert(mp2_buffer);
			constexpr uint8_t num_channels = 2; // L & R
			const auto pos = num_channels * mp2_buffer->count++;
			return mp2_buffer->interleaved + pos;
		};
		// A lamda to get the current frame and decode the next
		// MP2 buffer once we've used all the current frames.
		//
		auto get_frame = [&]() -> float* {
			// If the MP2 buffer is still valid, return the audio
			// frame at the current position.
			if (mp2_buffer && mp2_buffer->count < max_frames) {
				return at_pos();
			}
			// Otherwise try decoding the next MP2 frame
			assert(mpeg_stream);
			if (mp2_buffer = plm_decode_audio(mpeg_stream); mp2_buffer) {
				// If we got a new MP2 buffer then reset the
				// usage count and return its first frame.
				mp2_buffer->count = 0;
				return at_pos();
			}
			// We're out! No more frames or MP2 buffers available.
			return nullptr;
		};
		// A lamda to skip past initial empty audio chunks (up to half a
		// frame's worth) which helps reduce or eliminate gap-stuttering
		// during the initial video playback.
		//
		auto skip_initial_gaps = [&](float* frame) {
			while (num_inspected < max_frames && frame) {
				++num_inspected;
				if (frame[0] == 0.0f && frame[1] == 0.0f) {
					frame = get_frame();
				} else {
					break;
				}
			}
			return frame;
		};

		return skip_initial_gaps(get_frame());
	}

	void ResetMp2Buffer()
	{
		mp2_buffer    = {};
		num_inspected = 0;
	}
};
} // namespace

static void ActivatePlayerAudioFifo(AudioFifo& audio_fifo);
static void DeactivatePlayerAudioFifo(AudioFifo& audio_fifo);

//
// implementation of a "ReelMagic Media Player" and handles begins here...
//
namespace {
class ReelMagic_MediaPlayerImplementation : public ReelMagic_MediaPlayer,
                                            public ReelMagic_VideoMixerMPEGProvider {
	// creation parameters...
	const std::unique_ptr<ReelMagic_MediaPlayerFile> _file = {};
	ReelMagic_PlayerConfiguration _config = _globalDefaultPlayerConfiguration;
	ReelMagic_PlayerAttributes _attrs = {};

	// running / adjustable variables...
	bool _stopOnComplete = {};
	bool _playing        = {};

	// output state...
	float _vgaFps                          = {};
	float _vgaFramesPerMpegFrame           = {};
	float _waitVgaFramesUntilNextMpegFrame = {};
	bool _drawNextFrame                    = {};

	// stuff about the MPEG decoder...
	plm_t* _plm                   = {};
	plm_frame_t* _nextFrame       = {};
	float _framerate              = {};
	uint8_t _magicalRSizeOverride = {};

	AudioFifo audio_fifo = {};

	static void plmBufferLoadCallback(plm_buffer_t* self, void* user)
	{
		// note: based on plm_buffer_load_file_callback()
		try {
			if (self->discard_read_bytes) {
				plm_buffer_discard_read_bytes(self);
			}
			auto bytes_available = self->capacity - self->length;
			if (bytes_available > 4096)
				bytes_available = 4096;
			const uint32_t bytes_read = ((ReelMagic_MediaPlayerImplementation*)user)
			                                    ->_file->Read(self->bytes + self->length,
			                                                  static_cast<uint16_t>(
			                                                          bytes_available));
			self->length += bytes_read;

			if (bytes_read == 0) {
				self->has_ended = TRUE;
			}
		} catch (...) {
			self->has_ended = TRUE;
		}
	}
	static void plmBufferSeekCallback([[maybe_unused]] plm_buffer_t* self, void* user, size_t absPos)
	{
		assert(absPos <= UINT32_MAX);
		try {
			((ReelMagic_MediaPlayerImplementation*)user)
			        ->_file->Seek(static_cast<uint32_t>(absPos), DOS_SEEK_SET);
		} catch (...) {
			// XXX what to do on failure !?
		}
	}

	static void plmDecodeMagicalPictureHeaderCallback(plm_video_t* self, void* user)
	{
		switch (self->picture_type) {
		case PLM_VIDEO_PICTURE_TYPE_B:
			self->motion_backward.r_size =
			        ((ReelMagic_MediaPlayerImplementation*)user)->_magicalRSizeOverride;
			// fallthrough
		case PLM_VIDEO_PICTURE_TYPE_PREDICTIVE:
			self->motion_forward.r_size = ((ReelMagic_MediaPlayerImplementation*)user)->_magicalRSizeOverride;
		}
	}

	void advanceNextFrame()
	{
		_nextFrame = plm_decode_video(_plm);
		if (!_nextFrame) {
			// note: will return nullptr frame once when looping...
			// give it one more go...
			if (plm_get_loop(_plm)) {
				_nextFrame = plm_decode_video(_plm);
			}
			if (!_nextFrame) {
				_playing = false;
			}
		}
	}

	unsigned FindMagicalFCode()
	{
		// now this is some mighty fine half assery...
		// i'm sure this is suppoed to be done on a per-picture basis, but for now, this
		// hack seems to work ok... the idea here is that MPEG-1 assets with a picture_rate
		// code >= 0x9 in the MPEG sequence header have screwed up f_code values. i'm not
		// sure why but this may be some form of copy and/or clone protection for ReelMagic.
		// pictures with a temporal sequence number of either 3 or 8 seem to contain a
		// truthful f_code when a "key" of 0x40044041 (ReelMagic default) is given to us and
		// a temporal sequence number of 4 seems to contain the truthful f_code when a "key"
		// of 0xC39D7088 is given to us
		//
		// for now, this hack scrubs the MPEG file in search of the first P or B pictures
		// with a temporal sequence number matching a truthful value based on the player's
		// "magic key" the player then applies the found f_code value as a global static
		// forward and backward value for this entire asset.
		//
		// ultimately, this should probably be done on a per-picture basis using some sort
		// of algorithm to translate the screwed-up values on-the-fly...

		unsigned result = 0;

		const int audio_enabled = plm_get_audio_enabled(_plm);
		const int loop_enabled  = plm_get_loop(_plm);
		plm_rewind(_plm);
		plm_set_audio_enabled(_plm, FALSE);
		plm_set_loop(_plm, FALSE);

		do {
			if (plm_buffer_find_start_code(_plm->video_decoder->buffer,
			                               PLM_START_PICTURE) == -1) {
				break;
			}
			const unsigned temporal_seqnum = plm_buffer_read(_plm->video_decoder->buffer, 10);
			const unsigned picture_type = plm_buffer_read(_plm->video_decoder->buffer, 3);
			if ((picture_type == PLM_VIDEO_PICTURE_TYPE_PREDICTIVE) ||
			    (picture_type == PLM_VIDEO_PICTURE_TYPE_B)) {
				plm_buffer_skip(_plm->video_decoder->buffer, 16); // skip vbv_delay
				plm_buffer_skip(_plm->video_decoder->buffer, 1);  // skip full_px
				result = plm_buffer_read(_plm->video_decoder->buffer, 3);
				switch (_config.MagicDecodeKey) {
				case thehorde_magic_key:
					if (temporal_seqnum != 4)
						result = 0;
					break;

				default:
					LOG(LOG_REELMAGIC, LOG_WARN)
					("Unknown magic key: 0x%08X. Defaulting to the common key: 0x%08X",
					 _config.MagicDecodeKey, common_magic_key);
					// fall-through

				// most ReelMagic games seem to use this "key"
				case common_magic_key:
					// tsn=3 and tsn=8 seem to contain truthful
					if ((temporal_seqnum != 3) && (temporal_seqnum != 8))
						result = 0;
					break;
				}
			}
		} while (result == 0);

		plm_set_loop(_plm, loop_enabled);
		plm_set_audio_enabled(_plm, audio_enabled);
		plm_rewind(_plm);

		return result;
	}

	void CollectVideoStats()
	{
		_attrs.PictureSize.Width  = static_cast<uint16_t>(plm_get_width(_plm));
		_attrs.PictureSize.Height = static_cast<uint16_t>(plm_get_height(_plm));
		if (_attrs.PictureSize.Width && _attrs.PictureSize.Height) {
			if (_plm->video_decoder->seqh_picture_rate >= 0x9) {
				LOG(LOG_REELMAGIC, LOG_NORMAL)
				("Detected a magical picture_rate code of 0x%X.",
				 (unsigned)_plm->video_decoder->seqh_picture_rate);
				const unsigned magical_f_code = _magicalFcodeOverride
				                                      ? _magicalFcodeOverride
				                                      : FindMagicalFCode();
				if (magical_f_code) {
					const auto reduced_f_code = magical_f_code - 1;
					assert(reduced_f_code <= UINT8_MAX);
					_magicalRSizeOverride = static_cast<uint8_t>(reduced_f_code);
					plm_video_set_decode_picture_header_callback(
					        _plm->video_decoder,
					        &plmDecodeMagicalPictureHeaderCallback,
					        this);
					LOG(LOG_REELMAGIC, LOG_NORMAL)
					("Applying static %u:%u f_code override",
					 magical_f_code,
					 magical_f_code);
				} else {
					LOG(LOG_REELMAGIC, LOG_WARN)
					("No magical f_code found. Playback will likely be screwed up!");
				}
				_plm->video_decoder->framerate =
				        PLM_VIDEO_PICTURE_RATE[0x7 & _plm->video_decoder->seqh_picture_rate];
			}
			if (_plm->video_decoder->framerate == 0.000) {
				LOG(LOG_REELMAGIC, LOG_ERROR)
				("Detected a bad framerate. Hardcoding to 30. This video will likely not work at all.");
				_plm->video_decoder->framerate = 30.000;
			}
		}
		_framerate = static_cast<float>(plm_get_framerate(_plm));
	}

	void SetupVESOnlyDecode()
	{
		plm_set_audio_enabled(_plm, FALSE);
		if (_plm->audio_decoder) {
			plm_audio_destroy(_plm->audio_decoder);
			_plm->audio_decoder = nullptr;
		}
		plm_demux_rewind(_plm->demux);
		_plm->has_decoders      = TRUE;
		_plm->video_packet_type = PLM_DEMUX_PACKET_VIDEO_1;
		if (_plm->video_decoder)
			plm_video_destroy(_plm->video_decoder);
		_plm->video_decoder = plm_video_create_with_buffer(_plm->demux->buffer, FALSE);
	}

public:
	ReelMagic_MediaPlayerImplementation(const ReelMagic_MediaPlayerImplementation&) = delete;
	ReelMagic_MediaPlayerImplementation& operator=(const ReelMagic_MediaPlayerImplementation&) = delete;

	ReelMagic_MediaPlayerImplementation(ReelMagic_MediaPlayerFile* const player_file)
	        : _file(player_file)
	{
		assert(_file);
		auto plmBuf = plm_buffer_create_with_virtual_file(&plmBufferLoadCallback,
		                                                  &plmBufferSeekCallback,
		                                                  this,
		                                                  _file->GetFileSize());
		assert(plmBuf);

		// TRUE means that the buffer is destroyed on failure or when closing _plm
		_plm = plm_create_with_buffer(plmBuf, TRUE);
		if (!_plm) {
			LOG(LOG_REELMAGIC, LOG_ERROR)
			("Player failed creating buffer using file %s",
			 _file->GetFileName());
			plmBuf = nullptr;
			return;
		}

		assert(_plm);
		plm_demux_set_stop_on_program_end(_plm->demux, TRUE);

		bool detetectedFileTypeVesOnly = false;
		if (!plm_has_headers(_plm)) {
			// failed to detect an MPEG-1 PS (muxed) stream...
			// try MPEG-ES: assuming video-only...
			detetectedFileTypeVesOnly = true;
			SetupVESOnlyDecode();
		}

		CollectVideoStats();
		advanceNextFrame(); // attempt to decode the first frame of video...
		if (!_nextFrame || (_attrs.PictureSize.Width == 0) ||
		    (_attrs.PictureSize.Height == 0)) {
			// something failed... asset is deemed bad at this point...
			plm_destroy(_plm);
			_plm = nullptr;
		}
		// Setup the audio FIFO if we have audio
		if (_plm && _plm->audio_decoder) {
			audio_fifo = AudioFifo(_plm);
		}

		if (!_plm) {
			LOG(LOG_REELMAGIC, LOG_ERROR)
			("Failed creating media player: MPEG type-detection failed %s",
			 _file->GetFileName());
		} else {
			LOG(LOG_REELMAGIC, LOG_NORMAL)
			("Created Media Player %s %ux%u @ %0.2ffps %s",
			 detetectedFileTypeVesOnly ? "MPEG-ES" : "MPEG-PS",
			 (unsigned)_attrs.PictureSize.Width,
			 (unsigned)_attrs.PictureSize.Height,
			 (double)_framerate,
			 _file->GetFileName());
			if (audio_fifo.GetSampleRate()) {
				LOG(LOG_REELMAGIC, LOG_NORMAL)
				("Media Player Audio Decoder Enabled @ %uHz",
				 (unsigned)audio_fifo.GetSampleRate());
			}
		}
	}
	~ReelMagic_MediaPlayerImplementation() override
	{
		LOG(LOG_REELMAGIC, LOG_NORMAL)
		("Destroying Media Player #%u with file %s", GetBaseHandle(), _file->GetFileName());
		DeactivatePlayerAudioFifo(audio_fifo);
		if (ReelMagic_GetVideoMixerMPEGProvider() == this)
			ReelMagic_ClearVideoMixerMPEGProvider();
		if (_plm) {
			plm_destroy(_plm);
		}
	}

	//
	// ReelMagic_VideoMixerMPEGProvider implementation here...
	//
	void OnVerticalRefresh(void* const outputBuffer, const float fps) override
	{
		if (fps != _vgaFps) {
			_vgaFps                = fps;
			_vgaFramesPerMpegFrame = _vgaFps;
			_vgaFramesPerMpegFrame /= _framerate;
			_waitVgaFramesUntilNextMpegFrame = _vgaFramesPerMpegFrame;
			_drawNextFrame                   = true;
		}

		if (_drawNextFrame) {
			if (_nextFrame) {
				plm_frame_to_rgb(_nextFrame,
				                 (uint8_t*)outputBuffer,
				                 _attrs.PictureSize.Width * 3);
			}
			_drawNextFrame = false;
		}

		if (!_playing) {
			if (_stopOnComplete)
				ReelMagic_ClearVideoMixerMPEGProvider();
			return;
		}

		for (_waitVgaFramesUntilNextMpegFrame -= 1.f; _waitVgaFramesUntilNextMpegFrame < 0.f;
		     _waitVgaFramesUntilNextMpegFrame += _vgaFramesPerMpegFrame) {
			advanceNextFrame();
			_drawNextFrame = true;
		}
	}

	const ReelMagic_PlayerConfiguration& GetConfig() const override
	{
		return _config;
	}
	// const ReelMagic_PlayerAttributes& GetAttrs() const -- implemented in the
	// ReelMagic_MediaPlayer functions below

	//
	// ReelMagic_MediaPlayer implementation here...
	//
	ReelMagic_PlayerConfiguration& Config() override
	{
		return _config;
	}
	const ReelMagic_PlayerAttributes& GetAttrs() const override
	{
		return _attrs;
	}
	bool HasDemux() const override
	{
		return _plm && _plm->demux->buffer != _plm->video_decoder->buffer;
	}
	bool HasVideo() const override
	{
		return _plm && plm_get_video_enabled(_plm);
	}
	bool HasAudio() const override
	{
		return _plm && plm_get_audio_enabled(_plm);
	}
	bool IsPlaying() const override
	{
		return _playing;
	}

	// Handle registation functions.
	void RegisterBaseHandle(const reelmagic_handle_t handle)
	{
		assert(handle != reelmagic_invalid_handle);
		_attrs.Handles.Base = handle;
	}

	reelmagic_handle_t GetBaseHandle() const
	{
		assert(_attrs.Handles.Base != reelmagic_invalid_handle);
		return _attrs.Handles.Base;
	}

	// The return value indicates if the handle was registered.
	bool RegisterDemuxHandle(const reelmagic_handle_t handle)
	{
		const auto has_demux = HasDemux();
		_attrs.Handles.Demux = has_demux ? handle : reelmagic_invalid_handle;
		return has_demux;
	}

	bool RegisterVideoHandle(const reelmagic_handle_t handle)
	{
		const auto has_video = HasVideo();
		_attrs.Handles.Video = has_video ? handle : reelmagic_invalid_handle;
		return has_video;
	}

	bool RegisterAudioHandle(const reelmagic_handle_t handle)
	{
		const auto has_audio = HasAudio();
		_attrs.Handles.Audio = has_audio ? handle : reelmagic_invalid_handle;
		return has_audio;
	}

	Bitu GetBytesDecoded() const override
	{
		if (!_plm) {
			return 0;
		}
		// the "real" ReelMagic setup seems to only return values in multiples of 4k...
		// therfore, we must emulate the same behavior here...
		// rounding up the demux position to align....
		// NOTE: I'm not sure if this should be different for DMA streaming mode!
		const Bitu alignTo = 4096;
		Bitu rv            = plm_buffer_tell(_plm->demux->buffer);
		rv += alignTo - 1;
		rv &= ~(alignTo - 1);
		return rv;
	}

	void Play(const PlayMode playMode) override
	{
		if (!_plm) {
			return;
		}
		if (_playing)
			return;
		_playing = true;
		plm_set_loop(_plm, (playMode == MPPM_LOOP) ? TRUE : FALSE);
		_stopOnComplete = playMode == MPPM_STOPONCOMPLETE;
		ReelMagic_SetVideoMixerMPEGProvider(this);
		ActivatePlayerAudioFifo(audio_fifo);
		_vgaFps = 0.0f; // force drawing of next frame and timing reset
	}
	void Pause() override
	{
		_playing = false;
	}
	void Stop() override
	{
		_playing = false;
		if (ReelMagic_GetVideoMixerMPEGProvider() == this)
			ReelMagic_ClearVideoMixerMPEGProvider();
	}
	void SeekToByteOffset(const uint32_t offset) override
	{
		plm_rewind(_plm);
		plm_buffer_seek(_plm->demux->buffer, (size_t)offset);
		audio_fifo.ResetMp2Buffer();

		// this is a hacky way to force an audio decoder reset...
		if (_plm->audio_decoder)
			// something (hopefully not sample rate) changes between byte seeks in crime
			// patrol...
			_plm->audio_decoder->has_header = FALSE;

		advanceNextFrame();
	}
	void NotifyConfigChange() override
	{
		if (ReelMagic_GetVideoMixerMPEGProvider() == this)
			ReelMagic_SetVideoMixerMPEGProvider(this);
	}
};
} // namespace

//
// stuff to manage ReelMagic media/decoder/player handles...
//
using player_t = std::shared_ptr<ReelMagic_MediaPlayerImplementation>;
static std::vector<player_t> player_registry = {};

void deregister_player(const player_t& player)
{
	for (auto& p : player_registry) {
		if (p.get() == player.get()) {
			p = {};
		}
	}
}

// Registers one or more handles for the player's elementary streams.
// Returns the base handle on success or the invalid handle on failure.
static reelmagic_handle_t register_player(const player_t& player)
{
	auto get_available_handle = []() {
		// Walk from the first to (potentially) last valid handle
		auto h = reelmagic_first_handle;
		while (h <= reelmagic_last_handle) {

			// Should we grow the registry to accomodate this handle?
			if (player_registry.size() <= h) {
				player_registry.emplace_back();
				continue;
			}
			// Is this handle available (i.e.: unused) in the registry?
			if (!player_registry[h]) {
				return h;
			}
			// Otherwise step forward to the next handle
			++h;
		}
		LOG_ERR("REELMAGIC: Ran out of handles while registering player");
		throw reelmagic_invalid_handle;
	};
	try {
		// At a minimum, we register the player itself
		auto h = get_available_handle();
		player->RegisterBaseHandle(h);
		player_registry[h] = player;

		// The first stream reuses the player's handle
		if (player->RegisterDemuxHandle(h)) {
			h = get_available_handle();
		}
		if (player->RegisterVideoHandle(h)) {
			player_registry[h] = player;
			h = get_available_handle();
		}
		if (player->RegisterAudioHandle(h)) {
			player_registry[h] = player;
		}
	} catch (reelmagic_handle_t invalid_handle) {
		deregister_player(player);
		return invalid_handle;
	}
	return player->GetBaseHandle();
}

reelmagic_handle_t ReelMagic_NewPlayer(struct ReelMagic_MediaPlayerFile* const playerFile)
{
	// so why all this mickey-mouse for simply allocating a handle?
	// the real setup allocates one handle per decoder resource
	// for example, if an MPEG file is opened that only contains a video ES,
	// then only one handle is allocated
	// however, if an MPEG PS file is openened that contains both A/V ES streams,
	// then three handles are allocated. One for system, one for audio, one for video
	//
	// to ensure maximum compatibility, we must also emulate this behavior

	auto player = std::make_shared<ReelMagic_MediaPlayerImplementation>(playerFile);
	return register_player(player);
}

void ReelMagic_DeletePlayer(const reelmagic_handle_t handle)
{
	if (handle < player_registry.size()) {
		if (const auto p = player_registry[handle]; p) {
			deregister_player(p);
		}
	}
}

ReelMagic_MediaPlayer& ReelMagic_HandleToMediaPlayer(const reelmagic_handle_t handle)
{
	if (handle >= player_registry.size() || !player_registry.at(handle)) {
		throw RMException("Invalid handle #%u", handle);
	}

	return *player_registry[handle];
}

void ReelMagic_DeleteAllPlayers()
{
	player_registry = {};
}

//
// audio stuff begins here...
//
static AudioFifo* active_fifo = nullptr;
ReelMagicAudio reel_magic_audio = {};

static void ActivatePlayerAudioFifo(AudioFifo& audio_fifo)
{
	if (!audio_fifo.GetSampleRate()) {
		return;
	}
	active_fifo = &audio_fifo;
	assert(reel_magic_audio.channel);
	reel_magic_audio.channel->SetSampleRate(active_fifo->GetSampleRate());
	reel_magic_audio.output_queue.Start();
	reel_magic_audio.channel->Enable(true);
}

static void DeactivatePlayerAudioFifo(AudioFifo& audio_fifo)
{
	if (active_fifo != &audio_fifo) {
		return;
	}
	active_fifo = nullptr;
	if (reel_magic_audio.channel) {
		reel_magic_audio.channel->Enable(false);
		reel_magic_audio.output_queue.Stop();
	}
}

static void ReelMagic_PicCallback()
{
	if (!active_fifo || !reel_magic_audio.channel || !reel_magic_audio.channel->is_enabled) {
		return;
	}

	static float frame_counter = 0.0f;
	frame_counter += reel_magic_audio.channel->GetFramesPerTick();
	auto frames_remaining = ifloor(frame_counter);
	frame_counter -= static_cast<float>(frames_remaining);

	while (frames_remaining > 0) {
		if (const auto frame = active_fifo->PopFrame(); frame) {
			reel_magic_audio.output_queue.NonblockingEnqueue(AudioFrame{frame[0], frame[1]});
			--frames_remaining;
		} else {
			break;
		}
	}

	for (int i = 0; i < frames_remaining; ++i) {
		reel_magic_audio.output_queue.NonblockingEnqueue(AudioFrame{});
	}
}

void ReelMagic_EnableAudioChannel(const bool should_enable)
{
	MIXER_LockMixerThread();

	if (should_enable == false) {
		// Deregister the mixer channel and remove it
		TIMER_DelTickHandler(ReelMagic_PicCallback);
		MIXER_DeregisterChannel(reel_magic_audio.channel);
		reel_magic_audio.channel.reset();
		MIXER_UnlockMixerThread();
		return;
	}

	constexpr bool Stereo = true;
	constexpr bool SignedData = true;
	constexpr bool NativeOrder = true;
	const auto audio_callback = std::bind(MIXER_PullFromQueueCallback<ReelMagicAudio, AudioFrame, Stereo, SignedData, NativeOrder>,
	                                std::placeholders::_1,
	                                &reel_magic_audio);

	reel_magic_audio.channel = MIXER_AddChannel(audio_callback,
	                                 UseMixerRate,
	                                 ChannelName::ReelMagic,
	                                 {// ChannelFeature::Sleep,
	                                  ChannelFeature::Stereo,
	                                  // ChannelFeature::ReverbSend,
	                                  // ChannelFeature::ChorusSend,
	                                  ChannelFeature::DigitalAudio});
	assert(reel_magic_audio.channel);

	// The decoded MP2 frame contains samples ranging from [-1.0f, +1.0f],
	// so to hit 0 dB 16-bit signed, we need to multiply up from unity to
	// the maximum magnitude (32k).
	constexpr float mpeg1_db0_volume_scalar = {Max16BitSampleValue};
	reel_magic_audio.channel->Set0dbScalar(mpeg1_db0_volume_scalar);

	// Size to 2x blocksize. The mixer callback will request 1x blocksize.
	// This provides a good size to avoid over-runs and stalls.
	reel_magic_audio.output_queue.Resize(iceil(reel_magic_audio.channel->GetFramesPerBlock() * 2.0f));

	TIMER_AddTickHandler(ReelMagic_PicCallback);

	MIXER_UnlockMixerThread();
}

static void set_magic_key(const std::string& key_choice)
{
	if (key_choice == "auto") {
		_initialMagicKey = common_magic_key;
		// default: don't report anything
	} else if (key_choice == "common") {
		_initialMagicKey = common_magic_key;
		LOG_MSG("REELMAGIC: Using the common key: 0x%x", common_magic_key);
	} else if (key_choice == "thehorde") {
		_initialMagicKey = thehorde_magic_key;
		LOG_MSG("REELMAGIC: Using The Horde's key: 0x%x", thehorde_magic_key);
	} else if (unsigned int k; sscanf(key_choice.c_str(), "%x", &k) == 1) {
		_initialMagicKey = k;
		LOG_MSG("REELMAGIC: Using custom key: 0x%x", k);
	} else {
		LOG_WARNING("REELMAGIC: Failed parsing key choice '%s', using built-in routines",
		            key_choice.c_str());
		_initialMagicKey = common_magic_key;
	}
}

static void set_fcode(const int fps_code_choice)
{
	// Default
	constexpr auto default_fps_code = 0;

	if (fps_code_choice == default_fps_code) {
		_magicalFcodeOverride = default_fps_code;
		return;
	}

	auto fps_from_code = [=]() {
		switch (fps_code_choice) {
		case 1: return "23.976";
		case 2: return "24";
		case 3: return "25";
		case 4: return "29.97";
		case 5: return "30";
		case 6: return "50";
		case 7: return "59.94";
		};
		return "unknown"; // should never hit this
	};

	// Override with a valid code
	if (fps_code_choice >= 1 && fps_code_choice <= 7) {
		LOG_MSG("REELMAGIC: Overriding the frame rate to %s FPS (code %d)",
		        fps_from_code(),
		        fps_code_choice);
		_magicalFcodeOverride = fps_code_choice;
		return;
	}

	LOG_WARNING("REELMAGIC: Frame rate code '%d' is not between 0 and 7, using built-in routines",
	            fps_code_choice);
	_magicalFcodeOverride = default_fps_code;
}

void ReelMagic_InitPlayer(Section* sec)
{
	assert(sec);
	const auto section = static_cast<Section_prop*>(sec);
	set_magic_key(section->GetString("reelmagic_key"));

	set_fcode(section->GetInt("reelmagic_fcode"));

	ReelMagic_EnableAudioChannel(true);

	ReelMagic_ClearPlayers();
}

void ReelMagic_ClearPlayers()
{
	ReelMagic_DeleteAllPlayers();

	// set the global configuration default values here...
	ReelMagic_PlayerConfiguration& cfg = _globalDefaultPlayerConfiguration;

	cfg.VideoOutputVisible = true;
	cfg.UnderVga           = false;
	cfg.VgaAlphaIndex      = 0;
	cfg.MagicDecodeKey     = _initialMagicKey;
	cfg.DisplayPosition.X  = 0;
	cfg.DisplayPosition.Y  = 0;
	cfg.DisplaySize.Width  = 0;
	cfg.DisplaySize.Height = 0;
}

ReelMagic_PlayerConfiguration& ReelMagic_GlobalDefaultPlayerConfig()
{
	return _globalDefaultPlayerConfiguration;
}

void REELMAGIC_NotifyLockMixer()
{
	reel_magic_audio.output_queue.Stop();
}
void REELMAGIC_NotifyUnlockMixer()
{
	reel_magic_audio.output_queue.Start();
}
