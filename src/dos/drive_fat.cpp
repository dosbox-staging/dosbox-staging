// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/drives.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "dos.h"
#include "ints/bios.h"
#include "ints/bios_disk.h"
#include "misc/support.h"
#include "utils/string_utils.h"

#define IMGTYPE_FLOPPY 0
#define IMGTYPE_ISO    1
#define IMGTYPE_HDD	   2

#define FAT12		   0
#define FAT16		   1
#define FAT32		   2

static constexpr uint16_t BytePerSector = 512;

class fatFile final : public DOS_File {
public:
	fatFile(const char* name, uint32_t startCluster, uint32_t fileLen, std::shared_ptr<fatDrive> useDrive, bool _read_only_medium);
	fatFile(const fatFile&) = delete; // prevent copy
	fatFile& operator=(const fatFile&) = delete; // prevent assignment
	bool Read(uint8_t * data,uint16_t * size) override;
	bool Write(uint8_t * data,uint16_t * size) override;
	bool Seek(uint32_t * pos,uint32_t type) override;
	void Close() override;
	uint16_t GetInformation(void) override;
	bool IsOnReadOnlyMedium() const override;
public:
	std::shared_ptr<fatDrive> myDrive   = nullptr;
	uint32_t firstCluster               = 0;
	uint32_t seekpos                    = 0;
	uint32_t filelength                 = 0;
	uint32_t currentSector              = 0;
	uint32_t curSectOff                 = 0;
	uint8_t sectorBuffer[BytePerSector] = {0};
	/* Record of where in the directory structure this file is located */
	uint32_t dirCluster = 0;
	uint32_t dirIndex   = 0;

	bool set_archive_on_close   = false;
	bool loadedSector           = false;
	const bool read_only_medium = false;
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

fatFile::fatFile(const char* /*name*/, uint32_t startCluster, uint32_t fileLen,
                 std::shared_ptr<fatDrive> useDrive, bool _read_only_medium)
        : myDrive(useDrive),
          firstCluster(startCluster),
          filelength(fileLen),
          read_only_medium(_read_only_medium)
{
	uint32_t seekto = 0;
	if(filelength > 0) {
		Seek(&seekto, DOS_SEEK_SET);
	}
}

bool fatFile::Read(uint8_t * data, uint16_t *size) {
	// check if file opened in write-only mode
	if ((this->flags & 0xf) == OPEN_WRITE) {
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
	// check if file opened in read-only mode
	uint8_t lastflags = this->flags & 0xf;
	if (lastflags == OPEN_READ || lastflags == OPEN_READ_NO_MOD) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	// File should always be opened in read-only mode if on read-only drive
	assert(!IsOnReadOnlyMedium());

	direntry tmpentry;
	uint16_t sizedec, sizecount;
	sizedec = *size;
	sizecount = 0;

	set_archive_on_close = true;

	if (seekpos < filelength && *size == 0) {
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
	// TODO: On MS-DOS 6.22 timestamps only get flushed to disk on file close.
	// The time value should also be the time of close, not of write.
	// This is unlikely to cause huge problems and I don't feel confident in changing this code right now.
	tmpentry.modTime = DOS_GetBiosTimePacked();
	tmpentry.modDate = DOS_GetBiosDatePacked();
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

void fatFile::Close()
{
	if (flush_time_on_close == FlushTimeOnClose::ManuallySet ||
	    set_archive_on_close) {
		assert(!IsOnReadOnlyMedium());
		direntry tmpentry;
		myDrive->directoryBrowse(dirCluster, &tmpentry, dirIndex);
		if (flush_time_on_close == FlushTimeOnClose::ManuallySet) {
			tmpentry.modTime = time;
			tmpentry.modDate = date;
		}
		if (set_archive_on_close) {
			FatAttributeFlags tmp = tmpentry.attrib;
			tmp.archive           = true;
			tmpentry.attrib       = tmp._data;
		}
		myDrive->directoryChange(dirCluster, &tmpentry, dirIndex);
	}

	// Flush buffer
	if (loadedSector) {
		myDrive->writeSector(currentSector, sectorBuffer);
	}

	set_archive_on_close = false;
}

bool fatFile::IsOnReadOnlyMedium() const
{
	return read_only_medium;
}

uint16_t fatFile::GetInformation(void) {
	return 0;
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
			readSector(fatsectnum + 1, &fatSectBuffer[BytePerSector]);
		curFatSect = fatsectnum;
	}

	switch(fattype) {
		case FAT12:
			clustValue = var_read((uint16_t *)&fatSectBuffer[fatentoff]);
			if(clustNum & 0x1) { //-V1051
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
			        readSector(fatsectnum + 1,
			                   &fatSectBuffer[BytePerSector]);
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
				writeSector(fatsectnum + 1 +
				                    (fc * bootbuffer.sectorsperfat),
				            &fatSectBuffer[BytePerSector]);
		}
	}
}

bool fatDrive::getEntryName(const char *fullname, char *entname) {
	char dirtoken[DOS_PATHLENGTH];

	char * findDir;
	char * findFile;
	safe_strcpy(dirtoken, fullname);

	//LOG_MSG("Testing for filename %s", fullname);
	findDir = strtok(dirtoken,"\\");
	if (findDir==nullptr) {
		return true;	// root always exists
	}
	findFile = findDir;
	while(findDir != nullptr) {
		findFile = findDir;
		findDir = strtok(nullptr,"\\");
	}

	assert(entname);
	entname[0] = '\0';
	strncat(entname, findFile, DOS_NAMELENGTH_ASCII - 1);
	return true;
}

bool fatDrive::getFileDirEntry(const char* const filename, direntry* useEntry,
                               uint32_t* dirClust, uint32_t* subEntry,
                               const bool dir_ok)
{
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
		while(findDir != nullptr) {
			imgDTA->SetupSearch(0, FatAttributeFlags::Directory, findDir);
			imgDTA->SetDirID(0);
			
			findFile = findDir;
			if (!FindNextInternal(currentClust, *imgDTA, &foundEntry)) {
				break;
			} else {
				// Found something. See if it's a directory
				// (findfirst always finds regular files)
				DOS_DTA::Result search_result = {};
				imgDTA->GetResult(search_result);
				if (!search_result.IsDirectory()) {
					break;
				}
				char* findNext;
				findNext = strtok(nullptr, "\\");
				if (findNext == nullptr && dir_ok)
					break;
				findDir = findNext;
			}

			currentClust = foundEntry.loFirstClust;
		}
	} else {
		/* Set to root directory */
	}

	/* Search found directory for our file */
	FatAttributeFlags attributes = {};
	attributes.read_only         = true;
	attributes.hidden            = true;
	attributes.system            = true;
	attributes.directory         = dir_ok;

	imgDTA->SetupSearch(0, attributes, findFile);
	imgDTA->SetDirID(0);
	if(!FindNextInternal(currentClust, *imgDTA, &foundEntry)) return false;

	memcpy(useEntry, &foundEntry, sizeof(direntry));
	*dirClust = currentClust;
	*subEntry = ((uint32_t)imgDTA->GetDirID()-1);
	return true;
}

bool fatDrive::getDirClustNum(const char* dir, uint32_t* clustNum, bool parDir)
{
	auto len = static_cast<uint32_t>(strnlen(dir, DOS_PATHLENGTH));
	char dirtoken[DOS_PATHLENGTH];
	uint32_t currentClust = 0;
	direntry foundEntry;
	char * findDir;
	safe_strcpy(dirtoken, dir);

	/* Skip if testing for root directory */
	if ((len>0) && (dir[len-1]!='\\')) {
		//LOG_MSG("Testing for dir %s", dir);
		findDir = strtok(dirtoken,"\\");
		while(findDir != nullptr) {
			imgDTA->SetupSearch(0, FatAttributeFlags::Directory, findDir);
			imgDTA->SetDirID(0);
			findDir = strtok(nullptr,"\\");
			if(parDir && (findDir == nullptr)) break;

			if(!FindNextInternal(currentClust, *imgDTA, &foundEntry)) {
				return false;
			} else {
				DOS_DTA::Result search_result = {};
				imgDTA->GetResult(search_result);
				if (!search_result.IsDirectory()) {
					return false;
				}
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
		if (isEOF && (skipClust >= 1)) {
			if (skipClust == 1 && fattype == FAT12) {
				//break;
				LOG(LOG_DOSMISC,
				    LOG_WARN)("End of cluster chain reached.");
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

constexpr uint16_t dta_pages()
{
	constexpr auto BytesPerPage = 16;
	uint16_t pages = sizeof(struct sDTA) / BytesPerPage;
	if ((sizeof(struct sDTA) % BytesPerPage) != 0) {
		++pages;
	}
	return pages;
}

fatDrive::fatDrive(const char* sysFilename, uint32_t bytesector,
                   uint32_t cylsector, uint32_t headscyl, uint32_t cylinders,
                   uint8_t mediaid, bool roflag)
        : loadedDisk(nullptr),
          created_successfully(true),
          partSectOff(0),
          mediaid(mediaid),
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
		imgDTASeg = DOS_GetMemory(dta_pages());
		imgDTAPtr = RealMake(imgDTASeg, 0);
		imgDTA    = new DOS_DTA(imgDTAPtr);
	}
	assert(sysFilename);
	diskfile = fopen_wrap_ro_fallback(sysFilename, readonly);
	created_successfully = (diskfile != nullptr);
	if (!created_successfully)
		return;
	const auto sz = stdio_size_kb(diskfile);
	if (sz < 0) {
		fclose(diskfile);
		return;
	}
	filesize = check_cast<uint32_t>(sz);
	is_hdd   = (filesize > 2880);

	/* Load disk image */
	loadedDisk = std::make_shared<imageDisk>(diskfile, sysFilename, filesize, is_hdd);

	if(is_hdd) {
		/* Set user specified harddrive parameters */
		loadedDisk->Set_Geometry(headscyl, cylinders,cylsector, bytesector);

		loadedDisk->Read_Sector(0,0,1,&mbrData);

		if(mbrData.magic1!= 0x55 ||	mbrData.magic2!= 0xaa) LOG_MSG("Possibly invalid partition table in disk image.");

		uint32_t startSector = 63;
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

	if (bytesector != BytePerSector) {
		/* Non-standard sector sizes not implemented */
		created_successfully = false;
		LOG_WARNING("DOS: MOUNT - Non-standard sector size detected: %u bytes per sector",
		            bytesector);
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
			uint8_t sectorBuffer[BytePerSector];
			loadedDisk->Read_AbsoluteSector(1,&sectorBuffer);
			uint8_t mdesc = sectorBuffer[0];

			if (mdesc >= 0xf8) {
				/* DOS 1.x format, create BPB for 160kB floppy */
				bootbuffer.bytespersector    = BytePerSector;
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
				LOG_WARNING("DOS: MOUNT - Unknown floppy format detected (media descriptor 0x%02x).",
				            mdesc);
				return;
			}
		}
	}

	if ((bootbuffer.magic1 != 0x55) || (bootbuffer.magic2 != 0xaa)) {
		/* Not a FAT filesystem */
		LOG_MSG("Loaded image has no valid magicnumbers at the end.");
	}

	/* Sanity checks */

	if (bootbuffer.sectorsperfat == 0) {
		/* Possibly a FAT32 or non-FAT filesystem */
		created_successfully = false;
		LOG_WARNING("DOS: MOUNT - Loaded image has zero sectors per FAT! FAT32 and non-FAT filesystems are not supported.");
		return;
	}
	if (bootbuffer.bytespersector != BytePerSector) {
		created_successfully = false;
		LOG_WARNING("DOS: MOUNT - Bytes Per Sector mismatch: expected %u, got %u.",
		            BytePerSector,
		            bootbuffer.bytespersector);
		return;
	}
	if (bootbuffer.sectorspercluster == 0) {
		created_successfully = false;
		LOG_WARNING("DOS: MOUNT - Loaded image has zero sectors per cluster.");
		return;
	}
	if (bootbuffer.rootdirentries == 0) {
		created_successfully = false;
		LOG_WARNING("DOS: MOUNT - Loaded image has zero root directory entries.");
		return;
	}
	if (bootbuffer.fatcopies == 0) {
		created_successfully = false;
		LOG_WARNING("DOS: MOUNT - Loaded image has zero FAT copies.");
		return;
	}
	/* Check geometry values */
	if (bootbuffer.headcount == 0) {
		created_successfully = false;
		LOG_WARNING("DOS: MOUNT - Loaded image has zero heads per cylinder.");
		return;
	}
	if (bootbuffer.headcount > headscyl) {
		created_successfully = false;
		LOG_WARNING("DOS: MOUNT - Loaded image has more heads per cylinder (%u) than the disk geometry allows (%u).",
		            bootbuffer.headcount,
		            headscyl);
		return;
	}
	if (bootbuffer.sectorspertrack == 0) {
		created_successfully = false;
		LOG_WARNING("DOS: MOUNT - Loaded image has zero sectors per track.");
		return;
	}
	if (bootbuffer.sectorspertrack > cylsector) {
		created_successfully = false;
		LOG_WARNING("DOS: MOUNT - Loaded image has more sectors per track (%u) than the disk geometry allows (%u).",
		            bootbuffer.sectorspertrack,
		            cylsector);
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

	if (CountOfClusters < 4085) {
		/* Volume is FAT12 */
		LOG_MSG("FAT: Mounted %s as FAT12 volume with %d clusters",
		        sysFilename,
		        CountOfClusters);
		fattype = FAT12;
	} else if (CountOfClusters < 65525) {
		LOG_MSG("FAT: Mounted %s as FAT16 volume with %d clusters",
		        sysFilename,
		        CountOfClusters);
		fattype = FAT16;
	} else {
		LOG_MSG("FAT: Mounted %s as FAT32 volume with %d clusters",
		        sysFilename,
		        CountOfClusters);
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

bool fatDrive::IsRemote(void) {	return false; }
bool fatDrive::IsRemovable(void) { return false; }

Bits fatDrive::UnMount()
{
	return 0;
}

uint8_t fatDrive::GetMediaByte(void) {
	return mediaid;
}

// name can be a full DOS path with filename, up-to DOS_PATHLENGTH in length
std::unique_ptr<DOS_File> fatDrive::FileCreate(const char* name,
                                               FatAttributeFlags attributes)
{
	if (IsReadOnly()) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return nullptr;
	}
	direntry fileEntry;
	uint32_t dirClust, subEntry;
	char dirName[DOS_NAMELENGTH_ASCII];
	char pathName[11]; // pathName is actually just the filename, without path

	uint16_t save_errorcode = dos.errorcode;

	attributes.archive = true;

	/* Check if file already exists */
	if(getFileDirEntry(name, &fileEntry, &dirClust, &subEntry)) {
		const FatAttributeFlags entry_attributes = fileEntry.attrib;
		if (entry_attributes.read_only) {
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return nullptr;
		}

		/* Truncate file */
		if (fileEntry.loFirstClust != 0) {
			deleteClustChain(fileEntry.loFirstClust, 0);
			fileEntry.loFirstClust = 0;
		}
		fileEntry.entrysize = 0;
		fileEntry.attrib    = attributes._data;
		fileEntry.modTime   = DOS_GetBiosTimePacked();
		fileEntry.modDate   = DOS_GetBiosDatePacked();
		directoryChange(dirClust, &fileEntry, subEntry);
	} else {
		/* Can we even get the name of the file itself? */
		if (!getEntryName(name, &dirName[0])) {
			return nullptr;
		}
		convToDirFile(&dirName[0], &pathName[0]);

		/* Can we find the base directory? */
		if (!getDirClustNum(name, &dirClust, true)) {
			return nullptr;
		}
		fileEntry = {};
		memcpy(&fileEntry.entryname, &pathName[0], 11);
		fileEntry.attrib  = attributes._data;
		fileEntry.modTime = DOS_GetBiosTimePacked();
		fileEntry.modDate = DOS_GetBiosDatePacked();
		addDirectoryEntry(dirClust, fileEntry);

		/* Check if file exists now */
		if (!getFileDirEntry(name, &fileEntry, &dirClust, &subEntry)) {
			return nullptr;
		}
	}

	// These must be extracted to temporaries or GCC throws a compile error
	// It doesn't like something about the combination of the packed attribute
	// and how make_unique uses references as arguments
	const auto first_cluster = fileEntry.loFirstClust;
	const auto entry_size = fileEntry.entrysize;

	/* Empty file created, now lets open it */
	auto fat_file        = std::make_unique<fatFile>(name,
                                                  first_cluster,
                                                  entry_size,
                                                  shared_from_this(),
                                                  IsReadOnly());
	fat_file->flags      = OPEN_READWRITE;
	fat_file->dirCluster = dirClust;
	fat_file->dirIndex   = subEntry;
	fat_file->time       = fileEntry.modTime;
	fat_file->date       = fileEntry.modDate;

	dos.errorcode=save_errorcode;
	return fat_file;
}

bool fatDrive::FileExists(const char *name) {
	direntry fileEntry;
	uint32_t dummy1, dummy2;
	uint16_t save_errorcode = dos.errorcode;
	bool found = getFileDirEntry(name, &fileEntry, &dummy1, &dummy2);
	dos.errorcode = save_errorcode;
	return found;
}

std::unique_ptr<DOS_File> fatDrive::FileOpen(const char* name, uint8_t flags)
{
	direntry fileEntry;
	uint32_t dirClust, subEntry;
	if (!getFileDirEntry(name, &fileEntry, &dirClust, &subEntry)) {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return nullptr;
	}

	const FatAttributeFlags entry_attributes = fileEntry.attrib;
	const bool is_readonly                   = entry_attributes.read_only;
	bool open_for_readonly                   = ((flags & 0xf) == OPEN_READ ||
		                                        (flags & 0xf) == OPEN_READ_NO_MOD);
	if (is_readonly && !open_for_readonly) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return nullptr;
	}

	// Force read-only mode if the drive is read-only.
	if (!open_for_readonly && IsReadOnly()) {
		flags = OPEN_READ;
	}

	// These must be extracted to temporaries or GCC throws a compile error
	// It doesn't like something about the combination of the packed attribute
	// and how make_unique uses references as arguments
	const auto first_cluster = fileEntry.loFirstClust;
	const auto entry_size = fileEntry.entrysize;

	auto fat_file = std::make_unique<fatFile>(name,
	                                          first_cluster,
	                                          entry_size,
	                                          shared_from_this(),
	                                          IsReadOnly());

	fat_file->flags      = flags;
	fat_file->dirCluster = dirClust;
	fat_file->dirIndex   = subEntry;
	fat_file->time       = fileEntry.modTime;
	fat_file->date       = fileEntry.modDate;

	return fat_file;
}

bool fatDrive::FileUnlink(const char * name) {
	if (IsReadOnly()) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	direntry fileEntry;
	uint32_t dirClust, subEntry;
	if(!getFileDirEntry(name, &fileEntry, &dirClust, &subEntry)) {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}

	const FatAttributeFlags entry_attributes = fileEntry.attrib;

	/* Not sure if this is correct. */
#if 0
	if (entry_attributes.system || entry_attributes.hidden) {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
#endif

	if (entry_attributes.read_only) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	fileEntry.entryname[0] = 0xe5;
	directoryChange(dirClust, &fileEntry, subEntry);

	if(fileEntry.loFirstClust != 0) deleteClustChain(fileEntry.loFirstClust, 0);

	return true;
}

bool fatDrive::FindFirst(const char *_dir, DOS_DTA &dta,bool /*fcb_findfirst*/) {
	direntry dummyClust;
#if 0
	uint8_t attr;char pattern[DOS_NAMELENGTH_ASCII];
	dta.GetSearchParams(attr,pattern);
	if(attr == FatAttributeFlags::Volume) {
		if (strcmp(GetLabel(), "") == 0 ) {
			DOS_SetError(DOSERR_NO_MORE_FILES);
			return false;
		}
		dta.SetResult(GetLabel(),0,0,0,FatAttributeFlags::Volume);
		return true;
	}
	if (FatAttributeFlags(attr).volume) //check for root dir or fcb_findfirst
		LOG(LOG_DOSMISC,LOG_WARN)("findfirst for volumelabel used on fatDrive. Unhandled.");
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

bool fatDrive::FindNextInternal(uint32_t dirClustNumber, DOS_DTA& dta,
                                direntry* foundEntry)
{
	direntry sectbuf[16];  /* 16 directory entries per sector */
	uint32_t logentsector; /* Logical entry sector */
	uint32_t entryoffset;  /* Index offset within sector */
	uint32_t tmpsector;
	FatAttributeFlags attrs = {};
	uint16_t dirPos;
	char search_pattern[DOS_NAMELENGTH_ASCII];
	char find_name[DOS_NAMELENGTH_ASCII];
	char extension[4];

	dta.GetSearchParams(attrs, search_pattern);
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

	const auto entry_attributes = FatAttributeFlags(sectbuf[entryoffset].attrib);

	// if(!entry_attributes.directory)

	if (extension[0] != 0) {
		safe_strcat(find_name, ".");
		safe_strcat(find_name, extension);
	}

	// TODO What about volume/directory attributes?
	if (attrs == FatAttributeFlags::Volume) {
		if (!entry_attributes.volume) {
			goto nextfile;
		}
		dirCache.SetLabel(find_name, false, true);
	} else {
		// Compare attributes to search attributes
		const FatAttributeFlags attr_mask = {
		        FatAttributeFlags::Directory |
		        FatAttributeFlags::Volume |
		        FatAttributeFlags::System |
		        FatAttributeFlags::Hidden};

		if (~(attrs._data) & entry_attributes._data & attr_mask._data) {
			goto nextfile;
		}
	}

	/* Compare name to search pattern */
	if (!wild_file_cmp(find_name, search_pattern)) {
		goto nextfile;
	}

	copyDirEntry(&sectbuf[entryoffset], foundEntry);

	//dta.SetResult(find_name, foundEntry->entrysize, foundEntry->crtDate, foundEntry->crtTime, foundEntry->attrib);

	dta.SetResult(find_name, foundEntry->entrysize, foundEntry->modDate, foundEntry->modTime, foundEntry->attrib);

	return true;
}

bool fatDrive::FindNext(DOS_DTA &dta) {
	direntry dummyClust;

	return FindNextInternal(dta.GetDirIDCluster(), dta, &dummyClust);
}

bool fatDrive::GetFileAttr(const char* name, FatAttributeFlags* attr)
{
	/* you CAN get file attr root directory */
	if (*name == 0) {
		*attr = FatAttributeFlags::Directory;
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

bool fatDrive::SetFileAttr(const char* name, const FatAttributeFlags attr)
{
	if (IsReadOnly()) {
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
		fileEntry.attrib = attr._data;
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
	auto dirPos = static_cast<uint16_t>(start);
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
	uint8_t secBuffer[BytePerSector];

	memset(&secBuffer[0], 0, BytePerSector);

	int i;
	for(i=0;i<bootbuffer.sectorspercluster;i++) {
		writeSector(getAbsoluteSectFromChain(clustNumber,i), &secBuffer[0]);
	}
}

bool fatDrive::MakeDir(const char* dir)
{
	if (IsReadOnly()) {
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
	tmpentry.attrib       = FatAttributeFlags::Directory;
	addDirectoryEntry(dirClust, tmpentry);

	/* Add the [.] and [..] entries to our new directory*/
	/* [.] entry */
	memset(&tmpentry,0, sizeof(direntry));
	memcpy(&tmpentry.entryname, ".          ", 11);
	tmpentry.loFirstClust = (uint16_t)(dummyClust & 0xffff);
	tmpentry.hiFirstClust = (uint16_t)(dummyClust >> 16);
	tmpentry.attrib       = FatAttributeFlags::Directory;
	addDirectoryEntry(dummyClust, tmpentry);

	/* [..] entry */
	memset(&tmpentry,0, sizeof(direntry));
	memcpy(&tmpentry.entryname, "..         ", 11);
	tmpentry.loFirstClust = (uint16_t)(dirClust & 0xffff);
	tmpentry.hiFirstClust = (uint16_t)(dirClust >> 16);
	tmpentry.attrib       = FatAttributeFlags::Directory;
	addDirectoryEntry(dummyClust, tmpentry);

	return true;
}

bool fatDrive::RemoveDir(const char *dir) {
	if (IsReadOnly()) {
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

bool fatDrive::Rename(const char * oldname, const char * newname) {
	if (IsReadOnly()) {
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

		char dirName2[DOS_NAMELENGTH_ASCII] = {};
		char pathName2[11] = {};

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

bool fatDrive::TestDir(const char *dir) {
	uint32_t dummyClust;
	return getDirClustNum(dir, &dummyClust, false);
}
