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

#ifndef _DRIVES_H__
#define _DRIVES_H__

#include <sys/types.h>
#include <dirent.h>
#include "dos_system.h"

class localDrive : public DOS_Drive {
public:
	localDrive(char * startdir);
	bool FileOpen(DOS_File * * file,char * name,Bit32u flags);
	bool FileCreate(DOS_File * * file,char * name,Bit16u attributes);
	bool FileUnlink(char * name);
	bool RemoveDir(char * dir);
	bool MakeDir(char * dir);
	bool TestDir(char * dir);
	bool FindFirst(char * search,DTA_FindBlock * dta);
	bool FindNext(DTA_FindBlock * dta);
	bool GetFileAttr(char * name,Bit16u * attr);
	bool Rename(char * oldname,char * newname);
	bool FreeSpace(Bit16u * bytes,Bit16u * sectors,Bit16u * clusters,Bit16u * free);
	bool FileExists(const char* name) const ;
	bool FileStat(const char* name, struct stat* const stat_block) const;
private:
	bool FillDTABlock(DTA_FindBlock * dta);
	char basedir[512];
	char directory[512];
	char wild_name[15];
	char * wild_ext;
	DIR *pdir;
};

struct VFILE_Block;


class Virtual_Drive: public DOS_Drive {
public:
	Virtual_Drive();
	bool FileOpen(DOS_File * * file,char * name,Bit32u flags);
	bool FileCreate(DOS_File * * file,char * name,Bit16u attributes);
	bool FileUnlink(char * name);
	bool RemoveDir(char * dir);
	bool MakeDir(char * dir);
	bool TestDir(char * dir);
	bool FindFirst(char * search,DTA_FindBlock * dta);
	bool FindNext(DTA_FindBlock * dta);
	bool GetFileAttr(char * name,Bit16u * attr);
	bool Rename(char * oldname,char * newname);
	bool FreeSpace(Bit16u * bytes,Bit16u * sectors,Bit16u * clusters,Bit16u * free);
    bool FileExists(const char* name) const ;
    bool FileStat(const char* name, struct stat* const stat_block) const;

private:
	VFILE_Block * search_file;
	char search_string[255];
};

#endif
