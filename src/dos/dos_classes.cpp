// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

#include "hardware/memory.h"
#include "utils/string_utils.h"
#include "misc/support.h"

static void dos_memset(PhysPt addr, uint8_t val, size_t n)
{
	assert(n < UINT32_MAX);
	for (uint32_t i = 0; i < n; ++i)
		mem_writeb(addr + i, val);
}

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

void DOS_InfoBlock::SetLocation(uint16_t segment)
{
	seg = segment;
	pt = PhysicalMake(seg, 0);

	/* Clear the initial Block */
	dos_memset(pt, 0xff, sizeof(sDIB));
	dos_memset(pt, 0x00, 14);

	SSET_WORD(sDIB, regCXfrom5e, uint16_t(0));
	SSET_WORD(sDIB, countLRUcache, uint16_t(0));
	SSET_WORD(sDIB, countLRUopens, uint16_t(0));
	SSET_WORD(sDIB, protFCBs, uint16_t(0));
	SSET_WORD(sDIB, specialCodeSeg, uint16_t(0));
	SSET_BYTE(sDIB, joindedDrives, uint8_t(0));
	SSET_BYTE(sDIB, lastdrive, uint8_t(0x01)); // increase this if you add drives to cds-chain
	const RealPt dib_addr = RealMake(segment, offsetof(sDIB, diskBufferHeadPt));
	SSET_DWORD(sDIB, diskInfoBuffer, dib_addr);
	SSET_DWORD(sDIB, setverPtr, uint32_t(0));
	SSET_WORD(sDIB, a20FixOfs, uint16_t(0));
	SSET_WORD(sDIB, pspLastIfHMA, uint16_t(0));
	SSET_BYTE(sDIB, blockDevices, uint8_t(0));
	SSET_BYTE(sDIB, bootDrive, uint8_t(0));
	SSET_BYTE(sDIB, useDwordMov, uint8_t(1));
	const auto ext_size = static_cast<uint16_t>(MEM_TotalPages() * 4 - 1024);
	SSET_WORD(sDIB, extendedSize, ext_size);
	SSET_WORD(sDIB, magicWord, uint16_t(0x0001)); // dos5+
	SSET_WORD(sDIB, sharingCount, uint16_t(0));
	SSET_WORD(sDIB, sharingDelay, uint16_t(0));
	SSET_WORD(sDIB, ptrCONinput, uint16_t(0)); // no unread input available
	SSET_WORD(sDIB, maxSectorLength, uint16_t(0x200));
	SSET_WORD(sDIB, dirtyDiskBuffers, uint16_t(0));
	SSET_DWORD(sDIB, lookaheadBufPt, uint32_t(0));
	SSET_WORD(sDIB, lookaheadBufNumber, uint16_t(0));
	SSET_BYTE(sDIB, bufferLocation, uint8_t(0)); // buffer in base memory,
	                                             // no workspace
	SSET_DWORD(sDIB, workspaceBuffer, uint32_t(0));
	SSET_WORD(sDIB, minMemForExec, uint16_t(0));
	SSET_WORD(sDIB, memAllocScanStart, uint16_t(DOS_MEM_START));
	SSET_WORD(sDIB, startOfUMBChain, uint16_t(0xffff));
	SSET_BYTE(sDIB, chainingUMB, uint8_t(0));
	SSET_DWORD(sDIB, nulNextDriver, uint32_t(0xffffffff));
	SSET_WORD(sDIB, nulAttributes, uint16_t(0x8004));
	SSET_DWORD(sDIB, nulStrategy, uint32_t(0x00000000));
	SSET_BYTE(sDIB, nulString[0], uint8_t(0x4e));
	SSET_BYTE(sDIB, nulString[1], uint8_t(0x55));
	SSET_BYTE(sDIB, nulString[2], uint8_t(0x4c));
	SSET_BYTE(sDIB, nulString[3], uint8_t(0x20));
	SSET_BYTE(sDIB, nulString[4], uint8_t(0x20));
	SSET_BYTE(sDIB, nulString[5], uint8_t(0x20));
	SSET_BYTE(sDIB, nulString[6], uint8_t(0x20));
	SSET_BYTE(sDIB, nulString[7], uint8_t(0x20));

	// Create a fake SFT, so programs think there are 100 file handles
	const uint16_t sft_offset = offsetof(sDIB, firstFileTable) + 0xa2;
	const RealPt sft_addr = RealMake(segment, sft_offset);
	SSET_DWORD(sDIB, firstFileTable, sft_addr);
	// Next File Table
	real_writed(segment, sft_offset + SftNextTableOffset, RealMake(segment + 0x26, 0));
	// File Table supports 100 files
	real_writew(segment, sft_offset + SftNumberOfFilesOffset, 100);
	// Last File Table
	real_writed(segment + 0x26, SftNextTableOffset, SftEndPointer);
	// File Table supports 100 files
	real_writew(segment + 0x26, SftNumberOfFilesOffset, 100);
}

void DOS_InfoBlock::SetBuffers(uint16_t x, uint16_t y)
{
	SSET_WORD(sDIB, buffers_x, x);
	SSET_WORD(sDIB, buffers_y, y);
}

uint16_t DOS_PSP::rootpsp = 0;

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
	SSET_BYTE(sPSP, dos_version_major, dos.version.major);
	SSET_BYTE(sPSP, dos_version_minor, dos.version.minor);
	/* terminate 22,break 23,crititcal error 24 address stored */
	SaveVectors();

	/* FCBs are filled with 0 */
	// ....
	/* Init file pointer and max_files */
	const RealPt ftab_addr = RealMake(seg, offsetof(sPSP, files));
	SSET_DWORD(sPSP, file_table, ftab_addr);
	SSET_WORD(sPSP, max_files, uint16_t(20));
	for (uint16_t ct=0;ct<20;ct++) SetFileHandle(ct,0xff);

	/* User Stack pointer */
//	if (prevpsp.GetSegment()!=0) SSET_DWORD(sPSP,stack,prevpsp.GetStack());

	if (rootpsp==0) rootpsp = seg;
}

uint8_t DOS_PSP::GetFileHandle(uint16_t index) const
{
	if (index >= SGET_WORD(sPSP, max_files))
		return 0xff;
	const PhysPt files = RealToPhysical(SGET_DWORD(sPSP, file_table));
	return mem_readb(files + index);
}

void DOS_PSP::SetFileHandle(uint16_t index, uint8_t handle)
{
	if (index < SGET_WORD(sPSP, max_files)) {
		const PhysPt files = RealToPhysical(SGET_DWORD(sPSP, file_table));
		mem_writeb(files + index, handle);
	} else {
		LOG_DEBUG("DOS: Prevented buffer overflow on write to PSP file_table[%u]",
		          index);
	}
}

uint16_t DOS_PSP::FindFreeFileEntry() const
{
	PhysPt files = RealToPhysical(SGET_DWORD(sPSP, file_table));
	const auto max_files = SGET_WORD(sPSP, max_files);
	for (uint16_t i = 0; i < max_files; ++i) {
		if (mem_readb(files + i) == 0xff)
			return i;
	}
	return 0xff;
}

uint16_t DOS_PSP::FindEntryByHandle(uint8_t handle) const
{
	const PhysPt files = RealToPhysical(SGET_DWORD(sPSP, file_table));
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
	for (uint16_t i=0;i<20;i++) {
		uint8_t handle = srcpsp->GetFileHandle(i);
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
		MEM_BlockCopy(pt+offsetof(sPSP,cmdtail),RealToPhysical(src),128);
	} else { // empty
		SSET_BYTE(sPSP, cmdtail.count, uint8_t(0));
		mem_writeb(pt+offsetof(sPSP,cmdtail.buffer),0x0d);
	}
}

void DOS_PSP::SetFCB1(RealPt src) {
	if (src) MEM_BlockCopy(PhysicalMake(seg,offsetof(sPSP,fcb1)),RealToPhysical(src),16);
}

void DOS_PSP::SetFCB2(RealPt src) {
	if (src) MEM_BlockCopy(PhysicalMake(seg,offsetof(sPSP,fcb2)),RealToPhysical(src),16);
}

bool DOS_PSP::SetNumFiles(uint16_t file_num)
{
	// 20 minimum. clipper program.
	if (file_num < 20)
		file_num = 20;

	if (file_num > 20 && ((file_num + 2) > SGET_WORD(sPSP, max_files))) {
		// Allocate needed paragraphs
		file_num += 2; // Add a few more files for safety
		const uint16_t para = (file_num / 16) + ((file_num % 16) > 0);
		const RealPt data = RealMake(DOS_GetMemory(para), 0);
		for (uint16_t i = 0; i < file_num; i++) {
			const uint8_t handle = (i < 20 ? GetFileHandle(i) : 0xFF);
			mem_writeb(RealToPhysical(data) + i, handle);
		}
		SSET_DWORD(sPSP, file_table, data);
	}
	SSET_WORD(sPSP, max_files, file_num);
	return true;
}

static constexpr auto bytes_to_read = 1024;

std::optional<std::string> DOS_PSP::GetEnvironmentValue(const std::string_view variable) const
{
	/* Walk through the internal environment and see for a match */
	PhysPt env_read = PhysicalMake(GetEnvironment(), 0);

	char env_string[bytes_to_read + 1];
	if (variable.empty()) {
		return {};
	}

	for (;;) {
		MEM_StrCopy(env_read, env_string, bytes_to_read);
		if (!env_string[0]) {
			return {};
		}
		env_read += (PhysPt)(safe_strlen(env_string) + 1);
		char* equal = strchr(env_string, '=');
		if (!equal) {
			continue;
		}
		/* replace the = with \0 to get the length */
		*equal = '\0';
		if (strlen(env_string) != variable.size()) {
			continue;
		}
		if (!iequals(variable, env_string)) {
			continue;
		}

		return env_string + variable.size() + sizeof('=');
	}
}

std::vector<std::string> DOS_PSP::GetAllRawEnvironmentStrings() const
{
	std::vector<std::string> all_env_vars = {};

	char env_string[bytes_to_read + 1];
	PhysPt env_read = PhysicalMake(GetEnvironment(), 0);
	for (;;) {
		MEM_StrCopy(env_read, env_string, bytes_to_read);
		if (!env_string[0]) {
			return all_env_vars;
		}
		all_env_vars.emplace_back(env_string);
		env_read += (PhysPt)(safe_strlen(env_string) + 1);
	}
}

bool DOS_PSP::SetEnvironmentValue(std::string_view variable, std::string_view new_string)
{
	PhysPt env_read = PhysicalMake(GetEnvironment(), 0);

	// Get size of environment.
	DOS_MCB mcb(GetEnvironment() - 1);
	uint16_t envsize = mcb.GetSize() * 16;

	PhysPt env_write                   = env_read;
	PhysPt env_write_start             = env_read;
	char env_string[bytes_to_read + 1] = {0};
	const auto entry_length            = variable.size();
	do {
		MEM_StrCopy(env_read, env_string, bytes_to_read);
		if (!env_string[0]) {
			break;
		}
		env_read += (PhysPt)(safe_strlen(env_string) + 1);
		if (!strchr(env_string, '=')) {
			continue; /* Remove corrupt entry? */
		}
		if (iequals(variable,
		            std::string_view(env_string).substr(0, entry_length)) &&
		    env_string[entry_length] == '=') {
			continue;
		}
		MEM_BlockWrite(env_write,
		               env_string,
		               (Bitu)(safe_strlen(env_string) + 1));
		env_write += (PhysPt)(safe_strlen(env_string) + 1);
	} while (true);
	/* TODO Maybe save the program name sometime. not really needed though */
	/* Save the new entry */

	// ensure room
	if (envsize <= (env_write - env_write_start) + variable.size() + 1 +
	                       new_string.size() + 2) {
		return false;
	}

	if (!new_string.empty()) {
		std::string bigentry(variable);
		for (auto& entry : bigentry) {
			entry = toupper(entry);
		}
		snprintf(env_string,
		         bytes_to_read + 1,
		         "%s=%s",
		         bigentry.c_str(),
		         std::string(new_string).c_str());
		MEM_BlockWrite(env_write,
		               env_string,
		               (Bitu)(safe_strlen(env_string) + 1));
		env_write += (PhysPt)(safe_strlen(env_string) + 1);
	}
	/* Clear out the final piece of the environment */
	mem_writeb(env_write, 0);
	return true;
}

std::string DOS_DTA::Result::GetExtension() const
{
	const auto pos = name.rfind('.');
	if (pos == std::string::npos) {
		return std::string();
	}

	return name.substr(pos + 1);
}

std::string DOS_DTA::Result::GetBareName() const
{
	if (name == "." || name == "..") {
		return name;
	}

	const auto pos = name.rfind('.');
	if (pos == std::string::npos) {
		return name;
	} else if (pos < 1) {
		return std::string();
	}

	return name.substr(0, pos);
}

void DOS_DTA::SetupSearch(uint8_t drive, FatAttributeFlags attr, char* pattern)
{
	SSET_BYTE(sDTA, sdrive, drive);
	SSET_BYTE(sDTA, sattr, attr._data);
	/* Fill with spaces */
	dos_memset(pt + offsetof(sDTA, sname), ' ', sizeof(sDTA::sname));
	dos_memset(pt + offsetof(sDTA, sext), ' ', sizeof(sDTA::sext));
	char * find_ext;
	find_ext=strchr(pattern,'.');
	if (find_ext) {
		auto size=(Bitu)(find_ext-pattern);
		if (size>8) size=8;
		MEM_BlockWrite(pt+offsetof(sDTA,sname),pattern,size);
		find_ext++;
		MEM_BlockWrite(pt+offsetof(sDTA,sext),find_ext,(strlen(find_ext)>3) ? 3 : (Bitu)strlen(find_ext));
	} else {
		MEM_BlockWrite(pt+offsetof(sDTA,sname),pattern,(strlen(pattern) > 8) ? 8 : (Bitu)strlen(pattern));
	}
}

void DOS_DTA::SetResult(const char* found_name, uint32_t found_size,
                        uint16_t found_date, uint16_t found_time,
                        FatAttributeFlags found_attr)
{
	MEM_BlockWrite(pt + offsetof(sDTA, name), found_name, strlen(found_name) + 1);
	SSET_DWORD(sDTA, size, found_size);
	SSET_WORD(sDTA, date, found_date);
	SSET_WORD(sDTA, time, found_time);
	SSET_BYTE(sDTA, attr, found_attr._data);
}

void DOS_DTA::GetResult(Result& result) const
{
	char found_name[DOS_NAMELENGTH_ASCII];
	constexpr auto name_offset = offsetof(sDTA, name);
	MEM_BlockRead(pt + name_offset, found_name, DOS_NAMELENGTH_ASCII);

	result.size = SGET_DWORD(sDTA, size);
	result.date = SGET_WORD(sDTA, date);
	result.time = SGET_WORD(sDTA, time);
	result.attr = SGET_BYTE(sDTA, attr);
	result.name = found_name;
}

void DOS_DTA::GetSearchParams(FatAttributeFlags& attr, char* pattern) const
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
        : MemStruct(seg, off),
          extended(false),
          real_pt(pt)
{
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
	dos_memset(real_pt, 0x00, fill);
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

void DOS_FCB::GetAttr(FatAttributeFlags& attr) const
{
	if (extended) {
		attr = mem_readb(pt - 1);
	}
}

void DOS_FCB::SetAttr(FatAttributeFlags attr)
{
	if (extended) {
		mem_writeb(pt - 1, attr._data);
	}
}

void DOS_FCB::SetResult(uint32_t size, uint16_t date, uint16_t time,
                        FatAttributeFlags attr)
{
	mem_writed(pt + 0x1d, size);
	mem_writew(pt + 0x19, date);
	mem_writew(pt + 0x17,time);
	mem_writeb(pt + 0x0c, attr._data);
}

void DOS_SDA::Init()
{
	/* Clear */
	dos_memset(pt, 0x00, sizeof(sSDA));
	SSET_BYTE(sSDA, drive_crit_error, uint8_t(0xff));
}
