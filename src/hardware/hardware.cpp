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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
	This could do with a serious revision :)
*/

#include <dirent.h>
#include <string.h>
#include "dosbox.h"
#include "hardware.h"
#include "setup.h"
#include "support.h"

static char * capturedir;
extern char * RunningProgram;

FILE * OpenCaptureFile(const char * type,const char * ext) {
	Bitu last=0;
	char file_name[CROSS_LEN];
	char file_start[16];
	DIR * dir;struct dirent * dir_ent;
	/* Find a filename to open */
	dir=opendir(capturedir);
	if (!dir) {
		LOG_MSG("Can't open dir %s for capturing %s",capturedir,type);
		return 0;
	}
	strcpy(file_start,RunningProgram);
	strcat(file_start,"_");
	while ((dir_ent=readdir(dir))) {
		char tempname[CROSS_LEN];
		strcpy(tempname,dir_ent->d_name);
		char * test=strstr(tempname,ext);
		if (!test || strlen(test)!=strlen(ext)) continue;
		*test=0;
		if (strncasecmp(tempname,file_start,strlen(file_start))!=0) continue;
		Bitu num=atoi(&tempname[strlen(file_start)]);
		if (num>=last) last=num+1;
	}
	closedir(dir);
	sprintf(file_name,"%s%c%s%03d%s",capturedir,CROSS_FILESPLIT,file_start,last,ext);
	lowcase(file_name);
	/* Open the actual file */
	FILE * handle=fopen(file_name,"wb");
	if (handle) {
		LOG_MSG("Capturing %s to %s",type,file_name);
	} else {
		LOG_MSG("Failed to open %s for capturing %s",file_name,type);
	}
	return handle;
}


void HARDWARE_Init(Section * sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	capturedir=(char *)section->Get_string("captures");
}

