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
