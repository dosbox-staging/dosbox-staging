/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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


#include <string.h>
#include <stdlib.h>
#include "dosbox.h"
#include "mem.h"
#include "dos_inc.h"
#include "support.h"

void DOS_ParamBlock::Clear()
{
	memset(&exec, 0, sizeof(exec));
	memset(&overlay, 0, sizeof(overlay));
}

void DOS_ParamBlock::LoadData()
{
	exec.envseg = SGET_WORD(sExec, envseg);
	exec.cmdtail = SGET_DWORD(sExec, cmdtail);
	exec.fcb1 = SGET_DWORD(sExec, fcb1);
	exec.fcb2 = SGET_DWORD(sExec, fcb2);
	exec.initsssp = SGET_DWORD(sExec, initsssp);
	exec.initcsip = SGET_DWORD(sExec, initcsip);
	overlay.loadseg = SGET_WORD(sOverlay, loadseg);
	overlay.relocation = SGET_WORD(sOverlay, relocation);
}

void DOS_ParamBlock::SaveData()
{
	SSET_WORD(sExec, envseg, exec.envseg);
	SSET_DWORD(sExec, cmdtail, exec.cmdtail);
	SSET_DWORD(sExec, fcb1, exec.fcb1);
	SSET_DWORD(sExec, fcb2, exec.fcb2);
	SSET_DWORD(sExec, initsssp, exec.initsssp);
	SSET_DWORD(sExec, initcsip, exec.initcsip);
}


void DOS_InfoBlock::SetLocation(Bit16u segment) {
	seg = segment;
	pt=PhysMake(seg,0);
	/* Clear the initial Block */
	for(Bitu i=0;i<sizeof(sDIB);i++) mem_writeb(pt+i,0xff);
	for(Bitu i=0;i<14;i++) mem_writeb(pt+i,0);

	sSave(sDIB,regCXfrom5e,(Bit16u)0);
	sSave(sDIB,countLRUcache,(Bit16u)0);
	sSave(sDIB,countLRUopens,(Bit16u)0);

	sSave(sDIB,protFCBs,(Bit16u)0);
	sSave(sDIB,specialCodeSeg,(Bit16u)0);
	sSave(sDIB,joindedDrives,(Bit8u)0);
	sSave(sDIB,lastdrive,(Bit8u)0x01);//increase this if you add drives to cds-chain

	sSave(sDIB,diskInfoBuffer,RealMake(segment,offsetof(sDIB,diskBufferHeadPt)));
	sSave(sDIB,setverPtr,(Bit32u)0);

	sSave(sDIB,a20FixOfs,(Bit16u)0);
	sSave(sDIB,pspLastIfHMA,(Bit16u)0);
	sSave(sDIB,blockDevices,(Bit8u)0);
	
	sSave(sDIB,bootDrive,(Bit8u)0);
	sSave(sDIB,useDwordMov,(Bit8u)1);
	sSave(sDIB,extendedSize,(Bit16u)(MEM_TotalPages()*4-1024));
	sSave(sDIB,magicWord,(Bit16u)0x0001);		// dos5+

	sSave(sDIB,sharingCount,(Bit16u)0);
	sSave(sDIB,sharingDelay,(Bit16u)0);
	sSave(sDIB,ptrCONinput,(Bit16u)0);			// no unread input available
	sSave(sDIB,maxSectorLength,(Bit16u)0x200);

	sSave(sDIB,dirtyDiskBuffers,(Bit16u)0);
	sSave(sDIB,lookaheadBufPt,(Bit32u)0);
	sSave(sDIB,lookaheadBufNumber,(Bit16u)0);
	sSave(sDIB,bufferLocation,(Bit8u)0);		// buffer in base memory, no workspace
	sSave(sDIB,workspaceBuffer,(Bit32u)0);

	sSave(sDIB,minMemForExec,(Bit16u)0);
	sSave(sDIB,memAllocScanStart,(Bit16u)DOS_MEM_START);
	sSave(sDIB,startOfUMBChain,(Bit16u)0xffff);
	sSave(sDIB,chainingUMB,(Bit8u)0);

	sSave(sDIB,nulNextDriver,(Bit32u)0xffffffff);
	sSave(sDIB,nulAttributes,(Bit16u)0x8004);
	sSave(sDIB,nulStrategy,(Bit32u)0x00000000);
	sSave(sDIB,nulString[0],(Bit8u)0x4e);
	sSave(sDIB,nulString[1],(Bit8u)0x55);
	sSave(sDIB,nulString[2],(Bit8u)0x4c);
	sSave(sDIB,nulString[3],(Bit8u)0x20);
	sSave(sDIB,nulString[4],(Bit8u)0x20);
	sSave(sDIB,nulString[5],(Bit8u)0x20);
	sSave(sDIB,nulString[6],(Bit8u)0x20);
	sSave(sDIB,nulString[7],(Bit8u)0x20);

	/* Create a fake SFT, so programs think there are 100 file handles */
	Bit16u sftOffset=offsetof(sDIB,firstFileTable)+0xa2;
	sSave(sDIB,firstFileTable,RealMake(segment,sftOffset));
	real_writed(segment,sftOffset+0x00,RealMake(segment+0x26,0));	//Next File Table
	real_writew(segment,sftOffset+0x04,100);		//File Table supports 100 files
	real_writed(segment+0x26,0x00,0xffffffff);		//Last File Table
	real_writew(segment+0x26,0x04,100);				//File Table supports 100 files
}

void DOS_InfoBlock::SetFirstMCB(Bit16u _firstmcb) {
	sSave(sDIB,firstMCB,_firstmcb); //c2woody
}

void DOS_InfoBlock::SetBuffers(Bit16u x,Bit16u y) {
	sSave(sDIB,buffers_x,x);
	sSave(sDIB,buffers_y,y);
}

void DOS_InfoBlock::SetCurDirStruct(Bit32u _curdirstruct) {
	sSave(sDIB,curDirStructure,_curdirstruct);
}

void DOS_InfoBlock::SetFCBTable(Bit32u _fcbtable) {
	sSave(sDIB,fcbTable,_fcbtable);
}

void DOS_InfoBlock::SetDeviceChainStart(Bit32u _devchain) {
	sSave(sDIB,nulNextDriver,_devchain);
}

void DOS_InfoBlock::SetDiskBufferHeadPt(Bit32u _dbheadpt) {
	sSave(sDIB,diskBufferHeadPt,_dbheadpt);
}

Bit16u DOS_InfoBlock::GetStartOfUMBChain(void) {
	return (Bit16u)sGet(sDIB,startOfUMBChain);
}

void DOS_InfoBlock::SetStartOfUMBChain(Bit16u _umbstartseg) {
	sSave(sDIB,startOfUMBChain,_umbstartseg);
}

Bit8u DOS_InfoBlock::GetUMBChainState(void) {
	return (Bit8u)sGet(sDIB,chainingUMB);
}

void DOS_InfoBlock::SetUMBChainState(Bit8u _umbchaining) {
	sSave(sDIB,chainingUMB,_umbchaining);
}

void DOS_InfoBlock::SetBlockDevices(Bit8u _count) {
	sSave(sDIB,blockDevices,_count);
}

RealPt DOS_InfoBlock::GetPointer(void) {
	return RealMake(seg,offsetof(sDIB,firstDPB));
}

Bit32u DOS_InfoBlock::GetDeviceChain(void) {
	return sGet(sDIB,nulNextDriver);
}

Bit16u DOS_PSP::rootpsp = 0;

void DOS_PSP::MakeNew(uint16_t mem_size)
{
	/* get previous */
//	DOS_PSP prevpsp(dos.psp());
	/* Clear it first */
	for (PhysPt i = 0; i < sizeof(sPSP); ++i)
		mem_writeb(pt + i, 0);
	// Set size
	SSET_WORD(sPSP, next_seg, static_cast<uint16_t>(seg + mem_size));
	/* far call opcode */
	SSET_BYTE(sPSP, far_call, uint8_t(0xea));
	// far call to interrupt 0x21 - faked for bill & ted
	// lets hope nobody really uses this address
	SSET_DWORD(sPSP, cpm_entry, RealMake(0xdead, 0xffff));
	/* Standard blocks,int 20  and int21 retf */
	SSET_BYTE(sPSP, exit[0], uint8_t(0xcd));
	SSET_BYTE(sPSP, exit[1], uint8_t(0x20));
	SSET_BYTE(sPSP, service[0], uint8_t(0xcd));
	SSET_BYTE(sPSP, service[1], uint8_t(0x21));
	SSET_BYTE(sPSP, service[2], uint8_t(0xcb));
	/* psp and psp-parent */
	SSET_WORD(sPSP, psp_parent, dos.psp());
	SSET_DWORD(sPSP, prev_psp, uint32_t(0xffffffff));
	SSET_WORD(sPSP, dos_version, uint16_t(0x0005));
	/* terminate 22,break 23,crititcal error 24 address stored */
	SaveVectors();

	/* FCBs are filled with 0 */
	// ....
	/* Init file pointer and max_files */
	SSET_DWORD(sPSP, file_table, RealMake(seg, offsetof(sPSP, files)));
	SSET_WORD(sPSP, max_files, uint16_t(20));
	for (Bit16u ct=0;ct<20;ct++) SetFileHandle(ct,0xff);

	/* User Stack pointer */
//	if (prevpsp.GetSegment()!=0) SSET_DWORD(sPSP,stack,prevpsp.GetStack());

	if (rootpsp==0) rootpsp = seg;
}

uint8_t DOS_PSP::GetFileHandle(uint16_t index) const
{
	if (index >= SGET_WORD(sPSP, max_files))
		return 0xff;
	const PhysPt files = Real2Phys(SGET_DWORD(sPSP, file_table));
	return mem_readb(files + index);
}

void DOS_PSP::SetFileHandle(uint16_t index, uint8_t handle)
{
	if (index < SGET_WORD(sPSP, max_files)) {
		const PhysPt files = Real2Phys(SGET_DWORD(sPSP, file_table));
		mem_writeb(files + index, handle);
	} else {
		DEBUG_LOG_MSG("DOS: Prevented buffer overflow on write to PSP file_table[%u]",
		              index);
	}
}

uint16_t DOS_PSP::FindFreeFileEntry() const
{
	PhysPt files = Real2Phys(SGET_DWORD(sPSP, file_table));
	const auto max_files = SGET_WORD(sPSP, max_files);
	for (uint16_t i = 0; i < max_files; ++i) {
		if (mem_readb(files + i) == 0xff)
			return i;
	}
	return 0xff;
}

uint16_t DOS_PSP::FindEntryByHandle(uint8_t handle) const
{
	const PhysPt files = Real2Phys(SGET_DWORD(sPSP, file_table));
	const auto max_files = SGET_WORD(sPSP, max_files);
	for (uint16_t i = 0; i < max_files; ++i) {
		if (mem_readb(files + i) == handle)
			return i;
	}
	return 0xff;
}

void DOS_PSP::CopyFileTable(DOS_PSP *srcpsp, bool createchildpsp)
{
	/* Copy file table from calling process */
	for (Bit16u i=0;i<20;i++) {
		Bit8u handle = srcpsp->GetFileHandle(i);
		if(createchildpsp)
		{	//copy obeying not inherit flag.(but dont duplicate them)
			bool allowCopy = true;//(handle==0) || ((handle>0) && (FindEntryByHandle(handle)==0xff));
			if((handle<DOS_FILES) && Files[handle] && !(Files[handle]->flags & DOS_NOT_INHERIT) && allowCopy)
			{
				Files[handle]->AddRef();
				SetFileHandle(i,handle);
			}
			else
			{
				SetFileHandle(i,0xff);
			}
		}
		else
		{	//normal copy so don't mind the inheritance
			SetFileHandle(i,handle);
		}
	}
}

void DOS_PSP::CloseFiles()
{
	const auto max_files = SGET_WORD(sPSP, max_files);
	for (uint16_t i = 0; i < max_files; ++i)
		DOS_CloseFile(i);
}

void DOS_PSP::SaveVectors()
{
	/* Save interrupt 22,23,24 */
	SSET_DWORD(sPSP, int_22, RealGetVec(0x22));
	SSET_DWORD(sPSP, int_23, RealGetVec(0x23));
	SSET_DWORD(sPSP, int_24, RealGetVec(0x24));
}

void DOS_PSP::RestoreVectors()
{
	/* Restore interrupt 22,23,24 */
	RealSetVec(0x22, SGET_DWORD(sPSP, int_22));
	RealSetVec(0x23, SGET_DWORD(sPSP, int_23));
	RealSetVec(0x24, SGET_DWORD(sPSP, int_24));
}

void DOS_PSP::SetCommandTail(RealPt src)
{
	if (src) { // valid source
		MEM_BlockCopy(pt+offsetof(sPSP,cmdtail),Real2Phys(src),128);
	} else { // empty
		SSET_BYTE(sPSP, cmdtail.count, uint8_t(0));
		mem_writeb(pt+offsetof(sPSP,cmdtail.buffer),0x0d);
	}
}

void DOS_PSP::SetFCB1(RealPt src) {
	if (src) MEM_BlockCopy(PhysMake(seg,offsetof(sPSP,fcb1)),Real2Phys(src),16);
}

void DOS_PSP::SetFCB2(RealPt src) {
	if (src) MEM_BlockCopy(PhysMake(seg,offsetof(sPSP,fcb2)),Real2Phys(src),16);
}

bool DOS_PSP::SetNumFiles(uint16_t file_num)
{
	// 20 minimum. clipper program.
	if (file_num < 20)
		file_num = 20;

	if (file_num > 20) {
		// Allocate needed paragraphs
		file_num += 2; // Add a few more files for safety
		const uint16_t para = (file_num / 16) + ((file_num % 16) > 0);
		const RealPt data = RealMake(DOS_GetMemory(para), 0);
		SSET_DWORD(sPSP, file_table, data);
		SSET_WORD(sPSP, max_files, file_num);

		// enumerating 20 constexpr integers manually is easier and
		// faster than actually using std::integer_sequence...
		// (second parameter to SGET_BYTE must be constexpr)
		SetFileHandle(0, SGET_BYTE(sPSP, files[0]));
		SetFileHandle(1, SGET_BYTE(sPSP, files[1]));
		SetFileHandle(2, SGET_BYTE(sPSP, files[2]));
		SetFileHandle(3, SGET_BYTE(sPSP, files[3]));
		SetFileHandle(4, SGET_BYTE(sPSP, files[4]));
		SetFileHandle(5, SGET_BYTE(sPSP, files[5]));
		SetFileHandle(6, SGET_BYTE(sPSP, files[6]));
		SetFileHandle(7, SGET_BYTE(sPSP, files[7]));
		SetFileHandle(8, SGET_BYTE(sPSP, files[8]));
		SetFileHandle(9, SGET_BYTE(sPSP, files[9]));
		SetFileHandle(10, SGET_BYTE(sPSP, files[10]));
		SetFileHandle(11, SGET_BYTE(sPSP, files[11]));
		SetFileHandle(12, SGET_BYTE(sPSP, files[12]));
		SetFileHandle(13, SGET_BYTE(sPSP, files[13]));
		SetFileHandle(14, SGET_BYTE(sPSP, files[14]));
		SetFileHandle(15, SGET_BYTE(sPSP, files[15]));
		SetFileHandle(16, SGET_BYTE(sPSP, files[16]));
		SetFileHandle(17, SGET_BYTE(sPSP, files[17]));
		SetFileHandle(18, SGET_BYTE(sPSP, files[18]));
		SetFileHandle(19, SGET_BYTE(sPSP, files[19]));

		for (uint16_t i = 20; i < file_num; i++)
			SetFileHandle(i, 0xFF);
	} else {
		SSET_WORD(sPSP, max_files, file_num);
	}
	return true;
}

void DOS_DTA::SetupSearch(uint8_t drive, uint8_t attr, char *pattern)
{
	SSET_BYTE(sDTA, sdrive, drive);
	SSET_BYTE(sDTA, sattr, attr);
	/* Fill with spaces */
	Bitu i;
	for (i=0;i<11;i++) mem_writeb(pt+offsetof(sDTA,sname)+i,' ');
	char * find_ext;
	find_ext=strchr(pattern,'.');
	if (find_ext) {
		Bitu size=(Bitu)(find_ext-pattern);
		if (size>8) size=8;
		MEM_BlockWrite(pt+offsetof(sDTA,sname),pattern,size);
		find_ext++;
		MEM_BlockWrite(pt+offsetof(sDTA,sext),find_ext,(strlen(find_ext)>3) ? 3 : (Bitu)strlen(find_ext));
	} else {
		MEM_BlockWrite(pt+offsetof(sDTA,sname),pattern,(strlen(pattern) > 8) ? 8 : (Bitu)strlen(pattern));
	}
}

void DOS_DTA::SetResult(const char *found_name,
                        uint32_t found_size,
                        uint16_t found_date,
                        uint16_t found_time,
                        uint8_t found_attr)
{
	MEM_BlockWrite(pt + offsetof(sDTA, name), found_name, strlen(found_name) + 1);
	SSET_DWORD(sDTA, size, found_size);
	SSET_WORD(sDTA, date, found_date);
	SSET_WORD(sDTA, time, found_time);
	SSET_BYTE(sDTA, attr, found_attr);
}

void DOS_DTA::GetResult(char *found_name,
                        uint32_t &found_size,
                        uint16_t &found_date,
                        uint16_t &found_time,
                        uint8_t &found_attr) const
{
	constexpr auto name_offset = offsetof(sDTA, name);
	MEM_BlockRead(pt + name_offset, found_name, DOS_NAMELENGTH_ASCII);
	found_size = SGET_DWORD(sDTA, size);
	found_date = SGET_WORD(sDTA, date);
	found_time = SGET_WORD(sDTA, time);
	found_attr = SGET_BYTE(sDTA, attr);
}

void DOS_DTA::GetSearchParams(uint8_t &attr, char *pattern) const
{
	attr = SGET_BYTE(sDTA, sattr);
	char temp[11];
	MEM_BlockRead(pt+offsetof(sDTA,sname),temp,11);
	memcpy(pattern,temp,8);
	pattern[8]='.';
	memcpy(&pattern[9],&temp[8],3);
	pattern[12]=0;
}

DOS_FCB::DOS_FCB(uint16_t seg, uint16_t off, bool allow_extended)
        : extended(false),
          real_pt(0)
{
	SetPt(seg, off);
	real_pt = pt;
	if (allow_extended) {
		if (SGET_BYTE(sFCB, drive) == 0xff) {
			pt+=7;
			extended=true;
		}
	}
}

void DOS_FCB::Create(bool _extended) {
	Bitu fill;
	if (_extended) fill=33+7;
	else fill=33;
	Bitu i;
	for (i=0;i<fill;i++) mem_writeb(real_pt+i,0);
	pt=real_pt;
	if (_extended) {
		mem_writeb(real_pt,0xff);
		pt+=7;
		extended=true;
	} else extended=false;
}

void DOS_FCB::SetName(uint8_t drive, const char *fname, const char *ext)
{
	SSET_BYTE(sFCB, drive, drive);
	constexpr size_t name_len = ARRAY_LEN(sFCB::filename);
	constexpr size_t ext_len = ARRAY_LEN(sFCB::ext);
	MEM_BlockWrite(pt + offsetof(sFCB, filename), fname, name_len);
	MEM_BlockWrite(pt + offsetof(sFCB, ext), ext, ext_len);
}

void DOS_FCB::SetSizeDateTime(uint32_t size, uint16_t mod_date, uint16_t mod_time)
{
	SSET_DWORD(sFCB, filesize, size);
	SSET_WORD(sFCB, date, mod_date);
	SSET_WORD(sFCB, time, mod_time);
}

void DOS_FCB::GetSizeDateTime(uint32_t &size, uint16_t &mod_date, uint16_t &mod_time) const
{
	size = SGET_DWORD(sFCB, filesize);
	mod_date = SGET_WORD(sFCB, date);
	mod_time = SGET_WORD(sFCB, time);
}

void DOS_FCB::GetRecord(uint16_t &block, uint8_t &rec) const
{
	block = SGET_WORD(sFCB, cur_block);
	rec = SGET_BYTE(sFCB, cur_rec);
}

void DOS_FCB::SetRecord(uint16_t block, uint8_t rec)
{
	SSET_WORD(sFCB, cur_block, block);
	SSET_BYTE(sFCB, cur_rec, rec);
}

void DOS_FCB::GetSeqData(uint8_t &fhandle, uint16_t &rsize) const
{
	fhandle = SGET_BYTE(sFCB, file_handle);
	rsize = SGET_WORD(sFCB, rec_size);
}

void DOS_FCB::SetSeqData(uint8_t fhandle, uint16_t rsize)
{
	SSET_BYTE(sFCB, file_handle, fhandle);
	SSET_WORD(sFCB, rec_size, rsize);
}

void DOS_FCB::ClearBlockRecsize()
{
	SSET_WORD(sFCB, cur_block, uint16_t(0));
	SSET_WORD(sFCB, rec_size, uint16_t(0));
}

void DOS_FCB::FileOpen(uint8_t fhandle)
{
	SSET_BYTE(sFCB, drive, static_cast<uint8_t>(GetDrive() + 1));
	SSET_BYTE(sFCB, file_handle, fhandle);
	SSET_WORD(sFCB, cur_block, uint16_t(0));
	SSET_WORD(sFCB, rec_size, uint16_t(128));
	// SSET_WORD(sFCB, rndm, 0); // breaks Jewels of darkness.
	uint32_t size = 0;
	Files[fhandle]->Seek(&size, DOS_SEEK_END);
	SSET_DWORD(sFCB, filesize, size);
	size = 0;
	Files[fhandle]->Seek(&size, DOS_SEEK_SET);
	SSET_WORD(sFCB, time, Files[fhandle]->time);
	SSET_WORD(sFCB, date, Files[fhandle]->date);
}

bool DOS_FCB::Valid() const
{
	// Very simple check for Oubliette
	return (SGET_BYTE(sFCB, filename[0]) || SGET_BYTE(sFCB, file_handle));
}

void DOS_FCB::FileClose(uint8_t &fhandle)
{
	fhandle = SGET_BYTE(sFCB, file_handle);
	SSET_BYTE(sFCB, file_handle, uint8_t(0xff));
}

uint8_t DOS_FCB::GetDrive() const
{
	const uint8_t drive = SGET_BYTE(sFCB, drive);
	if (!drive)
		return DOS_GetDefaultDrive();
	else
		return drive - 1;
}

void DOS_FCB::GetName(char * fillname) {
	fillname[0]=GetDrive()+'A';
	fillname[1]=':';
	MEM_BlockRead(pt+offsetof(sFCB,filename),&fillname[2],8);
	fillname[10]='.';
	MEM_BlockRead(pt+offsetof(sFCB,ext),&fillname[11],3);
	fillname[14]=0;
}

void DOS_FCB::GetAttr(uint8_t &attr) const
{
	if (extended)
		attr = mem_readb(pt - 1);
}

void DOS_FCB::SetAttr(uint8_t attr)
{
	if (extended)
		mem_writeb(pt - 1, attr);
}

void DOS_FCB::SetResult(Bit32u size,Bit16u date,Bit16u time,Bit8u attr) {
	mem_writed(pt + 0x1d,size);
	mem_writew(pt + 0x19,date);
	mem_writew(pt + 0x17,time);
	mem_writeb(pt + 0x0c,attr);
}

void DOS_SDA::Init()
{
	/* Clear */
	for (size_t i = 0; i < sizeof(sSDA); ++i)
		mem_writeb(pt + i, 0x00);
	SSET_BYTE(sSDA, drive_crit_error, uint8_t(0xff));
}
