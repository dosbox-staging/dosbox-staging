/*
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

#include "cdrom.h"

// ******************************************************
// SDL CDROM
// ******************************************************

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "SDL.h"

#include "callback.h"
#include "channel_names.h"
#include "dosbox.h"
#include "pic.h"
#include "string_utils.h"

CDROM_Interface_SDL::CDROM_Interface_SDL()
{
	driveID = 0;
	oldLeadOut = 0;
	cd = nullptr;
}

CDROM_Interface_SDL::~CDROM_Interface_SDL()
{
	CDROM_Interface_SDL::StopAudio();
	SDL_CDClose(cd);
	cd = nullptr;
}

bool CDROM_Interface_SDL::SetDevice(const char *path)
{
	assert(path != nullptr);
	std::string path_string = path;
	upcase(path_string);

	int num = SDL_CDNumDrives();
	const char *cdname = nullptr;
	for (int i = 0; i < num; i++) {
		cdname = SDL_CDName(i);
		if (path_string == cdname) {
			cd = SDL_CDOpen(i);
			SDL_CDStatus(cd);
			driveID = i;
			return true;
		};
	};
	return false;
}

bool CDROM_Interface_SDL::GetAudioTracks(uint8_t &stTrack, uint8_t &end, TMSF &leadOut)
{
	if (CD_INDRIVE(SDL_CDStatus(cd))) {
		stTrack = 1;
		end = cd->numtracks;
		leadOut = frames_to_msf(cd->track[cd->numtracks].offset);
	}
	return CD_INDRIVE(SDL_CDStatus(cd));
}

bool CDROM_Interface_SDL::GetAudioTrackInfo(uint8_t track,
                                            TMSF &start,
                                            unsigned char &attr)
{
	if (CD_INDRIVE(SDL_CDStatus(cd))) {
		start = frames_to_msf(cd->track[track - 1].offset);
		attr = cd->track[track - 1].type << 4; // sdl uses 0 for audio
		                                       // and 4 for data. instead
		                                       // of 0x00 and 0x40
	}
	return CD_INDRIVE(SDL_CDStatus(cd));
}

bool CDROM_Interface_SDL::GetAudioSub(unsigned char &attr,
                                      unsigned char &track,
                                      unsigned char &index,
                                      TMSF &relPos,
                                      TMSF &absPos)
{
	if (CD_INDRIVE(SDL_CDStatus(cd))) {
		track = cd->cur_track;
		index = cd->cur_track;
		attr = cd->track[track].type << 4;
		relPos = frames_to_msf(cd->cur_frame);
		absPos = frames_to_msf(cd->cur_frame + cd->track[track].offset);
		LagDriveResponse();
	}
	return CD_INDRIVE(SDL_CDStatus(cd));
}

bool CDROM_Interface_SDL::GetAudioStatus(bool &playing, bool &pause)
{
	if (CD_INDRIVE(SDL_CDStatus(cd))) {
		playing = (cd->status == CD_PLAYING);
		pause = (cd->status == CD_PAUSED);
	}
	return CD_INDRIVE(SDL_CDStatus(cd));
}

bool CDROM_Interface_SDL::GetMediaTrayStatus(bool &mediaPresent,
                                             bool &mediaChanged,
                                             bool &trayOpen)
{
	SDL_CDStatus(cd);
	mediaPresent = (cd->status != CD_TRAYEMPTY) && (cd->status != CD_ERROR);
	mediaChanged = (oldLeadOut != cd->track[cd->numtracks].offset);
	trayOpen = !mediaPresent;
	oldLeadOut = cd->track[cd->numtracks].offset;
	if (mediaChanged) {
		SDL_CDStatus(cd);
	}
	return true;
}

bool CDROM_Interface_SDL::PlayAudioSector(const uint32_t start, uint32_t len)
{
	// Has to be there, otherwise wrong cd status report (dunno why, sdl bug ?)
	SDL_CDClose(cd);
	cd = SDL_CDOpen(driveID);
	return (SDL_CDPlay(cd, start + 150, len) == 0);
}

bool CDROM_Interface_SDL::PauseAudio(bool resume)
{
	if (resume)
		return (SDL_CDResume(cd) == 0);
	else
		return (SDL_CDPause(cd) == 0);
}

bool CDROM_Interface_SDL::StopAudio()
{
	// Has to be there, otherwise wrong cd status report (dunno why, sdl bug ?)
	SDL_CDClose(cd);
	cd = SDL_CDOpen(driveID);

	return (SDL_CDStop(cd) == 0);
}

bool CDROM_Interface_SDL::LoadUnloadMedia([[maybe_unused]] bool unload)
{
	return (SDL_CDEject(cd) == 0);
}

int CDROM_GetMountType(const char *path, const int forceCD)
{
	// 0 - physical CDROM
	// 1 - Iso file
	// 2 - subdirectory
	// 1. Smells like a real cdrom
	// if ((strlen(path)<=3) && (path[2]=='\\') &&
	// (strchr(path,'\\')==strrchr(path,'\\')) &&
	// (GetDriveType(path)==DRIVE_CDROM)) return 0;

	std::string path_string = path;
	upcase(path_string);

	int num = SDL_CDNumDrives();
	// If cd drive is forced then check if its in range and return 0
	if ((forceCD >= 0) && (forceCD < num)) {
		LOG(LOG_ALL, LOG_ERROR)("CDROM: Using drive %d", forceCD);
		return 0;
	}

	const char *cdName;
	// compare names
	for (int i = 0; i < num; i++) {
		cdName = SDL_CDName(i);
		if (path_string == cdName)
			return 0;
	};

	// Detect ISO
	struct stat file_stat;
	if ((stat(path, &file_stat) == 0) && (file_stat.st_mode & S_IFREG))
		return 1;
	return 2;
}

// ******************************************************
// Fake CDROM
// ******************************************************

bool CDROM_Interface_Fake::GetAudioTracks(uint8_t& stTrack, uint8_t& end, TMSF& leadOut) {
	stTrack = end = 1;
	leadOut.min	= 60;
	leadOut.sec = leadOut.fr = 0;

	return true;
}

bool CDROM_Interface_Fake::GetAudioTrackInfo(uint8_t track, TMSF& start, unsigned char& attr) {
	if (track>1) return false;
	start.min = start.fr = 0;
	start.sec = 2;
	attr	  = 0x60; // data / permitted

	return true;
}

bool CDROM_Interface_Fake :: GetAudioSub(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos){
	attr	= 0;
	track	= index = 1;
	relPos.min = relPos.fr = 0; relPos.sec = 2;
	absPos.min = absPos.fr = 0; absPos.sec = 2;

	LagDriveResponse();
	return true;
}

bool CDROM_Interface_Fake :: GetAudioStatus(bool& playing, bool& pause) {
	playing = pause = false;
	return true;
}

bool CDROM_Interface_Fake :: GetMediaTrayStatus(bool& mediaPresent, bool& mediaChanged, bool& trayOpen) {
	mediaPresent = true;
	mediaChanged = false;
	trayOpen     = false;
	return true;
}

// Simulate the delay a physical CD-ROM drive took to respond to queries. When
// added to calls, this ensures that back-to-back queries report monotonically
// increasing Minute-Second-Frame (MSF) time values.
//
void CDROM_Interface::LagDriveResponse() const
{
	// Always simulate a very small amount of drive response time
	CALLBACK_Idle();

	// Handle tick-rollover
	static decltype(PIC_Ticks) prev_ticks = 0;
	prev_ticks = std::min(PIC_Ticks, prev_ticks);

	// Ensure results a monotonically increasing
	auto since_last_response_ms = [=]() { return PIC_Ticks - prev_ticks; };
	constexpr auto monotonic_response_ms = 1000 / REDBOOK_FRAMES_PER_SECOND;
	while (since_last_response_ms() < monotonic_response_ms) {
		CALLBACK_Idle();
	}

	prev_ticks = PIC_Ticks;
}

void CDROM_Interface_Physical::CdReaderLoop()
{
	std::unique_lock<std::mutex> lock(mutex);
	while (true) {
		waiter.wait(lock, [&]{ return sectors_remaining > 0 || should_exit; });

		if (should_exit) {
			return;
		}

		if (!queue.IsRunning()) {
			queue.Clear();
			queue.Start();
		}

		// Copy these to local variables so that we can release the mutex lock.
		const uint32_t local_sectors_remaining = sectors_remaining;
		const uint32_t local_sector_start = current_sector;
		lock.unlock();

		// Slow call to read from the CDROM drive. Avoid holding any locks
		const std::vector<int16_t> audio_samples = ReadAudio(local_sector_start, local_sectors_remaining);
		constexpr size_t NumSamplesPerFrame = 2;
		const size_t num_frames = audio_samples.size() / NumSamplesPerFrame;
		const size_t sectors_read = audio_samples.size() / SAMPLES_PER_REDBOOK_FRAME;

		// Convert from int16_t to float before enqueuing
		std::vector<AudioFrame> audio_frames;
		for (size_t i = 0; i < num_frames; ++i) {
			const int16_t left = host_to_le16(audio_samples[i * NumSamplesPerFrame]);
			const int16_t right = host_to_le16(audio_samples[(i * NumSamplesPerFrame) + 1]);
			audio_frames.emplace_back(left, right);
		}

		queue.BulkEnqueue(audio_frames, audio_frames.size());

		lock.lock();
		if (mixer_channel && !is_paused && sectors_remaining > 0 && queue.IsRunning()) {
			mixer_channel->Enable(true);
		}

		// Only modify these variables if they have not changed while we had the mutex lock released.
		// Ex. The game did not want to switch tracks or stop the audio while we were reading/enqueuing.
		if (sectors_remaining == local_sectors_remaining && current_sector == local_sector_start) {
			sectors_remaining -= sectors_read;
			current_sector += sectors_read;
		}
	}
}

void CDROM_Interface_Physical::InitAudio()
{
	if (mixer_channel) {
		return;
	}

	// Each audio channel must have a unique name.
	// Append a number after the CDAUDIO name.
	// This way we don't conflict with CDROM_Interface_Image or multiple drives.
	bool found_unique_name = false;
	std::string name;
	for (int i = 0; i < ChannelName::MaxCdAudioChannel; ++i) {
		name = std::string(ChannelName::CdAudio) + "_" + std::to_string(i);
		if (!MIXER_FindChannel(name.c_str())) {
			found_unique_name = true;
			break;
		}
	}

	if (!found_unique_name) {
		LOG_ERR("CDROM_IOCTL: Too many mixer channels");
		return;
	}

	auto callback = std::bind(&CDROM_Interface_Physical::CdAudioCallback,
	                          this,
	                          std::placeholders::_1);
	mixer_channel = MIXER_AddChannel(callback,
	                                 REDBOOK_PCM_FRAMES_PER_SECOND,
	                                 name.c_str(),
	                                 {ChannelFeature::Stereo,
	                                  ChannelFeature::DigitalAudio});

	thread = std::thread(&CDROM_Interface_Physical::CdReaderLoop, this);
}

void CDROM_Interface_Physical::CdAudioCallback(const uint16_t requested_frames)
{
	assert(requested_frames > 0);

	const size_t queue_size = queue.Size();
	if (queue_size == 0 && sectors_remaining == 0) {
		mixer_channel->Enable(false);
		return;
	}

	std::vector<AudioFrame> audio_frames;
	const size_t num_frames = std::min(static_cast<size_t>(requested_frames), queue_size);
	if (num_frames > 0) {
		queue.BulkDequeue(audio_frames, num_frames);
	}

	audio_frames.resize(requested_frames);
	mixer_channel->AddSamples_sfloat(audio_frames.size(), &audio_frames[0][0]);
}

// Taken from CDROM_Interface_Image
void CDROM_Interface_Physical::ChannelControl(TCtrl ctrl)
{
	if (!mixer_channel) {
		return;
	}

	constexpr float MaxVolume = 255.0f;
	// Adjust the volume of our mixer channel as defined by the application
	mixer_channel->SetAppVolume(
	        {ctrl.vol[0] / MaxVolume, ctrl.vol[1] / MaxVolume});

	// Map the audio channels in our mixer channel as defined by the
	// application
	const auto left_mapped  = static_cast<LineIndex>(ctrl.out[0]);
	const auto right_mapped = static_cast<LineIndex>(ctrl.out[1]);
	mixer_channel->SetChannelMap({left_mapped, right_mapped});
#ifdef DEBUG_IOCTL
	LOG_INFO("CDROM_IOCTL: ChannelControl => volumes %d/255 and %d/255, "
	         "and left-right map %d, %d",
	         ctrl.vol[0],
	         ctrl.vol[1],
	         ctrl.out[0],
	         ctrl.out[1]);
#endif
}

bool CDROM_Interface_Physical::PlayAudioSector(const uint32_t start, uint32_t len)
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		queue.Stop();

		current_sector    = start;
		sectors_remaining = len;
		is_paused         = false;
	}
	waiter.notify_all();

#ifdef DEBUG_IOCTL
	LOG_INFO("CDROM_IOCTL: PlayAudioSector: start: %u len: %u", start, len);
#endif

	return true;
}

bool CDROM_Interface_Physical::PauseAudio(bool resume)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (mixer_channel) {
		mixer_channel->Enable(resume);
	}
	is_paused = !resume;

#ifdef DEBUG_IOCTL
	LOG_INFO("CDROM: PauseAudio => audio is now %s",
	         resume ? "unpaused" : "paused");
#endif

	return true;
}

bool CDROM_Interface_Physical::StopAudio()
{
	std::lock_guard<std::mutex> lock(mutex);
	if (mixer_channel) {
		mixer_channel->Enable(false);
	}
	is_paused         = false;
	current_sector    = 0;
	sectors_remaining = 0;

#ifdef DEBUG_IOCTL
	LOG_INFO("CDROM_IOCTL: StopAudio => stopped playback and halted the mixer");
#endif
	return true;
}

bool CDROM_Interface_Physical::GetAudioStatus(bool& playing, bool& pause)
{
	playing = sectors_remaining > 0;
	pause   = is_paused;

#ifdef DEBUG_IOCTL
	LOG_INFO("CDROM_IOCTL: GetAudioStatus => %s and %s",
	         playing ? "is playing" : "stopped",
	         pause ? "paused" : "not paused");
#endif

	return true;
}

CDROM_Interface_Physical::~CDROM_Interface_Physical()
{
	if (mixer_channel) {
		MIXER_DeregisterChannel(mixer_channel);
	}
	if (thread.joinable()) {
		mutex.lock();
		should_exit = true;
		mutex.unlock();
		waiter.notify_all();
		queue.Stop();
		thread.join();
	}
}
