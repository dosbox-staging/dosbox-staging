/*
 *  Copyright (C) 2002  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef DOS_H_
#define DOS_H_

#include <dos_system.h>
#include <mem.h>

#pragma pack (1)
struct CommandTail{
  Bit8u count;				/* number of bytes returned */
  char buffer[127];			 /* the buffer itself */
} GCC_ATTRIBUTE(packed);


#pragma pack ()

struct DOS_Date {
	Bit16u year;
	Bit8u month;
	Bit8u day;
};

struct DOS_Version {
	Bit8u major,minor,revision;
};

struct DOS_Block {
	DOS_Date date;
	DOS_Version version;
	Bit16u firstMCB;
	Bit16u errorcode;
	Bit16u psp;
	Bit16u env;
	RealPt cpmentry;
	RealPt dta;
	Bit8u return_code,return_mode;
	
	Bit8u current_drive;
	bool verify;
	bool breakcheck;
	bool echo;          // if set to true dev_con::read will echo input 
	struct  {
		RealPt indosflag;
		RealPt mediaid;
		RealPt tempdta;
	} tables;
};

enum { MCB_FREE=0x0000,MCB_DOS=0x0008 };
enum { RETURN_EXIT=0,RETURN_CTRLC=1,RETURN_ABORT=2,RETURN_TSR=3};

#define DOS_FILES 50
#define DOS_DRIVES 26

/* internal Dos Tables */
extern DOS_Block dos;
extern DOS_File * Files[DOS_FILES];
extern DOS_Drive * Drives[DOS_DRIVES];
extern Bit8u dos_copybuf[0x10000];


void DOS_SetError(Bit16u code);

/* File Handling Routines */

enum { STDIN=0,STDOUT=1,STDERR=2,STDAUX=3,STDNUL=4,STDPRN=5};
enum { HAND_NONE=0,HAND_FILE,HAND_DEVICE};

/* Routines for File Class */
void DOS_SetupFiles (void);
bool DOS_ReadFile(Bit16u handle,Bit8u * data,Bit16u * amount);
bool DOS_WriteFile(Bit16u handle,Bit8u * data,Bit16u * amount);
bool DOS_SeekFile(Bit16u handle,Bit32u * pos,Bit32u type);
bool DOS_CloseFile(Bit16u handle);
bool DOS_DuplicateEntry(Bit16u entry,Bit16u * newentry);
bool DOS_ForceDuplicateEntry(Bit16u entry,Bit16u newentry);
bool DOS_GetFileDate(Bit16u entry, Bit16u* otime, Bit16u* odate);

/* Routines for Drive Class */
bool DOS_OpenFile(char * name,Bit8u flags,Bit16u * entry);
bool DOS_CreateFile(char * name,Bit16u attribute,Bit16u * entry);
bool DOS_UnlinkFile(char * name);
bool DOS_FindFirst(char *search,Bit16u attr);
bool DOS_FindNext(void);
bool DOS_Canonicalize(char * name,char * big);
bool DOS_CreateTempFile(char * name,Bit16u * entry);
bool DOS_FileExists(char * name);

/* Drive Handing Routines */
Bit8u DOS_GetDefaultDrive(void);
void DOS_SetDefaultDrive(Bit8u drive);
bool DOS_SetDrive(Bit8u drive);
bool DOS_GetCurrentDir(Bit8u drive,char * bugger);
bool DOS_ChangeDir(char * dir);
bool DOS_MakeDir(char * dir);
bool DOS_RemoveDir(char * dir);
bool DOS_Rename(char * oldname,char * newname);
bool DOS_GetFreeDiskSpace(Bit8u drive,Bit16u * bytes,Bit8u * sectors,Bit16u * clusters,Bit16u * free);
bool DOS_GetFileAttr(char * name,Bit16u * attr);

/* IOCTL Stuff */
bool DOS_IOCTL(void);
bool DOS_GetSTDINStatus();
Bit8u DOS_FindDevice(char * name);
void DOS_SetupDevices(void);

/* Execute and new process creation */
bool DOS_NewPSP(Bit16u pspseg,Bit16u size);
bool DOS_Execute(char * name,PhysPt block,Bit8u flags);
bool DOS_Terminate(bool tsr);

/* Memory Handling Routines */
void DOS_SetupMemory(void);
bool DOS_AllocateMemory(Bit16u * segment,Bit16u * blocks);
bool DOS_ResizeMemory(Bit16u segment,Bit16u * blocks);
bool DOS_FreeMemory(Bit16u segment);
void DOS_FreeProcessMemory(Bit16u pspseg);
Bit16u DOS_GetMemory(Bit16u pages);
void DOS_SetMemAllocStrategy(Bit16u strat);
Bit16u DOS_GetMemAllocStrategy(void);

/* FCB stuff */
bool DOS_FCBOpen(Bit16u seg,Bit16u offset);
bool DOS_FCBCreate(Bit16u seg,Bit16u offset);
bool DOS_FCBClose(Bit16u seg,Bit16u offset);
bool DOS_FCBFindFirst(Bit16u seg,Bit16u offset);
bool DOS_FCBFindNext(Bit16u seg,Bit16u offset);
Bit8u DOS_FCBRead(Bit16u seg,Bit16u offset, Bit16u numBlocks);
Bit8u DOS_FCBWrite(Bit16u seg,Bit16u offset,Bit16u numBlocks);
Bit8u DOS_FCBRandomRead(Bit16u seg,Bit16u offset,Bit16u numRec,bool restore);
Bit8u DOS_FCBRandomWrite(Bit16u seg,Bit16u offset,Bit16u numRec,bool restore);
bool DOS_FCBGetFileSize(Bit16u seg,Bit16u offset,Bit16u numRec);
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

INLINE Bit16u long2para(Bit32u size) {
	if (size>0xFFFF0) return 0xffff;
	if (size&0xf) return (Bit16u)((size>>4)+1);
	else return (Bit16u)(size>>4);
}


INLINE Bit16u DOS_PackTime(Bit16u hour,Bit16u min,Bit16u sec) {
	return (hour&0x1f)<<11 | (min&0x3f) << 5 | ((sec/2)&0x1f);
}

INLINE Bit16u DOS_PackDate(Bit16u year,Bit16u mon,Bit16u day) {
	return ((year-1980)&0x7f)<<9 | (mon&0x3f) << 5 | (day&0x1f);
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

/* Remains some classes used to access certain things */

#define sGet(s,m) GetIt(((s *)Phys2Host(pt))->m,(PhysPt)&(((s *)0)->m))
#define sSave(s,m,val) SaveIt(((s *)Phys2Host(pt))->m,(PhysPt)&(((s *)0)->m),val)

class MemStruct {
public:
	INLINE Bit8u GetIt(Bit8u,PhysPt addr) {
		return mem_readb(pt+addr);
	}
	INLINE Bit16u GetIt(Bit16u,PhysPt addr) {
		return mem_readw(pt+addr);
	}
	INLINE Bit32u GetIt(Bit32u,PhysPt addr) {
		return mem_readd(pt+addr);
	}
	INLINE void SaveIt(Bit8u,PhysPt addr,Bit8u val) {
		mem_writeb(pt+addr,val);
	}
	INLINE void SaveIt(Bit16u,PhysPt addr,Bit16u val) {
		mem_writew(pt+addr,val);
	}
	INLINE void SaveIt(Bit32u,PhysPt addr,Bit32u val) {
		mem_writed(pt+addr,val);
	}
	INLINE void SetPt(Bit16u seg) { pt=PhysMake(seg,0);}
	INLINE void SetPt(Bit16u seg,Bit16u off) { pt=PhysMake(seg,off);}
	INLINE void SetPt(RealPt addr) { pt=Real2Phys(addr);}
protected:
	PhysPt pt;
};

class DOS_PSP :public MemStruct {
public:
	DOS_PSP						(Bit16u segment)		{ SetPt(segment);seg=segment;psp=(sPSP *)HostMake(segment,0);};
	void	MakeNew				(Bit16u memSize);
	void	CopyFileTable		(DOS_PSP* srcpsp);
	Bit16u	FindFreeFileEntry	(void);
	void	CloseFiles			(void);

	void	SaveVectors			(void);
	void	RestoreVectors		(void);
	void	SetSize				(Bit16u size)			{ sSave(sPSP,next_seg,size);		};
	Bit16u	GetSize				()						{ return sGet(sPSP,next_seg);		};
	void	SetDTA				(RealPt ptdta)			{ sSave(sPSP,dta,ptdta);			};
	RealPt	GetDTA				(void)					{ return sGet(sPSP,dta);			};
	void	SetEnvironment		(Bit16u envseg)			{ sSave(sPSP,environment,envseg);	};
	Bit16u	GetEnvironment		(void)					{ return sGet(sPSP,environment);	};
	Bit16u	GetSegment			(void)					{ return seg;						};
	void	SetFileHandle		(Bit16u index, Bit8u handle);
	Bit8u	GetFileHandle		(Bit16u index);
	void	SetParent			(Bit16u parent)			{ sSave(sPSP,psp_parent,parent);	};
	Bit16u	GetParent			(void)					{ return sGet(sPSP,psp_parent);		};
	void	SetStack			(RealPt stackpt)		{ sSave(sPSP,stack,stackpt);		};
	RealPt	GetStack			(void)					{ return sGet(sPSP,stack);			};
	void	SetInt22			(RealPt int22pt)		{ sSave(sPSP,int_22,int22pt);		};
	RealPt	GetInt22			(void)					{ return sGet(sPSP,int_22);			};
	void	SetFCB1				(RealPt src);
	void	SetFCB2				(RealPt src);
	void	SetCommandTail		(RealPt src);	
	bool	SetNumFiles			(Bit16u fileNum);
	
private:
	#pragma pack(1)
	struct sPSP {
		Bit8u	exit[2];			/* CP/M-like exit poimt */
		Bit16u	next_seg;			/* Segment of first byte beyond memory allocated or program */
		Bit8u	fill_1;				/* single char fill */
		Bit8u	far_call;			/* far call opcode */
		RealPt	cpm_entry;			/* CPM Service Request address*/
		RealPt	int_22;				/* Terminate Address */
		RealPt	int_23;				/* Break Address */
		RealPt	int_24;				/* Critical Error Address */
		Bit16u	psp_parent;			/* Parent PSP Segment */
		Bit8u	files[20];			/* File Table - 0xff is unused */
		Bit16u	environment;		/* Segment of evironment table */
		RealPt	stack;				/* SS:SP Save point for int 0x21 calls */
		Bit16u	max_files;			/* Maximum open files */
		RealPt	file_table;			/* Pointer to File Table PSP:0x18 */
		RealPt	prev_psp;			/* Pointer to previous PSP */
		RealPt	dta;				/* Pointer to current Process DTA */
		Bit8u	fill_2[16];			/* Lot's of unused stuff i can't care aboue */
		Bit8u	service[3];			/* INT 0x21 Service call int 0x21;retf; */
		Bit8u	fill_3[9];			/* This has some blocks with FCB info */
		Bit8u	fcb1[16];			/* first FCB */
		Bit8u	fcb2[16];			/* second FCB */
		Bit8u	fill_4[4];			/* unused */
		CommandTail cmdtail;		
	} GCC_ATTRIBUTE(packed);
	#pragma pack()	
	Bit16u	seg;
	sPSP*	psp;
public:
	static	Bit16u rootpsp;
};

class DOS_ParamBlock:public MemStruct {
public:
	DOS_ParamBlock(PhysPt addr) {pt=addr;}
	void Clear(void);
	void LoadData(void);
	void SaveData(void);		/* Save it as an exec block */
	#pragma pack (1)
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
	#pragma pack()
	sExec exec;
	sOverlay overlay;
};

class DOS_InfoBlock:public MemStruct {
public:
	DOS_InfoBlock			() {};
	void SetLocation(Bit16u  seg);
	void SetFirstMCB(Bit16u _first_mcb);
	void SetfirstFileTable(RealPt _first_table);
	RealPt GetPointer (void);
private:
	#pragma pack(1)
	struct sDIB {		
		Bit8u	stuff1[22];			// some stuff, hopefully never used....
		Bit16u	firstMCB;			// first memory control block
		RealPt	firstDPB;			// first drive parameter block
		RealPt	firstFileTable;		// first system file table
		RealPt	activeClock;		// active clock device header
		RealPt	activeCon;			// active console device header
		Bit16u	maxSectorLength;	// maximum bytes per sector of any block device;
		RealPt	discInfoBuffer;		// pointer to disc info buffer
		RealPt  curDirStructure;	// pointer to current array of directory structure
		RealPt	fcbTable;			// pointer to system FCB table
		// some more stuff, hopefully never used.
	} GCC_ATTRIBUTE(packed);
	#pragma pack ()
	Bit16u	seg;
};

class DOS_DTA:public MemStruct{
public:
	DOS_DTA(RealPt addr) { SetPt(addr); }

	void SetupSearch(Bit8u _sdrive,Bit8u _sattr,char * _pattern);
	void SetResult(const char * _name,Bit32u _size,Bit16u _date,Bit16u _time,Bit8u _attr);
	
	Bit8u GetSearchDrive(void);
	void GetSearchParams(Bit8u & _sattr,char * _spattern);
	void GetResult(char * _name,Bit32u & _size,Bit16u & _date,Bit16u & _time,Bit8u & _attr);

	void	SetDirID(Bit16u entry)		{ sSave(sDTA,dirID,entry); };
	Bit16u	GetDirID(void)				{ return sGet(sDTA,dirID); };
private:
	#pragma pack(1)
	struct sDTA {
		Bit8u sdrive;						/* The Drive the search is taking place */
		Bit8u sattr;						/* The Attributes that need to be found */
		Bit8u sname[8];						/* The Search pattern for the filename */		
		Bit8u sext[3];						/* The Search pattern for the extenstion */
		Bit16u dirID;						/* custom: dir-search ID for multiple searches at the same time */
		Bit8u fill[6];
		Bit8u attr;
		Bit16u time;
		Bit16u date;
		Bit32u size;
		char name[DOS_NAMELENGTH_ASCII];
	} GCC_ATTRIBUTE(packed);
	#pragma pack()
};

class DOS_FCB: public MemStruct {
public:
	DOS_FCB(Bit16u seg,Bit16u off);
	void Create(bool _extended);
	void SetName(Bit8u _drive,char * _fname,char * _ext);
	void SetSizeDateTime(Bit32u _size,Bit16u _date,Bit16u _time);
	void GetSizeDateTime(Bit32u & _size,Bit16u & _date,Bit16u & _time);
	void GetName(char * fillname);
	void FileOpen(Bit8u _fhandle);
	void FileClose(Bit8u & _fhandle);
	void GetRecord(Bit16u & _cur_block,Bit8u & _cur_rec);
	void SetRecord(Bit16u _cur_block,Bit8u _cur_rec);
	void GetSeqData(Bit8u & _fhandle,Bit16u & _rec_size);
	void GetRandom(Bit32u & _random);
	void SetRandom(Bit32u  _random);
	Bit8u GetDrive(void);
	bool Extended(void);
private:
	bool extended;
	PhysPt real_pt;
	#pragma pack (1)
	struct sFCB {
		Bit8u drive;			/* Drive number 0=default, 1=A, etc */
		Bit8u filename[8];		/* Space padded name */
		Bit8u ext[3];			/* Space padded extension */
		Bit16u cur_block;		/* Current Block */
		Bit16u rec_size;		/* Logical record size */
		Bit32u filesize;		/* File Size */
		Bit16u date;
		Bit16u time;
		/* Reserved Block should be 8 bytes */
		Bit8u file_handle;
		Bit8u reserved[7];
		/* end */
		Bit8u  cur_rec;			/* Current record in current block */
		Bit32u rndm;			/* Current relative record number */
	} GCC_ATTRIBUTE(packed);
	#pragma pack ()
};

class DOS_MCB : public MemStruct{
public:
	DOS_MCB(Bit16u seg) { SetPt(seg); }
	void SetFileName(char * _name) { MEM_BlockWrite(pt+offsetof(sMCB,filename),_name,8); }
	void GetFileName(char * _name) { MEM_BlockRead(pt+offsetof(sMCB,filename),_name,8);_name[8]=0;}
	void SetType(Bit8u _type) { sSave(sMCB,type,_type);}
	void SetSize(Bit16u _size) { sSave(sMCB,size,_size);}
	void SetPSPSeg(Bit16u _pspseg) { sSave(sMCB,psp_segment,_pspseg);}
	Bit8u GetType(void) { return sGet(sMCB,type);}
	Bit16u GetSize(void) { return sGet(sMCB,size);}
	Bit16u GetPSPSeg(void) { return sGet(sMCB,psp_segment);}
private:
	#pragma pack (1)
	struct sMCB {
		Bit8u type;
		Bit16u psp_segment;
		Bit16u size;	
		Bit8u unused[3];
		Bit8u filename[8];
	} GCC_ATTRIBUTE(packed);
	#pragma pack ()
};

extern DOS_InfoBlock dos_infoblock;;

INLINE Bit8u RealHandle(Bit16u handle) {
	DOS_PSP psp(dos.psp);	
	return psp.GetFileHandle(handle);
}

#endif

