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
#include "cross.h"
 

bool WildFileCmp(const char * file, const char * wild);

class localDrive : public DOS_Drive {
public:
	localDrive(const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid);
	bool FileOpen(DOS_File * * file,char * name,Bit32u flags);
	bool FileCreate(DOS_File * * file,char * name,Bit16u attributes);
	bool FileUnlink(char * name);
	bool RemoveDir(char * dir);
	bool MakeDir(char * dir);
	bool TestDir(char * dir);
	bool FindFirst(char * _dir,DOS_DTA & dta);
	bool FindNext(DOS_DTA & dta);
	bool GetFileAttr(char * name,Bit16u * attr);
	bool Rename(char * oldname,char * newname);
	bool AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters);
	bool FileExists(const char* name);
	bool FileStat(const char* name, FileStat_Block * const stat_block);
	Bit8u GetMediaByte(void);
private:
	char basedir[CROSS_LEN];
	char srch_dir[CROSS_LEN];
	DIR * srch_opendir;
	struct {
		Bit16u bytes_sector;
		Bit8u sectors_cluster;
		Bit16u total_clusters;
		Bit16u free_clusters;
		Bit8u mediaid;
	} allocation;
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
	bool FindFirst(char * _dir,DOS_DTA & dta);
	bool FindNext(DOS_DTA & dta);
	bool GetFileAttr(char * name,Bit16u * attr);
	bool Rename(char * oldname,char * newname);
	bool AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters);
    bool FileExists(const char* name);
    bool FileStat(const char* name, FileStat_Block* const stat_block);
	Bit8u GetMediaByte(void);
private:
	VFILE_Block * search_file;
};

#endif
