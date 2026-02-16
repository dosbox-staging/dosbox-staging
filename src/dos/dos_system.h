// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DOS_SYSTEM_H
#define DOSBOX_DOS_SYSTEM_H

#include "dosbox.h"

#include <string>
#include <vector>

#include "utils/bit_view.h"
#include "misc/cross.h"
#include "utils/fs_utils.h"
#include "hardware/memory.h"
#include "misc/support.h"

#define DOS_NAMELENGTH 12
#define DOS_NAMELENGTH_ASCII (DOS_NAMELENGTH+1)
#define DOS_FCBNAME 15
#define DOS_DIRDEPTH 8
#define DOS_PATHLENGTH 80
#define DOS_TEMPSIZE 1024
#define DOS_MFNLENGTH 8
#define DOS_EXTLENGTH 3

#define LFN_NAMELENGTH 255

constexpr auto CurrentDirectory = ".";
constexpr auto ParentDirectory  = "..";
constexpr auto DosSeparator     = '\\';

union FatAttributeFlags {
	enum : uint8_t {
		ReadOnly  = bit::literals::b0,
		Hidden    = bit::literals::b1,
		System    = bit::literals::b2,
		Volume    = bit::literals::b3,
		Directory = bit::literals::b4,
		Archive   = bit::literals::b5,
		Device    = bit::literals::b6,
		NotVolume = bit::mask_flip_all(Volume),
	};

	uint8_t _data = 0;

	bit_view<0, 1> read_only;
	bit_view<1, 1> hidden;
	bit_view<2, 1> system;
	bit_view<3, 1> volume;
	bit_view<4, 1> directory;
	bit_view<5, 1> archive;
	bit_view<6, 1> device;
	bit_view<7, 1> unused;

	FatAttributeFlags() : _data(0) {}
	FatAttributeFlags(const uint8_t data) : _data(data) {}
	FatAttributeFlags(const FatAttributeFlags& other) : _data(other._data) {}

	FatAttributeFlags& operator=(const FatAttributeFlags& other)
	{
		_data = other._data;
		return *this;
	}

	bool operator==(const FatAttributeFlags& other) const
	{
		return _data == other._data;
	}
};

struct FileStat_Block {
	uint32_t size;
	uint16_t time;
	uint16_t date;
	uint16_t attr;
};

class DOS_DTA;

struct DosFilename {
	std::string name = {};
	std::string ext  = {};
};

struct FileRegionLock {
	uint32_t pos = 0;
	uint32_t len = 0;
};

enum class FlushTimeOnClose {
	NoUpdate,
	ManuallySet,
	CurrentTime,
};

class DOS_File {
public:
	DOS_File() = default;
	DOS_File(const DOS_File &orig) = default;
	DOS_File &operator=(const DOS_File &orig);

	virtual ~DOS_File() = default;

	const char *GetName() const { return name.c_str(); }

	void SetName(const char *str) { name = str; }

	bool IsName(const char *str) const
	{
		return !name.empty() && (strcasecmp(name.c_str(), str) == 0);
	}

	virtual bool	Read(uint8_t * data,uint16_t * size)=0;
	virtual bool	Write(uint8_t * data,uint16_t * size)=0;
	virtual bool	Seek(uint32_t * pos,uint32_t type)=0;
	virtual void	Close()=0;
	virtual uint16_t	GetInformation(void)=0;
	virtual bool IsOnReadOnlyMedium() const = 0;

	virtual void AddRef() { refCtr++; }
	virtual Bits RemoveRef() { return --refCtr; }

	void SetDrive(uint8_t drv) { hdrive=drv;}
	uint8_t GetDrive() const { return hdrive;}
	uint8_t flags    = 0;
	uint16_t time    = 0;
	uint16_t date    = 0;
	FatAttributeFlags attr = {};
	Bits refCtr      = 0;
	std::string name = {};
	FlushTimeOnClose flush_time_on_close = FlushTimeOnClose::NoUpdate;
	std::vector<FileRegionLock> region_locks = {};
	/* Some Device Specific Stuff */
private:
	uint8_t hdrive = 0xff;
};

class DOS_Device : public DOS_File {
public:
	DOS_Device() : DOS_File(), devnum(0) {}

	DOS_Device(const DOS_Device& orig) = default;
	DOS_Device &operator=(const DOS_Device &orig) = default;

	bool Read(uint8_t* data, uint16_t* size) override;
	bool Write(uint8_t* data, uint16_t* size) override;
	bool Seek(uint32_t* pos, uint32_t type) override;
	void Close() override;
	uint16_t GetInformation(void) override;
	bool IsOnReadOnlyMedium() const override { return false; }
	virtual bool ReadFromControlChannel(PhysPt bufptr, uint16_t size,
	                                    uint16_t* retcode);
	virtual bool WriteToControlChannel(PhysPt bufptr, uint16_t size,
	                                   uint16_t* retcode);
	virtual uint8_t GetStatus(bool input_flag);
	void SetDeviceNumber(Bitu num)
	{
		devnum = num;
	}

private:
	Bitu devnum;
};

/* The following variable can be lowered to free up some memory.
 * The negative side effect: The stored searches will be turned over faster.
 * Should not have impact on systems with few directory entries. */
#define MAX_OPENDIRS 2048
//Can be high as it's only storage (16 bit variable)

class DOS_Drive_Cache final {
public:
	enum TDirSort { NOSORT, ALPHABETICAL, DIRALPHABETICAL, ALPHABETICALREV, DIRALPHABETICALREV };
	DOS_Drive_Cache            (void);
	DOS_Drive_Cache            (const char* path);
	DOS_Drive_Cache            (const DOS_Drive_Cache&) = delete; // prevent copying
	DOS_Drive_Cache& operator= (const DOS_Drive_Cache&) = delete; // prevent assignment
	~DOS_Drive_Cache           (void);

	void SetBaseDir(const char *path);
	void SetDirSort(TDirSort sort) { sortDirType = sort; }

	bool  OpenDir              (const char* path, uint16_t& id);
	bool  ReadDir              (uint16_t id, char* &result);

	void ExpandNameAndNormaliseCase(char* path);
	char* GetExpandNameAndNormaliseCase(const char* path);
	bool GetShortName(const char* fullname, char* shortname);

	bool  FindFirst            (char* path, uint16_t& id);
	bool  FindNext             (uint16_t id, char* &result);

	void  CacheOut             (const char* path, bool ignoreLastDir = false);
	void  AddEntry             (const char* path, bool checkExist = false);
	void  AddEntryDirOverlay   (const char* path, bool checkExist = false);

	void  DeleteEntry          (const char* path, bool ignoreLastDir = false);
	void  EmptyCache           (void);

	void SetLabel(const char *name, bool cdrom, bool allowupdate);
	const char *GetLabel() const { return label; }

	class CFileInfo final {
	public:
		CFileInfo(void)
		        : orgname{0},
		          shortname{0},
		          isOverlayDir(false),
		          isDir(false),
		          id(MAX_OPENDIRS),
		          nextEntry(0),
		          shortNr(0),
		          fileList(0),
		          longNameList(0)
		{}

		~CFileInfo()
		{
			for (auto p : fileList) {
				delete p;
			}
			fileList.clear();
			longNameList.clear();
		}

		char        orgname[CROSS_LEN];
		char        shortname[DOS_NAMELENGTH_ASCII];
		bool        isOverlayDir;
		bool        isDir;
		uint16_t      id;
		Bitu        nextEntry;
		unsigned    shortNr;
		// contents
		std::vector<CFileInfo*> fileList;
		std::vector<CFileInfo*> longNameList;
	};

private:
	void ClearFileInfo(CFileInfo *dir);
	void DeleteFileInfo(CFileInfo *dir);

	bool		RemoveTrailingDot	(char* shortname);
	Bits		GetLongName		(CFileInfo* info, char* shortname, const size_t shortname_len);
	void		CreateShortName		(CFileInfo* dir, CFileInfo* info);
	unsigned        CreateShortNameID       (CFileInfo* dir, const char* name);
	int		CompareShortname	(const char* compareName, const char* shortName);
	bool		SetResult		(CFileInfo* dir, char * &result, Bitu entryNr);
	bool		IsCachedIn		(CFileInfo* dir);
	CFileInfo*	FindDirInfo		(const char* path, char* expandedPath);
	bool		RemoveSpaces		(char* str);
	bool		OpenDir			(CFileInfo* dir, const char* path, uint16_t& id);
	void		CreateEntry		(CFileInfo* dir, const char* name, bool is_directory);
	void		CopyEntry		(CFileInfo* dir, CFileInfo* from);
	uint16_t		GetFreeID		(CFileInfo* dir);
	void		Clear			(void);

	CFileInfo*	dirBase;
	char		dirPath				[CROSS_LEN];
	char		basePath			[CROSS_LEN];
	TDirSort	sortDirType;
	CFileInfo*	save_dir;
	char		save_path			[CROSS_LEN];
	char		save_expanded		[CROSS_LEN];

	uint16_t		srchNr;
	CFileInfo*	dirSearch			[MAX_OPENDIRS];
	CFileInfo*	dirFindFirst		[MAX_OPENDIRS];
	uint16_t		nextFreeFindFirst;

	char		label				[CROSS_LEN];
	bool		updatelabel;
};

enum class DosDriveType : uint16_t {
	Unknown = 0,
	Local   = 1,
	Cdrom   = 2,
	Fat     = 3,
	Iso     = 4,
	Virtual = 5,
};

class DOS_Drive {
public:
	DOS_Drive();
	virtual ~DOS_Drive() = default;

	virtual std::unique_ptr<DOS_File> FileOpen(const char* name,
	                                           uint8_t flags) = 0;
	virtual std::unique_ptr<DOS_File> FileCreate(const char* name,
	                                             FatAttributeFlags attributes) = 0;
	virtual bool FileUnlink(const char* name)                           = 0;
	virtual bool RemoveDir(const char* dir)                             = 0;
	virtual bool MakeDir(const char* dir)                               = 0;
	virtual bool TestDir(const char* dir)                               = 0;
	virtual bool FindFirst(const char* dir, DOS_DTA& dta,
	                       bool fcb_findfirst = false)                  = 0;
	virtual bool FindNext(DOS_DTA& dta)                                 = 0;
	virtual bool GetFileAttr(const char* name, FatAttributeFlags* attr) = 0;
	virtual bool SetFileAttr(const char* name, const FatAttributeFlags attr) = 0;
	virtual bool Rename(const char* oldname, const char* newname) = 0;
	virtual bool AllocationInfo(uint16_t* _bytes_sector, uint8_t* _sectors_cluster,
	                            uint16_t* _total_clusters,
	                            uint16_t* _free_clusters) = 0;
	virtual bool FileExists(const char* name)                     = 0;
	virtual uint8_t GetMediaByte(void)                            = 0;
	virtual void SetDir(const char* path);
	virtual void EmptyCache() { dirCache.EmptyCache(); }
	virtual bool IsReadOnly() const = 0;
	virtual bool IsRemote(void)     = 0;
	virtual bool IsRemovable(void)  = 0;
	virtual Bits UnMount(void)      = 0;

	DosDriveType GetType() const
	{
		return type;
	}
	const char *GetInfo() const
	{
		return info;
	}
	std::string GetInfoString() const
	{
		switch (type) {
		case DosDriveType::Local:
			return MSG_Get("MOUNT_TYPE_LOCAL_DIRECTORY") +
			       std::string(" ") + info;
		case DosDriveType::Cdrom:
			return MSG_Get("MOUNT_TYPE_CDROM") + std::string(" ") + info;
		case DosDriveType::Fat:
			return MSG_Get("MOUNT_TYPE_FAT") + std::string(" ") + info;
		case DosDriveType::Iso:
			return MSG_Get("MOUNT_TYPE_ISO") + std::string(" ") + info;
		case DosDriveType::Virtual: return MSG_Get("MOUNT_TYPE_VIRTUAL");
		default: return MSG_Get("MOUNT_TYPE_UNKNOWN");
		}
	}

	char curdir[DOS_PATHLENGTH];
	char info[256];
	DosDriveType type = DosDriveType::Unknown;

	// Can be overridden for example in iso images
	virtual const char *GetLabel() { return dirCache.GetLabel(); }

	DOS_Drive_Cache dirCache;

	// disk cycling functionality (request resources)
	virtual void Activate() {}
};

enum FatPermissionFlags : uint8_t { // 8-bit
	OPEN_READ        = 0b0000'0000,
	OPEN_WRITE       = 0b0000'0001,
	OPEN_READWRITE   = 0b0000'0010,
	OPEN_READ_NO_MOD = 0b0000'0100,
	DOS_NOT_INHERIT  = 0b1000'0000,
};

enum SeekType : uint8_t {
	DOS_SEEK_SET = 0,
	DOS_SEEK_CUR = 1,
	DOS_SEEK_END = 2,
};

// A multiplex handler should read the registers to check what function is being
// called. If the handler returns false DOS will stop checking other handlers.

typedef bool (MultiplexHandler)(void);
void DOS_AddMultiplexHandler(MultiplexHandler* handler);
void DOS_DeleteMultiplexHandler(MultiplexHandler* const handler);

/* AddDevice stores the pointer to a created device */
void DOS_AddDevice(DOS_Device * adddev);
/* DelDevice destroys the device that is pointed to. */
void DOS_DelDevice(DOS_Device * dev);

// Get, append, and query the DOS device header linked list
RealPt DOS_GetNextDevice(const RealPt rp);
RealPt DOS_GetLastDevice();
void DOS_AppendDevice(const uint16_t segment, const uint16_t offset = 0);
bool DOS_IsEndPointer(const RealPt rp);
bool DOS_DeviceHasName(const RealPt rp, const std::string_view req_name);
bool DOS_DeviceHasAttributes(const RealPt rp, const uint16_t attributes);
uint16_t DOS_GetDeviceStrategy(const RealPt rp);
uint16_t DOS_GetDeviceInterrupt(const RealPt rp);

void VFILE_Register(const char *name,
                    const uint8_t *data,
                    const uint32_t size,
                    const char *dir = "");
void VFILE_Register(const char* name, const std::vector<uint8_t>& data,
                    const char* dir = "");
bool VFILE_Update(const char* name, const std::vector<uint8_t> &data, const char* dir = "");
void VFILE_Remove(const char* name, const char* dir = "");

#endif // DOSBOX_DOS_SYSTEM_H
