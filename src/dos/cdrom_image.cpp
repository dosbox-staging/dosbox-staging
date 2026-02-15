// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

// #define DEBUG 1

#include "cdrom.h"
#include "cdrom_mds.h"

#include <cassert>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <vector>

#if !defined(WIN32)
#include <libgen.h>
#else
#include <cstring>
#endif

#include "audio/channel_names.h"
#include "config/setup.h"
#include "dos/drives.h"
#include "utils/fs_utils.h"
#include "utils/math_utils.h"
#include "utils/string_utils.h"

// String maximums, local to this file
#define MAX_LINE_LENGTH 512
#define MAX_FILENAME_LENGTH 256

// STL type shorteners, local to this file
using track_iter       = std::vector<CDROM_Interface_Image::Track>::iterator;
using track_const_iter = std::vector<CDROM_Interface_Image::Track>::const_iterator;
using tracks_size_t    = std::vector<CDROM_Interface_Image::Track>::size_type;

// Ensure the maximum allowed redbook bytes stays within the API type sizes
static_assert(MAX_REDBOOK_BYTES <= UINT32_MAX);

// Report bad seeks that would go beyond the end of the track
bool CDROM_Interface_Image::TrackFile::offsetInsideTrack(const uint32_t offset)
{
	if (static_cast<int>(offset) >= getLength()) {
		LOG_MSG("CDROM: attempted to seek to byte %u, beyond the "
		        "track's %d byte-length",
		        offset, length_redbook_bytes);
		return false;
	}
	return true;
}

// Trim requested read sizes that will spill beyond the track ending
uint32_t CDROM_Interface_Image::TrackFile::adjustOverRead(const uint32_t offset,
                                                          const uint32_t requested_bytes)
{
	// The most likely scenario is read is valid and no adjustment is needed
	uint32_t adjusted_bytes = requested_bytes;

	// If we seek beyond the end of the track, then we can't read any bytes...
	if (static_cast<int>(offset) >= getLength()) {
		adjusted_bytes = 0;
		LOG_MSG("CDROM: can't read audio because requested offset %u "
		        "is beyond the track length, %u",
		        offset, getLength());
	}
	// Otherwise if our seek + requested bytes goes beyond, then prune back
	// the request
	else if (static_cast<int>(offset + requested_bytes) > getLength()) {
		adjusted_bytes = static_cast<uint32_t>(getLength()) - offset;
		LOG_MSG("CDROM: reducing read-length by %u bytes to avoid "
		        "reading beyond track.",
		        requested_bytes - adjusted_bytes);
	}
	return adjusted_bytes;
}

CDROM_Interface_Image::BinaryFile::BinaryFile(const std_fs::path &filename, bool &error)
        : TrackFile(BYTES_PER_RAW_REDBOOK_FRAME),
          file(nullptr)
{
	file = new std::ifstream(filename, std::ios::in | std::ios::binary);
	// If new fails, an exception is generated and scope leaves this constructor
	error = file->fail();
}

CDROM_Interface_Image::BinaryFile::~BinaryFile()
{
	// Guard: only cleanup if needed
	if (file == nullptr)
		return;

	delete file;
	file = nullptr;
}

bool CDROM_Interface_Image::BinaryFile::read(uint8_t *buffer,
                                             const uint32_t offset,
                                             const uint32_t requested_bytes)
{
	// Check for logic bugs and illegal values
	assertm(file && buffer, "The file and/or buffer pointer is invalid");
	assertm(offset <= MAX_REDBOOK_BYTES, "Requested offset exceeds CDROM size");
	assertm(requested_bytes <= MAX_REDBOOK_BYTES, "Requested bytes exceeds CDROM size");

	const uint32_t adjusted_bytes = adjustOverRead(offset, requested_bytes);
	if (adjusted_bytes == 0) // no work to do!
		return true;

	// Reposition if needed
	if (!seek(offset))
		return false;

	file->read((char *)buffer, adjusted_bytes);
	return !file->fail();
}

int CDROM_Interface_Image::BinaryFile::getLength()
{
	// Return our cached result if we've already been asked before
	if (length_redbook_bytes < 0 && file) {
		file->seekg(0, std::ios::end);
		/**
		 *  All read(..) operations involve an absolute position and
		 *  this function isn't called in other threads, therefore
		 *  we don't need to restore the original file position.
		 */
		length_redbook_bytes = static_cast<int>(file->tellg());

		assertm(length_redbook_bytes >= 0,
		        "Track length could not be determined");
		assertm(static_cast<uint32_t>(length_redbook_bytes) <= MAX_REDBOOK_BYTES,
		        "Track length exceeds the maximum CDROM size");
#ifdef DEBUG
		LOG_MSG("CDROM: Length of image is %d bytes", length_redbook_bytes);
#endif
	}
	return length_redbook_bytes;
}

uint16_t CDROM_Interface_Image::BinaryFile::getEndian()
{
	// Image files are always little endian
	return AUDIO_S16LSB;
}

bool CDROM_Interface_Image::BinaryFile::seek(const uint32_t offset)
{
	// Check for logic bugs and illegal values
	assertm(file, "The file pointer needs to be valid, but is the nullptr");
	assertm(offset <= MAX_REDBOOK_BYTES, "Requested offset exceeds CDROM size");

	if (!offsetInsideTrack(offset))
		return false;

	if (static_cast<uint32_t>(file->tellg()) == offset)
		return true;

	file->seekg(offset, std::ios::beg);

	// If the first seek attempt failed, then try harder
	if (file->fail()) {
		file->clear();                      // clear fail and eof bits
		file->seekg(0, std::ios::beg);      // "I have returned."
		file->seekg(offset, std::ios::beg); // "It will be done."
	}
	return !file->fail();
}

uint32_t CDROM_Interface_Image::BinaryFile::decode(int16_t *buffer,
                                                   const uint32_t desired_track_frames)
{
	// Guard against logic bugs and illegal values
	assertm(buffer && file, "The file pointer or buffer are invalid");
	assertm(desired_track_frames <= MAX_REDBOOK_FRAMES,
	        "Requested number of frames exceeds the maximum for a CDROM");
	assertm(audio_pos < MAX_REDBOOK_BYTES,
	        "Tried to decode audio before the playback position was set");

	// Reposition against our last audio position if needed
	if (static_cast<uint32_t>(file->tellg()) != audio_pos)
		if (!seek(audio_pos))
			return 0;

	file->read((char*)buffer, desired_track_frames * BYTES_PER_REDBOOK_PCM_FRAME);
	/**
	 *  Note: gcount returns a signed type, but according to specification:
	 *  "Except in the constructors of std::strstreambuf, negative values of
	 *  std::streamsize are never used."; so we store it as unsigned.
	 */
	const auto bytes_read = static_cast<uint32_t>(file->gcount());

	// decoding is an audio-task, so update our audio position
	audio_pos += bytes_read;

	// Return the number of decoded Redbook frames
	return ceil_udivide(bytes_read, BYTES_PER_REDBOOK_PCM_FRAME);
}

CDROM_Interface_Image::AudioFile::AudioFile(const char *filename, bool &error)
	: TrackFile(4096)
{
	// Use the audio file's sample rate and number of channels as-is
	Sound_AudioInfo desired = {AUDIO_S16, 0, 0};
	sample = Sound_NewSampleFromFile(filename, &desired);
	const std::string filename_only = get_basename(filename);
	if (sample) {
		error = false;
		LOG_MSG("CDROM: Loaded %s [%d Hz, %d-channel, %2.1f minutes]",
		        filename_only.c_str(), getRate(), getChannels(),
		        getLength() / static_cast<double>(REDBOOK_PCM_BYTES_PER_MIN));
	} else {
		LOG_MSG("CDROM: Failed adding '%s' as CDDA track!", filename_only.c_str());
		error = true;
	}
}

CDROM_Interface_Image::AudioFile::~AudioFile()
{
	// Guard to prevent double-free or nullptr free
	if (sample == nullptr)
		return;

	Sound_FreeSample(sample);
	sample = nullptr;
}

/**
 *  Seek takes in a Redbook CD-DA byte offset relative to the track's start
 *  time and returns true if the seek succeeded.
 * 
 *  When dealing with a raw bin/cue file, this requested byte position maps
 *  one-to-one with the bytes in raw binary image, as we see used in the
 *  BinaryTrack::seek() function.  However, when dealing with codec-based
 *  tracks, we need the codec's help to seek to the equivalent redbook position
 *  within the track, regardless of the track's sampling rate, bit-depth,
 *  or number of channels.  To do this, we convert the byte offset to a
 *  time-offset, and use the Sound_Seek() function to move the read position.
 */
bool CDROM_Interface_Image::AudioFile::seek(const uint32_t requested_pos)
{
	// Check for logic bugs and if the track is already positioned as requested
	assertm(sample, "Audio sample needs to be valid, but is the nullptr");
	assertm(requested_pos <= MAX_REDBOOK_BYTES, "Requested offset exceeds CDROM size");

	if (!offsetInsideTrack(requested_pos))
		return false;

	if (audio_pos == requested_pos) {
#ifdef DEBUG
		LOG_MSG("CDROM: seek to %u avoided with position-tracking", requested_pos);
#endif
		return true;
	}

	// Convert the position from a byte offset to time offset, in milliseconds.
	const uint32_t ms_per_s = 1000;
	const uint32_t pos_in_frames = ceil_udivide(requested_pos, BYTES_PER_RAW_REDBOOK_FRAME);
	const uint32_t pos_in_ms = ceil_udivide(pos_in_frames * ms_per_s, REDBOOK_FRAMES_PER_SECOND);

#ifdef DEBUG
	/**
	 *  In DEBUG mode, we additionally measure the seek latency, which can
	 *  be an issue for some codecs. 
	 */
	using namespace std::chrono;
	using clock = std::chrono::steady_clock;
	clock::time_point begin = clock::now(); // start the timer
#endif

	// Perform the seek and update our position
	const bool result = Sound_Seek(sample, pos_in_ms);
	audio_pos = result ? requested_pos : std::numeric_limits<uint32_t>::max();

#ifdef DEBUG
	clock::time_point end = clock::now(); // stop the timer
	const auto elapsed_ms = static_cast<int32_t>
	    (duration_cast<milliseconds>(end - begin).count());

	// Report general seek diagnostics
	const double pos_in_min = static_cast<double>(pos_in_ms) / 60000.0;
	LOG_MSG("CDROM: seeked to byte %u (frame %u at %.2f min), and took %u ms",
	        requested_pos, pos_in_frames, pos_in_min, elapsed_ms);

	/**
	 *  Inform the user if the seek took longer than that of a physical
	 *  CDROM drive, which might have caused in-game symptoms like pauses
	 *  or stuttering.
	 */
	const int32_t average_cdrom_seek_ms = 200;
	if (elapsed_ms > average_cdrom_seek_ms)
		LOG_MSG("CDROM: seek took %d ms, which is slower than an average "
		        "physical CDROM drive's seek time of %d ms.",
		        elapsed_ms, average_cdrom_seek_ms);
#endif

	return result;
}

bool CDROM_Interface_Image::AudioFile::read(uint8_t *buffer,
                                            const uint32_t requested_pos,
                                            const uint32_t requested_bytes)
{
	// Guard again logic bugs and the no-op case
	assertm(buffer != nullptr, "buffer needs to be allocated but is the nullptr");
	assertm(sample != nullptr, "Audio sample needs to be valid, but is the nullptr");
	assertm(requested_pos <= MAX_REDBOOK_BYTES, "Requested offset exceeds CDROM size");
	assertm(requested_bytes <= MAX_REDBOOK_BYTES,
	        "Requested bytes exceeds CDROM size");

	/**
	 *  Support DAE for stereo and mono 44.1 kHz tracks, otherwise inform the user.
	 *  Also, we allow up to 10 DAE-attempts because some CD Player software will query
	 *  this interface before using CDROM-directed playback (not DAE), therefore we
	 *  don't want to fail in those cases.
	 */
	if (getRate() != REDBOOK_PCM_FRAMES_PER_SECOND) {
		static uint8_t dae_attempts = 0;
		if (dae_attempts++ > 10) {
			E_Exit("\n"
			       "CDROM: Digital Audio Extraction (DAE) was attempted with a %u kHz\n"
			       "       track, but DAE is only compatible with %u kHz tracks.",
			       getRate(),
			       REDBOOK_PCM_FRAMES_PER_SECOND);
		}
		return false; // we always correctly return false to the application in this case.
	}

	if (!seek(requested_pos))
		return false;

	const uint32_t adjusted_bytes = adjustOverRead(requested_pos, requested_bytes);
	if (adjusted_bytes == 0) // no work to do!
		return true;

	// Setup characteristics about our track and the request
	const uint8_t channels = getChannels();
	const uint8_t bytes_per_frame = channels * REDBOOK_BPS;
	const uint32_t requested_frames = ceil_udivide(adjusted_bytes,
	                                               BYTES_PER_REDBOOK_PCM_FRAME);

	uint32_t decoded_bytes = 0;
	uint32_t decoded_frames = 0;
	while (decoded_frames < requested_frames) {
		const uint32_t decoded = Sound_Decode_Direct(sample,
		                                             buffer + decoded_bytes,
		                                             requested_frames - decoded_frames);
		if (sample->flags & (SOUND_SAMPLEFLAG_ERROR | SOUND_SAMPLEFLAG_EOF) || !decoded)
			break;
		decoded_frames += decoded;
		decoded_bytes = decoded_frames * bytes_per_frame;
	}
	// Zero out any remainining frames that we didn't fill
	if (decoded_frames < requested_frames)
		memset(buffer + decoded_bytes, 0, adjusted_bytes - decoded_bytes);

	// If the track is mono, convert to stereo
	if (channels == 1 && decoded_frames) {
#ifdef DEBUG
		using clock = std::chrono::steady_clock;
		clock::time_point begin = clock::now(); // start the timer
#endif
		/**
		 *  Convert to stereo in-place:
		 *  The current buffer is half full of mono samples:
		 *  0. [mmmmmmmm00000000]
		 * 
		 *  We walk backward from the last mono sample to the first,
		 *  duplicating them to left and right samples at the rear
		 *  of the buffer:
		 *  1. [mmmmmmm(m)000000(LR)]
		 *              ^________\/
		 * 
		 *  2. [mmmmmm(m)m0000(LR)LR]
		 *             ^_______\/
		 * 
		 *  Eventually the left and right samples overwrite the prior
		 *  mono samples until the buffer is full of LR samples (where
		 *  the first mono samples is simply the 'left' value):
		 * 
		 *  (N - 1). [(m)(R)LRLRLRLRLRLRLR]
		 *             ^_/
		 */
		auto pcm_buffer = reinterpret_cast<int16_t*>(buffer);
		const uint32_t mono_samples = decoded_frames;
		for (uint32_t i = mono_samples - 1; i > 0; --i) {
			pcm_buffer[i * REDBOOK_CHANNELS + 1] = pcm_buffer[i]; // right
			pcm_buffer[i * REDBOOK_CHANNELS + 0] = pcm_buffer[i]; // left
		}

#ifdef DEBUG
		clock::time_point end = clock::now(); // stop the timer
		const int32_t elapsed_us = static_cast<int32_t>
		    (duration_cast<microseconds>(end - begin).count());
		LOG_MSG("CDROM: converted %u mono to %u samples in %d us",
		         mono_samples, mono_samples * REDBOOK_CHANNELS, elapsed_us);
#endif
		// Taken into account that we've now fill both (stereo) channels
		decoded_bytes *= REDBOOK_CHANNELS;
	}
	// reading DAE is an audio-task, so update our audio position
	audio_pos += decoded_bytes;
	return !(sample->flags & SOUND_SAMPLEFLAG_ERROR);
}

uint32_t CDROM_Interface_Image::AudioFile::decode(int16_t *buffer,
                                                  const uint32_t desired_track_frames)
{
	assertm(audio_pos < MAX_REDBOOK_BYTES,
	        "Tried to decode audio before the playback position was set");

	// Sound_Decode_Direct returns frames (agnostic of bitrate and channels)
	const uint32_t frames_decoded =
	    Sound_Decode_Direct(sample, (void*)buffer, desired_track_frames);

	// decoding is an audio-task, so update our audio position
	// in terms of Redbook-equivalent bytes
	const uint32_t redbook_bytes = frames_decoded * BYTES_PER_REDBOOK_PCM_FRAME;
	audio_pos += redbook_bytes;

	return frames_decoded;
}

uint16_t CDROM_Interface_Image::AudioFile::getEndian()
{
	return sample ? sample->actual.format : AUDIO_S16SYS;
}

uint32_t CDROM_Interface_Image::AudioFile::getRate()
{
	return sample ? sample->actual.rate : 0;
}

uint8_t CDROM_Interface_Image::AudioFile::getChannels()
{
	return sample ? sample->actual.channels : 0;
}

int CDROM_Interface_Image::AudioFile::getLength()
{
	if (length_redbook_bytes < 0 && sample) {
		/*
		 *  Sound_GetDuration returns milliseconds but getLength()
		 *  needs to return bytes, so we covert using PCM bytes/s
		 */
		const auto track_ms = Sound_GetDuration(sample);
		const auto track_bytes = static_cast<float>(track_ms) * REDBOOK_PCM_BYTES_PER_MS;
		assert(track_bytes < static_cast<float>(INT32_MAX));
		length_redbook_bytes = static_cast<int32_t>(track_bytes);
	}
	assertm(length_redbook_bytes >= 0,
	        "Track length could not be determined");
	assertm(static_cast<uint32_t>(length_redbook_bytes) <= MAX_REDBOOK_BYTES,
	        "Track length exceeds the maximum CDROM size");
	return length_redbook_bytes;
}

// initialize static members
int CDROM_Interface_Image::refCount = 0;
CDROM_Interface_Image::imagePlayer CDROM_Interface_Image::player;

CDROM_Interface_Image::CDROM_Interface_Image()
        : tracks{},
          readBuffer{},
          mcn("")
{
	if (refCount == 0) {
		if (!player.channel) {
			MIXER_LockMixerThread();
			const auto mixer_callback = std::bind(&CDROM_Interface_Image::CDAudioCallback,
			                                      this, std::placeholders::_1);

			player.channel = MIXER_AddChannel(mixer_callback,
			                                  UseMixerRate,
			                                  ChannelName::CdAudio,
			                                  {ChannelFeature::Stereo,
			                                   ChannelFeature::DigitalAudio});

			player.channel->Enable(false); // only enabled during playback periods
			MIXER_UnlockMixerThread();
		}
#ifdef DEBUG
		LOG_MSG("CDROM: Initialised the %s audio channel", ChannelName::CdAudio);
#endif
	}
	refCount++;
}

CDROM_Interface_Image::~CDROM_Interface_Image()
{
	MIXER_LockMixerThread();
	refCount--;

	// Stop playback before wiping out the CD Player
	if (refCount == 0) {
		LOG_MSG("CDROM: Shutting down CD-DA player");

		if (player.cd) {
			StopAudio();
		}
		MIXER_DeregisterChannel(player.channel);
		player.channel.reset();
	}
	if (player.cd == this) {
		player.cd = nullptr;
	}
	MIXER_UnlockMixerThread();
}

bool CDROM_Interface_Image::SetDevice(const char* path)
{
	std::lock_guard lock(player.mutex);
	const bool result = LoadMdsFile(path) || LoadCueSheet(path) || LoadIsoFile(path);
	if (!result) {
		// print error message on dosbox console
		char buf[MAX_LINE_LENGTH];
		snprintf(buf, MAX_LINE_LENGTH, "Could not load image file: %s\r\n", path);
		auto size = static_cast<uint16_t>(strlen(buf));
		DOS_WriteFile(STDOUT, (uint8_t*)buf, &size);
	}
	return result;
}

bool CDROM_Interface_Image::GetUPC(unsigned char& attr, std::string& upc)
{
	attr = 0;
	upc = mcn;
#ifdef DEBUG
	LOG_MSG("CDROM: GetUPC => returned %s", upc);
#endif
	return true;
}

bool CDROM_Interface_Image::GetAudioTracks(uint8_t& start_track_num,
                                           uint8_t& end_track_num,
                                           TMSF& lead_out_msf)
{
	/**
	 *  Guard: A valid CD has atleast two tracks: the first plus the lead-out,
	 *  so bail out if we have fewer than 2 tracks
	 */
	if (tracks.size() < MIN_REDBOOK_TRACKS) {
#ifdef DEBUG
		LOG_MSG("CDROM: GetAudioTracks: game wanted to dump track "
		        "metadata but our CD image has too few tracks: %u",
		        static_cast<unsigned int>(tracks.size()));
#endif
		return false;
	}
	start_track_num = tracks.front().number;
	end_track_num = next(tracks.crbegin())->number; // next(crbegin) == [vec.size - 2]
	lead_out_msf = frames_to_msf(tracks.back().start + REDBOOK_FRAME_PADDING);
#ifdef DEBUG
	LOG_MSG("CDROM: GetAudioTracks => start track is %2d, last playable track is %2d, "
	        "and lead-out MSF is %02d:%02d:%02d",
	        start_track_num,
	        end_track_num,
	        lead_out_msf.min,
	        lead_out_msf.sec,
	        lead_out_msf.fr);
#endif
	return true;
}

bool CDROM_Interface_Image::GetAudioTrackInfo(uint8_t requested_track_num,
                                              TMSF& start_msf,
                                              unsigned char& attr)
{
	if (tracks.size() < MIN_REDBOOK_TRACKS
	    || requested_track_num < 1
	    || requested_track_num > 99
	    || requested_track_num >= tracks.size()) {
#ifdef DEBUG
		LOG_MSG("CDROM: GetAudioTrackInfo for track %u => "
		        "outside the CD's track range [1 to %" PRIuPTR ")",
		        requested_track_num,
		        tracks.size());
#endif
		return false;
	}

	const int requested_track_index = static_cast<int>(requested_track_num) - 1;
	track_const_iter track = tracks.begin() + requested_track_index;
	start_msf = frames_to_msf(track->start + REDBOOK_FRAME_PADDING);
	attr = track->attr;
#ifdef DEBUG
	LOG_MSG("CDROM: GetAudioTrackInfo for track %u => "
	        "MSF %02d:%02d:%02d, which is sector %d",
	        requested_track_num,
	        start_msf.min,
	        start_msf.sec,
	        start_msf.fr,
	        msf_to_frames(start_msf));
#endif
	return true;
}

bool CDROM_Interface_Image::GetAudioSub(unsigned char& attr,
                                        unsigned char& track_num,
                                        unsigned char& index,
                                        TMSF& relative_msf,
                                        TMSF& absolute_msf) {
	// Setup valid defaults to handle all scenarios
	attr = 0;
	track_num = 1;
	index = 1;
	uint32_t absolute_sector = 0;
	uint32_t relative_sector = 0;

	if (!tracks.empty()) { 	// We have a useable CD; get a valid play-position
		auto track = tracks.begin();
		// the CD's current track is valid

		 // reserve the track_file as a shared_ptr to avoid deletion in another thread
		const auto track_file = player.trackFile.lock();
		if (track_file) {
			LagDriveResponse();
			const uint32_t sample_rate = track_file->getRate();
			const uint32_t played_frames = ceil_udivide(player.playedTrackFrames
			                               * REDBOOK_FRAMES_PER_SECOND, sample_rate);
			absolute_sector = player.startSector + played_frames;
			track_iter current_track = GetTrack(absolute_sector);
			if (current_track != tracks.end()) {
				track = current_track;
				relative_sector = absolute_sector >= track->start ?
				                  absolute_sector - track->start : 0;
			} else { // otherwise fallback to the beginning track
				absolute_sector = track->start;
				// relative_sector is zero because we're at the start of the track
			}
			// the CD hasn't been played yet or has an invalid
			// audio_pos
		} else {
			for (auto it = tracks.begin(); it != tracks.end(); ++it) {
				if (it->attr == 0) {	// Found an audio track
					track = it;
					absolute_sector = it->start;
					break;
				} // otherwise fallback to the beginning track
			}
		}
		attr = track->attr;
		track_num = track->number;
	}
	absolute_msf = frames_to_msf(absolute_sector + REDBOOK_FRAME_PADDING);
	relative_msf = frames_to_msf(relative_sector);
#ifdef DEBUG
	LOG_MSG("CDROM: GetAudioSub => position at %02d:%02d:%02d (on sector %u) "
	        "within track %u at %02d:%02d:%02d (at its sector %u)",
	        absolute_msf.min, absolute_msf.sec, absolute_msf.fr,
	        absolute_sector + REDBOOK_FRAME_PADDING, track_num, relative_msf.min,
	        relative_msf.sec, relative_msf.fr, relative_sector);
#endif
	return true;
}

bool CDROM_Interface_Image::GetAudioStatus(bool& playing, bool& pause)
{
	playing = player.isPlaying;
	pause = player.isPaused;
#ifdef DEBUG
	LOG_MSG("CDROM: GetAudioStatus => %s and %s",
	        playing ? "is playing" : "stopped",
	        pause ? "paused" : "not paused");
#endif
	return true;
}

bool CDROM_Interface_Image::GetMediaTrayStatus(bool& mediaPresent, bool& mediaChanged, bool& trayOpen)
{
	mediaPresent = true;
	mediaChanged = false;
	trayOpen = false;
#ifdef DEBUG
	LOG_MSG("CDROM: GetMediaTrayStatus => media is %s, %s, and the tray is %s",
	        mediaPresent ? "present" : "not present",
	        mediaChanged ? "was changed" : "hasn't been changed",
	        trayOpen ? "open" : "closed");
#endif
	return true;
}

bool CDROM_Interface_Image::PlayAudioTrack(const Track& track, const uint32_t sector_offset)
{
	const auto track_file = track.file;
	if (!track_file || track.attr == 0x40 || !player.channel) {
		StopAudio();
		return false;
	}

	const auto byte_offset = track.skip + sector_offset * track.sector_size;

	// Guard: Bail if our track could not be seeked
	if (!track_file->seek(byte_offset)) {
		LOG_MSG("CDROM: Track %d failed to seek to byte %u, so cancelling playback",
		        track.number, byte_offset);
		StopAudio();
		return false;
	}

	// We're performing an audio-task, so update the audio position
	track_file->setAudioPosition(byte_offset);

	// Get properties about the current track
	const uint8_t track_channels = track_file->getChannels();
	const uint32_t track_rate = track_file->getRate();

	// Update our player with properties about this playback sequence
	player.cd = this;
	player.trackFile = track_file;
	player.isPlaying = true;
	player.isPaused = false;
	currentTrackIndex = track.number - 1;

	// Assign the mixer function associated with this track's content type
	if (track_file->getEndian() == AUDIO_S16SYS) {
		player.addFrames = (track_channels == 2)
		                         ? &MixerChannel::AddSamples_s16
		                         : &MixerChannel::AddSamples_m16;
	} else {
		player.addFrames = (track_channels == 2)
		                         ? &MixerChannel::AddSamples_s16_nonnative
		                         : &MixerChannel::AddSamples_m16_nonnative;
	}

	// start the channel!
	player.channel->SetSampleRate(track_rate);
	player.channel->Enable(true);
	return true;
}

bool CDROM_Interface_Image::PlayAudioSector(uint32_t start, uint32_t len)
{
	std::lock_guard lock(player.mutex);

	// Find the track that holds the requested sector
	track_const_iter track = GetTrack(start);

	// Guard: sanity check the request beyond what GetTrack already checks
	if (len == 0 || track == tracks.end()) {
		StopAudio();
#ifdef DEBUG
		LOG_MSG("CDROM: PlayAudioSector => sanity check failed");
#endif
		return false;
	}
	// If the request falls into the pregap, which is prior to the track's
	// actual start but not so earlier that it falls into the prior track's
	// audio, then we simply skip the pre-gap (because we can't negatively
	// seek into the track) and instead start playback at the actual track
	// start.
	if (start < track->start) {
		len -= (track->start - start);
		start = track->start;
	}

	const auto sector_offset = start - track->start;
	if (!PlayAudioTrack(*track, sector_offset)) {
		return false;
	}

	player.startSector = start;

	/**
	 *  Convert Redbook frames (len) to Track PCM frames, rounding up to whole
	 *  integer frames. Note: the intermediate numerator in the calculation
	 *  below can overflow uint32_t, so the variable types used must stay
	 *  64-bit.
	 */
	player.playedTrackFrames = 0;
	player.totalTrackFrames = len *
	                          (track->file->getRate() / REDBOOK_FRAMES_PER_SECOND);

#ifdef DEBUG
	if (start < track->start) {
		LOG_MSG("CDROM: Play sector %u to %u in the pregap of track %d [pregap %d,"
		        " start %u, end %u] for %u PCM frames at rate %u",
		        start,
		        start + len,
		        track->number,
		        prev(track)->start - prev(track)->length,
		        track->start,
		        track->start + track->length,
		        player.totalTrackFrames,
		        track_rate);
	} else {
		LOG_MSG("CDROM: Play sector %u to %u in track %d [start %u, end %u],"
		        " for %u PCM frames at rate %u",
		        start,
		        start + len,
		        track->number,
		        track->start,
		        track->start + track->length,
		        player.totalTrackFrames,
		        track_rate);
	}
#endif

	return true;
}

bool CDROM_Interface_Image::PauseAudio(bool resume)
{
	player.isPaused = !resume;
	if (player.channel) {
		player.channel->Enable(resume);
	}
#ifdef DEBUG
	LOG_MSG("CDROM: PauseAudio => audio is now %s",
	        resume ? "unpaused" : "paused");
#endif
	return true;
}

bool CDROM_Interface_Image::StopAudio(void)
{
	player.isPlaying = false;
	player.isPaused = false;
	if (player.channel) {
		player.channel->Enable(false);
	}
#ifdef DEBUG
	LOG_MSG("CDROM: StopAudio => stopped playback and halted the mixer");
#endif
	return true;
}

void CDROM_Interface_Image::ChannelControl(TCtrl ctrl)
{
	// Guard: Bail if our mixer channel hasn't been allocated
	if (!player.channel) {
#ifdef DEBUG
		LOG_MSG("CDROM: ChannelControl => game tried applying channel controls "
		        "before playing audio");
#endif
		return;
	}
	// Adjust the volume of our mixer channel as defined by the application
	player.channel->SetAppVolume({ctrl.vol[0] / 255.0f, ctrl.vol[1] / 255.0f});

	// Map the audio channels in our mixer channel as defined by the application
	const auto left_mapped = static_cast<LineIndex>(ctrl.out[0]);
	const auto right_mapped = static_cast<LineIndex>(ctrl.out[1]);
	player.channel->SetChannelMap({left_mapped, right_mapped});

#ifdef DEBUG
	LOG_MSG("CDROM: ChannelControl => volumes %d/255 and %d/255, "
	        "and left-right map %d, %d",
	        ctrl.vol[0],
	        ctrl.vol[1],
	        ctrl.out[0],
	        ctrl.out[1]);
#endif
}

bool CDROM_Interface_Image::ReadSectors(PhysPt buffer,
                                        const bool raw,
                                        const uint32_t sector,
                                        const uint16_t num)
{
	const uint16_t sector_size = (raw ? BYTES_PER_RAW_REDBOOK_FRAME
	                                 : BYTES_PER_COOKED_REDBOOK_FRAME);
	const uint32_t requested_bytes = num * sector_size;

	// Resize our underlying vector if it's not big enough
	if (readBuffer.size() < requested_bytes)
		readBuffer.resize(requested_bytes);

	// Setup state-tracking variables to be used in the read-loop
	bool success = true; //Gobliiins reads 0 sectors
	uint32_t bytes_read = 0;
	uint32_t current_sector = sector;
	uint8_t* buffer_position = readBuffer.data();

	// Read until we have enough or fail
	while (bytes_read < requested_bytes) {
		success = ReadSector(buffer_position, raw, current_sector);
		if (!success)
			break;
		current_sector++;
		bytes_read += sector_size;
		buffer_position += sector_size;
	}
	// Write only the successfully read bytes
	MEM_BlockWrite(buffer, readBuffer.data(), bytes_read);
#ifdef DEBUG
	LOG_MSG("CDROM: Read %u %s sectors at sector %u: "
	        "%s after %u sectors (%u bytes)",
	        num, raw ? "raw" : "cooked", sector,
	        success ? "Succeeded" : "Failed",
	        ceil_udivide(bytes_read, sector_size), bytes_read);
#endif
	return success;
}

bool CDROM_Interface_Image::LoadUnloadMedia(bool /*unload*/)
{
	return true;
}

track_iter CDROM_Interface_Image::GetTrack(const uint32_t sector)
{
	// Guard if we have no tracks or the sector is beyond the lead-out
	if (sector > MAX_REDBOOK_SECTOR ||
	    tracks.size() < MIN_REDBOOK_TRACKS ||
	    sector >= tracks.back().start) {
		LOG_MSG("CDROM: GetTrack at sector %u is outside the"
		        " playable range", sector);
		return tracks.end();
	}

	/**
	 *  Walk the tracks checking if the desired sector falls inside of a given
	 *  track's range, which starts at the end of the prior track and goes to
	 *  the current track's (start + length).
	 */
	track_iter track = tracks.begin();
	uint32_t lower_bound = track->start;
	while (track != tracks.end()) {
		const uint32_t upper_bound = track->start + track->length;
		if (lower_bound <= sector && sector < upper_bound) {
			break;
		}
		++track;
		lower_bound = upper_bound;
	}
#ifdef DEBUG
	if (track != tracks.end() && track->number != 1) {
		if (sector < track->start) {
			LOG_MSG("CDROM: GetTrack at sector %d => in the pregap of "
			        "track %d [pregap %d, start %d, end %d]",
			        sector,
			        track->number,
			        prev(track)->start - prev(track)->length,
			        track->start,
			        track->start + track->length);
		} else {
			LOG_MSG("CDROM: GetTrack at sector %d => track %d [start %d, end %d]",
			        sector,
			        track->number,
			        track->start,
			        track->start + track->length);
		}
	}
#endif
	return track;
}

bool CDROM_Interface_Image::ReadSector(uint8_t *buffer, const bool raw, const uint32_t sector)
{
	track_const_iter track = GetTrack(sector);

	// Guard: Bail if the requested sector fell outside our tracks
	if (track == tracks.end() || track->file == nullptr) {
#ifdef DEBUG
		LOG_MSG("CDROM: ReadSector at %u => resulted "
		        "in an invalid track or track->file",
		        sector);
#endif
		return false;
	}
	uint32_t offset = track->skip + (sector - track->start) * track->sector_size;
	const uint16_t length = (raw ? BYTES_PER_RAW_REDBOOK_FRAME : BYTES_PER_COOKED_REDBOOK_FRAME);
	// Subchannel is trailing meta-data only used by MDS/MDF files.
	// Its size is part of the sector for the purposes of calculating stride.
	// We don't ever read this data so for the purposes of this code, we need the size excluding the subchannel.
	assert(track->subchannel_size < track->sector_size);
	const auto sector_size_without_subchannel = track->sector_size - track->subchannel_size;
	if (sector_size_without_subchannel != BYTES_PER_RAW_REDBOOK_FRAME && raw) {
		return false;
	}
	if (sector_size_without_subchannel == BYTES_PER_RAW_REDBOOK_FRAME && !track->mode2 && !raw)
		offset += 16;
	if (track->mode2 && !raw)
		offset += 24;

#if 0 // Excessively verbose.. only enable if needed
#ifdef DEBUG
	LOG_MSG("CDROM: ReadSector track %2d, desired raw %s, sector %ld, length=%d",
	        track->number,
	        raw ? "true":"false",
	        sector,
	        length);
#endif
#endif
	return track->file->read(buffer, offset, length);
}

bool CDROM_Interface_Image::ReadSectorsHost(void *buffer, bool raw, unsigned long sector, unsigned long num)
{
	unsigned int sector_size = raw ? BYTES_PER_RAW_REDBOOK_FRAME : BYTES_PER_COOKED_REDBOOK_FRAME;
	bool success = true; //Gobliiins reads 0 sectors
	for(unsigned long i = 0; i < num; i++) {
		success = ReadSector((uint8_t*)buffer + (i * (Bitu)sector_size), raw, sector + i);
		if (!success) break;
	}

	return success;
}

void CDROM_Interface_Image::PlayNextAudioTrack()
{
	const auto next_track_index = currentTrackIndex + 1;
	if (next_track_index >= tracks.size()) {
		StopAudio();
		return;
	}

	const auto &next_track = tracks[next_track_index];
	assert(next_track.number == next_track_index + 1);

	PlayAudioTrack(next_track, 0);
}

void CDROM_Interface_Image::CDAudioCallback(const int desired_track_frames)
{
	/**
	 *  This callback runs in SDL's mixer thread, so there's a risk
	 *  our track_file pointer could be removed by the main thread.
	 *  We reserve the track_file up-front for the scope of this call.
	 */
	std::lock_guard lock(player.mutex);
	std::shared_ptr<TrackFile> track_file = player.trackFile.lock();

	// Guards: Bail if the request or our player is invalid
	if (desired_track_frames == 0 || !player.cd || !track_file) {
#ifdef DEBUG
		LOG_MSG("CDROM: CDAudioCallback called with one more empty dependencies:\n"
		        "\t - frames to play (%" PRIuPTR ")\n"
		        "\t - pointer to the CD object (%p)\n"
		        "\t - pointer to the track's file (%p)\n",
		        desired_track_frames, static_cast<void *>(player.cd),
		        static_cast<void *>(track_file.get()));
#endif
		if (player.cd)
			player.cd->StopAudio();
		return;
	}

	const auto decoded_track_frames = check_cast<uint16_t>(
	        track_file->decode(player.buffer, desired_track_frames));

	if (!decoded_track_frames) {
		// This particular CDDA track has come to an end, but the
		// program has requested we continue playing for a longer
		// period. So keep going!
		const auto frames_remaining = (player.playedTrackFrames <
		                               player.totalTrackFrames)
		                                    ? (player.totalTrackFrames -
		                                       player.playedTrackFrames)
		                                    : 0;
		const auto num_frames_in_two_seconds = track_file->getRate() * 2;
		if (frames_remaining < num_frames_in_two_seconds) {
			// Less than 2 seconds remain of requested playback.
			// Stop the audio rather than playing the next track for
			// only a few seconds. Fixes Alone in the Dark 2:
			// https://github.com/dosbox-staging/dosbox-staging/issues/4445
			player.cd->StopAudio();
			return;
		} else {
			player.cd->PlayNextAudioTrack();
			return;
		}
	}

	// Use the stereo or mono and native or nonnative AddSamples call
	// assigned during construction
	(player.channel.get()->*player.addFrames)(decoded_track_frames, player.buffer);

	player.playedTrackFrames += decoded_track_frames;
	if (player.playedTrackFrames >= player.totalTrackFrames) {
#ifdef DEBUG
		LOG_MSG("CDROM: CDAudioCallback stopping because "
		"playedTrackFrames (%u) >= totalTrackFrames (%u)",
		player.playedTrackFrames, player.totalTrackFrames);
#endif
		player.cd->StopAudio();
	}
}

bool CDROM_Interface_Image::LoadIsoFile(const char* filename)
{
	tracks.clear();

	// data track (track 1)
	Track track = {};
	bool error  = false;
	track.file  = std::make_shared<BinaryFile>(filename, error);

	if (error) { //-V547
		return false;
	}
	track.number = 1;
	track.attr = 0x40;//data

	// try to detect iso type
	if (CanReadPVD(track.file.get(), BYTES_PER_COOKED_REDBOOK_FRAME, false)) {
		track.sector_size = BYTES_PER_COOKED_REDBOOK_FRAME;
		assert(track.mode2 == false);
	} else if (CanReadPVD(track.file.get(), BYTES_PER_RAW_REDBOOK_FRAME, false)) {
		track.sector_size = BYTES_PER_RAW_REDBOOK_FRAME;
		assert(track.mode2 == false);
	} else if (CanReadPVD(track.file.get(), 2336, true)) {
		track.sector_size = 2336;
		track.mode2 = true;
	} else if (CanReadPVD(track.file.get(), BYTES_PER_RAW_REDBOOK_FRAME, true)) {
		track.sector_size = BYTES_PER_RAW_REDBOOK_FRAME;
		track.mode2 = true;
	} else {
		return false;
	}
	const int32_t track_bytes = track.file->getLength();
	if (track_bytes < 0)
		return false;

	track.length = static_cast<uint32_t>(track_bytes) / track.sector_size;

#ifdef DEBUG
	LOG_MSG("LoadIsoFile parsed %s => track 1, 0x40, sector_size %d, mode2 is %s",
	        filename,
	        track.sector_size,
	        track.mode2 ? "true":"false");
#endif

	tracks.push_back(track);

	// lead-out track (track 2)
	Track leadout_track;
	leadout_track.number = 2;
	leadout_track.start = track.length;
	tracks.push_back(leadout_track);
	return true;
}

bool CDROM_Interface_Image::CanReadPVD(TrackFile *file,
                                       const uint16_t sector_size,
                                       const bool mode2)
{
	// Guard: Bail if our file pointer is empty
	if (file == nullptr) return false;

	// Initialize our array in the event file->read() doesn't fully write it
	uint8_t pvd[BYTES_PER_COOKED_REDBOOK_FRAME] = {0};

	uint32_t seek = 16 * sector_size;  // first vd is located at sector 16
	if (sector_size == BYTES_PER_RAW_REDBOOK_FRAME && !mode2) seek += 16;
	if (mode2) seek += 24;
	file->read(pvd, seek, BYTES_PER_COOKED_REDBOOK_FRAME);
	// pvd[0] = descriptor type, pvd[1..5] = standard identifier,
	// pvd[6] = iso version (+8 for High Sierra)
	return ((pvd[0] == 1 && !strncmp((char*)(&pvd[1]), "CD001", 5) && pvd[6]  == 1) ||
	        (pvd[8] == 1 && !strncmp((char*)(&pvd[9]), "CDROM", 5) && pvd[14] == 1));
}

#if defined(WIN32)
static std::string dirname(char* file)
{
	char * sep = strrchr(file, '\\');
	if (sep == nullptr)
		sep = strrchr(file, '/');
	if (sep == nullptr)
		return "";
	else {
		int len = (int)(sep - file);
		char tmp[MAX_FILENAME_LENGTH];
		safe_strncpy(tmp, file, len+1);
		return tmp;
	}
}
#endif

static std::optional<MdsHeader> read_and_validate_mds_header(std::ifstream &file)
{
	const auto mds_header = read_mds_header(file);
	if (!mds_header) {
		return {};
	}

	constexpr uint8_t MdsSignature[] = {'M', 'E', 'D', 'I', 'A', ' ', 'D', 'E', 'S', 'C', 'R', 'I', 'P', 'T', 'O', 'R'};
	static_assert(sizeof(mds_header->signature) == sizeof(MdsSignature));

	if (memcmp(mds_header->signature, MdsSignature, sizeof(MdsSignature))) {
		// Not an MDS file. Fall through to CUE/ISO handlers.
		return {};
	}

	// Version field is a 2 character array in the format of major.minor.
	// We don't care about the minor version but the major version must be one.
	// This is what is produced by Alcohol 120%.
	if (mds_header->version[0] != 1) {
		LOG_ERR("CDROM: Invalid MDS version: %hhu.%hhu", mds_header->version[0], mds_header->version[1]);
		return {};
	}

	if (mds_header->num_sessions == 0) {
		LOG_ERR("CDROM: Invalid MDS file");
		return {};
	}
	if (mds_header->num_sessions > 1) {
		LOG_WARNING("CDROM: MDS/MDF file contains %hu sessions. Only the first will be used.", mds_header->num_sessions);
	}
	if (mds_header->session_block_offset == 0) {
		LOG_ERR("CDROM: Invalid MDS file");
		return {};
	}

	return mds_header;
}

static std::optional<MdsSessionBlock> read_and_validate_mds_session_block(std::ifstream &file, const std::ifstream::pos_type pos)
{
	const auto session_block = read_mds_session_block(file, pos);
	if (!session_block) {
		LOG_ERR("CDROM: Invalid MDS file");
		return {};
	}
	if (session_block->num_all_blocks == 0) {
		LOG_ERR("CDROM: Invalid MDS file");
		return {};
	}
	if (session_block->track_block_offset == 0) {
		LOG_ERR("CDROM: Invalid MDS file");
		return {};
	}

	return session_block;
}

static bool set_track_mode(CDROM_Interface_Image::Track &track, uint8_t mode)
{
	mode &= 0x0F;
	if (mode >= 8) {
		// Modes 8-15 are duplicates of modes 0-7
		mode -= 8;
	}
	switch (mode) {
		// Audio track
		case 1:
			track.attr = 0;
			track.mode2 = false;
			break;
		// Mode 2 Form 1
		case 4:
		// Mode 2 Form 2
		case 5:
		// Unknown
		case 6:
			// Form 1/2 are CDROM-XA modes which will need deeper integration to support.
			LOG_ERR("CDROM: Unsupported mode: %hhu", mode);
			return false;

		// Mode 1 data track
		case 2:
			track.attr = 0x40;
			track.mode2 = false;
			break;

		// Mode 2 data track (0, 3, 7 appear to have the same meaning)
		case 0:
		case 3:
		case 7:
			track.attr = 0x40;
			track.mode2 = true;
			break;
		default:
			assertm(false, "Unhandled case (should never happen due to *mode &= 0x0F)");
			return false;
	}

	return true;
}

static std_fs::path read_mdf_filename(std::ifstream &file, const MdsFooter footer)
{
	file.seekg(footer.filename_offset);
	if (file.fail()) {
		LOG_ERR("CDROM: Invalid MDS file");
		return {};
	}
	if (footer.widechar_filename) {
		std::u16string utf16_string = {};
		while (true) {
			uint16_t wide_char = 0;
			file.read(reinterpret_cast<char*>(&wide_char), sizeof(wide_char));
			if (wide_char == 0) {
				return utf16_string;
			}
			utf16_string.push_back(le16_to_host(wide_char));
		}
	} else {
		std::string ascii_string = {};
		while (true) {
			char c = 0;
			file.read(reinterpret_cast<char*>(&c), sizeof(c));
			if (c == 0) {
				return ascii_string;
			}
			ascii_string.push_back(c);
		}
	}
}

// Simplified version of an MDS parser.
// Does not handle copy protection.
// Credit to reverse engineering efforts by cdemu/libmirage.
// Code is not taken directly from the project but I read their code:
// https://github.com/cdemu/cdemu/blob/master/libmirage/images/image-mds/parser.c
bool CDROM_Interface_Image::LoadMdsFile(const char *mds_filename)
{
	const std_fs::path mds_path(to_native_path(mds_filename));
	std::ifstream file(mds_path, std::ios::in | std::ios::binary);

	const auto mds_header = read_and_validate_mds_header(file);
	if (!mds_header) {
		return false;
	}

	const auto session_block = read_and_validate_mds_session_block(file, mds_header->session_block_offset);
	if (!session_block) {
		return false;
	}

	std::unordered_map<std_fs::path, std::shared_ptr<TrackFile>> track_map = {};
	for (uint32_t i = 0; i < session_block->num_all_blocks; ++i) {
		const auto track_block = read_mds_track_block(file, session_block->track_block_offset + (sizeof(MdsTrackBlock) * i));
		if (!track_block) {
			LOG_ERR("CDROM: Invalid MDS file");
			return false;
		}
		if (track_block->point < 1 || track_block->point > 99) {
			// This is a non-track block. cdemu simply skips these.
			continue;
		}
		Track track = {};
		track.number = track_block->point;
		set_track_mode(track, track_block->mode);
		switch (track_block->subchannel) {
			case 0:
				track.subchannel_size = 0;
				break;
			case 8:
				track.subchannel_size = 96;
				break;
			default:
				LOG_WARNING("CDROM: Unknown subchannel type %hhu assuming subchannel size of 0", track_block->subchannel);
				track.subchannel_size = 0;
				break;
		}
		track.sector_size = track_block->sector_size;
		if (track.subchannel_size >= track.sector_size) {
			LOG_ERR("CDROM: Invalid sector/subchannel size. Sector size: %hu Subchannel size: %hu", track.sector_size, track.subchannel_size);
			return false;
		}
		track.start = track_block->start_sector;
		track.skip = check_cast<uint32_t>(track_block->start_offset);
		if (track_block->number_of_files != 1) {
			// According to comments in cdemu, DVD tracks can be split into multiple files.
			// We don't care about DVDs and CDROM should always be 1 file per track.
			LOG_ERR("CDROM: %u files in track %hhu. Must be exactly 1.", track_block->number_of_files, track.number);
			return false;
		}
		if (track_block->footer_offset == 0 || track_block->extra_offset == 0) {
			LOG_ERR("CDROM: Invalid MDS file");
			return false;
		}
		const auto extra_block = read_mds_extra_block(file, track_block->extra_offset);
		if (!extra_block) {
			LOG_ERR("CDROM: Invalid MDS file");
			return false;
		}
		track.length = extra_block->length;
		const auto footer = read_mds_footer(file, track_block->footer_offset);
		if (!footer) {
			LOG_ERR("CDROM: Invalid MDS file");
			return false;
		}
		if (footer->filename_offset == 0) {
			LOG_ERR("CDROM: Invalid MDS file");
			return false;
		}
		const uint8_t prev_track = tracks.empty() ? 0 : tracks.back().number; //-V807
		const uint32_t prev_sector = tracks.empty() ? 0 : tracks.back().start + tracks.back().length;
		if (track.number != prev_track + 1 || track.start < prev_sector) {
			LOG_ERR("CDROM: Non-contigious track found in MDS file");
			return false;
		}
		auto mdf_filename = read_mdf_filename(file, *footer);
		if (mdf_filename.empty()) {
			LOG_ERR("CDROM: Missing MDF filename");
			return false;
		}
		if (mdf_filename == "*.mdf") {
			mdf_filename = mds_path;
			mdf_filename.replace_extension("mdf");
		} else {
			mdf_filename = mds_path.parent_path() / mdf_filename;
		}
		if (const auto iter = track_map.find(mdf_filename); iter != track_map.end()) {
			track.file = iter->second;
		} else {
			bool error = false;
			track.file = std::make_shared<BinaryFile>(mdf_filename, error);
			if (error) { //-V547
				LOG_ERR("CDROM: Failed to open MDF file: %s", mdf_filename.string().c_str());
				return false;
			}
			track_map.try_emplace(mdf_filename, track.file);
		}
		tracks.push_back(track);
	}
	if (tracks.empty()) {
		LOG_ERR("CDROM: Failed to find any tracks");
		return false;
	}
	Track lead_out = {};
	lead_out.start = tracks.back().start + tracks.back().length;
	tracks.push_back(lead_out);
	return true;
}

bool CDROM_Interface_Image::LoadCueSheet(const char *cuefile)
{
	tracks.clear();

	Track track = {};
	uint32_t shift = 0;
	uint32_t currPregap = 0;
	uint32_t totalPregap = 0;
	int32_t prestart = -1;
	int track_number;
	bool success;
	bool canAddTrack = false;
	char tmp[MAX_FILENAME_LENGTH];  // dirname can change its argument
	safe_strcpy(tmp, cuefile);
	std::string pathname(dirname(tmp));
	std::ifstream in;
	in.open(to_native_path(cuefile), std::ios::in);
	if (in.fail()) {
		return false;
	}

	while (!in.eof()) {
		// get next line
		char buf[MAX_LINE_LENGTH];
		in.getline(buf, MAX_LINE_LENGTH);
		if (in.fail() && !in.eof()) {
			return false;  // probably a binary file
		}
		std::istringstream line(buf);

		std::string command;
		GetCueKeyword(command, line);

		if (command == "TRACK") {
			if (canAddTrack) success = AddTrack(track, shift, prestart, totalPregap, currPregap);
			else success = true;

			track.start = 0;
			track.skip = 0;
			currPregap = 0;
			prestart = -1;

			line >> track_number; // (cin) read into a true int first
			track.number = static_cast<uint8_t>(track_number);
			std::string type;
			GetCueKeyword(type, line);

			if (type == "AUDIO") {
				track.sector_size = BYTES_PER_RAW_REDBOOK_FRAME;
				track.attr = 0;
				track.mode2 = false;
			} else if (type == "MODE1/2048") {
				track.sector_size = BYTES_PER_COOKED_REDBOOK_FRAME;
				track.attr = 0x40;
				track.mode2 = false;
			} else if (type == "MODE1/2352") {
				track.sector_size = BYTES_PER_RAW_REDBOOK_FRAME;
				track.attr = 0x40;
				track.mode2 = false;
			} else if (type == "MODE2/2336") {
				track.sector_size = 2336;
				track.attr = 0x40;
				track.mode2 = true;
			} else if (type == "MODE2/2352") {
				track.sector_size = BYTES_PER_RAW_REDBOOK_FRAME;
				track.attr = 0x40;
				track.mode2 = true;
			} else success = false;

			canAddTrack = true;
		}
		else if (command == "INDEX") {
			int index;
			line >> index;
			uint32_t frame;
			success = GetCueFrame(frame, line);

			if (index == 1) track.start = frame;
			else if (index == 0) prestart = static_cast<int32_t>(frame);
			// ignore other indices
		}
		else if (command == "FILE") {
			if (canAddTrack) success = AddTrack(track, shift, prestart, totalPregap, currPregap);
			else success = true;
			canAddTrack = false;

			std::string filename;
			GetCueString(filename, line);
			GetRealFileName(filename, pathname);
			std::string type;
			GetCueKeyword(type, line);

			bool error = true;
			if (type == "BINARY") {
				track.file = std::make_shared<BinaryFile>(
				        filename, error);
			} else {
				track.file = std::make_shared<AudioFile>(
				        filename.c_str(), error);
				/**
				 *  SDL_Sound first tries using a decoder having a matching
				 *  registered extension as the filename, and then falls back to
				 *  trying each decoder before finally giving up.
				 */
			}
			if (error) { //-V547
				success = false;
			}
		}
		else if (command == "PREGAP") success = GetCueFrame(currPregap, line);
		else if (command == "CATALOG") success = GetCueString(mcn, line);
		// ignored commands
		else if (command == "CDTEXTFILE" || command == "FLAGS"   || command == "ISRC" ||
		         command == "PERFORMER"  || command == "POSTGAP" || command == "REM" ||
		         command == "SONGWRITER" || command == "TITLE"   || command.empty()) {
			success = true;
		}
		// failure
		else {
			success = false;
		}
		if (!success) {
			return false;
		}
	}
	// add last track
	if (!AddTrack(track, shift, prestart, totalPregap, currPregap)) {
		return false;
	}

	// add lead-out track
	track.number++;
	track.attr = 0;//sync with load iso
	track.start = 0;
	track.length = 0;
	track.file.reset();
	if (!AddTrack(track, shift, -1, totalPregap, 0)) {
		return false;
	}
	return true;
}



bool CDROM_Interface_Image::AddTrack(Track &curr,
                                     uint32_t &shift,
                                     const int32_t prestart,
                                     uint32_t &totalPregap,
                                     uint32_t currPregap)
{
	uint32_t skip = 0;

	// frames between index 0(prestart) and 1(curr.start) must be skipped
	if (prestart >= 0) {
		if (prestart > static_cast<int>(curr.start)) {
			LOG_MSG("CDROM: AddTrack => prestart %d cannot be > curr.start %u",
			        prestart, curr.start);
			return false;
		}
		skip = static_cast<uint32_t>(static_cast<int>(curr.start) - prestart);
	}

	// Add the first track, if our vector is empty
	if (tracks.empty()) {
		assertm(curr.number == 1, "The first track must be labelled number 1 [BUG!]");
		curr.skip = skip * curr.sector_size;
		curr.start += currPregap;
		totalPregap = currPregap;
		tracks.push_back(curr);
		return true;
	}

	// Guard against undefined behavior in subsequent tracks.back() call
	assert(!tracks.empty());
	Track &prev = tracks.back();

	// current track consumes data from the same file as the previous
	if (prev.file == curr.file) {
		curr.start += shift;
		if (!prev.length) {
			prev.length = curr.start + totalPregap - prev.start - skip;
		}
		curr.skip += prev.skip + prev.length * prev.sector_size + skip * curr.sector_size;
		totalPregap += currPregap;
		curr.start += totalPregap;
	// current track uses a different file as the previous track
	} else {
		const uint32_t tmp = static_cast<uint32_t>
		                     (prev.file->getLength()) - prev.skip;
		prev.length = tmp / prev.sector_size;
		if (tmp % prev.sector_size != 0)
			prev.length++; // padding

		curr.start += prev.start + prev.length + currPregap;
		curr.skip = skip * curr.sector_size;
		shift += prev.start + prev.length;
		totalPregap = currPregap;
	}
	// error checks
	if (curr.number <= 1
	    || prev.number + 1 != curr.number
	    || curr.start < prev.start + prev.length) {
		LOG_MSG("AddTrack: failed consistency checks\n"
		"\tcurr.number (%d) <= 1\n"
		"\tprev.number (%d) + 1 != curr.number (%d)\n"
		"\tcurr.start (%d) < prev.start (%d) + prev.length (%d)\n",
		curr.number, prev.number, curr.number,
		curr.start, prev.start, prev.length);
		return false;
	}

	tracks.push_back(curr);
	return true;
}

bool CDROM_Interface_Image::HasDataTrack() const
{
	//Data track has attribute 0x40
	for (const auto &track : tracks) {
		if (track.attr == 0x40) {
			return true;
		}
	}
	return false;
}


bool CDROM_Interface_Image::GetRealFileName(std::string &filename, std::string &pathname)
{
	// check if file exists
	if (local_drive_path_exists(filename.c_str())) {
		return true;
	}

	// Check if file with path relative to cue file exists.
	// Consider the possibility that the filename has a windows directory
	// seperator or case-insensitive path (inside the CUE file) which is common
	// for some commercial rereleases of DOS games using DOSBox.
	const std::string cue_file_entry = (pathname + CROSS_FILESPLIT + filename);
	const std::string tmpstr = to_native_path(cue_file_entry);

	if (local_drive_path_exists(tmpstr.c_str())) {
		filename = tmpstr;
		return true;
	}

	// finally check if file is in a dosbox local drive
	char fullname[CROSS_LEN];
	char tmp[CROSS_LEN];
	safe_strcpy(tmp, filename.c_str());
	uint8_t drive;
	if (!DOS_MakeName(tmp, fullname, &drive)) {
		return false;
	}

	const auto ldp = std::dynamic_pointer_cast<localDrive>(Drives.at(drive));
	if (ldp) {
		const std::string host_filename = ldp->MapDosToHostFilename(fullname);
		if (local_drive_path_exists(host_filename.c_str())) {
			filename = host_filename;
			return true;
		}
	}

	return false;
}

bool CDROM_Interface_Image::GetCueKeyword(std::string& keyword, std::istream& in)
{
	in >> keyword;
	for (Bitu i = 0; i < keyword.size(); i++) {
		keyword[i] = static_cast<char>(toupper(keyword[i]));
	}
	return true;
}

bool CDROM_Interface_Image::GetCueFrame(uint32_t& frames, std::istream& in)
{
	std::string msf;
	in >> msf;
	TMSF tmp = {0, 0, 0};
	bool success = sscanf(msf.c_str(), "%hhu:%hhu:%hhu", &tmp.min, &tmp.sec, &tmp.fr) == 3;
	frames = msf_to_frames(tmp);
	return success;
}

bool CDROM_Interface_Image::GetCueString(std::string& str, std::istream& in)
{
	auto pos = static_cast<int>(in.tellg());
	in >> str;
	if (str[0] == '\"') {
		if (str[str.size() - 1] == '\"') {
			str = str.substr(1, str.size() - 2);
		} else {
			in.seekg(pos, std::ios::beg);
			char buffer[MAX_FILENAME_LENGTH];
			in.getline(buffer, MAX_FILENAME_LENGTH, '\"');	// skip
			in.getline(buffer, MAX_FILENAME_LENGTH, '\"');
			str = buffer;
		}
	}
	return true;
}

void CDROM_Image_Init()
{
	Sound_Init();
}

void CDROM_Image_Destroy() {
	Sound_Quit();
}
