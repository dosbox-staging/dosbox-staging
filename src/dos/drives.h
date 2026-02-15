// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DRIVES_H
#define DOSBOX_DRIVES_H

#include "dosbox.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "config/setup.h"
#include "dos/dos.h"
#include "dos/dos_system.h"

// GCC throws a warning about non-virtual destructor for std::enable_shared_from_this
// This is normally a helpful warning. Ex: If DOS_Drive had a non-virtual destructor, it would be a problem.
// In this case, it is safe so hide the warning for this file.
// It gets restored at the end.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"

void Set_Label(const char* const input, char* const output, bool cdrom);
std::string To_Label(const char* name);
std::string generate_8x3(const char *lfn, const unsigned int num, const bool start = false);
bool filename_not_8x3(const char *n);
bool filename_not_strict_8x3(const char *n);
char *VFILE_Generate_8x3(const char *name, const unsigned int onpos);

class imageDisk; // forward declare

class DriveManager {
public:
	using filesystem_images_t = std::vector<std::shared_ptr<DOS_Drive>>;
	struct DriveInfo {
		filesystem_images_t disks = {};
		uint16_t current_disk     = 0;
	};
	using drive_infos_t = std::array<DriveInfo, DOS_DRIVES>;

	static void AppendFilesystemImages(const int drive,
	                                   const filesystem_images_t& filesystem_images);

	static void RegisterFilesystemImage(const int drive,
	                                    std::shared_ptr<DOS_Drive> filesystem_image);

	static void InitializeDrive(int drive);
	static int UnmountDrive(int drive);
//	static void CycleDrive(bool pressed);
//	static void CycleDisk(bool pressed);
	static void CycleDisks(int drive, bool notify);
	static void CycleAllDisks(void);
	static char *GetDrivePosition(int drive);

	static void Init();

private:
	static drive_infos_t drive_infos;
};

// Must be constructed with a shared_ptr as it uses weak_from_this()
class localDrive : public DOS_Drive,
                   public std::enable_shared_from_this<localDrive> {
public:
	localDrive(const char* startdir, uint16_t _bytes_sector,
	           uint8_t _sectors_cluster, uint16_t _total_clusters,
	           uint16_t _free_clusters, uint8_t _mediaid,
	           bool _readonly, bool _always_open_ro_files = false);
	std::unique_ptr<DOS_File> FileOpen(const char* name, uint8_t flags) override;
	std::unique_ptr<DOS_File> FileCreate(const char* name,
	                                     FatAttributeFlags attributes) override;
	FILE* GetHostFilePtr(const char* const name, const char* const type);
	std::string MapDosToHostFilename(const char* const dos_name);
	bool FileUnlink(const char* name) override;
	bool RemoveDir(const char* dir) override;
	bool MakeDir(const char* dir) override;
	bool TestDir(const char* dir) override;
	bool FindFirst(const char* _dir, DOS_DTA& dta, bool fcb_findfirst = false) override;
	bool FindNext(DOS_DTA& dta) override;
	bool GetFileAttr(const char* name, FatAttributeFlags* attr) override;
	bool SetFileAttr(const char* name, const FatAttributeFlags attr) override;
	bool Rename(const char* oldname, const char* newname) override;
	bool AllocationInfo(uint16_t* _bytes_sector, uint8_t* _sectors_cluster,
	                    uint16_t* _total_clusters,
	                    uint16_t* _free_clusters) override;
	bool FileExists(const char* name) override;
	uint8_t GetMediaByte(void) override;
	bool IsReadOnly() const override { return readonly; }
	bool IsRemote(void) override;
	bool IsRemovable(void) override;
	Bits UnMount(void) override;
	const char* GetBasedir() const
	{
		return basedir;
	}

	std::unordered_map<std::string, DosDateTime> timestamp_cache = {};

protected:
	char basedir[CROSS_LEN] = "";
	struct {
		char srch_dir[CROSS_LEN] = "";
	} srchInfo[MAX_OPENDIRS];

private:
	void MaybeLogFilesystemProtection(const std::string& filename);
	bool FileIsReadOnly(const char* name);
	bool FileOrDriveIsReadOnly(const char* name);
	const bool readonly;
	const bool always_open_ro_files;
	std::unordered_set<std::string> write_protected_files;
	struct {
		uint16_t bytes_sector;
		uint8_t sectors_cluster;
		uint16_t total_clusters;
		uint16_t free_clusters;
		uint8_t mediaid;
	} allocation;
};

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct bootstrap {
	uint8_t  nearjmp[3];
	uint8_t  oemname[8];
	uint16_t bytespersector;
	uint8_t  sectorspercluster;
	uint16_t reservedsectors;
	uint8_t  fatcopies;
	uint16_t rootdirentries;
	uint16_t totalsectorcount;
	uint8_t  mediadescriptor;
	uint16_t sectorsperfat;
	uint16_t sectorspertrack;
	uint16_t headcount;
	/* 32-bit FAT extensions */
	uint32_t hiddensectorcount;
	uint32_t totalsecdword;
	uint8_t  bootcode[474];
	uint8_t  magic1; /* 0x55 */
	uint8_t  magic2; /* 0xaa */
} GCC_ATTRIBUTE(packed);

struct direntry {
	uint8_t entryname[11];
	uint8_t attrib;
	uint8_t NTRes;
	uint8_t milliSecondStamp;
	uint16_t crtTime;
	uint16_t crtDate;
	uint16_t accessDate;
	uint16_t hiFirstClust;
	uint16_t modTime;
	uint16_t modDate;
	uint16_t loFirstClust;
	uint32_t entrysize;
} GCC_ATTRIBUTE(packed);

struct partTable {
	uint8_t booter[446];
	struct {
		uint8_t bootflag;
		uint8_t beginchs[3];
		uint8_t parttype;
		uint8_t endchs[3];
		uint32_t absSectStart;
		uint32_t partSize;
	} pentry[4];
	uint8_t  magic1; /* 0x55 */
	uint8_t  magic2; /* 0xaa */
} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack ()
#endif

// Must be constructed with a shared_ptr or it will throw an exception on internal call to shared_from_this()
class fatDrive final : public DOS_Drive, public std::enable_shared_from_this<fatDrive> {
public:
	fatDrive(const char* sysFilename, uint32_t bytesector,
	         uint32_t cylsector, uint32_t headscyl, uint32_t cylinders,
	         uint8_t mediaid, bool roflag);
	fatDrive(const fatDrive&)            = delete; // prevent copying
	fatDrive& operator=(const fatDrive&) = delete; // prevent assignment
	std::unique_ptr<DOS_File> FileOpen(const char* name, uint8_t flags) override;
	std::unique_ptr<DOS_File> FileCreate(const char* name,
	                                     FatAttributeFlags attributes) override;
	bool FileUnlink(const char* name) override;
	bool RemoveDir(const char* dir) override;
	bool MakeDir(const char* dir) override;
	bool TestDir(const char* dir) override;
	bool FindFirst(const char* _dir, DOS_DTA& dta, bool fcb_findfirst = false) override;
	bool FindNext(DOS_DTA& dta) override;
	bool GetFileAttr(const char* name, FatAttributeFlags* attr) override;
	bool SetFileAttr(const char* name, const FatAttributeFlags attr) override;
	bool Rename(const char* oldname, const char* newname) override;
	bool AllocationInfo(uint16_t* _bytes_sector, uint8_t* _sectors_cluster,
	                    uint16_t* _total_clusters,
	                    uint16_t* _free_clusters) override;
	bool FileExists(const char* name) override;
	uint8_t GetMediaByte(void) override;
	bool IsReadOnly() const override { return readonly; }
	bool IsRemote(void) override;
	bool IsRemovable(void) override;
	Bits UnMount(void) override;
	void EmptyCache(void) override {}

public:
	uint8_t readSector(uint32_t sectnum, void * data);
	uint8_t writeSector(uint32_t sectnum, void * data);
	uint32_t getAbsoluteSectFromBytePos(uint32_t startClustNum, uint32_t bytePos);
	uint32_t getSectorCount();
	uint32_t getSectorSize(void);
	uint32_t getClusterSize(void);
	uint32_t getAbsoluteSectFromChain(uint32_t startClustNum, uint32_t logicalSector);
	bool allocateCluster(uint32_t useCluster, uint32_t prevCluster);
	uint32_t appendCluster(uint32_t startCluster);
	void deleteClustChain(uint32_t startCluster, uint32_t bytePos);
	uint32_t getFirstFreeClust(void);
	bool directoryBrowse(uint32_t dirClustNumber, direntry *useEntry, int32_t entNum, int32_t start=0);
	bool directoryChange(uint32_t dirClustNumber, direntry *useEntry, int32_t entNum);
	std::shared_ptr<imageDisk> loadedDisk;
	bool created_successfully;
	uint32_t partSectOff;

private:
	uint32_t getClusterValue(uint32_t clustNum);
	void setClusterValue(uint32_t clustNum, uint32_t clustValue);
	uint32_t getClustFirstSect(uint32_t clustNum);
	bool FindNextInternal(uint32_t dirClustNumber, DOS_DTA & dta, direntry *foundEntry);
	bool getDirClustNum(const char * dir, uint32_t * clustNum, bool parDir);
	bool getFileDirEntry(const char* const filename, direntry* useEntry,
	                     uint32_t* dirClust, uint32_t* subEntry,
	                     const bool dir_ok = false);
	bool addDirectoryEntry(uint32_t dirClustNumber, direntry useEntry);
	void zeroOutCluster(uint32_t clustNumber);
	bool getEntryName(const char *fullname, char *entname);

	uint8_t mediaid;
	bootstrap bootbuffer;
	bool absolute;
	bool readonly;
	uint8_t fattype;
	uint32_t CountOfClusters;
	uint32_t firstDataSector;
	uint32_t firstRootDirSect;

	uint32_t cwdDirCluster;

	uint8_t fatSectBuffer[1024];
	uint32_t curFatSect;
};

class cdromDrive final : public localDrive
{
public:
	cdromDrive(const char _driveLetter, const char* startdir,
	           uint16_t _bytes_sector, uint8_t _sectors_cluster,
	           uint16_t _total_clusters, uint16_t _free_clusters,
	           uint8_t _mediaid, int& error);
	std::unique_ptr<DOS_File> FileOpen(const char* name, uint8_t flags) override;
	std::unique_ptr<DOS_File> FileCreate(const char* name,
	                                     FatAttributeFlags attributes) override;
	bool FileUnlink(const char* name) override;
	bool RemoveDir(const char* dir) override;
	bool MakeDir(const char* dir) override;
	bool Rename(const char* oldname, const char* newname) override;
	bool GetFileAttr(const char* name, FatAttributeFlags* attr) override;
	bool FindFirst(const char* _dir, DOS_DTA& dta, bool fcb_findfirst = false) override;
	void SetDir(const char* path) override;
	bool IsRemote(void) override;
	bool IsRemovable(void) override;
	Bits UnMount(void) override;

private:
	uint8_t subUnit;
	char driveLetter;
};

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct isoPVD {
	uint8_t type;
	uint8_t standardIdent[5];
	uint8_t version;
	uint8_t unused1;
	uint8_t systemIdent[32];
	uint8_t volumeIdent[32];
	uint8_t unused2[8];
	uint32_t volumeSpaceSizeL;
	uint32_t volumeSpaceSizeM;
	uint8_t unused3[32];
	uint16_t volumeSetSizeL;
	uint16_t volumeSetSizeM;
	uint16_t volumeSeqNumberL;
	uint16_t volumeSeqNumberM;
	uint16_t logicBlockSizeL;
	uint16_t logicBlockSizeM;
	uint32_t pathTableSizeL;
	uint32_t pathTableSizeM;
	uint32_t locationPathTableL;
	uint32_t locationOptPathTableL;
	uint32_t locationPathTableM;
	uint32_t locationOptPathTableM;
	uint8_t rootEntry[34];
	uint32_t unused4[1858];
} GCC_ATTRIBUTE(packed);

struct isoDirEntry {
	uint8_t length;
	uint8_t extAttrLength;
	uint32_t extentLocationL;
	uint32_t extentLocationM;
	uint32_t dataLengthL;
	uint32_t dataLengthM;
	uint8_t dateYear;
	uint8_t dateMonth;
	uint8_t dateDay;
	uint8_t timeHour;
	uint8_t timeMin;
	uint8_t timeSec;
	uint8_t timeZone;
	uint8_t fileFlags;
	uint8_t fileUnitSize;
	uint8_t interleaveGapSize;
	uint16_t VolumeSeqNumberL;
	uint16_t VolumeSeqNumberM;
	uint8_t fileIdentLength;
	uint8_t ident[222];
} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack ()
#endif

#if defined (WORDS_BIGENDIAN)
#define EXTENT_LOCATION(de)	((de).extentLocationM)
#define DATA_LENGTH(de)		((de).dataLengthM)
#else
#define EXTENT_LOCATION(de)	((de).extentLocationL)
#define DATA_LENGTH(de)		((de).dataLengthL)
#endif

#define ISO_FRAMESIZE		2048
#define ISO_ASSOCIATED		4
#define ISO_DIRECTORY		2
#define ISO_HIDDEN		1
#define ISO_MAX_FILENAME_LENGTH 37
#define ISO_MAXPATHNAME		256
#define ISO_FIRST_VD		16
#define IS_ASSOC(fileFlags)	(!!(fileFlags & ISO_ASSOCIATED))
#define IS_DIR(fileFlags)	(!!(fileFlags & ISO_DIRECTORY))
#define IS_HIDDEN(fileFlags)	(!!(fileFlags & ISO_HIDDEN))
#define ISO_MAX_HASH_TABLE_SIZE 	100

// Must be constructed with a shared_ptr or it will throw an exception on internal call to shared_from_this()
class isoDrive final : public DOS_Drive, public std::enable_shared_from_this<isoDrive> {
public:
	isoDrive(char driveLetter, const char* device_name, uint8_t mediaid,
	         int& error);
	~isoDrive() override = default;
	std::unique_ptr<DOS_File> FileOpen(const char* name, uint8_t flags) override;
	std::unique_ptr<DOS_File> FileCreate(const char* name,
	                                     FatAttributeFlags attributes) override;
	bool FileUnlink(const char* name) override;
	bool RemoveDir(const char* dir) override;
	bool MakeDir(const char* dir) override;
	bool TestDir(const char* dir) override;
	bool FindFirst(const char* _dir, DOS_DTA& dta, bool fcb_findfirst) override;
	bool FindNext(DOS_DTA& dta) override;
	bool GetFileAttr(const char* name, FatAttributeFlags* attr) override;
	bool SetFileAttr(const char* name, const FatAttributeFlags attr) override;
	bool Rename(const char* oldname, const char* newname) override;
	bool AllocationInfo(uint16_t* bytes_sector, uint8_t* sectors_cluster,
	                    uint16_t* total_clusters,
	                    uint16_t* free_clusters) override;
	bool FileExists(const char* name) override;
	uint8_t GetMediaByte(void) override;
	void EmptyCache(void) override {}
	bool IsReadOnly() const override { return true; }
	bool IsRemote(void) override;
	bool IsRemovable(void) override;
	Bits UnMount(void) override;
	bool readSector(uint8_t* buffer, uint32_t sector);
	const char* GetLabel() override
	{
		return discLabel;
	}
	void Activate(void) override;

private:
	int  readDirEntry(isoDirEntry *de, uint8_t *data);
	bool loadImage();
	bool lookupSingle(isoDirEntry *de, const char *name, uint32_t sectorStart, uint32_t length);
	bool lookup(isoDirEntry *de, const char *path);
	int  UpdateMscdex(char driveLetter, const char* physicalPath, uint8_t& subUnit);
	int  GetDirIterator(const isoDirEntry* de);
	bool GetNextDirEntry(const int dirIterator, isoDirEntry* de);
	void FreeDirIterator(const int dirIterator);
	bool ReadCachedSector(uint8_t** buffer, const uint32_t sector);
	
	struct DirIterator {
		bool valid;
		bool root;
		uint32_t currentSector;
		uint32_t endSector;
		uint32_t pos;
	} dirIterators[MAX_OPENDIRS];
	
	int nextFreeDirIterator;
	
	struct SectorHashEntry {
		bool valid;
		uint32_t sector;
		uint8_t data[ISO_FRAMESIZE];
	} sectorHashEntries[ISO_MAX_HASH_TABLE_SIZE];

	bool iso;
	bool dataCD;
	isoDirEntry rootEntry;
	uint8_t mediaid;
	char fileName[CROSS_LEN];
	uint8_t subUnit;
	char driveLetter;
	char discLabel[32];
};

class VFILE_Block;
using vfile_block_t = std::shared_ptr<VFILE_Block>;

class Virtual_Drive final : public DOS_Drive {
public:
	Virtual_Drive();
	std::unique_ptr<DOS_File> FileOpen(const char* name, uint8_t flags) override;
	std::unique_ptr<DOS_File> FileCreate(const char* name,
	                                     FatAttributeFlags attributes) override;
	bool FileUnlink(const char* name) override;
	bool RemoveDir(const char* dir) override;
	bool MakeDir(const char* dir) override;
	bool TestDir(const char* dir) override;
	bool FindFirst(const char* _dir, DOS_DTA& dta, bool fcb_findfirst) override;
	bool FindNext(DOS_DTA& dta) override;
	bool GetFileAttr(const char* name, FatAttributeFlags* attr) override;
	bool SetFileAttr(const char* name, const FatAttributeFlags attr) override;
	bool Rename(const char* oldname, const char* newname) override;
	bool AllocationInfo(uint16_t* _bytes_sector, uint8_t* _sectors_cluster,
	                    uint16_t* _total_clusters,
	                    uint16_t* _free_clusters) override;
	bool FileExists(const char* name) override;
	uint8_t GetMediaByte() override;
	void EmptyCache() override;
	bool IsReadOnly() const override { return true; }
	bool IsRemote() override;
	bool IsRemovable() override;
	Bits UnMount() override;
	const char* GetLabel() override;

private:
	Virtual_Drive(const Virtual_Drive&); // prevent copying
	Virtual_Drive& operator= (const Virtual_Drive&); // prevent assignment
	vfile_block_t search_file;
	bool is_name_equal(vfile_block_t file_block, const char* name) const;
	vfile_block_t find_vfile_by_name(const char* name) const;
	vfile_block_t find_vfile_dir_by_name(const char* dir) const;
	bool vfile_name_exists(const std::string& name) const;
};

// Must be constructed with a shared_ptr as it uses weak_from_this()
// std::enable_shared_from_this inherited from localDrive
class Overlay_Drive final : public localDrive {
public:
	Overlay_Drive(const char *startdir,
	              const char *overlay,
	              uint16_t _bytes_sector,
	              uint8_t _sectors_cluster,
	              uint16_t _total_clusters,
	              uint16_t _free_clusters,
	              uint8_t _mediaid,
	              uint8_t &error);

	std::unique_ptr<DOS_File> FileOpen(const char* name, uint8_t flags) override;
	std::unique_ptr<DOS_File> FileCreate(const char* name,
	                                     FatAttributeFlags attributes) override;
	bool FindFirst(const char* _dir, DOS_DTA& dta, bool fcb_findfirst) override;
	bool FindNext(DOS_DTA& dta) override;
	bool FileUnlink(const char* name) override;
	bool GetFileAttr(const char* name, FatAttributeFlags* attr) override;
	bool SetFileAttr(const char* name, const FatAttributeFlags attr) override;
	bool FileExists(const char* name) override;
	bool Rename(const char* oldname, const char* newname) override;
	void EmptyCache(void) override;

	std::pair<NativeFileHandle, std::string> create_file_in_overlay(
	        const char* dos_filename, const FatAttributeFlags attributes);

	Bits UnMount(void) override;
	bool TestDir(const char* dir) override;
	bool RemoveDir(const char* dir) override;
	bool MakeDir(const char* dir) override;

private:
	char overlaydir[CROSS_LEN];
	bool Sync_leading_dirs(const char* dos_filename);
	void add_DOSname_to_cache(const char* name);
	void remove_DOSname_from_cache(const char* name);
	void add_DOSdir_to_cache(const char* name);
	void remove_DOSdir_from_cache(const char* name);
	void update_cache(bool read_directory_contents = false);

	std::vector<std::string> deleted_files_in_base; //Set is probably better, or some other solution (involving the disk).
	std::vector<std::string> deleted_paths_in_base; //Currently only used to hide the overlay folder.
	std::string overlap_folder;
	void add_deleted_file(const char* name, bool create_on_disk);
	void remove_deleted_file(const char* name, bool create_on_disk);
	bool is_deleted_file(const char* name);
	void add_deleted_path(const char* name, bool create_on_disk);
	void remove_deleted_path(const char* name, bool create_on_disk);
	bool is_deleted_path(const char* name);
	bool check_if_leading_is_deleted(const char* name);

	bool is_dir_only_in_overlay(const char* name); //cached


	void remove_special_file_from_disk(const char* dosname, const char* operation);
	void add_special_file_to_disk(const char* dosname, const char* operation);
	std::string create_filename_of_special_operation(const char* dosname, const char* operation);
	void convert_overlay_to_DOSname_in_base(char* dirname );
	//For caching the update_cache routine.
	std::vector<std::string> DOSnames_cache; //Also set is probably better.
	std::vector<std::string> DOSdirs_cache; //Can not blindly change its type. it is important that subdirs come after the parent directory.
	const std::string special_prefix;
};

#pragma GCC diagnostic pop

void DRIVES_Init();

#endif // DOSBOX_DRIVES_H
