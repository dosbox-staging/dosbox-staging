#include "../src/dos/cdrom.h"

/** @name Frames / MSF Conversion Functions
 *  Conversion functions from frames to Minute/Second/Frames and vice versa
 */
/*@{*/
#define FRAMES_TO_MSF(f, M, S, F) \
	{ \
		TMSF msf = frames_to_msf(f); \
		*(F) = msf.fr; \
		*(S) = msf.sec; \
		*(M) = msf.min; \
	}
#define CD_FPS 75
#define MSF_TO_FRAMES(M, S, F) ((M)*60 * CD_FPS + (S)*CD_FPS + (F))
/*@}*/
