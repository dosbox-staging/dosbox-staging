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

#ifndef DOSBOX_DOS_SYSTEM_H
#define DOSBOX_DOS_SYSTEM_H

#include "dosbox.h"

#include <string>
#include <vector>

#include "cross.h"
#include "mem.h"
#include "support.h"

#define DOS_NAMELENGTH 12
#define DOS_NAMELENGTH_ASCII (DOS_NAMELENGTH+1)
#define DOS_FCBNAME 15
#define DOS_DIRDEPTH 8
#define DOS_PATHLENGTH 80
#define DOS_TEMPSIZE 1024
#define DOS_MFNLENGTH 8
#define DOS_EXTLENGTH 3

#define LFN_NAMELENGTH 255

enum {
	DOS_ATTR_READ_ONLY=	0x01,
	DOS_ATTR_HIDDEN=	0x02,
	DOS_ATTR_SYSTEM=	0x04,
	DOS_ATTR_VOLUME=	0x08,
	DOS_ATTR_DIRECTORY=	0x10,
	DOS_ATTR_ARCHIVE=	0x20,
	DOS_ATTR_DEVICE=	0x40
};

struct FileStat_Block {
	uint32_t size;
	uint16_t time;
	uint16_t date;
	uint16_t attr;
};

class DOS_DTA;

class DOS_File {
public:
	DOS_File()
	        : flags(0),
	          time(0),
	          date(0),
	          attr(0),
	          refCtr(0),
	          open(false),
	          name(""),
	          newtime(false),
	          hdrive(0xff)
	{}

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
	virtual bool	Close()=0;
	virtual uint16_t	GetInformation(void)=0;

	virtual bool IsOpen() { return open; }
	virtual void AddRef() { refCtr++; }
	virtual Bits RemoveRef() { return --refCtr; }
	virtual bool UpdateDateTimeFromHost() { return true; }
	virtual void SetFlagReadOnlyMedium() {}

	void SetDrive(uint8_t drv) { hdrive=drv;}
	uint8_t GetDrive(void) { return hdrive;}
	uint32_t flags;
	uint16_t time;
	uint16_t date;
	uint16_t attr;
	Bits refCtr;
	bool open;
	std::string name;
	bool newtime;
	/* Some Device Specific Stuff */
private:
	uint8_t hdrive;
};

class DOS_Device : public DOS_File {
public:
	DOS_Device() : DOS_File(), devnum(0) {}

	DOS_Device(const DOS_Device& orig)
		: DOS_File(orig),
		  devnum(orig.devnum)
	{
		open = true;
	}

	DOS_Device &operator=(const DOS_Device &orig)
	{
		DOS_File::operator=(orig);
		devnum = orig.devnum;
		open = true;
		return *this;
	}

	virtual bool	Read(uint8_t * data,uint16_t * size);
	virtual bool	Write(uint8_t * data,uint16_t * size);
	virtual bool	Seek(uint32_t * pos,uint32_t type);
	virtual bool	Close();
	virtual uint16_t	GetInformation(void);
	virtual bool	ReadFromControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode);
	virtual bool	WriteToControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode);
	virtual uint8_t GetStatus(bool input_flag);
	void SetDeviceNumber(Bitu num) { devnum=num;}
private:
	Bitu devnum;
};

class localFile : public DOS_File {
public:
	localFile(const char *name, FILE *handle, const char *basedir);
	localFile(const localFile &) = delete;            // prevent copying
	localFile &operator=(const localFile &) = delete; // prevent assignment
	bool Read(uint8_t *data, uint16_t *size);
	bool Write(uint8_t *data, uint16_t *size);
	bool Seek(uint32_t *pos, uint32_t type);
	bool Close();
	uint16_t GetInformation();
	bool UpdateDateTimeFromHost();
	void Flush();
	void SetFlagReadOnlyMedium() { read_only_medium = true; }
	const char *GetBaseDir() const { return basedir; }
	FILE *fhandle = nullptr; // todo handle this properly
private:
	const char *basedir;
	long stream_pos = 0;
	bool ftell_and_check();
	void fseek_and_check(int whence);
	bool fseek_to_and_check(long pos, int whence);
	bool read_only_medium;
	enum { NONE,READ,WRITE } last_action;
};

/* The following variable can be lowered to free up some memory.
 * The negative side effect: The stored searches will be turned over faster.
 * Should not have impact on systems with few directory entries. */
#define MAX_OPENDIRS 2048
//Can be high as it's only storage (16 bit variable)

class DOS_Drive_Cache {
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

	void  ExpandName           (char* path);
	char* GetExpandName        (const char* path);
	bool  GetShortName         (const char* fullname, char* shortname);

	bool  FindFirst            (char* path, uint16_t& id);
	bool  FindNext             (uint16_t id, char* &result);

	void  CacheOut             (const char* path, bool ignoreLastDir = false);
	void  AddEntry             (const char* path, bool checkExist = false);
	void  AddEntryDirOverlay   (const char* path, bool checkExist = false);

	void  DeleteEntry          (const char* path, bool ignoreLastDir = false);
	void  EmptyCache           (void);

	void SetLabel(const char *name, bool cdrom, bool allowupdate);
	const char *GetLabel() const { return label; }

	class CFileInfo {
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

		virtual ~CFileInfo()
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

	virtual bool FileOpen(DOS_File * * file,char * name,uint32_t flags)=0;
	virtual bool FileCreate(DOS_File * * file,char * name,uint16_t attributes)=0;
	virtual bool FileUnlink(char * _name)=0;
	virtual bool RemoveDir(char * _dir)=0;
	virtual bool MakeDir(char * _dir)=0;
	virtual bool TestDir(char * _dir)=0;
	virtual bool FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst=false)=0;
	virtual bool FindNext(DOS_DTA & dta)=0;
	virtual bool GetFileAttr(char * name, uint16_t * attr) = 0;
	virtual bool SetFileAttr(const char * name, const uint16_t attr) = 0;
	virtual bool Rename(char * oldname,char * newname)=0;
	virtual bool AllocationInfo(uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters,uint16_t * _free_clusters)=0;
	virtual bool FileExists(const char* name)=0;
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block)=0;
	virtual uint8_t GetMediaByte(void)=0;
	virtual void SetDir(const char *path);
	virtual void EmptyCache() { dirCache.EmptyCache(); }
	virtual bool isRemote(void)=0;
	virtual bool isRemovable(void)=0;
	virtual Bits UnMount(void)=0;

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
		default: return MSG_Get("MOUNT_TYPE_UNKOWN");
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

enum { OPEN_READ=0, OPEN_WRITE=1, OPEN_READWRITE=2, OPEN_READ_NO_MOD=4, DOS_NOT_INHERIT=128};
enum { DOS_SEEK_SET=0,DOS_SEEK_CUR=1,DOS_SEEK_END=2};


/*
 A multiplex handler should read the registers to check what function is being called
 If the handler returns false dos will stop checking other handlers
*/

typedef bool (MultiplexHandler)(void);
void DOS_AddMultiplexHandler(MultiplexHandler * handler);
void DOS_DelMultiplexHandler(MultiplexHandler * handler);

/* AddDevice stores the pointer to a created device */
void DOS_AddDevice(DOS_Device * adddev);
/* DelDevice destroys the device that is pointed to. */
void DOS_DelDevice(DOS_Device * dev);

void VFILE_Register(const char *name,
                    const uint8_t *data,
                    const uint32_t size,
                    const char *dir = "");

void VFILE_Register(const char *name, const std::vector<uint8_t> &blob, const char *dir);
#endif
