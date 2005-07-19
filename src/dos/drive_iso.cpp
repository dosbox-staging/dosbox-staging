/*
 *  Copyright (C) 2002-2005  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: drive_iso.cpp,v 1.8 2005-07-19 19:45:31 qbix79 Exp $ */

#include <cctype>
#include <cstring>
#include "cdrom.h"
#include "dosbox.h"
#include "dos_system.h"
#include "support.h"
#include "drives.h"

using namespace std;

class isoFile : public DOS_File {
public:
	isoFile(isoDrive *drive, const char *name, FileStat_Block *stat, Bit32u offset, Bit16u info);
	bool Read(Bit8u *data, Bit16u *size);
	bool Write(Bit8u *data, Bit16u *size);
	bool Seek(Bit32u *pos, Bit32u type);
	bool Close();
	Bit16u GetInformation(void);
private:
	isoDrive *drive;
	Bit8u buffer[ISO_FRAMESIZE];
	int cachedSector;
	Bit32u fileBegin;
	Bit32u filePos;
	Bit32u fileEnd;
	Bit16u info;
};

isoFile::isoFile(isoDrive *drive, const char *name, FileStat_Block *stat, Bit32u offset, Bit16u info)
{
	this->drive = drive;
	time = stat->time;
	date = stat->date;
	attr = stat->attr;
	size = stat->size;
	fileBegin = offset;
	filePos = fileBegin;
	fileEnd = fileBegin + size;
	cachedSector = -1;
	open = true;
	info = info;
	this->name = NULL;
	SetName(name);
}

bool isoFile::Read(Bit8u *data, Bit16u *size)
{
	if (filePos + *size > fileEnd)
		*size = fileEnd - filePos;
	
	Bit16u nowSize = 0;
	int sector = filePos / ISO_FRAMESIZE;
	Bit16u sectorPos = filePos % ISO_FRAMESIZE;
	
	if (sector != cachedSector) {
		if (drive->readSector(buffer, sector)) cachedSector = sector;
		else { *size = 0; cachedSector = -1; }
	}
	while (nowSize < *size) {
		Bit16u remSector = ISO_FRAMESIZE - sectorPos;
		Bit16u remSize = *size - nowSize;
		if(remSector < remSize) {
			memcpy(&data[nowSize], &buffer[sectorPos], remSector);
			nowSize += remSector;
			sectorPos = 0;
			sector++;
			cachedSector++;
			if (!drive->readSector(buffer, sector)) {
				*size = nowSize;
				cachedSector = -1;
			}
		} else {
			memcpy(&data[nowSize], &buffer[sectorPos], remSize);
			nowSize += remSize;
		}
			
	}
	
	*size = nowSize;
	filePos += *size;
	return true;
}

bool isoFile::Write(Bit8u *data, Bit16u *size)
{
	return false;
}

bool isoFile::Seek(Bit32u *pos, Bit32u type)
{
	switch (type) {
		case DOS_SEEK_SET:
			filePos = fileBegin + *pos;
			break;
		case DOS_SEEK_CUR:
			filePos += *pos;
			break;
		case DOS_SEEK_END:
			filePos = fileEnd + *pos;
			break;
		default:
			return false;
	}
	if (filePos > fileEnd || filePos < fileBegin)
		filePos = fileEnd;
	
	*pos = filePos - fileBegin;
	return true;
}

bool isoFile::Close()
{
	if (refCtr == 1) open = false;
	return true;
}

Bit16u isoFile::GetInformation(void)
{
	return info;
}

int  MSCDEX_AddDrive(char driveLetter, const char* physicalPath, Bit8u& subUnit);
bool MSCDEX_HasMediaChanged(Bit8u subUnit);
bool MSCDEX_GetVolumeName(Bit8u subUnit, char* name);

isoDrive::isoDrive(char driveLetter, const char *fileName, Bit8u mediaid, int &error)
{
	error = MSCDEX_AddDrive(driveLetter, fileName, subUnit);

	if (!error) {
		if (loadImage()) {
			strcpy(info, "isoDrive");
			searchCache.clear();
			dirIter = searchCache.end();
			this->mediaid = mediaid;
			char buffer[32] = { 0 };
			if (!MSCDEX_GetVolumeName(subUnit, buffer)) strcpy(buffer, "");

			//Code Copied from drive_cache. (convert mscdex label to a dos 8.3 file)
			Bitu togo	= 8;
			Bitu bufPos	= 0;
			Bitu labelPos	= 0;
			bool point	= false;
			while (togo>0) {
				if (buffer[bufPos]==0) break;
				if (!point && (buffer[bufPos]=='.')) { togo=4; point=true; }
				discLabel[labelPos] = toupper(buffer[bufPos]);
				labelPos++; bufPos++;
				togo--;
				if ((togo==0) && !point) {
					if (buffer[bufPos]=='.') bufPos++;
					discLabel[labelPos]='.'; labelPos++; point=true; togo=3;
				}
			};
			discLabel[labelPos]=0;
			//Remove trailing dot.
			if((labelPos > 0) && (discLabel[labelPos - 1] == '.'))
				discLabel[labelPos - 1] = 0;
		} else error = 6;
	}
}

isoDrive::~isoDrive() { }

bool isoDrive::FileOpen(DOS_File **file, char *name, Bit32u flags)
{
	if (flags == OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	
	isoDirEntry de;
	bool success = lookup(&de, name) && !IS_DIR(de.fileFlags);

	if (success) {
		FileStat_Block file_stat;
		file_stat.size = DATA_LENGTH(de);
		file_stat.attr = DOS_ATTR_ARCHIVE | DOS_ATTR_READ_ONLY;
		file_stat.date = DOS_PackDate(1900 + de.dateYear, de.dateMonth, de.dateDay);
		file_stat.time = DOS_PackTime(de.timeHour, de.timeMin, de.timeSec);
		*file = new isoFile(this, name, &file_stat, EXTENT_LOCATION(de) * ISO_FRAMESIZE, 0x202);
		(*file)->flags = flags;
	}
	return success;
}

bool isoDrive::FileCreate(DOS_File **file, char *name, Bit16u attributes)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::FileUnlink(char *name)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::RemoveDir(char *dir)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::MakeDir(char *dir)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::TestDir(char *dir)
{
	isoDirEntry de;	
	return (lookup(&de, dir) && IS_DIR(de.fileFlags));
}

bool isoDrive::FindFirst(char *dir, DOS_DTA &dta, bool fcb_findfirst)
{
	isoDirEntry de;
	if (!lookup(&de, dir)) {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	
	Bit32u sectorStart = EXTENT_LOCATION(de);
	Bit32u sectorEnd = sectorStart + DATA_LENGTH(de) / ISO_FRAMESIZE;
	if (DATA_LENGTH(de) % ISO_FRAMESIZE != 0) sectorEnd++;
	searchCache.clear();
	
	for(Bit32u sector = sectorStart; sector < sectorEnd; sector++) {
		Bit8u block[ISO_FRAMESIZE];
		readSector(block, sector);
		
		Bit32u pos = 0;
		while (pos < ISO_FRAMESIZE && block[pos] != 0 && (pos + block[pos]) <= ISO_FRAMESIZE) {
			isoDirEntry tmp;
			int length = readDirEntry(&tmp, &block[pos]);
			if (length < 0) return false;
			searchCache.push_back(tmp);
			pos += length;
		}
	}
	dirIter = searchCache.begin();

	Bit8u attr;
	char pattern[ISO_MAXPATHNAME];
	dta.GetSearchParams(attr, pattern);
	if ((attr & DOS_ATTR_VOLUME) && ((*dir == 0) || fcb_findfirst)) {
		// Get Volume Label (DOS_ATTR_VOLUME) and only in basedir
		dta.SetResult(discLabel, 0, 0, 0, DOS_ATTR_VOLUME);
		return true;
	}
	return FindNext(dta);
}

bool isoDrive::FindNext(DOS_DTA &dta)
{
	Bit8u attr;
	char pattern[DOS_NAMELENGTH_ASCII];
	dta.GetSearchParams(attr, pattern);
	
	while (dirIter != searchCache.end()) {
		isoDirEntry &de = *dirIter;
		Bit8u findAttr = 0;
		if (IS_DIR(de.fileFlags)) findAttr |= DOS_ATTR_DIRECTORY;
		else findAttr |= DOS_ATTR_ARCHIVE;
		if (IS_HIDDEN(de.fileFlags)) findAttr |= DOS_ATTR_HIDDEN;

		if (WildFileCmp((char*)de.ident, pattern)
			&& !(~attr & findAttr & (DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM))) {
			
			/* file is okay, setup everything to be copied in DTA Block */
			char findName[DOS_NAMELENGTH_ASCII];		
			if(strlen((char*)de.ident) < DOS_NAMELENGTH_ASCII) {
				strcpy(findName, (char*)de.ident);
				upcase(findName);
			}
			Bit32u findSize = DATA_LENGTH(de);
			Bit16u findDate = DOS_PackDate(1900 + de.dateYear, de.dateMonth, de.dateDay);
			Bit16u findTime = DOS_PackTime(de.timeHour, de.timeMin, de.timeSec);
			dta.SetResult(findName, findSize, findDate, findTime, findAttr);
			
			dirIter++;		
			return true;
		}
		dirIter++;
	}
	
	DOS_SetError(DOSERR_NO_MORE_FILES);
	return false;
}

bool isoDrive::Rename(char *oldname, char *newname)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::GetFileAttr(char *name, Bit16u *attr)
{
	*attr = 0;
	isoDirEntry de;
	bool success = lookup(&de, name);
	if (success) {
		*attr = DOS_ATTR_ARCHIVE | DOS_ATTR_READ_ONLY;
		if (IS_DIR(de.fileFlags)) *attr |= DOS_ATTR_DIRECTORY;
	}
	return success;
}

bool isoDrive::AllocationInfo(Bit16u *bytes_sector, Bit8u *sectors_cluster, Bit16u *total_clusters, Bit16u *free_clusters)
{
	*bytes_sector = 2048;
	*sectors_cluster = 1; // cluster size for cdroms ?
	*total_clusters = 60000;
	*free_clusters = 0;
	return true;
}

bool isoDrive::FileExists(const char *name)
{
	isoDirEntry de;
	return (lookup(&de, name) && !IS_DIR(de.fileFlags));
}

bool isoDrive::FileStat(const char *name, FileStat_Block *const stat_block)
{
	isoDirEntry de;
	bool success = lookup(&de, name);
	
	if (success) {
		stat_block->date = DOS_PackDate(1900 + de.dateYear, de.dateMonth, de.dateDay);
		stat_block->time = DOS_PackTime(de.timeHour, de.timeMin, de.timeSec);
		stat_block->size = DATA_LENGTH(de);
		stat_block->attr = DOS_ATTR_ARCHIVE | DOS_ATTR_READ_ONLY;
		if (IS_DIR(de.fileFlags)) stat_block->attr |= DOS_ATTR_DIRECTORY;
	}
	
	return success;
}

Bit8u isoDrive::GetMediaByte(void)
{
	return mediaid;
}

bool isoDrive::isRemote(void)
{
	return true;
}

bool isoDrive::isRemovable(void)
{
	return true;
}

inline bool isoDrive :: readSector(Bit8u *buffer, Bit32u sector)
{
	return CDROM_Interface_Image::images[subUnit]->ReadSector(buffer, false, sector);
}

int isoDrive :: readDirEntry(isoDirEntry *de, Bit8u *data)
{	
	// copy data into isoDirEntry struct, data[0] = length of DirEntry
	if (data[0] > sizeof(isoDirEntry)) return -1;
	memcpy(de, data, data[0]);
	
	// xa not supported
	if (de->extAttrLength != 0) return -1;
	// interleaved mode not supported
	if (de->fileUnitSize != 0 || de->interleaveGapSize != 0) return -1;
	
	// modify file identifier for use with dosbox
	if ((de->length < 33 + de->fileIdentLength)) return -1;
	if (IS_DIR(de->fileFlags)) {
		if (de->fileIdentLength == 1 && de->ident[0] == 0) strcpy((char*)de->ident, ".");
		else if (de->fileIdentLength == 1 && de->ident[0] == 1) strcpy((char*)de->ident, "..");
		else {
			if (de->fileIdentLength > 31) return -1;
			de->ident[de->fileIdentLength] = 0;
		}
	} else {
		if (de->fileIdentLength > 37) return -1;
		de->ident[de->fileIdentLength] = 0;	
		// remove any file version identifiers as there are some cdroms that don't have them
		strreplace((char*)de->ident, ';', 0);	
		// if file has no extension remove the trailing dot
		int tmp = strlen((char*)de->ident);
		if (tmp > 0 && de->ident[tmp - 1] == '.') de->ident[tmp - 1] = 0;
	}
	return de->length;
}

bool isoDrive :: loadImage()
{
	isoPVD pvd;
	readSector((Bit8u*)(&pvd), ISO_FIRST_VD);
	if (pvd.type != 1 || strncmp((char*)pvd.standardIdent, "CD001", 5) || pvd.version != 1) return false;
	return (readDirEntry(&this->rootEntry, pvd.rootEntry));
}

bool isoDrive :: lookupSingle(isoDirEntry *de, const char *name, Bit32u start, Bit32u length)
{
	Bit32u end = start + length / ISO_FRAMESIZE;
	if (length % ISO_FRAMESIZE != 0) end++;
	
	for(Bit32u i = start; i < end; i++) {
		Bit8u sector[ISO_FRAMESIZE];
		if (!readSector(sector, i)) return false;
		
		int pos = 0;
		while (pos < ISO_FRAMESIZE && sector[pos] != 0 && (pos + sector[pos]) <= ISO_FRAMESIZE) {
			int deLength = readDirEntry(de, &sector[pos]);
			if (deLength < 1) return false;
			pos += deLength;
			int tmp = strncasecmp((char*)de->ident, name, 38);
			if (tmp == 0) return true;
		}
	}
	return false;	
}

bool isoDrive :: lookup(isoDirEntry *de, const char *path)
{
	*de = this->rootEntry;
	if (!strcmp(path, "")) return true;
	
	char isoPath[ISO_MAXPATHNAME];
	safe_strncpy(isoPath, path, ISO_MAXPATHNAME);
	strreplace(isoPath, '\\', '/');
	
	int beginPos = 0;
	int pos = 0;
	while (isoPath[pos] != 0) {
		if (isoPath[pos] == '/') {
			char name[38];
			if (pos - beginPos >= 38) return false;
			if (beginPos >= ISO_MAXPATHNAME) return false;
			safe_strncpy(name, &isoPath[beginPos], pos - beginPos + 1);
			beginPos = pos + 1;
			if (!IS_DIR(de->fileFlags)) return false;
			if (!lookupSingle(de, name, EXTENT_LOCATION(*de), DATA_LENGTH(*de))) return false;
		}
		pos++;
	}
	return lookupSingle(de, &isoPath[beginPos], EXTENT_LOCATION(*de), DATA_LENGTH(*de));
}
