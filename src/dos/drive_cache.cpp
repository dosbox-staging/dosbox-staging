/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "dos_system.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <vector>

#include "cross.h"
#include "dos_inc.h"
#include "drives.h"
#include "string_utils.h"
#include "support.h"

int fileInfoCounter = 0;

bool SortByName(DOS_Drive_Cache::CFileInfo* const &a, DOS_Drive_Cache::CFileInfo* const &b) {
	return strcmp(a->shortname,b->shortname)<0;
}

bool SortByNameRev(DOS_Drive_Cache::CFileInfo* const &a, DOS_Drive_Cache::CFileInfo* const &b) {
	return strcmp(a->shortname,b->shortname)>0;
}

bool SortByDirName(DOS_Drive_Cache::CFileInfo* const &a, DOS_Drive_Cache::CFileInfo* const &b) {
	// Directories first...
	if (a->isDir!=b->isDir) return (a->isDir>b->isDir);	
	return strcmp(a->shortname,b->shortname)<0;
}

bool SortByDirNameRev(DOS_Drive_Cache::CFileInfo* const &a, DOS_Drive_Cache::CFileInfo* const &b) {
	// Directories first...
	if (a->isDir!=b->isDir) return (a->isDir>b->isDir);	
	return strcmp(a->shortname,b->shortname)>0;
}

DOS_Drive_Cache::DOS_Drive_Cache(void)
	: dirBase(new CFileInfo),
	  dirPath{0},
	  basePath{0},
	  sortDirType(DIRALPHABETICAL),
	  save_dir(nullptr),
	  save_path{0},
	  save_expanded{0},
	  srchNr(0),
	  dirSearch{nullptr},
	  dirFindFirst{nullptr},
	  nextFreeFindFirst(0),
	  label{0},
	  updatelabel(true)
{
}

DOS_Drive_Cache::DOS_Drive_Cache(const char* path)
	: dirBase(new CFileInfo),
	  dirPath{0},
	  basePath{0},
	  sortDirType(DIRALPHABETICAL),
	  save_dir(nullptr),
	  save_path{0},
	  save_expanded{0},
	  srchNr(0),
	  dirSearch{nullptr},
	  dirFindFirst{nullptr},
	  nextFreeFindFirst(0),
	  label{0},
	  updatelabel(true)
{
	SetBaseDir(path);
}

DOS_Drive_Cache::~DOS_Drive_Cache(void) {
	Clear();
	for (uint32_t i=0; i<MAX_OPENDIRS; i++) {
		DeleteFileInfo(dirFindFirst[i]);
		dirFindFirst[i] = nullptr;
	}
}

void DOS_Drive_Cache::Clear(void) {
	DeleteFileInfo(dirBase);
	dirBase = nullptr;
	nextFreeFindFirst	= 0;
	for (uint32_t i=0; i<MAX_OPENDIRS; i++)
		dirSearch[i] = nullptr;
}

void DOS_Drive_Cache::EmptyCache(void) {
	// Empty Cache and reinit
	Clear();
	dirBase		= new CFileInfo;
	save_dir	= nullptr;
	srchNr		= 0;
	if (basePath[0] != 0) SetBaseDir(basePath);
}

void DOS_Drive_Cache::SetLabel(const char* vname,bool cdrom,bool allowupdate) {
/* allowupdate defaults to true. if mount sets a label then allowupdate is 
 * false and will this function return at once after the first call.
 * The label will be set at the first call. */

	if(!this->updatelabel) return;
	this->updatelabel = allowupdate;
	Set_Label(vname,label,cdrom);
	LOG(LOG_DOSMISC,LOG_NORMAL)("DIRCACHE: Set volume label to %s",label);
}

uint16_t DOS_Drive_Cache::GetFreeID(CFileInfo* dir) {
	if (dir->id != MAX_OPENDIRS)
		return dir->id;
	for (uint16_t i=0; i<MAX_OPENDIRS; i++) {
		if (!dirSearch[i]) {
			dir->id = i;
			return i;
		}
	}
	LOG(LOG_FILES,LOG_NORMAL)("DIRCACHE: Too many open directories!");
	dir->id = 0;
	return 0;
}

void DOS_Drive_Cache::SetBaseDir(const char *baseDir)
{
	if (is_empty(baseDir))
		return;

	// Guard if source and destination are the same
	if (basePath == baseDir) {
		return;
	}

	safe_strcpy(basePath, baseDir);
	static uint16_t id = 0;
	if (OpenDir(baseDir,id)) {
		char* result = 0;
		ReadDir(id,result);
	}
	// Get Volume Label
#if defined (WIN32)
	bool cdrom = false;
	char labellocal[256]={ 0 };
	char drive[4] = "C:\\";
	drive[0] = basePath[0];
	if (GetVolumeInformation(drive,labellocal,256,NULL,NULL,NULL,NULL,0)) {
	UINT test = GetDriveType(drive);
	if(test == DRIVE_CDROM) cdrom = true;
		/* Set label and allow being updated */
		SetLabel(labellocal,cdrom,true);
	}
#endif
}

void DOS_Drive_Cache::ExpandName(char* path) {
	safe_strncpy(path, GetExpandName(path), CROSS_LEN);
}

char* DOS_Drive_Cache::GetExpandName(const char* path) {
	static char work [CROSS_LEN] = { 0 };
	char dir [CROSS_LEN];

	work[0] = 0;
	safe_strcpy (dir, path);

	const char* pos = strrchr(path,CROSS_FILESPLIT);

	if (pos) dir[pos-path+1] = 0;
	CFileInfo* dirInfo = FindDirInfo(dir, work);
		
	if (pos) {
		// Last Entry = File
		safe_strcpy(dir, pos+1);
		(void) GetLongName(dirInfo, dir, sizeof(dir)); // ignore the return code
		safe_strcat(work, dir);
	}

	if (*work) {
		size_t len = safe_strlen(work);
#if defined (WIN32)
		if((work[len-1] == CROSS_FILESPLIT ) && (len >= 2) && (work[len-2] != ':')) {
#else
		if((len > 1) && (work[len-1] == CROSS_FILESPLIT )) {
#endif
			work[len-1] = 0; // Remove trailing slashes except when in root
		}
	}
	return work;
}

void DOS_Drive_Cache::AddEntry(const char* path, bool checkExists) {
	// Get Last part...
	char file	[CROSS_LEN];
	char expand	[CROSS_LEN];

	CFileInfo* dir = FindDirInfo(path,expand);
	const char* pos = strrchr(path,CROSS_FILESPLIT);

	if (pos && dir) {
		safe_strcpy(file, pos+1);
		// Check if file already exists, then don't add new entry...
		if (checkExists) {
			if (GetLongName(dir, file, sizeof(file))>=0) return;
		}

		CreateEntry(dir,file,false);

		Bits index = GetLongName(dir, file, sizeof(file));
		if (index>=0) {
			uint32_t i;
			// Check if there are any open search dir that are affected by this...
			for (i=0; i<MAX_OPENDIRS; i++) {
				if ((dirSearch[i]==dir) && ((uint32_t)index<=dirSearch[i]->nextEntry)) 
					dirSearch[i]->nextEntry++;
			}
		}
		//		LOG_DEBUG("DIR: Added Entry %s",path);
	} else {
//		LOG_DEBUG("DIR: Error: Failed to add %s",path);	
	}
}
void DOS_Drive_Cache::AddEntryDirOverlay(const char* path, bool checkExists) {
	// Get Last part...
	char file	[CROSS_LEN];
	char expand	[CROSS_LEN];
	char dironly[CROSS_LEN + 1];

	//When adding a directory, the directory we want to operate inside in is the above it. (which can go wrong if the directory already exists.)
	safe_strcpy(dironly, path);
	char* post = strrchr(dironly,CROSS_FILESPLIT);

	if (post) {
#if defined (WIN32)
		if (post > dironly && *(post - 1) == ':' && (post - dironly) == 2) 
			post++; //move away from X: as need to end up with x:\ .  
#else
	//Lets hope this is not really used.. (root folder specified as overlay)
		if (post == dironly)
			post++; //move away from / 
#endif
		*post = 0; //TODO take care of AddEntryDIR D:\\piet) (so mount d d:\ as base)
		*(post + 1) = 0; //As FindDirInfo is skipping over the base directory
	}
	CFileInfo* dir = FindDirInfo(dironly,expand);
	const char* pos = strrchr(path,CROSS_FILESPLIT);

	if (pos && dir) {
		safe_strcpy(file, pos + 1);
		// Check if directory already exists, then don't add new entry...
		if (checkExists) {
			Bits index = GetLongName(dir, file, sizeof(file));
			if (index >= 0) {
				//directory already exists, but most likely empty. 
				dir = dir->fileList[index];
				if (dir->isOverlayDir && dir->fileList.empty()) {
					//maybe care about searches ? but this function should only run on cache inits/refreshes.
					//add dot entries
					CreateEntry(dir,".",true);
					CreateEntry(dir,"..",true);
				}
				return;
			}
		}

		CreateEntry(dir,file,true);
		

		Bits index = GetLongName(dir, file, sizeof(file));
		if (index>=0) {
			uint32_t i;
			// Check if there are any open search dir that are affected by this...
			for (i=0; i<MAX_OPENDIRS; i++) {
				if ((dirSearch[i]==dir) && ((uint32_t)index<=dirSearch[i]->nextEntry)) 
					dirSearch[i]->nextEntry++;
			}

			dir = dir->fileList[index];
			dir->isOverlayDir = true;
			CreateEntry(dir,".",true);
			CreateEntry(dir,"..",true);
		}
		//		LOG_DEBUG("DIR: Added Entry %s",path);
	} else {
		//		LOG_DEBUG("DIR: Error: Failed to add %s",path);	
	}
}

void DOS_Drive_Cache::DeleteEntry(const char* path, bool ignoreLastDir) {
	CacheOut(path,ignoreLastDir);
	if (dirSearch[srchNr] && (dirSearch[srchNr]->nextEntry>0)) dirSearch[srchNr]->nextEntry--;

	if (!ignoreLastDir) {
		// Check if there are any open search dir that are affected by this...
		uint32_t i;
		char expand	[CROSS_LEN];
		CFileInfo* dir = FindDirInfo(path,expand);
		if (dir) for (i=0; i<MAX_OPENDIRS; i++) {
			if ((dirSearch[i]==dir) && (dirSearch[i]->nextEntry>0)) 
				dirSearch[i]->nextEntry--;
		}	
	}
}

void DOS_Drive_Cache::CacheOut(const char* path, bool ignoreLastDir) {
	char expand[CROSS_LEN] = { 0 };
	CFileInfo* dir;
	
	if (ignoreLastDir) {
		char tmp[CROSS_LEN] = { 0 };
		int32_t len=0;
		const char* pos = strrchr(path,CROSS_FILESPLIT);
		if (pos)
			len = (int32_t)(pos - path);
		if (len>0) {
			safe_strncpy(tmp,path,len+1);
		} else	{
			safe_strcpy(tmp, path);
		}
		dir = FindDirInfo(tmp,expand);
	} else {
		dir = FindDirInfo(path,expand);	
	}

//	LOG_DEBUG("DIR: Caching out %s : dir %s",expand,dir->orgname);
	// delete file objects...
	//Maybe check if it is a file and then only delete the file and possibly the long name. instead of all objects in the dir.
	for(uint32_t i=0; i<dir->fileList.size(); i++) {
		if (dirSearch[srchNr]==dir->fileList[i])
			dirSearch[srchNr] = nullptr;
		DeleteFileInfo(dir->fileList[i]);
		dir->fileList[i] = nullptr;
	}
	// clear lists
	dir->fileList.clear();
	dir->longNameList.clear();
	save_dir = nullptr;
}

bool DOS_Drive_Cache::IsCachedIn(CFileInfo* curDir) {
	return (curDir->isOverlayDir || curDir->fileList.size()>0);
}


bool DOS_Drive_Cache::GetShortName(const char* fullname, char* shortname) {
	// Get Dir Info
	char expand[CROSS_LEN] = {0};
	CFileInfo* curDir = FindDirInfo(fullname,expand);

	const char* pos = strrchr(fullname,CROSS_FILESPLIT);
	if (pos)
		pos++;
	else
		return false;

	std::vector<CFileInfo*>::size_type filelist_size = curDir->longNameList.size();
	if (GCC_UNLIKELY(filelist_size<=0))
		return false;

	// The orgname part of the list is not sorted (shortname is)! So we can only walk through it.
	for(Bitu i = 0; i < filelist_size; i++) {
#if defined (WIN32)
		if (strcasecmp(pos,curDir->longNameList[i]->orgname) == 0) {
#else
		if (strcmp(pos,curDir->longNameList[i]->orgname) == 0) {
#endif
			safe_strncpy(shortname, curDir->longNameList[i]->shortname, DOS_NAMELENGTH_ASCII);
			return true;
		}
	}
	return false;
}

int DOS_Drive_Cache::CompareShortname(const char* compareName, const char* shortName) {
	char const* cpos = strchr(shortName,'~');
	if (cpos) {
/* the following code is replaced as it's not safe when char* is 64 bits */
/*		Bits compareCount1	= (int)cpos - (int)shortName;
		char* endPos		= strchr(cpos,'.');
		Bitu numberSize		= endPos ? int(endPos)-int(cpos) : safe_strlen(cpos);
		
		char* lpos			= strchr(compareName,'.');
		Bits compareCount2	= lpos ? int(lpos)-int(compareName) : safe_strlen(compareName);
		if (compareCount2>8) compareCount2 = 8;

		compareCount2 -= numberSize;
		if (compareCount2>compareCount1) compareCount1 = compareCount2;
*/
		size_t compareCount1 = strcspn(shortName,"~");
		size_t numberSize    = strcspn(cpos,".");
		size_t compareCount2 = strcspn(compareName,".");
		if(compareCount2 > 8) compareCount2 = 8;
		/* We want 
		 * compareCount2 -= numberSize;
		 * if (compareCount2>compareCount1) compareCount1 = compareCount2;
		 * but to prevent negative numbers: 
		 */
		if(compareCount2 > compareCount1 + numberSize)
			compareCount1 = compareCount2 - numberSize;
		return strncmp(compareName,shortName,compareCount1);
	}
	return strcmp(compareName,shortName);
}

unsigned DOS_Drive_Cache::CreateShortNameID(CFileInfo *curDir, const char *name)
{
	assert(curDir);
	const auto filelist_size = curDir->longNameList.size();
	if (filelist_size == 0)
		return 1; // short name IDs start with 1

	unsigned found_nr = 0;
	Bits low = 0;
	Bits high = (Bits)(filelist_size - 1);

	while (low <= high) {
		auto mid = (low + high) / 2;
		const char *other_shortname = curDir->longNameList[mid]->shortname;
		const int res = CompareShortname(name, other_shortname);
		
		if (res>0)	low  = mid+1; else
		if (res<0)	high = mid-1; 
		else {
			// any more same x chars in next entries ?	
			do {
				found_nr = curDir->longNameList[mid]->shortNr;
				mid++;
			} while((Bitu)mid < filelist_size && (CompareShortname(name, curDir->longNameList[mid]->shortname) == 0));
			break;
		};
	}
	return found_nr + 1;
}

bool DOS_Drive_Cache::RemoveTrailingDot(char* shortname) {
// remove trailing '.' if no extension is available (Linux compatibility)
	size_t len = strlen(shortname);
	if (len && (shortname[len-1]=='.')) {
		if (len==1) return false;
		if ((len==2) && (shortname[0]=='.')) return false;
		shortname[len-1] = 0;	
		return true;
	}	
	return false;
}

#define WINE_DRIVE_SUPPORT 1
#if WINE_DRIVE_SUPPORT
//Changes to interact with WINE by supporting their namemangling.
//The code is rather slow, because orglist is unordered, so it needs to be avoided if possible.
//Hence the tests in GetLongFileName


// From the Wine project
static Bits wine_hash_short_file_name( char* name, char* buffer )
{
	constexpr char hash_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345";

	// returns '_' if invalid or upper case if valid.
	auto replace_invalid = [](char c) -> char {
		constexpr char invalid_chars[] = {'*',
		                                  '?',
		                                  '<',
		                                  '>',
		                                  '|',
		                                  '"',
		                                  '+',
		                                  '=',
		                                  ',',
		                                  ';',
		                                  '[',
		                                  ']',
		                                  ' ',
		                                  '\345',
		                                  '~',
		                                  '.',
		                                  '\0'};
		const auto is_invalid = char_is_negative(c) ||
		                        strchr(invalid_chars, c);
		return is_invalid ? '_' : toupper(c);
	};

	char *p = nullptr;
	char *ext = nullptr;
	char *end = name + strlen(name);
	char *dst = nullptr;
	uint16_t hash = 0;
	int i = 0;

	// Compute the hash code of the file name
	for (p = name, hash = 0xbeef; p < end - 1; p++)
		hash = (hash<<3) ^ (hash>>5) ^ tolower(*p) ^ (tolower(p[1]) << 8);
	hash = (hash<<3) ^ (hash>>5) ^ tolower(*p); // Last character

	// Find last dot for start of the extension
	for (p = name + 1, ext = NULL; p < end - 1; p++) if (*p == '.') ext = p;

	// Copy first 4 chars, replacing invalid chars with '_'
	for (i = 4, p = name, dst = buffer; i > 0; i--, p++)
	{
		if (p == end || p == ext) {
			break;
		}
		*dst++ = replace_invalid(*p);
	}
	// Pad to 5 chars with '~'
	while (i-- >= 0)
		*dst++ = '~';

	// Insert hash code converted to 3 ASCII chars
	*dst++ = hash_chars[(hash >> 10) & 0x1f];
	*dst++ = hash_chars[(hash >> 5) & 0x1f];
	*dst++ = hash_chars[hash & 0x1f];

	// Copy the first 3 chars of the extension (if any)
	if (ext) {
		*dst++ = '.';
		for (i = 3, ext++; (i > 0) && ext < end; i--, ext++) {
			*dst++ = replace_invalid(*ext);
		}
	}

	return dst - buffer;
}
#endif

Bits DOS_Drive_Cache::GetLongName(CFileInfo* curDir, char* shortName, const size_t shortName_len) {
	std::vector<CFileInfo*>::size_type filelist_size = curDir->fileList.size();
	if (GCC_UNLIKELY(filelist_size<=0)) return -1;

	// Remove dot, if no extension...
	RemoveTrailingDot(shortName);
	// Search long name and return array number of element
	Bits low	= 0;
	Bits high	= (Bits)(filelist_size-1);
	Bits mid,res;
	while (low<=high) {
		mid = (low+high)/2;
		res = strcmp(shortName,curDir->fileList[mid]->shortname);
		if (res>0)	low  = mid+1; else
		if (res<0)	high = mid-1; else
		{	// Found
			safe_strncpy(shortName, curDir->fileList[mid]->orgname, shortName_len);
			return mid;
		};
	}
#ifdef WINE_DRIVE_SUPPORT
	if (strlen(shortName) < 8 || shortName[4] != '~' || shortName[5] == '.' || shortName[6] == '.' || shortName[7] == '.') return -1; // not available
	// else it's most likely a Wine style short name ABCD~###, # = not dot  (length at least 8) 
	// The above test is rather strict as the following loop can be really slow if filelist_size is large.
	char buff[CROSS_LEN];
	for (Bitu i = 0; i < filelist_size; i++) {
		res = wine_hash_short_file_name(curDir->fileList[i]->orgname,buff);
		buff[res] = 0;
		if (!strcmp(shortName,buff)) {	
			// Found
			safe_strncpy(shortName, curDir->fileList[i]->orgname, shortName_len);
			return (Bits)i;
		}
	}
#endif
	// not available
	return -1;
}

bool DOS_Drive_Cache::RemoveSpaces(char* str) {
// Removes all spaces
	char*	curpos	= str;
	char*	chkpos	= str;
	while (*chkpos!=0) { 
		if (*chkpos==' ') chkpos++; else *curpos++ = *chkpos++; 
	}
	*curpos = 0;
	return (curpos!=chkpos);
}

void DOS_Drive_Cache::CreateShortName(CFileInfo* curDir, CFileInfo* info) {
	Bits	len			= 0;
	bool	createShort = false;

	// Remove Spaces
	char tmpNameBuffer[CROSS_LEN];
	safe_strcpy(tmpNameBuffer, info->orgname);
	char* tmpName = tmpNameBuffer;
	upcase(tmpName);
	createShort = RemoveSpaces(tmpName);

	// Get Length of filename
	char* pos = strchr(tmpName,'.');
	if (pos) {
		// ignore preceding '.' if extension is longer than "3"
		if (strlen(pos) > 4) {
			while (*tmpName=='.') tmpName++;
			createShort = true;
		}
		pos = strchr(tmpName,'.');
		if (pos)
			len = pos - tmpName;
		else
			len = (Bits)strlen(tmpName);
	} else {
		len = strlen(tmpName);
	}

	// Should shortname version be created ?
	createShort = createShort || (len>8);
	if (!createShort) {
		char buffer[CROSS_LEN];
		safe_strcpy(buffer, tmpName);
		createShort = (GetLongName(curDir, buffer, sizeof(buffer)) >= 0);
	}

	if (createShort) {
		// Create number
		info->shortNr = CreateShortNameID(curDir, tmpName);

		// If processing a directory containing 10 million or more long files,
		// then ten duplicate short filenames will be named ~1000000.ext,
		// another 10 duplicates will be named ~1000001.ext, and so on, back
		// through to ~9999999.ext if 999,999,999 files are present.
		// Yes, this is a broken corner-case, but is still memory-safe.
		// TODO: modify MOUNT/IMGMOUNT to exit with an error when encountering
		// a directory having more than 65534 files, which is FAT32's limit.
		char short_nr[8] = {'\0'};
		if (GCC_UNLIKELY(info->shortNr > 9999999)) E_Exit("~9999999 same name files overflow");
		safe_sprintf(short_nr, "%u", info->shortNr);

		// Copy first letters
		Bits tocopy = 0;
		size_t buflen = safe_strlen(short_nr);
		if (len + buflen + 1 > 8)
			tocopy = (Bits)(8 - buflen - 1);
		else
			tocopy = len;

		// Copy the lesser of "DOS_NAMELENGTH_ASCII" or "tocopy + 1" characters.
		safe_strncpy(info->shortname, tmpName,
		             tocopy < DOS_NAMELENGTH_ASCII ? tocopy + 1 : DOS_NAMELENGTH_ASCII);
		// Copy number
		safe_strcat(info->shortname, "~");
		safe_strcat(info->shortname, short_nr);

		// Add (and cut) Extension, if available
		if (pos) {
			// Step to last extension...
			pos = strrchr(tmpName, '.'); // extensions are at-most 3 chars (4 with terminator)
			// add extension
			unsigned int remaining_space = DOS_NAMELENGTH_ASCII - safe_strlen(info->shortname) - 1;
			strncat(info->shortname, pos, 4 < remaining_space ? 4 : remaining_space);
			info->shortname[DOS_NAMELENGTH] = 0;
		}

		// keep list sorted for CreateShortNameID to work correctly
		if (curDir->longNameList.size()>0) {
			if (!(strcmp(info->shortname,curDir->longNameList.back()->shortname)<0)) {
				// append at end of list
				curDir->longNameList.push_back(info);
			} else {
				// look for position where to insert this element
				bool found=false;
				std::vector<CFileInfo*>::iterator it;
				for (it=curDir->longNameList.begin(); it!=curDir->longNameList.end(); ++it) {
					if (strcmp(info->shortname,(*it)->shortname)<0) {
						found = true;
						break;
					}
				}
				// Put it in longname list...
				if (found) curDir->longNameList.insert(it,info);
				else curDir->longNameList.push_back(info);
			}
		} else {
			// empty file list, append
			curDir->longNameList.push_back(info);
		}
	} else {
		safe_strcpy(info->shortname, tmpName);
	}
	RemoveTrailingDot(info->shortname);
}

DOS_Drive_Cache::CFileInfo* DOS_Drive_Cache::FindDirInfo(const char* path, char* expandedPath) {
	// statics
	static char	split[2] = { CROSS_FILESPLIT,0 };
	
	char		dir  [CROSS_LEN]; 
	char		work [CROSS_LEN];
	const char*	start = path;
	const char*		pos;
	CFileInfo*	curDir = dirBase;
	uint16_t		id;

	if (save_dir && (strcmp(path,save_path)==0)) {
		safe_strncpy(expandedPath, save_expanded, CROSS_LEN);
		return save_dir;
	};

//	LOG_DEBUG("DIR: Find %s",path);

	// Remove base dir path
	start += safe_strlen(basePath);
	safe_strncpy(expandedPath, basePath, CROSS_LEN);

	// hehe, baseDir should be cached in... 
	if (!IsCachedIn(curDir)) {
		safe_strcpy(work, basePath);
		if (OpenDir(curDir,work,id)) {
			char buffer[CROSS_LEN];
			char* result = 0;
			safe_strcpy(buffer, dirPath);
			ReadDir(id,result);
			safe_strcpy(dirPath, buffer);
			if (dirSearch[id]) {
				dirSearch[id]->id = MAX_OPENDIRS;
				dirSearch[id] = nullptr;
			}
		};
	};

	do {
		pos = strchr(start,CROSS_FILESPLIT);
		if (pos) {
			safe_strncpy(dir, start,
			             static_cast<unsigned int>(pos - start) < sizeof(dir) ? pos - start + 1 : sizeof(dir));
		}
		else {
			safe_strcpy(dir, start);
		};
 
		// Path found
		Bits nextDir = GetLongName(curDir, dir, sizeof(dir));
		strncat(expandedPath, dir, CROSS_LEN - strlen(expandedPath) - 1);

		// Error check
/*		if ((errorcheck) && (nextDir<0)) {
			LOG_DEBUG("DIR: Error: %s not found.",expandedPath);
		};
*/
		// Follow Directory
		if ((nextDir>=0) && curDir->fileList[nextDir]->isDir) {
			curDir = curDir->fileList[nextDir];
			safe_strcpy(curDir->orgname, dir);
			if (!IsCachedIn(curDir)) {
				if (OpenDir(curDir,expandedPath,id)) {
					char buffer[CROSS_LEN];
					char* result = 0;
					safe_strcpy(buffer, dirPath);
					ReadDir(id,result);
					safe_strcpy(dirPath, buffer);
					if (dirSearch[id]) {
						dirSearch[id]->id = MAX_OPENDIRS;
						dirSearch[id] = nullptr;
					}
				};
			}
		};
		if (pos) {
			strncat(expandedPath, split, CROSS_LEN - strlen(expandedPath) - 1);
			start = pos+1;
		}
	} while (pos);

	// Save last result for faster access next time
	safe_strcpy(save_path, path);
	safe_strcpy(save_expanded, expandedPath);
	save_dir = curDir;

	return curDir;
}

bool DOS_Drive_Cache::OpenDir(const char* path, uint16_t& id) {
	char expand[CROSS_LEN] = {0};
	CFileInfo* dir = FindDirInfo(path,expand);
	if (OpenDir(dir,expand,id)) {
		dirSearch[id]->nextEntry = 0;
		return true;
	}
	return false;
}

bool DOS_Drive_Cache::OpenDir(CFileInfo* dir, const char* expand, uint16_t& id) {
	id = GetFreeID(dir);
	dirSearch[id] = dir;
	char expandcopy [CROSS_LEN];
	safe_strcpy(expandcopy, expand);
	// Add "/"
	char end[2]={CROSS_FILESPLIT,0};
	const size_t expandcopylen = safe_strlen(expandcopy);
	if (expandcopylen > 0 && expandcopy[expandcopylen - 1] != CROSS_FILESPLIT) {
		safe_strcat(expandcopy, end);
	}
	// open dir
	if (dirSearch[id]) {
		// open dir
		dir_information* dirp = open_directory(expandcopy);
		if (dirp || dir->isOverlayDir) { 
			// Reset it..
			if (dirp) close_directory(dirp);
			safe_strcpy(dirPath, expandcopy);
			return true;
		}
		if (dirSearch[id]) {
			dirSearch[id]->id = MAX_OPENDIRS;
			dirSearch[id] = nullptr;
		}
	};
	return false;
}

void DOS_Drive_Cache::CreateEntry(CFileInfo* dir, const char* name, bool is_directory) {
	CFileInfo* info = new CFileInfo;
	safe_strcpy(info->orgname, name);
	info->shortNr = 0;
	info->isDir = is_directory;

	// Check for long filenames...
	CreateShortName(dir, info);		

	bool found = false;

	// keep list sorted (so GetLongName works correctly, used by CreateShortName in this routine)
	if (dir->fileList.size()>0) {
		if (!(strcmp(info->shortname,dir->fileList.back()->shortname)<0)) {
			// append at end of list
			dir->fileList.push_back(info);
		} else {
			// look for position where to insert this element
			std::vector<CFileInfo*>::iterator it;
			for (it=dir->fileList.begin(); it!=dir->fileList.end(); ++it) {
				if (strcmp(info->shortname,(*it)->shortname)<0) {
					found = true;
					break;
				}
			}
			// Put file in lists
			if (found) dir->fileList.insert(it,info);
			else dir->fileList.push_back(info);
		}
	} else {
		// empty file list, append
		dir->fileList.push_back(info);
	}
}

void DOS_Drive_Cache::CopyEntry(CFileInfo* dir, CFileInfo* from) {
	CFileInfo* info = new CFileInfo;
	// just copy things into new fileinfo
	safe_strcpy(info->orgname, from->orgname);
	safe_strcpy(info->shortname, from->shortname);
	info->shortNr = from->shortNr;
	info->isDir = from->isDir;

	dir->fileList.push_back(info);
}

bool DOS_Drive_Cache::ReadDir(uint16_t id, char* &result) {
	// shouldnt happen...
	if (id >= MAX_OPENDIRS)
		return false;

	if (!IsCachedIn(dirSearch[id])) {
		// Try to open directory
		dir_information* dirp = open_directory(dirPath);
		if (!dirp) {
			if (dirSearch[id]) {
				dirSearch[id]->id = MAX_OPENDIRS;
				dirSearch[id] = nullptr;
			}
			return false;
		}
		// Read complete directory
		char dir_name[CROSS_LEN];
		bool is_directory;
		if (read_directory_first(dirp, dir_name, is_directory)) {
			CreateEntry(dirSearch[id], dir_name, is_directory);
			while (read_directory_next(dirp, dir_name, is_directory)) {
				CreateEntry(dirSearch[id], dir_name, is_directory);
			}
		}

		// close dir
		close_directory(dirp);

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
	if (dirSearch[id]) {
		if (SetResult(dirSearch[id], result, dirSearch[id]->nextEntry))
			return true;
		dirSearch[id]->id = MAX_OPENDIRS;
		dirSearch[id] = nullptr;
	}
	return false;
}

bool DOS_Drive_Cache::SetResult(CFileInfo* dir, char* &result, Bitu entryNr)
{
	static char res[CROSS_LEN] = { 0 };

	result = res;
	if (entryNr>=dir->fileList.size()) return false;
	CFileInfo* info = dir->fileList[entryNr];
	// copy filename, short version
	safe_strcpy(res, info->shortname);
	// Set to next Entry
	dir->nextEntry = entryNr+1;
	return true;
}

// FindFirst / FindNext
bool DOS_Drive_Cache::FindFirst(char* path, uint16_t& id) {
	uint16_t	dirID;
	// Cache directory in 
	if (!OpenDir(path,dirID)) return false;

	//Find a free slot.
	//If the next one isn't free, move on to the next, if none is free => reset and assume the worst
	uint16_t local_findcounter = 0;
	while ( local_findcounter < MAX_OPENDIRS ) {
		if (dirFindFirst[this->nextFreeFindFirst] == nullptr) break;
		if (++this->nextFreeFindFirst >= MAX_OPENDIRS) this->nextFreeFindFirst = 0; //Wrap around
		local_findcounter++;
	}

	uint16_t	dirFindFirstID = this->nextFreeFindFirst++;
	if (this->nextFreeFindFirst >= MAX_OPENDIRS) this->nextFreeFindFirst = 0; //Increase and wrap around for the next search.

	if (local_findcounter == MAX_OPENDIRS) { //Here is the reset from above.
		// no free slot found...
		LOG(LOG_MISC,LOG_ERROR)("DIRCACHE: FindFirst/Next: All slots full. Resetting");
		// Clear the internal list then.
		dirFindFirstID = 0;
		this->nextFreeFindFirst = 1; //the next free one after this search
		for(Bitu n=0; n<MAX_OPENDIRS;n++) {	
	     	// Clear and reuse slot
			DeleteFileInfo(dirFindFirst[n]);
			dirFindFirst[n] = nullptr;
		}
	   
	}
	assert(dirFindFirst[dirFindFirstID] == nullptr);
	dirFindFirst[dirFindFirstID] = new CFileInfo();
	dirFindFirst[dirFindFirstID]->nextEntry = 0;

	// Copy entries to use with FindNext
	for (Bitu i=0; i<dirSearch[dirID]->fileList.size(); i++) {
		CopyEntry(dirFindFirst[dirFindFirstID],dirSearch[dirID]->fileList[i]);
	}
	// Now re-sort the fileList accordingly to output
	switch (sortDirType) {
		case ALPHABETICAL		: break;
//		case ALPHABETICAL		: std::sort(dirFindFirst[dirFindFirstID]->fileList.begin(), dirFindFirst[dirFindFirstID]->fileList.end(), SortByName);		break;
		case DIRALPHABETICAL	: std::sort(dirFindFirst[dirFindFirstID]->fileList.begin(), dirFindFirst[dirFindFirstID]->fileList.end(), SortByDirName);		break;
		case ALPHABETICALREV	: std::sort(dirFindFirst[dirFindFirstID]->fileList.begin(), dirFindFirst[dirFindFirstID]->fileList.end(), SortByNameRev);		break;
		case DIRALPHABETICALREV	: std::sort(dirFindFirst[dirFindFirstID]->fileList.begin(), dirFindFirst[dirFindFirstID]->fileList.end(), SortByDirNameRev);	break;
		case NOSORT				: break;
	}

//	LOG(LOG_MISC,LOG_ERROR)("DIRCACHE: FindFirst : %s (ID:%02X)",path,dirFindFirstID);
	id = dirFindFirstID;
	return true;
}

bool DOS_Drive_Cache::FindNext(uint16_t id, char* &result) {
	// out of range ?
	if ((id>=MAX_OPENDIRS) || !dirFindFirst[id]) {
		LOG(LOG_MISC,LOG_ERROR)("DIRCACHE: FindFirst/Next failure : ID out of range: %04X",id);
		return false;
	}
	if (!SetResult(dirFindFirst[id], result, dirFindFirst[id]->nextEntry)) {
		// free slot
		DeleteFileInfo(dirFindFirst[id]);
		dirFindFirst[id] = nullptr;
		return false;
	}
	return true;
}

void DOS_Drive_Cache::ClearFileInfo(CFileInfo *dir) {
	for(uint32_t i=0; i<dir->fileList.size(); i++) {
		if (CFileInfo *info = dir->fileList[i])
			ClearFileInfo(info);
	}
	if (dir->id != MAX_OPENDIRS) {
		dirSearch[dir->id] = nullptr;
		dir->id = MAX_OPENDIRS;
	}
}

void DOS_Drive_Cache::DeleteFileInfo(CFileInfo *dir) {
	if (dir) {
		ClearFileInfo(dir);
		delete dir;
	}
}
