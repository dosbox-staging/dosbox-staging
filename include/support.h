/*
 *  Copyright (C) 2002-2004  The DOSBox Team
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

#if !defined __SUPPORT_H
#define __SUPPORT_H

#include <string.h>
#include <ctype.h>

#include "dosbox.h"

#if defined (_MSC_VER)						/* MS Visual C++ */
#define	strcasecmp(a,b) stricmp(a,b)
#define strncasecmp(a,b,n) _strnicmp(a,b,n)
//		if (stricmp(name,devices[index]->name)==0) return index;
#else
//if (strcasecmp(name,devices[index]->name)==0) return index;
//#define	nocasestrcmp(a,b) stricmp(a,b)
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

void strreplace(char * str,char o,char n);
char *ltrim(const char *str);
char *rtrim(const char *str);
char *trim(const char *str);

bool ScanCMDBool(char * cmd,char * check);
char * ScanCMDRemain(char * cmd);
char * StripWord(char *&cmd);
bool IsDecWord(char * word);
bool IsHexWord(char * word);
Bits ConvDecWord(char * word);
Bits ConvHexWord(char * word);

INLINE char * upcase(char * str) {
    for (char* idx = str; *idx ; idx++) *idx = toupper(*idx);
    return str;
}

INLINE char * lowcase(char * str) {
	for(char* idx = str; *idx ; idx++)  *idx = tolower(*idx);
	return str;
}


#endif

