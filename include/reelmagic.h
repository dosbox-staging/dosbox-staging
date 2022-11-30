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

#ifndef DOSBOX_REELMAGIC_H
#define DOSBOX_REELMAGIC_H

#include "dosbox.h"

#include "dos_inc.h"
//
// video mixer stuff
//
struct ReelMagic_PlayerConfiguration;
struct ReelMagic_PlayerAttributes;
struct ReelMagic_VideoMixerMPEGProvider {
	virtual ~ReelMagic_VideoMixerMPEGProvider() {}
	virtual void OnVerticalRefresh(void* const outputBuffer, const float fps) = 0;
	virtual const ReelMagic_PlayerConfiguration& GetConfig() const = 0;
	virtual const ReelMagic_PlayerAttributes& GetAttrs() const     = 0;
};

void ReelMagic_RENDER_SetPal(uint8_t entry, uint8_t red, uint8_t green, uint8_t blue);
void ReelMagic_RENDER_SetSize(uint32_t width, uint32_t height, uint32_t bpp,
                              double fps, double ratio, bool dblw, bool dblh);
bool ReelMagic_RENDER_StartUpdate(void);
// void ReelMagic_RENDER_EndUpdate(bool abort);
// void ReelMagic_RENDER_DrawLine(const void *src);
typedef void (*ReelMagic_ScalerLineHandler_t)(const void* src);
extern ReelMagic_ScalerLineHandler_t ReelMagic_RENDER_DrawLine;

bool ReelMagic_IsVideoMixerEnabled();
void ReelMagic_ClearVideoMixer();
void ReelMagic_SetVideoMixerEnabled(const bool enabled);
ReelMagic_VideoMixerMPEGProvider* ReelMagic_GetVideoMixerMPEGProvider();
void ReelMagic_SetVideoMixerMPEGProvider(ReelMagic_VideoMixerMPEGProvider* const provider);
void ReelMagic_ClearVideoMixerMPEGProvider();
void ReelMagic_InitVideoMixer(Section* /*sec*/);

// audio mixer related
constexpr auto reelmagic_channel_name = "REELMAGIC";
void ReelMagic_EnableAudioChannel(const bool should_enable);

//
// player stuff
//

// FMPDRV.EXE uses handle value 0 as invalid and 1+ as valid
using reelmagic_handle_t = uint8_t;

constexpr reelmagic_handle_t reelmagic_invalid_handle = 0;
constexpr reelmagic_handle_t reelmagic_first_handle   = 1;
constexpr reelmagic_handle_t reelmagic_last_handle    = {DOS_FILES - 1};

struct ReelMagic_PlayerConfiguration {
	bool VideoOutputVisible = false;
	bool UnderVga           = false;

	uint8_t VgaAlphaIndex   = 0;
	uint32_t MagicDecodeKey = 0;
	uint32_t UserData       = 0;
	struct {
		uint16_t X = 0;
		uint16_t Y = 0;
	} DisplayPosition = {};
	struct {
		uint16_t Width  = 0;
		uint16_t Height = 0;
	} DisplaySize = {};
};
struct ReelMagic_PlayerAttributes {
	struct {
		reelmagic_handle_t Base  = reelmagic_invalid_handle;
		reelmagic_handle_t Demux = reelmagic_invalid_handle;
		reelmagic_handle_t Video = reelmagic_invalid_handle;
		reelmagic_handle_t Audio = reelmagic_invalid_handle;
	} Handles = {};
	struct {
		uint16_t Width  = 0;
		uint16_t Height = 0;
	} PictureSize = {};
};
struct ReelMagic_MediaPlayerFile {
	virtual ~ReelMagic_MediaPlayerFile() {}

	virtual const char* GetFileName() const = 0;
	virtual uint32_t GetFileSize() const    = 0;

	virtual uint32_t Read(uint8_t* data, uint32_t amount) = 0;
	// type can be either DOS_SEEK_SET || DOS_SEEK_CUR...
	virtual void Seek(uint32_t pos, uint32_t type) = 0;
};
struct ReelMagic_MediaPlayer {
	virtual ~ReelMagic_MediaPlayer() {}
	virtual ReelMagic_PlayerConfiguration& Config()            = 0;
	virtual const ReelMagic_PlayerAttributes& GetAttrs() const = 0;

	virtual bool HasDemux() const = 0;
	virtual bool HasVideo() const = 0;
	virtual bool HasAudio() const = 0;

	virtual bool IsPlaying() const       = 0;
	virtual Bitu GetBytesDecoded() const = 0;

	enum PlayMode {
		MPPM_PAUSEONCOMPLETE,
		MPPM_STOPONCOMPLETE,
		MPPM_LOOP,
	};
	virtual void Play(const PlayMode playMode = MPPM_PAUSEONCOMPLETE) = 0;
	virtual void Pause()                                              = 0;
	virtual void Stop()                                               = 0;
	virtual void SeekToByteOffset(const uint32_t offset)              = 0;
	virtual void NotifyConfigChange()                                 = 0;
};

// note: once a player file object is handed to new/delete player, regardless of
// success, it will be cleaned up
reelmagic_handle_t ReelMagic_NewPlayer(struct ReelMagic_MediaPlayerFile* const playerFile);
void ReelMagic_DeletePlayer(const reelmagic_handle_t handle);
ReelMagic_MediaPlayer& ReelMagic_HandleToMediaPlayer(const reelmagic_handle_t handle); // throws on invalid handle
void ReelMagic_DeleteAllPlayers();

void ReelMagic_InitPlayer(Section* /*sec*/);
void ReelMagic_ClearPlayers();
ReelMagic_PlayerConfiguration& ReelMagic_GlobalDefaultPlayerConfig();

//
// driver and general stuff
//
void ReelMagic_Init(Section* /*sec*/);

#endif /* #ifndef DOSBOX_REELMAGIC_H */
