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


#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "dosbox.h"
#include "support.h"
#include "video.h"

/* 
	Ripped some source from freedos for this one.

*/


/*
 * replaces all instances of character o with character c
 */


void strreplace(char * str,char o,char n) {
	while (*str) {
		if (*str==o) *str=n;
		str++;
	}
}
/*
 * Name: ltrim() - left trims a string by removing leading spaces
 * Input: str - a pointer to a string
 * Output: returns a trimmed copy of str
 */
char *ltrim(char *str) { 
	char c;
	assert(str);

	while ((c = *str++) != '\0' && isspace(c));
	return str - 1;
}

/*
 * Name: rtrim() - right trims a string by removing trailing spaces
 * Input: str - a pointer to a string
 * Output: str will have all spaces removed from the right.
 */
void rtrim(char * const str) {
	char *p;

	assert(str);

	p = strchr(str, '\0');
	while (--p >= str && isspace(*p));
	p[1] = '\0';
}

/*
 *  Combines ltrim() & rtrim()
 */
char *trim(char *str) {
	assert(str);
	rtrim(str);
	return ltrim(str);
}


bool ScanCMDBool(char * cmd,char * check) {
	char * scan=cmd;size_t c_len=strlen(check);
	while (scan=strchr(scan,'/')) {
		/* found a / now see behind it */
		scan++;
		if (strncasecmp(scan,check,c_len)==0 && (scan[c_len]==' ' || scan[c_len]==0)) {
		/* Found a math now remove it from the string */
			memmove(scan-1,scan+c_len,strlen(scan+c_len)+1);
			trim(scan-1);
			return true;
		}
	}
	return false;
}


bool ScanCMDHex(char * cmd,char * check,Bits * result) {
	char * scan=cmd;size_t c_len=strlen(check);
	while (scan=strchr(scan,'/')) {
		/* found a / now see behind it */
		scan++;
		if (strncasecmp(scan,check,c_len)==0 && (scan[c_len]==' ' || scan[c_len]==0)) {
			/* Found a match now find the number and remove it from the string */
			char * begin=scan-1;
			scan=ltrim(scan+c_len);
			bool res=true;
			*result=-1;
			if (!sscanf(scan,"%X",result)) res=false;
			scan=strrchr(scan,'/');
			if (scan) memmove(begin,scan,strlen(scan)+1);
			else *begin=0;
			trim(begin);
			return res;
		}
	}
	return false;

}

/* This scans the command line for a remaining switch and reports it else returns 0*/
char * ScanCMDRemain(char * cmd) {
	char * scan,*found;;
	if (scan=found=strchr(cmd,'/')) {
		while (*scan!=' ' && *scan!=0) scan++;
		*scan=0;
		return found;
	} else return 0; 
}

char * StripWord(char * cmd) {
	bool quoted=false;
	char * begin=cmd;
	if (*cmd=='"') {
		quoted=true;
		cmd++;
	}
	char * end;
	if (quoted) {
		end=strchr(cmd,'"');	
	} else {
		end=strchr(cmd,' ');	
	}
	if (!end) {
		return cmd+strlen(cmd);
	}
	*end=0;
	if (quoted) {
		memmove(begin,cmd,end-begin+1);
	}
	return trim(cmd+strlen(begin)+1);
}

void GFX_ShowMsg(char * msg);
void DEBUG_ShowMsg(char * msg);

void S_Warn(char * format,...) {
	char buf[1024];
	va_list msg;
	
	va_start(msg,format);
	vsprintf(buf,format,msg);
	va_end(msg);
#if C_DEBUG
	DEBUG_ShowMsg(buf);
#else
	GFX_ShowMsg(buf);
#endif
}

static char buf[1024];           //greater scope as else it doesn't always gets thrown right (linux/gcc2.95)
void E_Exit(char * format,...) {
	if(errorlevel>=1) {
		va_list msg;
		va_start(msg,format);
		vsprintf(buf,format,msg);
		va_end(msg);
		
		strcat(buf,"\n");
	} else  {
		strcpy(buf,"an unsupported feature\n");
	}
	throw(buf);
}
