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

#include "cdrom.h"

// #define DEBUG 1
#ifdef DEBUG
#include <time.h> // time_t, tm, time(), and localtime()
#endif

#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>
#include <sys/stat.h>

#if !defined(WIN32)
#include <libgen.h>
#else
#include <string.h>
#endif

#include "drives.h"
#include "support.h"
#include "setup.h"

using namespace std;

#define MAX_LINE_LENGTH 512
#define MAX_FILENAME_LENGTH 256

char* get_time() {
#ifdef DEBUG
	static char time_str[] = "00:00:00 ";
	static time_t rawtime;
	time(&rawtime);
	const struct tm* ptime = localtime(&rawtime);
	snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d ", ptime->tm_hour, ptime->tm_min, ptime->tm_sec);
#else
	static char time_str[] = "";
#endif
	return time_str;
}


CDROM_Interface_Image::BinaryFile::BinaryFile(const char *filename, bool &error)
	:TrackFile(RAW_SECTOR_SIZE)
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
	bool rcode(false);
	if (file) {
		file->seekg(seek, ios::beg);
		file->read((char*)buffer, count);
		rcode = !file->fail();
	}
	return rcode;
}

int CDROM_Interface_Image::BinaryFile::getLength()
{
	int length = -1; // The maximum value held by a signed int is 2,147,483,647,
	                 // which is larger than the maximum size of a CDROM, which
	                 // is known to be 99 minutes or roughly 870 MB in size.
	if (file) {
		std::streampos original_pos = file->tellg();
		file->seekg(0, ios::end);
		length = static_cast<int>(file->tellg());
		file->seekg(original_pos, ios::end);
	}
	return length;
}

Bit16u CDROM_Interface_Image::BinaryFile::getEndian()
{
	// Image files are always little endian
	return AUDIO_S16LSB;
}


bool CDROM_Interface_Image::BinaryFile::seek(Bit32u offset)
{
	bool rcode(false);
	if (file) {
		file->seekg(offset, ios::beg);
		rcode = !file->fail();
	}
	return rcode;
}

Bit32u CDROM_Interface_Image::BinaryFile::decode(Bit16s *buffer, Bit32u desired_track_frames)
{
	Bit32u rcode(0);
	if (file) {
		file->read((char*)buffer, desired_track_frames * BYTES_PER_TRACK_FRAME);
		rcode = (Bit32u) file->gcount() / BYTES_PER_TRACK_FRAME;
	}
	return rcode;
}

CDROM_Interface_Image::AudioFile::AudioFile(const char *filename, bool &error)
	:TrackFile(4096)
{
	// Use the audio file's actual sample rate and number of channels as opposed to overriding
	Sound_AudioInfo desired = {AUDIO_S16, 0, 0};
	sample = Sound_NewSampleFromFile(filename, &desired);
	if (sample) {
		error = false;
		std::string filename_only(filename);
		filename_only = filename_only.substr(filename_only.find_last_of("\\/") + 1);
		LOG_MSG("%sCDROM: Loaded %s [%d Hz %d-channel]",
		        get_time(),
		        filename_only.c_str(),
		        this->getRate(),
		        this->getChannels());
	} else {
		error = true;
	}
}

CDROM_Interface_Image::AudioFile::~AudioFile()
{
	Sound_FreeSample(sample);
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
	const bool result = Sound_Seek(sample, lround(offset/176.4f));

#ifdef BENCHMARK
	const auto end = std::chrono::steady_clock::now();
	LOG_MSG("%sCDROM: seek(%u) took %f ms",
	        get_time(),
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
	int length(-1);

	// GetDuration returns milliseconds ... but getLength needs Red Book bytes.
	const int duration_ms = Sound_GetDuration(sample);
	if (duration_ms > 0) {
		// ... so convert ms to "Red Book bytes" by multiplying with 176.4f,
		// which is 44,100 samples/second * 2-channels * 2 bytes/sample
		// / 1000 milliseconds/second
		length = (int) round(duration_ms * 176.4f);
	}
#ifdef DEBUG
	LOG_MSG("%sCDROM: AudioFile::getLength is %d bytes",
	        get_time(),
	        length);
#endif

	return length;
}

// initialize static members
int CDROM_Interface_Image::refCount = 0;
CDROM_Interface_Image* CDROM_Interface_Image::images[26] = {};
CDROM_Interface_Image::imagePlayer CDROM_Interface_Image::player = {
	{0},        // buffer[]
	nullptr,    // TrackFile*
	nullptr,    // MixerChannel*
	nullptr,    // CDROM_Interface_Image*
	nullptr,    // addFrames
	0,          // startRedbookFrame
	0,          // totalRedbookFrames
	0,          // playedTrackFrames
	0,          // totalTrackFrames
	false,      // isPlaying
	false       // isPaused
};

CDROM_Interface_Image::CDROM_Interface_Image(Bit8u subUnit)
		      :subUnit(subUnit)
{
	images[subUnit] = this;
	if (refCount == 0) {
		if (player.channel == nullptr) {
			// channel is kept dormant except during cdrom playback periods
			player.channel = MIXER_AddChannel(&CDAudioCallBack, 0, "CDAUDIO");
			player.channel->Enable(false);
#ifdef DEBUG
			LOG_MSG("%sCDROM: Initialized the CDAUDIO mixer channel", get_time());
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
		MIXER_DelChannel(player.channel);
		player.channel = nullptr;
#ifdef DEBUG
		LOG_MSG("%sCDROM: Audio channel freed", get_time());
#endif
	}
}

void CDROM_Interface_Image::InitNewMedia()
{
}

bool CDROM_Interface_Image::SetDevice(char* path, int forceCD)
{
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
	LOG_MSG("%sCDROM: GetUPC=%s",
	        get_time(),
	        upc);
#endif

	return true;
}

bool CDROM_Interface_Image::GetAudioTracks(int& stTrack, int& end, TMSF& leadOut)
{
	stTrack = 1;
	end = (int)(tracks.size() - 1);
	frames_to_msf(tracks[tracks.size() - 1].start + 150,
	              &leadOut.min, &leadOut.sec, &leadOut.fr);
#ifdef DEBUG
	LOG_MSG("%sCDROM: GetAudioTracks, stTrack=%d, end=%d, "
	        "leadOut.min=%d, leadOut.sec=%d, leadOut.fr=%d",
	        get_time(),
	        stTrack,
	        end,
	        leadOut.min,
	        leadOut.sec,
	        leadOut.fr);
#endif

	return true;
}

bool CDROM_Interface_Image::GetAudioTrackInfo(int track, TMSF& start, unsigned char& attr)
{
	if (track < 1 || track > (int)tracks.size()) {
		return false;
	}
	frames_to_msf(tracks[track - 1].start + 150,
	              &start.min, &start.sec, &start.fr);
	attr = tracks[track - 1].attr;

#ifdef DEBUG
	LOG_MSG("%sCDROM: GetAudioTrackInfo track=%d MSF %02d:%02d:%02d, attr=%u",
	        get_time(),
	        track,
	        start.min,
	        start.sec,
	        start.fr,
	        attr);
#endif
	return true;
}

bool CDROM_Interface_Image::GetAudioSub(unsigned char& attr, unsigned char& track,
                                        unsigned char& index, TMSF& relPos, TMSF& absPos)
{
	// Guard 1: bail if the game requests playback status before actually playing
	if (player.trackFile == nullptr) {
#ifdef DEBUG
		LOG_MSG("%sCDROM: Game asked for playback position "
		        "before playing audio (ignoring)",
		        get_time());
#endif
		return false;
	}

	// Convert our running tally of track-frames played to Redbook-frames played.
	// We round up because if our track-frame tally lands in the middle of a (fractional)
	// Redbook frame, then that Redbook frame would have to be considered played to produce
	// even the smallest amount of track-frames played.  This also accurately represents
	// the very end of a sequence where the last Redbook frame might only contain a couple
	// PCM samples - but the entire last 2352-byte Redbook frame is needed to cover those samples.
	const Bit32u playedRedbookFrames = static_cast<Bit32u>(
	    ceil(static_cast<float>(REDBOOK_FRAMES_PER_SECOND * player.playedTrackFrames)
	    / player.trackFile->getRate()) );

	// Add that to the track's starting Redbook frame to determine our absolute current Redbook frame
	const Bit32u currentRedbookFrame = player.startRedbookFrame + playedRedbookFrames;
	const int cur_track = GetTrack(currentRedbookFrame);

	// Guard 2: bail if the track is invalid
	if (cur_track < 1 || cur_track > 99) {
#ifdef DEBUG
		LOG_MSG("%sCDROM: playback position lands on track"
		        " %d, which is invalid (ignoring)",
		        get_time(),
				cur_track);
#endif
		return false;
	}

	track = static_cast<unsigned char>(cur_track);
	attr = tracks[track - 1].attr;
	index = 1;
	frames_to_msf(currentRedbookFrame + 150,
	              &absPos.min,
	              &absPos.sec,
	              &absPos.fr);
	frames_to_msf(currentRedbookFrame - tracks[track - 1].start,
	              &relPos.min,
	              &relPos.sec,
	              &relPos.fr);
#ifdef DEBUG
	LOG_MSG("%sCDROM: GetAudioSub attr=%u, track=%u, index=%u",
	        get_time(),
	        attr,
	        track,
	        index);
	LOG_MSG("%sCDROM: GetAudioSub absoute  offset (%d), MSF=%d:%d:%d",
	        get_time(),
	        currentRedbookFrame + 150,
	        absPos.min,
	        absPos.sec,
	        absPos.fr);
	LOG_MSG("%sCDROM: GetAudioSub relative offset (%d), MSF=%d:%d:%d",
	        get_time(),
	        currentRedbookFrame - tracks[track - 1].start + 150,
	        relPos.min,
	        relPos.sec,
	        relPos.fr);
#endif
	return true;
}

bool CDROM_Interface_Image::GetAudioStatus(bool& playing, bool& pause)
{
	playing = player.isPlaying;
	pause = player.isPaused;

#ifdef DEBUG
	LOG_MSG("%sCDROM: GetAudioStatus playing=%d, paused=%d",
	        get_time(),
	        playing,
	        pause);
#endif

	return true;
}

bool CDROM_Interface_Image::GetMediaTrayStatus(bool& mediaPresent, bool& mediaChanged, bool& trayOpen)
{
	mediaPresent = true;
	mediaChanged = false;
	trayOpen = false;

#ifdef DEBUG
	LOG_MSG("%sCDROM: GetMediaTrayStatus present=%d, changed=%d, open=%d",
	        get_time(),
	        mediaPresent,
	        mediaChanged,
	        trayOpen);
#endif

	return true;
}

bool CDROM_Interface_Image::PlayAudioSector(unsigned long start, unsigned long len)
{
	const int track = GetTrack(start) - 1;
	bool rcode(false);

	// Guard 1: The Redbook standard allows up to 99 tracks, which includes the
	//          data track.  Bail if the request lands outside those limits.
	if (track < 0 || track > 99)
		LOG(LOG_MISC, LOG_WARN)("%sCDROM: Game tried to load track %d, which is invalid",
		                        get_time(),
		                        track);

	// Guard 2: Bail if the game attempts to play zero sectors
	else if (len == 0)
		LOG(LOG_MISC, LOG_WARN)("%sCDROM: Game tried to play zero sectors, skipping",
		                        get_time());

	// Guard 3: The maximum storage achieved on a CDROM was ~900MB or
	//          just under 100 minutes.  Bail if the start section is
	//          beyond this limit.
	else if (start > 450000)
		LOG(LOG_MISC, LOG_WARN)("%sCDROM: Game tried to read sector %lu, "
		                        "which is beyond the 100-minute maximum of a CDROM",
		                        get_time(),
		                        start);

	// Guard 4: Don't play audio from a data track (as it would result in garbage/static)
	else if (track >= 0 && tracks[track].attr == 0x40)
		LOG(LOG_MISC,LOG_WARN)("%sCDROM: Game tried to play the data track. Not doing this",
		                       get_time());

	// Guard 5: Bail if our track's file-pointer is empty
	else if (tracks[track].file == nullptr)
		LOG(LOG_MISC,LOG_WARN)("%sCDROM: The file-pointer associated with track %d is empty",
		                       get_time(),
		                       track);

	// Guard 6: Bail if our track channel is null
	else if (player.channel == nullptr)
		LOG(LOG_MISC,LOG_WARN)("%sCDROM: The CD player's mixer channel hasn't been allocated yet",
		                       get_time());

	// Finished without hitting any of the above issues? Great!
	else rcode = true;

	// Guard 7: Check if we failed any of the above..
	if (rcode == false) {
		StopAudio();
		return rcode;
	}

	// Convert the playback start sector to a time offset (milliseconds) relative to the track
	const Bit32u offset = tracks[track].skip + (start - tracks[track].start) * tracks[track].sectorSize;

	// Seek into the calculated time offset
	TrackFile* trackFile = tracks[track].file;
	rcode = trackFile->seek(offset);

	// Guard 8: Bail if our track could not be seeked
	if (rcode == false) {
		LOG(LOG_MISC,LOG_WARN)("%sCDROM: Track %d failed to seek to %u ms, so will not play it",
		                       get_time(),
		                       track,
		                       offset);
		StopAudio();
		return rcode;
	}

	// Get properties about the current track
	const Bit8u track_channels = trackFile->getChannels();
	const Bit32u track_rate = trackFile->getRate();

	// Populate our play with properties about this playback sequence
	player.cd = this;
	player.trackFile = trackFile;
	player.startRedbookFrame = start;
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
	player.totalTrackFrames = static_cast<Bit32u>(
	    ceil(static_cast<float>(track_rate * player.totalRedbookFrames)
	    / REDBOOK_FRAMES_PER_SECOND));
	player.playedTrackFrames = 0;

#ifdef DEBUG
	LOG_MSG("%sCDROM: Playing track %d (%d Hz "
	        "%d-channel) at starting sector %lu (%.1f minute-mark) "
	        "for %u Redbook frames (%.1f seconds)",
	        get_time(),
	        track,
	        track_rate,
	        track_channels,
	        start,
	        static_cast<float>(start) / (REDBOOK_FRAMES_PER_SECOND * 60),
	        player.totalRedbookFrames,
	        static_cast<float>(player.totalRedbookFrames) / REDBOOK_FRAMES_PER_SECOND);
#endif

	// start the channel!
	player.channel->SetFreq(track_rate);
	player.channel->Enable(true);
	return rcode;
}

bool CDROM_Interface_Image::PauseAudio(bool resume)
{
	// Guard: Bail if our mixer channel hasn't been allocated
	if (player.channel == nullptr) {
#ifdef DEBUG
		LOG_MSG("%sCDROM: Game tried toggling pause before playing audio",
		        get_time());
#endif
		return false;
	}

	// Only switch states if needed
	if (player.isPaused == resume) {
		player.channel->Enable(resume);
		player.isPaused = !resume;
	}
#ifdef DEBUG
	LOG_MSG("%sCDROM: PauseAudio, state=%s",
	        get_time(), resume ? "resumed" : "paused");
#endif
	return true;
}

bool CDROM_Interface_Image::StopAudio(void)
{
	// Guard: Bail if our mixer channel hasn't been allocated
	if (player.channel == nullptr) {
#ifdef DEBUG
		LOG_MSG("%sCDROM: Game tried stopping the CD before playing audio",
		        get_time());
#endif
		return false;
	}

	// Only switch states if needed
	if (player.isPlaying) {
		player.channel->Enable(false);
		player.isPlaying = false;
		player.isPaused = false;
	}
#ifdef DEBUG
	LOG_MSG("%sCDROM: StopAudio", get_time());
#endif
	return true;
}

void CDROM_Interface_Image::ChannelControl(TCtrl ctrl)
{
	// Guard: Bail if our mixer channel hasn't been allocated
	if (player.channel == nullptr) {
#ifdef DEBUG
		LOG_MSG("%sCDROM: Game tried applying channel controls "
		        "before playing audio",
		        get_time());
#endif
		return;
	}

	// Adjust the volume of our mixer channel as defined by the application
	player.channel->SetScale(static_cast<float>(ctrl.vol[0]/255.0),  // left vol
	                         static_cast<float>(ctrl.vol[1]/255.0)); // right vol

	// Map the audio channels in our mixer channel as defined by the application
	player.channel->MapChannels(ctrl.out[0],  // left map
	                            ctrl.out[1]); // right map
}

bool CDROM_Interface_Image::ReadSectors(PhysPt buffer, bool raw, unsigned long sector, unsigned long num)
{
	int sectorSize = raw ? RAW_SECTOR_SIZE : COOKED_SECTOR_SIZE;
	Bitu buflen = num * sectorSize;
	Bit8u* buf = new Bit8u[buflen];

	bool success = true; //Gobliiins reads 0 sectors
	for(unsigned long i = 0; i < num; i++) {
		success = ReadSector(&buf[i * sectorSize], raw, sector + i);
		if (!success) break;
	}

	MEM_BlockWrite(buffer, buf, buflen);
	delete[] buf;
	return success;
}

bool CDROM_Interface_Image::LoadUnloadMedia(bool unload)
{
	return true;
}

int CDROM_Interface_Image::GetTrack(int sector)
{
	vector<Track>::iterator i = tracks.begin();
	vector<Track>::iterator end = tracks.end() - 1;

	while(i != end) {
		Track &curr = *i;
		Track &next = *(i + 1);
		if (curr.start <= sector && sector < next.start) {
			return curr.number;
		}
		i++;
	}
	return -1;
}

bool CDROM_Interface_Image::ReadSector(Bit8u *buffer, bool raw, unsigned long sector)
{
	int track = GetTrack(sector) - 1;

	// Guard 1: Bail if our track is invalid
	if (track < 0 || track > 99) {
#ifdef DEBUG
		LOG_MSG("%sCDROM: Game asked for a sector in track %d, which is invalid",
		        get_time(),
		        track);
#endif
		return false;
	}

	// Guard 2: Bail if our track's file point is null
	if	(tracks[track].file == nullptr) {
#ifdef DEBUG
		LOG_MSG("%sCDROM: File pointer associated with track %d is empty",
		        get_time(),
		        track);
#endif
		return false;
	}

	int seek = tracks[track].skip + (sector - tracks[track].start) * tracks[track].sectorSize;
	int length = (raw ? RAW_SECTOR_SIZE : COOKED_SECTOR_SIZE);
	if (tracks[track].sectorSize != RAW_SECTOR_SIZE && raw) {
		return false;
	}
	if (tracks[track].sectorSize == RAW_SECTOR_SIZE && !tracks[track].mode2 && !raw) seek += 16;
	if (tracks[track].mode2 && !raw) seek += 24;

#if 0 // Excessively verbose.. only enable if needed
#ifdef DEBUG
	LOG_MSG("%sCDROM: ReadSector track=%d, desired raw=%s, sector=%ld, length=%d",
	        get_time(),
	        track,
	        raw ? "true":"false",
	        sector,
	        length);
#endif
#endif

	return tracks[track].file->read(buffer, seek, length);
}


void CDROM_Interface_Image::CDAudioCallBack(Bitu desired_track_frames)
{
	// Guard 1: Bail if we're asked to decode no frames
	if (desired_track_frames == 0
	// Guard 2: Bail if our player's trackFile hasn't been assigned
	|| player.trackFile == nullptr
	// Guard 3: Bail if our player's CD object hasn't been assigned
	|| player.cd == nullptr) {
#ifdef DEBUG
		LOG_MSG("%sCDROM: skipping early or unecessary decode event",
		        get_time());
#endif
		return;
	}

	const Bit32u decoded_track_frames =
	             player.trackFile->decode(player.buffer, desired_track_frames);

	// uses either the stereo or mono and native or nonnative AddSamples call assigned during construction
	(player.channel->*player.addFrames)(decoded_track_frames, player.buffer);

	// Stop the audio if our decode stream has run dry or when we've played at least the
	// total number of frames. We consider "played" to mean the running tally so far plus
	// the current requested frames, which takes into account that the number of currently
	// decoded frames might be less than desired if we're at the end of the track.
	// (The mixer will request frames forever until we stop it, so at some point we /will/
	// decode fewer than requested; in this "tail scenario", we push those remaining decoded
	// frames into the mixer and then stop the audio.)
	if (decoded_track_frames == 0
	    || player.playedTrackFrames + desired_track_frames >= player.totalTrackFrames) {
		player.cd->StopAudio();
	}

	// Increment our tally of played frames by those we just decoded (and fed to the mixer).
	// Even if we've hit the end of the track and we've stopped the audio, we still want to
	// increment our tally so subsequent calls to GetAudioSub() return the full number of
	// frames played.
	player.playedTrackFrames += decoded_track_frames;
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
	if (CanReadPVD(track.file, COOKED_SECTOR_SIZE, false)) {
		track.sectorSize = COOKED_SECTOR_SIZE;
		track.mode2 = false;
	} else if (CanReadPVD(track.file, RAW_SECTOR_SIZE, false)) {
		track.sectorSize = RAW_SECTOR_SIZE;
		track.mode2 = false;
	} else if (CanReadPVD(track.file, 2336, true)) {
		track.sectorSize = 2336;
		track.mode2 = true;
	} else if (CanReadPVD(track.file, RAW_SECTOR_SIZE, true)) {
		track.sectorSize = RAW_SECTOR_SIZE;
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
	LOG_MSG("LoadIsoFile: %s, track 1, 0x40, sectorSize=%d, mode2=%s",
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
	Bit8u pvd[COOKED_SECTOR_SIZE] = {0};

	int seek = 16 * sectorSize;	// first vd is located at sector 16
	if (sectorSize == RAW_SECTOR_SIZE && !mode2) seek += 16;
	if (mode2) seek += 24;
	file->read(pvd, seek, COOKED_SECTOR_SIZE);
	// pvd[0] = descriptor type, pvd[1..5] = standard identifier, pvd[6] = iso version (+8 for High Sierra)
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
				track.sectorSize = RAW_SECTOR_SIZE;
				track.attr = 0;
				track.mode2 = false;
			} else if (type == "MODE1/2048") {
				track.sectorSize = COOKED_SECTOR_SIZE;
				track.attr = 0x40;
				track.mode2 = false;
			} else if (type == "MODE1/2352") {
				track.sectorSize = RAW_SECTOR_SIZE;
				track.attr = 0x40;
				track.mode2 = false;
			} else if (type == "MODE2/2336") {
				track.sectorSize = 2336;
				track.attr = 0x40;
				track.mode2 = true;
			} else if (type == "MODE2/2352") {
				track.sectorSize = RAW_SECTOR_SIZE;
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



bool CDROM_Interface_Image::AddTrack(Track &curr, int &shift, int prestart,
	                                 int &totalPregap, int currPregap)
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
		if (!prev.length && prev.file != nullptr) {
			int tmp = prev.file->getLength() - prev.skip;
			prev.length = tmp / prev.sectorSize;
			if (tmp % prev.sectorSize != 0) prev.length++; // padding
		}
		curr.start += prev.start + prev.length + currPregap;
		curr.skip = skip * curr.sectorSize;
		shift += prev.start + prev.length;
		totalPregap = currPregap;
	}

#ifdef DEBUG
	LOG_MSG("%sCDROM: AddTrack cur.start=%d cur.len=%d cur.start+len=%d "
	        "| prev.start=%d prev.len=%d prev.start+len=%d",
	        get_time(),
	        curr.start,
	        curr.length,
	        curr.start + curr.length,
	        prev.start,
	        prev.length,
	        prev.start + prev.length);
#endif

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
	for(track_it it = tracks.begin(); it != tracks.end(); it++) {
		if ((*it).attr == 0x40) {
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
	vector<Track>::iterator i = tracks.begin();
	vector<Track>::iterator end = tracks.end();

	TrackFile* last = nullptr;
	while(i != end) {
		Track &curr = *i;
		if (curr.file != last) {
			delete curr.file;
			last = curr.file;
		}
		i++;
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
