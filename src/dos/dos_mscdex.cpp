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

/* $Id: dos_mscdex.cpp,v 1.32 2005-09-28 11:21:50 qbix79 Exp $ */

#include <string.h>
#include <ctype.h>
#include "regs.h"
#include "callback.h"
#include "dos_system.h"
#include "dos_inc.h"
#include "setup.h"
#include "support.h"

#include "cdrom.h"

#define MSCDEX_LOG LOG(LOG_MISC,LOG_ERROR)
//#define MSCDEX_LOG

#define MSCDEX_VERSION_HIGH	2
#define MSCDEX_VERSION_LOW	23
#define MSCDEX_MAX_DRIVES	5

// Error Codes
#define MSCDEX_ERROR_BAD_FORMAT			11
#define MSCDEX_ERROR_UNKNOWN_DRIVE		15
#define MSCDEX_ERROR_DRIVE_NOT_READY	21

// Request Status
#define	REQUEST_STATUS_DONE		0x0100
#define	REQUEST_STATUS_ERROR	0x8000

// Use cdrom Interface
int useCdromInterface	= CDROM_USE_SDL;
int forceCD				= -1;

static Bitu MSCDEX_Strategy_Handler(void); 
static Bitu MSCDEX_Interrupt_Handler(void);

class DOS_DeviceHeader:public MemStruct
{
public:
	DOS_DeviceHeader(PhysPt ptr)				{ pt = ptr; };
	
	void	SetNextDeviceHeader	(RealPt ptr)	{ sSave(sDeviceHeader,nextDeviceHeader,ptr);	};
	RealPt	GetNextDeviceHeader	(void)			{ return sGet(sDeviceHeader,nextDeviceHeader);	};
	void	SetAttribute		(Bit16u atr)	{ sSave(sDeviceHeader,devAttributes,atr);		};
	void	SetDriveLetter		(Bit8u letter)	{ sSave(sDeviceHeader,driveLetter,letter);		};
	void	SetNumSubUnits		(Bit8u num)		{ sSave(sDeviceHeader,numSubUnits,num);			};
	Bit8u	GetNumSubUnits		(void)			{ return sGet(sDeviceHeader,numSubUnits);		};
	void	SetName				(char* _name)	{ MEM_BlockWrite(pt+offsetof(sDeviceHeader,name),_name,8); };
	void	SetInterrupt		(Bit16u ofs)	{ sSave(sDeviceHeader,interrupt,ofs);			};
	void	SetStrategy			(Bit16u ofs)	{ sSave(sDeviceHeader,strategy,ofs);			};

public:
	#ifdef _MSC_VER
	#pragma pack(1)
	#endif
	struct sDeviceHeader{
		RealPt	nextDeviceHeader;
		Bit16u	devAttributes;
		Bit16u	strategy;
		Bit16u	interrupt;
		Bit8u	name[8];
		Bit16u  wReserved;
		Bit8u	driveLetter;
		Bit8u	numSubUnits;
	} TDeviceHeader;
	#ifdef _MSC_VER
	#pragma pack()
	#endif
};

class CMscdex
{
public:
	CMscdex		(void);
	~CMscdex	(void);

	Bit16u		GetVersion			(void)	{ return (MSCDEX_VERSION_HIGH<<8)+MSCDEX_VERSION_LOW; };
	Bit16u		GetNumDrives		(void)	{ return numDrives;			};
	Bit16u		GetFirstDrive		(void)	{ return dinfo[0].drive; };
	Bit8u		GetSubUnit			(Bit16u _drive);
	bool		GetUPC				(Bit8u subUnit, Bit8u& attr, char* upc);

	void		InitNewMedia		(Bit8u subUnit);
	bool		PlayAudioSector		(Bit8u subUnit, Bit32u start, Bit32u length);
	bool		PlayAudioMSF		(Bit8u subUnit, Bit32u start, Bit32u length);
	bool		StopAudio			(Bit8u subUnit);
	bool		GetAudioStatus		(Bit8u subUnit, bool& playing, bool& pause, TMSF& start, TMSF& end);

	bool		GetSubChannelData	(Bit8u subUnit, Bit8u& attr, Bit8u& track, Bit8u &index, TMSF& rel, TMSF& abs);

	int			AddDrive			(Bit16u _drive, char* physicalPath, Bit8u& subUnit);
	void		GetDrives			(PhysPt data);
	void		GetDriverInfo		(PhysPt data);
	bool		GetVolumeName		(Bit8u subUnit, char* name);
	bool		GetCopyrightName	(Bit16u drive, PhysPt data);
	bool		GetAbstractName		(Bit16u drive, PhysPt data);
	bool		GetDocumentationName(Bit16u drive, PhysPt data);
	bool		GetDirectoryEntry	(Bit16u drive, bool copyFlag, PhysPt pathname, PhysPt buffer, Bitu& error);
	bool		ReadVTOC			(Bit16u drive, Bit16u volume, PhysPt data, Bit16u& error);
	bool		ReadSectors			(Bit16u drive, Bit32u sector, Bit16u num, PhysPt data);
	bool		ReadSectors			(Bit8u subUnit, bool raw, Bit32u sector, Bit16u num, PhysPt data);
	bool		ReadSectorsMSF		(Bit8u subUnit, bool raw, Bit32u sector, Bit16u num, PhysPt data);
	bool		SendDriverRequest	(Bit16u drive, PhysPt data);
	bool		IsValidDrive		(Bit16u drive);
	bool		GetCDInfo			(Bit8u subUnit, Bit8u& tr1, Bit8u& tr2, TMSF& leadOut);
	Bit32u		GetVolumeSize		(Bit8u subUnit);
	bool		GetTrackInfo		(Bit8u subUnit, Bit8u track, Bit8u& attr, TMSF& start);
	Bit16u		GetStatusWord		(Bit8u subUnit);
	bool		GetCurrentPos		(Bit8u subUnit, TMSF& pos);
	Bit32u		GetDeviceStatus		(Bit8u subUnit);
	bool		GetMediaStatus		(Bit8u subUnit, Bit8u& status);
	bool		LoadUnloadMedia		(Bit8u subUnit, bool unload);
	bool		ResumeAudio			(Bit8u subUnit);
	bool		GetMediaStatus		(Bit8u subUnit, bool& media, bool& changed, bool& trayOpen);

private:

	PhysPt		GetDefaultBuffer	(void);

	Bit16u		numDrives;

	typedef struct SDriveInfo {
		Bit8u	drive;			// drive letter in dosbox
		Bit8u	physDrive;		// drive letter in system
		bool	audioPlay;		// audio playing active
		bool	audioPaused;	// audio playing paused
		Bit32u	audioStart;		// StartLoc for resume
		Bit32u	audioEnd;		// EndLoc for resume
		bool	locked;			// drive locked ?
		bool	lastResult;		// last operation success ?
		Bit32u	volumeSize;		// for media change
	} TDriveInfo;

	PhysPt				defaultBuffer;
	TDriveInfo			dinfo[MSCDEX_MAX_DRIVES];
	CDROM_Interface*	cdrom[MSCDEX_MAX_DRIVES];
	
public:
	Bit16u		rootDriverHeaderSeg;
};

CMscdex::CMscdex(void)
{
	numDrives			= 0;
	rootDriverHeaderSeg	= 0;
	defaultBuffer		= 0;

	memset(dinfo,0,sizeof(dinfo));
	for (Bit32u i=0; i<MSCDEX_MAX_DRIVES; i++) cdrom[i] = 0;
};

CMscdex::~CMscdex(void)
{
	if (defaultBuffer!=0) {
		DOS_FreeMemory(RealSeg(defaultBuffer));
		defaultBuffer = 0;
	}
	for (Bit16u i=0; i<GetNumDrives(); i++) {
		delete (cdrom)[i];
		cdrom[i] = 0;
	};
};

void CMscdex::GetDrives(PhysPt data)
{
	for (Bit16u i=0; i<GetNumDrives(); i++) mem_writeb(data+i,dinfo[i].drive);
};

bool CMscdex::IsValidDrive(Bit16u _drive)
{
	for (Bit16u i=0; i<GetNumDrives(); i++) if (dinfo[i].drive==_drive) return true;
	return false;
};

Bit8u CMscdex::GetSubUnit(Bit16u _drive)
{
	for (Bit16u i=0; i<GetNumDrives(); i++) if (dinfo[i].drive==_drive) return (Bit8u)i;
	return 0xff;
};

int CMscdex::AddDrive(Bit16u _drive, char* physicalPath, Bit8u& subUnit)
{
	subUnit = 0;
	if (GetNumDrives()==0) {
		
		Bit16u driverSize = sizeof(DOS_DeviceHeader::sDeviceHeader) + 10; // 10 = Bytes for 3 callbacks
		
		// Create Device Header
		Bit16u seg = DOS_GetMemory(driverSize/16+((driverSize%16)>0));
		DOS_DeviceHeader devHeader(PhysMake(seg,0));
		devHeader.SetNextDeviceHeader	(0xFFFFFFFF);
		devHeader.SetDriveLetter		(_drive+1);
		devHeader.SetNumSubUnits		(1);
		devHeader.SetName				("MSCD001 ");

		// Create Callback Strategy
		Bit16u off = sizeof(DOS_DeviceHeader::sDeviceHeader);
		Bitu call_strategy=CALLBACK_Allocate();
		CallBack_Handlers[call_strategy]=MSCDEX_Strategy_Handler;
		real_writeb(seg,off+0,(Bit8u)0xFE);		//GRP 4
		real_writeb(seg,off+1,(Bit8u)0x38);		//Extra Callback instruction
		real_writew(seg,off+2,call_strategy);	//The immediate word
		real_writeb(seg,off+4,(Bit8u)0xCB);		//A RETF Instruction
		devHeader.SetStrategy(off);
		
		// Create Callback Interruptoff += 5;
		Bitu call_interrupt=CALLBACK_Allocate();
		CallBack_Handlers[call_interrupt]=MSCDEX_Interrupt_Handler;
		real_writeb(seg,off+0,(Bit8u)0xFE);		//GRP 4
		real_writeb(seg,off+1,(Bit8u)0x38);		//Extra Callback instruction
		real_writew(seg,off+2,call_interrupt);	//The immediate word
		real_writeb(seg,off+4,(Bit8u)0xCB);		//A RETF Instruction
		devHeader.SetInterrupt(off);
		
		rootDriverHeaderSeg = seg;
	
	} else {
		// Error check, driveletter have to be in a row
		if (dinfo[numDrives-1].drive+1!=_drive) return 1;
	};

	if (GetNumDrives()+1<MSCDEX_MAX_DRIVES) {
		// Set return type to ok
		int result = 0;
		// Get Mounttype and init needed cdrom interface
		switch (CDROM_GetMountType(physicalPath,forceCD)) {
			case 0x00	: {	
							LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: Mounting physical cdrom: %s"	,physicalPath);
							#if defined (WIN32)
								// Check OS
								OSVERSIONINFO osi;
								osi.dwOSVersionInfoSize = sizeof(osi);
								GetVersionEx(&osi);
								if ((osi.dwPlatformId==VER_PLATFORM_WIN32_NT) && (osi.dwMajorVersion>4)) {
									// only WIN NT/200/XP
									if (useCdromInterface==CDROM_USE_IOCTL) {
										cdrom[numDrives] = new CDROM_Interface_Ioctl();
										LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: IOCTL Interface.");
										break;
									}
								}
								if (useCdromInterface==CDROM_USE_ASPI) {
									// all Wins - ASPI
									cdrom[numDrives] = new CDROM_Interface_Aspi();
									LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: ASPI Interface.");
									break;
								}
							#endif
							#if defined (LINUX)
								// Always use IOCTL in Linux
//								if (useCdromInterface==CDROM_USE_IOCTL) {
									cdrom[numDrives] = new CDROM_Interface_Ioctl();
									LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: IOCTL Interface.");
									break;
//								}
							#endif
							cdrom[numDrives] = new CDROM_Interface_SDL();
							LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: SDL Interface.");
						  } break;
			case 0x01	:	// iso cdrom interface	
							LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: Mounting iso file as cdrom: %s", physicalPath);
							cdrom[numDrives] = new CDROM_Interface_Image((Bit8u)numDrives);
							break;
			case 0x02	:	// fake cdrom interface (directories)
							cdrom[numDrives] = new CDROM_Interface_Fake;
							LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: Mounting directory as cdrom: %s",physicalPath);	
							LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: You wont have full MSCDEX support !");	
							result = 5;
							break;
			default		:	// weird result
							return 6;
		};
		if (!cdrom[numDrives]->SetDevice(physicalPath,forceCD)) return 3;
		subUnit = (Bit8u)numDrives;
		// Set drive
		DOS_DeviceHeader devHeader(PhysMake(rootDriverHeaderSeg,0));
		devHeader.SetNumSubUnits(devHeader.GetNumSubUnits()+1);
		dinfo[numDrives].drive		= (Bit8u)_drive;
		dinfo[numDrives].physDrive	= toupper(physicalPath[0]);
		numDrives++;
		// stop audio
		StopAudio(subUnit);
		return result;
	}
	return 4;
};

PhysPt CMscdex::GetDefaultBuffer(void)
{
	if (defaultBuffer==0) {
		Bit16u size = 128; //Size in block is size in pages ?
		defaultBuffer = DOS_GetMemory(size);
	};
	return PhysMake(defaultBuffer,0);
};

void CMscdex::GetDriverInfo	(PhysPt data)
{
	for (Bit16u i=0; i<GetNumDrives(); i++) {
		mem_writeb(data  ,0x00);
		mem_writed(data+1,RealMake(rootDriverHeaderSeg,0));
		data+=5;
	};
};

bool CMscdex::GetCDInfo(Bit8u subUnit, Bit8u& tr1, Bit8u& tr2, TMSF& leadOut)
{
	if (subUnit>=numDrives) return false;
	int tr1i,tr2i;
	// Assume Media change
	cdrom[subUnit]->InitNewMedia();
	dinfo[subUnit].lastResult = cdrom[subUnit]->GetAudioTracks(tr1i,tr2i,leadOut);
	if (!dinfo[subUnit].lastResult) {
		tr1 = tr2 = 0;
		memset(&leadOut,0,sizeof(leadOut));
	} else {
		tr1 = (Bit8u) tr1i;
		tr2 = (Bit8u) tr2i;
	}
	return dinfo[subUnit].lastResult;
}

bool CMscdex::GetTrackInfo(Bit8u subUnit, Bit8u track, Bit8u& attr, TMSF& start)
{
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = cdrom[subUnit]->GetAudioTrackInfo(track,start,attr);	
	if (!dinfo[subUnit].lastResult) {
		attr = 0;
		memset(&start,0,sizeof(start));
	};
	return dinfo[subUnit].lastResult;
};

bool CMscdex::PlayAudioSector(Bit8u subUnit, Bit32u sector, Bit32u length)
{
	if (subUnit>=numDrives) return false;
	// If value from last stop is used, this is meant as a resume
	// better start using resume command
	if (dinfo[subUnit].audioPaused && (sector==dinfo[subUnit].audioStart)) {
		dinfo[subUnit].lastResult = cdrom[subUnit]->PauseAudio(true);
	} else 
		dinfo[subUnit].lastResult = cdrom[subUnit]->PlayAudioSector(sector,length);

	if (dinfo[subUnit].lastResult) {
		dinfo[subUnit].audioPlay	= true;
		dinfo[subUnit].audioPaused	= false;
		dinfo[subUnit].audioStart	= sector;
		dinfo[subUnit].audioEnd		= length;
	};
	return dinfo[subUnit].lastResult;
};

bool CMscdex::PlayAudioMSF(Bit8u subUnit, Bit32u start, Bit32u length)
{
	if (subUnit>=numDrives) return false;
	Bit8u min		= (Bit8u)(start>>16) & 0xFF;
	Bit8u sec		= (Bit8u)(start>> 8) & 0xFF;
	Bit8u fr		= (Bit8u)(start>> 0) & 0xFF;
	Bit32u sector	= min*60*75+sec*75+fr - 150;
	return dinfo[subUnit].lastResult = PlayAudioSector(subUnit,sector,length);
};

bool CMscdex::GetSubChannelData(Bit8u subUnit, Bit8u& attr, Bit8u& track, Bit8u &index, TMSF& rel, TMSF& abs)
{
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = cdrom[subUnit]->GetAudioSub(attr,track,index,rel,abs);
	if (!dinfo[subUnit].lastResult) {
		attr = track = index = 0;
		memset(&rel,0,sizeof(rel));
		memset(&abs,0,sizeof(abs));
	};
	return dinfo[subUnit].lastResult;
};

bool CMscdex::GetAudioStatus(Bit8u subUnit, bool& playing, bool& pause, TMSF& start, TMSF& end)
{
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = cdrom[subUnit]->GetAudioStatus(playing,pause);
	if (dinfo[subUnit].lastResult) {
		// Start
		Bit32u addr	= dinfo[subUnit].audioStart + 150;
		start.fr	= (Bit8u)(addr%75);	addr/=75;
		start.sec	= (Bit8u)(addr%60); 
		start.min	= (Bit8u)(addr/60);
		// End
		addr		= dinfo[subUnit].audioEnd + 150;
		end.fr		= (Bit8u)(addr%75);	addr/=75;
		end.sec		= (Bit8u)(addr%60); 
		end.min		= (Bit8u)(addr/60);
	} else {
		playing		= false;
		pause		= false;
		memset(&start,0,sizeof(start));
		memset(&end,0,sizeof(end));
	};
	
	return dinfo[subUnit].lastResult;
};

bool CMscdex::StopAudio(Bit8u subUnit)
{
	if (subUnit>=numDrives) return false;
	if (dinfo[subUnit].audioPlay)	dinfo[subUnit].lastResult = cdrom[subUnit]->PauseAudio(false);
	else							dinfo[subUnit].lastResult = cdrom[subUnit]->StopAudio();
	
	if (dinfo[subUnit].lastResult) {
		if (dinfo[subUnit].audioPlay) {
			TMSF pos;
			GetCurrentPos(subUnit,pos);
			dinfo[subUnit].audioStart	= pos.min*60*75+pos.sec*75+pos.fr - 150;
			dinfo[subUnit].audioPaused  = true;
		} else {	
			dinfo[subUnit].audioPaused  = false;
			dinfo[subUnit].audioStart	= 0;
			dinfo[subUnit].audioEnd		= 0;
		};
		dinfo[subUnit].audioPlay = false;
	};
	return dinfo[subUnit].lastResult;
};

bool CMscdex::ResumeAudio(Bit8u subUnit)
{
	if (subUnit>=numDrives) return false;
	return dinfo[subUnit].lastResult = PlayAudioSector(subUnit,dinfo[subUnit].audioStart,dinfo[subUnit].audioEnd);
};

Bit32u CMscdex::GetVolumeSize(Bit8u subUnit)
{
	if (subUnit>=numDrives) return false;
	Bit8u tr1,tr2;
	TMSF leadOut;
	dinfo[subUnit].lastResult = GetCDInfo(subUnit,tr1,tr2,leadOut);
	if (dinfo[subUnit].lastResult) return (leadOut.min*60*75)+(leadOut.sec*75)+leadOut.fr;
	return 0;
};

bool CMscdex::ReadVTOC(Bit16u drive, Bit16u volume, PhysPt data, Bit16u& error)	
{ 
     if (!ReadSectors(GetSubUnit(drive),false,16+volume,1,data)) {
          error=MSCDEX_ERROR_DRIVE_NOT_READY;
          return false;
     }
     char id[5];
     MEM_BlockRead(data + 1, id, 5);
     if (strncmp("CD001",id, 5)!=0) {
          error = MSCDEX_ERROR_BAD_FORMAT;
          return false;
     }
     Bit8u type = mem_readb(data);
     error = (type == 1) ? 1 : (type == 0xFF) ? 0xFF : 0;
     return true;
};

bool CMscdex::GetVolumeName(Bit8u subUnit, char* data) 
{	
	if (subUnit>=numDrives) return false;
	Bit16u drive = dinfo[subUnit].drive;

	Bit16u error,seg,size = 128;
	bool success = false;
	if (DOS_AllocateMemory(&seg,&size)) {
		PhysPt ptoc = PhysMake(seg,0);
		success = ReadVTOC(drive,0x00,ptoc,error);
		if (success) {
			MEM_StrCopy(ptoc+40,data,31);
			data[31] = 0;
			rtrim(data);
		};
		DOS_FreeMemory(seg);
	} else { LOG(LOG_MISC,LOG_ERROR)("MSCDEX buffer allocation failed."); };
	return success; 
};

bool CMscdex::GetCopyrightName(Bit16u drive, PhysPt data) 
{	
	Bit16u error,seg,size = 128;
	bool success = false;
	if (DOS_AllocateMemory(&seg,&size)) {
		PhysPt ptoc = PhysMake(seg,0);
		success = ReadVTOC(drive,0x00,ptoc,error);
		if (success) {
			MEM_BlockCopy(data,ptoc+702,37);
			mem_writeb(data+37,0);
		};
		DOS_FreeMemory(seg);
	} else { LOG(LOG_MISC,LOG_ERROR)("MSCDEX buffer allocation failed."); };
	return success; 
};

bool CMscdex::GetAbstractName(Bit16u drive, PhysPt data) 
{ 
	Bit16u error,seg,size = 128;
	bool success = false;
	if (DOS_AllocateMemory(&seg,&size)) {
		PhysPt ptoc = PhysMake(seg,0);
		success = ReadVTOC(drive,0x00,ptoc,error);
		if (success) {
			MEM_BlockCopy(data,ptoc+739,37);
			mem_writeb(data+37,0);
		};
		DOS_FreeMemory(seg);
	} else { LOG(LOG_MISC,LOG_ERROR)("MSCDEX buffer allocation failed."); };
	return success; 
};

bool CMscdex::GetDocumentationName(Bit16u drive, PhysPt data) 
{ 
	Bit16u error,seg,size = 128;
	bool success = false;
	if (DOS_AllocateMemory(&seg,&size)) {
		PhysPt ptoc = PhysMake(seg,0);
		success = ReadVTOC(drive,0x00,ptoc,error);
		if (success) {
			MEM_BlockCopy(data,ptoc+776,37);
			mem_writeb(data+37,0);
		};
		DOS_FreeMemory(seg);
	} else { LOG(LOG_MISC,LOG_ERROR)("MSCDEX buffer allocation failed."); };
	return success; 
};

bool CMscdex::GetUPC(Bit8u subUnit, Bit8u& attr, char* upc)
{
	if (subUnit>=numDrives) return false;
	return dinfo[subUnit].lastResult = cdrom[subUnit]->GetUPC(attr,&upc[0]);
};

bool CMscdex::ReadSectors(Bit8u subUnit, bool raw, Bit32u sector, Bit16u num, PhysPt data)
{
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = cdrom[subUnit]->ReadSectors(data,raw,sector,num);
	return dinfo[subUnit].lastResult;
};

bool CMscdex::ReadSectorsMSF(Bit8u subUnit, bool raw, Bit32u start, Bit16u num, PhysPt data)
{
	if (subUnit>=numDrives) return false;
	Bit8u min		= (Bit8u)(start>>16) & 0xFF;
	Bit8u sec		= (Bit8u)(start>> 8) & 0xFF;
	Bit8u fr		= (Bit8u)(start>> 0) & 0xFF;
	Bit32u sector	= min*60*75+sec*75+fr - 150;
	// TODO: Check, if num has to be converted too ?!
	return ReadSectors(subUnit,raw,sector,num,data);
};

bool CMscdex::ReadSectors(Bit16u drive, Bit32u sector, Bit16u num, PhysPt data)
// Called from INT 2F
{
	return ReadSectors(GetSubUnit(drive),false,sector,num,data);
};

bool CMscdex::GetDirectoryEntry(Bit16u drive, bool copyFlag, PhysPt pathname, PhysPt buffer, Bitu& error)
{
	char	volumeID[6] = {0};
	char	searchName[256];
	char	entryName[256];
	bool	foundComplete = false;
	bool	foundName;
	char*	useName;
	Bitu	entryLength,nameLength;
	// clear error
	error = 0;
	MEM_StrCopy(pathname+1,searchName,mem_readb(pathname));
	upcase(searchName);
	char* searchPos = searchName;

	//strip of tailing . (XCOM APOCALIPSE)
	int searchlen = strlen(searchName);
	if(searchlen > 1 && strcmp(searchName,".."))
		if(searchName[searchlen-1] =='.')  searchName[searchlen-1] = 0;

	//LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Get DirEntry : Find : %s",searchName);
	// read vtoc
	PhysPt defBuffer = GetDefaultBuffer();
	if (!ReadSectors(GetSubUnit(drive),false,16,1,defBuffer)) return false;
	// TODO: has to be iso 9960
	MEM_StrCopy(defBuffer+1,volumeID,5); volumeID[5] = 0;
	bool iso = (strcmp("CD001",volumeID)==0);
	if (!iso) E_Exit("MSCDEX: GetDirEntry: Not an ISO 9960 CD.");
	// get directory position
	Bitu dirEntrySector	= mem_readd(defBuffer+156+2);
	Bits dirSize		= mem_readd(defBuffer+156+10);
	Bitu index;
	while (dirSize>0) {
		index = 0;
		if (!ReadSectors(GetSubUnit(drive),false,dirEntrySector,1,defBuffer)) return false;
		// Get string part
		foundName	= false;
		if (searchPos) { 
			useName = searchPos; 
			searchPos = strchr(searchPos,'\\'); 
		}

	   	if (searchPos) { *searchPos = 0; searchPos++; }
		else foundComplete = true;

		do {
			entryLength = mem_readb(defBuffer+index);
			if (entryLength==0) break;
			nameLength  = mem_readb(defBuffer+index+32);
			MEM_StrCopy(defBuffer+index+33,entryName,nameLength);
			if (strcmp(entryName,useName)==0) {
				//LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Get DirEntry : Found : %s",useName);
				foundName = true;
				break;
			}
			/* Xcom Apocalipse searches for MUSIC. and expects to find MUSIC;1
			 * All Files on the CDROM are of the kind blah;1
			 */
			char* longername = strchr(entryName,';');
			if(longername) {
				*longername = 0;
				if (strcmp(entryName,useName)==0) {
					//LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Get DirEntry : Found : %s",useName);
					foundName = true;
					break;
				}
			}
			index += entryLength;
		} while (index+33<=2048);
		
		if (foundName) {
			// TO DO : name gefunden, Daten in den Buffer kopieren
			if (foundComplete) {
				if (copyFlag) LOG(LOG_MISC,LOG_ERROR)("MSCDEX: GetDirEntry: Unsupported copyflag. (result structure should be different");
				// Direct copy
				MEM_BlockCopy(buffer,defBuffer+index,entryLength);
				error = iso ? 1:0;
				return true;
			}
			// directory wechseln
			dirEntrySector = mem_readd(defBuffer+index+2);
			dirSize	= mem_readd(defBuffer+index+10);
		} else {
			// continue search in next sector
			dirSize -= 2048;
			dirEntrySector++;
		}
	};
	error = 2; // file not found
	return false; // not found
};

bool CMscdex::GetCurrentPos(Bit8u subUnit, TMSF& pos)
{
	if (subUnit>=numDrives) return false;
	TMSF rel;
	Bit8u attr,track,index;
	dinfo[subUnit].lastResult = GetSubChannelData(subUnit, attr, track, index, rel, pos);
	if (!dinfo[subUnit].lastResult) memset(&pos,0,sizeof(pos));
	return dinfo[subUnit].lastResult;
};

bool CMscdex::GetMediaStatus(Bit8u subUnit, bool& media, bool& changed, bool& trayOpen)
{
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = cdrom[subUnit]->GetMediaTrayStatus(media,changed,trayOpen);
	return dinfo[subUnit].lastResult;
};

Bit32u CMscdex::GetDeviceStatus(Bit8u subUnit)
{
	if (subUnit>=numDrives) return false;
	bool media,changed,trayOpen;

	dinfo[subUnit].lastResult = GetMediaStatus(subUnit,media,changed,trayOpen);
	Bit32u status = (trayOpen << 0)					|	// Drive is open ?
					(dinfo[subUnit].locked	<< 1)	|	// Drive is locked ?
					(1<<2)							|	// raw + cooked sectors
					(1<<4)							|	// Can read sudio
					(1<<9)							|	// Red book & HSG
					((!media) << 11);					// Drive is empty ?
	return status;
};

bool CMscdex::GetMediaStatus(Bit8u subUnit, Bit8u& status)
{
	if (subUnit>=numDrives) return false;
	bool media,changed,open,result;
	result = GetMediaStatus(subUnit,media,changed,open);
	status = changed ? 0xFF : 0x01;
	return result;
};

bool CMscdex::LoadUnloadMedia(Bit8u subUnit, bool unload)
{
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = cdrom[subUnit]->LoadUnloadMedia(unload);
	return dinfo[subUnit].lastResult;
};

bool CMscdex::SendDriverRequest(Bit16u drive, PhysPt data)
{
	Bit8u subUnit = GetSubUnit(drive);
	if (subUnit>=numDrives) return false;	
	// Get SubUnit
	mem_writeb(data+1,subUnit);
	// Call Strategy / Interrupt
	MSCDEX_Strategy_Handler();
	MSCDEX_Interrupt_Handler();
	return true;
};

Bit16u CMscdex::GetStatusWord(Bit8u subUnit)
{
	if (subUnit>=numDrives) return 0x02; // error : Drive not ready

	Bit16u status ;
	if (dinfo[subUnit].lastResult)	status = REQUEST_STATUS_DONE;				// ok
	else							status = REQUEST_STATUS_ERROR; 

	if (dinfo[subUnit].audioPlay) {
		// Check if audio is still playing....
		TMSF start,end;
		bool playing,pause;
		if (GetAudioStatus(subUnit,playing,pause,start,end)) {
			dinfo[subUnit].audioPlay = playing;
		} else
			dinfo[subUnit].audioPlay = false;

		status |= (dinfo[subUnit].audioPlay<<9);
	} 
	dinfo[subUnit].lastResult	= true;
	return status;
};

void CMscdex::InitNewMedia(Bit8u subUnit) {
	if (subUnit<numDrives) {
		// Reopen new media
		cdrom[subUnit]->InitNewMedia();
	}
};

static CMscdex* mscdex = 0;

static Bitu MSCDEX_Strategy_Handler(void) {
//	MSCDEX_LOG("MSCDEX: Device Strategy Routine called.");
	return CBRET_NONE;
}

static Bitu MSCDEX_Interrupt_Handler(void) {
	Bit8u	subFuncNr	= 0xFF;
	PhysPt	data		= PhysMake(SegValue(es),reg_bx);
	Bit8u	subUnit		= mem_readb(data+1);
	Bit8u	funcNr		= mem_readb(data+2);

	MSCDEX_LOG("MSCDEX: Driver Function %02X",funcNr);

	switch (funcNr) {
	
		case 0x03	: {	/* IOCTL INPUT */
						PhysPt buffer	= PhysMake(mem_readw(data+0x10),mem_readw(data+0x0E));
						subFuncNr		= mem_readb(buffer);
						MSCDEX_LOG("MSCDEX: IOCTL INPUT Subfunction %02X",subFuncNr);
						switch (subFuncNr) {
							case 0x00 : /* Get Device Header address */
										mem_writed(buffer+1,RealMake(mscdex->rootDriverHeaderSeg,0));
										break;
							case 0x01 :{/* Get current position */
										TMSF pos;
										mscdex->GetCurrentPos(subUnit,pos);
										/*mem_writeb(buffer+1,0x01); // Red book
										mem_writeb(buffer+2,pos.fr);
										mem_writeb(buffer+3,pos.sec);
										mem_writeb(buffer+4,pos.min);
										mem_writeb(buffer+5,0x00);*/
							//Changed to HSG as default 
							//(Seems to fix a few broken games which don't test for it)
										mem_writeb(buffer+1,0x00); //HSG
										mem_writed(buffer+2,MSF_TO_FRAMES (pos.min, pos.sec, pos.fr));
									   }break;
							case 0x06 : /* Get Device status */
										mem_writed(buffer+1,mscdex->GetDeviceStatus(subUnit)); 
										break;
							case 0x07 : /* Get sector size */
										if (mem_readb(buffer+1)==0x01)	mem_writed(buffer+2,2352);
										else							mem_writed(buffer+2,2048);
										break;
							case 0x08 : /* Get size of current volume */
										mem_writed(buffer+1,mscdex->GetVolumeSize(subUnit));
										break;
							case 0x09 : /* Media change ? */
										Bit8u status;
										//TEMP mscdex->GetMediaStatus(subUnit,status);
										status = 1;
										mem_writeb(buffer+1,status);
										break;
							case 0x0A : /* Get Audio Disk info */	
										Bit8u tr1,tr2; TMSF leadOut;
										mscdex->GetCDInfo(subUnit,tr1,tr2,leadOut);
										mem_writeb(buffer+1,tr1);
										mem_writeb(buffer+2,tr2);
										mem_writeb(buffer+3,leadOut.fr);
										mem_writeb(buffer+4,leadOut.sec);
										mem_writeb(buffer+5,leadOut.min);
										mem_writeb(buffer+6,0x00);
										break;
							case 0x0B :{/* Audio Track Info */
										Bit8u attr; TMSF start;
										Bit8u track = mem_readb(buffer+1);
										mscdex->GetTrackInfo(subUnit,track,attr,start);		
										mem_writeb(buffer+2,start.fr);
										mem_writeb(buffer+3,start.sec);
										mem_writeb(buffer+4,start.min);
										mem_writeb(buffer+5,0x00);
										mem_writeb(buffer+6,attr);
										break; };
							case 0x0C :{/* Get Audio Sub Channel data */
										Bit8u attr,track,index; 
										TMSF abs,rel;
										mscdex->GetSubChannelData(subUnit,attr,track,index,rel,abs);
										mem_writeb(buffer+1,attr);
										mem_writeb(buffer+2,track);
										mem_writeb(buffer+3,index);
										mem_writeb(buffer+4,rel.min);
										mem_writeb(buffer+5,rel.sec);
										mem_writeb(buffer+6,rel.fr);
										mem_writeb(buffer+7,0x00);
										mem_writeb(buffer+8,abs.min);
										mem_writeb(buffer+9,abs.sec);
										mem_writeb(buffer+10,abs.fr);
										break;
									   };
							case 0x0E :{ /* Get UPC */	
										Bit8u attr; char upc[8];
										mscdex->GetUPC(subUnit,attr,&upc[0]);
										mem_writeb(buffer+1,attr);
										for (int i=0; i<7; i++) mem_writeb(buffer+2+i,upc[i]);
										mem_writeb(buffer+9,0x00);
										break;
									   };
							case 0x0F :{ /* Get Audio Status */	
										bool playing,pause;
										TMSF resStart,resEnd;
										mscdex->GetAudioStatus(subUnit,playing,pause,resStart,resEnd);
										mem_writeb(buffer+1,pause);
										mem_writeb(buffer+3,resStart.min);
										mem_writeb(buffer+4,resStart.sec);
										mem_writeb(buffer+5,resStart.fr);
										mem_writeb(buffer+6,0x00);
										mem_writeb(buffer+7,resEnd.min);
										mem_writeb(buffer+8,resEnd.sec);
										mem_writeb(buffer+9,resEnd.fr);
										mem_writeb(buffer+10,0x00);
										break;
									   };
							default :	LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Unsupported IOCTL INPUT Subfunction %02X",subFuncNr);
										break;
						}
						break;
					  };
		case 0x0C	: {	/* IOCTL OUTPUT */
						PhysPt buffer	= PhysMake(mem_readw(data+0x10),mem_readw(data+0x0E));
						subFuncNr		= mem_readb(buffer);
//						LOG(LOG_MISC,LOG_ERROR)("MSCDEX: IOCTL OUTPUT Subfunction %02X",subFuncNr);
						switch (subFuncNr) {
							case 0x00 :	// Unload /eject) media
										mscdex->LoadUnloadMedia(subUnit,true);
										break;
							case 0x01 : // (un)Lock door 
										// do nothing -> report as success
										break;
							case 0x02 : // Reset Drive
										LOG(LOG_MISC,LOG_WARN)("cdromDrive reset");
										mscdex->StopAudio(subUnit);
										break;
							case 0x05 :	// load media
										mscdex->LoadUnloadMedia(subUnit,false);
										break;
							default	:	LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Unsupported IOCTL OUTPUT Subfunction %02X",subFuncNr);
										break;
						};
						break;
					  };
		case 0x0D	:	// device open
		case 0x0E	:	// device close - dont care :)
						break;
		case 0x80	:	// Read long
		case 0x82	: { // Read long prefetch -> both the same here :)
						PhysPt buff  = PhysMake(mem_readw(data+0x10),mem_readw(data+0x0E));
						Bit32u start = mem_readd(data+0x14);
						Bit16u len	 = mem_readw(data+0x12);
						bool raw	 = (mem_readb(data+0x18)==1);
						if (mem_readb(data+0x0D)==0x00) // HSG
							mscdex->ReadSectors(subUnit,raw,start,len,buff);
						else 
							mscdex->ReadSectorsMSF(subUnit,raw,start,len,buff);
						break;
					  };
		case 0x83	:	// Seek - dont care :)
						break;
		case 0x84	: {	/* Play Audio Sectors */
						Bit32u start = mem_readd(data+0x0E);
						Bit32u len	 = mem_readd(data+0x12);
						if (mem_readb(data+0x0D)==0x00) // HSG
							mscdex->PlayAudioSector(subUnit,start,len);
						else // RED BOOK
							mscdex->PlayAudioMSF(subUnit,start,len);
						break;
					  };
		case 0x85	:	/* Stop Audio */
						mscdex->StopAudio(subUnit);
						break;
		case 0x88	:	/* Resume Audio */
						mscdex->ResumeAudio(subUnit);
						break;
		default		:	LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Unsupported Driver Request %02X",funcNr);
						break;
	
	};
	
	// Set Statusword
	mem_writew(data+3,mscdex->GetStatusWord(subUnit));
	MSCDEX_LOG("MSCDEX: Status : %04X",mem_readw(data+3));						
	return CBRET_NONE;
}

static bool MSCDEX_Handler(void) {
	if(reg_ah == 0x11) {
		if(reg_al == 0x00) { 
			reg_al = 0xff;
			return true;
		} else {
			LOG(LOG_MISC,LOG_ERROR)("NETWORK REDIRECTOR USED!!!");
			reg_ax = 0x49;//NETWERK SOFTWARE NOT INSTALLED
			CALLBACK_SCF(true);
			return true;
		}
	}

	if (reg_ah!=0x15) return false;

	PhysPt data = PhysMake(SegValue(es),reg_bx);
	MSCDEX_LOG("MSCDEX: INT 2F %04X BX= %04X CX=%04X",reg_ax,reg_bx,reg_bx);
	switch (reg_ax) {
	
		case 0x1500:	/* Install check */
						reg_bx = mscdex->GetNumDrives();
						if (reg_bx>0) reg_cx = mscdex->GetFirstDrive();
						return true;
		case 0x1501:	/* Get cdrom driver info */
						mscdex->GetDriverInfo(data);
						return true;
		case 0x1502:	/* Get Copyright filename */
						if (mscdex->GetCopyrightName(reg_cx,data)) {
							CALLBACK_SCF(false);
						} else {
							reg_al = MSCDEX_ERROR_UNKNOWN_DRIVE;
							CALLBACK_SCF(true);							
						};
						return true;		
		case 0x1503:	/* Get Abstract filename */
						if (mscdex->GetAbstractName(reg_cx,data)) {
							CALLBACK_SCF(false);
						} else {
							reg_al = MSCDEX_ERROR_UNKNOWN_DRIVE;
							CALLBACK_SCF(true);							
						};
						return true;		
		case 0x1504:	/* Get Documentation filename */
						if (mscdex->GetDocumentationName(reg_cx,data)) {
							CALLBACK_SCF(false);
						} else {
							reg_al = MSCDEX_ERROR_UNKNOWN_DRIVE;
							CALLBACK_SCF(true);							
						};
						return true;		
		case 0x1505: {	// read vtoc 
						Bit16u error = 0;
						if (mscdex->ReadVTOC(reg_cx,reg_dx,data,error)) {
							CALLBACK_SCF(false);
						} else {
							reg_ax = error;
							CALLBACK_SCF(true);							
						};
					 };
						return true;
		case 0x1508: {	// read sectors 
						Bit32u sector = (reg_si<<16)+reg_di;
						if (mscdex->ReadSectors(reg_cx,sector,reg_dx,data)) {
							CALLBACK_SCF(false);
						} else {
							reg_al = MSCDEX_ERROR_UNKNOWN_DRIVE;
							CALLBACK_SCF(true);
						};
						return true;
					 };
		case 0x1509:	// write sectors - not supported 
						reg_al = MSCDEX_ERROR_DRIVE_NOT_READY;
						CALLBACK_SCF(true);
						return true;
		case 0x150B:	/* Valid CDROM drive ? */
						reg_ax = mscdex->IsValidDrive(reg_cx);
						reg_bx = 0xADAD;
						return true;
		case 0x150C:	/* Get MSCDEX Version */
						reg_bx = mscdex->GetVersion();
						return true;
		case 0x150D:	/* Get drives */
						mscdex->GetDrives(data);
						return true;
		case 0x150F: {	// Get directory entry
						Bitu error;
						bool success = mscdex->GetDirectoryEntry(reg_cl,reg_ch&1,data,PhysMake(reg_si,reg_di),error);
						reg_ax = error;
						CALLBACK_SCF(!success);
					 };	return true;
		case 0x1510:	/* Device driver request */
						mscdex->SendDriverRequest(reg_cx,data);
						return true;
 
		default	:		LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Unknwon call : %04X",reg_ax);
						return true;

	};
	return false;
};

class device_MSCDEX : public DOS_Device {
public:
	device_MSCDEX() { SetName("MSCD001"); }
	bool Read (Bit8u * data,Bit16u * size) { return false;}
	bool Write(Bit8u * data,Bit16u * size) { 
		LOG(LOG_ALL,LOG_NORMAL)("Write to mscdex device");	
		return false;
	}
	bool Seek(Bit32u * pos,Bit32u type){return false;}
	bool Close(){return false;}
	Bit16u GetInformation(void){return 0x8093;}
private:
	Bit8u cache;
};

int MSCDEX_AddDrive(char driveLetter, const char* physicalPath, Bit8u& subUnit)
{
	int result = mscdex->AddDrive(driveLetter-'A',(char*)physicalPath,subUnit);
	return result;
};

bool MSCDEX_GetVolumeName(Bit8u subUnit, char* name)
{
	return mscdex->GetVolumeName(subUnit,name);
};

bool MSCDEX_HasMediaChanged(Bit8u subUnit)
{
	static TMSF leadOut[MSCDEX_MAX_DRIVES];

	TMSF leadnew;
	Bit8u tr1,tr2;
	if (mscdex->GetCDInfo(subUnit,tr1,tr2,leadnew)) {
		bool changed = (leadOut[subUnit].min!=leadnew.min) || (leadOut[subUnit].sec!=leadnew.sec) || (leadOut[subUnit].fr!=leadnew.fr);
		if (changed) {
			leadOut[subUnit].min = leadnew.min;
			leadOut[subUnit].sec = leadnew.sec;
			leadOut[subUnit].fr	 = leadnew.fr;
			mscdex->InitNewMedia(subUnit);
		}
		return changed;
	};
	if (subUnit<MSCDEX_MAX_DRIVES) {
		leadOut[subUnit].min = 0;
		leadOut[subUnit].sec = 0;
		leadOut[subUnit].fr	 = 0;
	}
	return true;
};

void MSCDEX_SetCDInterface(int intNr, int numCD)
{
	useCdromInterface = intNr;
	forceCD	= numCD;
};

void MSCDEX_ShutDown(Section* sec)
{
	delete mscdex; mscdex = 0;
};

void MSCDEX_Init(Section* sec) 
{
	// AddDestroy func
	sec->AddDestroyFunction(&MSCDEX_ShutDown);
	/* Register the mscdex device */
	DOS_Device * newdev = new device_MSCDEX();
	DOS_AddDevice(newdev);
	/* Add Multiplexer */
	DOS_AddMultiplexHandler(MSCDEX_Handler);
	/* Create MSCDEX */
	mscdex = new CMscdex;
};
