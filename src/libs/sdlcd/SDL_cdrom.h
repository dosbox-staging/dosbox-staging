#ifndef _SDL_cdrom_h
#define _SDL_cdrom_h

#include "SDL_config.h"
#include <stdio.h>
#include <stdint.h>

/** The maximum number of CD-ROM tracks on a disk */
#define SDL_MAX_TRACKS 99
#define SDL_AUDIO_TRACK 0x00
#define SDL_DATA_TRACK 0x04

#ifndef Uint8
#define Uint8 uint8_t
#endif
#ifndef Uint16
#define Uint16 uint16_t
#endif
#ifndef Uint32
#define Uint32 uint32_t
#endif

/** Given a status, returns true if there's a disk in the drive */
#define CD_INDRIVE(status) ((int)(status) > 0)

/** @name Frames / MSF Conversion Functions
 *  Conversion functions from frames to Minute/Second/Frames and vice versa
 */
/*@{*/
#define CD_FPS 75
#define FRAMES_TO_MSF(f, M,S,F)	{	\
	unsigned int value = f;			\
	*(F) = value%CD_FPS;			\
	value /= CD_FPS;				\
	*(S) = value%60u;				\
	value /= 60u;					\
	*(M) = value;					\
}
#define MSF_TO_FRAMES(M, S, F) ((M)*60 * CD_FPS + (S)*CD_FPS + (F))
/*@}*/

/** The possible states which a CD-ROM drive can be in. */
typedef enum {
	CD_TRAYEMPTY,
	CD_STOPPED,
	CD_PLAYING,
	CD_PAUSED,
	CD_ERROR = -1
} CDstatus;

typedef struct SDL_CDtrack {
	Uint8 id;   /**< Track number */
	Uint8 type; /**< Data or audio track */
	Uint16 unused;
	Uint32 length; /**< Length, in frames, of this track */
	Uint32 offset; /**< Offset, in frames, from start of disk */
} SDL_CDtrack;

/** This structure is only current as of the last call to SDL_CDStatus() */
typedef struct SDL_CD {
	int id;          /**< Private drive identifier */
	CDstatus status; /**< Current drive status */

	/** The rest of this structure is only valid if there's a CD in drive */
	/*@{*/
	int numtracks; /**< Number of tracks on disk */
	int cur_track; /**< Current track position */
	int cur_frame; /**< Current frame offset within current track */
	SDL_CDtrack track[SDL_MAX_TRACKS + 1];
	/*@}*/
} SDL_CD;

#endif
