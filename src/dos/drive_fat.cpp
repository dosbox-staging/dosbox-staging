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

#include "drives.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bios_disk.h"
#include "bios.h"
#include "cross.h"
#include "dos_inc.h"
#include "string_utils.h"
#include "support.h"

#define IMGTYPE_FLOPPY 0
#define IMGTYPE_ISO    1
#define IMGTYPE_HDD	   2

#define FAT12		   0
#define FAT16		   1
#define FAT32		   2

class fatFile final : public DOS_File {
public:
	fatFile(const char* name, uint32_t startCluster, uint32_t fileLen, fatDrive *useDrive);
	fatFile(const fatFile&) = delete; // prevent copy
	fatFile& operator=(const fatFile&) = delete; // prevent assignment
	bool Read(uint8_t * data,uint16_t * size);
	bool Write(uint8_t * data,uint16_t * size);
	bool Seek(uint32_t * pos,uint32_t type);
	bool Close();
	uint16_t GetInformation(void);
	bool UpdateDateTimeFromHost(void);   
public:
	uint32_t firstCluster;
	uint32_t seekpos;
	uint32_t filelength;
	uint32_t currentSector;
	uint32_t curSectOff;
	uint8_t sectorBuffer[512];
	/* Record of where in the directory structure this file is located */
	uint32_t dirCluster;
	uint32_t dirIndex;

	bool loadedSector;
	fatDrive *myDrive;
};


/* IN - char * filename: Name in regular filename format, e.g. bob.txt */
/* OUT - char * filearray: Name in DOS directory format, eleven char, e.g. bob     txt */
static void convToDirFile(char *filename, char *filearray) {
	uint32_t charidx = 0;
	uint32_t flen,i;
	flen = (uint32_t)strnlen(filename, DOS_NAMELENGTH_ASCII);
	memset(filearray, 32, 11);
	for(i=0;i<flen;i++) {
		if(charidx >= 11) break;
		if(filename[i] != '.') {
			filearray[charidx] = filename[i];
			charidx++;
		} else {
			charidx = 8;
		}
	}
}

fatFile::fatFile(const char* /*name*/,
                 uint32_t startCluster,
                 uint32_t fileLen,
                 fatDrive *useDrive)
	: firstCluster(startCluster),
	  seekpos(0),
	  filelength(fileLen),
	  currentSector(0),
	  curSectOff(0),
	  sectorBuffer{0},
	  dirCluster(0),
	  dirIndex(0),
	  loadedSector(false),
	  myDrive(useDrive)
{
	uint32_t seekto = 0;
	open = true;
	if(filelength > 0) {
		Seek(&seekto, DOS_SEEK_SET);
	}
}

bool fatFile::Read(uint8_t * data, uint16_t *size) {
	if ((this->flags & 0xf) == OPEN_WRITE) {	// check if file opened in write-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	uint16_t sizedec, sizecount;
	if(seekpos >= filelength) {
		*size = 0;
		return true;
	}

	if (!loadedSector) {
		currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
		if(currentSector == 0) {
			/* EOC reached before EOF */
			*size = 0;
			loadedSector = false;
			return true;
		}
		curSectOff = seekpos % myDrive->getSectorSize();
		myDrive->readSector(currentSector, sectorBuffer);
		loadedSector = true;
	}

	sizedec = *size;
	sizecount = 0;
	while(sizedec != 0) {
		if(seekpos >= filelength) {
			*size = sizecount;
			return true; 
		}
		data[sizecount++] = sectorBuffer[curSectOff++];
		seekpos++;
		if(curSectOff >= myDrive->getSectorSize()) {
			currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
			if(currentSector == 0) {
				/* EOC reached before EOF */
				//LOG_MSG("EOC reached before EOF, seekpos %d, filelen %d", seekpos, filelength);
				*size = sizecount;
				loadedSector = false;
				return true;
			}
			curSectOff = 0;
			myDrive->readSector(currentSector, sectorBuffer);
			loadedSector = true;
			//LOG_MSG("Reading absolute sector at %d for seekpos %d", currentSector, seekpos);
		}
		--sizedec;
	}
	*size =sizecount;
	return true;
}

bool fatFile::Write(uint8_t * data, uint16_t *size) {
	if ((this->flags & 0xf) == OPEN_READ) {	// check if file opened in read-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	direntry tmpentry;
	uint16_t sizedec, sizecount;
	sizedec = *size;
	sizecount = 0;

	if(seekpos < filelength && *size == 0) {
		/* Truncate file to current position */
		if (firstCluster != 0)
			myDrive->deleteClustChain(firstCluster, seekpos);
		if (seekpos == 0)
			firstCluster = 0;
		filelength = seekpos;
		goto finalizeWrite;
	}

	if(seekpos > filelength) {
		/* Extend file to current position */
		uint32_t clustSize = myDrive->getClusterSize();
		if(filelength == 0) {
			firstCluster = myDrive->getFirstFreeClust();
			if(firstCluster == 0) goto finalizeWrite; // out of space
			myDrive->allocateCluster(firstCluster, 0);
			filelength = clustSize;
		}
		filelength = ((filelength - 1) / clustSize + 1) * clustSize;
		while(filelength < seekpos) {
			if(myDrive->appendCluster(firstCluster) == 0) goto finalizeWrite; // out of space
			filelength += clustSize;
		}
		if(filelength > seekpos) filelength = seekpos;
		if(*size == 0) goto finalizeWrite;
	}

	while(sizedec != 0) {
		/* Increase filesize if necessary */
		if(seekpos >= filelength) {
			if(filelength == 0) {
				firstCluster = myDrive->getFirstFreeClust();
				if(firstCluster == 0) goto finalizeWrite; // out of space
				myDrive->allocateCluster(firstCluster, 0);
				currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
				myDrive->readSector(currentSector, sectorBuffer);
				loadedSector = true;
			}
			if (!loadedSector) {
				currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
				if(currentSector == 0) {
					/* EOC reached before EOF - try to increase file allocation */
					myDrive->appendCluster(firstCluster);
					/* Try getting sector again */
					currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
					if(currentSector == 0) {
						/* No can do. lets give up and go home.  We must be out of room */
						goto finalizeWrite;
					}
				}
				curSectOff = seekpos % myDrive->getSectorSize();
				myDrive->readSector(currentSector, sectorBuffer);
				loadedSector = true;
			}
			filelength = seekpos+1;
		}
		sectorBuffer[curSectOff++] = data[sizecount++];
		seekpos++;
		if(curSectOff >= myDrive->getSectorSize()) {
			if(loadedSector) myDrive->writeSector(currentSector, sectorBuffer);

			currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
			if(currentSector == 0) loadedSector = false;
			else {
				curSectOff = 0;
				myDrive->readSector(currentSector, sectorBuffer);
				loadedSector = true;
			}
		}
		--sizedec;
	}
	if(curSectOff>0 && loadedSector) myDrive->writeSector(currentSector, sectorBuffer);

finalizeWrite:
	myDrive->directoryBrowse(dirCluster, &tmpentry, dirIndex);
	tmpentry.entrysize = filelength;
	tmpentry.loFirstClust = (uint16_t)firstCluster;
	myDrive->directoryChange(dirCluster, &tmpentry, dirIndex);

	*size =sizecount;
	return true;
}

bool fatFile::Seek(uint32_t *pos, uint32_t type) {
	int32_t seekto=0;
	
	switch(type) {
		case DOS_SEEK_SET:
			seekto = (int32_t)*pos;
			break;
		case DOS_SEEK_CUR:
			/* Is this relative seek signed? */
			seekto = (int32_t)*pos + (int32_t)seekpos;
			break;
		case DOS_SEEK_END:
			seekto = (int32_t)filelength + (int32_t)*pos;
			break;
	}
//	LOG_MSG("Seek to %d with type %d (absolute value %d)", *pos, type, seekto);

	if(seekto<0) seekto = 0;
	seekpos = (uint32_t)seekto;
	currentSector = myDrive->getAbsoluteSectFromBytePos(firstCluster, seekpos);
	if (currentSector == 0) {
		/* not within file size, thus no sector is available */
		loadedSector = false;
	} else {
		curSectOff = seekpos % myDrive->getSectorSize();
		myDrive->readSector(currentSector, sectorBuffer);
		loadedSector = true;
	}
	*pos = seekpos;
	return true;
}

bool fatFile::Close() {
	/* Flush buffer */
	if (loadedSector) myDrive->writeSector(currentSector, sectorBuffer);

	return false;
}

uint16_t fatFile::GetInformation(void) {
	return 0;
}

bool fatFile::UpdateDateTimeFromHost(void) {
	return true;
}

uint32_t fatDrive::getClustFirstSect(uint32_t clustNum) {
	return ((clustNum - 2) * bootbuffer.sectorspercluster) + firstDataSector;
}

uint32_t fatDrive::getClusterValue(uint32_t clustNum) {
	uint32_t fatoffset=0;
	uint32_t fatsectnum;
	uint32_t fatentoff;
	uint32_t clustValue=0;

	switch(fattype) {
		case FAT12:
			fatoffset = clustNum + (clustNum / 2);
			break;
		case FAT16:
			fatoffset = clustNum * 2;
			break;
		case FAT32:
			fatoffset = clustNum * 4;
			break;
	}
	fatsectnum = bootbuffer.reservedsectors + (fatoffset / bootbuffer.bytespersector) + partSectOff;
	fatentoff = fatoffset % bootbuffer.bytespersector;

	if(curFatSect != fatsectnum) {
		/* Load two sectors at once for FAT12 */
		readSector(fatsectnum, &fatSectBuffer[0]);
		if (fattype==FAT12)
			readSector(fatsectnum+1, &fatSectBuffer[512]);
		curFatSect = fatsectnum;
	}

	switch(fattype) {
		case FAT12:
			clustValue = var_read((uint16_t *)&fatSectBuffer[fatentoff]);
			if(clustNum & 0x1) {
				clustValue >>= 4;
			} else {
				clustValue &= 0xfff;
			}
			break;
		case FAT16:
			clustValue = var_read((uint16_t *)&fatSectBuffer[fatentoff]);
			break;
		case FAT32:
			clustValue = var_read((uint32_t *)&fatSectBuffer[fatentoff]);
			break;
	}

	return clustValue;
}

void fatDrive::setClusterValue(uint32_t clustNum, uint32_t clustValue) {
	uint32_t fatoffset=0;
	uint32_t fatsectnum;
	uint32_t fatentoff;

	switch(fattype) {
		case FAT12:
			fatoffset = clustNum + (clustNum / 2);
			break;
		case FAT16:
			fatoffset = clustNum * 2;
			break;
		case FAT32:
			fatoffset = clustNum * 4;
			break;
	}
	fatsectnum = bootbuffer.reservedsectors + (fatoffset / bootbuffer.bytespersector) + partSectOff;
	fatentoff = fatoffset % bootbuffer.bytespersector;

	if(curFatSect != fatsectnum) {
		/* Load two sectors at once for FAT12 */
		readSector(fatsectnum, &fatSectBuffer[0]);
		if (fattype==FAT12)
			readSector(fatsectnum+1, &fatSectBuffer[512]);
		curFatSect = fatsectnum;
	}

	switch(fattype) {
		case FAT12: {
			uint16_t tmpValue = var_read((uint16_t *)&fatSectBuffer[fatentoff]);
			if(clustNum & 0x1) {
				clustValue &= 0xfff;
				clustValue <<= 4;
				tmpValue &= 0xf;
				tmpValue |= (uint16_t)clustValue;

			} else {
				clustValue &= 0xfff;
				tmpValue &= 0xf000;
				tmpValue |= (uint16_t)clustValue;
			}
			var_write((uint16_t *)&fatSectBuffer[fatentoff], tmpValue);
			break;
			}
		case FAT16:
			var_write((uint16_t *)&fatSectBuffer[fatentoff], (uint16_t)clustValue);
			break;
		case FAT32:
			var_write((uint32_t *)&fatSectBuffer[fatentoff], clustValue);
			break;
	}
	for(int fc=0;fc<bootbuffer.fatcopies;fc++) {
		writeSector(fatsectnum + (fc * bootbuffer.sectorsperfat), &fatSectBuffer[0]);
		if (fattype==FAT12) {
			if (fatentoff>=511)
				writeSector(fatsectnum+1+(fc * bootbuffer.sectorsperfat), &fatSectBuffer[512]);
		}
	}
}

bool fatDrive::getEntryName(char *fullname, char *entname) {
	char dirtoken[DOS_PATHLENGTH];

	char * findDir;
	char * findFile;
	safe_strcpy(dirtoken, fullname);

	//LOG_MSG("Testing for filename %s", fullname);
	findDir = strtok(dirtoken,"\\");
	if (findDir==NULL) {
		return true;	// root always exists
	}
	findFile = findDir;
	while(findDir != NULL) {
		findFile = findDir;
		findDir = strtok(NULL,"\\");
	}

	assert(entname);
	entname[0] = '\0';
	strncat(entname, findFile, DOS_NAMELENGTH_ASCII - 1);
	return true;
}

bool fatDrive::getFileDirEntry(char const * const filename, direntry * useEntry, uint32_t * dirClust, uint32_t * subEntry, const bool dir_ok) {
	size_t len = strnlen(filename, DOS_PATHLENGTH);
	char dirtoken[DOS_PATHLENGTH];
	uint32_t currentClust = 0;

	direntry foundEntry;
	char * findDir;
	char * findFile;
	safe_strcpy(dirtoken, filename);
	findFile=dirtoken;

	/* Skip if testing in root directory */
	if ((len>0) && (filename[len-1]!='\\')) {
		//LOG_MSG("Testing for filename %s", filename);
		findDir = strtok(dirtoken,"\\");
		findFile = findDir;
		while(findDir != NULL) {
			imgDTA->SetupSearch(0,DOS_ATTR_DIRECTORY,findDir);
			imgDTA->SetDirID(0);
			
			findFile = findDir;
			if(!FindNextInternal(currentClust, *imgDTA, &foundEntry)) break;
			else {
				//Found something. See if it's a directory (findfirst always finds regular files)
				char find_name[DOS_NAMELENGTH_ASCII];uint16_t find_date,find_time;uint32_t find_size;uint8_t find_attr;
				imgDTA->GetResult(find_name,find_size,find_date,find_time,find_attr);
				if(!(find_attr & DOS_ATTR_DIRECTORY)) break;
				char *findNext;
				findNext = strtok(NULL, "\\");
				if (findNext == NULL && dir_ok)
					break;
				findDir = findNext;
			}

			currentClust = foundEntry.loFirstClust;
		}
	} else {
		/* Set to root directory */
	}

	/* Search found directory for our file */
	imgDTA->SetupSearch(0,0x7 | (dir_ok ? DOS_ATTR_DIRECTORY : 0),findFile);
	imgDTA->SetDirID(0);
	if(!FindNextInternal(currentClust, *imgDTA, &foundEntry)) return false;

	memcpy(useEntry, &foundEntry, sizeof(direntry));
	*dirClust = currentClust;
	*subEntry = ((uint32_t)imgDTA->GetDirID()-1);
	return true;
}

bool fatDrive::getDirClustNum(char *dir, uint32_t *clustNum, bool parDir) {
	uint32_t len = (uint32_t)strnlen(dir, DOS_PATHLENGTH);
	char dirtoken[DOS_PATHLENGTH];
	uint32_t currentClust = 0;
	direntry foundEntry;
	char * findDir;
	safe_strcpy(dirtoken, dir);

	/* Skip if testing for root directory */
	if ((len>0) && (dir[len-1]!='\\')) {
		//LOG_MSG("Testing for dir %s", dir);
		findDir = strtok(dirtoken,"\\");
		while(findDir != NULL) {
			imgDTA->SetupSearch(0,DOS_ATTR_DIRECTORY,findDir);
			imgDTA->SetDirID(0);
			findDir = strtok(NULL,"\\");
			if(parDir && (findDir == NULL)) break;

			char find_name[DOS_NAMELENGTH_ASCII];uint16_t find_date,find_time;uint32_t find_size;uint8_t find_attr;
			if(!FindNextInternal(currentClust, *imgDTA, &foundEntry)) {
				return false;
			} else {
				imgDTA->GetResult(find_name,find_size,find_date,find_time,find_attr);
				if(!(find_attr &DOS_ATTR_DIRECTORY)) return false;
			}
			currentClust = foundEntry.loFirstClust;

		}
		*clustNum = currentClust;
	} else {
		/* Set to root directory */
		*clustNum = 0;
	}
	return true;
}

uint8_t fatDrive::readSector(uint32_t sectnum, void * data) {
	// Guard
	if (!loadedDisk) {
		return 0;
	}

	if (absolute) {
		return loadedDisk->Read_AbsoluteSector(sectnum, data);
	}
	uint32_t cylindersize = bootbuffer.headcount * bootbuffer.sectorspertrack;
	uint32_t cylinder = sectnum / cylindersize;
	sectnum %= cylindersize;
	uint32_t head = sectnum / bootbuffer.sectorspertrack;
	uint32_t sector = sectnum % bootbuffer.sectorspertrack + 1L;
	return loadedDisk->Read_Sector(head, cylinder, sector, data);
}

uint8_t fatDrive::writeSector(uint32_t sectnum, void * data) {
	// Guard
	if (!loadedDisk) {
		return 0;
	}

	if (absolute) {
		return loadedDisk->Write_AbsoluteSector(sectnum, data);
	}
	uint32_t cylindersize = bootbuffer.headcount * bootbuffer.sectorspertrack;
	uint32_t cylinder = sectnum / cylindersize;
	sectnum %= cylindersize;
	uint32_t head = sectnum / bootbuffer.sectorspertrack;
	uint32_t sector = sectnum % bootbuffer.sectorspertrack + 1L;
	return loadedDisk->Write_Sector(head, cylinder, sector, data);
}

uint32_t fatDrive::getSectorCount()
{
	if (bootbuffer.totalsectorcount != 0)
		return check_cast<uint32_t>(bootbuffer.totalsectorcount);
	else
		return bootbuffer.totalsecdword;
}

uint32_t fatDrive::getSectorSize(void)
{
	return bootbuffer.bytespersector;
}

uint32_t fatDrive::getClusterSize(void) {
	return bootbuffer.sectorspercluster * bootbuffer.bytespersector;
}

uint32_t fatDrive::getAbsoluteSectFromBytePos(uint32_t startClustNum, uint32_t bytePos) {
	return  getAbsoluteSectFromChain(startClustNum, bytePos / bootbuffer.bytespersector);
}

uint32_t fatDrive::getAbsoluteSectFromChain(uint32_t startClustNum, uint32_t logicalSector) {
	int32_t skipClust = logicalSector / bootbuffer.sectorspercluster;
	uint32_t sectClust = logicalSector % bootbuffer.sectorspercluster;

	uint32_t currentClust = startClustNum;
	uint32_t testvalue;

	while(skipClust!=0) {
		bool isEOF = false;
		testvalue = getClusterValue(currentClust);
		switch(fattype) {
			case FAT12:
				if(testvalue >= 0xff8) isEOF = true;
				break;
			case FAT16:
				if(testvalue >= 0xfff8) isEOF = true;
				break;
			case FAT32:
				if(testvalue >= 0xfffffff8) isEOF = true;
				break;
		}
		if((isEOF) && (skipClust>=1)) {
			//LOG_MSG("End of cluster chain reached before end of logical sector seek!");
			if (skipClust == 1 && fattype == FAT12) {
				//break;
				LOG(LOG_DOSMISC, LOG_ERROR)("End of cluster chain reached, but maybe good after all ?");
			}
			return 0;
		}
		currentClust = testvalue;
		--skipClust;
	}

	return (getClustFirstSect(currentClust) + sectClust);
}

void fatDrive::deleteClustChain(uint32_t startCluster, uint32_t bytePos) {
	uint32_t clustSize = getClusterSize();
	uint32_t endClust = (bytePos + clustSize - 1) / clustSize;
	uint32_t countClust = 1;

	uint32_t testvalue;
	uint32_t currentClust = startCluster;
	bool isEOF = false;
	while(!isEOF) {
		testvalue = getClusterValue(currentClust);
		if(testvalue == 0) {
			/* What the crap?  Cluster is already empty - BAIL! */
			break;
		}
		switch(fattype) {
			case FAT12:
				if(testvalue >= 0xff8) isEOF = true;
				break;
			case FAT16:
				if(testvalue >= 0xfff8) isEOF = true;
				break;
			case FAT32:
				if(testvalue >= 0xfffffff8) isEOF = true;
				break;
		}
		if(countClust == endClust && !isEOF) {
			/* Mark cluster as end */
			switch(fattype) {
				case FAT12:
					setClusterValue(currentClust, 0xfff);
					break;
				case FAT16:
					setClusterValue(currentClust, 0xffff);
					break;
				case FAT32:
					setClusterValue(currentClust, 0xffffffff);
					break;
			}
		} else if(countClust > endClust) {
			/* Mark cluster as empty */
			setClusterValue(currentClust, 0);
		}
		if(isEOF) break;
		currentClust = testvalue;
		countClust++;
	}
}

uint32_t fatDrive::appendCluster(uint32_t startCluster) {
	uint32_t testvalue;
	uint32_t currentClust = startCluster;
	bool isEOF = false;
	
	while(!isEOF) {
		testvalue = getClusterValue(currentClust);
		switch(fattype) {
			case FAT12:
				if(testvalue >= 0xff8) isEOF = true;
				break;
			case FAT16:
				if(testvalue >= 0xfff8) isEOF = true;
				break;
			case FAT32:
				if(testvalue >= 0xfffffff8) isEOF = true;
				break;
		}
		if(isEOF) break;
		currentClust = testvalue;
	}

	uint32_t newClust = getFirstFreeClust();
	/* Drive is full */
	if(newClust == 0) return 0;

	if(!allocateCluster(newClust, currentClust)) return 0;

	zeroOutCluster(newClust);

	return newClust;
}

bool fatDrive::allocateCluster(uint32_t useCluster, uint32_t prevCluster) {

	/* Can't allocate cluster #0 */
	if(useCluster == 0) return false;

	if(prevCluster != 0) {
		/* Refuse to allocate cluster if previous cluster value is zero (unallocated) */
		if(!getClusterValue(prevCluster)) return false;

		/* Point cluster to new cluster in chain */
		setClusterValue(prevCluster, useCluster);
		//LOG_MSG("Chaining cluser %d to %d", prevCluster, useCluster);
	} 

	switch(fattype) {
		case FAT12:
			setClusterValue(useCluster, 0xfff);
			break;
		case FAT16:
			setClusterValue(useCluster, 0xffff);
			break;
		case FAT32:
			setClusterValue(useCluster, 0xffffffff);
			break;
	}
	return true;
}

fatDrive::fatDrive(const char *sysFilename,
                   uint32_t bytesector,
                   uint32_t cylsector,
                   uint32_t headscyl,
                   uint32_t cylinders,
                   uint32_t startSector,
                   bool roflag)
	: loadedDisk(nullptr),
	  created_successfully(true),
	  partSectOff(0),
	  bootbuffer{{0}, {0}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0}, 0, 0},
	  absolute(false),
	  readonly(roflag),
	  fattype(0),
	  CountOfClusters(0),
	  firstDataSector(0),
	  firstRootDirSect(0),
	  cwdDirCluster(0),
	  fatSectBuffer{0},
	  curFatSect(0)
{
	FILE *diskfile;
	uint32_t filesize;
	bool is_hdd;
	struct partTable mbrData;
	
	if(imgDTASeg == 0) {
		imgDTASeg = DOS_GetMemory(2);
		imgDTAPtr = RealMake(imgDTASeg, 0);
		imgDTA    = new DOS_DTA(imgDTAPtr);
	}
	diskfile = fopen_wrap_ro_fallback(sysFilename, readonly);
	created_successfully = (diskfile != nullptr);
	if (!created_successfully)
		return;
	fseek(diskfile, 0L, SEEK_END);
	filesize = (uint32_t)ftell(diskfile) / 1024L;
	is_hdd = (filesize > 2880);

	/* Load disk image */
	loadedDisk.reset(new imageDisk(diskfile, sysFilename, filesize, is_hdd));

	if(is_hdd) {
		/* Set user specified harddrive parameters */
		loadedDisk->Set_Geometry(headscyl, cylinders,cylsector, bytesector);

		loadedDisk->Read_Sector(0,0,1,&mbrData);

		if(mbrData.magic1!= 0x55 ||	mbrData.magic2!= 0xaa) LOG_MSG("Possibly invalid partition table in disk image.");

		startSector = 63;
		int m;
		for(m=0;m<4;m++) {
			/* Pick the first available partition */
			if(mbrData.pentry[m].partSize != 0x00) {
				mbrData.pentry[m].absSectStart = host_to_le(mbrData.pentry[m].absSectStart);
				mbrData.pentry[m].partSize     = host_to_le(mbrData.pentry[m].partSize);
				LOG_MSG("Using partition %d on drive; skipping %d sectors", m, mbrData.pentry[m].absSectStart);
				startSector = mbrData.pentry[m].absSectStart;
				break;
			}
		}

		if(m==4) LOG_MSG("No good partition found in image.");

		partSectOff = startSector;
	} else {
		/* Get floppy disk parameters based on image size */
		loadedDisk->Get_Geometry(&headscyl, &cylinders, &cylsector, &bytesector);
		/* Floppy disks don't have partitions */
		partSectOff = 0;
	}

	if (bytesector != 512) {
		/* Non-standard sector sizes not implemented */
		created_successfully = false;
		return;
	}

	loadedDisk->Read_AbsoluteSector(0+partSectOff,&bootbuffer);

	bootbuffer.bytespersector    = host_to_le(bootbuffer.bytespersector);
	bootbuffer.reservedsectors   = host_to_le(bootbuffer.reservedsectors);
	bootbuffer.rootdirentries    = host_to_le(bootbuffer.rootdirentries);
	bootbuffer.totalsectorcount  = host_to_le(bootbuffer.totalsectorcount);
	bootbuffer.sectorsperfat     = host_to_le(bootbuffer.sectorsperfat);
	bootbuffer.sectorspertrack   = host_to_le(bootbuffer.sectorspertrack);
	bootbuffer.headcount         = host_to_le(bootbuffer.headcount);
	bootbuffer.hiddensectorcount = host_to_le(bootbuffer.hiddensectorcount);
	bootbuffer.totalsecdword     = host_to_le(bootbuffer.totalsecdword);

	if (!is_hdd) {
		/* Identify floppy format */
		if ((bootbuffer.nearjmp[0] == 0x69 || bootbuffer.nearjmp[0] == 0xe9 ||
			(bootbuffer.nearjmp[0] == 0xeb && bootbuffer.nearjmp[2] == 0x90)) &&
			(bootbuffer.mediadescriptor & 0xf0) == 0xf0) {
			/* DOS 2.x or later format, BPB assumed valid */

			if ((bootbuffer.mediadescriptor != 0xf0 && !(bootbuffer.mediadescriptor & 0x1)) &&
				(bootbuffer.oemname[5] != '3' || bootbuffer.oemname[6] != '.' || bootbuffer.oemname[7] < '2')) {
				/* Fix pre-DOS 3.2 single-sided floppy */
				bootbuffer.sectorspercluster = 1;
			}
		} else {
			/* Read media descriptor in FAT */
			uint8_t sectorBuffer[512];
			loadedDisk->Read_AbsoluteSector(1,&sectorBuffer);
			uint8_t mdesc = sectorBuffer[0];

			if (mdesc >= 0xf8) {
				/* DOS 1.x format, create BPB for 160kB floppy */
				bootbuffer.bytespersector = 512;
				bootbuffer.sectorspercluster = 1;
				bootbuffer.reservedsectors = 1;
				bootbuffer.fatcopies = 2;
				bootbuffer.rootdirentries = 64;
				bootbuffer.totalsectorcount = 320;
				bootbuffer.mediadescriptor = mdesc;
				bootbuffer.sectorsperfat = 1;
				bootbuffer.sectorspertrack = 8;
				bootbuffer.headcount = 1;
				bootbuffer.magic1 = 0x55;	// to silence warning
				bootbuffer.magic2 = 0xaa;
				if (!(mdesc & 0x2)) {
					/* Adjust for 9 sectors per track */
					bootbuffer.totalsectorcount = 360;
					bootbuffer.sectorsperfat = 2;
					bootbuffer.sectorspertrack = 9;
				}
				if (mdesc & 0x1) {
					/* Adjust for 2 sides */
					bootbuffer.sectorspercluster = 2;
					bootbuffer.rootdirentries = 112;
					bootbuffer.totalsectorcount *= 2;
					bootbuffer.headcount = 2;
				}
			} else {
				/* Unknown format */
				created_successfully = false;
				return;
			}
		}
	}

	if ((bootbuffer.magic1 != 0x55) || (bootbuffer.magic2 != 0xaa)) {
		/* Not a FAT filesystem */
		LOG_MSG("Loaded image has no valid magicnumbers at the end!");
	}

	/* Sanity checks */
	if ((bootbuffer.sectorsperfat == 0) || // FAT32 not implemented yet
		(bootbuffer.bytespersector != 512) || // non-standard sector sizes not implemented
		(bootbuffer.sectorspercluster == 0) ||
		(bootbuffer.rootdirentries == 0) ||
		(bootbuffer.fatcopies == 0) ||
		(bootbuffer.headcount == 0) ||
		(bootbuffer.headcount > headscyl) ||
		(bootbuffer.sectorspertrack == 0) ||
		(bootbuffer.sectorspertrack > cylsector)) {
		created_successfully = false;
		return;
	}

	/* Filesystem must be contiguous to use absolute sectors, otherwise CHS will be used */
	absolute = ((bootbuffer.headcount == headscyl) && (bootbuffer.sectorspertrack == cylsector));

	/* Determine FAT format, 12, 16 or 32 */

	/* Get size of root dir in sectors */
	uint32_t RootDirSectors = ((bootbuffer.rootdirentries * 32) + (bootbuffer.bytespersector - 1)) / bootbuffer.bytespersector;
	uint32_t DataSectors;
	if(bootbuffer.totalsectorcount != 0) {
		DataSectors = bootbuffer.totalsectorcount - (bootbuffer.reservedsectors + (bootbuffer.fatcopies * bootbuffer.sectorsperfat) + RootDirSectors);
	} else {
		DataSectors = bootbuffer.totalsecdword - (bootbuffer.reservedsectors + (bootbuffer.fatcopies * bootbuffer.sectorsperfat) + RootDirSectors);

	}
	CountOfClusters = DataSectors / bootbuffer.sectorspercluster;

	firstDataSector = (bootbuffer.reservedsectors + (bootbuffer.fatcopies * bootbuffer.sectorsperfat) + RootDirSectors) + partSectOff;
	firstRootDirSect = bootbuffer.reservedsectors + (bootbuffer.fatcopies * bootbuffer.sectorsperfat) + partSectOff;

	if(CountOfClusters < 4085) {
		/* Volume is FAT12 */
		LOG_MSG("Mounted FAT volume is FAT12 with %d clusters", CountOfClusters);
		fattype = FAT12;
	} else if (CountOfClusters < 65525) {
		LOG_MSG("Mounted FAT volume is FAT16 with %d clusters", CountOfClusters);
		fattype = FAT16;
	} else {
		LOG_MSG("Mounted FAT volume is FAT32 with %d clusters", CountOfClusters);
		fattype = FAT32;
	}

	/* There is no cluster 0, this means we are in the root directory */
	cwdDirCluster = 0;

	memset(fatSectBuffer,0,1024);
	curFatSect = 0xffffffff;

	type = DosDriveType::Fat;
	safe_strcpy(info, sysFilename);
}

bool fatDrive::AllocationInfo(uint16_t *_bytes_sector, uint8_t *_sectors_cluster, uint16_t *_total_clusters, uint16_t *_free_clusters) {
	// Guard
	if (!loadedDisk) {
		return false;
	}

	uint32_t hs, cy, sect,sectsize;
	uint32_t countFree = 0;
	uint32_t i;

	loadedDisk->Get_Geometry(&hs, &cy, &sect, &sectsize);
	*_bytes_sector = (uint16_t)sectsize;
	*_sectors_cluster = bootbuffer.sectorspercluster;

	if (CountOfClusters<65536) {
		*_total_clusters = (uint16_t)CountOfClusters;
	} else {
		// maybe some special handling needed for fat32
		*_total_clusters = 65535;
	}

	for (i=0; i<CountOfClusters; i++) {
		if (!getClusterValue(i + 2)) {
			countFree++;
		}
	}

	if (countFree<65536) {
		*_free_clusters = (uint16_t)countFree;
	} else {
		// maybe some special handling needed for fat32
		*_free_clusters = 65535;
	}
	return true;
}

uint32_t fatDrive::getFirstFreeClust(void) {
	uint32_t i;
	for(i=0;i<CountOfClusters;i++) {
		if(!getClusterValue(i+2)) return (i+2);
	}

	/* No free cluster found */
	return 0;
}

bool fatDrive::isRemote(void) {	return false; }
bool fatDrive::isRemovable(void) { return false; }

Bits fatDrive::UnMount(void) {
	delete this;
	return 0;
}

uint8_t fatDrive::GetMediaByte(void) {
	return loadedDisk ? loadedDisk->GetBiosType() : 0;
}

// name can be a full DOS path with filename, up-to DOS_PATHLENGTH in length
bool fatDrive::FileCreate(DOS_File **file, char *name, uint16_t attributes) {
	if (readonly) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	direntry fileEntry;
	uint32_t dirClust, subEntry;
	char dirName[DOS_NAMELENGTH_ASCII];
	char pathName[11]; // pathName is actually just the filename, without path

	uint16_t save_errorcode=dos.errorcode;

	/* Check if file already exists */
	if(getFileDirEntry(name, &fileEntry, &dirClust, &subEntry)) {
		/* Truncate file */
		if (fileEntry.loFirstClust != 0) {
			deleteClustChain(fileEntry.loFirstClust, 0);
			fileEntry.loFirstClust = 0;
		}
		fileEntry.entrysize = 0;
		directoryChange(dirClust, &fileEntry, subEntry);
	} else {
		/* Can we even get the name of the file itself? */
		if(!getEntryName(name, &dirName[0])) return false;
		convToDirFile(&dirName[0], &pathName[0]);

		/* Can we find the base directory? */
		if(!getDirClustNum(name, &dirClust, true)) return false;
		memset(&fileEntry, 0, sizeof(direntry));
		memcpy(&fileEntry.entryname, &pathName[0], 11);
		fileEntry.attrib = (uint8_t)(attributes & 0xff);
		addDirectoryEntry(dirClust, fileEntry);

		/* Check if file exists now */
		if(!getFileDirEntry(name, &fileEntry, &dirClust, &subEntry)) return false;
	}

	/* Empty file created, now lets open it */
	/* TODO: check for read-only flag and requested write access */
	*file = new fatFile(name, fileEntry.loFirstClust, fileEntry.entrysize, this);
	(*file)->flags=OPEN_READWRITE;
	((fatFile *)(*file))->dirCluster = dirClust;
	((fatFile *)(*file))->dirIndex = subEntry;
	/* Maybe modTime and date should be used ? (crt matches findnext) */
	((fatFile *)(*file))->time = fileEntry.crtTime;
	((fatFile *)(*file))->date = fileEntry.crtDate;

	dos.errorcode=save_errorcode;
	return true;
}

bool fatDrive::FileExists(const char *name) {
	direntry fileEntry;
	uint32_t dummy1, dummy2;
	uint16_t save_errorcode = dos.errorcode;
	bool found = getFileDirEntry(name, &fileEntry, &dummy1, &dummy2);
	dos.errorcode = save_errorcode;
	return found;
}

bool fatDrive::FileOpen(DOS_File **file, char *name, uint32_t flags) {
	direntry fileEntry;
	uint32_t dirClust, subEntry;
	if(!getFileDirEntry(name, &fileEntry, &dirClust, &subEntry)) return false;
	/* TODO: check for read-only flag and requested write access */
	*file = new fatFile(name, fileEntry.loFirstClust, fileEntry.entrysize, this);
	(*file)->flags = flags;
	((fatFile *)(*file))->dirCluster = dirClust;
	((fatFile *)(*file))->dirIndex = subEntry;
	/* Maybe modTime and date should be used ? (crt matches findnext) */
	((fatFile *)(*file))->time = fileEntry.crtTime;
	((fatFile *)(*file))->date = fileEntry.crtDate;
	return true;
}

bool fatDrive::FileStat(const char * /*name*/, FileStat_Block *const /*stat_block*/) {
	/* TODO: Stub */
	return false;
}

bool fatDrive::FileUnlink(char * name) {
	if (readonly) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	direntry fileEntry;
	uint32_t dirClust, subEntry;

	if(!getFileDirEntry(name, &fileEntry, &dirClust, &subEntry)) {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
/*
	Technically correct, but maybe an unwanted obstruction, so inactive for now.

	if(fileEntry.attrib & (DOS_ATTR_SYSTEM | DOS_ATTR_HIDDEN)) {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
	if(fileEntry.attrib & DOS_ATTR_READ_ONLY) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
*/
	fileEntry.entryname[0] = 0xe5;
	directoryChange(dirClust, &fileEntry, subEntry);

	if(fileEntry.loFirstClust != 0) deleteClustChain(fileEntry.loFirstClust, 0);

	return true;
}

bool fatDrive::FindFirst(char *_dir, DOS_DTA &dta,bool /*fcb_findfirst*/) {
	direntry dummyClust;
#if 0
	uint8_t attr;char pattern[DOS_NAMELENGTH_ASCII];
	dta.GetSearchParams(attr,pattern);
	if(attr==DOS_ATTR_VOLUME) {
		if (strcmp(GetLabel(), "") == 0 ) {
			DOS_SetError(DOSERR_NO_MORE_FILES);
			return false;
		}
		dta.SetResult(GetLabel(),0,0,0,DOS_ATTR_VOLUME);
		return true;
	}
	if(attr & DOS_ATTR_VOLUME) //check for root dir or fcb_findfirst
		LOG(LOG_DOSMISC,LOG_WARN)("findfirst for volumelabel used on fatDrive. Unhandled!!!!!");
#endif
	if(!getDirClustNum(_dir, &cwdDirCluster, false)) {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	dta.SetDirID(0);
	dta.SetDirIDCluster((uint16_t)(cwdDirCluster&0xffff));
	return FindNextInternal(cwdDirCluster, dta, &dummyClust);
}

char* removeTrailingSpaces(char* str, const size_t max_len) {
	const auto str_len = strnlen(str, max_len);
	if (str_len == 0)
		return str;

	char* end = str + str_len;
	while((*--end == ' ') && (end > str)) {
		/* do nothing; break on while criteria */
	}
	*++end = '\0';
	return str;
}

char* removeLeadingSpaces(char* str, const size_t max_len) {
	const size_t len = strnlen(str, max_len);
	const size_t pos = strspn(str, " ");
	memmove(str, str + pos, len - pos + 1);
	return str;
}

char* trimString(char* str, const size_t max_len) {
	return removeTrailingSpaces(removeLeadingSpaces(str, max_len), max_len);
}

static void copyDirEntry(const direntry *src, direntry *dst) {
	memcpy(dst->entryname, src->entryname, sizeof(src->entryname));
	dst->attrib           = host_to_le(src->attrib);
	dst->NTRes            = host_to_le(src->NTRes);
	dst->milliSecondStamp = host_to_le(src->milliSecondStamp);
	dst->crtTime          = host_to_le(src->crtTime);
	dst->crtDate          = host_to_le(src->crtDate);
	dst->accessDate       = host_to_le(src->accessDate);
	dst->hiFirstClust     = host_to_le(src->hiFirstClust);
	dst->modTime          = host_to_le(src->modTime);
	dst->modDate          = host_to_le(src->modDate);
	dst->loFirstClust     = host_to_le(src->loFirstClust);
	dst->entrysize        = host_to_le(src->entrysize);
}

bool fatDrive::FindNextInternal(uint32_t dirClustNumber, DOS_DTA &dta, direntry *foundEntry) {
	direntry sectbuf[16]; /* 16 directory entries per sector */
	uint32_t logentsector; /* Logical entry sector */
	uint32_t entryoffset;  /* Index offset within sector */
	uint32_t tmpsector;
	uint8_t attrs;
	uint16_t dirPos;
	char srch_pattern[DOS_NAMELENGTH_ASCII];
	char find_name[DOS_NAMELENGTH_ASCII];
	char extension[4];

	dta.GetSearchParams(attrs, srch_pattern);
	dirPos = dta.GetDirID();

nextfile:
	logentsector = dirPos / 16;
	entryoffset = dirPos % 16;

	if(dirClustNumber==0) {
		if(dirPos >= bootbuffer.rootdirentries) {
			DOS_SetError(DOSERR_NO_MORE_FILES);
			return false;
		}
		readSector(firstRootDirSect+logentsector,sectbuf);
	} else {
		tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
		/* A zero sector number can't happen */
		if(tmpsector == 0) {
			DOS_SetError(DOSERR_NO_MORE_FILES);
			return false;
		}
		readSector(tmpsector,sectbuf);
	}
	dirPos++;
	dta.SetDirID(dirPos);

	/* Deleted file entry */
	if (sectbuf[entryoffset].entryname[0] == 0xe5) goto nextfile;

	/* End of directory list */
	if (sectbuf[entryoffset].entryname[0] == 0x00) {
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;
	}
	memset(find_name,0,DOS_NAMELENGTH_ASCII);
	memset(extension,0,4);
	memcpy(find_name,&sectbuf[entryoffset].entryname[0],8);
	memcpy(extension,&sectbuf[entryoffset].entryname[8],3);
	trimString(&find_name[0], sizeof(find_name));
	trimString(&extension[0], sizeof(extension));
	
	//if(!(sectbuf[entryoffset].attrib & DOS_ATTR_DIRECTORY))
	if (extension[0]!=0) {
		safe_strcat(find_name, ".");
		safe_strcat(find_name, extension);
	}

	/* Compare attributes to search attributes */

	//TODO What about attrs = DOS_ATTR_VOLUME|DOS_ATTR_DIRECTORY ?
	if (attrs == DOS_ATTR_VOLUME) {
		if (!(sectbuf[entryoffset].attrib & DOS_ATTR_VOLUME)) goto nextfile;
		dirCache.SetLabel(find_name, false, true);
	} else {
		if (~attrs & sectbuf[entryoffset].attrib & (DOS_ATTR_DIRECTORY | DOS_ATTR_VOLUME | DOS_ATTR_SYSTEM | DOS_ATTR_HIDDEN) ) goto nextfile;
	}


	/* Compare name to search pattern */
	if(!WildFileCmp(find_name,srch_pattern)) goto nextfile;

	copyDirEntry(&sectbuf[entryoffset], foundEntry);

	//dta.SetResult(find_name, foundEntry->entrysize, foundEntry->crtDate, foundEntry->crtTime, foundEntry->attrib);

	dta.SetResult(find_name, foundEntry->entrysize, foundEntry->modDate, foundEntry->modTime, foundEntry->attrib);

	return true;
}

bool fatDrive::FindNext(DOS_DTA &dta) {
	direntry dummyClust;

	return FindNextInternal(dta.GetDirIDCluster(), dta, &dummyClust);
}

bool fatDrive::GetFileAttr(char *name, uint16_t *attr)
{
	/* you CAN get file attr root directory */
	if (*name == 0) {
		*attr = DOS_ATTR_DIRECTORY;
		return true;
	}

	direntry fileEntry = {};
	uint32_t dirClust;
	uint32_t subEntry;
	if (!getFileDirEntry(name, &fileEntry, &dirClust, &subEntry, true)) {
		return false;
	} else
		*attr = fileEntry.attrib;
	return true;
}

bool fatDrive::SetFileAttr(const char *name, const uint16_t attr)
{
	if (readonly) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	/* you cannot set file attr root directory (right?) */
	if (*name == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	direntry fileEntry = {};
	uint32_t dirClust;
	uint32_t subEntry;
	if (!getFileDirEntry(name, &fileEntry, &dirClust, &subEntry, true)) {
		return false;
	} else {
		fileEntry.attrib = (uint8_t)attr;
		directoryChange(dirClust, &fileEntry, (int32_t)subEntry);
	}
	return true;
}

bool fatDrive::directoryBrowse(uint32_t dirClustNumber, direntry *useEntry, int32_t entNum, int32_t start/*=0*/) {
	direntry sectbuf[16];	/* 16 directory entries per sector */
	uint32_t logentsector;	/* Logical entry sector */
	uint32_t entryoffset = 0;	/* Index offset within sector */
	uint32_t tmpsector;
	if ((start<0) || (start>65535)) return false;
	uint16_t dirPos = (uint16_t)start;
	if (entNum<start) return false;
	entNum-=start;

	while(entNum>=0) {

		logentsector = dirPos / 16;
		entryoffset = dirPos % 16;

		if(dirClustNumber==0) {
			if(dirPos >= bootbuffer.rootdirentries) return false;
			tmpsector = firstRootDirSect+logentsector;
			readSector(tmpsector,sectbuf);
		} else {
			tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
			/* A zero sector number can't happen */
			if(tmpsector == 0) return false;
			readSector(tmpsector,sectbuf);
		}
		dirPos++;


		/* End of directory list */
		if (sectbuf[entryoffset].entryname[0] == 0x00) return false;
		--entNum;
	}

	copyDirEntry(&sectbuf[entryoffset], useEntry);
	return true;
}

bool fatDrive::directoryChange(uint32_t dirClustNumber, direntry *useEntry, int32_t entNum) {
	direntry sectbuf[16];	/* 16 directory entries per sector */
	uint32_t logentsector;	/* Logical entry sector */
	uint32_t entryoffset = 0;	/* Index offset within sector */
	uint32_t tmpsector = 0;
	uint16_t dirPos = 0;
	
	while(entNum>=0) {
		
		logentsector = dirPos / 16;
		entryoffset = dirPos % 16;

		if(dirClustNumber==0) {
			if(dirPos >= bootbuffer.rootdirentries) return false;
			tmpsector = firstRootDirSect+logentsector;
			readSector(tmpsector,sectbuf);
		} else {
			tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
			/* A zero sector number can't happen */
			if(tmpsector == 0) return false;
			readSector(tmpsector,sectbuf);
		}
		dirPos++;


		/* End of directory list */
		if (sectbuf[entryoffset].entryname[0] == 0x00) return false;
		--entNum;
	}
	if(tmpsector != 0) {
		copyDirEntry(useEntry, &sectbuf[entryoffset]);
		writeSector(tmpsector, sectbuf);
		return true;
	} else {
		return false;
	}
}

bool fatDrive::addDirectoryEntry(uint32_t dirClustNumber, direntry useEntry) {
	direntry sectbuf[16]; /* 16 directory entries per sector */
	uint32_t logentsector; /* Logical entry sector */
	uint32_t entryoffset;  /* Index offset within sector */
	uint32_t tmpsector;
	uint16_t dirPos = 0;
	
	for(;;) {
		
		logentsector = dirPos / 16;
		entryoffset = dirPos % 16;

		if(dirClustNumber==0) {
			if(dirPos >= bootbuffer.rootdirentries) return false;
			tmpsector = firstRootDirSect+logentsector;
			readSector(tmpsector,sectbuf);
		} else {
			tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
			/* A zero sector number can't happen - we need to allocate more room for this directory*/
			if(tmpsector == 0) {
				uint32_t newClust;
				newClust = appendCluster(dirClustNumber);
				if(newClust == 0) return false;
				/* Try again to get tmpsector */
				tmpsector = getAbsoluteSectFromChain(dirClustNumber, logentsector);
				if(tmpsector == 0) return false; /* Give up if still can't get more room for directory */
			}
			readSector(tmpsector,sectbuf);
		}
		dirPos++;

		/* Deleted file entry or end of directory list */
		if ((sectbuf[entryoffset].entryname[0] == 0xe5) || (sectbuf[entryoffset].entryname[0] == 0x00)) {
			copyDirEntry(&useEntry, &sectbuf[entryoffset]);
			writeSector(tmpsector,sectbuf);
			break;
		}
	}

	return true;
}

void fatDrive::zeroOutCluster(uint32_t clustNumber) {
	uint8_t secBuffer[512];

	memset(&secBuffer[0], 0, 512);

	int i;
	for(i=0;i<bootbuffer.sectorspercluster;i++) {
		writeSector(getAbsoluteSectFromChain(clustNumber,i), &secBuffer[0]);
	}
}

bool fatDrive::MakeDir(char *dir) {
	if (readonly) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	uint32_t dummyClust, dirClust;
	direntry tmpentry;
	char dirName[DOS_NAMELENGTH_ASCII];
	char pathName[11];

	/* Can we even get the name of the directory itself? */
	if(!getEntryName(dir, &dirName[0])) return false;
	convToDirFile(&dirName[0], &pathName[0]);

	/* Fail to make directory if already exists */
	if(getDirClustNum(dir, &dummyClust, false)) return false;

	dummyClust = getFirstFreeClust();
	/* No more space */
	if(dummyClust == 0) return false;
	
	if(!allocateCluster(dummyClust, 0)) return false;

	zeroOutCluster(dummyClust);

	/* Can we find the base directory? */
	if(!getDirClustNum(dir, &dirClust, true)) return false;
	
	/* Add the new directory to the base directory */
	memset(&tmpentry,0, sizeof(direntry));
	memcpy(&tmpentry.entryname, &pathName[0], 11);
	tmpentry.loFirstClust = (uint16_t)(dummyClust & 0xffff);
	tmpentry.hiFirstClust = (uint16_t)(dummyClust >> 16);
	tmpentry.attrib = DOS_ATTR_DIRECTORY;
	addDirectoryEntry(dirClust, tmpentry);

	/* Add the [.] and [..] entries to our new directory*/
	/* [.] entry */
	memset(&tmpentry,0, sizeof(direntry));
	memcpy(&tmpentry.entryname, ".          ", 11);
	tmpentry.loFirstClust = (uint16_t)(dummyClust & 0xffff);
	tmpentry.hiFirstClust = (uint16_t)(dummyClust >> 16);
	tmpentry.attrib = DOS_ATTR_DIRECTORY;
	addDirectoryEntry(dummyClust, tmpentry);

	/* [..] entry */
	memset(&tmpentry,0, sizeof(direntry));
	memcpy(&tmpentry.entryname, "..         ", 11);
	tmpentry.loFirstClust = (uint16_t)(dirClust & 0xffff);
	tmpentry.hiFirstClust = (uint16_t)(dirClust >> 16);
	tmpentry.attrib = DOS_ATTR_DIRECTORY;
	addDirectoryEntry(dummyClust, tmpentry);

	return true;
}

bool fatDrive::RemoveDir(char *dir) {
	if (readonly) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	uint32_t dummyClust, dirClust;
	direntry tmpentry;
	char dirName[DOS_NAMELENGTH_ASCII];
	char pathName[11];

	/* Can we even get the name of the directory itself? */
	if(!getEntryName(dir, &dirName[0])) return false;
	convToDirFile(&dirName[0], &pathName[0]);

	/* Get directory starting cluster */
	if(!getDirClustNum(dir, &dummyClust, false)) return false;

	/* Can't remove root directory */
	if(dummyClust == 0) return false;

	/* Get parent directory starting cluster */
	if(!getDirClustNum(dir, &dirClust, true)) return false;

	/* Check to make sure directory is empty */
	uint32_t filecount = 0;
	/* Set to 2 to skip first 2 entries, [.] and [..] */
	int32_t fileidx = 2;
	while(directoryBrowse(dummyClust, &tmpentry, fileidx)) {
		/* Check for non-deleted files */
		if(tmpentry.entryname[0] != 0xe5) filecount++;
		fileidx++;
	}

	/* Return if directory is not empty */
	if(filecount > 0) return false;

	/* Find directory entry in parent directory */
	if (dirClust==0) fileidx = 0;	// root directory
	else fileidx = 2;
	bool found = false;
	while(directoryBrowse(dirClust, &tmpentry, fileidx)) {
		if(memcmp(&tmpentry.entryname, &pathName[0], 11) == 0) {
			found = true;
			tmpentry.entryname[0] = 0xe5;
			directoryChange(dirClust, &tmpentry, fileidx);
			deleteClustChain(dummyClust, 0);

			break;
		}
		fileidx++;
	}

	if(!found) return false;

	return true;
}

bool fatDrive::Rename(char * oldname, char * newname) {
	if (readonly) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	direntry fileEntry1;
	uint32_t dirClust1, subEntry1;
	if(!getFileDirEntry(oldname, &fileEntry1, &dirClust1, &subEntry1)) return false;
	/* File to be renamed really exists */

	direntry fileEntry2;
	uint32_t dirClust2, subEntry2;

	/* Check if file already exists */
	if(!getFileDirEntry(newname, &fileEntry2, &dirClust2, &subEntry2)) {
		/* Target doesn't exist, can rename */

		char dirName2[DOS_NAMELENGTH_ASCII];
		char pathName2[11];
		/* Can we even get the name of the file itself? */
		if(!getEntryName(newname, &dirName2[0])) return false;
		convToDirFile(&dirName2[0], &pathName2[0]);

		/* Can we find the base directory? */
		if(!getDirClustNum(newname, &dirClust2, true)) return false;
		memcpy(&fileEntry2, &fileEntry1, sizeof(direntry));
		memcpy(&fileEntry2.entryname, &pathName2[0], 11);
		addDirectoryEntry(dirClust2, fileEntry2);

		/* Check if file exists now */
		if(!getFileDirEntry(newname, &fileEntry2, &dirClust2, &subEntry2)) return false;

		/* Remove old entry */
		fileEntry1.entryname[0] = 0xe5;
		directoryChange(dirClust1, &fileEntry1, subEntry1);

		return true;
	}

	/* Target already exists, fail */
	return false;
}

bool fatDrive::TestDir(char *dir) {
	uint32_t dummyClust;
	return getDirClustNum(dir, &dummyClust, false);
}
