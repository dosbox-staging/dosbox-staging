/*
 *  Copyright (C) 2002  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _SETTINGS_H_
#define _SETTINSG_H_

/* Enable the debugger, this only seems to work in win32 for now. */
#define C_DEBUG 0

/* Enable the logging of extra information for debugging to the console */
#define C_LOGGING 0

/* Use multi threading to speed up things on multi cpu's, also gives a nice frame-skipping effect :) */
#define C_THREADED 1

/* Enable some big compile-time increasing inlines, great for speed though */
#define C_EXTRAINLINE 0

/* Enable the FPU module, still only for beta testing */
#define C_FPU 0

/* Maximum memory range in megabytes */
#define C_MEM_MAX_SIZE 12

/* Maximum memory available for xms, should be lower than maxsize-1 */
#define C_MEM_XMS_SIZE 8

/* Maximum memory available for ems, should be lower than 32 */
#define C_MEM_EMS_SIZE 4

/* Enable debug messages for several modules, requires C_LOGGING */
#define DEBUG_SBLASTER 0	/* SoundBlaster Debugging*/
#define DEBUG_DMA 0			/* DMA Debugging */
#define DEBUG_DOS 0			/* DOS Debugging */

#define LOG_MSG S_Warn

#if C_LOGGING
#define LOG_DEBUG S_Warn
#define LOG_WARN S_Warn
#define LOG_ERROR S_Warn
#else
#define LOG_DEBUG
#define LOG_WARN
#define LOG_ERROR
#endif

#endif
