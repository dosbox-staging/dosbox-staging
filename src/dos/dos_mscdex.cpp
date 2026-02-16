// SPDX-FileCopyrightText:  2019-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos_mscdex.h"

#include <cassert>
#include <cctype>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "cdrom.h"
#include "config/setup.h"
#include "cpu/callback.h"
#include "cpu/cpu.h"
#include "cpu/registers.h"
#include "dos.h"
#include "dos/dos_system.h"
#include "ints/bios_disk.h"
#include "misc/compiler.h"
#include "misc/support.h"
#include "utils/fs_utils.h"
#include "utils/math_utils.h"
#include "utils/string_utils.h"

#define MSCDEX_LOG LOG(LOG_MISC,LOG_ERROR)
//#define MSCDEX_LOG

#define MSCDEX_VERSION_HIGH	2
#define MSCDEX_VERSION_LOW	23
#define MSCDEX_MAX_DRIVES	8

// Error Codes
#define MSCDEX_ERROR_INVALID_FUNCTION	1
#define MSCDEX_ERROR_BAD_FORMAT			11
#define MSCDEX_ERROR_UNKNOWN_DRIVE		15
#define MSCDEX_ERROR_DRIVE_NOT_READY	21

// Request Status
#define	REQUEST_STATUS_DONE		0x0100
#define	REQUEST_STATUS_ERROR	0x8000

static Bitu MSCDEX_Strategy_Handler(void); 
static Bitu MSCDEX_Interrupt_Handler(void);

class DOS_DeviceHeader final : public MemStruct {
public:
	DOS_DeviceHeader(PhysPt ptr) { pt = ptr; }

	void SetNextDeviceHeader(RealPt ptr)
	{
		SSET_DWORD(sDeviceHeader, nextDeviceHeader, ptr);
	}

	RealPt GetNextDeviceHeader() const
	{
		return SGET_DWORD(sDeviceHeader, nextDeviceHeader);
	}

	void SetAttribute(uint16_t atr)
	{
		SSET_WORD(sDeviceHeader, devAttributes, atr);
	}

	void SetDriveLetter(uint8_t letter)
	{
		SSET_BYTE(sDeviceHeader, driveLetter, letter);
	}

	void SetNumSubUnits(uint8_t num)
	{
		SSET_BYTE(sDeviceHeader, numSubUnits, num);
	}

	uint8_t GetNumSubUnits() const
	{
		return SGET_BYTE(sDeviceHeader, numSubUnits);
	}

	void SetName(const char *new_name)
	{
		MEM_BlockWrite(pt + offsetof(sDeviceHeader, name), new_name, 8);
	}

	void SetInterrupt(uint16_t ofs)
	{
		SSET_WORD(sDeviceHeader, interrupt, ofs);
	}

	void SetStrategy(uint16_t offset)
	{
		SSET_WORD(sDeviceHeader, strategy, offset);
	}

public:
#ifdef _MSC_VER
	#pragma pack(1)
	#endif
	struct sDeviceHeader {
		RealPt	nextDeviceHeader;
		uint16_t	devAttributes;
		uint16_t	strategy;
		uint16_t	interrupt;
		uint8_t	name[8];
		uint16_t  wReserved;
		uint8_t	driveLetter;
		uint8_t	numSubUnits;
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack()
	#endif
};

class CMscdex {
public:
	CMscdex  ();
	~CMscdex () = default;

	CMscdex            (const CMscdex&) = delete; // prevent copying
	CMscdex& operator= (const CMscdex&) = delete; // prevent assignment

	uint16_t GetVersion() const
	{
		return (MSCDEX_VERSION_HIGH << 8) + MSCDEX_VERSION_LOW;
	}

	uint16_t GetNumDrives() const { return numDrives; }
	uint16_t GetFirstDrive() const { return dinfo[0].drive; }

	uint8_t		GetSubUnit		(uint16_t _drive);
	bool		GetUPC			(uint8_t subUnit, uint8_t& attr, std::string& upc);

	void		InitNewMedia		(uint8_t subUnit);
	bool		PlayAudioSector		(uint8_t subUnit, uint32_t start, uint32_t length);
	bool		PlayAudioMSF		(uint8_t subUnit, uint32_t start, uint32_t length);
	bool		StopAudio		(uint8_t subUnit);
	bool		GetAudioStatus		(uint8_t subUnit, bool& playing, bool& pause, TMSF& start, TMSF& end);

	bool		GetSubChannelData	(uint8_t subUnit, uint8_t& attr, uint8_t& track, uint8_t &index, TMSF& rel, TMSF& abs);

	int		RemoveDrive		(uint16_t _drive);
	int		AddDrive		(uint16_t _drive, const char* physicalPath, uint8_t& subUnit);
	bool 		HasDrive		(uint16_t drive);
	void		ReplaceDrive		(std::unique_ptr<CDROM_Interface> newCdrom, uint8_t subUnit);
	void		GetDrives		(PhysPt data);
	void		GetDriverInfo		(PhysPt data);
	bool		GetVolumeName		(uint8_t subUnit, char* name);
	bool		GetFileName		(uint16_t drive, uint16_t pos, PhysPt data);	
	bool		GetDirectoryEntry	(uint16_t drive, bool copyFlag, PhysPt pathname, PhysPt buffer, uint16_t& error);
	bool		ReadVTOC		(uint16_t drive, uint16_t volume, PhysPt data, uint16_t& offset, uint16_t& error);
	bool		ReadSectors		(uint16_t drive, uint32_t sector, uint16_t num, PhysPt data);
	bool		ReadSectors		(uint8_t subUnit, bool raw, uint32_t sector, uint16_t num, PhysPt data);
	bool		ReadSectorsMSF		(uint8_t subUnit, bool raw, uint32_t sector, uint16_t num, PhysPt data);
	bool		SendDriverRequest	(uint16_t drive, PhysPt data);
	bool		IsValidDrive		(uint16_t drive);
	bool		GetCDInfo		(uint8_t subUnit, uint8_t& tr1, uint8_t& tr2, TMSF& leadOut);
	uint32_t	GetVolumeSize		(uint8_t subUnit);
	bool		GetTrackInfo		(uint8_t subUnit, uint8_t track, uint8_t& attr, TMSF& start);
	uint16_t	GetStatusWord		(uint8_t subUnit,uint16_t status);
	bool		GetCurrentPos		(uint8_t subUnit, TMSF& pos);
	uint32_t	GetDeviceStatus		(uint8_t subUnit);
	bool		GetMediaStatus		(uint8_t subUnit, uint8_t& status);
	bool		LoadUnloadMedia		(uint8_t subUnit, bool unload);
	bool		ResumeAudio		(uint8_t subUnit);
	bool		GetMediaStatus		(uint8_t subUnit, bool& media, bool& changed, bool& trayOpen);
	bool		Seek		(uint8_t subUnit, uint32_t sector);

private:

	PhysPt		GetDefaultBuffer	(void);
	PhysPt		GetTempBuffer		(void);

	uint16_t		numDrives;

	typedef struct SDriveInfo {
		uint8_t	drive;			// drive letter in dosbox
		uint8_t	physDrive;		// drive letter in system
		bool	audioPlay;		// audio playing active
		bool	audioPaused;	// audio playing paused
		uint32_t	audioStart;		// StartLoc for resume
		uint32_t	audioEnd;		// EndLoc for resume
		bool	locked;			// drive locked ?
		bool	lastResult;		// last operation success ?
		uint32_t	volumeSize;		// for media change
		TCtrl	audioCtrl;		// audio channel control
	} TDriveInfo;

	uint16_t            defaultBufSeg;
	
public:
	uint16_t rootDriverHeaderSeg;
	TDriveInfo        dinfo[MSCDEX_MAX_DRIVES];

	bool ChannelControl(uint8_t subUnit, TCtrl ctrl);
	bool GetChannelControl(uint8_t subUnit, TCtrl &ctrl);
};

CMscdex::CMscdex()
	: numDrives(0),
	  defaultBufSeg(0),
	  rootDriverHeaderSeg(0)
{
	memset(dinfo, 0, sizeof(dinfo));
}

void CMscdex::GetDrives(PhysPt data)
{
	for (uint16_t i=0; i<GetNumDrives(); i++) mem_writeb(data+i,dinfo[i].drive);
}

bool CMscdex::IsValidDrive(uint16_t _drive)
{
	_drive &= 0xff; //Only lowerpart (Ultimate domain)
	for (uint16_t i=0; i<GetNumDrives(); i++) if (dinfo[i].drive==_drive) return true;
	return false;
}

uint8_t CMscdex::GetSubUnit(uint16_t _drive)
{
	_drive &= 0xff; //Only lowerpart (Ultimate domain)
	for (uint16_t i=0; i<GetNumDrives(); i++) if (dinfo[i].drive==_drive) return (uint8_t)i;
	return 0xff;
}

int CMscdex::RemoveDrive(uint16_t _drive)
{
	uint16_t idx = MSCDEX_MAX_DRIVES;
	for (uint16_t i=0; i<GetNumDrives(); i++) {
		if (dinfo[i].drive == _drive) {
			idx = i;
			break;
		}
	}

	if (idx == MSCDEX_MAX_DRIVES || (idx!=0 && idx!=GetNumDrives()-1)) return 0;
	CDROM::cdroms[idx].reset();
	if (idx==0) {
		for (uint16_t i=0; i<GetNumDrives(); i++) {
			if (i == MSCDEX_MAX_DRIVES-1) {
				CDROM::cdroms[i].reset();
				memset(&dinfo[i],0,sizeof(TDriveInfo));
			} else {
				dinfo[i] = dinfo[i+1];
				CDROM::cdroms[i] = std::move(CDROM::cdroms[i + 1]);
			}
		}
	} else {
		memset(&dinfo[idx],0,sizeof(TDriveInfo));
	}
	numDrives--;

	if (GetNumDrives() == 0) {
		DOS_DeviceHeader devHeader(PhysicalMake(rootDriverHeaderSeg,0));
		uint16_t off = sizeof(DOS_DeviceHeader::sDeviceHeader);
		devHeader.SetStrategy(off+4);		// point to the RETF (To deactivate MSCDEX)
		devHeader.SetInterrupt(off+4);		// point to the RETF (To deactivate MSCDEX)
		devHeader.SetDriveLetter(0);
	} else if (idx==0) {
		DOS_DeviceHeader devHeader(PhysicalMake(rootDriverHeaderSeg,0));
		devHeader.SetDriveLetter(GetFirstDrive()+1);
	}
	return 1;
}

static std::unique_ptr<CDROM_Interface> create_cdrom_interface(const char *path)
{
	if (!path_exists(path)) {
		return {};
	}

	if (!is_directory(path)) {
		if (auto cdrom_interface = std::make_unique<CDROM_Interface_Image>();
		    cdrom_interface->SetDevice(path)) {
			return cdrom_interface;
		} else {
			return {};
		}
	}

#if defined(LINUX)
	if (auto cdrom_interface = std::make_unique<CDROM_Interface_Ioctl>();
	    cdrom_interface->SetDevice(path)) {
		return cdrom_interface;
	}
#elif defined(WIN32)
	if (auto cdrom_interface = std::make_unique<CDROM_Interface_Win32>();
	    cdrom_interface->SetDevice(path)) {
		return cdrom_interface;
	}
#endif
			
	if (auto cdrom_interface = std::make_unique<CDROM_Interface_Fake>();
	    cdrom_interface->SetDevice(path)) {
		return cdrom_interface;
	}
	return {};
}

int CMscdex::AddDrive(uint16_t _drive, const char* physicalPath, uint8_t& subUnit)
{
	subUnit = 0;
	if ((Bitu)GetNumDrives()+1>=MSCDEX_MAX_DRIVES) return 4;
	if (GetNumDrives()) {
		// Error check, driveletter have to be in a row
		if (dinfo[0].drive-1!=_drive && dinfo[numDrives-1].drive+1!=_drive) 
			return 1;
	}
	// Set return type to ok
	int result = 0;
	auto cdrom = create_cdrom_interface(physicalPath);
	if (!cdrom) {
		return 3;
	}
	if (!cdrom->HasFullMscdexSupport()) {
		result = 5;
	}

	if (rootDriverHeaderSeg==0) {
		
		constexpr uint16_t driverSize = sizeof(DOS_DeviceHeader::sDeviceHeader) + 10; // 10 = Bytes for 3 callbacks
		
		// Create Device Header
		static_assert((driverSize % 16) == 0, "should always be zero");
		uint16_t seg = DOS_GetMemory(driverSize / 16);
		DOS_DeviceHeader devHeader(PhysicalMake(seg,0));
		devHeader.SetNextDeviceHeader	(0xFFFFFFFF);
		devHeader.SetAttribute(0xc800);
		devHeader.SetDriveLetter		(_drive+1);
		devHeader.SetNumSubUnits		(1);
		devHeader.SetName				("MSCD001 ");

		//Link it in the device chain
		DOS_AppendDevice(seg);

		// Create Callback Strategy
		uint16_t off = sizeof(DOS_DeviceHeader::sDeviceHeader);
		auto call_strategy = static_cast<uint16_t>(CALLBACK_Allocate());
		Callback_Handlers[call_strategy]=MSCDEX_Strategy_Handler;
		real_writeb(seg,off+0,(uint8_t)0xFE);		//GRP 4
		real_writeb(seg,off+1,(uint8_t)0x38);		//Extra Callback instruction
		real_writew(seg,off+2,call_strategy);	//The immediate word
		real_writeb(seg,off+4,(uint8_t)0xCB);		//A RETF Instruction
		devHeader.SetStrategy(off);
		
		// Create Callback Interrupt
		off += 5;
		auto call_interrupt = static_cast<uint16_t>(CALLBACK_Allocate());
		Callback_Handlers[call_interrupt]=MSCDEX_Interrupt_Handler;
		real_writeb(seg,off+0,(uint8_t)0xFE);		//GRP 4
		real_writeb(seg,off+1,(uint8_t)0x38);		//Extra Callback instruction
		real_writew(seg,off+2,call_interrupt);	//The immediate word
		real_writeb(seg,off+4,(uint8_t)0xCB);		//A RETF Instruction
		devHeader.SetInterrupt(off);
		
		rootDriverHeaderSeg = seg;
	
	} else if (GetNumDrives() == 0) {
		DOS_DeviceHeader devHeader(PhysicalMake(rootDriverHeaderSeg,0));
		uint16_t off = sizeof(DOS_DeviceHeader::sDeviceHeader);
		devHeader.SetDriveLetter(_drive+1);
		devHeader.SetStrategy(off);
		devHeader.SetInterrupt(off+5);
	}

	// Set drive
	DOS_DeviceHeader devHeader(PhysicalMake(rootDriverHeaderSeg,0));
	devHeader.SetNumSubUnits(devHeader.GetNumSubUnits()+1);

	if (dinfo[0].drive - 1 == _drive) {
		// Perform a push_front on dinfo and CDROM::cdroms
		for (auto i = GetNumDrives(); i > 0; --i) {
			// Shift all data one step to the right to make room
			dinfo[i]         = dinfo[i - 1];
			CDROM::cdroms[i] = std::move(CDROM::cdroms[i - 1]);
		}
		CDROM::cdroms[0]   = std::move(cdrom);
		dinfo[0].drive     = (uint8_t)_drive;
		dinfo[0].physDrive = (uint8_t)toupper(physicalPath[0]);
		subUnit            = 0;
	} else {
		// Perform a push_back on dinfo and CDROM::cdroms
		CDROM::cdroms[numDrives]   = std::move(cdrom); 
		dinfo[numDrives].drive     = (uint8_t)_drive;
		dinfo[numDrives].physDrive = (uint8_t)toupper(physicalPath[0]);
		subUnit                    = (uint8_t)numDrives;
	}
	numDrives++;
	// init channel control
	for (uint8_t chan=0;chan<4;chan++) {
		dinfo[subUnit].audioCtrl.out[chan]=chan;
		dinfo[subUnit].audioCtrl.vol[chan]=0xff;
	}
	// stop audio
	StopAudio(subUnit);
	return result;
}

bool CMscdex::HasDrive(uint16_t drive) {
	return (GetSubUnit(drive) != 0xff);
}

void CMscdex::ReplaceDrive(std::unique_ptr<CDROM_Interface> newCdrom, uint8_t subUnit) {
	if (CDROM::cdroms[subUnit] != nullptr) {
		StopAudio(subUnit);
	}
	CDROM::cdroms[subUnit] = std::move(newCdrom);
}

PhysPt CMscdex::GetDefaultBuffer(void) {
	if (defaultBufSeg==0) {
		uint16_t size = (2352*2+15)/16;
		defaultBufSeg = DOS_GetMemory(size);
	};
	return PhysicalMake(defaultBufSeg,2352);
}

PhysPt CMscdex::GetTempBuffer(void) {
	if (defaultBufSeg==0) {
		uint16_t size = (2352*2+15)/16;
		defaultBufSeg = DOS_GetMemory(size);
	};
	return PhysicalMake(defaultBufSeg,0);
}

void CMscdex::GetDriverInfo	(PhysPt data) {
	for (uint16_t i=0; i<GetNumDrives(); i++) {
		mem_writeb(data  ,(uint8_t)i);	// subunit
		mem_writed(data+1,RealMake(rootDriverHeaderSeg,0));
		data+=5;
	};
}

bool CMscdex::GetCDInfo(uint8_t subUnit, uint8_t& tr1, uint8_t& tr2, TMSF& leadOut) {
	if (subUnit>=numDrives) return false;
	// Assume Media change
	CDROM::cdroms[subUnit]->InitNewMedia();
	dinfo[subUnit].lastResult = CDROM::cdroms[subUnit]->GetAudioTracks(tr1,
	                                                                   tr2,
	                                                                   leadOut);
	return dinfo[subUnit].lastResult;
}

bool CMscdex::GetTrackInfo(uint8_t subUnit, uint8_t track, uint8_t& attr, TMSF& start) {
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult =
	        CDROM::cdroms[subUnit]->GetAudioTrackInfo(track, start, attr);	
	if (!dinfo[subUnit].lastResult) {
		attr = 0;
		memset(&start,0,sizeof(start));
	}
	return dinfo[subUnit].lastResult;
}

bool CMscdex::PlayAudioSector(uint8_t subUnit, uint32_t sector, uint32_t length) {
	if (subUnit>=numDrives) return false;
	// If value from last stop is used, this is meant as a resume
	// better start using resume command
	if (dinfo[subUnit].audioPaused && (sector==dinfo[subUnit].audioStart) && (dinfo[subUnit].audioEnd!=0)) {
		dinfo[subUnit].lastResult = CDROM::cdroms[subUnit]->PauseAudio(true);
	} else 
		dinfo[subUnit].lastResult =
		        CDROM::cdroms[subUnit]->PlayAudioSector(sector, length);

	if (dinfo[subUnit].lastResult) {
		dinfo[subUnit].audioPlay	= true;
		dinfo[subUnit].audioPaused	= false;
		dinfo[subUnit].audioStart	= sector;
		dinfo[subUnit].audioEnd		= length;
	}
	return dinfo[subUnit].lastResult;
}

bool CMscdex::PlayAudioMSF(uint8_t subUnit, uint32_t start, uint32_t length) {
	if (subUnit>=numDrives) return false;
	uint8_t min		= (uint8_t)(start>>16) & 0xFF;
	uint8_t sec		= (uint8_t)(start>> 8) & 0xFF;
	uint8_t fr		= (uint8_t)(start>> 0) & 0xFF;
	uint32_t sector	= min * 60 * REDBOOK_FRAMES_PER_SECOND
	                  + sec * REDBOOK_FRAMES_PER_SECOND
	                  + fr
	                  - REDBOOK_FRAME_PADDING;
	return dinfo[subUnit].lastResult = PlayAudioSector(subUnit,sector,length);
}

bool CMscdex::GetSubChannelData(uint8_t subUnit, uint8_t& attr, uint8_t& track, uint8_t &index, TMSF& rel, TMSF& abs) {
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult =
	        CDROM::cdroms[subUnit]->GetAudioSub(attr, track, index, rel, abs);
	if (!dinfo[subUnit].lastResult) {
		attr = track = index = 0;
		memset(&rel,0,sizeof(rel));
		memset(&abs,0,sizeof(abs));
	}
	return dinfo[subUnit].lastResult;
}

bool CMscdex::GetAudioStatus(uint8_t subUnit, bool& playing, bool& pause, TMSF& start, TMSF& end) {
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = CDROM::cdroms[subUnit]->GetAudioStatus(playing,
	                                                                   pause);
	if (dinfo[subUnit].lastResult) {
		if (playing) {
			// Start
			uint32_t addr = dinfo[subUnit].audioStart + REDBOOK_FRAME_PADDING;
			start.fr    = (uint8_t)(addr % REDBOOK_FRAMES_PER_SECOND);
			addr       /= REDBOOK_FRAMES_PER_SECOND;
			start.sec   = (uint8_t)(addr % 60);
			start.min   = (uint8_t)(addr / 60);
			// End
			addr        = dinfo[subUnit].audioEnd + REDBOOK_FRAME_PADDING;
			end.fr      = (uint8_t)(addr % REDBOOK_FRAMES_PER_SECOND);
			addr       /= REDBOOK_FRAMES_PER_SECOND;
			end.sec     = (uint8_t)(addr % 60);
			end.min     = (uint8_t)(addr / 60);
		} else {
			memset(&start,0,sizeof(start));
			memset(&end,0,sizeof(end));
		}
	} else {
		playing		= false;
		pause		= false;
		memset(&start,0,sizeof(start));
		memset(&end,0,sizeof(end));
	}
	
	return dinfo[subUnit].lastResult;
}

bool CMscdex::StopAudio(uint8_t subUnit) {
	if (subUnit>=numDrives) return false;
	if (dinfo[subUnit].audioPlay) {
		// Check if audio is still playing....
		TMSF start,end;
		bool playing,pause;
		if (GetAudioStatus(subUnit,playing,pause,start,end))
			dinfo[subUnit].audioPlay = playing;
		else
			dinfo[subUnit].audioPlay = false;
	}
	if (dinfo[subUnit].audioPlay) {
		dinfo[subUnit].lastResult = CDROM::cdroms[subUnit]->PauseAudio(false);
	} else {
		dinfo[subUnit].lastResult = CDROM::cdroms[subUnit]->StopAudio();
	}
	
	if (dinfo[subUnit].lastResult) {
		if (dinfo[subUnit].audioPlay) {
			TMSF pos;
			GetCurrentPos(subUnit,pos);
			dinfo[subUnit].audioStart = pos.min * 60 * REDBOOK_FRAMES_PER_SECOND
			                            + pos.sec * REDBOOK_FRAMES_PER_SECOND
			                            + pos.fr
			                            - REDBOOK_FRAME_PADDING;
			dinfo[subUnit].audioPaused  = true;
		} else {	
			dinfo[subUnit].audioPaused  = false;
			dinfo[subUnit].audioStart   = 0;
			dinfo[subUnit].audioEnd     = 0;
		}
		dinfo[subUnit].audioPlay = false;
	}
	return dinfo[subUnit].lastResult;
}

bool CMscdex::ResumeAudio(uint8_t subUnit) {
	if (subUnit>=numDrives) return false;
	return dinfo[subUnit].lastResult = PlayAudioSector(subUnit,dinfo[subUnit].audioStart,dinfo[subUnit].audioEnd);
}

uint32_t CMscdex::GetVolumeSize(uint8_t subUnit) {
	if (subUnit>=numDrives) return false;
	uint8_t tr1,tr2; // <== place-holders (use lead-out for size calculation)
	TMSF leadOut;
	dinfo[subUnit].lastResult = GetCDInfo(subUnit,tr1,tr2,leadOut);
	if (dinfo[subUnit].lastResult)
		return leadOut.min * 60 * REDBOOK_FRAMES_PER_SECOND
		       + leadOut.sec * REDBOOK_FRAMES_PER_SECOND
		       + leadOut.fr;
	return 0;
}

bool CMscdex::ReadVTOC(uint16_t drive, uint16_t volume, PhysPt data, uint16_t& offset, uint16_t& error) {
	uint8_t subunit = GetSubUnit(drive);
/*	if (subunit>=numDrives) {
		error=MSCDEX_ERROR_UNKNOWN_DRIVE;
		return false;
	} */
	if (!ReadSectors(subunit,false,16+volume,1,data)) {
		error=MSCDEX_ERROR_DRIVE_NOT_READY;
		return false;
	}
	char id[5];
	MEM_BlockRead(data + 1, id, 5);
	if (strncmp("CD001", id, 5)==0) offset = 0;
	else {
		MEM_BlockRead(data + 9, id, 5);
		if (strncmp("CDROM", id, 5)==0) offset = 8;
		else {
			error = MSCDEX_ERROR_BAD_FORMAT;
			return false;
		}
	}
	uint8_t type = mem_readb(data + offset);
	error = (type == 1) ? 1 : (type == 0xFF) ? 0xFF : 0;
	return true;
}

bool CMscdex::GetVolumeName(uint8_t subUnit, char* data) {	
	if (subUnit>=numDrives) return false;
	uint16_t drive = dinfo[subUnit].drive;

	uint16_t offset = 0, error;
	bool success = false;
	PhysPt ptoc = GetTempBuffer();
	success = ReadVTOC(drive,0x00,ptoc,offset,error);
	if (success) {
		MEM_StrCopy(ptoc+offset+40,data,31);
		data[31] = 0;
		rtrim(data);
	};

	return success; 
}

bool CMscdex::GetFileName(uint16_t drive, uint16_t pos, PhysPt data) {
	uint16_t offset = 0, error;
	bool success = false;
	PhysPt ptoc = GetTempBuffer();
	success = ReadVTOC(drive,0x00,ptoc,offset,error);
	if (success) {
		Bitu len;
		for (len=0;len<37;len++) {
			uint8_t c=mem_readb(ptoc+offset+pos+len);
			if (c==0 || c==0x20) break;
		}
		MEM_BlockCopy(data,ptoc+offset+pos,len);
		mem_writeb(data+len,0);
	};
	return success; 
}

bool CMscdex::GetUPC(const uint8_t subUnit, uint8_t& attr, std::string& upc)
{
	if (subUnit>=numDrives) return false;
	return dinfo[subUnit].lastResult = CDROM::cdroms[subUnit]->GetUPC(attr, upc);
}

bool CMscdex::ReadSectors(uint8_t subUnit, bool raw, uint32_t sector, uint16_t num, PhysPt data) {
	if (subUnit>=numDrives) return false;
	if ((4*num*2048+5) < CPU_Cycles) CPU_Cycles -= 4*num*2048;
	else CPU_Cycles = 5;
	dinfo[subUnit].lastResult =
	        CDROM::cdroms[subUnit]->ReadSectors(data, raw, sector, num);
	return dinfo[subUnit].lastResult;
}

bool CMscdex::ReadSectorsMSF(uint8_t subUnit, bool raw, uint32_t start, uint16_t num, PhysPt data) {
	if (subUnit>=numDrives) return false;
	uint8_t min		= (uint8_t)(start>>16) & 0xFF;
	uint8_t sec		= (uint8_t)(start>> 8) & 0xFF;
	uint8_t fr		= (uint8_t)(start>> 0) & 0xFF;
	uint32_t sector	= min * 60 * REDBOOK_FRAMES_PER_SECOND
	                  + sec * REDBOOK_FRAMES_PER_SECOND
	                  + fr
	                  - REDBOOK_FRAME_PADDING;
	return ReadSectors(subUnit,raw,sector,num,data);
}

// Called from INT 2F
bool CMscdex::ReadSectors(uint16_t drive, uint32_t sector, uint16_t num, PhysPt data) {
	return ReadSectors(GetSubUnit(drive),false,sector,num,data);
}

bool CMscdex::GetDirectoryEntry(uint16_t drive, bool copyFlag, PhysPt pathname, PhysPt buffer, uint16_t& error) {
	char	volumeID[6] = {0};
	char	searchName[256];
	char	entryName[256];
	bool	foundComplete = false;
	bool	foundName;
	bool	nextPart = true;
	char*	useName = nullptr;
	Bitu	entryLength,nameLength;
	// clear error
	error = 0;
	MEM_StrCopy(pathname+1,searchName,mem_readb(pathname));
	upcase(searchName);
	char* searchPos = searchName;

	//strip of tailing . (XCOM APOCALYPSE)
	auto searchlen = safe_strlen(searchName);
	if (searchlen > 1 && strcmp(searchName,".."))
		if (searchName[searchlen-1] =='.')  searchName[searchlen-1] = 0;

	//LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Get DirEntry : Find : %s",searchName);
	// read vtoc
	PhysPt defBuffer = GetDefaultBuffer();
	if (!ReadSectors(GetSubUnit(drive),false,16,1,defBuffer)) return false;
	MEM_StrCopy(defBuffer+1,volumeID,5); volumeID[5] = 0;
	bool iso = (strcmp("CD001",volumeID)==0);
	if (!iso) {
		MEM_StrCopy(defBuffer+9,volumeID,5);
		if (strcmp("CDROM",volumeID)!=0) E_Exit("MSCDEX: GetDirEntry: Not an ISO 9660 or HSF CD.");
	}
	uint16_t offset = iso ? 156:180;
	// get directory position
	Bitu dirEntrySector	= mem_readd(defBuffer+offset+2);
	Bits dirSize		= mem_readd(defBuffer+offset+10);
	Bitu index;
	while (dirSize>0) {
		index = 0;
		if (!ReadSectors(GetSubUnit(drive),false,dirEntrySector,1,defBuffer)) return false;
		// Get string part
		foundName	= false;
		if (nextPart) {
			if (searchPos) { 
				useName = searchPos; 
				searchPos = strchr(searchPos,'\\'); 
			}
			if (searchPos) { *searchPos = 0; searchPos++; }
			else foundComplete = true;
		}

		do {
			entryLength = mem_readb(defBuffer+index);
			if (entryLength==0) break;
			if (mem_readb(defBuffer + index + (iso?0x19:0x18) ) & 4) {
				// skip associated files
				index += entryLength;
				continue;
			}
			nameLength  = mem_readb(defBuffer+index+32);
			MEM_StrCopy(defBuffer+index+33,entryName,nameLength);
			// strip separator and file version number
			char* separator = strchr(entryName,';');
			if (separator) *separator = 0;
			// strip trailing period
			size_t entrylen = safe_strlen(entryName);
			if (entrylen>0 && entryName[entrylen-1]=='.') entryName[entrylen-1] = 0;

			if (strcmp(entryName,useName)==0) {
				//LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Get DirEntry : Found : %s",useName);
				foundName = true;
				break;
			}
			index += entryLength;
		} while (index+33<=2048);
		
		if (foundName) {
			if (foundComplete) {
				if (copyFlag) {
					LOG(LOG_MISC,LOG_WARN)("MSCDEX: GetDirEntry: Copyflag structure not entirely accurate maybe");
					uint8_t readBuf[256];
					uint8_t writeBuf[256];
					assertm(entryLength <= 256, "entryLength should never exceed 256");
					MEM_BlockRead( defBuffer+index, readBuf, entryLength );
					writeBuf[0] = readBuf[1];						// 00h	BYTE	length of XAR in Logical Block Numbers
					memcpy( &writeBuf[1], &readBuf[0x2], 4);		// 01h	DWORD	Logical Block Number of file start
					writeBuf[5] = 0;writeBuf[6] = 8;				// 05h	WORD	size of disk in logical blocks
					memcpy( &writeBuf[7], &readBuf[0xa], 4);		// 07h	DWORD	file length in bytes
					memcpy( &writeBuf[0xb], &readBuf[0x12], 6);		// 0bh	BYTEs	date and time
					writeBuf[0x11] = iso ? readBuf[0x18]:0;			// 11h	BYTE	time zone
					writeBuf[0x12] = readBuf[iso ? 0x19:0x18];		// 12h	BYTE	bit flags
					writeBuf[0x13] = readBuf[0x1a];					// 13h	BYTE	interleave size
					writeBuf[0x14] = readBuf[0x1b];					// 14h	BYTE	interleave skip factor
					memcpy( &writeBuf[0x15], &readBuf[0x1c], 2);	// 15h	WORD	volume set sequence number
					writeBuf[0x17] = readBuf[0x20];
					memcpy( &writeBuf[0x18], &readBuf[21], readBuf[0x20] <= 38 ? readBuf[0x20] : 38 );
					MEM_BlockWrite( buffer, writeBuf, 0x18 + 40 );
				} else {
					// Direct copy
					MEM_BlockCopy(buffer,defBuffer+index,entryLength);
				}
				error = iso ? 1:0;
				return true;
			}
			// change directory
			dirEntrySector = mem_readd(defBuffer+index+2);
			dirSize	= mem_readd(defBuffer+index+10);
			nextPart = true;
		} else {
			// continue search in next sector
			dirSize -= 2048;
			dirEntrySector++;
			nextPart = false;
		}
	}
	error = 2; // file not found
	return false; // not found
}

bool CMscdex::GetCurrentPos(uint8_t subUnit, TMSF& pos) {
	if (subUnit>=numDrives) return false;
	if (!dinfo[subUnit].audioPlay) {
		pos = frames_to_msf(dinfo[subUnit].audioStart + REDBOOK_FRAME_PADDING);
		return true;
	}
	TMSF rel;
	uint8_t attr,track,index;
	dinfo[subUnit].lastResult = GetSubChannelData(subUnit, attr, track, index, rel, pos);
	if (!dinfo[subUnit].lastResult) memset(&pos,0,sizeof(pos));
	return dinfo[subUnit].lastResult;
}

bool CMscdex::GetMediaStatus(uint8_t subUnit, bool& media, bool& changed, bool& trayOpen) {
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = CDROM::cdroms[subUnit]->GetMediaTrayStatus(
		media, changed, trayOpen);
	return dinfo[subUnit].lastResult;
}

uint32_t CMscdex::GetDeviceStatus(uint8_t subUnit) {
	if (subUnit>=numDrives) return false;
	bool media,changed,trayOpen;

	dinfo[subUnit].lastResult = GetMediaStatus(subUnit,media,changed,trayOpen);
	if (dinfo[subUnit].audioPlay) {
		// Check if audio is still playing....
		TMSF start,end;
		bool playing,pause;
		if (GetAudioStatus(subUnit,playing,pause,start,end))
			dinfo[subUnit].audioPlay = playing;
		else
			dinfo[subUnit].audioPlay = false;
	}

	uint32_t status = ((trayOpen?1:0) << 0)					|	// Drive is open ?
					((dinfo[subUnit].locked?1:0) << 1)		|	// Drive is locked ?
					(1<<2)									|	// raw + cooked sectors
					(1<<4)									|	// Can read sudio
					(1<<8)									|	// Can control audio
					(1<<9)									|	// Red book & HSG
					((dinfo[subUnit].audioPlay?1:0) << 10)	|	// Audio is playing ?
					((media?0:1) << 11);						// Drive is empty ?
	return status;
}

bool CMscdex::GetMediaStatus(uint8_t subUnit, uint8_t& status) {
	if (subUnit>=numDrives) return false;
/*	bool media,changed,open,result;
	result = GetMediaStatus(subUnit,media,changed,open);
	status = changed ? 0xFF : 0x01;
	return result; */
	status = getSwapRequest() ? 0xFF : 0x01;
	return true;
}

bool CMscdex::LoadUnloadMedia(uint8_t subUnit, bool unload) {
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = CDROM::cdroms[subUnit]->LoadUnloadMedia(unload);
	return dinfo[subUnit].lastResult;
}

bool CMscdex::SendDriverRequest(uint16_t drive, PhysPt data) {
	uint8_t subUnit = GetSubUnit(drive);
	if (subUnit>=numDrives) return false;
	// Get SubUnit
	mem_writeb(data+1,subUnit);
	// Call Strategy / Interrupt
	MSCDEX_Strategy_Handler();
	MSCDEX_Interrupt_Handler();
	return true;
}

uint16_t CMscdex::GetStatusWord(uint8_t subUnit,uint16_t status) {
	if (subUnit>=numDrives) return REQUEST_STATUS_ERROR | 0x02; // error : Drive not ready

	if (dinfo[subUnit].lastResult)	status |= REQUEST_STATUS_DONE;				// ok
	else							status |= REQUEST_STATUS_ERROR; 

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
}

void CMscdex::InitNewMedia(uint8_t subUnit) {
	if (subUnit<numDrives) {
		// Reopen new media
		CDROM::cdroms[subUnit]->InitNewMedia();
	}
}

bool CMscdex::ChannelControl(uint8_t subUnit, TCtrl ctrl) {
	if (subUnit>=numDrives) return false;
	// adjust strange channel mapping
	if (ctrl.out[0]>1) ctrl.out[0]=0;
	if (ctrl.out[1]>1) ctrl.out[1]=1;
	dinfo[subUnit].audioCtrl=ctrl;
	CDROM::cdroms[subUnit]->ChannelControl(ctrl);
	return true;
}

bool CMscdex::GetChannelControl(uint8_t subUnit, TCtrl& ctrl) {
	if (subUnit>=numDrives) return false;
	ctrl=dinfo[subUnit].audioCtrl;
	return true;
}

bool CMscdex::Seek(uint8_t subUnit, uint32_t sector)
{
	if (subUnit >= numDrives) {
		return false;
	}
	dinfo[subUnit].lastResult = CDROM::cdroms[subUnit]->StopAudio();
	if (dinfo[subUnit].lastResult) {
		dinfo[subUnit].audioPlay   = false;
		dinfo[subUnit].audioPaused = false;
		dinfo[subUnit].audioStart  = sector;
		dinfo[subUnit].audioEnd    = 0;
	}
	return dinfo[subUnit].lastResult;
}

static CMscdex* mscdex        = nullptr;
static PhysPt curReqheaderPtr = 0;

bool GetMSCDEXDrive(unsigned char drive_letter,CDROM_Interface **_cdrom) {
	Bitu i;

	if (mscdex == nullptr) {
		if (_cdrom) *_cdrom = nullptr;
		return false;
	}

	for (i=0;i < MSCDEX_MAX_DRIVES;i++) {
		if (CDROM::cdroms[i] == nullptr) {
			continue;
		}
		if (mscdex->dinfo[i].drive == drive_letter) {
			if (_cdrom) {
				*_cdrom = CDROM::cdroms[i].get();
			}
			return true;
		}
	}

	return false;
}

static uint16_t MSCDEX_IOCTL_Input(PhysPt buffer,uint8_t drive_unit) {
	uint8_t ioctl_fct = mem_readb(buffer);
	MSCDEX_LOG("MSCDEX: IOCTL INPUT Subfunction %02X",ioctl_fct);
	switch (ioctl_fct) {
		case 0x00 : /* Get Device Header address */
					mem_writed(buffer+1,RealMake(mscdex->rootDriverHeaderSeg,0));
					break;
		case 0x01 :{/* Get current position */
					TMSF pos = {0, 0, 0};
					mscdex->GetCurrentPos(drive_unit,pos);
					uint8_t addr_mode = mem_readb(buffer+1);
					if (addr_mode == 0) { // HSG
						auto frames = static_cast<uint32_t>(msf_to_frames(pos));
						if (frames < REDBOOK_FRAME_PADDING)
							MSCDEX_LOG("MSCDEX: Get position: invalid position %d:%d:%d", pos.min, pos.sec, pos.fr);
						else
							frames -= REDBOOK_FRAME_PADDING;
						mem_writed(buffer+2,frames);
					} else if (addr_mode==1) {	// Red book
						mem_writeb(buffer+2,pos.fr);
						mem_writeb(buffer+3,pos.sec);
						mem_writeb(buffer+4,pos.min);
						mem_writeb(buffer+5,0x00);
					} else {
						MSCDEX_LOG("MSCDEX: Get position: invalid address mode %x",addr_mode);
						return 0x03;		// invalid function
					}
				   }break;
		case 0x04 : /* Audio Channel control */
					TCtrl ctrl;
					if (!mscdex->GetChannelControl(drive_unit,ctrl)) return 0x01;
					for (uint8_t chan=0;chan<4;chan++) {
						mem_writeb(buffer+chan*2+1,ctrl.out[chan]);
						mem_writeb(buffer+chan*2+2,ctrl.vol[chan]);
					}
					break;
		case 0x06 : /* Get Device status */
					mem_writed(buffer+1,mscdex->GetDeviceStatus(drive_unit)); 
					break;
		case 0x07 : /* Get sector size */
					if (mem_readb(buffer+1)==0) mem_writew(buffer+2,2048);
					else if (mem_readb(buffer+1)==1) mem_writew(buffer+2,2352);
					else return 0x03;		// invalid function
					break;
		case 0x08 : /* Get size of current volume */
					mem_writed(buffer+1,mscdex->GetVolumeSize(drive_unit));
					break;
		case 0x09 : /* Media change ? */
					uint8_t status;
					if (!mscdex->GetMediaStatus(drive_unit,status)) {
						status = 0;		// state unknown
					}
					mem_writeb(buffer+1,status);
					break;
		case 0x0A : /* Get Audio Disk info */	
					uint8_t tr1,tr2; TMSF leadOut;
					if (!mscdex->GetCDInfo(drive_unit,tr1,tr2,leadOut)) {
						// The MSCDEX spec says that tracks return values
						// must be bounded inclusively between 1 and 99, so
						// set acceptable defaults if GetCDInfo fails.
						tr1 = 1;
						tr2 = 1;
						leadOut.min = 0;
						leadOut.sec = 0;
						leadOut.fr = 0;
					}
					mem_writeb(buffer+1,tr1);
					mem_writeb(buffer+2,tr2);
					mem_writeb(buffer+3,leadOut.fr);
					mem_writeb(buffer+4,leadOut.sec);
					mem_writeb(buffer+5,leadOut.min);
					mem_writeb(buffer+6,0x00);
					break;
		case 0x0B :{/* Audio Track Info */
					uint8_t attr = 0;
					TMSF start = {};
					uint8_t track = mem_readb(buffer+1);
					mscdex->GetTrackInfo(drive_unit,track,attr,start);		
					mem_writeb(buffer+2,start.fr);
					mem_writeb(buffer+3,start.sec);
					mem_writeb(buffer+4,start.min);
					mem_writeb(buffer+5,0x00);
					mem_writeb(buffer+6,attr);
					break; };
		case 0x0C :{/* Get Audio Sub Channel data */
					uint8_t attr = 0;
					uint8_t track = 0;
					uint8_t index = 0;
					TMSF abs = {};
					TMSF rel = {};
					mscdex->GetSubChannelData(drive_unit,attr,track,index,rel,abs);
					mem_writeb(buffer+1,attr);
					mem_writeb(buffer+2,((track/10)<<4)|(track%10)); // track in BCD
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
					uint8_t attr = 0;
					std::string upc = {};
					mscdex->GetUPC(drive_unit,attr,upc);
					mem_writeb(buffer+1,attr);
					std::vector<uint8_t> bcd = ascii_to_bcd(upc);
					bcd.resize(7);
					for (int i = 0; i < 7; ++i) {
						mem_writeb(buffer + 2 + i, bcd[i]);
					}
					mem_writeb(buffer+9,0x00);
					break;
				   };
		case 0x0F :{ /* Get Audio Status */	
					bool playing = false;
					bool pause = false;
					TMSF resStart = {0, 0, 0};
					TMSF resEnd = {0, 0, 0};
					mscdex->GetAudioStatus(drive_unit,playing,pause,resStart,resEnd);
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
		default :	LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Unsupported IOCTL INPUT Subfunction %02X",ioctl_fct);
					return 0x03;	// invalid function
	}
	return 0x00;	// success
}

static uint16_t MSCDEX_IOCTL_Optput(PhysPt buffer,uint8_t drive_unit) {
	uint8_t ioctl_fct = mem_readb(buffer);
//	MSCDEX_LOG("MSCDEX: IOCTL OUTPUT Subfunction %02X",ioctl_fct);
	switch (ioctl_fct) {
		case 0x00 :	// Unload /eject media
					if (!mscdex->LoadUnloadMedia(drive_unit,true)) return 0x02;
					break;
		case 0x03: //Audio Channel control
					TCtrl ctrl;
					for (uint8_t chan=0;chan<4;chan++) {
						ctrl.out[chan]=mem_readb(buffer+chan*2+1);
						ctrl.vol[chan]=mem_readb(buffer+chan*2+2);
					}
					if (!mscdex->ChannelControl(drive_unit,ctrl)) return 0x01;
					break;
		case 0x01 : // (un)Lock door 
					// do nothing -> report as success
					break;
		case 0x02 : // Reset Drive
					LOG(LOG_MISC,LOG_WARN)("cdromDrive reset");
					if (!mscdex->StopAudio(drive_unit))  return 0x02;
					break;
		case 0x05 :	// load media
					if (!mscdex->LoadUnloadMedia(drive_unit,false)) return 0x02;
					break;
		default	:	LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Unsupported IOCTL OUTPUT Subfunction %02X",ioctl_fct);
					return 0x03;	// invalid function
	}
	return 0x00;	// success
}

static Bitu MSCDEX_Strategy_Handler(void) {
	curReqheaderPtr = PhysicalMake(SegValue(es),reg_bx);
//	MSCDEX_LOG("MSCDEX: Device Strategy Routine called, request header at %x",curReqheaderPtr);
	return CBRET_NONE;
}

// MSCDEX interrupt documentation: https://makbit.com/articles/mscdex.txt
static Bitu MSCDEX_Interrupt_Handler(void) {
	if (curReqheaderPtr==0) {
		MSCDEX_LOG("MSCDEX: invalid call to interrupt handler");						
		return CBRET_NONE;
	}
	uint8_t	subUnit		= mem_readb(curReqheaderPtr+1);
	uint8_t	funcNr		= mem_readb(curReqheaderPtr+2);
	uint16_t	errcode		= 0;
	PhysPt	buffer		= 0;

	MSCDEX_LOG("MSCDEX: Driver Function %02X",funcNr);

	if ((funcNr==0x03) || (funcNr==0x0c) || (funcNr==0x80) || (funcNr==0x82)) {
		buffer = PhysicalMake(mem_readw(curReqheaderPtr+0x10),mem_readw(curReqheaderPtr+0x0E));
	}

 	switch (funcNr) {
		case 0x03	: {	/* IOCTL INPUT */
						uint16_t error=MSCDEX_IOCTL_Input(buffer,subUnit);
						if (error) errcode = error;
						break;
					  };
		case 0x0C	: {	/* IOCTL OUTPUT */
						uint16_t error=MSCDEX_IOCTL_Optput(buffer,subUnit);
						if (error) errcode = error;
						break;
					  };
		case 0x0D	:	// device open
		case 0x0E	:	// device close - dont care :)
						break;
		case 0x80	:	// Read long
		case 0x82	: { // Read long prefetch -> both the same here :)
						uint32_t start = mem_readd(curReqheaderPtr+0x14);
						uint16_t len	 = mem_readw(curReqheaderPtr+0x12);
						bool raw	 = (mem_readb(curReqheaderPtr+0x18)==1);
						if (mem_readb(curReqheaderPtr+0x0D)==0x00) // HSG
							mscdex->ReadSectors(subUnit,raw,start,len,buffer);
						else 
							mscdex->ReadSectorsMSF(subUnit,raw,start,len,buffer);
						break;
					  };
		case 0x83	: { // Seek
						uint8_t addressing_mode = mem_readb(curReqheaderPtr + 0x0D);
						uint32_t sector = mem_readd(curReqheaderPtr + 0x14);
						if (addressing_mode != 0) {
							TMSF msf = {};
							msf.min = (sector >> 16) & 0xFF;
							msf.sec = (sector >> 8) & 0xFF;
							msf.fr = (sector >> 0) & 0xFF;
							sector = msf_to_frames(msf);
							if (sector < REDBOOK_FRAME_PADDING) {
								MSCDEX_LOG("MSCDEX: Seek: invalid position %d:%d:%d", msf.min, msf.sec, msf.fr);
								sector = 0;
							} else {
								sector -= REDBOOK_FRAME_PADDING;
							}
						}
						mscdex->Seek(subUnit, sector);
						break;
					  };
		case 0x84	: {	/* Play Audio Sectors */
						uint32_t start = mem_readd(curReqheaderPtr+0x0E);
						uint32_t len	 = mem_readd(curReqheaderPtr+0x12);
						if (mem_readb(curReqheaderPtr+0x0D)==0x00) // HSG
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
	mem_writew(curReqheaderPtr+3,mscdex->GetStatusWord(subUnit,errcode));
	MSCDEX_LOG("MSCDEX: Status : %04X",mem_readw(curReqheaderPtr+3));						
	return CBRET_NONE;
}

static bool MSCDEX_Handler(void) {
	if(reg_ah == 0x11) {
		if(reg_al == 0x00) { 
			if (mscdex->rootDriverHeaderSeg==0) return false;
			if(real_readw(SegValue(ss),reg_sp+6) == 0xDADA) {
				//MSCDEX sets word on stack to ADAD if it DADA on entry.
				real_writew(SegValue(ss),reg_sp+6,0xADAD);
			}
			reg_al = 0xff;
			return true;
		} else
			return false;
	}

	if (reg_ah!=0x15) return false;		// not handled here, continue chain
	if (mscdex->rootDriverHeaderSeg==0) return false;	// not handled if MSCDEX not installed

	PhysPt data = PhysicalMake(SegValue(es),reg_bx);
	MSCDEX_LOG("MSCDEX: INT 2F %04X BX= %04X CX=%04X",reg_ax,reg_bx,reg_cx);
	CALLBACK_SCF(false); // carry flag cleared for all functions (undocumented); only set on error
	switch (reg_ax) {
		case 0x1500:	/* Install check */
						reg_bx = mscdex->GetNumDrives();
						if (reg_bx>0) reg_cx = mscdex->GetFirstDrive();
						reg_al = 0xff;
						break;
		case 0x1501:	/* Get cdrom driver info */
						mscdex->GetDriverInfo(data);
						break;
		case 0x1502:	/* Get Copyright filename */
		case 0x1503:	/* Get Abstract filename */
		case 0x1504:	/* Get Documentation filename */
						if (!mscdex->GetFileName(reg_cx,702+(reg_al-2)*37,data)) {
							reg_ax = MSCDEX_ERROR_UNKNOWN_DRIVE;
							CALLBACK_SCF(true);
						}
						break;
		case 0x1505: {	// read vtoc 
						uint16_t offset = 0, error = 0;
						bool success = mscdex->ReadVTOC(reg_cx,reg_dx,data,offset,error);
						reg_ax = error;
						if (!success) CALLBACK_SCF(true);
					 }
						break;
		case 0x1506:	/* Debugging on */
		case 0x1507:	/* Debugging off */
						// not functional in production MSCDEX
						break;
		case 0x1508: {	// read sectors 
						uint32_t sector = (reg_si<<16)+reg_di;
						if (mscdex->ReadSectors(reg_cx,sector,reg_dx,data)) {
							reg_ax = 0;
						} else {
							// possibly: MSCDEX_ERROR_DRIVE_NOT_READY if sector is beyond total length
							reg_ax = MSCDEX_ERROR_UNKNOWN_DRIVE;
							CALLBACK_SCF(true);
						}
					 }
						break;
		case 0x1509:	// write sectors - not supported 
						reg_ax = MSCDEX_ERROR_INVALID_FUNCTION;
						CALLBACK_SCF(true);
						break;
		case 0x150A:	/* Reserved */
						break;
		case 0x150B:	/* Valid CDROM drive ? */
						reg_ax = (mscdex->IsValidDrive(reg_cx) ? 0x5ad8 : 0x0000);
						reg_bx = 0xADAD;
						break;
		case 0x150C:	/* Get MSCDEX Version */
						reg_bx = mscdex->GetVersion();
						break;
		case 0x150D:	/* Get drives */
						mscdex->GetDrives(data);
						break;
		case 0x150E:	/* Get/Set Volume Descriptor Preference */
						if (mscdex->IsValidDrive(reg_cx)) {
							if (reg_bx == 0) {
								// get preference
								reg_dx = 0x100;	// preference?
							} else if (reg_bx == 1) {
								// set preference
								if (reg_dh != 1) {
									reg_ax = MSCDEX_ERROR_INVALID_FUNCTION;
									CALLBACK_SCF(true);
								}
							} else {
								reg_ax = MSCDEX_ERROR_INVALID_FUNCTION;
								CALLBACK_SCF(true);
							}
						} else {
							reg_ax = MSCDEX_ERROR_UNKNOWN_DRIVE;
							CALLBACK_SCF(true);
						}
						break;
		case 0x150F: {	// Get directory entry
						uint16_t error;
						bool success = mscdex->GetDirectoryEntry(reg_cl,reg_ch&1,data,PhysicalMake(reg_si,reg_di),error);
						reg_ax = error;
						if (!success) CALLBACK_SCF(true);
					 }
						break;
		case 0x1510:	/* Device driver request */
						if (!mscdex->SendDriverRequest(reg_cx,data)) {
							reg_ax = MSCDEX_ERROR_UNKNOWN_DRIVE;
							CALLBACK_SCF(true);
						}
						break;
		default:		LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Unknown call : %04X",reg_ax);
						reg_ax = MSCDEX_ERROR_INVALID_FUNCTION;
						CALLBACK_SCF(true);
						break;
	}
	return true;
}

class device_MSCDEX final : public DOS_Device {
public:
	device_MSCDEX()
	{
		SetName("MSCD001");
	}
	bool Read(uint8_t* /*data*/, uint16_t* /*size*/) override
	{
		return false;
	}
	bool Write(uint8_t* /*data*/, uint16_t* /*size*/) override
	{
		LOG(LOG_ALL, LOG_NORMAL)("Write to mscdex device");
		return false;
	}
	bool Seek(uint32_t* /*pos*/, uint32_t /*type*/) override
	{
		return false;
	}
	void Close() override
	{
	}
	uint16_t GetInformation(void) override
	{
		return 0xc880;
	}
	bool ReadFromControlChannel(PhysPt bufptr, uint16_t size,
	                            uint16_t* retcode) override;
	bool WriteToControlChannel(PhysPt bufptr, uint16_t size,
	                           uint16_t* retcode) override;

private:
	//	uint8_t cache;
};

bool device_MSCDEX::ReadFromControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode) { 
	if (MSCDEX_IOCTL_Input(bufptr,0)==0) {
		*retcode=size;
		return true;
	}
	return false;
}

bool device_MSCDEX::WriteToControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode) { 
	if (MSCDEX_IOCTL_Optput(bufptr,0)==0) {
		*retcode=size;
		return true;
	}
	return false;
}

// TODO: this functions modifies physicalPath despite several callers passing it
// a const char*. Figure out if a copy can suffice or if the it really should
// change it upstream (in which case we should drop const and fix the callers).
int MSCDEX_AddDrive(char driveLetter, const char* physicalPath, uint8_t& subUnit)
{
	int result = mscdex->AddDrive(drive_index(driveLetter),
	                              physicalPath, subUnit);
	return result;
}

int MSCDEX_RemoveDrive(char driveLetter)
{
	if(!mscdex) return 0;
	return mscdex->RemoveDrive(drive_index(driveLetter));
}

bool MSCDEX_HasDrive(char driveLetter)
{
	return mscdex->HasDrive(drive_index(driveLetter));
}

void MSCDEX_ReplaceDrive(std::unique_ptr<CDROM_Interface> cdrom, uint8_t subUnit)
{
	mscdex->ReplaceDrive(std::move(cdrom), subUnit);
}

uint8_t MSCDEX_GetSubUnit(char driveLetter)
{
	return mscdex->GetSubUnit(drive_index(driveLetter));
}

bool MSCDEX_GetVolumeName(uint8_t subUnit, char* name)
{
	return mscdex->GetVolumeName(subUnit,name);
}

bool MSCDEX_HasMediaChanged(uint8_t subUnit)
{
	bool has_changed = true;
	static TMSF leadOut[MSCDEX_MAX_DRIVES];
	TMSF leadnew;
	uint8_t tr1,tr2; // <== place-holders (use lead-out for change status)
	if (mscdex->GetCDInfo(subUnit,tr1,tr2,leadnew)) {
		has_changed = (leadOut[subUnit].min != leadnew.min
		               || leadOut[subUnit].sec != leadnew.sec
		               || leadOut[subUnit].fr != leadnew.fr);
		if (has_changed) {
			leadOut[subUnit].min = leadnew.min;
			leadOut[subUnit].sec = leadnew.sec;
			leadOut[subUnit].fr	 = leadnew.fr;
			mscdex->InitNewMedia(subUnit);
		}
	// fail-safe assumes the media has changed (if a valid drive is selected)
	} else if (subUnit<MSCDEX_MAX_DRIVES) {
		leadOut[subUnit].min = 0;
		leadOut[subUnit].sec = 0;
		leadOut[subUnit].fr	 = 0;
	}
	return has_changed;
}

void MSCDEX_Init()
{
	// Register the mscdex device
	DOS_Device* newdev = new device_MSCDEX();
	DOS_AddDevice(newdev);
	curReqheaderPtr = 0;

	// Add Multiplexer
	DOS_AddMultiplexHandler(MSCDEX_Handler);

	// Create MSCDEX
	mscdex = new CMscdex;
}

void MSCDEX_Destroy()
{
	std::for_each(CDROM::cdroms.begin(),
	              CDROM::cdroms.end(),
	              [](auto& cdrom_ptr) { cdrom_ptr.reset(); });

	delete mscdex;
	mscdex          = nullptr;
	curReqheaderPtr = 0;
}

