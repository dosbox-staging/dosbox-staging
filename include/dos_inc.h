/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
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

#include <cstddef>
#include <type_traits>

#include "dos_system.h"
#include "mem.h"

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct CommandTail{
  Bit8u count;				/* number of bytes returned */
  char buffer[127];			 /* the buffer itself */
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack ()
#endif

struct DOS_Date {
	Bit16u year;
	Bit8u month;
	Bit8u day;
};

struct DOS_Version {
	Bit8u major,minor,revision;
};


#ifdef _MSC_VER
#pragma pack (1)
#endif
union bootSector {
	struct entries {
		Bit8u jump[3];
		Bit8u oem_name[8];
		Bit16u bytesect;
		Bit8u sectclust;
		Bit16u reserve_sect;
		Bit8u misc[496];
	} bootdata;
	Bit8u rawdata[512];
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack ()
#endif


enum { MCB_FREE=0x0000,MCB_DOS=0x0008 };
enum { RETURN_EXIT=0,RETURN_CTRLC=1,RETURN_ABORT=2,RETURN_TSR=3};

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

/* internal Dos Tables */

extern DOS_File * Files[DOS_FILES];
extern DOS_Drive * Drives[DOS_DRIVES];
extern DOS_Device * Devices[DOS_DEVICES];

extern Bit8u dos_copybuf[0x10000];


void DOS_SetError(Bit16u code);

/* File Handling Routines */

enum { STDIN=0,STDOUT=1,STDERR=2,STDAUX=3,STDPRN=4};
enum { HAND_NONE=0,HAND_FILE,HAND_DEVICE};

/* Routines for File Class */
void DOS_SetupFiles (void);
bool DOS_ReadFile(Bit16u handle,Bit8u * data,Bit16u * amount, bool fcb = false);
bool DOS_WriteFile(Bit16u handle,Bit8u * data,Bit16u * amount,bool fcb = false);
bool DOS_SeekFile(Bit16u handle,Bit32u * pos,Bit32u type,bool fcb = false);
bool DOS_CloseFile(Bit16u handle,bool fcb = false,Bit8u * refcnt = NULL);
bool DOS_FlushFile(Bit16u handle);
bool DOS_DuplicateEntry(Bit16u entry,Bit16u * newentry);
bool DOS_ForceDuplicateEntry(Bit16u entry,Bit16u newentry);
bool DOS_GetFileDate(Bit16u entry, Bit16u* otime, Bit16u* odate);
bool DOS_SetFileDate(uint16_t entry, uint16_t ntime, uint16_t ndate);

// Date and Time Conversion
uint16_t DOS_PackTime(uint16_t hour, uint16_t min, uint16_t sec) noexcept;
uint16_t DOS_PackTime(const struct tm &datetime) noexcept;
uint16_t DOS_PackDate(uint16_t year, uint16_t mon, uint16_t day) noexcept;
uint16_t DOS_PackDate(const struct tm &datetime) noexcept;

/* Routines for Drive Class */
bool DOS_OpenFile(char const * name,Bit8u flags,Bit16u * entry,bool fcb = false);
bool DOS_OpenFileExtended(char const * name, Bit16u flags, Bit16u createAttr, Bit16u action, Bit16u *entry, Bit16u* status);
bool DOS_CreateFile(char const * name,Bit16u attribute,Bit16u * entry, bool fcb = false);
bool DOS_UnlinkFile(char const * const name);
bool DOS_FindFirst(const char *search, uint16_t attr, bool fcb_findfirst = false);
bool DOS_FindNext(void);
bool DOS_Canonicalize(char const * const name,char * const big);
bool DOS_CreateTempFile(char * const name,Bit16u * entry);
bool DOS_FileExists(char const * const name);

/* Helper Functions */
bool DOS_MakeName(char const * const name,char * const fullname,Bit8u * drive);
/* Drive Handing Routines */
Bit8u DOS_GetDefaultDrive(void);
void DOS_SetDefaultDrive(Bit8u drive);
bool DOS_SetDrive(Bit8u drive);
bool DOS_GetCurrentDir(Bit8u drive,char * const buffer);
bool DOS_ChangeDir(char const * const dir);
bool DOS_MakeDir(char const * const dir);
bool DOS_RemoveDir(char const * const dir);
bool DOS_Rename(char const * const oldname,char const * const newname);
bool DOS_GetFreeDiskSpace(Bit8u drive,Bit16u * bytes,Bit8u * sectors,Bit16u * clusters,Bit16u * free);
bool DOS_GetFileAttr(char const * const name,Bit16u * attr);
bool DOS_SetFileAttr(char const * const name,Bit16u attr);

/* IOCTL Stuff */
bool DOS_IOCTL(void);
bool DOS_GetSTDINStatus();
Bit8u DOS_FindDevice(char const * name);
void DOS_SetupDevices(void);

/* Execute and new process creation */
bool DOS_NewPSP(Bit16u pspseg,Bit16u size);
bool DOS_ChildPSP(Bit16u pspseg,Bit16u size);
bool DOS_Execute(char * name,PhysPt block,Bit8u flags);
void DOS_Terminate(Bit16u pspseg,bool tsr,Bit8u exitcode);

/* Memory Handling Routines */
void DOS_SetupMemory(void);
bool DOS_AllocateMemory(Bit16u * segment,Bit16u * blocks);
bool DOS_ResizeMemory(Bit16u segment,Bit16u * blocks);
bool DOS_FreeMemory(Bit16u segment);
void DOS_FreeProcessMemory(Bit16u pspseg);
Bit16u DOS_GetMemory(Bit16u pages);
bool DOS_SetMemAllocStrategy(Bit16u strat);
Bit16u DOS_GetMemAllocStrategy(void);
void DOS_BuildUMBChain(bool umb_active,bool ems_active);
bool DOS_LinkUMBsToMemChain(Bit16u linkstate);

/* FCB stuff */
bool DOS_FCBOpen(Bit16u seg,Bit16u offset);
bool DOS_FCBCreate(Bit16u seg,Bit16u offset);
bool DOS_FCBClose(Bit16u seg,Bit16u offset);
bool DOS_FCBFindFirst(Bit16u seg,Bit16u offset);
bool DOS_FCBFindNext(Bit16u seg,Bit16u offset);
Bit8u DOS_FCBRead(Bit16u seg,Bit16u offset, Bit16u numBlocks);
Bit8u DOS_FCBWrite(Bit16u seg,Bit16u offset,Bit16u numBlocks);
Bit8u DOS_FCBRandomRead(Bit16u seg,Bit16u offset,Bit16u * numRec,bool restore);
Bit8u DOS_FCBRandomWrite(Bit16u seg,Bit16u offset,Bit16u * numRec,bool restore);
bool DOS_FCBGetFileSize(Bit16u seg,Bit16u offset);
bool DOS_FCBDeleteFile(Bit16u seg,Bit16u offset);
bool DOS_FCBRenameFile(Bit16u seg, Bit16u offset);
void DOS_FCBSetRandomRecord(Bit16u seg, Bit16u offset);
Bit8u FCB_Parsename(Bit16u seg,Bit16u offset,Bit8u parser ,char *string, Bit8u *change);
bool DOS_GetAllocationInfo(Bit8u drive,Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters);

/* Extra DOS Interrupts */
void DOS_SetupMisc(void);

/* The DOS Tables */
void DOS_SetupTables(void);

/* Internal DOS Setup Programs */
void DOS_SetupPrograms(void);

/* Initialize Keyboard Layout */
void DOS_KeyboardLayout_Init(Section* sec);

bool DOS_LayoutKey(Bitu key, Bit8u flags1, Bit8u flags2, Bit8u flags3);

DOS_Version DOS_ParseVersion(const char *word, const char *args);

enum {
	KEYB_NOERROR=0,
	KEYB_FILENOTFOUND,
	KEYB_INVALIDFILE,
	KEYB_LAYOUTNOTFOUND,
	KEYB_INVALIDCPFILE
};


static INLINE Bit16u long2para(Bit32u size) {
	if (size>0xFFFF0) return 0xffff;
	if (size&0xf) return (Bit16u)((size>>4)+1);
	else return (Bit16u)(size>>4);
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
#define DOSERR_FILE_ALREADY_EXISTS 80

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
	MemStruct(uint16_t seg, uint16_t off) : pt(PhysMake(seg, off)) {}
	MemStruct(RealPt addr) : pt(Real2Phys(addr)) {}

	void SetPt(uint16_t seg) { pt = PhysMake(seg, 0); }

protected:
	PhysPt pt = 0;
};

/* Program Segment Prefix */

class DOS_PSP final : public MemStruct {
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

	bool SetNumFiles(uint16_t file_num);
	void SetFCB1(RealPt src);
	void SetFCB2(RealPt src);
	void SetCommandTail(RealPt src);

private:
	#ifdef _MSC_VER
	#pragma pack(1)
	#endif
	struct sPSP {
		Bit8u   exit[2];     /* CP/M-like exit poimt */
		Bit16u  next_seg;    /* Segment of first byte beyond memory allocated or program */
		Bit8u   fill_1;      /* single char fill */
		Bit8u   far_call;    /* far call opcode */
		RealPt  cpm_entry;   /* CPM Service Request address*/
		RealPt  int_22;      /* Terminate Address */
		RealPt  int_23;      /* Break Address */
		RealPt  int_24;      /* Critical Error Address */
		Bit16u  psp_parent;  /* Parent PSP Segment */
		Bit8u   files[20];   /* File Table - 0xff is unused */
		Bit16u  environment; /* Segment of evironment table */
		RealPt  stack;       /* SS:SP Save point for int 0x21 calls */
		Bit16u  max_files;   /* Maximum open files */
		RealPt  file_table;  /* Pointer to File Table PSP:0x18 */
		RealPt  prev_psp;    /* Pointer to previous PSP */
		Bit8u   interim_flag;
		Bit8u   truename_flag;
		Bit16u  nn_flags;
		Bit16u  dos_version;
		Bit8u   fill_2[14];  /* Lot's of unused stuff i can't care aboue */
		Bit8u   service[3];  /* INT 0x21 Service call int 0x21;retf; */
		Bit8u   fill_3[9];   /* This has some blocks with FCB info */
		Bit8u   fcb1[16];    /* first FCB */
		Bit8u   fcb2[16];    /* second FCB */
		Bit8u   fill_4[4];   /* unused */
		CommandTail cmdtail;
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack()
	#endif

	uint16_t seg;

public:
	static	Bit16u rootpsp;
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
		Bit16u loadseg;
		Bit16u relocation;
	} GCC_ATTRIBUTE(packed);
	struct sExec {
		Bit16u envseg;
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
		Bit8u	unknown1[4];
		Bit16u	magicWord;			// -0x22 needs to be 1
		Bit8u	unknown2[8];
		Bit16u	regCXfrom5e;		// -0x18 CX from last int21/ah=5e
		Bit16u	countLRUcache;		// -0x16 LRU counter for FCB caching
		Bit16u	countLRUopens;		// -0x14 LRU counter for FCB openings
		Bit8u	stuff[6];		// -0x12 some stuff, hopefully never used....
		Bit16u	sharingCount;		// -0x0c sharing retry count
		Bit16u	sharingDelay;		// -0x0a sharing retry delay
		RealPt	diskBufPtr;		// -0x08 pointer to disk buffer
		Bit16u	ptrCONinput;		// -0x04 pointer to con input
		Bit16u	firstMCB;		// -0x02 first memory control block
		RealPt	firstDPB;		//  0x00 first drive parameter block
		RealPt	firstFileTable;		//  0x04 first system file table
		RealPt	activeClock;		//  0x08 active clock device header
		RealPt	activeCon;		//  0x0c active console device header
		Bit16u	maxSectorLength;	//  0x10 maximum bytes per sector of any block device;
		RealPt	diskInfoBuffer;		//  0x12 pointer to disk info buffer
		RealPt  curDirStructure;	//  0x16 pointer to current array of directory structure
		RealPt	fcbTable;		//  0x1a pointer to system FCB table
		Bit16u	protFCBs;		//  0x1e protected fcbs
		Bit8u	blockDevices;		//  0x20 installed block devices
		Bit8u	lastdrive;		//  0x21 lastdrive
		Bit32u	nulNextDriver;	//  0x22 NUL driver next pointer
		Bit16u	nulAttributes;	//  0x26 NUL driver aattributes
		Bit32u	nulStrategy;	//  0x28 NUL driver strategy routine
		Bit8u	nulString[8];	//  0x2c NUL driver name string
		Bit8u	joindedDrives;		//  0x34 joined drives
		Bit16u	specialCodeSeg;		//  0x35 special code segment
		RealPt  setverPtr;		//  0x37 pointer to setver
		Bit16u  a20FixOfs;		//  0x3b a20 fix routine offset
		Bit16u  pspLastIfHMA;		//  0x3d psp of last program (if dos in hma)
		Bit16u	buffers_x;		//  0x3f x in BUFFERS x,y
		Bit16u	buffers_y;		//  0x41 y in BUFFERS x,y
		Bit8u	bootDrive;		//  0x43 boot drive
		Bit8u	useDwordMov;		//  0x44 use dword moves
		Bit16u	extendedSize;		//  0x45 size of extended memory
		Bit32u	diskBufferHeadPt;	//  0x47 pointer to least-recently used buffer header
		Bit16u	dirtyDiskBuffers;	//  0x4b number of dirty disk buffers
		Bit32u	lookaheadBufPt;		//  0x4d pointer to lookahead buffer
		Bit16u	lookaheadBufNumber;		//  0x51 number of lookahead buffers
		Bit8u	bufferLocation;			//  0x53 workspace buffer location
		Bit32u	workspaceBuffer;		//  0x54 pointer to workspace buffer
		Bit8u	unknown3[11];			//  0x58
		Bit8u	chainingUMB;			//  0x63 bit0: UMB chain linked to MCB chain
		Bit16u	minMemForExec;			//  0x64 minimum paragraphs needed for current program
		Bit16u	startOfUMBChain;		//  0x66 segment of first UMB-MCB
		Bit16u	memAllocScanStart;		//  0x68 start paragraph for memory allocation
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack ()
	#endif
	Bit16u	seg;
};

/* Disk Transfer Address
 *
 * Some documents refer to it also as Data Transfer Address or Disk Transfer Area.
 */

class DOS_DTA final : public MemStruct {
public:
	DOS_DTA(RealPt addr) : MemStruct(addr) {}

	void SetupSearch(uint8_t drive, uint8_t attr, char *pattern);
	uint8_t GetSearchDrive() const { return SGET_BYTE(sDTA, sdrive); }
	void GetSearchParams(uint8_t &attr, char *pattern) const;

	void SetResult(const char *name,
	               uint32_t size,
	               uint16_t date,
	               uint16_t time,
	               uint8_t attr);
	void GetResult(char *name,
	               uint32_t &size,
	               uint16_t &date,
	               uint16_t &time,
	               uint8_t &attr) const;

	void SetDirID(uint16_t id) { SSET_WORD(sDTA, dirID, id); }
	uint16_t GetDirID() const { return SGET_WORD(sDTA, dirID); }

	void SetDirIDCluster(uint16_t cl) { SSET_WORD(sDTA, dirCluster, cl); }
	uint16_t GetDirIDCluster() const { return SGET_WORD(sDTA, dirCluster); }

private:
	#ifdef _MSC_VER
	#pragma pack(1)
	#endif
	struct sDTA {
		Bit8u sdrive;						/* The Drive the search is taking place */
		Bit8u sname[8];						/* The Search pattern for the filename */
		Bit8u sext[3];						/* The Search pattern for the extension */
		Bit8u sattr;						/* The Attributes that need to be found */
		Bit16u dirID;						/* custom: dir-search ID for multiple searches at the same time */
		Bit16u dirCluster;					/* custom (drive_fat only): cluster number for multiple searches at the same time */
		Bit8u fill[4];
		Bit8u attr;
		Bit16u time;
		Bit16u date;
		Bit32u size;
		char name[DOS_NAMELENGTH_ASCII];
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack()
	#endif
};

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

	void SetAttr(uint8_t attr);
	void GetAttr(uint8_t &attr) const;

	void SetResult(Bit32u size,Bit16u date,Bit16u time,Bit8u attr);

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
		Bit8u drive;			/* Drive number 0=default, 1=A, etc */
		Bit8u filename[8];		/* Space padded name */
		Bit8u ext[3];			/* Space padded extension */
		Bit16u cur_block;		/* Current Block */
		Bit16u rec_size;		/* Logical record size */
		Bit32u filesize;		/* File Size */
		uint16_t date;                  // Date of last modification
		uint16_t time;                  // Time of last modification
		/* Reserved Block should be 8 bytes */
		Bit8u sft_entries;
		Bit8u share_attributes;
		Bit8u extra_info;
		/* Maybe swap file_handle and sft_entries now that fcbs
		 * aren't stored in the psp filetable anymore */
		Bit8u file_handle;
		Bit8u reserved[4];
		/* end */
		Bit8u  cur_rec;			/* Current record in current block */
		Bit32u rndm;			/* Current relative record number */
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack ()
	#endif
};

/* Memory Control Block */

class DOS_MCB final : public MemStruct {
public:
	DOS_MCB(uint16_t seg) : MemStruct(seg, 0) {}

	void SetFileName(char const * const _name) { MEM_BlockWrite(pt+offsetof(sMCB,filename),_name,8); }
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
		Bit8u type;
		Bit16u psp_segment;
		uint16_t size; // Allocation size in 16-byte paragraphs
		Bit8u unused[3];
		Bit8u filename[8];
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
		Bit8u crit_error_flag;		/* 0x00 Critical Error Flag */
		Bit8u inDOS_flag;		/* 0x01 InDOS flag (count of active INT 21 calls) */
		Bit8u drive_crit_error;		/* 0x02 Drive on which current critical error occurred or FFh */
		Bit8u locus_of_last_error;	/* 0x03 locus of last error */
		Bit16u extended_error_code;	/* 0x04 extended error code of last error */
		Bit8u suggested_action;		/* 0x06 suggested action for last error */
		Bit8u error_class;		/* 0x07 class of last error*/
		Bit32u last_error_pointer; 	/* 0x08 ES:DI pointer for last error */
		Bit32u current_dta;		/* 0x0C current DTA (Disk Transfer Address) */
		Bit16u current_psp; 		/* 0x10 current PSP */
		Bit16u sp_int_23;		/* 0x12 stores SP across an INT 23 */
		Bit16u return_code;		/* 0x14 return code from last process termination (zerod after reading with AH=4Dh) */
		Bit8u current_drive;		/* 0x16 current drive */
		Bit8u extended_break_flag; 	/* 0x17 extended break flag */
		Bit8u fill[2];			/* 0x18 flag: code page switching || flag: copy of previous byte in case of INT 24 Abort*/
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack()
	#endif
};
extern DOS_InfoBlock dos_infoblock;

struct DOS_Block {
	DOS_Date date;
	DOS_Version version;
	Bit16u firstMCB;
	Bit16u errorcode;

	uint16_t psp() { return DOS_SDA(DOS_SDA_SEG, DOS_SDA_OFS).GetPSP(); }
	void psp(uint16_t seg) { DOS_SDA(DOS_SDA_SEG, DOS_SDA_OFS).SetPSP(seg); }

	Bit16u env;
	RealPt cpmentry;

	RealPt dta() { return DOS_SDA(DOS_SDA_SEG, DOS_SDA_OFS).GetDTA(); }
	void dta(RealPt dtap) { DOS_SDA(DOS_SDA_SEG, DOS_SDA_OFS).SetDTA(dtap); }

	Bit8u return_code,return_mode;

	Bit8u current_drive;
	bool verify;
	bool breakcheck;
	bool echo;          // if set to true dev_con::read will echo input 
	bool direct_output;
	bool internal_output;
	struct  {
		RealPt mediaid;
		RealPt tempdta;
		RealPt tempdta_fcbdelete;
		RealPt dbcs;
		RealPt filenamechar;
		RealPt collatingseq;
		RealPt upcase;
		Bit8u* country;//Will be copied to dos memory. resides in real mem
		Bit16u dpb; //Fake Disk parameter system using only the first entry so the drive letter matches
	} tables;
	Bit16u loaded_codepage;
};

extern DOS_Block dos;

static INLINE Bit8u RealHandle(Bit16u handle) {
	DOS_PSP psp(dos.psp());	
	return psp.GetFileHandle(handle);
}

#endif
