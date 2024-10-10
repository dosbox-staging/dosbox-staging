/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
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

#ifndef DOSBOX_DOS_INC_H
#define DOSBOX_DOS_INC_H

#include "dosbox.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>

#include "dos_system.h"
#include "mem.h"

#define EXT_DEVICE_BIT 0x0200

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct CommandTail{
	uint8_t count = 0;     /* number of bytes returned */
	static constexpr size_t MaxCmdtailBufferSize = 126;
	char buffer[MaxCmdtailBufferSize + 1] = {}; /* the buffer itself */
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack ()
#endif

struct DOS_Date {
	uint16_t year;
	uint8_t month;
	uint8_t day;
};

struct DOS_Version {
	uint8_t major,minor,revision;
};


#ifdef _MSC_VER
#pragma pack (1)
#endif
union bootSector {
	struct entries {
		uint8_t jump[3];
		uint8_t oem_name[8];
		uint16_t bytesect;
		uint8_t sectclust;
		uint16_t reserve_sect;
		uint8_t misc[496];
	} bootdata;
	uint8_t rawdata[512];
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack ()
#endif


enum { MCB_FREE=0x0000,MCB_DOS=0x0008 };

enum class DosReturnMode : uint8_t {
	Exit                     = 0,
	CtrlC                    = 1,
	Abort                    = 2,
	TerminateAndStayResident = 3
};

#define DOS_FILES   255
#define DOS_DRIVES  26
#define DOS_DEVICES 10

// dos swappable area is 0x320 bytes beyond the sysvars table
// device driver chain is inside sysvars
#define DOS_INFOBLOCK_SEG 0x80	// sysvars (list of lists)
#define DOS_CONDRV_SEG 0xa0
#define DOS_CONSTRING_SEG 0xa8
#define DOS_SDA_SEG 0xb2		// dos swappable area
#define DOS_SDA_OFS 0
#define DOS_CDS_SEG 0x108
#define DOS_FIRST_SHELL 0x118
#define DOS_MEM_START 0x16f		//First Segment that DOS can use

#define DOS_PRIVATE_SEGMENT 0xc800
#define DOS_PRIVATE_SEGMENT_END 0xd000

constexpr int SftHeaderSize = 6;
constexpr int SftEntrySize = 59;

constexpr uint32_t SftEndPointer = 0xffffffff;
constexpr uint16_t SftNextTableOffset = 0x0;
constexpr uint16_t SftNumberOfFilesOffset = 0x04;

// Fake SFT table for use by DOS_MultiplexFunctions() ax = 0x1216
extern RealPt fake_sft_table;
constexpr int FakeSftEntries = 16;

/* internal Dos Tables */

extern std::array<std::unique_ptr<DOS_File>, DOS_FILES> Files;
extern std::array<std::shared_ptr<DOS_Drive>, DOS_DRIVES> Drives;
extern DOS_Device * Devices[DOS_DEVICES];

extern uint8_t dos_copybuf[0x10000];


void DOS_SetError(uint16_t code);

// Guest OS booting routines

void DOS_NotifyBooting();
bool DOS_IsGuestOsBooted();

// File handling routines

enum { STDIN=0,STDOUT=1,STDERR=2,STDAUX=3,STDPRN=4};
enum { HAND_NONE=0,HAND_FILE,HAND_DEVICE};

void DOS_SetupFiles (void);
bool DOS_ReadFile(uint16_t handle,uint8_t * data,uint16_t * amount, bool fcb = false);
bool DOS_WriteFile(uint16_t handle,uint8_t * data,uint16_t * amount,bool fcb = false);
bool DOS_SeekFile(uint16_t handle,uint32_t * pos,uint32_t type,bool fcb = false);
bool DOS_CloseFile(uint16_t handle,bool fcb = false,uint8_t * refcnt = nullptr);
bool DOS_FlushFile(uint16_t handle);
bool DOS_DuplicateEntry(uint16_t entry,uint16_t * newentry);
bool DOS_ForceDuplicateEntry(uint16_t entry,uint16_t newentry);
bool DOS_GetFileDate(uint16_t entry, uint16_t* otime, uint16_t* odate);
bool DOS_SetFileDate(uint16_t entry, uint16_t ntime, uint16_t ndate);

uint16_t DOS_GetBiosTimePacked();
uint16_t DOS_GetBiosDatePacked();

// Date and Time Conversion

constexpr uint16_t DOS_PackTime(const uint16_t hour,
                                const uint16_t min,
                                const uint16_t sec) noexcept
{
	const auto h_bits = 0b1111100000000000 & (hour << 11);
	const auto m_bits = 0b0000011111100000 & (min << 5);
	const auto s_bits = 0b0000000000011111 & (sec / 2);
	const auto packed = h_bits | m_bits | s_bits;
	return static_cast<uint16_t>(packed);
}

constexpr uint16_t DOS_PackTime(const struct tm &datetime) noexcept
{
	return DOS_PackTime(static_cast<uint16_t>(datetime.tm_hour),
	                    static_cast<uint16_t>(datetime.tm_min),
	                    static_cast<uint16_t>(datetime.tm_sec));
}

constexpr uint16_t DOS_PackDate(const uint16_t year,
                                const uint16_t mon,
                                const uint16_t day) noexcept
{
	const int delta_year = year - 1980;

	constexpr int delta_year_min = 0;
	constexpr int delta_year_max = INT8_MAX;
	const auto years_after_1980  = static_cast<uint16_t>(
                std::clamp(delta_year, delta_year_min, delta_year_max));

	const auto y_bits = 0b1111111000000000 & (years_after_1980 << 9);
	const auto m_bits = 0b0000000111100000 & (mon << 5);
	const auto d_bits = 0b0000000000011111 & day;
	const auto packed = y_bits | m_bits | d_bits;
	return static_cast<uint16_t>(packed);
}

constexpr uint16_t DOS_PackDate(const struct tm &datetime) noexcept
{
	return DOS_PackDate(static_cast<uint16_t>(datetime.tm_year + 1900),
	                    static_cast<uint16_t>(datetime.tm_mon + 1),
	                    static_cast<uint16_t>(datetime.tm_mday));
}

constexpr struct tm DOS_UnpackDateTime(const uint16_t date, const uint16_t time)
{
	struct tm ret = {};
	ret.tm_sec = (time & 0x1f) * 2;
	ret.tm_min = (time >> 5) & 0x3f;
	ret.tm_hour = (time >> 11) & 0x1f;
	ret.tm_mday = date & 0x1f;
	ret.tm_mon = ((date >> 5) & 0x0f) - 1;
	ret.tm_year = (date >> 9) + 1980 - 1900;
	// have the C run-time library code compute whether standard
	// time or daylight saving time is in effect.
	ret.tm_isdst = -1;
	return ret;
}

/* Routines for Drive Class */
bool DOS_OpenFile(const char* name, uint8_t flags, uint16_t* entry, bool fcb = false);
bool DOS_OpenFileExtended(const char* name, uint16_t flags,
                          FatAttributeFlags createAttr, uint16_t action,
                          uint16_t* entry, uint16_t* status);
bool DOS_CreateFile(const char* name, FatAttributeFlags attribute,
                    uint16_t* entry, bool fcb = false);
bool DOS_UnlinkFile(const char* const name);
bool DOS_FindFirst(const char* search, FatAttributeFlags attr,
                   bool fcb_findfirst = false);
bool DOS_FindNext(void);
bool DOS_Canonicalize(const char* const name, char* const canonicalized);
std::string DOS_Canonicalize(const char* const name);
bool DOS_CreateTempFile(char* const name, uint16_t* entry);
bool DOS_FileExists(const char* const name);
bool DOS_LockFile(const uint16_t entry, const uint32_t pos, const uint32_t len);
bool DOS_UnlockFile(const uint16_t entry, const uint32_t pos, const uint32_t len);

/* Helper Functions */
bool DOS_MakeName(const char* const name, char* const fullname, uint8_t* drive);

/* Drive Handing Routines */
uint8_t DOS_GetDefaultDrive(void);
void DOS_SetDefaultDrive(uint8_t drive);
bool DOS_SetDrive(uint8_t drive);
bool DOS_GetCurrentDir(uint8_t drive,char * const buffer);
bool DOS_ChangeDir(const char* const dir);
bool DOS_MakeDir(const char* const dir);
bool DOS_RemoveDir(const char* const dir);
bool DOS_Rename(const char* const oldname, const char* const newname);
bool DOS_GetFreeDiskSpace(uint8_t drive,uint16_t * bytes,uint8_t * sectors,uint16_t * clusters,uint16_t * free);
bool DOS_GetFileAttr(const char* const name, FatAttributeFlags* attr);
bool DOS_SetFileAttr(const char* const name, FatAttributeFlags attr);

/* IOCTL Stuff */
bool DOS_IOCTL(void);
bool DOS_GetSTDINStatus();
uint8_t DOS_FindDevice(const char* name);
void DOS_SetupDevices();
void DOS_ClearDrivesAndFiles();
void DOS_ShutDownDevices();

/* Execute and new process creation */
bool DOS_NewPSP(uint16_t pspseg,uint16_t size);
bool DOS_ChildPSP(uint16_t pspseg,uint16_t size);
bool DOS_Execute(char * name,PhysPt block,uint8_t flags);

void DOS_Terminate(const uint16_t psp_seg, const bool is_terminate_and_stay_resident,
                   const uint8_t exit_code);

/* Memory Handling Routines */
void DOS_SetupMemory(void);
bool DOS_AllocateMemory(uint16_t * segment,uint16_t * blocks);
bool DOS_ResizeMemory(uint16_t segment,uint16_t * blocks);
bool DOS_FreeMemory(uint16_t segment);
void DOS_FreeProcessMemory(uint16_t pspseg);
uint16_t DOS_GetMemory(uint16_t pages);
void DOS_FreeTableMemory();
bool DOS_SetMemAllocStrategy(uint16_t strat);
void DOS_SetMcbFaultStrategy(const char *mcb_fault_strategy_pref);
uint16_t DOS_GetMemAllocStrategy(void);
void DOS_BuildUMBChain(bool umb_active,bool ems_active);
bool DOS_LinkUMBsToMemChain(uint16_t linkstate);

/* FCB stuff */
bool DOS_FCBOpen(uint16_t seg,uint16_t offset);
bool DOS_FCBCreate(uint16_t seg,uint16_t offset);
bool DOS_FCBClose(uint16_t seg,uint16_t offset);
bool DOS_FCBFindFirst(uint16_t seg,uint16_t offset);
bool DOS_FCBFindNext(uint16_t seg,uint16_t offset);
uint8_t DOS_FCBRead(uint16_t seg,uint16_t offset, uint16_t numBlocks);
uint8_t DOS_FCBWrite(uint16_t seg,uint16_t offset,uint16_t numBlocks);
uint8_t DOS_FCBRandomRead(uint16_t seg,uint16_t offset,uint16_t * numRec,bool restore);
uint8_t DOS_FCBRandomWrite(uint16_t seg,uint16_t offset,uint16_t * numRec,bool restore);
bool DOS_FCBGetFileSize(uint16_t seg,uint16_t offset);
bool DOS_FCBDeleteFile(uint16_t seg,uint16_t offset);
bool DOS_FCBRenameFile(uint16_t seg, uint16_t offset);
void DOS_FCBSetRandomRecord(uint16_t seg, uint16_t offset);
uint8_t FCB_Parsename(uint16_t seg, uint16_t offset, uint8_t parser,
                      const char* string, uint8_t* change);
bool DOS_GetAllocationInfo(uint8_t drive,uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters);

/* Extra DOS Interrupts */
void DOS_SetupMisc(void);

/* The DOS Tables */
void DOS_SetupTables(void);

/* Internal DOS Setup Programs */
void DOS_SetupPrograms(void);

/* Initialize Keyboard Layout */
void DOS_KeyboardLayout_Init(Section* sec);

bool DOS_LayoutKey(const uint8_t key, const uint8_t flags1,
                   const uint8_t flags2, const uint8_t flags3);

DOS_Version DOS_ParseVersion(const char *word, const char *args);

static inline uint16_t long2para(uint32_t size) {
	if (size>0xFFFF0) return 0xffff;
	if (size&0xf) return (uint16_t)((size>>4)+1);
	else return (uint16_t)(size>>4);
}

/* Dos Error Codes */
#define DOSERR_NONE 0
#define DOSERR_FUNCTION_NUMBER_INVALID 1
#define DOSERR_FILE_NOT_FOUND 2
#define DOSERR_PATH_NOT_FOUND 3
#define DOSERR_TOO_MANY_OPEN_FILES 4
#define DOSERR_ACCESS_DENIED 5
#define DOSERR_INVALID_HANDLE 6
#define DOSERR_MCB_DESTROYED 7
#define DOSERR_INSUFFICIENT_MEMORY 8
#define DOSERR_MB_ADDRESS_INVALID 9
#define DOSERR_ENVIRONMENT_INVALID 10
#define DOSERR_FORMAT_INVALID 11
#define DOSERR_ACCESS_CODE_INVALID 12
#define DOSERR_DATA_INVALID 13
#define DOSERR_RESERVED 14
#define DOSERR_FIXUP_OVERFLOW 14
#define DOSERR_INVALID_DRIVE 15
#define DOSERR_REMOVE_CURRENT_DIRECTORY 16
#define DOSERR_NOT_SAME_DEVICE 17
#define DOSERR_NO_MORE_FILES 18
#define DOSERR_LOCK_VIOLATION 33
#define DOSERR_FILE_ALREADY_EXISTS 80

/* Wait/check user input */
bool DOS_IsCancelRequest();

/* Macros SSET_* and SGET_* are used to safely access fields in memory-mapped
 * DOS structures represented via classes inheriting from MemStruct class.
 *
 * All of these macros depend on 'pt' base pointer from MemStruct base class;
 * all DOS-specific fields are accessed by reading memory relative to that
 * pointer.
 *
 * Example usage:
 *
 *   SSET_WORD(dos-structure-name, field-name, value);
 *   uint16_t x = SGET_WORD(dos-structure-name, field-name);
 */
template <size_t N, typename S, typename T1, typename T2 = T1>
constexpr PhysPt assert_macro_args_ok()
{
	static_assert(sizeof(T1) == N, "Requested struct field has unexpected size");
	static_assert(sizeof(T2) == N, "Type used to save value has unexpected size");
	static_assert(std::is_standard_layout<S>::value,
	              "Struct needs to have standard layout for offsetof calculation");
	// returning 0, so compiler can optimize-out no-op "0 +" expression
	return 0;
}

#define VERIFY_SSET_ARGS(n, s, f, v)                                           \
	assert_macro_args_ok<n, s, decltype(s::f), decltype(v)>()
#define VERIFY_SGET_ARGS(n, s, f)                                              \
	assert_macro_args_ok<n, s, decltype(s::f)>()

#define SSET_BYTE(s, f, v)                                                     \
	mem_writeb(VERIFY_SSET_ARGS(1, s, f, v) + pt + offsetof(s, f), v)
#define SSET_WORD(s, f, v)                                                     \
	mem_writew(VERIFY_SSET_ARGS(2, s, f, v) + pt + offsetof(s, f), v)
#define SSET_DWORD(s, f, v)                                                    \
	mem_writed(VERIFY_SSET_ARGS(4, s, f, v) + pt + offsetof(s, f), v)

#define SGET_BYTE(s, f)                                                        \
	mem_readb(VERIFY_SGET_ARGS(1, s, f) + pt + offsetof(s, f))
#define SGET_WORD(s, f)                                                        \
	mem_readw(VERIFY_SGET_ARGS(2, s, f) + pt + offsetof(s, f))
#define SGET_DWORD(s, f)                                                       \
	mem_readd(VERIFY_SGET_ARGS(4, s, f) + pt + offsetof(s, f))

class MemStruct {
public:
	MemStruct() = default;
	MemStruct(uint16_t seg, uint16_t off) : pt(PhysicalMake(seg, off)) {}
	MemStruct(RealPt addr) : pt(RealToPhysical(addr)) {}

	void SetPt(uint16_t seg)
	{
		pt = PhysicalMake(seg, 0);
	}

protected:
	PhysPt pt    = 0;
	~MemStruct() = default;
};

class Environment {
public:
	virtual std::optional<std::string> GetEnvironmentValue(std::string_view entry) const = 0;

	virtual ~Environment() = default;

protected:
	Environment()                              = default;
	Environment(const Environment&)            = default;
	Environment& operator=(const Environment&) = default;
	Environment(Environment&&)                 = default;
	Environment& operator=(Environment&&)      = default;
};

/* Program Segment Prefix */
class DOS_PSP final : public MemStruct, public Environment {
public:
	DOS_PSP(uint16_t segment) : seg(segment) { SetPt(seg); }

	void MakeNew(uint16_t mem_size);

	void CopyFileTable(DOS_PSP *srcpsp, bool createchildpsp);

	void CloseFiles();

	uint16_t GetSegment() const { return seg; }

	void SaveVectors();
	void RestoreVectors();

	void SetFileHandle(uint16_t index, uint8_t handle);
	uint8_t GetFileHandle(uint16_t index) const;

	uint16_t FindFreeFileEntry() const;
	uint16_t FindEntryByHandle(uint8_t handle) const;

	void SetSize(uint16_t size) { SSET_WORD(sPSP, next_seg, size); }
	uint16_t GetSize() const { return SGET_WORD(sPSP, next_seg); }

	void SetInt22(RealPt int22pt) { SSET_DWORD(sPSP, int_22, int22pt); }
	RealPt GetInt22() const { return SGET_DWORD(sPSP, int_22); }

	void SetParent(uint16_t parent) { SSET_WORD(sPSP, psp_parent, parent); }
	uint16_t GetParent() const { return SGET_WORD(sPSP, psp_parent); }

	void SetEnvironment(uint16_t env) { SSET_WORD(sPSP, environment, env); }
	uint16_t GetEnvironment() const { return SGET_WORD(sPSP, environment); }

	void SetStack(RealPt stackpt) { SSET_DWORD(sPSP, stack, stackpt); }
	RealPt GetStack() const { return SGET_DWORD(sPSP, stack); }

	void SetVersion(const uint8_t major, const uint8_t minor)
	{
		SSET_BYTE(sPSP, dos_version_major, major);
		SSET_BYTE(sPSP, dos_version_minor, minor);
	}
	uint8_t GetVersionMajor() const
	{
		return SGET_BYTE(sPSP, dos_version_major);
	}
	uint8_t GetVersionMinor() const
	{
		return SGET_BYTE(sPSP, dos_version_minor);
	}

	bool SetNumFiles(uint16_t file_num);
	void SetFCB1(RealPt src);
	void SetFCB2(RealPt src);
	void SetCommandTail(RealPt src);

	std::optional<std::string> GetEnvironmentValue(std::string_view variable) const override;
	std::vector<std::string> GetAllRawEnvironmentStrings() const;
	bool SetEnvironmentValue(std::string_view variable,
	                         std::string_view new_string);

private:
	#ifdef _MSC_VER
	#pragma pack(1)
	#endif
	struct sPSP {
		uint8_t  exit[2];     /* CP/M-like exit poimt */
		uint16_t next_seg;    /* Segment of first byte beyond memory allocated or program */
		uint8_t  fill_1;      /* single char fill */
		uint8_t  far_call;    /* far call opcode */
		RealPt   cpm_entry;   /* CPM Service Request address*/
		RealPt   int_22;      /* Terminate Address */
		RealPt   int_23;      /* Break Address */
		RealPt   int_24;      /* Critical Error Address */
		uint16_t psp_parent;  /* Parent PSP Segment */
		uint8_t  files[20];   /* File Table - 0xff is unused */
		uint16_t environment; /* Segment of evironment table */
		RealPt   stack;       /* SS:SP Save point for int 0x21 calls */
		uint16_t max_files;   /* Maximum open files */
		RealPt   file_table;  /* Pointer to File Table PSP:0x18 */
		RealPt   prev_psp;    /* Pointer to previous PSP */
		uint8_t  interim_flag;
		uint8_t  truename_flag;
		uint16_t nn_flags;
		uint8_t  dos_version_major;
		uint8_t  dos_version_minor;
		uint8_t  fill_2[14]; /* Lot's of unused stuff i can't care aboue */
		uint8_t  service[3]; /* INT 0x21 Service call int 0x21;retf; */
		uint8_t  fill_3[9];  /* This has some blocks with FCB info */
		uint8_t  fcb1[16];   /* first FCB */
		uint8_t  fcb2[16];   /* second FCB */
		uint8_t  fill_4[4];  /* unused */
		CommandTail cmdtail;
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack()
	#endif

	uint16_t seg;

public:
	static	uint16_t rootpsp;
};

class DOS_ParamBlock final : public MemStruct {
public:
	DOS_ParamBlock(PhysPt addr)
		: exec{0, 0, 0, 0, 0, 0},
		  overlay{0, 0}
	{
		pt = addr;
	}

	void Clear();
	void LoadData();
	void SaveData(); // Save it as an exec block

	#ifdef _MSC_VER
	#pragma pack (1)
	#endif
	struct sOverlay {
		uint16_t loadseg;
		uint16_t relocation;
	} GCC_ATTRIBUTE(packed);
	struct sExec {
		uint16_t envseg;
		RealPt cmdtail;
		RealPt fcb1;
		RealPt fcb2;
		RealPt initsssp;
		RealPt initcsip;
	}GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack()
	#endif
	sExec exec;
	sOverlay overlay;
};

class DOS_InfoBlock final : public MemStruct {
public:
	DOS_InfoBlock() : seg(0) {}

	void SetLocation(uint16_t segment);
	void SetBuffers(uint16_t x, uint16_t y);

	RealPt GetPointer() const
	{
		return RealMake(seg, offsetof(sDIB, firstDPB));
	}

	void SetDeviceChainStart(uint32_t chain) { SSET_DWORD(sDIB, nulNextDriver, chain); }
	uint32_t GetDeviceChain() const { return SGET_DWORD(sDIB, nulNextDriver); }

	void SetUMBChainState(uint8_t state) { SSET_BYTE(sDIB, chainingUMB, state); }
	uint8_t GetUMBChainState() const { return SGET_BYTE(sDIB, chainingUMB); }

	void SetStartOfUMBChain(uint16_t start_seg) { SSET_WORD(sDIB, startOfUMBChain, start_seg); }
	uint16_t GetStartOfUMBChain() const { return SGET_WORD(sDIB, startOfUMBChain); }

	void SetDiskBufferHeadPt(uint32_t db) { SSET_DWORD(sDIB, diskBufferHeadPt, db); }
	void SetFirstMCB(uint16_t mcb) { SSET_WORD(sDIB, firstMCB, mcb); }
	void SetCurDirStruct(uint32_t cds) { SSET_DWORD(sDIB, curDirStructure, cds); }
	void SetFCBTable(uint32_t tab) { SSET_DWORD(sDIB, fcbTable, tab); }
	void SetBlockDevices(uint8_t num) { SSET_BYTE(sDIB, blockDevices, num); }

	#ifdef _MSC_VER
	#pragma pack(1)
	#endif
	struct sDIB {
		uint8_t	unknown1[4];
		uint16_t	magicWord;			// -0x22 needs to be 1
		uint8_t	unknown2[8];
		uint16_t	regCXfrom5e;		// -0x18 CX from last int21/ah=5e
		uint16_t	countLRUcache;		// -0x16 LRU counter for FCB caching
		uint16_t	countLRUopens;		// -0x14 LRU counter for FCB openings
		uint8_t	stuff[6];		// -0x12 some stuff, hopefully never used....
		uint16_t	sharingCount;		// -0x0c sharing retry count
		uint16_t	sharingDelay;		// -0x0a sharing retry delay
		RealPt	diskBufPtr;		// -0x08 pointer to disk buffer
		uint16_t	ptrCONinput;		// -0x04 pointer to con input
		uint16_t	firstMCB;		// -0x02 first memory control block
		RealPt	firstDPB;		//  0x00 first drive parameter block
		RealPt	firstFileTable;		//  0x04 first system file table
		RealPt	activeClock;		//  0x08 active clock device header
		RealPt	activeCon;		//  0x0c active console device header
		uint16_t	maxSectorLength;	//  0x10 maximum bytes per sector of any block device;
		RealPt	diskInfoBuffer;		//  0x12 pointer to disk info buffer
		RealPt  curDirStructure;	//  0x16 pointer to current array of directory structure
		RealPt	fcbTable;		//  0x1a pointer to system FCB table
		uint16_t	protFCBs;		//  0x1e protected fcbs
		uint8_t	blockDevices;		//  0x20 installed block devices
		uint8_t	lastdrive;		//  0x21 lastdrive
		uint32_t	nulNextDriver;	//  0x22 NUL driver next pointer
		uint16_t	nulAttributes;	//  0x26 NUL driver aattributes
		uint32_t	nulStrategy;	//  0x28 NUL driver strategy routine
		uint8_t	nulString[8];	//  0x2c NUL driver name string
		uint8_t	joindedDrives;		//  0x34 joined drives
		uint16_t	specialCodeSeg;		//  0x35 special code segment
		RealPt  setverPtr;		//  0x37 pointer to setver
		uint16_t  a20FixOfs;		//  0x3b a20 fix routine offset
		uint16_t  pspLastIfHMA;		//  0x3d psp of last program (if dos in hma)
		uint16_t	buffers_x;		//  0x3f x in BUFFERS x,y
		uint16_t	buffers_y;		//  0x41 y in BUFFERS x,y
		uint8_t	bootDrive;		//  0x43 boot drive
		uint8_t	useDwordMov;		//  0x44 use dword moves
		uint16_t	extendedSize;		//  0x45 size of extended memory
		uint32_t	diskBufferHeadPt;	//  0x47 pointer to least-recently used buffer header
		uint16_t	dirtyDiskBuffers;	//  0x4b number of dirty disk buffers
		uint32_t	lookaheadBufPt;		//  0x4d pointer to lookahead buffer
		uint16_t	lookaheadBufNumber;		//  0x51 number of lookahead buffers
		uint8_t	bufferLocation;			//  0x53 workspace buffer location
		uint32_t	workspaceBuffer;		//  0x54 pointer to workspace buffer
		uint8_t	unknown3[11];			//  0x58
		uint8_t	chainingUMB;			//  0x63 bit0: UMB chain linked to MCB chain
		uint16_t	minMemForExec;			//  0x64 minimum paragraphs needed for current program
		uint16_t	startOfUMBChain;		//  0x66 segment of first UMB-MCB
		uint16_t	memAllocScanStart;		//  0x68 start paragraph for memory allocation
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack ()
	#endif
	uint16_t	seg;
};

/* Disk Transfer Address
 *
 * Some documents refer to it also as Data Transfer Address or Disk Transfer Area.
 */

#ifdef _MSC_VER
#pragma pack(1)
#endif
struct sDTA {
	uint8_t sdrive;						/* The Drive the search is taking place */
	uint8_t sname[8];						/* The Search pattern for the filename */
	uint8_t sext[3];						/* The Search pattern for the extension */
	uint8_t sattr;						/* The Attributes that need to be found */
	uint16_t dirID;						/* custom: dir-search ID for multiple searches at the same time */
	uint16_t dirCluster;					/* custom (drive_fat only): cluster number for multiple searches at the same time */
	uint8_t fill[4];
	uint8_t attr;
	uint16_t time;
	uint16_t date;
	uint32_t size;
	char name[DOS_NAMELENGTH_ASCII];
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack()
#endif

class DOS_DTA final : public MemStruct {
public:
	DOS_DTA(RealPt addr) : MemStruct(addr) {}

	void SetupSearch(uint8_t drive, FatAttributeFlags attr, char* pattern);
	uint8_t GetSearchDrive() const { return SGET_BYTE(sDTA, sdrive); }
	void GetSearchParams(FatAttributeFlags& attr, char* pattern) const;

	struct Result {
		std::string name = {};

		uint32_t size = 0;
		uint16_t date = 0;
		uint16_t time = 0;

		FatAttributeFlags attr = {};

		std::string GetExtension() const;
		std::string GetBareName() const; // name without extension

		bool IsFile() const
		{
			return !attr.directory && !attr.volume && !attr.device;
		}

		bool IsDirectory() const
		{
			return attr.directory;
		}

		bool IsDummyDirectory() const
		{
			return attr.directory && (name == "." || name == "..");
		}

		bool IsDevice() const
		{
			return attr.device;
		}

		bool IsReadOnly() const
		{
			return attr.read_only;
		}
	};

	void SetResult(const char* name, uint32_t size, uint16_t date,
	               uint16_t time, FatAttributeFlags attr);
	void GetResult(Result& result) const;

	void SetDirID(uint16_t id) { SSET_WORD(sDTA, dirID, id); }
	uint16_t GetDirID() const { return SGET_WORD(sDTA, dirID); }

	void SetDirIDCluster(uint16_t cl) { SSET_WORD(sDTA, dirCluster, cl); }
	uint16_t GetDirIDCluster() const { return SGET_WORD(sDTA, dirCluster); }
};

enum class ResultGrouping {
	None,
	FilesFirst,
	NonFilesFirst,
};

enum class ResultSorting {
	None,
	ByName,
	ByExtension,
	BySize,
	ByDateTime,
};

void DOS_Sort(std::vector<DOS_DTA::Result>& list, const ResultSorting sorting,
              const bool reverse_order      = false,
              const ResultGrouping grouping = ResultGrouping::None);

/* File Control Block */

class DOS_FCB final : public MemStruct {
public:
	DOS_FCB(uint16_t seg, uint16_t off, bool allow_extended = true);

	void Create(bool _extended);

	bool Extended() const { return extended; }

	void SetName(uint8_t drive, const char *fname, const char *ext);
	void GetName(char * fillname);

	void SetSizeDateTime(uint32_t size, uint16_t mod_date, uint16_t mod_time);
	void GetSizeDateTime(uint32_t &size, uint16_t &mod_date, uint16_t &mod_time) const;

	void FileOpen(uint8_t fhandle);
	void FileClose(uint8_t &fhandle);

	void SetRecord(uint16_t cur_block, uint8_t cur_rec);
	void GetRecord(uint16_t &cur_block, uint8_t &cur_rec) const;

	void SetSeqData(uint8_t fhandle, uint16_t rec_size);
	void GetSeqData(uint8_t &fhandle, uint16_t &rec_size) const;

	void SetRandom(uint32_t random) { SSET_DWORD(sFCB, rndm, random); }
	uint32_t GetRandom() const { return SGET_DWORD(sFCB, rndm); }

	void SetAttr(FatAttributeFlags attr);
	void GetAttr(FatAttributeFlags& attr) const;

	void SetResult(uint32_t size, uint16_t date, uint16_t time,
	               FatAttributeFlags attr);

	uint8_t GetDrive() const;

	bool Valid() const;

	void ClearBlockRecsize();

private:
	bool extended;
	PhysPt real_pt;
	#ifdef _MSC_VER
	#pragma pack (1)
	#endif
	struct sFCB {
		uint8_t drive;			/* Drive number 0=default, 1=A, etc */
		uint8_t filename[8];		/* Space padded name */
		uint8_t ext[3];			/* Space padded extension */
		uint16_t cur_block;		/* Current Block */
		uint16_t rec_size;		/* Logical record size */
		uint32_t filesize;		/* File Size */
		uint16_t date;                  // Date of last modification
		uint16_t time;                  // Time of last modification
		/* Reserved Block should be 8 bytes */
		uint8_t sft_entries;
		uint8_t share_attributes;
		uint8_t extra_info;
		/* Maybe swap file_handle and sft_entries now that fcbs
		 * aren't stored in the psp filetable anymore */
		uint8_t file_handle;
		uint8_t reserved[4];
		/* end */
		uint8_t  cur_rec;			/* Current record in current block */
		uint32_t rndm;			/* Current relative record number */
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack ()
	#endif
};

/* Memory Control Block */

class DOS_MCB final : public MemStruct {
public:
	DOS_MCB(uint16_t seg) : MemStruct(seg, 0) {}

	void SetFileName(const char* const _name)
	{
		MEM_BlockWrite(pt + offsetof(sMCB, filename), _name, 8);
	}
	void GetFileName(char * const _name) { MEM_BlockRead(pt+offsetof(sMCB,filename),_name,8);_name[8]=0;}

	void SetType(uint8_t mcb_type) { SSET_BYTE(sMCB, type, mcb_type); }
	uint8_t GetType() const { return SGET_BYTE(sMCB, type); }

	void SetSize(uint16_t size_paras) { SSET_WORD(sMCB, size, size_paras); }
	uint16_t GetSize() const { return SGET_WORD(sMCB, size); }

	void SetPSPSeg(uint16_t psp) { SSET_WORD(sMCB, psp_segment, psp); }
	uint16_t GetPSPSeg() const { return SGET_WORD(sMCB, psp_segment); }

private:
	#ifdef _MSC_VER
	#pragma pack (1)
	#endif
	struct sMCB {
		uint8_t type;
		uint16_t psp_segment;
		uint16_t size; // Allocation size in 16-byte paragraphs
		uint8_t unused[3];
		uint8_t filename[8];
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack ()
	#endif
};

class DOS_SDA final : public MemStruct {
public:
	DOS_SDA(uint16_t seg, uint16_t off) : MemStruct(seg, off) {}

	void Init();

	void SetDrive(uint8_t drive) { SSET_BYTE(sSDA, current_drive, drive); }
	uint8_t GetDrive() const { return SGET_BYTE(sSDA, current_drive); }

	void SetDTA(uint32_t dta) { SSET_DWORD(sSDA, current_dta, dta); }
	uint32_t GetDTA() const { return SGET_DWORD(sSDA, current_dta); }

	void SetPSP(uint16_t psp) { SSET_WORD(sSDA, current_psp, psp); }
	uint16_t GetPSP() const { return SGET_WORD(sSDA, current_psp); }

private:
	#ifdef _MSC_VER
	#pragma pack (1)
	#endif
	struct sSDA {
		uint8_t crit_error_flag;		/* 0x00 Critical Error Flag */
		uint8_t inDOS_flag;		/* 0x01 InDOS flag (count of active INT 21 calls) */
		uint8_t drive_crit_error;		/* 0x02 Drive on which current critical error occurred or FFh */
		uint8_t locus_of_last_error;	/* 0x03 locus of last error */
		uint16_t extended_error_code;	/* 0x04 extended error code of last error */
		uint8_t suggested_action;		/* 0x06 suggested action for last error */
		uint8_t error_class;		/* 0x07 class of last error*/
		uint32_t last_error_pointer; 	/* 0x08 ES:DI pointer for last error */
		uint32_t current_dta;		/* 0x0C current DTA (Disk Transfer Address) */
		uint16_t current_psp; 		/* 0x10 current PSP */
		uint16_t sp_int_23;		/* 0x12 stores SP across an INT 23 */
		uint16_t return_code;		/* 0x14 return code from last process termination (zerod after reading with AH=4Dh) */
		uint8_t current_drive;		/* 0x16 current drive */
		uint8_t extended_break_flag; 	/* 0x17 extended break flag */
		uint8_t fill[2];			/* 0x18 flag: code page switching || flag: copy of previous byte in case of INT 24 Abort*/
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack()
	#endif
};
extern DOS_InfoBlock dos_infoblock;

struct DOS_Block {
	DOS_Date date       = {};
	DOS_Version version = {};
	uint16_t firstMCB   = {};
	uint16_t errorcode  = {};

	uint16_t psp()
	{
		return DOS_SDA(DOS_SDA_SEG, DOS_SDA_OFS).GetPSP();
	}

	void psp(uint16_t seg)
	{
		DOS_SDA(DOS_SDA_SEG, DOS_SDA_OFS).SetPSP(seg);
	}

	uint16_t env    = {};
	RealPt cpmentry = {};

	RealPt dta()
	{
		return DOS_SDA(DOS_SDA_SEG, DOS_SDA_OFS).GetDTA();
	}
	void dta(RealPt dtap)
	{
		DOS_SDA(DOS_SDA_SEG, DOS_SDA_OFS).SetDTA(dtap);
	}

	uint8_t return_code       = {};
	DosReturnMode return_mode = {};

	uint8_t current_drive = {};
	bool verify           = {};
	bool breakcheck       = {};

	// if set to true dev_con::read will echo input
	bool echo = {};

	bool direct_output   = {};
	bool internal_output = {};

	struct {
		RealPt mediaid           = {};
		RealPt tempdta           = {};
		RealPt tempdta_fcbdelete = {};
		RealPt dbcs              = {};
		RealPt filenamechar      = {};
		RealPt collatingseq      = {};
		RealPt upcase            = {};

		// Will be copied to dos memory. resides in real mem
		uint8_t* country = {};

		// Fake Disk parameter system using only the first entry so the
		// drive letter matches
		uint16_t dpb = {};
	} tables = {};

	uint16_t country_code    = {};
	uint16_t loaded_codepage = {};
	uint16_t dcp = {};
};

extern DOS_Block dos;

static inline uint8_t RealHandle(uint16_t handle) {
	DOS_PSP psp(dos.psp());
	return psp.GetFileHandle(handle);
}

/* Locale information */

enum class DosDateFormat : uint8_t {
	MonthDayYear = 0,
	DayMonthYear = 1,
	YearMonthDay = 2,
};

enum class DosTimeFormat : uint8_t {
	Time12H = 0, // AM/PM
	Time24H = 1,
};

enum class DosCurrencyFormat : uint8_t {
	SymbolAmount      = 0,
	AmountSymbol      = 1,
	SymbolSpaceAmount = 2,
	AmountSpaceSymbol = 3,

	// Some sources claim that bit 2 set means currency symbol should
	// replace decimal point; so far it is unknown which (if any)
	// COUNTRY.SYS uses this bit, most likely no DOS software uses it.
};

DosDateFormat DOS_GetLocaleDateFormat();
DosTimeFormat DOS_GetLocaleTimeFormat();
char DOS_GetLocaleDateSeparator();
char DOS_GetLocaleTimeSeparator();
char DOS_GetLocaleThousandsSeparator();
char DOS_GetLocaleDecimalSeparator();
char DOS_GetLocaleListSeparator();

#endif
