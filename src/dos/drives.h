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
 

bool WildFileCmp(const char * file, const char * wild);

class DOS_Drive_Cache {
public:
	DOS_Drive_Cache					(void);
	DOS_Drive_Cache					(const char* path);
	~DOS_Drive_Cache				(void);

	typedef enum TDirSort { NOSORT, ALPHABETICAL, DIRALPHABETICAL, ALPHABETICALREV, DIRALPHABETICALREV };

	void		SetBaseDir			(const char* path);
	void		SetDirSort			(TDirSort sort) { sortDirType = sort; };
	bool		OpenDir				(const char* path);
	bool		ReadDir				(struct dirent* &result);

	void		ExpandName			(char* path);
	char*		GetExpandName		(const char* path);
	
	void		CacheOut			(const char* path, bool ignoreLastDir = false);
	void		AddEntry			(const char* path);

	class CFileInfo {
	public:	
		~CFileInfo(void) {
			for (Bit32u i=0; i<fileList.size(); i++) delete fileList[i];
			fileList.clear();
			longNameList.clear();
			outputList.clear();
		};
		char		fullname	[CROSS_LEN];
		char		orgname		[CROSS_LEN];
		char		shortname	[DOS_NAMELENGTH_ASCII];
		bool		isDir;
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
	bool		OpenDir				(CFileInfo* dir, char* path);
	void		CreateEntry			(CFileInfo* dir, const char* name);

	Bit32u		nextEntry;	
	CFileInfo*	dirBase;
	CFileInfo*  dirSearch;
	char		dirPath				[CROSS_LEN];
	bool		dirFirstTime;
	TDirSort	sortDirType;
	CFileInfo*	save_dir;
	char		save_path			[CROSS_LEN];
	char		save_expanded		[CROSS_LEN];

};

class DOS_No_Drive_Cache {
public:
	DOS_No_Drive_Cache				(void) {};
	DOS_No_Drive_Cache				(const char* path);
	~DOS_No_Drive_Cache				(void) {};

	typedef enum TDirSort { NOSORT, ALPHABETICAL, DIRALPHABETICAL, ALPHABETICALREV, DIRALPHABETICALREV };

	void		SetBaseDir			(const char* path);
	void		SetDirSort			(TDirSort sort) {};
	bool		OpenDir				(const char* path);
	bool		ReadDir				(struct dirent* &result);

	void		ExpandName			(char* path) {};
	char*		GetExpandName		(const char* path) { return (char*)path; };
	
	void		CacheOut			(const char* path, bool ignoreLastDir = false) {};
	void		AddEntry			(const char* path) {};
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
	DOS_Drive_Cache	dirCache;
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
