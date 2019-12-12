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


#ifndef __CDROM_INTERFACE__
#define __CDROM_INTERFACE__

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <SDL.h>
#include <SDL_thread.h>

#include "dosbox.h"
#include "mem.h"
#include "mixer.h"
#include "../libs/decoders/SDL_sound.h"

// CDROM data and audio format constants
#define BYTES_PER_RAW_REDBOOK_FRAME    2352
#define BYTES_PER_COOKED_REDBOOK_FRAME 2048
#define REDBOOK_FRAMES_PER_SECOND        75
#define MAX_REDBOOK_FRAMES           400000 // frames are Redbook's data unit
#define MAX_REDBOOK_SECTOR           399999 // a sector is the index to a frame
#define MAX_REDBOOK_TRACKS               99
#define MIN_REDBOOK_TRACKS                2 // One track plus the lead-out track
#define REDBOOK_PCM_BYTES_PER_MS     176.4f // 44.1 frames/ms * 4 bytes/frame
#define BYTES_PER_REDBOOK_PCM_FRAME       4 // 2 bytes/sample * 2 samples/frame

typedef struct SMSF {
	unsigned char min;
	unsigned char sec;
	unsigned char fr;
} TMSF;

typedef struct SCtrl {
	Bit8u	out[4];			// output channel mapping
	Bit8u	vol[4];			// channel volume (0 to 255)
} TCtrl;

// Conversion function from frames to Minutes/Second/Frames
//
template<typename T>
inline void frames_to_msf(int frames, T *m, T *s, T *f) {
	*f = frames % REDBOOK_FRAMES_PER_SECOND;
	frames /= REDBOOK_FRAMES_PER_SECOND;
	*s = frames % 60;
	frames /= 60;
	*m = frames;
}

// Conversion function from Minutes/Second/Frames to frames
//
inline int msf_to_frames(int m, int s, int f) {
	return m * 60 * REDBOOK_FRAMES_PER_SECOND + s * REDBOOK_FRAMES_PER_SECOND + f;
}

class CDROM_Interface
{
public:
	virtual ~CDROM_Interface        (void) {}
	virtual bool SetDevice          (char *path) = 0;
	virtual bool GetUPC             (unsigned char& attr, char* upc) = 0;
	virtual bool GetAudioTracks     (int& stTrack, int& end, TMSF& leadOut) = 0;
	virtual bool GetAudioTrackInfo  (int track, TMSF& start, unsigned char& attr) = 0;
	virtual bool GetAudioSub        (unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos) = 0;
	virtual bool GetAudioStatus     (bool& playing, bool& pause) = 0;
	virtual bool GetMediaTrayStatus (bool& mediaPresent, bool& mediaChanged, bool& trayOpen) = 0;
	virtual bool PlayAudioSector    (unsigned long start,unsigned long len) = 0;
	virtual bool PauseAudio         (bool resume) = 0;
	virtual bool StopAudio          (void) = 0;
	virtual void ChannelControl     (TCtrl ctrl) = 0;
	virtual bool ReadSectors        (PhysPt buffer, bool raw, unsigned long sector, unsigned long num) = 0;
	virtual bool LoadUnloadMedia    (bool unload) = 0;
	virtual void InitNewMedia       (void) {};
};

class CDROM_Interface_Fake : public CDROM_Interface
{
public:
	bool SetDevice          (char *) { return true; }
	bool GetUPC             (unsigned char& attr, char* upc) { attr = 0; strcpy(upc,"UPC"); return true; };
	bool GetAudioTracks     (int& stTrack, int& end, TMSF& leadOut);
	bool GetAudioTrackInfo  (int track, TMSF& start, unsigned char& attr);
	bool GetAudioSub        (unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos);
	bool GetAudioStatus     (bool& playing, bool& pause);
	bool GetMediaTrayStatus (bool& mediaPresent, bool& mediaChanged, bool& trayOpen);
	bool PlayAudioSector    (unsigned long /*start*/,unsigned long /*len*/) { return true; };
	bool PauseAudio         (bool /*resume*/) { return true; };
	bool StopAudio          (void) { return true; };
	void ChannelControl     (TCtrl ctrl) { (void)ctrl; // unused by part of the API
	                                       return; };
	bool ReadSectors        (PhysPt /*buffer*/, bool /*raw*/, unsigned long /*sector*/, unsigned long /*num*/) { return true; };
	bool LoadUnloadMedia    (bool /*unload*/) { return true; };
};

class CDROM_Interface_Image : public CDROM_Interface
{
private:
	// Nested Class Definitions
	class TrackFile {
	protected:
		TrackFile(Bit16u _chunkSize) : chunkSize(_chunkSize) {}
	public:
		virtual         ~TrackFile() = default;
		virtual bool    read(Bit8u *buffer, int seek, int count) = 0;
		virtual bool    seek(Bit32u offset) = 0;
		virtual Bit32u  decode(Bit16s *buffer, Bit32u desired_track_frames) = 0;
		virtual Bit16u  getEndian() = 0;
		virtual Bit32u  getRate() = 0;
		virtual Bit8u   getChannels() = 0;
		virtual int     getLength() = 0;
		const Bit16u    chunkSize;
	};

	class BinaryFile : public TrackFile {
	public:
		BinaryFile      (const char *filename, bool &error);
		~BinaryFile     ();

		BinaryFile      () = delete;
		BinaryFile      (const BinaryFile&) = delete; // prevent copying
		BinaryFile&     operator= (const BinaryFile&) = delete; // prevent assignment

		bool            read(Bit8u *buffer, int seek, int count);
		bool            seek(Bit32u offset);
		Bit32u          decode(Bit16s *buffer, Bit32u desired_track_frames);
		Bit16u          getEndian();
		Bit32u          getRate() { return 44100; }
		Bit8u           getChannels() { return 2; }
		int             getLength();
	private:
		std::ifstream   *file;
	};

	class AudioFile : public TrackFile {
	public:
		AudioFile       (const char *filename, bool &error);
		~AudioFile      ();

		AudioFile       () = delete;
		AudioFile       (const AudioFile&) = delete; // prevent copying
		AudioFile&      operator= (const AudioFile&) = delete; // prevent assignment

		bool            read(Bit8u *buffer, int seek, int count) {
		                    (void)buffer; // unused but part of the API
		                    (void)seek;   // ...
		                    (void)count;  // ...
		                    return false; }
		bool            seek(Bit32u offset);
		Bit32u          decode(Bit16s *buffer, Bit32u desired_track_frames);
		Bit16u          getEndian();
		Bit32u          getRate();
		Bit8u           getChannels();
		int             getLength();
	private:
		Sound_Sample    *sample;
	};

public:
	// Nested struct definition
	struct Track {
		TrackFile *file;
		int       number;
		int       attr;
		int       start;
		int       length;
		int       skip;
		int       sectorSize;
		bool      mode2;
	};
	CDROM_Interface_Image           (Bit8u _subUnit);
	virtual ~CDROM_Interface_Image  (void);
	void	InitNewMedia            (void);
	bool	SetDevice               (char *path);
	bool	GetUPC                  (unsigned char& attr, char* upc);
	bool	GetAudioTracks          (int& stTrack, int& end, TMSF& leadOut);
	bool	GetAudioTrackInfo       (int track, TMSF& start, unsigned char& attr);
	bool	GetAudioSub             (unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos);
	bool	GetAudioStatus          (bool& playing, bool& pause);
	bool	GetMediaTrayStatus      (bool& mediaPresent, bool& mediaChanged, bool& trayOpen);
	bool	PlayAudioSector         (unsigned long start,unsigned long len);
	bool	PauseAudio              (bool resume);
	bool	StopAudio               (void);
	void	ChannelControl          (TCtrl ctrl);
	bool	ReadSectors             (PhysPt buffer, bool raw, unsigned long sector, unsigned long num);
	bool	LoadUnloadMedia         (bool unload);
	bool	ReadSector              (Bit8u *buffer, bool raw, unsigned long sector);
	bool	HasDataTrack            (void);
	static CDROM_Interface_Image* images[26];

private:
	static struct imagePlayer {
		Bit16s                buffer[MIXER_BUFSIZE * 2]; // 2 channels (max)
		SDL_mutex             *mutex;
		TrackFile             *trackFile;
		MixerChannel          *channel;
		CDROM_Interface_Image *cd;
		void                  (MixerChannel::*addFrames) (Bitu, const Bit16s*);
		Bit32u                startSector;
		Bit32u                totalRedbookFrames;
		Bit32u                playedTrackFrames;
		Bit32u                totalTrackFrames;
		bool                  isPlaying;
		bool                  isPaused;
	} player;

	// Private utility functions
	void  ClearTracks();
	bool  LoadIsoFile(char *filename);
	bool  CanReadPVD(TrackFile *file, int sectorSize, bool mode2);
	std::vector<Track>::iterator GetTrack(int sector);
	static void CDAudioCallBack (Bitu desired_frames);

	// Private functions for cue sheet processing
	bool  LoadCueSheet(char *cuefile);
	bool  GetRealFileName(std::string& filename, std::string& pathname);
	bool  GetCueKeyword(std::string &keyword, std::istream &in);
	bool  GetCueFrame(int &frames, std::istream &in);
	bool  GetCueString(std::string &str, std::istream &in);
	bool  AddTrack(Track &curr, int &shift, int prestart, int &totalPregap, int currPregap);

	// member variables
	std::vector<Track>  tracks;
	std::string         mcn;
	static int          refCount;
	Bit8u               subUnit;
};

#endif /* __CDROM_INTERFACE__ */
