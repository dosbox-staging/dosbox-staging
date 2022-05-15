/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2019-2022  The DOSBox Staging Team
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

#ifndef DOSBOX_CDROM_H
#define DOSBOX_CDROM_H

#include "dosbox.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <SDL.h>
#include <SDL_thread.h>

#include "support.h"
#include "mem.h"
#include "mixer.h"
#include "../../libs/sdlcd/SDL_cdrom.h"
#include "../libs/decoders/SDL_sound.h"

// CDROM data and audio format constants
#define BYTES_PER_RAW_REDBOOK_FRAME    2352u
#define BYTES_PER_COOKED_REDBOOK_FRAME 2048u
#define REDBOOK_FRAMES_PER_SECOND        75u
#define REDBOOK_CHANNELS                  2u
#define REDBOOK_BPS                       2u // bytes per sample
#define REDBOOK_PCM_FRAMES_PER_SECOND 44100u // also CD Audio sampling rate
#define REDBOOK_FRAME_PADDING           150u // The relationship between High Sierra sectors and Redbook
                                             // frames is described by the equation:
                                             // Sector = Minute * 60 * 75 + Second * 75 + Frame - 150
#define MAX_REDBOOK_FRAMES           400000u // frames are Redbook's data unit
#define MAX_REDBOOK_SECTOR           399999u // a sector is the index to a frame
#define MAX_REDBOOK_TRACKS               99u // a CD can contain 99 playable tracks plus the remaining leadout
#define MIN_REDBOOK_TRACKS                2u // One track plus the lead-out track
#define REDBOOK_PCM_BYTES_PER_MS      176.4f // 44.1 frames/ms * 4 bytes/frame
#define REDBOOK_PCM_BYTES_PER_MIN  10584000u // 44.1 frames/ms * 4 bytes/frame * 1000 ms/s * 60 s/min
#define BYTES_PER_REDBOOK_PCM_FRAME       4u // 2 bytes/sample * 2 samples/frame
#define MAX_REDBOOK_BYTES (MAX_REDBOOK_FRAMES * BYTES_PER_RAW_REDBOOK_FRAME) // length of a CDROM in bytes
#define MAX_REDBOOK_DURATION_MS (99 * 60 * 1000) // 99 minute CDROM in milliseconds
#define AUDIO_DECODE_BUFFER_SIZE 16512u

enum { CDROM_USE_SDL };

struct TMSF
{
	unsigned char min;
	unsigned char sec;
	unsigned char fr;
};

typedef struct SCtrl {
	uint8_t	out[4];			// output channel mapping
	uint8_t	vol[4];			// channel volume (0 to 255)
} TCtrl;

// Conversion function from frames to Minutes/Second/Frames
//
inline TMSF frames_to_msf(uint32_t frames)
{
	TMSF msf = {0, 0, 0};
	msf.fr = frames % REDBOOK_FRAMES_PER_SECOND;
	frames /= REDBOOK_FRAMES_PER_SECOND;
	msf.sec = frames % 60;
	frames /= 60;
	msf.min = static_cast<uint8_t>(frames);
	return msf;
}

// Conversion function from Minutes/Second/Frames to frames
//
inline uint32_t msf_to_frames(const TMSF &msf)
{
	return msf.min * 60 * REDBOOK_FRAMES_PER_SECOND + msf.sec * REDBOOK_FRAMES_PER_SECOND + msf.fr;
}

class CDROM_Interface
{
public:
	virtual ~CDROM_Interface        () {}
	virtual bool SetDevice          (const char *path, const int cd_number) = 0;
	virtual bool GetUPC             (unsigned char& attr, char* upc) = 0;
	virtual bool GetAudioTracks     (uint8_t& stTrack, uint8_t& end, TMSF& leadOut) = 0;
	virtual bool GetAudioTrackInfo  (uint8_t track, TMSF& start, unsigned char& attr) = 0;
	virtual bool GetAudioSub        (unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos) = 0;
	virtual bool GetAudioStatus     (bool& playing, bool& pause) = 0;
	virtual bool GetMediaTrayStatus (bool& mediaPresent, bool& mediaChanged, bool& trayOpen) = 0;
	virtual bool PlayAudioSector    (const uint32_t start, uint32_t len) = 0;
	virtual bool PauseAudio         (bool resume) = 0;
	virtual bool StopAudio          () = 0;
	virtual void ChannelControl     (TCtrl ctrl) = 0;
	virtual bool ReadSectors        (PhysPt buffer, const bool raw, const uint32_t sector, const uint16_t num) = 0;
	virtual bool ReadSectorsHost    (void* buffer, bool raw, unsigned long sector, unsigned long num) = 0;
	virtual bool LoadUnloadMedia    (bool unload) = 0;
	virtual void InitNewMedia       () {}
};

class CDROM_Interface_SDL : public CDROM_Interface
{
public:
	CDROM_Interface_SDL();
	CDROM_Interface_SDL(const CDROM_Interface_SDL&);
	CDROM_Interface_SDL& operator = (const CDROM_Interface_SDL&);
	virtual ~CDROM_Interface_SDL();
	virtual bool SetDevice(const char *path, int cd_number);
	virtual bool GetUPC(unsigned char &attr, char *upc)
	{
		attr = '\0';
		strcpy(upc, "UPC");
		return true;
	}
	virtual bool GetAudioTracks(uint8_t &stTrack, uint8_t &end, TMSF &leadOut);
	virtual bool GetAudioTrackInfo(uint8_t track, TMSF &start, unsigned char &attr);
	virtual bool GetAudioSub(unsigned char &attr,
	                         unsigned char &track,
	                         unsigned char &index,
	                         TMSF &relPos,
	                         TMSF &absPos);
	virtual bool GetAudioStatus(bool &playing, bool &pause);
	virtual bool GetMediaTrayStatus(bool &mediaPresent,
	                                bool &mediaChanged,
	                                bool &trayOpen);
	virtual bool PlayAudioSector(const uint32_t start, uint32_t len);
	virtual bool PauseAudio(bool resume);
	virtual bool StopAudio();
	virtual void ChannelControl([[maybe_unused]] TCtrl ctrl)
	{
		return;
	}
	virtual bool ReadSectors([[maybe_unused]] PhysPt buffer,
	                         [[maybe_unused]] const bool raw,
	                         [[maybe_unused]] const uint32_t sector,
	                         [[maybe_unused]] const uint16_t num)
	{
		return false;
	}
	virtual bool ReadSectorsHost([[maybe_unused]] void *buffer,
	                             [[maybe_unused]] bool raw,
	                             [[maybe_unused]] unsigned long sector,
	                             [[maybe_unused]] unsigned long num)
	{
		return true;
	}
	virtual bool LoadUnloadMedia(bool unload);

private:
	bool Open();
	void Close();

	SDL_CD *cd = nullptr;
	int driveID = 0;
	Uint32 oldLeadOut = 0;
};

class CDROM_Interface_Fake final : public CDROM_Interface
{
public:
	bool SetDevice          ([[maybe_unused]] const char *path, [[maybe_unused]] const int cd_number) { return true; }
	bool GetUPC             (unsigned char& attr, char* upc) { attr = 0; strcpy(upc,"UPC"); return true; }
	bool GetAudioTracks     (uint8_t& stTrack, uint8_t& end, TMSF& leadOut);
	bool GetAudioTrackInfo  (uint8_t track, TMSF& start, unsigned char& attr);
	bool GetAudioSub        (unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos);
	bool GetAudioStatus     (bool& playing, bool& pause);
	bool GetMediaTrayStatus (bool& mediaPresent, bool& mediaChanged, bool& trayOpen);
	bool PlayAudioSector    (const uint32_t start, uint32_t len) { (void)start; (void)len; return true; }
	bool PauseAudio         (bool /*resume*/) { return true; }
	bool StopAudio          () { return true; }

	void ChannelControl([[maybe_unused]] TCtrl ctrl) {}

	bool ReadSectors        (PhysPt /*buffer*/, const bool /*raw*/, const uint32_t /*sector*/, const uint16_t /*num*/) { return true; }
	bool ReadSectorsHost    ([[maybe_unused]] void* buffer, [[maybe_unused]] bool raw, [[maybe_unused]] unsigned long sector, [[maybe_unused]] unsigned long num) { return true; }
	bool LoadUnloadMedia    (bool /*unload*/) { return true; }
};

class CDROM_Interface_Image final : public CDROM_Interface
{
private:
	// Nested Class Definitions
	class TrackFile {
	protected:
		TrackFile(uint16_t _chunkSize) : chunkSize(_chunkSize) {}
		bool offsetInsideTrack(const uint32_t offset);
		uint32_t adjustOverRead(const uint32_t offset,
		                        const uint32_t requested_bytes);
		int length_redbook_bytes = -1;
		uint32_t audio_pos = std::numeric_limits<uint32_t>::max(); // last position when playing audio

	public:
		virtual          ~TrackFile() = default;
		virtual bool     read(uint8_t *buffer,
		                      const uint32_t offset,
		                      const uint32_t requested_bytes) = 0;
		virtual bool     seek(const uint32_t offset) = 0;
		virtual uint32_t decode(int16_t *buffer, const uint32_t desired_track_frames) = 0;
		virtual uint16_t   getEndian() = 0;
		virtual uint32_t   getRate() = 0;
		virtual uint8_t    getChannels() = 0;
		virtual int      getLength() = 0;
		virtual void setAudioPosition(uint32_t pos) = 0;
		const uint16_t chunkSize = 0;
	};

	class BinaryFile final : public TrackFile {
	public:
		BinaryFile      (const char *filename, bool &error);
		~BinaryFile     ();

		BinaryFile      () = delete;
		BinaryFile      (const BinaryFile&) = delete; // prevent copying
		BinaryFile&     operator= (const BinaryFile&) = delete; // prevent assignment

		bool            read(uint8_t *buffer,
		                     const uint32_t offset,
		                     const uint32_t requested_bytes);
		bool            seek(const uint32_t offset);
		uint32_t        decode(int16_t *buffer, const uint32_t desired_track_frames);
		uint16_t          getEndian();
		uint32_t          getRate() { return 44100; }
		uint8_t           getChannels() { return 2; }
		int             getLength();
		void setAudioPosition(uint32_t pos) { audio_pos = pos; }

	private:
		std::ifstream   *file;
	};

	class AudioFile final : public TrackFile {
	public:
		AudioFile       (const char *filename, bool &error);
		~AudioFile      ();

		AudioFile       () = delete;
		AudioFile       (const AudioFile&) = delete; // prevent copying
		AudioFile&      operator= (const AudioFile&) = delete; // prevent assignment

		bool            read(uint8_t *buffer,
		                     const uint32_t offset,
		                     const uint32_t requested_bytes);
		bool            seek(const uint32_t offset);
		uint32_t        decode(int16_t *buffer, const uint32_t desired_track_frames);
		uint16_t          getEndian();
		uint32_t          getRate();
		uint8_t           getChannels();
		int             getLength();
		// This is a no-op because we track the audio position in all
		// areas of this class.
		void setAudioPosition([[maybe_unused]] uint32_t pos) {}
	private:
		Sound_Sample *sample = nullptr;
	};

public:
	// Nested struct definition
	struct Track {
		std::shared_ptr<TrackFile> file       = nullptr;
		uint32_t                   start      = 0;
		uint32_t                   length     = 0;
		uint32_t                   skip       = 0;
		uint16_t                   sectorSize = 0;
		uint8_t                    number     = 0;
		uint8_t                    attr       = 0;
		bool                       mode2      = false;
	};

	CDROM_Interface_Image(uint8_t sub_unit);

	virtual ~CDROM_Interface_Image  ();
	void	InitNewMedia            () {}
	bool	SetDevice               (const char *path, const int cd_number);
	bool	GetUPC                  (unsigned char& attr, char* upc);
	bool	GetAudioTracks          (uint8_t& stTrack, uint8_t& end, TMSF& leadOut);
	bool	GetAudioTrackInfo       (uint8_t track, TMSF& start, unsigned char& attr);
	bool	GetAudioSub             (unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos);
	bool	GetAudioStatus          (bool& playing, bool& pause);
	bool	GetMediaTrayStatus      (bool& mediaPresent, bool& mediaChanged, bool& trayOpen);
	bool	PlayAudioSector         (const uint32_t start, uint32_t len);
	bool	PauseAudio              (bool resume);
	bool	StopAudio               ();
	void	ChannelControl          (TCtrl ctrl);
	bool	ReadSectors             (PhysPt buffer, const bool raw, const uint32_t sector, const uint16_t num);
	bool	ReadSectorsHost         (void* buffer, bool raw, unsigned long sector, unsigned long num);
	bool	LoadUnloadMedia         (bool unload);
	bool	ReadSector              (uint8_t *buffer, const bool raw, const uint32_t sector);
	bool	HasDataTrack            ();
	static CDROM_Interface_Image* images[26];

private:
	static struct imagePlayer {
		// Objects, pointers, and then scalars; in descending size-order.
		std::weak_ptr<TrackFile> trackFile = {};
		mixer_channel_t channel = nullptr;
		CDROM_Interface_Image    *cd                = nullptr;
		void (MixerChannel::*addFrames)(uint16_t, const int16_t *) = nullptr;
		uint32_t                 playedTrackFrames  = 0;
		uint32_t                 totalTrackFrames   = 0;
		uint32_t                 startSector        = 0;
		uint32_t                 totalRedbookFrames = 0;
		int16_t                  buffer[MIXER_BUFSIZE * REDBOOK_CHANNELS] = {0};
		bool                     isPlaying          = false;
		bool                     isPaused           = false;
	} player;

	// Private utility functions
	bool  LoadIsoFile(char *filename);
	bool  CanReadPVD(TrackFile *file,
	                 const uint16_t sectorSize,
	                 const bool mode2);
	std::vector<Track>::iterator GetTrack(const uint32_t sector);
	void CDAudioCallBack(uint16_t desired_frames);

	// Private functions for cue sheet processing
	bool  LoadCueSheet(char *cuefile);
	bool  GetRealFileName(std::string& filename, std::string& pathname);
	bool  GetCueKeyword(std::string &keyword, std::istream &in);
	bool  GetCueFrame(uint32_t &frames, std::istream &in);
	bool  GetCueString(std::string &str, std::istream &in);
	bool  AddTrack(Track         &curr,
	               uint32_t      &shift,
	               const int32_t prestart,
	               uint32_t      &totalPregap,
	               uint32_t      currPregap);
	// member variables
	std::vector<Track>   tracks;
	std::vector<uint8_t> readBuffer;
	std::string          mcn;
	static int           refCount;
};

#if defined (LINUX)
class CDROM_Interface_Ioctl : public CDROM_Interface_SDL
{
public:
	CDROM_Interface_Ioctl		();

	bool	SetDevice		(const char* path, const int cd_number);
	bool	GetUPC			(unsigned char& attr, char* upc);
	bool	ReadSectors		(PhysPt buffer, const bool raw, const uint32_t sector, const uint16_t num);
	bool	ReadSectorsHost	(void* buffer, bool raw, unsigned long sector, unsigned long num);

private:
	char	device_name[512];
};

#endif /* LINUX */

extern "C" SDL_CD *SDL_CDOpen(int drive);
extern "C" void SDL_CDClose(SDL_CD *cdrom);
extern "C" CDstatus SDL_CDStatus(SDL_CD *cdrom);
extern "C" int SDL_CDPlay(SDL_CD *cdrom, int sframe, int length);
extern "C" int SDL_CDPause(SDL_CD *cdrom);
extern "C" int SDL_CDResume(SDL_CD *cdrom);
extern "C" int SDL_CDStop(SDL_CD *cdrom);
extern "C" int SDL_CDEject(SDL_CD *cdrom);
extern "C" const char *SDL_CDName(int drive);
extern "C" int SDL_CDNumDrives();
extern "C" int SDL_CDROMInit();
extern "C" void SDL_CDROMQuit();
void MSCDEX_SetCDInterface(int int_nr, int num_cd);

#endif
