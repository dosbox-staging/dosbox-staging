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

#ifndef DOSSYSTEM_H_
#define DOSSYSTEM_H_

#include <dosbox.h>

#define DOS_NAMELENGTH 12
#define DOS_DIRDEPTH 16
#define DOS_PATHLENGTH (DOS_DIRDEPTH+1)*(DOS_NAMELENGTH+2)
#define DOS_TEMPSIZE 1024

enum {
	DOS_ATTR_READ_ONLY=	0x01,
	DOS_ATTR_HIDDEN=	0x02,
	DOS_ATTR_SYSTEM=	0x04,
	DOS_ATTR_VOLUME=	0x08,
	DOS_ATTR_DIRECTORY=	0x10,
	DOS_ATTR_ARCHIVE=	0x20
};

#pragma pack (1)
struct DTA_FindBlock {
	Bit8u sdrive;								/* The Drive the search is taking place */
	Bit16u sattr;								/* The attributes that need to be found */
	Bit8u fill[18];
	Bit8u attr;
	Bit16u time;
	Bit16u date;
	Bit32u size;
	char name[DOS_NAMELENGTH];
};
#pragma pack ()

class DOS_File {
public:
	virtual bool Read(Bit8u * data,Bit16u * size)=0;
	virtual bool Write(Bit8u * data,Bit16u * size)=0;
	virtual bool Seek(Bit32u * pos,Bit32u type)=0;
	virtual bool Close()=0;
	virtual Bit16u GetInformation(void)=0;
	Bit8u type;Bit32u flags;
/* Some Device Specific Stuff */
};

class DOS_Device : public DOS_File {
public:
/* Some Device Specific Stuff */
	char * name;
	Bit8u fhandle;	
};



class DOS_Drive {
public:
	DOS_Drive();
	virtual bool FileOpen(DOS_File * * file,char * name,Bit32u flags)=0;
	virtual bool FileCreate(DOS_File * * file,char * name,Bit16u attributes)=0;
	virtual bool FileUnlink(char * name)=0;
	virtual bool RemoveDir(char * dir)=0;
	virtual bool MakeDir(char * dir)=0;
	virtual bool TestDir(char * dir)=0;
	virtual bool FindFirst(char * search,DTA_FindBlock * dta)=0;
	virtual bool FindNext(DTA_FindBlock * dta)=0;
	virtual bool GetFileAttr(char * name,Bit16u * attr)=0;
	virtual bool Rename(char * oldname,char * newname)=0;
	virtual bool FreeSpace(Bit16u * bytes,Bit16u * sectors,Bit16u * clusters,Bit16u * free)=0;
	virtual bool FileExists(const char* name) const=0;
	virtual bool FileStat(const char* name, struct stat* const stat_block) const=0;
	char * GetInfo(void);
	char curdir[DOS_PATHLENGTH];
	char info[256];
};

enum { OPEN_READ=0,OPEN_WRITE=1,OPEN_READWRITE=2 };
enum { DOS_SEEK_SET=0,DOS_SEEK_CUR=1,DOS_SEEK_END=2};


/*
 A multiplex handler should read the registers to check what function is being called 
 If the handler returns false dos will stop checking other handlers 
*/

typedef bool (MultiplexHandler)(void);
void DOS_AddMultiplexHandler(MultiplexHandler * handler);

void DOS_AddDevice(DOS_Device * adddev);
void VFILE_Register(char * name,Bit8u * data,Bit32u size);
#endif

