// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/drives.h"

#include <cctype>
#include <cstring>

#include "cdrom.h"
#include "dos_mscdex.h"
#include "dos/dos_system.h"
#include "utils/string_utils.h"
#include "misc/support.h"

#define FLAGS1	((iso) ? de.fileFlags : de.timeZone)
#define FLAGS2	((iso) ? de->fileFlags : de->timeZone)

class isoFile final : public DOS_File {
public:
	isoFile(std::shared_ptr<isoDrive> drive, const char *name, FileStat_Block *stat, uint32_t offset);
	isoFile(const isoFile &) = delete;            // prevent copying
	isoFile &operator=(const isoFile &) = delete; // prevent assignment

	bool Read(uint8_t *data, uint16_t *size) override;
	bool Write(const uint8_t* data, uint16_t* size) override;
	bool Seek(uint32_t* pos, const uint32_t type) override;
	void Close() override;
	uint16_t GetInformation(void) override;
	bool IsOnReadOnlyMedium() const override;

private:
	std::shared_ptr<isoDrive> drive = nullptr;
	int cachedSector = -1;
	uint32_t fileBegin = 0;
	uint32_t filePos = 0;
	uint32_t fileEnd = 0;
	uint8_t buffer[ISO_FRAMESIZE] = {{}};
};

isoFile::isoFile(std::shared_ptr<isoDrive> iso_drive, const char *name, FileStat_Block *stat, uint32_t offset)
        : drive(iso_drive),
          fileBegin(offset),
          filePos(offset),
          fileEnd(offset + stat->size)
{
	SetName(name);

	// Initialize base members
	assert(stat);
	time = stat->time;
	date = stat->date;
	attr = static_cast<uint8_t>(stat->attr);
}

bool isoFile::Read(uint8_t *data, uint16_t *size) {
	if (filePos + *size > fileEnd)
		*size = (uint16_t)(fileEnd - filePos);

	uint16_t nowSize = 0;
	uint32_t sector = filePos / ISO_FRAMESIZE;

	if (static_cast<int>(sector) != cachedSector) {
		if (drive->readSector(buffer, sector)) {
			cachedSector = static_cast<int>(sector);
		} else {
			*size = 0;
			cachedSector = -1;
		}
	}

	static_assert(ISO_FRAMESIZE <= UINT16_MAX);
	auto sectorPos = static_cast<uint16_t>(filePos % ISO_FRAMESIZE);

	while (nowSize < *size) {
		uint16_t remSector = ISO_FRAMESIZE - sectorPos;
		uint16_t remSize = *size - nowSize;
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

bool isoFile::Write(const uint8_t* /*data*/, uint16_t* /*size*/)
{
	return false;
}

bool isoFile::Seek(uint32_t* pos, const uint32_t type)
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

void isoFile::Close() {
}

uint16_t isoFile::GetInformation(void) {
	return 0x40;		// read-only drive
}

bool isoFile::IsOnReadOnlyMedium() const
{
	return true;
}

isoDrive::isoDrive(char driveLetter, const char *fileName, uint8_t mediaid, int &error)
        : nextFreeDirIterator(0),
          iso(false),
          dataCD(false),
          rootEntry{},
          mediaid(0),
          subUnit(0),
          driveLetter('\0')
{
	this->fileName[0]  = '\0';
	this->discLabel[0] = '\0';
	memset(dirIterators, 0, sizeof(dirIterators));
	memset(sectorHashEntries, 0, sizeof(sectorHashEntries));
	memset(&rootEntry, 0, sizeof(isoDirEntry));

	safe_strcpy(this->fileName, fileName);
	type  = DosDriveType::Iso;
	error = UpdateMscdex(driveLetter, fileName, subUnit);

	if (!error) {
		if (loadImage()) {
			safe_strcpy(info, fileName);
			this->driveLetter = driveLetter;
			this->mediaid = mediaid;
			char buffer[32] = { 0 };
			if (!MSCDEX_GetVolumeName(subUnit, buffer)) strcpy(buffer, "");
			Set_Label(buffer,discLabel,true);

		} else if (CDROM::cdroms[subUnit]->HasDataTrack() == false) { // Audio only cdrom
			safe_strcpy(info, fileName);
			this->driveLetter = driveLetter;
			this->mediaid = mediaid;
			char buffer[32] = { 0 };
			safe_strcpy(buffer, "Audio_CD");
			Set_Label(buffer,discLabel,true);
		} else error = 6; //Corrupt image
	}
}

isoDrive::~isoDrive() { }

int isoDrive::UpdateMscdex(char drive_letter, const char *path, uint8_t &sub_unit)
{
	if (MSCDEX_HasDrive(drive_letter)) {
		sub_unit = MSCDEX_GetSubUnit(drive_letter);
		std::unique_ptr<CDROM_Interface> cdrom = std::make_unique<CDROM_Interface_Image>();
		char pathCopy[CROSS_LEN];
		safe_strcpy(pathCopy, path);
		if (!cdrom->SetDevice(pathCopy)) {
			return 3;
		}
		MSCDEX_ReplaceDrive(std::move(cdrom), sub_unit);
		return 0;
	} else {
		return MSCDEX_AddDrive(drive_letter, path, sub_unit);
	}
}

void isoDrive::Activate(void) {
	UpdateMscdex(driveLetter, fileName, subUnit);
}

std::unique_ptr<DOS_File> isoDrive::FileOpen(const char* name, uint8_t flags)
{
	if ((flags & 0x0f) == OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return nullptr;
	}

	isoDirEntry de;
	if (!(lookup(&de, name) && !IS_DIR(FLAGS1))) {
		return nullptr;
	}

	FileStat_Block file_stat;
	file_stat.size = DATA_LENGTH(de);
	file_stat.attr = FatAttributeFlags::ReadOnly;
	file_stat.date = DOS_PackDate(1900 + de.dateYear, de.dateMonth, de.dateDay);
	file_stat.time = DOS_PackTime(de.timeHour, de.timeMin, de.timeSec);
	auto file      = std::make_unique<isoFile>(
                shared_from_this(), name, &file_stat, EXTENT_LOCATION(de) * ISO_FRAMESIZE);
	file->flags = flags;

	return file;
}

std::unique_ptr<DOS_File> isoDrive::FileCreate(const char* /*name*/,
                                               FatAttributeFlags /*attributes*/)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return nullptr;
}

bool isoDrive::FileUnlink(const char* /*name*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::RemoveDir(const char* /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::MakeDir(const char* /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::TestDir(const char *dir) {
	isoDirEntry de;
	return (lookup(&de, dir) && IS_DIR(FLAGS1));
}

bool isoDrive::FindFirst(const char* dir, DOS_DTA& dta, bool fcb_findfirst)
{
	isoDirEntry de;
	if (!lookup(&de, dir)) {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}

	// get a directory iterator and save its id in the dta
	int dirIterator = GetDirIterator(&de);
	bool isRoot = (*dir == 0);
	dirIterators[dirIterator].root = isRoot;
	dta.SetDirID((uint16_t)dirIterator);

	FatAttributeFlags attr = {};
	char pattern[ISO_MAXPATHNAME];
	dta.GetSearchParams(attr, pattern);

	if (attr == FatAttributeFlags::Volume) {
		dta.SetResult(discLabel, 0, 0, 0, FatAttributeFlags::Volume);
		return true;
	} else if (attr.volume && isRoot && !fcb_findfirst) {
		if (wild_file_cmp(discLabel, pattern)) {
			// Get Volume Label and only in basedir and if it
			// matches the searchstring
			dta.SetResult(discLabel, 0, 0, 0, FatAttributeFlags::Volume);
			return true;
		}
	}

	return FindNext(dta);
}

bool isoDrive::FindNext(DOS_DTA& dta)
{
	FatAttributeFlags attr = {};
	char pattern[DOS_NAMELENGTH_ASCII];
	dta.GetSearchParams(attr, pattern);

	int dirIterator = dta.GetDirID();
	bool isRoot = dirIterators[dirIterator].root;

	FatAttributeFlags attr_mask = {};
	attr_mask.directory         = true;
	attr_mask.hidden            = true;
	attr_mask.system            = true;

	isoDirEntry de;
	while (GetNextDirEntry(dirIterator, &de)) {
		FatAttributeFlags findAttr = {};
		findAttr.directory         = IS_DIR(FLAGS1);
		findAttr.hidden            = IS_HIDDEN(FLAGS1);

		if (!IS_ASSOC(FLAGS1) && !(isRoot && de.ident[0] == '.') &&
		    wild_file_cmp((char*)de.ident, pattern) &&
		    !(~(attr._data) & findAttr._data & attr_mask._data)) {
			/* file is okay, setup everything to be copied in DTA
			 * Block */
			char findName[DOS_NAMELENGTH_ASCII];
			findName[0] = 0;
			if(strlen((char*)de.ident) < DOS_NAMELENGTH_ASCII) {
				safe_strcpy(findName, (char *)de.ident);
				upcase(findName);
			}
			uint32_t findSize = DATA_LENGTH(de);
			uint16_t findDate = DOS_PackDate(1900 + de.dateYear, de.dateMonth, de.dateDay);
			uint16_t findTime = DOS_PackTime(de.timeHour, de.timeMin, de.timeSec);
			findAttr.read_only = true;
			dta.SetResult(findName, findSize, findDate, findTime, findAttr);
			return true;
		}
	}
	// after searching the directory, free the iterator
	FreeDirIterator(dirIterator);

	DOS_SetError(DOSERR_NO_MORE_FILES);
	return false;
}

bool isoDrive::Rename(const char* /*oldname*/, const char* /*newname*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::GetFileAttr(const char* name, FatAttributeFlags* attr)
{
	*attr = {};
	isoDirEntry de;
	bool success = lookup(&de, name);
	if (success) {
		attr->read_only = true;
		if (IS_HIDDEN(FLAGS1)) {
			attr->hidden = true;
		}
		if (IS_DIR(FLAGS1)) {
			attr->directory = true;
		}
	}
	return success;
}

bool isoDrive::SetFileAttr(const char* name,
                           [[maybe_unused]] const FatAttributeFlags attr)
{
	isoDirEntry de;
	if (lookup(&de, name))
		DOS_SetError(DOSERR_ACCESS_DENIED);
	else
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
	return false;
}

bool isoDrive::AllocationInfo(uint16_t *bytes_sector, uint8_t *sectors_cluster, uint16_t *total_clusters, uint16_t *free_clusters) {
	*bytes_sector = 2048;
	*sectors_cluster = 1; // cluster size for cdroms ?
	*total_clusters = 65535;
	*free_clusters = 0;
	return true;
}

bool isoDrive::FileExists(const char *name) {
	isoDirEntry de;
	return (lookup(&de, name) && !IS_DIR(FLAGS1));
}

uint8_t isoDrive::GetMediaByte(void) {
	return mediaid;
}

bool isoDrive::IsRemote(void) {
	return true;
}

bool isoDrive::IsRemovable(void) {
	return true;
}

Bits isoDrive::UnMount()
{
	return MSCDEX_RemoveDrive(driveLetter) ? 0 : 2;
}

int isoDrive::GetDirIterator(const isoDirEntry* de) {
	int dirIterator = nextFreeDirIterator;

	// get start and end sector of the directory entry (pad end sector if necessary)
	dirIterators[dirIterator].currentSector = EXTENT_LOCATION(*de);
	dirIterators[dirIterator].endSector =
		EXTENT_LOCATION(*de) + DATA_LENGTH(*de) / ISO_FRAMESIZE - 1;
	if (DATA_LENGTH(*de) % ISO_FRAMESIZE != 0)
		dirIterators[dirIterator].endSector++;

	// reset position and mark as valid
	dirIterators[dirIterator].pos = 0;
	dirIterators[dirIterator].valid = true;

	// advance to next directory iterator (wrap around if necessary)
	nextFreeDirIterator = (nextFreeDirIterator + 1) % MAX_OPENDIRS;

	return dirIterator;
}

bool isoDrive::GetNextDirEntry(const int dirIteratorHandle, isoDirEntry* de) {
	bool result = false;
	uint8_t* buffer = nullptr;
	DirIterator& dirIterator = dirIterators[dirIteratorHandle];

	// check if the directory entry is valid
	if (dirIterator.valid && ReadCachedSector(&buffer, dirIterator.currentSector)) {
		// check if the next sector has to be read
		if ((dirIterator.pos >= ISO_FRAMESIZE)
		 || (buffer[dirIterator.pos] == 0)
		 || (dirIterator.pos + buffer[dirIterator.pos] > ISO_FRAMESIZE)) {

			// check if there is another sector available
		 	if (dirIterator.currentSector < dirIterator.endSector) {
			 	dirIterator.pos = 0;
			 	dirIterator.currentSector++;
			 	if (!ReadCachedSector(&buffer, dirIterator.currentSector)) {
			 		return false;
			 	}
		 	} else {
		 		return false;
		 	}
		 }
		 // read sector and advance sector pointer
		 const int length = readDirEntry(de, &buffer[dirIterator.pos]);
		 result = length >= 0;
		 if (length >= 0)
			 dirIterator.pos += static_cast<unsigned>(length);
		 else // read failed, so step back to our prior iterator
			 dirIterator.pos--;
	}
	return result;
}

void isoDrive::FreeDirIterator(const int dirIterator) {
	dirIterators[dirIterator].valid = false;

	// if this was the last aquired iterator decrement nextFreeIterator
	if ((dirIterator + 1) % MAX_OPENDIRS == nextFreeDirIterator) {
		if (nextFreeDirIterator>0) {
			nextFreeDirIterator--;
		} else {
			nextFreeDirIterator = MAX_OPENDIRS-1;
		}
	}
}

bool isoDrive::ReadCachedSector(uint8_t** buffer, const uint32_t sector) {
	// get hash table entry
	int pos = sector % ISO_MAX_HASH_TABLE_SIZE;
	SectorHashEntry& he = sectorHashEntries[pos];

	// check if the entry is valid and contains the correct sector
	if (!he.valid || he.sector != sector) {
		if (!CDROM::cdroms[subUnit]->ReadSector(he.data, false, sector)) {
			return false;
		}
		he.valid = true;
		he.sector = sector;
	}

	*buffer = he.data;
	return true;
}

inline bool isoDrive::readSector(uint8_t *buffer, uint32_t sector) {
	return CDROM::cdroms[subUnit]->ReadSector(buffer, false, sector);
}

int isoDrive :: readDirEntry(isoDirEntry *de, uint8_t *data) {
	// copy data into isoDirEntry struct, data[0] = length of DirEntry
//	if (data[0] > sizeof(isoDirEntry)) return -1;//check disabled as isoDirentry is currently 258 bytes large. So it always fits
	memcpy(de, data, data[0]);//Perharps care about a zero at the end.

	// xa not supported
	if (de->extAttrLength != 0) return -1;
	// interleaved mode not supported
	if (de->fileUnitSize != 0 || de->interleaveGapSize != 0) return -1;

	// modify file identifier for use with dosbox
	if ((de->length < 33 + de->fileIdentLength)) return -1;
	if (IS_DIR(FLAGS2)) {
		if (de->fileIdentLength == 1 && de->ident[0] == 0)
			strcpy(reinterpret_cast<char *>(de->ident), ".");
		else if (de->fileIdentLength == 1 && de->ident[0] == 1)
			strcpy(reinterpret_cast<char *>(de->ident), "..");
		else {
			if (de->fileIdentLength > 200) return -1;
			de->ident[de->fileIdentLength] = 0;
		}
	} else {
		if (de->fileIdentLength > 200) return -1;
		de->ident[de->fileIdentLength] = 0;
		// remove any file version identifiers as there are some cdroms that don't have them
		strreplace((char*)de->ident, ';', 0);
		// if file has no extension remove the trailing dot
		size_t tmp = strlen((char*)de->ident);
		if (tmp > 0) {
			if (de->ident[tmp - 1] == '.') de->ident[tmp - 1] = 0;
		}
	}
	char* dotpos = strchr((char*)de->ident, '.');
	if (dotpos!=nullptr) {
		if (strlen(dotpos)>4) dotpos[4]=0;
		if (dotpos-(char*)de->ident>8) {
			constexpr int pos = 8;
			const auto maxlen = ARRAY_LEN(de->ident) - pos;
			const auto sub_ident = reinterpret_cast<char *>(de->ident + pos);
			snprintf(sub_ident, maxlen, "%s", dotpos);
		}
	} else if (strlen((char*)de->ident)>8) de->ident[8]=0;
	return de->length;
}

bool isoDrive :: loadImage() {
	uint8_t pvd[BYTES_PER_COOKED_REDBOOK_FRAME];
	dataCD = false;
	readSector(pvd, ISO_FIRST_VD);
	if (pvd[0] == 1 && !strncmp((char*)(&pvd[1]), "CD001", 5) && pvd[6] == 1) iso = true;
	else if (pvd[8] == 1 && !strncmp((char*)(&pvd[9]), "CDROM", 5) && pvd[14] == 1) iso = false;
	else return false;
	uint16_t offset = iso ? 156 : 180;
	if (readDirEntry(&this->rootEntry, &pvd[offset])>0) {
		dataCD = true;
		return true;
	}
	return false;
}

bool isoDrive :: lookup(isoDirEntry *de, const char *path) {
	if (!dataCD) return false;
	*de = this->rootEntry;
	if (!strcmp(path, "")) return true;

	char isoPath[ISO_MAXPATHNAME];
	safe_strcpy(isoPath, path);
	strreplace(isoPath, '\\', '/');

	// iterate over all path elements (name), and search each of them in the current de
	for(char* name = strtok(isoPath, "/"); nullptr != name; name = strtok(nullptr, "/")) {

		bool found = false;
		// current entry must be a directory, abort otherwise
		if (IS_DIR(FLAGS2)) {

			// remove the trailing dot if present
			size_t nameLength = strlen(name);
			if (nameLength > 0) {
				if (name[nameLength - 1] == '.') name[nameLength - 1] = 0;
			}

			// look for the current path element
			int dirIterator = GetDirIterator(de);
			while (!found && GetNextDirEntry(dirIterator, de)) {
				if (!IS_ASSOC(FLAGS2) && (0 == strncasecmp((char*) de->ident, name, ISO_MAX_FILENAME_LENGTH))) {
					found = true;
				}
			}
			FreeDirIterator(dirIterator);
		}
		if (!found) return false;
	}
	return true;
}
