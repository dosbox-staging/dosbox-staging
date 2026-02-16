// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2022 Jon Dennis
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_REELMAGIC_H
#define DOSBOX_REELMAGIC_H

#include "dosbox.h"

#include "config/config.h"
#include "dos/dos.h"
#include "gui/render/render.h"
#include "utils/fraction.h"

//
// video mixer stuff
//
struct ReelMagic_PlayerConfiguration;
struct ReelMagic_PlayerAttributes;
struct ReelMagic_VideoMixerMPEGProvider {
	virtual ~ReelMagic_VideoMixerMPEGProvider() = default;
	virtual void OnVerticalRefresh(void* const outputBuffer, const float fps) = 0;
	virtual const ReelMagic_PlayerConfiguration& GetConfig() const = 0;
	virtual const ReelMagic_PlayerAttributes& GetAttrs() const     = 0;
};

void ReelMagic_RENDER_SetPalette(const uint8_t entry, const uint8_t red,
                                 const uint8_t green, const uint8_t blue);

// forward declaration
struct ImageInfo;

void ReelMagic_RENDER_SetSize(const ImageInfo& image_info,
                              const double frames_per_second);

bool ReelMagic_RENDER_StartUpdate(void);

using ReelMagic_ScalerLineHandler = void (*)(const void* src);
extern ReelMagic_ScalerLineHandler ReelMagic_RENDER_DrawLine;

bool ReelMagic_IsVideoMixerEnabled();
void ReelMagic_ClearVideoMixer();
void ReelMagic_SetVideoMixerEnabled(const bool enabled);
ReelMagic_VideoMixerMPEGProvider* ReelMagic_GetVideoMixerMPEGProvider();
void ReelMagic_SetVideoMixerMPEGProvider(ReelMagic_VideoMixerMPEGProvider* const provider);
void ReelMagic_ClearVideoMixerMPEGProvider();
void ReelMagic_InitVideoMixer(Section* /*sec*/);

// audio mixer related
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
	virtual ~ReelMagic_MediaPlayerFile() = default;

	virtual const char* GetFileName() const = 0;
	virtual uint32_t GetFileSize() const    = 0;

	virtual uint32_t Read(uint8_t* data, uint32_t amount) = 0;
	// type can be either DOS_SEEK_SET || DOS_SEEK_CUR...
	virtual void Seek(uint32_t pos, uint32_t type) = 0;
};
struct ReelMagic_MediaPlayer {
	virtual ~ReelMagic_MediaPlayer() = default;
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

void REELMAGIC_AddConfigSection(const ConfigPtr& conf);
void REELMAGIC_Init();
void REELMAGIC_Destroy();

void REELMAGIC_NotifyLockMixer();
void REELMAGIC_NotifyUnlockMixer();

#endif /* #ifndef DOSBOX_REELMAGIC_H */
