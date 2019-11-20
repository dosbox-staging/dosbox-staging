/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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

// #define DEBUG 1

#include "cdrom.h"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <vector>
#include <sys/stat.h>

#if !defined(WIN32)
#include <libgen.h>
#else
#include <cstring>
#endif

#include "drives.h"
#include "support.h"
#include "setup.h"

using namespace std;

// String maximums, local to this file
#define MAX_LINE_LENGTH 512
#define MAX_FILENAME_LENGTH 256

// STL type shorteners, local to this file
using track_iter       = vector<CDROM_Interface_Image::Track>::iterator;
using track_const_iter = vector<CDROM_Interface_Image::Track>::const_iterator;
using tracks_size_t    = vector<CDROM_Interface_Image::Track>::size_type;

CDROM_Interface_Image::BinaryFile::BinaryFile(const char *filename, bool &error)
                                  :TrackFile(BYTES_PER_RAW_REDBOOK_FRAME),
                                   file(nullptr)
{
	file = new ifstream(filename, ios::in | ios::binary);
	error = (file == nullptr) || (file->fail());
}

CDROM_Interface_Image::BinaryFile::~BinaryFile()
{
	// Guard: only cleanup if needed
	if (file == nullptr) return;

	delete file;
	file = nullptr;
}

bool CDROM_Interface_Image::BinaryFile::read(Bit8u *buffer, int seek, int count)
{
	// Guard: only proceed with a valid file
	if (file == nullptr) return false;

	file->seekg(seek, ios::beg);
	file->read((char*)buffer, count);
	return !file->fail();
}

int CDROM_Interface_Image::BinaryFile::getLength()
{
	// Guard: only proceed with a valid file
	if (file == nullptr) return -1;

	std::streampos original_pos = file->tellg();
	file->seekg(0, ios::end);
	const int length = static_cast<int>(file->tellg());
	file->seekg(original_pos, ios::end);
	return length;
}

Bit16u CDROM_Interface_Image::BinaryFile::getEndian()
{
	// Image files are always little endian
	return AUDIO_S16LSB;
}


bool CDROM_Interface_Image::BinaryFile::seek(Bit32u offset)
{
	// Guard: only proceed with a valid file
	if (file == nullptr) return false;

	file->seekg(offset, ios::beg);
	return !file->fail();
}

Bit32u CDROM_Interface_Image::BinaryFile::decode(Bit16s *buffer, Bit32u desired_track_frames)
{
	// Guard: only proceed with a valid file
	if (file == nullptr) return 0;

	file->read((char*)buffer, desired_track_frames * BYTES_PER_REDBOOK_PCM_FRAME);
	return static_cast<Bit32u>(ceil(
		static_cast<float>(file->gcount()) / BYTES_PER_REDBOOK_PCM_FRAME));
}

CDROM_Interface_Image::AudioFile::AudioFile(const char *filename, bool &error)
                                 :TrackFile(4096),
                                  sample(nullptr)
{
	// Use the audio file's actual sample rate and number of channels as opposed to overriding
	Sound_AudioInfo desired = {AUDIO_S16, 0, 0};
	sample = Sound_NewSampleFromFile(filename, &desired);
	if (sample) {
		error = false;
		std::string filename_only(filename);
		filename_only = filename_only.substr(filename_only.find_last_of("\\/") + 1);
		LOG_MSG("CDROM: Loaded %s [%d Hz, %d-channel, %2.1f minutes]",
		        filename_only.c_str(),
		        getRate(),
		        getChannels(),
		        getLength()/static_cast<float>(REDBOOK_PCM_BYTES_PER_MS * 1000 * 60));
	} else {
		error = true;
	}
}

CDROM_Interface_Image::AudioFile::~AudioFile()
{
	// Guard to prevent double-free or nullptr free
	if (sample != nullptr) return;

	Sound_FreeSample(sample);
	sample = nullptr;
}

bool CDROM_Interface_Image::AudioFile::seek(Bit32u offset)
{
#ifdef BENCHMARK
#include <ctime>
	// This benchmarking block requires C++11 to create a trivial ANSI sub-second timer
	// that's portable across Windows, Linux, and macOS. Otherwise, to do so using lesser
	// standards requires a combination of very lengthly solutions for each operating system:
	// https://stackoverflow.com/questions/361363/how-to-measure-time-in-milliseconds-using-ansi-c
	// Leave this in place (but #ifdef'ed away) for development and regression testing purposes.
	const auto begin = std::chrono::steady_clock::now();
#endif
	// Convert the byte-offset to a time offset (milliseconds)
	const bool result = Sound_Seek(sample, lround(offset / REDBOOK_PCM_BYTES_PER_MS));

#ifdef BENCHMARK
	const auto end = std::chrono::steady_clock::now();
	LOG_MSG("CDROM: seek to sector %u => took %f ms",
	        offset,
	        chrono::duration <double, milli> (end - begin).count());
#endif
	return result;
}

Bit32u CDROM_Interface_Image::AudioFile::decode(Bit16s *buffer, Bit32u desired_track_frames)
{
	return Sound_Decode_Direct(sample, (void*)buffer, desired_track_frames);
}

Bit16u CDROM_Interface_Image::AudioFile::getEndian()
{
	return sample ? sample->actual.format : AUDIO_S16SYS;
}

Bit32u CDROM_Interface_Image::AudioFile::getRate()
{
	return sample ? sample->actual.rate : 0;
}

Bit8u CDROM_Interface_Image::AudioFile::getChannels()
{
	return sample ? sample->actual.channels : 0;
}

int CDROM_Interface_Image::AudioFile::getLength()
{
	// Sound_GetDuration returns milliseconds so we covert to bytes
	return sample ? static_cast<int>(round(
		Sound_GetDuration(sample) * REDBOOK_PCM_BYTES_PER_MS)) : -1;
}

// initialize static members
int CDROM_Interface_Image::refCount = 0;
CDROM_Interface_Image* CDROM_Interface_Image::images[26] = {};
CDROM_Interface_Image::imagePlayer CDROM_Interface_Image::player = {
	{0},        // buffer[]
	nullptr,    // SDL_Mutex*
	nullptr,    // TrackFile*
	nullptr,    // MixerChannel*
	nullptr,    // CDROM_Interface_Image*
	nullptr,    // addFrames
	0,          // startSector
	0,          // totalRedbookFrames
	0,          // playedTrackFrames
	0,          // totalTrackFrames
	false,      // isPlaying
	false       // isPaused
};

CDROM_Interface_Image::CDROM_Interface_Image(Bit8u _subUnit)
		      :subUnit(_subUnit)
{
	images[subUnit] = this;
	if (refCount == 0) {
		if (player.mutex == nullptr) {
			player.mutex = SDL_CreateMutex();
		}
		if (player.channel == nullptr) {
			// channel is kept dormant except during cdrom playback periods
			player.channel = MIXER_AddChannel(&CDAudioCallBack, 0, "CDAUDIO");
			player.channel->Enable(false);
#ifdef DEBUG
			LOG_MSG("CDROM: Initialized the CDAUDIO mixer channel and mutex");
#endif
		}
	}
	refCount++;
}

CDROM_Interface_Image::~CDROM_Interface_Image()
{
	refCount--;
	if (player.cd == this) {
		player.cd = nullptr;
	}
	ClearTracks();
	if (refCount == 0) {
		StopAudio();
		if (player.channel != nullptr) {
			MIXER_DelChannel(player.channel);
			player.channel = nullptr;
		}
		if (player.mutex != nullptr) {
			SDL_DestroyMutex(player.mutex);
			player.mutex = nullptr;
		}
#ifdef DEBUG
		LOG_MSG("CDROM: Released the CDAUDIO mixer channel and mutex");
#endif
	}
}

void CDROM_Interface_Image::InitNewMedia()
{
}

bool CDROM_Interface_Image::SetDevice(char* path, int forceCD)
{
	(void)forceCD; // unused by part of the API
	if (LoadCueSheet(path) ||
	    LoadIsoFile(path)) {
		return true;
	}

	// print error message on dosbox console
	char buf[MAX_LINE_LENGTH];
	snprintf(buf, MAX_LINE_LENGTH, "Could not load image file: %s\r\n", path);
	Bit16u size = (Bit16u)strlen(buf);
	DOS_WriteFile(STDOUT, (Bit8u*)buf, &size);
	return false;
}

bool CDROM_Interface_Image::GetUPC(unsigned char& attr, char* upc)
{
	attr = 0;
	strcpy(upc, this->mcn.c_str());
#ifdef DEBUG
	LOG_MSG("CDROM: GetUPC => returned %s", upc);
#endif
	return true;
}

bool CDROM_Interface_Image::GetAudioTracks(int& start_track_num, int& lead_out_num, TMSF& lead_out_msf)
{
	// Guard: A valid CD has atleast two tracks: the first plus the lead-out, so bail
	//        out if we have fewer than 2 tracks
	if (tracks.size() < MIN_REDBOOK_TRACKS) {
#ifdef DEBUG
		LOG_MSG("CDROM: GetAudioTracks: game wanted to dump track "
		        "metadata but our CD image has too few tracks: %u",
		        static_cast<unsigned int>(tracks.size()));
#endif
		return false;
	}
	start_track_num = tracks.begin()->number;
	track_const_iter lead_out(prev(tracks.end()));
	lead_out_num = lead_out->number;
	frames_to_msf(lead_out->start + 150,
	              &lead_out_msf.min,
	              &lead_out_msf.sec,
	              &lead_out_msf.fr);
#ifdef DEBUG
	LOG_MSG("CDROM: GetAudioTracks => start track is %2d, lead out track is %2d, "
	        "and lead out MSF is %02d:%02d:%02d",
	        start_track_num,
	        lead_out_num,
	        lead_out_msf.min,
	        lead_out_msf.sec,
	        lead_out_msf.fr);
#endif
	return true;
}

bool CDROM_Interface_Image::GetAudioTrackInfo(int requested_track_num,
                                              TMSF& start_msf,
                                              unsigned char& attr)
{
	if (tracks.size() < MIN_REDBOOK_TRACKS ||
	   (requested_track_num < 1 &&
	   static_cast<tracks_size_t>(requested_track_num) > tracks.size())) {
#ifdef DEBUG
		LOG_MSG("CDROM: GetAudioTrackInfo for track %d => "
		        "outside our valid track numbers: 1 to %u",
	            requested_track_num,
	            static_cast<unsigned int>(tracks.size()));
#endif
		return false;
	}

	const tracks_size_t	requested_track_index(requested_track_num - 1);
	track_const_iter track(tracks.begin() + requested_track_index);
	frames_to_msf(track->start + 150, &start_msf.min, &start_msf.sec, &start_msf.fr);
	attr = track->attr;
#ifdef DEBUG
	LOG_MSG("CDROM: GetAudioTrackInfo for track %d => "
	        "MSF %02d:%02d:%02d, which is sector %d",
	        requested_track_num,
	        start_msf.min,
	        start_msf.sec,
	        start_msf.fr,
	        msf_to_frames(start_msf.min, start_msf.sec, start_msf.fr));
#endif
	return true;
}

bool CDROM_Interface_Image::GetAudioSub(unsigned char& attr, unsigned char& track_num,
                                        unsigned char& index, TMSF& relPos, TMSF& absPos)
{
	// Guard 1: bail if the game requests playback status before actually playing
	if (player.trackFile == nullptr) {
#ifdef DEBUG
		LOG_MSG("CDROM: GetAudioSub => game asked for playback position "
		        "before playing audio (ignoring)");
#endif
		return false;
	}

	// Convert our running tally of played *track* frames to *Redbook* frames.
	// We 'ceil' because if our track-frame tally lands in the middle of a (fractional)
	// Redbook frame, then that Redbook frame would have to be considered played to produce
	// even the smallest amount of track-frames played.  This also accurately represents
	// the very end of a sequence where the last Redbook frame might only contain a couple
	// PCM samples - but the entire last 2352-byte Redbook frame is needed to cover those samples.
	const Bit32u playedRedbookFrames = static_cast<Bit32u>(
	    ceil(static_cast<float>(REDBOOK_FRAMES_PER_SECOND * player.playedTrackFrames)
	    / player.trackFile->getRate()) );

	// Add that to the track's starting sector to determine our absolute current sector
	const Bit32u currentSector = player.startSector + playedRedbookFrames;
	track_const_iter track(GetTrack(currentSector));

	// Guard 2: bail if the track is invalid
	if (track == tracks.end()) {
#ifdef DEBUG
		LOG_MSG("CDROM: GetAudioSub => playback position lands outside of our tracks");
#endif
		return false;
	}

	track_num = track->number;
	attr = track->attr;
	index = 1;
	frames_to_msf(currentSector + 150,
	              &absPos.min,
	              &absPos.sec,
	              &absPos.fr);
	frames_to_msf(currentSector - track->start,
	              &relPos.min,
	              &relPos.sec,
	              &relPos.fr);
#ifdef DEBUG
	LOG_MSG("CDROM: GetAudioSub => playing at %02d:%02d:%02d (on sector %u) "
	        "in track %u at its %02d:%02d:%02d (at its sector %d)",
	        absPos.min,
	        absPos.sec,
	        absPos.fr,
	        currentSector + 150,
	        track->number,
	        relPos.min,
	        relPos.sec,
	        relPos.fr,
	        static_cast<int>(currentSector) - track->start);
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

bool CDROM_Interface_Image::PlayAudioSector(unsigned long start, unsigned long len)
{
	track_const_iter track(GetTrack(start));

	// Guard: sanity check the request beyond what GetTrack already checks
	if (len == 0 ||
	    track == tracks.end() ||
	    track->attr == 0x40 ||
	    track->file == nullptr ||
	    player.channel == nullptr) {
		LOG_MSG("CDROM: PlayAudioSector at start sector %lu for %lu "
		        " number of frames => bad request, skipping",
		        start,
		        len);
		StopAudio();
		return false;
	}

	// Convert the requested absolute start sector to a byte offset relative to the track's start
	// Note: even though 'GetTrack() has determined the requested sector falls within our given
	//       track, it's still possible that the sector is outside of the "physical" bounds of the
	//       file itself - such as in the pre-gap region. Therefore, we clamp the offset within the
	//       bounds of the actual track.

	const int relative_start = start - track->start;
	// If the request falls in the pregap we deduct the difference from the playback duration.
	if (relative_start < 0) {
		len -= relative_start;
	}
	// Seek to the calculated byte offset, bounded to the valid byte offsets
	const int offset = (track->skip +
	                    + clamp(relative_start, 0, track->length - 1)
	                    * track->sectorSize);

	TrackFile* trackFile = track->file;
	// Guard: Bail if our track could not be seeked
	if (!trackFile->seek(offset)) {
		LOG_MSG("CDROM: Track %d failed to seek to byte %u, so cancelling playback",
		        track->number,
		        offset);
		StopAudio();
		return false;
	}

	// Get properties about the current track
	const Bit8u track_channels = trackFile->getChannels();
	const Bit32u track_rate = trackFile->getRate();

	// Guard:
	//   Before we update our player object with new track details, we lock access
	//   to it to prevent the Callback (which runs in a separate thread) from
	//   getting inconsistent or partial values.
	if (SDL_LockMutex(player.mutex) < 0) {
		LOG_MSG("CDROM: PlayAudioSector couldn't lock our player for exclusive access");
		StopAudio();
		return false;
	}

	// Update our player with properties about this playback sequence
	player.cd = this;
	player.trackFile = trackFile;
	player.startSector = start;
	player.totalRedbookFrames = len;
	player.isPlaying = true;
	player.isPaused = false;

	// Assign the mixer function associated with this track's content type
	if (trackFile->getEndian() == AUDIO_S16SYS) {
		player.addFrames = track_channels ==  2  ? &MixerChannel::AddSamples_s16 \
		                                         : &MixerChannel::AddSamples_m16;
	} else {
		player.addFrames = track_channels ==  2  ? &MixerChannel::AddSamples_s16_nonnative \
		                                         : &MixerChannel::AddSamples_m16_nonnative;
	}

	// Convert Redbook frames to Track frames, rounding up to whole integer frames.
	// Round up to whole track frames because the content originated from whole redbook frames,
	// which will require the last fractional frames to be represented by a whole PCM frame.
	player.playedTrackFrames = 0;
	player.totalTrackFrames = static_cast<Bit32u>(ceil(static_cast<float>
	                          (track_rate * player.totalRedbookFrames)
	                          / REDBOOK_FRAMES_PER_SECOND));

#ifdef DEBUG
	if (static_cast<int>(start) < track->start) {
		LOG_MSG("CDROM: PlayAudioSector from sector %lu to %lu => "
		        "starting in the pregap of track %d [pregap %d, start %u, end %u]",
		        start,
		        start + len,
		        track->number,
		        prev(track)->start - prev(track)->length,
		        track->start,
		        track->start + track->length);
	} else {
		LOG_MSG("CDROM: PlayAudioSector from sector %lu to %lu => "
		        "in track %d [start %u, end %u]",
		        start,
		        start + len,
		        track->number,
		        track->start,
		        track->start + track->length);
	}
#endif

	// start the channel!
	player.channel->SetFreq(track_rate);
	player.channel->Enable(true);

	// Guard: release the lock in this data
    if (SDL_UnlockMutex(player.mutex) < 0) {
        LOG_MSG("CDROM: PlayAudioSector couldn't unlock this thread");
		StopAudio();
		return false;
    }
	return true;
}

bool CDROM_Interface_Image::PauseAudio(bool resume)
{
	// Guard: Bail if our mixer channel hasn't been allocated
	if (player.channel == nullptr) {
#ifdef DEBUG
		LOG_MSG("CDROM: PauseAudio => game toggled before playing audio");
#endif
		return false;
	}

	// Only switch states if needed
	if (player.isPaused == resume) {
		player.channel->Enable(resume);
		player.isPaused = !resume;
#ifdef DEBUG
		LOG_MSG("CDROM: PauseAudio => audio is now %s",
		        resume ? "unpaused" : "paused");
#endif
	}
	return true;
}

bool CDROM_Interface_Image::StopAudio(void)
{
	// Guard: Bail if our mixer channel hasn't been allocated
	if (player.channel == nullptr) {
#ifdef DEBUG
		LOG_MSG("CDROM: StopAudio => game tried stopping the CD before playing audio");
#endif
		return false;
	}

	// Only switch states if needed
	if (player.isPlaying) {
		player.channel->Enable(false);
		player.isPlaying = false;
		player.isPaused = false;
#ifdef DEBUG
	LOG_MSG("CDROM: StopAudio => stopped playback and halted the mixer");
#endif
	}
	return true;
}

void CDROM_Interface_Image::ChannelControl(TCtrl ctrl)
{
	// Guard: Bail if our mixer channel hasn't been allocated
	if (player.channel == nullptr) {
#ifdef DEBUG
		LOG_MSG("CDROM: ChannelControl => game tried applying channel controls "
		        "before playing audio");
#endif
		return;
	}

	// Adjust the volume of our mixer channel as defined by the application
	player.channel->SetScale(static_cast<float>(ctrl.vol[0]/255.0),  // left vol
	                         static_cast<float>(ctrl.vol[1]/255.0)); // right vol

	// Map the audio channels in our mixer channel as defined by the application
	player.channel->MapChannels(ctrl.out[0],  // left map
	                            ctrl.out[1]); // right map

#ifdef DEBUG
	LOG_MSG("CDROM: ChannelControl => volumes %d/255 and %d/255, "
	        "and left-right map %d, %d",
	        ctrl.vol[0],
	        ctrl.vol[1],
	        ctrl.out[0],
	        ctrl.out[1]);
#endif
}

bool CDROM_Interface_Image::ReadSectors(PhysPt buffer, bool raw, unsigned long sector, unsigned long num)
{
	int sectorSize = (raw ? BYTES_PER_RAW_REDBOOK_FRAME : BYTES_PER_COOKED_REDBOOK_FRAME);
	Bitu buflen = num * sectorSize;
	Bit8u* buf = new Bit8u[buflen];

	bool success = true; //Gobliiins reads 0 sectors
	for(unsigned long i = 0; i < num; i++) {
		success = ReadSector(&buf[i * sectorSize], raw, sector + i);
		if (!success) {
			break;
		}
	}
	MEM_BlockWrite(buffer, buf, buflen);
	delete[] buf;
	return success;
}

bool CDROM_Interface_Image::LoadUnloadMedia(bool unload)
{
	(void)unload; // unused by part of the API
	return true;
}

track_iter CDROM_Interface_Image::GetTrack(const int sector)
{
	// Guard if we have no tracks or the sector is out of bounds
	if (sector < 0 ||
	    sector > MAX_REDBOOK_SECTOR ||
	    tracks.size() < MIN_REDBOOK_TRACKS ||
	    sector >= tracks.back().start + tracks.back().length) {
		LOG_MSG("CDROM: GetTrack at sector %d => "
		        "is outside the bounds of our CD having %u tracks",
		        sector,
		        static_cast<unsigned int>(tracks.size()));
		return tracks.end();
	}

	// Walk the tracks checking if the desired sector falls
	// inside of a given track's range, which starts at the
	// end of the prior track and goes to the current track's
	// (start + length).
	track_iter track(tracks.begin());
	int lower_bound = track->start;
	while(track != tracks.end()) {
		const int upper_bound = track->start + track->length;
		if (lower_bound <= sector && sector < upper_bound) {
			break;
		}
		track++;
		lower_bound = upper_bound;
	} // If we made it here without breaking, then the track
	  // wasn't found and the iterator is now the end() item.

#ifdef DEBUG
	// some games excessively query for track 1 (which isn't
	// audio anyway, so avoid flooding the console with these
	// calls
	if (track != tracks.begin() && track != tracks.end()) {
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
	} else if (track == tracks.end()) {
		LOG_MSG("CDROM: GetTrack at sector %d => fell outside "
		        "the bounds of our %u tracks",
		        sector,
		        static_cast<unsigned int>(tracks.size()));
	}
#endif
	return track;
}

bool CDROM_Interface_Image::ReadSector(Bit8u *buffer, bool raw, unsigned long sector)
{
	track_const_iter track = GetTrack(sector);

	// Guard: Bail if the requested sector fell outside our tracks
	if (track == tracks.end() || track->file == nullptr) {
#ifdef DEBUG
		LOG_MSG("CDROM: ReadSector at %lu => resulted "
		        "in an invalid track or track->file",
		        sector);
#endif
		return false;
	}
	int seek = track->skip + (sector - track->start) * track->sectorSize;
	int length = (raw ? BYTES_PER_RAW_REDBOOK_FRAME : BYTES_PER_COOKED_REDBOOK_FRAME);
	if (track->sectorSize != BYTES_PER_RAW_REDBOOK_FRAME && raw) {
		return false;
	}
	if (track->sectorSize == BYTES_PER_RAW_REDBOOK_FRAME && !track->mode2 && !raw) seek += 16;
	if (track->mode2 && !raw) seek += 24;

#if 0 // Excessively verbose.. only enable if needed
#ifdef DEBUG
	LOG_MSG("CDROM: ReadSector track %2d, desired raw %s, sector %ld, length=%d",
	        track->number,
	        raw ? "true":"false",
	        sector,
	        length);
#endif
#endif
	return track->file->read(buffer, seek, length);
}


void CDROM_Interface_Image::CDAudioCallBack(Bitu desired_track_frames)
{
	// Guards: Bail if the request or our player is invalid
	if (desired_track_frames == 0 || player.trackFile == nullptr || player.cd == nullptr) {
#ifdef DEBUG
		LOG_MSG("CDROM: CDAudioCallBack for %u frames => empty request, "
		        "file pointer, or CD pointer (skipping for now)",
		        static_cast<unsigned int>(desired_track_frames));
#endif
		return;
	}

	// Ensure we have exclusive access to update our player members
	if (SDL_LockMutex(player.mutex) < 0) {
		LOG_MSG("CDROM: CDAudioCallBack couldn't lock this thread");
		return;
	}

	const Bit32u decoded_track_frames =
	             player.trackFile->decode(player.buffer, desired_track_frames);
	player.playedTrackFrames += decoded_track_frames;

	// uses either the stereo or mono and native or
	// nonnative AddSamples call assigned during construction
	(player.channel->*player.addFrames)(decoded_track_frames, player.buffer);

	if (player.playedTrackFrames >= player.totalTrackFrames) {
		player.cd->StopAudio();
	} else if (decoded_track_frames == 0) {
		// Our track has run dry but we still have more music left to play!
		const double percent_played = static_cast<double>(
		                              player.playedTrackFrames)
		                              / player.totalTrackFrames;
		const Bit32u played_redbook_frames = static_cast<Bit32u>(ceil(
		                                     percent_played
		                                     * player.totalRedbookFrames));
		const Bit32u new_redbook_start_frame = player.startSector
		                                       + played_redbook_frames;
		const Bit32u remaining_redbook_frames = player.totalRedbookFrames
		                                        - played_redbook_frames;
		if (SDL_UnlockMutex(player.mutex) < 0) {
			LOG_MSG("CDROM: CDAudioCallBack couldn't unlock to move to next track");
			return;
		}
		player.cd->PlayAudioSector(new_redbook_start_frame, remaining_redbook_frames);
		return;
	}
	if (SDL_UnlockMutex(player.mutex) < 0) {
		LOG_MSG("CDROM: CDAudioCallBack couldn't unlock our player before returning");
	}
}

bool CDROM_Interface_Image::LoadIsoFile(char* filename)
{
	tracks.clear();

	// data track
	Track track = {nullptr, // TrackFile*
	               0,       // number
	               0,       // attr
	               0,       // start
	               0,       // length
	               0,       // skip
	               0,       // sectorSize
	               false};  // mode2

	bool error;
	track.file = new BinaryFile(filename, error);
	if (error) {
		if (track.file != nullptr) {
			delete track.file;
			track.file = nullptr;
		}
		return false;
	}
	track.number = 1;
	track.attr = 0x40;//data

	// try to detect iso type
	if (CanReadPVD(track.file, BYTES_PER_COOKED_REDBOOK_FRAME, false)) {
		track.sectorSize = BYTES_PER_COOKED_REDBOOK_FRAME;
		track.mode2 = false;
	} else if (CanReadPVD(track.file, BYTES_PER_RAW_REDBOOK_FRAME, false)) {
		track.sectorSize = BYTES_PER_RAW_REDBOOK_FRAME;
		track.mode2 = false;
	} else if (CanReadPVD(track.file, 2336, true)) {
		track.sectorSize = 2336;
		track.mode2 = true;
	} else if (CanReadPVD(track.file, BYTES_PER_RAW_REDBOOK_FRAME, true)) {
		track.sectorSize = BYTES_PER_RAW_REDBOOK_FRAME;
		track.mode2 = true;
	} else {
		if (track.file != nullptr) {
			delete track.file;
			track.file = nullptr;
		}
		return false;
	}
	track.length = track.file->getLength() / track.sectorSize;
#ifdef DEBUG
	LOG_MSG("LoadIsoFile parsed %s => track 1, 0x40, sectorSize %d, mode2 is %s",
	        filename,
	        track.sectorSize,
	        track.mode2 ? "true":"false");
#endif

	tracks.push_back(track);

	// leadout track
	track.number = 2;
	track.attr = 0;
	track.start = track.length;
	track.length = 0;
	track.file = nullptr;
	tracks.push_back(track);
	return true;
}

bool CDROM_Interface_Image::CanReadPVD(TrackFile *file, int sectorSize, bool mode2)
{
	// Guard: Bail if our file pointer is empty
	if (file == nullptr) return false;

	// Initialize our array in the event file->read() doesn't fully write it
	Bit8u pvd[BYTES_PER_COOKED_REDBOOK_FRAME] = {0};

	int seek = 16 * sectorSize;	// first vd is located at sector 16
	if (sectorSize == BYTES_PER_RAW_REDBOOK_FRAME && !mode2) seek += 16;
	if (mode2) seek += 24;
	file->read(pvd, seek, BYTES_PER_COOKED_REDBOOK_FRAME);
	// pvd[0] = descriptor type, pvd[1..5] = standard identifier,
	// pvd[6] = iso version (+8 for High Sierra)
	return ((pvd[0] == 1 && !strncmp((char*)(&pvd[1]), "CD001", 5) && pvd[6]  == 1) ||
	        (pvd[8] == 1 && !strncmp((char*)(&pvd[9]), "CDROM", 5) && pvd[14] == 1));
}

#if defined(WIN32)
static string dirname(char * file) {
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

bool CDROM_Interface_Image::LoadCueSheet(char *cuefile)
{
	Track track = {nullptr, // TrackFile*
	               0,       // number
	               0,       // attr
	               0,       // start
	               0,       // length
	               0,       // skip
	               0,       // sectorSize
	               false};  // mode2
	tracks.clear();
	int shift = 0;
	int currPregap = 0;
	int totalPregap = 0;
	int prestart = -1;
	bool success;
	bool canAddTrack = false;
	char tmp[MAX_FILENAME_LENGTH];  // dirname can change its argument
	safe_strncpy(tmp, cuefile, MAX_FILENAME_LENGTH);
	string pathname(dirname(tmp));
	ifstream in;
	in.open(cuefile, ios::in);
	if (in.fail()) {
		return false;
	}

	while(!in.eof()) {
		// get next line
		char buf[MAX_LINE_LENGTH];
		in.getline(buf, MAX_LINE_LENGTH);
		if (in.fail() && !in.eof()) {
			return false;  // probably a binary file
		}
		istringstream line(buf);

		string command;
		GetCueKeyword(command, line);

		if (command == "TRACK") {
			if (canAddTrack) success = AddTrack(track, shift, prestart, totalPregap, currPregap);
			else success = true;

			track.start = 0;
			track.skip = 0;
			currPregap = 0;
			prestart = -1;

			line >> track.number;
			string type;
			GetCueKeyword(type, line);

			if (type == "AUDIO") {
				track.sectorSize = BYTES_PER_RAW_REDBOOK_FRAME;
				track.attr = 0;
				track.mode2 = false;
			} else if (type == "MODE1/2048") {
				track.sectorSize = BYTES_PER_COOKED_REDBOOK_FRAME;
				track.attr = 0x40;
				track.mode2 = false;
			} else if (type == "MODE1/2352") {
				track.sectorSize = BYTES_PER_RAW_REDBOOK_FRAME;
				track.attr = 0x40;
				track.mode2 = false;
			} else if (type == "MODE2/2336") {
				track.sectorSize = 2336;
				track.attr = 0x40;
				track.mode2 = true;
			} else if (type == "MODE2/2352") {
				track.sectorSize = BYTES_PER_RAW_REDBOOK_FRAME;
				track.attr = 0x40;
				track.mode2 = true;
			} else success = false;

			canAddTrack = true;
		}
		else if (command == "INDEX") {
			int index;
			line >> index;
			int frame;
			success = GetCueFrame(frame, line);

			if (index == 1) track.start = frame;
			else if (index == 0) prestart = frame;
			// ignore other indices
		}
		else if (command == "FILE") {
			if (canAddTrack) success = AddTrack(track, shift, prestart, totalPregap, currPregap);
			else success = true;
			canAddTrack = false;

			string filename;
			GetCueString(filename, line);
			GetRealFileName(filename, pathname);
			string type;
			GetCueKeyword(type, line);

			track.file = nullptr;
			bool error = true;
			if (type == "BINARY") {
				track.file = new BinaryFile(filename.c_str(), error);
			}
			else {
				track.file = new AudioFile(filename.c_str(), error);
				// SDL_Sound first tries using a decoder having a matching registered extension
				// as the filename, and then falls back to trying each decoder before finally
				// giving up.
			}
			if (error) {
				if (track.file != nullptr) {
					delete track.file;
					track.file = nullptr;
				}
				success = false;
			}
		}
		else if (command == "PREGAP") success = GetCueFrame(currPregap, line);
		else if (command == "CATALOG") success = GetCueString(mcn, line);
		// ignored commands
		else if (command == "CDTEXTFILE" || command == "FLAGS"   || command == "ISRC" ||
		         command == "PERFORMER"  || command == "POSTGAP" || command == "REM" ||
		         command == "SONGWRITER" || command == "TITLE"   || command == "") {
			success = true;
		}
		// failure
		else {
			if (track.file != nullptr) {
				delete track.file;
				track.file = nullptr;
			}
			success = false;
		}
		if (!success) {
			if (track.file != nullptr) {
				delete track.file;
				track.file = nullptr;
			}
			return false;
		}
	}
	// add last track
	if (!AddTrack(track, shift, prestart, totalPregap, currPregap)) {
		return false;
	}

	// add leadout track
	track.number++;
	track.attr = 0;//sync with load iso
	track.start = 0;
	track.length = 0;
	track.file = nullptr;
	if (!AddTrack(track, shift, -1, totalPregap, 0)) {
		return false;
	}
	return true;
}



bool CDROM_Interface_Image::AddTrack(Track &curr, int &shift, const int prestart,
	                                 int &totalPregap, const int currPregap)
{
	int skip = 0;

	// frames between index 0(prestart) and 1(curr.start) must be skipped
	if (prestart >= 0) {
		if (prestart > curr.start) {
			return false;
		}
		skip = curr.start - prestart;
	}

	// first track (track number must be 1)
	if (tracks.empty()) {
		if (curr.number != 1) {
			return false;
		}
		curr.skip = skip * curr.sectorSize;
		curr.start += currPregap;
		totalPregap = currPregap;
		tracks.push_back(curr);
		return true;
	}

	Track &prev = *(tracks.end() - 1);

	// current track consumes data from the same file as the previous
	if (prev.file == curr.file) {
		curr.start += shift;
		if (!prev.length) {
			prev.length = curr.start + totalPregap - prev.start - skip;
		}
		curr.skip += prev.skip + prev.length * prev.sectorSize + skip * curr.sectorSize;
		totalPregap += currPregap;
		curr.start += totalPregap;
	// current track uses a different file as the previous track
	} else {
		const int tmp = prev.file->getLength() - prev.skip;
		prev.length = tmp / prev.sectorSize;
		if (tmp % prev.sectorSize != 0) prev.length++; // padding

		curr.start += prev.start + prev.length + currPregap;
		curr.skip = skip * curr.sectorSize;
		shift += prev.start + prev.length;
		totalPregap = currPregap;
	}
	// error checks
	if (curr.number <= 1                      ||
	    prev.number + 1 != curr.number        ||
	    curr.start < prev.start + prev.length ||
	    curr.length < 0) {
		return false;
	}

	tracks.push_back(curr);
	return true;
}

bool CDROM_Interface_Image::HasDataTrack(void)
{
	//Data track has attribute 0x40
	for(const auto &track : tracks) {
		if (track.attr == 0x40) {
			return true;
		}
	}
	return false;
}


bool CDROM_Interface_Image::GetRealFileName(string &filename, string &pathname)
{
	// check if file exists
	struct stat test;
	if (stat(filename.c_str(), &test) == 0) {
		return true;
	}

	// check if file with path relative to cue file exists
	string tmpstr(pathname + "/" + filename);
	if (stat(tmpstr.c_str(), &test) == 0) {
		filename = tmpstr;
		return true;
	}
	// finally check if file is in a dosbox local drive
	char fullname[CROSS_LEN];
	char tmp[CROSS_LEN];
	safe_strncpy(tmp, filename.c_str(), CROSS_LEN);
	Bit8u drive;
	if (!DOS_MakeName(tmp, fullname, &drive)) {
		return false;
	}

	localDrive *ldp = dynamic_cast<localDrive*>(Drives[drive]);
	if (ldp) {
		ldp->GetSystemFilename(tmp, fullname);
		if (stat(tmp, &test) == 0) {
			filename = tmp;
			return true;
		}
	}
#if defined (WIN32) || defined(OS2)
	//Nothing
#else
	//Consider the possibility that the filename has a windows directory seperator (inside the CUE file)
	//which is common for some commercial rereleases of DOS games using DOSBox

	string copy = filename;
	size_t l = copy.size();
	for (size_t i = 0; i < l;i++) {
		if (copy[i] == '\\') copy[i] = '/';
	}

	if (stat(copy.c_str(), &test) == 0) {
		filename = copy;
		return true;
	}

	tmpstr = pathname + "/" + copy;
	if (stat(tmpstr.c_str(), &test) == 0) {
		filename = tmpstr;
		return true;
	}

#endif
	return false;
}

bool CDROM_Interface_Image::GetCueKeyword(string &keyword, istream &in)
{
	in >> keyword;
	for(Bitu i = 0; i < keyword.size(); i++) keyword[i] = toupper(keyword[i]);

	return true;
}

bool CDROM_Interface_Image::GetCueFrame(int &frames, istream &in)
{
	string msf;
	in >> msf;
	int min, sec, fr;
	bool success = sscanf(msf.c_str(), "%d:%d:%d", &min, &sec, &fr) == 3;
	frames = msf_to_frames(min, sec, fr);
	return success;
}

bool CDROM_Interface_Image::GetCueString(string &str, istream &in)
{
	int pos = (int)in.tellg();
	in >> str;
	if (str[0] == '\"') {
		if (str[str.size() - 1] == '\"') {
			str.assign(str, 1, str.size() - 2);
		} else {
			in.seekg(pos, ios::beg);
			char buffer[MAX_FILENAME_LENGTH];
			in.getline(buffer, MAX_FILENAME_LENGTH, '\"');	// skip
			in.getline(buffer, MAX_FILENAME_LENGTH, '\"');
			str = buffer;
		}
	}
	return true;
}

void CDROM_Interface_Image::ClearTracks()
{
	for (auto &track : tracks) {
		if (track.file) {
			delete track.file;
			track.file = nullptr;
		}
	}
	tracks.clear();
}

void CDROM_Image_Destroy(Section*) {
	Sound_Quit();
}

void CDROM_Image_Init(Section* sec) {
	if (sec != nullptr) {
		sec->AddDestroyFunction(CDROM_Image_Destroy, false);
	}
	Sound_Init();
}
