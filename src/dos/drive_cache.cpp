
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

/* $Id: drive_cache.cpp,v 1.30 2003-12-26 20:39:27 finsterr Exp $ */

#include "drives.h"
#include "dos_inc.h"
#include "dirent.h"
#include "support.h"

#if defined (WIN32)   /* Win 32 */
#define WIN32_LEAN_AND_MEAN        // Exclude rarely-used stuff from 
#include <windows.h>
#endif

// STL stuff
#include <vector>
#include <iterator>
#include <algorithm>

int fileInfoCounter = 0;

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
	save_dir	= 0;
	srchNr		= 0;
	label[0]	= 0;
	for (Bit32u i=0; i<MAX_OPENDIRS; i++) { dirSearch[i] = 0; free[i] = true; dirFindFirst[i] = 0; };
	SetDirSort(DIRALPHABETICAL);
};

DOS_Drive_Cache::DOS_Drive_Cache(const char* path)
{
	dirBase		= new CFileInfo;
	save_dir	= 0;
	srchNr		= 0;
	label[0]	= 0;
	for (Bit32u i=0; i<MAX_OPENDIRS; i++) { dirSearch[i] = 0; free[i] = true; dirFindFirst[i] = 0; };
	SetDirSort(DIRALPHABETICAL);
	SetBaseDir(path);
};

DOS_Drive_Cache::~DOS_Drive_Cache(void)
{
	Clear();
	for (Bit32u i=0; i<MAX_OPENDIRS; i++) { delete dirFindFirst[i]; dirFindFirst[i]=0; };
};

void DOS_Drive_Cache::Clear(void)
{
	delete dirBase; dirBase = 0;
	for (Bit32u i=0; i<MAX_OPENDIRS; i++) dirSearch[i] = 0;
};

void DOS_Drive_Cache::EmptyCache(void)
{
	// Empty Cache and reinit
	Clear();
	dirBase		= new CFileInfo;
	save_dir	= 0;
	srchNr		= 0;
	for (Bit32u i=0; i<MAX_OPENDIRS; i++) free[i] = true; 
	SetBaseDir(basePath);
};

void DOS_Drive_Cache::SetLabel(const char* vname)
{
	Bitu togo		= 8;
	Bitu vnamePos	= 0;
	Bitu labelPos	= 0;
	bool point		= false;
	while (togo>0) {
		if (vname[vnamePos]==0) break;
		if (!point && (vname[vnamePos]=='.')) { togo=4; point=true; }
		label[labelPos]	= vname[vnamePos];
		label[labelPos] = *upcase(&label[labelPos]);
		labelPos++; vnamePos++;
		togo--;
		if ((togo==0) && !point) { 
			if (vname[vnamePos]=='.') vnamePos++;
			label[labelPos]='.'; labelPos++; point=true; togo=3; 
		}
	};
	label[labelPos]=0;
//	LOG(LOG_ALL,LOG_ERROR)("CACHE: Set volume label to %s",label);
};

Bit16u DOS_Drive_Cache::GetFreeID(CFileInfo* dir)
{
	for (Bit16u i=0; i<MAX_OPENDIRS; i++) if (free[i] || (dir==dirSearch[i])) return i;
	LOG(LOG_FILES,LOG_NORMAL)("DIRCACHE: Too many open directories!");
	return 0;
};

void DOS_Drive_Cache::SetBaseDir(const char* baseDir)
{
	Bit16u id;
	strcpy(basePath,baseDir);
	if (OpenDir(baseDir,id)) {
		char * result;
		ReadDir(id,result);
	};
	// Get Volume Label
#if defined (WIN32)
	char label[256];
	char drive[4] = "C:\\";
	drive[0] = basePath[0];
	if (GetVolumeInformation(drive,label,256,NULL,NULL,NULL,NULL,0)) SetLabel(label);
#endif
};

void DOS_Drive_Cache::ExpandName(char* path)
{
	strcpy(path,GetExpandName(path));
};

char* DOS_Drive_Cache::GetExpandName(const char* path)
{
	static char work [CROSS_LEN] = { 0 };
	char dir [CROSS_LEN]; 

	work[0] = 0;
	strcpy (dir,path);

	const char* pos = strrchr(path,CROSS_FILESPLIT);

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

void DOS_Drive_Cache::AddEntry(const char* path, bool checkExists)
{
	// Get Last part...
	char file	[CROSS_LEN];
	char expand	[CROSS_LEN];

	CFileInfo* dir = FindDirInfo(path,expand);
	const char* pos = strrchr(path,CROSS_FILESPLIT);

	if (pos) {
		strcpy(file,pos+1);	
		// Check if file already exists, then dont add new entry...
		if (checkExists && (GetLongName(dir,file)>=0)) return;

		CreateEntry(dir,file);
		// Sort Lists - filelist has to be alphabetically sorted
		std::sort(dir->fileList.begin(), dir->fileList.end(), SortByName);
		Bits index = GetLongName(dir,file);
		if (index>=0) {
			Bit32u i;
			// Check if there are any open search dir that are affected by this...
			if (dir) for (i=0; i<MAX_OPENDIRS; i++) {
				if ((dirSearch[i]==dir) && (index<=dirSearch[i]->nextEntry)) 
					dirSearch[i]->nextEntry++;
			}	
		};
		//		LOG_DEBUG("DIR: Added Entry %s",path);
	} else {
//		LOG_DEBUG("DIR: Error: Failed to add %s",path);	
	};
};

void DOS_Drive_Cache::DeleteEntry(const char* path, bool ignoreLastDir)
{
	CacheOut(path,ignoreLastDir);
	if (dirSearch[srchNr] && (dirSearch[srchNr]->nextEntry>0)) dirSearch[srchNr]->nextEntry--;

	if (!ignoreLastDir) {
		// Check if there are any open search dir that are affected by this...
		Bit32u i;
		char expand	[CROSS_LEN];
		CFileInfo* dir = FindDirInfo(path,expand);
		if (dir) for (i=0; i<MAX_OPENDIRS; i++) {
			if ((dirSearch[i]==dir) && (dirSearch[i]->nextEntry>0)) 
				dirSearch[i]->nextEntry--;
		}	
	}
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

//	LOG_DEBUG("DIR: Caching out %s : dir %s",expand,dir->orgname);
	// delete file objects...
	for(Bit32u i=0; i<dir->fileList.size(); i++) {
		if (dirSearch[srchNr]==dir->fileList[i]) dirSearch[srchNr] = 0;
		delete dir->fileList[i]; dir->fileList[i] = 0;
	}
	// clear lists
	dir->fileList.clear();
	dir->longNameList.clear();
	save_dir = 0;
};

bool DOS_Drive_Cache::IsCachedIn(CFileInfo* curDir)
{
	return (curDir->fileList.size()>0);
};


bool DOS_Drive_Cache::GetShortName(const char* fullname, char* shortname)
{
	// Get Dir Info
	char expand[CROSS_LEN] = {0};
	CFileInfo* curDir = FindDirInfo(fullname,expand);

	Bits low		= 0;
	Bits high		= curDir->longNameList.size()-1;
	Bits mid, res;

	while (low<=high) {
		mid = (low+high)/2;
		res = strcmp(fullname,curDir->longNameList[mid]->orgname);
		if (res>0)	low  = mid+1; else
		if (res<0)	high = mid-1; 
		else {
			strcpy(shortname,curDir->longNameList[mid]->shortname);
			return true;
		};
	}
	return false;
};

int DOS_Drive_Cache::CompareShortname(const char* compareName, const char* shortName)
{
	char* cpos = strchr(shortName,'~');
	if (cpos) {
		Bits compareCount1	= (int)cpos - (int)shortName;
		char* endPos		= strchr(cpos,'.');
		Bitu numberSize		= endPos ? int(endPos)-int(cpos) : strlen(cpos);
		
		char* lpos			= strchr(compareName,'.');
		Bits compareCount2	= lpos ? int(lpos)-int(compareName) : strlen(compareName);
		if (compareCount2>8) compareCount2 = 8;

		compareCount2 -= numberSize;
		if (compareCount2>compareCount1) compareCount1 = compareCount2;
		return strncmp(compareName,shortName,compareCount1);
	}
	return strcmp(compareName,shortName);
};

Bit16u DOS_Drive_Cache::CreateShortNameID(CFileInfo* curDir, const char* name)
{
	Bits foundNr	= 0;	
	Bits low		= 0;
	Bits high		= curDir->longNameList.size()-1;
	Bits mid, res;

	while (low<=high) {
		mid = (low+high)/2;
		res = CompareShortname(name,curDir->longNameList[mid]->shortname);
		
		if (res>0)	low  = mid+1; else
		if (res<0)	high = mid-1; 
		else {
			// any more same x chars in next entries ?	
			do {	
				foundNr = curDir->longNameList[mid]->shortNr;
				mid++;
			} while(mid<curDir->longNameList.size() && (CompareShortname(name,curDir->longNameList[mid]->shortname)==0));
			break;
		};
	}
	return foundNr+1;
};

bool DOS_Drive_Cache::RemoveTrailingDot(char* shortname)
// remove trailing '.' if no extension is available (Linux compatibility)
{
	Bitu len = strlen(shortname);
	if (len && (shortname[len-1]=='.')) {
		if (len==1) return false;
		if ((len==2) && (shortname[0]=='.')) return false;
		shortname[len-1] = 0;	
		return true;
	}	
	return false;
};

Bits DOS_Drive_Cache::GetLongName(CFileInfo* curDir, char* shortName)
{
	// Remove dot, if no extension...
	RemoveTrailingDot(shortName);
	// Search long name and return array number of element
	Bits low	= 0;
	Bits high = curDir->fileList.size()-1;
	Bits mid,res;
	while (low<=high) {
		mid = (low+high)/2;
		res = strcmp(shortName,curDir->fileList[mid]->shortname);
		if (res>0)	low  = mid+1; else
		if (res<0)	high = mid-1; else
		{	// Found
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
	while (*chkpos!=0) { 
		if (*chkpos==' ') chkpos++; else *curpos++ = *chkpos++; 
	}
	*curpos = 0;
	return (curpos!=chkpos);
};

void DOS_Drive_Cache::CreateShortName(CFileInfo* curDir, CFileInfo* info)
{
	Bits	len			= 0;
	Bits	lenExt		= 0;
	bool	createShort = false;

	char tmpNameBuffer[CROSS_LEN];

	char* tmpName = tmpNameBuffer;

	// Remove Spaces
	strcpy(tmpName,info->orgname);
	upcase(tmpName);
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
		if (len+strlen(buffer)+1>8)	tocopy = 8 - strlen(buffer) - 1;
		else						tocopy = len;
		strncpy(info->shortname,tmpName,tocopy);
		info->shortname[tocopy] = 0;
		// Copy number
		strcat(info->shortname,"~");
		strcat(info->shortname,buffer);
		// Create compare Count
//		info->compareCount = tocopy;
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
	RemoveTrailingDot(info->shortname);
};

DOS_Drive_Cache::CFileInfo* DOS_Drive_Cache::FindDirInfo(const char* path, char* expandedPath)
{
	// statics
	static char	split[2] = { CROSS_FILESPLIT,0 };
	
	char		dir  [CROSS_LEN]; 
	char		work [CROSS_LEN];
	const char*	start = path;
	const char*		pos;
	CFileInfo*	curDir = dirBase;
	Bit16u		id;

	if (save_dir && (strcmp(path,save_path)==0)) {
		strcpy(expandedPath,save_expanded);
		return save_dir;
	};

//	LOG_DEBUG("DIR: Find %s",path);

	// Remove base dir path
	start += strlen(basePath);
	strcpy(expandedPath,basePath);

	// hehe, baseDir should be cached in... 
	if (!IsCachedIn(curDir)) {
		strcpy(work,basePath);
		if (OpenDir(curDir,work,id)) {
			char buffer[CROSS_LEN];
			char * result;
			strcpy(buffer,dirPath);
			ReadDir(id,result);
			strcpy(dirPath,buffer);
			free[id] = true;
		};
	};

	do {
//		bool errorcheck = false;
		pos = strchr(start,CROSS_FILESPLIT);
		if (pos) { strncpy(dir,start,pos-start); dir[pos-start] = 0; /*errorcheck = true;*/ }
		else	 { strcpy(dir,start); };
 
		// Path found
		Bits nextDir = GetLongName(curDir,dir);
		strcat(expandedPath,dir);

		// Error check
/*		if ((errorcheck) && (nextDir<0)) {
			LOG_DEBUG("DIR: Error: %s not found.",expandedPath);
		};
*/
		// Follow Directory
		if ((nextDir>=0) && curDir->fileList[nextDir]->isDir) {
			curDir = curDir->fileList[nextDir];
			strcpy (curDir->orgname,dir);
			if (!IsCachedIn(curDir)) {
				if (OpenDir(curDir,expandedPath,id)) {
					char buffer[CROSS_LEN];
					char * result;
					strcpy(buffer,dirPath);
					ReadDir(id,result);
					strcpy(dirPath,buffer);
					free[id] = true;
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

bool DOS_Drive_Cache::OpenDir(const char* path, Bit16u& id)
{
	char expand[CROSS_LEN] = {0};
	CFileInfo* dir = FindDirInfo(path,expand);
	if (OpenDir(dir,expand,id)) {
		dirSearch[id]->nextEntry = 0;
		return true;
	}
	return false;
};	

bool DOS_Drive_Cache::OpenDir(CFileInfo* dir, const char* expand, Bit16u& id)
{
	id = GetFreeID(dir);
	dirSearch[id] = dir;
	char expandcopy [CROSS_LEN];
	strcpy(expandcopy,expand);   
	// Add "/"
	char end[2]={CROSS_FILESPLIT,0};
	if (expandcopy[strlen(expandcopy)-1]!=CROSS_FILESPLIT) strcat(expandcopy,end);
	// open dir
	if (dirSearch[id]) {
		// open dir
		DIR* dirp = opendir(expandcopy);
		if (dirp) { 
			// Reset it..
			closedir(dirp);
			strcpy(dirPath,expandcopy);
			free[id] = false;
			return true;
		}
	};
	return false;
};

void DOS_Drive_Cache::CreateEntry(CFileInfo* dir, const char* name)
{
	struct stat status;
	CFileInfo* info = new CFileInfo;
	strcpy(info->orgname ,name);				
	info->shortNr = 0;
	// Read and copy file stats
	char buffer[CROSS_LEN];
	strcpy(buffer,dirPath);
	strcat(buffer,info->orgname);
	if (stat(buffer,&status)==0)	info->isDir	= (S_ISDIR(status.st_mode)>0);
	else							info->isDir = false;
	// Check for long filenames...
	CreateShortName(dir, info);		
	// Put file in lists
	dir->fileList.push_back(info);
};

bool DOS_Drive_Cache::ReadDir(Bit16u id, char* &result)
{
	// shouldnt happen...
	if (id>MAX_OPENDIRS) return false;

	if (!IsCachedIn(dirSearch[id])) {
		// Try to open directory
		DIR* dirp = opendir(dirPath);
		if (!dirp) {
			free[id] = true;
			return false;
		}
		// Read complete directory
		struct dirent* tmpres;
		while ((tmpres = readdir(dirp))!=NULL) {			
			CreateEntry(dirSearch[id],tmpres->d_name);
			// Sort Lists - filelist has to be alphabetically sorted, even in between (for finding double file names) 
			// hmpf.. bit slow probably...
			std::sort(dirSearch[id]->fileList.begin(), dirSearch[id]->fileList.end(), SortByName);
		}
		// close dir
		closedir(dirp);
		// Info
/*		if (!dirp) {
			LOG_DEBUG("DIR: Error Caching in %s",dirPath);			
			return false;
		} else {	
			char buffer[128];
			sprintf(buffer,"DIR: Caching in %s (%d Files)",dirPath,dirSearch[srchNr]->fileList.size());
			LOG_DEBUG(buffer);
		};*/
	};
	if (SetResult(dirSearch[id], result, dirSearch[id]->nextEntry)) return true;
	free[id] = true;
	return false;
};

bool DOS_Drive_Cache::SetResult(CFileInfo* dir, char* &result, Bit16u entryNr)
{
	static char res[CROSS_LEN] = { 0 };

	result = res;
	if (entryNr>=dir->fileList.size()) return false;
	CFileInfo* info = dir->fileList[entryNr];
	// copy filename, short version
	strcpy(res,info->shortname);
	// Set to next Entry
	dir->nextEntry = entryNr+1;
	return true;
};

// FindFirst / FindNext
bool DOS_Drive_Cache::FindFirst(char* path, Bitu dtaAddress, Bitu& id)
{
	Bit16u	dirID;
	Bitu	dirFindFirstID = 0xffff;

	// Cache directory in 
	if (!OpenDir(path,dirID)) return false;
	// Seacrh if dta was already used before
	for (Bitu n=0; n<MAX_OPENDIRS; n++) {
		if (dirFindFirst[n]) {
			if (dirFindFirst[n]->compareCount == dtaAddress) {
				// Reuse old dta
				dirFindFirstID = n; break;
			}		
		} else if (dirFindFirstID==0xffff) {
			dirFindFirstID = n;
		}	
	} 
	if (dirFindFirstID==0xffff) {
		// no free slot found...
		LOG(LOG_MISC,LOG_ERROR)("DIRCACHE: FindFirst/Next failure : All slots full.");
		// always use first then
		dirFindFirstID = 0;
	}		
	// Clear and reuse slot
	delete dirFindFirst[dirFindFirstID];
	dirFindFirst[dirFindFirstID] = new CFileInfo();
	dirFindFirst[dirFindFirstID]-> nextEntry	= 0;
	dirFindFirst[dirFindFirstID]-> compareCount	= dtaAddress;	

	// Copy entries to use with FindNext
	for (Bitu i=0; i<dirSearch[dirID]->fileList.size(); i++) {
		CreateEntry(dirFindFirst[dirFindFirstID],dirSearch[dirID]->fileList[i]->orgname);
		// Sort Lists - filelist has to be alphabetically sorted, even in between (for finding double file names) 
		std::sort(dirFindFirst[dirFindFirstID]->fileList.begin(), dirFindFirst[dirFindFirstID]->fileList.end(), SortByName);
	};
	// Now re-sort the fileList accordingly to output
	switch (sortDirType) {
		case ALPHABETICAL		: std::sort(dirFindFirst[dirFindFirstID]->fileList.begin(), dirFindFirst[dirFindFirstID]->fileList.end(), SortByName);		break;
		case DIRALPHABETICAL	: std::sort(dirFindFirst[dirFindFirstID]->fileList.begin(), dirFindFirst[dirFindFirstID]->fileList.end(), SortByDirName);		break;
		case ALPHABETICALREV	: std::sort(dirFindFirst[dirFindFirstID]->fileList.begin(), dirFindFirst[dirFindFirstID]->fileList.end(), SortByNameRev);		break;
		case DIRALPHABETICALREV	: std::sort(dirFindFirst[dirFindFirstID]->fileList.begin(), dirFindFirst[dirFindFirstID]->fileList.end(), SortByDirNameRev);	break;
		case NOSORT				: break;
	};	

//	LOG(LOG_MISC,LOG_ERROR)("DIRCACHE: FindFirst : %s (ID:%02X)",path,dirFindFirstID);
	id = dirFindFirstID;
	return true;
};

bool DOS_Drive_Cache::FindNext(Bitu id, char* &result)
{
	// out of range ?
	if ((id>=MAX_OPENDIRS) || !dirFindFirst[id]) {
		LOG(LOG_MISC,LOG_ERROR)("DIRCACHE: FindFirst/Next failure : ID out of range: %04X",id);
		return false;
	}
	if (!SetResult(dirFindFirst[id], result, dirFindFirst[id]->nextEntry)) {
		// free slot
		delete dirFindFirst[id]; dirFindFirst[id] = 0;
		return false;
	}
	return true;
};

// ****************************************************************************
// No Dir Cache, 
// ****************************************************************************

static DIR*	srch_opendir = 0;

DOS_No_Drive_Cache::DOS_No_Drive_Cache(const char* path)
{
	SetBaseDir(path);
};

void DOS_No_Drive_Cache::SetBaseDir(const char* path)
{
	strcpy(basePath,path);
}

bool DOS_No_Drive_Cache::OpenDir(const char* path, Bit16u& id)
{
	id = 0;
	strcpy(dirPath,path);
	if((srch_opendir=opendir(dirPath))==NULL) return false;
	return true;
};

bool DOS_No_Drive_Cache::ReadDir(Bit16u id, char* &result)
{
	static char res[CROSS_LEN] = { 0 };
	dirent * ent;		

	if (!srch_opendir) return false;
	if ((ent=readdir(srch_opendir))==NULL) {
		strcpy(res,ent->d_name);
		result=res;
		closedir(srch_opendir);
		srch_opendir=NULL;
		return false;
	}
	return true;
};
