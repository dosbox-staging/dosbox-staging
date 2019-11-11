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

#define MAX_ASPI_CDROM	5

#include <string.h>
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <SDL.h>
#include <SDL_thread.h>

#include "dosbox.h"
#include "mem.h"
#include "mixer.h"
#include "../libs/decoders/SDL_sound.h"

// CDROM data and audio format constants
#define RAW_SECTOR_SIZE         2352
#define COOKED_SECTOR_SIZE      2048
#define BYTES_PER_TRACK_FRAME      4
#define REDBOOK_FRAMES_PER_SECOND 75

enum { CDROM_USE_SDL, CDROM_USE_ASPI, CDROM_USE_IOCTL_DIO, CDROM_USE_IOCTL_DX, CDROM_USE_IOCTL_MCI };

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

extern int CDROM_GetMountType(char* path, int force);

class CDROM_Interface
{
public:
	virtual ~CDROM_Interface			(void) {};

	virtual bool	SetDevice			(char* path, int forceCD) = 0;

	virtual bool	GetUPC				(unsigned char& attr, char* upc) = 0;

	virtual bool	GetAudioTracks		(int& stTrack, int& end, TMSF& leadOut) = 0;
	virtual bool	GetAudioTrackInfo	(int track, TMSF& start, unsigned char& attr) = 0;
	virtual bool	GetAudioSub			(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos) = 0;
	virtual bool	GetAudioStatus		(bool& playing, bool& pause) = 0;
	virtual bool	GetMediaTrayStatus	(bool& mediaPresent, bool& mediaChanged, bool& trayOpen) = 0;

	virtual bool	PlayAudioSector		(unsigned long start,unsigned long len) = 0;
	virtual bool	PauseAudio			(bool resume) = 0;
	virtual bool	StopAudio			(void) = 0;
	virtual void	ChannelControl		(TCtrl ctrl) = 0;
	
	virtual bool	ReadSectors			(PhysPt buffer, bool raw, unsigned long sector, unsigned long num) = 0;

	virtual bool	LoadUnloadMedia		(bool unload) = 0;
	
	virtual void	InitNewMedia		(void) {};
};	

class CDROM_Interface_SDL : public CDROM_Interface
{
public:
	CDROM_Interface_SDL			(void);
	virtual ~CDROM_Interface_SDL(void);

	virtual bool	SetDevice			(char* path, int forceCD);
	virtual bool	GetUPC				(unsigned char& attr, char* upc) { attr = 0; strcpy(upc,"UPC"); return true; };
	virtual bool	GetAudioTracks		(int& stTrack, int& end, TMSF& leadOut);
	virtual bool	GetAudioTrackInfo	(int track, TMSF& start, unsigned char& attr);
	virtual bool	GetAudioSub			(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos);
	virtual bool	GetAudioStatus		(bool& playing, bool& pause);
	virtual bool	GetMediaTrayStatus	(bool& mediaPresent, bool& mediaChanged, bool& trayOpen);
	virtual bool	PlayAudioSector		(unsigned long start,unsigned long len);
	virtual bool	PauseAudio			(bool resume);
	virtual bool	StopAudio			(void);
	virtual void	ChannelControl		(TCtrl ctrl) { return; };
	virtual bool	ReadSectors			(PhysPt /*buffer*/, bool /*raw*/, unsigned long /*sector*/, unsigned long /*num*/) { return false; };
	virtual bool	LoadUnloadMedia		(bool unload);

private:
	bool	Open				(void);
	void	Close				(void);

	SDL_CD*	cd;
	int		driveID;
	Uint32	oldLeadOut;
};

class CDROM_Interface_Fake : public CDROM_Interface
{
public:
	bool	SetDevice			(char* /*path*/, int /*forceCD*/) { return true; };
	bool	GetUPC				(unsigned char& attr, char* upc) { attr = 0; strcpy(upc,"UPC"); return true; };
	bool	GetAudioTracks		(int& stTrack, int& end, TMSF& leadOut);
	bool	GetAudioTrackInfo	(int track, TMSF& start, unsigned char& attr);
	bool	GetAudioSub			(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos);
	bool	GetAudioStatus		(bool& playing, bool& pause);
	bool	GetMediaTrayStatus	(bool& mediaPresent, bool& mediaChanged, bool& trayOpen);
	bool	PlayAudioSector		(unsigned long /*start*/,unsigned long /*len*/) { return true; };
	bool	PauseAudio			(bool /*resume*/) { return true; };
	bool	StopAudio			(void) { return true; };
	void	ChannelControl		(TCtrl ctrl) { return; };
	bool	ReadSectors			(PhysPt /*buffer*/, bool /*raw*/, unsigned long /*sector*/, unsigned long /*num*/) { return true; };
	bool	LoadUnloadMedia		(bool /*unload*/) { return true; };
};

class CDROM_Interface_Image : public CDROM_Interface
{
private:
	class TrackFile {
	protected:
		TrackFile(Bit16u chunkSize) : chunkSize(chunkSize) {}
	public:
		virtual bool    read(Bit8u *buffer, int seek, int count) = 0;
		virtual bool    seek(Bit32u offset) = 0;
		virtual Bit32u  decode(Bit16s *buffer, Bit32u desired_track_frames) = 0;
		virtual Bit16u  getEndian() = 0;
		virtual Bit32u  getRate() = 0;
		virtual Bit8u   getChannels() = 0;
		virtual int     getLength() = 0;
		const Bit16u    chunkSize;
		virtual         ~TrackFile() {}
	};
	
	class BinaryFile : public TrackFile {
	public:
		BinaryFile      (const char *filename, bool &error);
		bool            read(Bit8u *buffer, int seek, int count);
		bool            seek(Bit32u offset);
		Bit32u          decode(Bit16s *buffer, Bit32u desired_track_frames);
		Bit16u          getEndian();
		Bit32u          getRate() { return 44100; }
		Bit8u           getChannels() { return 2; }
		int             getLength();
		~BinaryFile();
	private:
		std::ifstream   *file;
		BinaryFile();
	};
	
	class AudioFile : public TrackFile {
	public:
		AudioFile       (const char *filename, bool &error);
		bool            read(Bit8u *buffer, int seek, int count) { return false; }
		bool            seek(Bit32u offset);
		Bit32u          decode(Bit16s *buffer, Bit32u desired_track_frames);
		Bit16u          getEndian();
		Bit32u          getRate();
		Bit8u           getChannels();
		int             getLength();
		~AudioFile();
	private:
		Sound_Sample    *sample;
		AudioFile();
	};
	
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
	
public:
	CDROM_Interface_Image           (Bit8u subUnit);
	virtual ~CDROM_Interface_Image  (void);
	void	InitNewMedia            (void);
	bool	SetDevice               (char* path, int forceCD);
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
	
static	CDROM_Interface_Image* images[26];

private:
	// player
static	void	CDAudioCallBack(Bitu desired_frames);
	int	GetTrack(int sector);

static  struct imagePlayer {
		Bit16s                buffer[MIXER_BUFSIZE * 2]; // 2 channels (max)
		TrackFile             *trackFile;
		MixerChannel          *channel;
		CDROM_Interface_Image *cd;
		void                  (MixerChannel::*addFrames) (Bitu, const Bit16s*);
		Bit32u                startRedbookFrame;
		Bit32u                totalRedbookFrames;
		Bit32u                playedTrackFrames;
		Bit32u                totalTrackFrames;
		bool                  isPlaying;
		bool                  isPaused;
	} player;
	
	void  ClearTracks();
	bool  LoadIsoFile(char *filename);
	bool  CanReadPVD(TrackFile *file, int sectorSize, bool mode2);

	// cue sheet processing
	bool  LoadCueSheet(char *cuefile);
	bool  GetRealFileName(std::string& filename, std::string& pathname);
	bool  GetCueKeyword(std::string &keyword, std::istream &in);
	bool  GetCueFrame(int &frames, std::istream &in);
	bool  GetCueString(std::string &str, std::istream &in);
	bool  AddTrack(Track &curr, int &shift, int prestart, int &totalPregap, int currPregap);

	// member variables
	std::vector<Track>                   tracks;
	typedef	std::vector<Track>::iterator track_it;
	std::string                          mcn;
	static int                           refCount;
	Bit8u                                subUnit;
};

#if defined (WIN32)	/* Win 32 */

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include "wnaspi32.h"			// Aspi stuff 

class CDROM_Interface_Aspi : public CDROM_Interface
{
public:
	CDROM_Interface_Aspi		(void);
	virtual ~CDROM_Interface_Aspi(void);

	bool	SetDevice			(char* path, int forceCD);

	bool	GetUPC				(unsigned char& attr, char* upc);

	bool	GetAudioTracks		(int& stTrack, int& end, TMSF& leadOut);
	bool	GetAudioTrackInfo	(int track, TMSF& start, unsigned char& attr);
	bool	GetAudioSub			(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos);
	bool	GetAudioStatus		(bool& playing, bool& pause);
	bool	GetMediaTrayStatus	(bool& mediaPresent, bool& mediaChanged, bool& trayOpen);

	bool	PlayAudioSector		(unsigned long start,unsigned long len);
	bool	PauseAudio			(bool resume);
	bool	StopAudio			(void);
	void	ChannelControl		(TCtrl ctrl) { return; };
	
	bool	ReadSectors			(PhysPt buffer, bool raw, unsigned long sector, unsigned long num);

	bool	LoadUnloadMedia		(bool unload);
	
private:
	DWORD	GetTOC				(LPTOC toc);
	HANDLE	OpenIOCTLFile		(char cLetter, BOOL bAsync);
	void	GetIOCTLAdapter		(HANDLE hF,int * iDA,int * iDT,int * iDL);
	bool	ScanRegistryFindKey	(HKEY& hKeyBase);
	bool	ScanRegistry		(HKEY& hKeyBase);
	BYTE	GetHostAdapter		(char* hardwareID);
	bool	GetVendor			(BYTE HA_num, BYTE SCSI_Id, BYTE SCSI_Lun, char* szBuffer);
		
	// ASPI stuff
	BYTE	haId;
	BYTE	target;
	BYTE	lun;
	char	letter;

	// Windows stuff
	HINSTANCE	hASPI;
	HANDLE		hEvent;											// global event
	DWORD		(*pGetASPI32SupportInfo)	(void);             // ptrs to aspi funcs
	DWORD		(*pSendASPI32Command)		(LPSRB);
	TMSF		oldLeadOut;
};

class CDROM_Interface_Ioctl : public CDROM_Interface
{
public:
	enum cdioctl_cdatype { CDIOCTL_CDA_DIO, CDIOCTL_CDA_MCI, CDIOCTL_CDA_DX };
	cdioctl_cdatype cdioctl_cda_selected;

	CDROM_Interface_Ioctl		(CDROM_Interface_Ioctl::cdioctl_cdatype ioctl_cda);
	virtual ~CDROM_Interface_Ioctl(void);

	bool	SetDevice			(char* path, int forceCD);

	bool	GetUPC				(unsigned char& attr, char* upc);

	bool	GetAudioTracks		(int& stTrack, int& end, TMSF& leadOut);
	bool	GetAudioTrackInfo	(int track, TMSF& start, unsigned char& attr);
	bool	GetAudioSub			(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos);
	bool	GetAudioStatus		(bool& playing, bool& pause);
	bool	GetMediaTrayStatus	(bool& mediaPresent, bool& mediaChanged, bool& trayOpen);

	bool	PlayAudioSector		(unsigned long start,unsigned long len);
	bool	PauseAudio			(bool resume);
	bool	StopAudio			(void);
	void	ChannelControl		(TCtrl ctrl);
	
	bool	ReadSector			(Bit8u *buffer, bool raw, unsigned long sector);
	bool	ReadSectors			(PhysPt buffer, bool raw, unsigned long sector, unsigned long num);

	bool	LoadUnloadMedia		(bool unload);

	void	InitNewMedia		(void) { Close(); Open(); };
private:

	bool	Open				(void);
	void	Close				(void);

	char	pathname[32];
	HANDLE	hIOCTL;
	TMSF	oldLeadOut;


	/* track start/length data */
	bool	track_start_valid;
	int		track_start_first,track_start_last;
	int		track_start[128];

	bool	GetAudioTracksAll	(void);


	/* mci audio cd interface */
	bool	use_mciplay;
	int		mci_devid;

	bool	mci_CDioctl				(UINT msg, DWORD flags, void *arg);
	bool	mci_CDOpen				(char drive);
	bool	mci_CDClose				(void);
	bool	mci_CDPlay				(int start, int length);
	bool	mci_CDPause				(void);
	bool	mci_CDResume			(void);
	bool	mci_CDStop				(void);
	int		mci_CDStatus			(void);
	bool	mci_CDPosition			(int *position);


	/* digital audio extraction cd interface */
	static void dx_CDAudioCallBack(Bitu len);

	bool	use_dxplay;
	static  struct dxPlayer {
		CDROM_Interface_Ioctl *cd;
		MixerChannel	*channel;
		SDL_mutex		*mutex;
		Bit8u   buffer[8192];
		int     bufLen;
		int     currFrame;
		int     targetFrame;
		bool    isPlaying;
		bool    isPaused;
		bool    ctrlUsed;
		TCtrl   ctrlData;
	} player;

};

#endif /* WIN 32 */

#if defined (LINUX) || defined(OS2)

class CDROM_Interface_Ioctl : public CDROM_Interface_SDL
{
public:
	CDROM_Interface_Ioctl		(void);

	bool	SetDevice		(char* path, int forceCD);
	bool	GetUPC			(unsigned char& attr, char* upc);
	bool	ReadSectors		(PhysPt buffer, bool raw, unsigned long sector, unsigned long num);

private:
	char	device_name[512];
};

#endif /* LINUX */

#endif /* __CDROM_INTERFACE__ */
