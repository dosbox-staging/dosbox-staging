
/*
 *  Copyright (C) 2002-2003  The DOSBox Team
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

#include "drives.h"
#include "dos_inc.h"
#include "dirent.h"
#include "support.h"

// STL stuff
#include <vector>
#include <iterator>
#include <algorithm>


bool SortByName(DOS_Drive_Cache::CFileInfo* const &a, DOS_Drive_Cache::CFileInfo* const &b) 
{
	return strcmp(a->shortname,b->shortname)<0;
};

bool SortByNameRev(DOS_Drive_Cache::CFileInfo* const &a, DOS_Drive_Cache::CFileInfo* const &b) 
{
	return strcmp(a->shortname,b->shortname)>0;
};

bool SortByDirName(DOS_Drive_Cache::CFileInfo* const &a, DOS_Drive_Cache::CFileInfo* const &b) 
{
	// Directories first...
	if (a->isDir!=b->isDir) return (a->isDir>b->isDir);	
	return strcmp(a->shortname,b->shortname)<0;
};

bool SortByDirNameRev(DOS_Drive_Cache::CFileInfo* const &a, DOS_Drive_Cache::CFileInfo* const &b) 
{
	// Directories first...
	if (a->isDir!=b->isDir) return (a->isDir>b->isDir);	
	return strcmp(a->shortname,b->shortname)>0;
};

DOS_Drive_Cache::DOS_Drive_Cache(void)
{
	dirBase		= new CFileInfo;
	dirSearch	= 0;
	save_dir	= 0;
	SetDirSort(DIRALPHABETICAL);
};

DOS_Drive_Cache::DOS_Drive_Cache(const char* path)
{
	dirBase		= new CFileInfo;
	dirSearch	= 0;
	save_dir	= 0;
	SetDirSort(DIRALPHABETICAL);
	SetBaseDir(path);
};

DOS_Drive_Cache::~DOS_Drive_Cache(void)
{
	delete dirBase; dirBase = 0;
};

void DOS_Drive_Cache::SetBaseDir(const char* baseDir)
{
	strcpy(dirBase->fullname,baseDir);
	if (OpenDir(baseDir)) {
		struct dirent result;
		ReadDir(&result);
	};
};

void DOS_Drive_Cache::ExpandName(char* path)
{
	strcpy(path,GetExpandName(path));
};

char* DOS_Drive_Cache::GetExpandName(const char* path)
{
	static char work [CROSS_LEN];
	char dir [CROSS_LEN]; 

	work[0] = 0;
	strcpy (dir,path);

	char* pos = strrchr(path,CROSS_FILESPLIT);

	if (pos) dir[pos-path+1] = 0;
	CFileInfo* dirInfo = FindDirInfo(dir, work);
		
	if (pos) {
		// Last Entry = File
		strcpy(dir,pos+1); 
		GetLongName(dirInfo, dir);
		strcat(work,dir);
	}
	return work;
};

void DOS_Drive_Cache::AddEntry(const char* path)
{
	// Get Last part...
	char file	[CROSS_LEN];
	char expand	[CROSS_LEN];

	CFileInfo* dir = FindDirInfo(path,expand);
	char* pos = strrchr(path,CROSS_FILESPLIT);

	if (pos) {
		strcpy(file,pos+1);	
		
		CreateEntry(dir,file);
		// Sort Lists - filelist has to be alphabetically sorted
		std::sort(dir->fileList.begin(), dir->fileList.end(), SortByName);
		// Output list - user defined
		switch (sortDirType) {
			case ALPHABETICAL		: std::sort(dir->outputList.begin(), dir->outputList.end(), SortByName);		break;
			case DIRALPHABETICAL	: std::sort(dir->outputList.begin(), dir->outputList.end(), SortByDirName);		break;
			case ALPHABETICALREV	: std::sort(dir->outputList.begin(), dir->outputList.end(), SortByNameRev);		break;
			case DIRALPHABETICALREV	: std::sort(dir->outputList.begin(), dir->outputList.end(), SortByDirNameRev);	break;
		};
//		LOG_DEBUG("DIR: Added Entry %s",path);
	} else {
//		LOG_DEBUG("DIR: Error: Failed to add %s",path);	
	};
};

void DOS_Drive_Cache::CacheOut(const char* path, bool ignoreLastDir)
{
	char expand[CROSS_LEN] = { 0 };
	CFileInfo* dir;
	
	if (ignoreLastDir) {
		char tmp[CROSS_LEN] = { 0 };
		Bit32s len = strrchr(path,CROSS_FILESPLIT) - path;
		if (len>0) { 
			strncpy(tmp,path,len); 
			tmp[len] = 0; 
		} else	{
			strcpy(tmp,path);
		}
		dir = FindDirInfo(tmp,expand);
	} else {
		dir = FindDirInfo(path,expand);	
	}
//	LOG_DEBUG("DIR: Caching out %s : dir %s",expand,dir->fullname);
	// delete file objects...
	for(Bit32u i=0; i<dir->fileList.size(); i++) delete dir->fileList[i];
	// clear lists
	dir->fileList.clear();
	dir->longNameList.clear();
	dir->outputList.clear();
	dir->shortNr = 0;
	save_dir = 0;
	nextEntry = 0;
};

bool DOS_Drive_Cache::IsCachedIn(CFileInfo* curDir)
{
	return (curDir->fileList.size()>0);
};

Bit16u DOS_Drive_Cache::CreateShortNameID(CFileInfo* curDir, const char* name)
{
	Bit16s foundNr	= 0;	
	Bit16s low		= 0;
	Bit16s high		= curDir->longNameList.size()-1;
	Bit16s mid, res;

	CFileInfo* test;

	while (low<=high) {
		mid = (low+high)/2;
		test = curDir->longNameList[mid];
		res = strncmp(name,curDir->longNameList[mid]->shortname,curDir->longNameList[mid]->compareCount);
		if (res>0)	low  = mid+1; else
		if (res<0)	high = mid-1; 
		else {
			// any more same x chars in next entries ?	
			do {	
				foundNr = curDir->longNameList[mid]->shortNr;
				mid++;
			} while(mid<curDir->longNameList.size() && (strncmp(name,curDir->longNameList[mid]->shortname,curDir->longNameList[mid]->compareCount)==0));
			break;
		};
	}
	return foundNr+1;
};

Bit16s DOS_Drive_Cache::GetLongName(CFileInfo* curDir, char* shortName)
{
	// Search long name and return array number of element
	Bit16s low	= 0;
	Bit16s high = curDir->fileList.size()-1;
	Bit16s mid,res;
	while (low<=high) {
		mid = (low+high)/2;
		CFileInfo* test = curDir->fileList[mid];
		res = strcmp(shortName,curDir->fileList[mid]->shortname);
		if (res>0)	low  = mid+1; else
		if (res<0)	high = mid-1; else
		{	// Found
//			strcpy(shortName,curDir->fileList[mid]->fullname);
			strcpy(shortName,curDir->fileList[mid]->orgname);
			return mid;
		};
	}
	// not available
	return -1;
};

bool DOS_Drive_Cache::RemoveSpaces(char* str)
// Removes all spaces
{
	char*	curpos	= str;
	char*	chkpos	= str;
	Bit16s	len		= -1;
	while (*chkpos!=0) { 
		if (*chkpos==' ') chkpos++; else *curpos++ = *chkpos++; 
	}
	*curpos = 0;
	return (curpos!=chkpos);
};

void DOS_Drive_Cache::CreateShortName(CFileInfo* curDir, CFileInfo* info)
{
	Bit16s	len			= 0;
	Bit16s	lenExt		= 0;
	bool	createShort = false;

	char tmpNameBuffer[CROSS_LEN];

	char* tmpName = tmpNameBuffer;

	// Remove Spaces
	strcpy(tmpName,info->fullname);
	createShort = RemoveSpaces(tmpName);

	// Get Length of filename
	char* pos = strchr(tmpName,'.');
	if (pos) {
		// Get Length of extension
		lenExt = strlen(pos)-1;
		// ignore preceding '.' if extension is longer than "3"
		if (lenExt>3) {
			while (*tmpName=='.') tmpName++;
			createShort = true;
		};
		pos = strchr(tmpName,'.');
		if (pos)	len = (Bit16u)(pos - tmpName);
		else		len = strlen(tmpName);

	} else 
		len = strlen(tmpName);

	// Should shortname version be created ?
	createShort = createShort || (len>8);
	if (!createShort) {
		char buffer[CROSS_LEN];
		strcpy(buffer,tmpName);
		createShort = (GetLongName(curDir,buffer)>=0);
	}

	if (createShort) {
		// Create number
		char buffer[8];
		info->shortNr = CreateShortNameID(curDir,tmpName);
		sprintf(buffer,"%d",info->shortNr);
		// Copy first letters
		Bit16u tocopy;
		if (len+strlen(buffer)>8)	tocopy = 8 - strlen(buffer) - 1;
		else						tocopy = len;
		strncpy(info->shortname,tmpName,tocopy);
		info->shortname[tocopy] = 0;
		// Copy number
		strcat(info->shortname,"~");
		strcat(info->shortname,buffer);
		// Create compare Count
		info->compareCount = tocopy;
		// Add (and cut) Extension, if available
		if (pos) {
			// Step to last extension...
			pos = strrchr(tmpName, '.');
			// add extension
			strncat(info->shortname,pos,4);
			info->shortname[DOS_NAMELENGTH] = 0;
		};
		// Put it in longname list...
		curDir->longNameList.push_back(info);
		std::sort(curDir->longNameList.begin(), curDir->longNameList.end(), SortByName);
	} else {
		strcpy(info->shortname,tmpName);
	}
};

DOS_Drive_Cache::CFileInfo* DOS_Drive_Cache::FindDirInfo(const char* path, char* expandedPath)
{
	// statics
	static char	split[2] = { CROSS_FILESPLIT,0 };
	
	char		dir  [CROSS_LEN]; 
	char		work [CROSS_LEN];
	char*		start = (char*)path;
	char*		pos;
	CFileInfo*	curDir = dirBase;

	if (save_dir && (strcmp(path,save_path)==0)) {
		strcpy(expandedPath,save_expanded);
		return save_dir;
	};

//	LOG_DEBUG("DIR: Find %s",path);

	// Remove base dir path
	start += strlen(dirBase->fullname);
	strcpy(expandedPath,dirBase->fullname);

	do {
//		bool errorcheck = false;
		pos = strchr(start,CROSS_FILESPLIT);
		if (pos) { strncpy(dir,start,pos-start); dir[pos-start] = 0; /*errorcheck = true;*/ }
		else	 { strcpy(dir,start); };
 
		// Path found
		Bit16s nextDir = GetLongName(curDir,dir);
		strcat(expandedPath,dir);

		// Error check
//		if ((errorcheck) && (nextDir<0)) {
//			LOG_DEBUG("DIR: Error: %s not found.",expandedPath);
//		};

		// Follow Directory
		if ((nextDir>=0) && curDir->fileList[nextDir]->isDir) {
			curDir = curDir->fileList[nextDir];
			strcpy (curDir->fullname,dir);
			strcpy (work,path); 
			// Cut Directory, if its only part of whole path
			if (pos) work[(Bit32u)pos-(Bit32u)path] = 0;
			if (!IsCachedIn(curDir)) {
				if (OpenDir(curDir,work)) {
					struct dirent result;
					ReadDir(&result);
				};
			}
		};
		if (pos) {
			strcat(expandedPath,split);
			start = pos+1;
		}
	} while (pos);

	// Save last result for faster access next time
	strcpy(save_path,path);
	strcpy(save_expanded,expandedPath);
	save_dir = curDir;

	return curDir;
};

bool DOS_Drive_Cache::OpenDir(const char* path)
{
	char expand[CROSS_LEN] = {0};
	CFileInfo* dir = FindDirInfo(path,expand);
	return OpenDir(dir,expand);
};	

bool DOS_Drive_Cache::OpenDir(CFileInfo* dir, char* expand)
{
	dirSearch = dir;
	// Add "/"
	char end[2]={CROSS_FILESPLIT,0};
	if (expand[strlen(expand)-1]!=CROSS_FILESPLIT) strcat(expand,end);
	// open dir
	if (dirSearch) {
		// open dir
		DIR* dirp = opendir(expand);
		if (dirp) { 
			dirFirstTime = true;
			closedir(dirp);
			strcpy(dirPath,expand);
			return true;
		}
	};
	return false;
};

void DOS_Drive_Cache::CreateEntry(CFileInfo* dir, const char* name)
{
	struct stat status;
	CFileInfo* info = new CFileInfo;
	strcpy(info->fullname,name);				
	strcpy(info->orgname ,name);				
	// always upcase
	upcase(info->fullname);
	info->shortNr = 0;
	// Read and copy file stats
	char buffer[CROSS_LEN];
	strcpy(buffer,dirPath);
	strcat(buffer,info->orgname);
	stat  (buffer,&status);
	info->isDir	= (S_ISDIR(status.st_mode)>0);
	// Check for long filenames...
	CreateShortName(dir, info);		
	// Put file in lists
	dir->fileList.push_back(info);
	dir->outputList.push_back(info);
};

bool DOS_Drive_Cache::ReadDir(struct dirent* result)
{
	if (dirFirstTime) {		
		if (!IsCachedIn(dirSearch)) {
			// Try to open directory
			DIR* dirp = opendir(dirPath);
			// Read complete directory
			struct dirent* tmpres;
			while (tmpres = readdir(dirp)) {			
				CreateEntry(dirSearch,tmpres->d_name);
				// Sort Lists - filelist has to be alphabetically sorted, even in between (for finding double file names) 
				// hmpf.. bit slow probably...
				std::sort(dirSearch->fileList.begin(), dirSearch->fileList.end(), SortByName);
			}
			// close dir
			closedir(dirp);
			// Output list - user defined
			switch (sortDirType) {
				case ALPHABETICAL		: std::sort(dirSearch->outputList.begin(), dirSearch->outputList.end(), SortByName);		break;
				case DIRALPHABETICAL	: std::sort(dirSearch->outputList.begin(), dirSearch->outputList.end(), SortByDirName);		break;
				case ALPHABETICALREV	: std::sort(dirSearch->outputList.begin(), dirSearch->outputList.end(), SortByNameRev);		break;
				case DIRALPHABETICALREV	: std::sort(dirSearch->outputList.begin(), dirSearch->outputList.end(), SortByDirNameRev);	break;
			};
			// Info
/*			if (!dirp) {
				LOG_DEBUG("DIR: Error Caching in %s",dirPath);			
				return false;
			} else {	
				char buffer[128];
				sprintf(buffer,"DIR: Caching in %s (%d Files)",dirPath,dirSearch->fileList.size());
				LOG_DEBUG(buffer);
			};*/
		} else {
			dirFirstTime=false;
		}
		// Reset it..
		nextEntry = 0;
	};
	return SetResult(dirSearch, result, nextEntry);
};

bool DOS_Drive_Cache::SetResult(CFileInfo* dir, struct dirent* result, Bit16u entryNr)
{
	if (entryNr>=dir->outputList.size()) return false;
	CFileInfo* info = dir->outputList[entryNr];
	// copy filename, short version
	strcpy(result->d_name,info->shortname);
	// Set to next Entry
	nextEntry = entryNr+1;
	return true;
};

