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

/* $Id: drives.h,v 1.21 2004-04-18 14:49:50 qbix79 Exp $ */

#ifndef _DRIVES_H__
#define _DRIVES_H__

#include <sys/types.h>
#include "dos_system.h"
#include "shell.h" /* for DOS_Shell */
#include "bios.h"  /* for fatDrive */

bool WildFileCmp(const char * file, const char * wild);

class localDrive : public DOS_Drive {
public:
	localDrive(const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid);
	virtual bool FileOpen(DOS_File * * file,char * name,Bit32u flags);
	virtual FILE *GetSystemFilePtr(char * name, char * type);
	virtual bool FileCreate(DOS_File * * file,char * name,Bit16u attributes);
	virtual bool FileUnlink(char * name);
	virtual bool RemoveDir(char * dir);
	virtual bool MakeDir(char * dir);
	virtual bool TestDir(char * dir);
	virtual bool FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual bool FindNext(DOS_DTA & dta);
	virtual bool GetFileAttr(char * name,Bit16u * attr);
	virtual bool Rename(char * oldname,char * newname);
	virtual bool AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters);
	virtual bool FileExists(const char* name);
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block);
	virtual Bit8u GetMediaByte(void);
	virtual bool isRemote(void);
private:
	char basedir[CROSS_LEN];
	friend void DOS_Shell::CMD_SUBST(char* args); 	
	struct {
		char srch_dir[CROSS_LEN];
	} srchInfo[MAX_OPENDIRS];

	struct {
		Bit16u bytes_sector;
		Bit8u sectors_cluster;
		Bit16u total_clusters;
		Bit16u free_clusters;
		Bit8u mediaid;
	} allocation;
};

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct bootstrap {
	Bit8u  nearjmp[3];
	Bit8u  oemname[8];
	Bit16u bytespersector;
	Bit8u  sectorspercluster;
	Bit16u reservedsectors;
	Bit8u  fatcopies;
	Bit16u rootdirentries;
	Bit16u totalsectorcount;
	Bit8u  mediadescriptor;
	Bit16u sectorsperfat;
	Bit16u sectorspertrack;
	Bit16u headcount;
	/* 32-bit FAT extensions */
	Bit32u hiddensectorcount;
	Bit32u totalsecdword;
	Bit8u  bootcode[474];
	Bit8u  magic1; /* 0x55 */
	Bit8u  magic2; /* 0xaa */
} GCC_ATTRIBUTE(packed);

struct direntry {
	Bit8u entryname[11];
	Bit8u attrib;
	Bit8u NTRes;
	Bit8u milliSecondStamp;
	Bit16u crtTime;
	Bit16u crtDate;
	Bit16u accessDate;
	Bit16u hiFirstClust;
	Bit16u modTime;
	Bit16u modDate;
	Bit16u loFirstClust;
	Bit32u entrysize;
} GCC_ATTRIBUTE(packed);

struct partTable {
	Bit8u booter[446];
	struct {
		Bit8u bootflag;
		Bit8u beginchs[3];
		Bit8u parttype;
		Bit8u endchs[3];
		Bit32u absSectStart;
		Bit32u partSize;
	} pentry[4];
	Bit8u  magic1; /* 0x55 */
	Bit8u  magic2; /* 0xaa */
} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack ()
#endif

class fatDrive : public DOS_Drive {
public:
	fatDrive(const char * sysFilename, Bit32u bytesector, Bit32u cylsector, Bit32u headscyl, Bit32u cylinders, Bit32u startSector);
	virtual bool FileOpen(DOS_File * * file,char * name,Bit32u flags);
	virtual bool FileCreate(DOS_File * * file,char * name,Bit16u attributes);
	virtual bool FileUnlink(char * name);
	virtual bool RemoveDir(char * dir);
	virtual bool MakeDir(char * dir);
	virtual bool TestDir(char * dir);
	virtual bool FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual bool FindNext(DOS_DTA & dta);
	virtual bool GetFileAttr(char * name,Bit16u * attr);
	virtual bool Rename(char * oldname,char * newname);
	virtual bool AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters);
	virtual bool FileExists(const char* name);
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block);
	virtual Bit8u GetMediaByte(void);
	virtual bool isRemote(void);
public:
	Bit32u getAbsoluteSectFromBytePos(Bit32u startClustNum, Bit32u bytePos);
	Bit32u getSectorSize(void);
	Bit32u getAbsoluteSectFromChain(Bit32u startClustNum, Bit32u logicalSector);
	bool allocateCluster(Bit32u useCluster, Bit32u prevCluster);
	Bit32u appendCluster(Bit32u startCluster);
	void deleteClustChain(Bit32u startCluster);
	Bit32u getFirstFreeClust(void);
	bool directoryBrowse(Bit32u dirClustNumber, direntry *useEntry, Bit32s entNum);
	bool directoryChange(Bit32u dirClustNumber, direntry *useEntry, Bit32s entNum);
	imageDisk *loadedDisk;
private:
	Bit32u getClusterValue(Bit32u clustNum);
	void setClusterValue(Bit32u clustNum, Bit32u clustValue);
	Bit32u getClustFirstSect(Bit32u clustNum);
	bool FindNextInternal(Bit32u dirClustNumber, DOS_DTA & dta, direntry *foundEntry);
	bool getDirClustNum(char * dir, Bit32u * clustNum, bool parDir);
	bool getFileDirEntry(char * filename, direntry * useEntry, Bit32u * dirClust, Bit32u * subEntry);
	bool addDirectoryEntry(Bit32u dirClustNumber, direntry useEntry);
	void zeroOutCluster(Bit32u clustNumber);
	bool getEntryName(char *fullname, char *entname);
	friend void DOS_Shell::CMD_SUBST(char* args); 	
	struct {
		char srch_dir[CROSS_LEN];
	} srchInfo[MAX_OPENDIRS];

	struct {
		Bit16u bytes_sector;
		Bit8u sectors_cluster;
		Bit16u total_clusters;
		Bit16u free_clusters;
		Bit8u mediaid;
	} allocation;
	
	bootstrap bootbuffer;
	Bit8u fattype;
	Bit32u CountOfClusters;
	Bit32u partSectOff;
	Bit32u firstDataSector;
	Bit32u firstRootDirSect;

	Bit32u cwdDirCluster;
	Bit32u dirPosition; /* Position in directory search */
};


class cdromDrive : public localDrive
{
public:
	cdromDrive(const char driveLetter, const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid, int& error);
	virtual bool FileOpen(DOS_File * * file,char * name,Bit32u flags);
	virtual bool FileCreate(DOS_File * * file,char * name,Bit16u attributes);
	virtual bool FileUnlink(char * name);
	virtual bool RemoveDir(char * dir);
	virtual bool MakeDir(char * dir);
	virtual bool Rename(char * oldname,char * newname);
	virtual bool GetFileAttr(char * name,Bit16u * attr);
	virtual bool FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual void SetDir(const char* path);
	virtual bool isRemote(void);
private:
	Bit8u subUnit;
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
	bool FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst);
	bool FindNext(DOS_DTA & dta);
	bool GetFileAttr(char * name,Bit16u * attr);
	bool Rename(char * oldname,char * newname);
	bool AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters);
    bool FileExists(const char* name);
    bool FileStat(const char* name, FileStat_Block* const stat_block);
	Bit8u GetMediaByte(void);
	void EmptyCache(void){}
	bool isRemote(void);
private:
	VFILE_Block * search_file;
};



#endif
