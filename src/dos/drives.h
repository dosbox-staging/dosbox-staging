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
#include <vector>
#include "dos_system.h"
#include "cross.h"

#define MAX_OPENDIRS 16 

bool WildFileCmp(const char * file, const char * wild);

class DOS_Drive_Cache {
public:
	DOS_Drive_Cache					(void);
	DOS_Drive_Cache					(const char* path);
	~DOS_Drive_Cache				(void);

	typedef enum TDirSort { NOSORT, ALPHABETICAL, DIRALPHABETICAL, ALPHABETICALREV, DIRALPHABETICALREV };

	void		SetBaseDir			(const char* path);
	void		SetDirSort			(TDirSort sort) { sortDirType = sort; };
	bool		OpenDir				(const char* path, Bit16u& id);
	bool		ReadDir				(Bit16u id, struct dirent* &result);

	void		ExpandName			(char* path);
	char*		GetExpandName		(const char* path);
	bool		GetShortName		(const char* fullname, char* shortname);
	
	void		CacheOut			(const char* path, bool ignoreLastDir = false);
	void		AddEntry			(const char* path, bool checkExist = false);
	void		DeleteEntry			(const char* path, bool ignoreLastDir = false);

	class CFileInfo {
	public:	
		~CFileInfo(void) {
			for (Bit32u i=0; i<fileList.size(); i++) delete fileList[i];
			fileList.clear();
			longNameList.clear();
			outputList.clear();
		};
		char		orgname		[CROSS_LEN];
		char		shortname	[DOS_NAMELENGTH_ASCII];
		bool		isDir;
		Bit16u		nextEntry;
		Bit16u		shortNr;
		Bit16u		compareCount;
		// contents
		std::vector<CFileInfo*>	fileList;
		std::vector<CFileInfo*>	longNameList;
		std::vector<CFileInfo*>	outputList;
	};

private:

	Bit16s		GetLongName			(CFileInfo* info, char* shortname);
	void		CreateShortName		(CFileInfo* dir, CFileInfo* info);
	Bit16u		CreateShortNameID	(CFileInfo* dir, const char* name);
	bool		SetResult			(CFileInfo* dir, struct dirent* &result, Bit16u entryNr);
	bool		IsCachedIn			(CFileInfo* dir);
	CFileInfo*	FindDirInfo			(const char* path, char* expandedPath);
	bool		RemoveSpaces		(char* str);
	bool		OpenDir				(CFileInfo* dir, char* path, Bit16u& id);
	void		CreateEntry			(CFileInfo* dir, const char* name);
	Bit16u		GetFreeID			(CFileInfo* dir);


	CFileInfo*	dirBase;
	char		dirPath				[CROSS_LEN];
	char		basePath			[CROSS_LEN];
	bool		dirFirstTime;
	TDirSort	sortDirType;
	CFileInfo*	save_dir;
	char		save_path			[CROSS_LEN];
	char		save_expanded		[CROSS_LEN];

	Bit16u		srchNr;
	CFileInfo*	dirSearch			[MAX_OPENDIRS];
	char		dirSearchName		[MAX_OPENDIRS];
	bool		free				[MAX_OPENDIRS];

};

class DOS_No_Drive_Cache {
public:
	DOS_No_Drive_Cache				(void) {};
	DOS_No_Drive_Cache				(const char* path);
	~DOS_No_Drive_Cache				(void) {};

	typedef enum TDirSort { NOSORT, ALPHABETICAL, DIRALPHABETICAL, ALPHABETICALREV, DIRALPHABETICALREV };

	void		SetBaseDir			(const char* path);
	void		SetDirSort			(TDirSort sort) {};
	bool		OpenDir				(const char* path, Bit16u& id);
	bool		ReadDir				(Bit16u id, struct dirent* &result);

	void		ExpandName			(char* path) {};
	char*		GetExpandName		(const char* path) { return (char*)path; };
	bool		GetShortName		(const char* fullname, char* shortname) { return false; };
	
	void		CacheOut			(const char* path, bool ignoreLastDir = false) {};
	void		AddEntry			(const char* path, bool checkExists = false) {};
	void		DeleteEntry			(const char* path, bool ignoreLastDir = false) {};

	void		SetCurrentEntry		(Bit16u entry) {};
	Bit16u		GetCurrentEntry		(void) { return 0; };

public:
	char		basePath			[CROSS_LEN];
	char		dirPath				[CROSS_LEN];
	DIR*		srch_opendir;
};

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
	bool GetShortName(const char* fullname, char* shortname) { return dirCache.GetShortName(fullname, shortname); };
private:
	char basedir[CROSS_LEN];
	
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
	DOS_Drive_Cache dirCache;
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
