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

#ifndef DOSBOX_LOGGING_H
#define DOSBOX_LOGGING_H

#include <cstdio>

#include "compiler.h"

#include "loguru.hpp"

enum LOG_TYPES {
	LOG_ALL,
	LOG_VGA, LOG_VGAGFX,LOG_VGAMISC,LOG_INT10,
	LOG_SB,LOG_DMACONTROL,
	LOG_FPU,LOG_CPU,LOG_PAGING,
	LOG_FCB,LOG_FILES,LOG_IOCTL,LOG_EXEC,LOG_DOSMISC,
	LOG_PIT,LOG_KEYBOARD,LOG_PIC,
	LOG_MOUSE,LOG_BIOS,LOG_GUI,LOG_MISC,
	LOG_IO,
	LOG_PCI,
	LOG_MAX
};

enum LOG_SEVERITIES {
	LOG_NORMAL,
	LOG_WARN,
	LOG_ERROR
};

#if C_DEBUG
class LOG 
{ 
	LOG_TYPES       d_type;
	LOG_SEVERITIES  d_severity;
public:

	LOG (LOG_TYPES type , LOG_SEVERITIES severity):
		d_type(type),
		d_severity(severity)
		{}
	void operator() (char const* buf, ...) GCC_ATTRIBUTE(__format__(__printf__, 2, 3));  //../src/debug/debug_gui.cpp

};

void DEBUG_ShowMsg(char const* format,...) GCC_ATTRIBUTE(__format__(__printf__, 1, 2));
#define LOG_MSG DEBUG_ShowMsg

#else  //C_DEBUG

struct LOG
{
	LOG(LOG_TYPES , LOG_SEVERITIES )										{ }
	void operator()(char const* )													{ }
	void operator()(char const* , double )											{ }
	void operator()(char const* , double , double )								{ }
	void operator()(char const* , double , double , double )					{ }
	void operator()(char const* , double , double , double , double )					{ }
	void operator()(char const* , double , double , double , double , double )					{ }
	void operator()(char const* , double , double , double , double , double , double )					{ }
	void operator()(char const* , double , double , double , double , double , double , double)					{ }



	void operator()(char const* , char const* )									{ }
	void operator()(char const* , char const* , double )							{ }
	void operator()(char const* , char const* , double ,double )				{ }
	void operator()(char const* , double , char const* )						{ }
	void operator()(char const* , double , double, char const* )						{ }
	void operator()(char const* , char const*, char const*)				{ }

	void operator()(char const* , double , double , double , char const* )					{ }
	void operator()(char const* , double, char const*, double, double )				{}
}; //add missing operators to here
	//try to avoid anything smaller than bit32...
void GFX_ShowMsg(char const* format,...) GCC_ATTRIBUTE(__format__(__printf__, 1, 2));

// Keep for compatibility
#define LOG_MSG(...)	LOG_F(INFO, __VA_ARGS__)

#endif //C_DEBUG

#define LOG_INFO(...)		LOG_F(INFO, __VA_ARGS__)
#define LOG_WARNING(...)	LOG_F(WARNING, __VA_ARGS__)
#define LOG_ERR(...)		LOG_F(ERROR, __VA_ARGS__)

#ifdef NDEBUG
// DEBUG_LOG_MSG exists only for messages useful during development, and not to
// be redirected into internal DOSBox debugger for DOS programs (C_DEBUG feature).
#define DEBUG_LOG_MSG(...)
#else
#define DEBUG_LOG_MSG(...) DLOG_F(INFO, __VA_ARGS__)
#endif // NDEBUG

#endif
