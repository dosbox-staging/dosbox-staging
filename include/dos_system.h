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

#include <string.h>
#include "dosbox.h"

#define DOS_NAMELENGTH 12
#define DOS_NAMELENGTH_ASCII (DOS_NAMELENGTH+1)
#define DOS_FCBNAME 15
#define DOS_DIRDEPTH 8
#define DOS_PATHLENGTH 80
#define DOS_TEMPSIZE 1024

enum {
	DOS_ATTR_READ_ONLY=	0x01,
	DOS_ATTR_HIDDEN=	0x02,
	DOS_ATTR_SYSTEM=	0x04,
	DOS_ATTR_VOLUME=	0x08,
	DOS_ATTR_DIRECTORY=	0x10,
	DOS_ATTR_ARCHIVE=	0x20
};

struct FileStat_Block {
	Bit32u size;
	Bit16u time;
	Bit16u date;
	Bit16u attr;
};

class DOS_DTA;

class DOS_File {
public:
	DOS_File()		{ name=0; };
	virtual	~DOS_File(){};
	virtual bool	Read(Bit8u * data,Bit16u * size)=0;
	virtual bool	Write(Bit8u * data,Bit16u * size)=0;
	virtual bool	Seek(Bit32u * pos,Bit32u type)=0;
	virtual bool	Close()=0;
	virtual Bit16u	GetInformation(void)=0;
	virtual void	SetName(const char* _name)	{ if (name) delete[] name; name = new char[strlen(_name)+1]; strcpy(name,_name); }
	virtual char*	GetName(void)				{ return name; };
	virtual bool	IsOpen()					{ return open; };
	virtual bool	IsName(const char* _name)	{ if (!name) return false; return strcmp(name,_name)==0; };
	Bit8u type;
	Bit32u flags;
	Bit16u time;
	Bit16u date;
	Bit16u attr;
	Bit32u size;
	bool open;
	char* name;
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
	virtual ~DOS_Drive(){};
	virtual bool FileOpen(DOS_File * * file,char * name,Bit32u flags)=0;
	virtual bool FileCreate(DOS_File * * file,char * name,Bit16u attributes)=0;
	virtual bool FileUnlink(char * _name)=0;
	virtual bool RemoveDir(char * _dir)=0;
	virtual bool MakeDir(char * _dir)=0;
	virtual bool TestDir(char * _dir)=0;
	virtual bool FindFirst(char * _dir,DOS_DTA & dta)=0;
	virtual bool FindNext(DOS_DTA & dta)=0;
	virtual bool GetFileAttr(char * name,Bit16u * attr)=0;
	virtual bool Rename(char * oldname,char * newname)=0;
	virtual bool AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters)=0;
	virtual bool FileExists(const char* name)=0;
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block)=0;
	virtual Bit8u GetMediaByte(void)=0;
	virtual void SetDir(const char* path) { strcpy(curdir,path); };
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
void VFILE_Register(const char * name,Bit8u * data,Bit32u size);
#endif

