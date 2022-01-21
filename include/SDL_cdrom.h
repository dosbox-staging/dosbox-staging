#ifndef _SDL_cdrom_h
#define _SDL_cdrom_h

#include "SDL_config.h"

/** The maximum number of CD-ROM tracks on a disk */
#define SDL_MAX_TRACKS	99

#define SDL_AUDIO_TRACK	0x00
#define SDL_DATA_TRACK	0x04

/** Given a status, returns true if there's a disk in the drive */
#define CD_INDRIVE(status)	((int)(status) > 0)

/** The possible states which a CD-ROM drive can be in. */
typedef enum {
	CD_TRAYEMPTY,
	CD_STOPPED,
	CD_PLAYING,
	CD_PAUSED,
	CD_ERROR = -1
} CDstatus;

typedef struct SDL_CDtrack {
	Uint8 id;		/**< Track number */
	Uint8 type;		/**< Data or audio track */
	Uint16 unused;
	Uint32 length;		/**< Length, in frames, of this track */
	Uint32 offset;		/**< Offset, in frames, from start of disk */
} SDL_CDtrack;

/** This structure is only current as of the last call to SDL_CDStatus() */
typedef struct SDL_CD {
	int id;			/**< Private drive identifier */
	CDstatus status;	/**< Current drive status */

	/** The rest of this structure is only valid if there's a CD in drive */
        /*@{*/
	int numtracks;		/**< Number of tracks on disk */
	int cur_track;		/**< Current track position */
	int cur_frame;		/**< Current frame offset within current track */
	SDL_CDtrack track[SDL_MAX_TRACKS+1];
        /*@}*/
} SDL_CD;

const char *SDL_CDName(int drive);
extern int SDL_CDNumDrives(void);
extern int SDL_CDROMInit(void);
extern void SDL_CDROMQuit(void);

#endif