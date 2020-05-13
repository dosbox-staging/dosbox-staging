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

// Uncomment to enable file-open diagnostic messages
// #define DEBUG 1

#include "drives.h"

#include <cstdio>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "dos_inc.h"
#include "dos_mscdex.h"
#include "support.h"
#include "cross.h"
#include "inout.h"

bool localDrive::FileCreate(DOS_File * * file,char * name,Bit16u /*attributes*/) {
//TODO Maybe care for attributes but not likely
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	char* temp_name = dirCache.GetExpandName(newname); //Can only be used in till a new drive_cache action is preformed */
	/* Test if file exists (so we need to truncate it). don't add to dirCache then */
	bool existing_file = false;
	
	FILE * test = fopen_wrap(temp_name,"rb+");
	if (test) {
		fclose(test);
		existing_file=true;

	}
	
	FILE * hand = fopen_wrap(temp_name,"wb+");
	if (!hand) {
		LOG_MSG("Warning: file creation failed: %s",newname);
		return false;
	}
   
	if (!existing_file) dirCache.AddEntry(newname, true);
	/* Make the 16 bit device information */
	*file=new localFile(name,hand);
	(*file)->flags=OPEN_READWRITE;

	return true;
}

bool localDrive::IsFirstEncounter(const std::string& filename) {
	const auto ret = write_protected_files.insert(filename);
	const bool was_inserted = ret.second;
	return was_inserted;
}

bool localDrive::FileOpen(DOS_File** file, char * name, Bit32u flags) {
	const char* type = nullptr;
	switch (flags&0xf) {
	case OPEN_READ:        type = "rb" ; break;
	case OPEN_WRITE:       type = "rb+"; break;
	case OPEN_READWRITE:   type = "rb+"; break;
	case OPEN_READ_NO_MOD: type = "rb" ; break; //No modification of dates. LORD4.07 uses this
	default:
		DOS_SetError(DOSERR_ACCESS_CODE_INVALID);
		return false;
	}
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

	//Flush the buffer of handles for the same file. (Betrayal in Antara)
	Bit8u i,drive=DOS_DRIVES;
	localFile *lfp = nullptr;
	for (i = 0; i < DOS_DRIVES; i++) {
		if (Drives[i] == this) {
			drive = i;
			break;
		}
	}
	for (i = 0; i < DOS_FILES; i++) {
		if (Files[i] && Files[i]->IsOpen() && Files[i]->GetDrive()==drive && Files[i]->IsName(name)) {
			lfp=dynamic_cast<localFile*>(Files[i]);
			if (lfp) lfp->Flush();
		}
	}

	FILE* fhandle = fopen(newname, type);

#ifdef DEBUG
	std::string open_msg;
	std::string flags_str;
	switch (flags & 0xf) {
		case OPEN_READ:        flags_str = "R";  break;
		case OPEN_WRITE:       flags_str = "W";  break;
		case OPEN_READWRITE:   flags_str = "RW"; break;
		case OPEN_READ_NO_MOD: flags_str = "RN"; break;
		default:               flags_str = "--";
	}
#endif

	// If we couldn't open the file, then it's possibile that
	// the file is simply write-protected and the flags requested
	// RW access.  So check if this is the case:
	if (!fhandle && flags & (OPEN_READWRITE | OPEN_WRITE)) {
		// If yes, check if the file can be opened with Read-only access:
		fhandle = fopen_wrap(newname, "rb");
		if (fhandle) {

#ifdef DEBUG
			open_msg = "wanted writes but opened read-only";
#else
			// Inform the user that the file is being protected against modification.
			// If the DOS program /really/ needs to write to the file, it will
			// crash/exit and this will be one of the last messages on the screen,
			// so the user can decide to un-write-protect the file if they wish.
			// We only print one message per file to eliminate redundant messaging.
			if (IsFirstEncounter(newname)) {
				// For brevity and clarity to the user, we show just the
				// filename instead of the more cluttered absolute path.
				LOG_MSG("FILESYSTEM: protected from modification: %s",
				        get_basename(newname).c_str());
			}
#endif
		}

#ifdef DEBUG
		else {
			open_msg += "failed desired and with read-only";
		}
#endif
	}

#ifdef DEBUG
	else {
		open_msg = "succeeded with desired flags";
	}
	LOG_MSG("FILESYSTEM: flags=%2s, %-12s %s",
	        flags_str.c_str(),
	        get_basename(newname).c_str(),
	        open_msg.c_str());
#endif

	if (fhandle) {
		*file = new localFile(name, fhandle);
		(*file)->flags = flags;  // for the inheritance flag and maybe check for others.
	} else {
		// Otherwise we really can't open the file.
		DOS_SetError(DOSERR_INVALID_HANDLE);
	}
	return (fhandle != NULL);
}

FILE * localDrive::GetSystemFilePtr(char const * const name, char const * const type) {

	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

	return fopen_wrap(newname,type);
}

bool localDrive::GetSystemFilename(char *sysName, char const * const dosName) {

	strcpy(sysName, basedir);
	strcat(sysName, dosName);
	CROSS_FILENAME(sysName);
	dirCache.ExpandName(sysName);
	return true;
}

bool localDrive::FileUnlink(char * name) {
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	char *fullname = dirCache.GetExpandName(newname);
	if (unlink(fullname)) {
		//Unlink failed for some reason try finding it.
		struct stat buffer;
		if (stat(fullname,&buffer)) return false; // File not found.

		FILE* file_writable = fopen_wrap(fullname,"rb+");
		if (!file_writable) return false; //No acces ? ERROR MESSAGE NOT SET. FIXME ?
		fclose(file_writable);

		//File exists and can technically be deleted, nevertheless it failed.
		//This means that the file is probably open by some process.
		//See if We have it open.
		bool found_file = false;
		for (Bitu i = 0;i < DOS_FILES;i++) {
			if (Files[i] && Files[i]->IsName(name)) {
				Bitu max = DOS_FILES;
				while (Files[i]->IsOpen() && max--) {
					Files[i]->Close();
					if (Files[i]->RemoveRef()<=0) break;
				}
				found_file=true;
			}
		}
		if (!found_file) return false;
		if (!unlink(fullname)) {
			dirCache.DeleteEntry(newname);
			return true;
		}
		return false;
	} else {
		dirCache.DeleteEntry(newname);
		return true;
	}
}

bool localDrive::FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst) {
	char tempDir[CROSS_LEN];
	safe_strcpy(tempDir, basedir);
	safe_strcat(tempDir, _dir);
	CROSS_FILENAME(tempDir);

	if (allocation.mediaid==0xF0) {
		EmptyCache(); //rescan floppie-content on each findfirst
	}
    
	char end[2]={CROSS_FILESPLIT,0};
	if (tempDir[strlen(tempDir) - 1] != CROSS_FILESPLIT)
		safe_strcat(tempDir, end);

	Bit16u id;
	if (!dirCache.FindFirst(tempDir,id)) {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	safe_strcpy(srchInfo[id].srch_dir, tempDir);
	dta.SetDirID(id);
	
	Bit8u sAttr;
	dta.GetSearchParams(sAttr,tempDir);

	if (this->isRemote() && this->isRemovable()) {
		// cdroms behave a bit different than regular drives
		if (sAttr == DOS_ATTR_VOLUME) {
			dta.SetResult(dirCache.GetLabel(),0,0,0,DOS_ATTR_VOLUME);
			return true;
		}
	} else {
		if (sAttr == DOS_ATTR_VOLUME) {
			if (strlen(dirCache.GetLabel()) == 0) {
//				LOG(LOG_DOSMISC,LOG_ERROR)("DRIVELABEL REQUESTED: none present, returned  NOLABEL");
//				dta.SetResult("NO_LABEL",0,0,0,DOS_ATTR_VOLUME);
//				return true;
				DOS_SetError(DOSERR_NO_MORE_FILES);
				return false;
			}
			dta.SetResult(dirCache.GetLabel(),0,0,0,DOS_ATTR_VOLUME);
			return true;
		} else if ((sAttr & DOS_ATTR_VOLUME)  && (*_dir == 0) && !fcb_findfirst) { 
		//should check for a valid leading directory instead of 0
		//exists==true if the volume label matches the searchmask and the path is valid
			if (WildFileCmp(dirCache.GetLabel(),tempDir)) {
				dta.SetResult(dirCache.GetLabel(),0,0,0,DOS_ATTR_VOLUME);
				return true;
			}
		}
	}
	return FindNext(dta);
}

bool localDrive::FindNext(DOS_DTA & dta) {

	char * dir_ent;
	struct stat stat_block;
	char full_name[CROSS_LEN];
	char dir_entcopy[CROSS_LEN];

	Bit8u srch_attr;char srch_pattern[DOS_NAMELENGTH_ASCII];
	Bit8u find_attr;

	dta.GetSearchParams(srch_attr,srch_pattern);
	Bit16u id = dta.GetDirID();

again:
	if (!dirCache.FindNext(id,dir_ent)) {
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;
	}
	if (!WildFileCmp(dir_ent,srch_pattern)) goto again;

	safe_strcpy(full_name, srchInfo[id].srch_dir);
	safe_strcat(full_name, dir_ent);

	//GetExpandName might indirectly destroy dir_ent (by caching in a new directory 
	//and due to its design dir_ent might be lost.)
	//Copying dir_ent first
	safe_strcpy(dir_entcopy, dir_ent);
	if (stat(dirCache.GetExpandName(full_name),&stat_block)!=0) { 
		goto again;//No symlinks and such
	}	

	if (stat_block.st_mode & S_IFDIR) find_attr=DOS_ATTR_DIRECTORY;
	else find_attr=DOS_ATTR_ARCHIVE;
 	if (~srch_attr & find_attr & (DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM)) goto again;
	
	/*file is okay, setup everything to be copied in DTA Block */
	char find_name[DOS_NAMELENGTH_ASCII];Bit16u find_date,find_time;Bit32u find_size;

	if (strlen(dir_entcopy)<DOS_NAMELENGTH_ASCII) {
		safe_strcpy(find_name, dir_entcopy);
		upcase(find_name);
	} 

	find_size=(Bit32u) stat_block.st_size;
	struct tm *time;
	if ((time=localtime(&stat_block.st_mtime))!=0) {
		find_date=DOS_PackDate((Bit16u)(time->tm_year+1900),(Bit16u)(time->tm_mon+1),(Bit16u)time->tm_mday);
		find_time=DOS_PackTime((Bit16u)time->tm_hour,(Bit16u)time->tm_min,(Bit16u)time->tm_sec);
	} else {
		find_time=6; 
		find_date=4;
	}
	dta.SetResult(find_name,find_size,find_date,find_time,find_attr);
	return true;
}

bool localDrive::GetFileAttr(char * name,Bit16u * attr) {
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

	struct stat status;
	if (stat(newname,&status)==0) {
		*attr=DOS_ATTR_ARCHIVE;
		if (status.st_mode & S_IFDIR) *attr|=DOS_ATTR_DIRECTORY;
		return true;
	}
	*attr=0;
	return false; 
}

bool localDrive::MakeDir(char * dir) {
	char newdir[CROSS_LEN];
	safe_strcpy(newdir, basedir);
	safe_strcat(newdir, dir);
	CROSS_FILENAME(newdir);
#if defined (WIN32)						/* MS Visual C++ */
	int temp=mkdir(dirCache.GetExpandName(newdir));
#else
	int temp=mkdir(dirCache.GetExpandName(newdir),0700);
#endif
	if (temp==0) dirCache.CacheOut(newdir,true);

	return (temp==0);// || ((temp!=0) && (errno==EEXIST));
}

bool localDrive::RemoveDir(char * dir) {
	char newdir[CROSS_LEN];
	safe_strcpy(newdir, basedir);
	safe_strcat(newdir, dir);
	CROSS_FILENAME(newdir);
	int temp=rmdir(dirCache.GetExpandName(newdir));
	if (temp==0) dirCache.DeleteEntry(newdir,true);
	return (temp==0);
}

bool localDrive::TestDir(char * dir) {
	char newdir[CROSS_LEN];
	safe_strcpy(newdir, basedir);
	safe_strcat(newdir, dir);
	CROSS_FILENAME(newdir);
	dirCache.ExpandName(newdir);
	// Skip directory test, if "\"
	size_t len = strlen(newdir);
	if (len && (newdir[len-1]!='\\')) {
		// It has to be a directory !
		struct stat test;
		if (stat(newdir,&test))			return false;
		if ((test.st_mode & S_IFDIR)==0)	return false;
	};
	int temp=access(newdir,F_OK);
	return (temp==0);
}

bool localDrive::Rename(char * oldname,char * newname) {
	char newold[CROSS_LEN];
	safe_strcpy(newold, basedir);
	safe_strcat(newold, oldname);
	CROSS_FILENAME(newold);
	dirCache.ExpandName(newold);
	
	char newnew[CROSS_LEN];
	safe_strcpy(newnew, basedir);
	safe_strcat(newnew, newname);
	CROSS_FILENAME(newnew);
	int temp=rename(newold,dirCache.GetExpandName(newnew));
	if (temp==0) dirCache.CacheOut(newnew);
	return (temp==0);

}

bool localDrive::AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters) {
	*_bytes_sector=allocation.bytes_sector;
	*_sectors_cluster=allocation.sectors_cluster;
	*_total_clusters=allocation.total_clusters;
	*_free_clusters=allocation.free_clusters;
	return true;
}

bool localDrive::FileExists(const char* name) {
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	struct stat temp_stat;
	if (stat(newname,&temp_stat)!=0) return false;
	if (temp_stat.st_mode & S_IFDIR) return false;
	return true;
}

bool localDrive::FileStat(const char* name, FileStat_Block * const stat_block) {
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	struct stat temp_stat;
	if (stat(newname,&temp_stat)!=0) return false;
	/* Convert the stat to a FileStat */
	struct tm *time;
	if ((time=localtime(&temp_stat.st_mtime))!=0) {
		stat_block->time=DOS_PackTime((Bit16u)time->tm_hour,(Bit16u)time->tm_min,(Bit16u)time->tm_sec);
		stat_block->date=DOS_PackDate((Bit16u)(time->tm_year+1900),(Bit16u)(time->tm_mon+1),(Bit16u)time->tm_mday);
	} else {

	}
	stat_block->size=(Bit32u)temp_stat.st_size;
	return true;
}


Bit8u localDrive::GetMediaByte(void) {
	return allocation.mediaid;
}

bool localDrive::isRemote(void) {
	return false;
}

bool localDrive::isRemovable(void) {
	return false;
}

Bits localDrive::UnMount(void) { 
	delete this;
	return 0; 
}

localDrive::localDrive(const char * startdir,
                       Bit16u _bytes_sector,
                       Bit8u _sectors_cluster,
                       Bit16u _total_clusters,
                       Bit16u _free_clusters,
                       Bit8u _mediaid)
	: write_protected_files{},
	  allocation{_bytes_sector,
	             _sectors_cluster,
	             _total_clusters,
	             _free_clusters,
	             _mediaid}
{
	safe_strcpy(basedir, startdir);
	sprintf(info,"local directory %s",startdir);
	dirCache.SetBaseDir(basedir);
}


//TODO Maybe use fflush, but that seemed to fuck up in visual c
bool localFile::Read(Bit8u * data,Bit16u * size) {
	if ((this->flags & 0xf) == OPEN_WRITE) {	// check if file opened in write-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	if (last_action==WRITE) fseek(fhandle,ftell(fhandle),SEEK_SET);
	last_action=READ;
	*size=(Bit16u)fread(data,1,*size,fhandle);
	/* Fake harddrive motion. Inspector Gadget with soundblaster compatible */
	/* Same for Igor */
	/* hardrive motion => unmask irq 2. Only do it when it's masked as unmasking is realitively heavy to emulate */
	Bit8u mask = IO_Read(0x21);
	if (mask & 0x4) IO_Write(0x21,mask&0xfb);
	return true;
}

bool localFile::Write(Bit8u * data,Bit16u * size) {
	Bit32u lastflags = this->flags & 0xf;
	if (lastflags == OPEN_READ || lastflags == OPEN_READ_NO_MOD) {	// check if file opened in read-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	if (last_action==READ) fseek(fhandle,ftell(fhandle),SEEK_SET);
	last_action=WRITE;
	if (*size == 0) {
		return !ftruncate(cross_fileno(fhandle), ftell(fhandle));
	} else {
		*size = (Bit16u)fwrite(data, 1, *size, fhandle);
		return true;
	}
}

bool localFile::Seek(Bit32u * pos,Bit32u type) {
	int seektype;
	switch (type) {
	case DOS_SEEK_SET:seektype=SEEK_SET;break;
	case DOS_SEEK_CUR:seektype=SEEK_CUR;break;
	case DOS_SEEK_END:seektype=SEEK_END;break;
	default:
	//TODO Give some doserrorcode;
		return false;//ERROR
	}
	int ret=fseek(fhandle,*reinterpret_cast<Bit32s*>(pos),seektype);
	if (ret!=0) {
		// Out of file range, pretend everythings ok 
		// and move file pointer top end of file... ?! (Black Thorne)
		fseek(fhandle,0,SEEK_END);
	};
#if 0
	fpos_t temppos;
	fgetpos(fhandle,&temppos);
	Bit32u * fake_pos=(Bit32u*)&temppos;
	*pos=*fake_pos;
#endif
	*pos=(Bit32u)ftell(fhandle);
	last_action=NONE;
	return true;
}

bool localFile::Close() {
	// only close if one reference left
	if (refCtr==1) {
		if (fhandle) fclose(fhandle);
		fhandle = 0;
		open = false;
	};
	return true;
}

Bit16u localFile::GetInformation(void) {
	return read_only_medium?0x40:0;
}
	

localFile::localFile(const char* _name, FILE * handle)
	: fhandle(handle),
	  read_only_medium(false),
	  last_action(NONE)
{
	open=true;
	UpdateDateTimeFromHost();

	attr=DOS_ATTR_ARCHIVE;

	SetName(_name);
}

bool localFile::UpdateDateTimeFromHost(void) {
	if (!open) return false;
	struct stat temp_stat;
	fstat(cross_fileno(fhandle), &temp_stat);
	struct tm * ltime;
	if ((ltime=localtime(&temp_stat.st_mtime))!=0) {
		time=DOS_PackTime((Bit16u)ltime->tm_hour,(Bit16u)ltime->tm_min,(Bit16u)ltime->tm_sec);
		date=DOS_PackDate((Bit16u)(ltime->tm_year+1900),(Bit16u)(ltime->tm_mon+1),(Bit16u)ltime->tm_mday);
	} else {
		time=1;date=1;
	}
	return true;
}

void localFile::Flush(void) {
	if (last_action==WRITE) {
		fseek(fhandle,ftell(fhandle),SEEK_SET);
		last_action=NONE;
	}
}


// ********************************************
// CDROM DRIVE
// ********************************************

cdromDrive::cdromDrive(const char _driveLetter,
                       const char * startdir,
                       Bit16u _bytes_sector,
                       Bit8u _sectors_cluster,
                       Bit16u _total_clusters,
                       Bit16u _free_clusters,
                       Bit8u _mediaid,
                       int& error)
	: localDrive(startdir,
	             _bytes_sector,
	             _sectors_cluster,
	             _total_clusters,
	             _free_clusters,
	             _mediaid),
	  subUnit(0),
	  driveLetter(_driveLetter)
{
	// Init mscdex
	error = MSCDEX_AddDrive(driveLetter,startdir,subUnit);
	safe_strcpy(info, "CDRom ");
	safe_strcat(info, startdir);
	// Get Volume Label
	char name[32];
	if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
}

bool cdromDrive::FileOpen(DOS_File * * file,char * name,Bit32u flags) {
	if ((flags&0xf)==OPEN_READWRITE) {
		flags &= ~OPEN_READWRITE;
	} else if ((flags&0xf)==OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	bool success = localDrive::FileOpen(file, name, flags);
	if (success)
		(*file)->SetFlagReadOnlyMedium();
	return success;
}

bool cdromDrive::FileCreate(DOS_File * * /*file*/,char * /*name*/,Bit16u /*attributes*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::FileUnlink(char * /*name*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::RemoveDir(char * /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::MakeDir(char * /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::Rename(char * /*oldname*/,char * /*newname*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::GetFileAttr(char * name,Bit16u * attr) {
	bool result = localDrive::GetFileAttr(name,attr);
	if (result) *attr |= DOS_ATTR_READ_ONLY;
	return result;
}

bool cdromDrive::FindFirst(char * _dir,DOS_DTA & dta,bool /*fcb_findfirst*/) {
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	return localDrive::FindFirst(_dir,dta);
}

void cdromDrive::SetDir(const char* path) {
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	localDrive::SetDir(path);
}

bool cdromDrive::isRemote(void) {
	return true;
}

bool cdromDrive::isRemovable(void) {
	return true;
}

Bits cdromDrive::UnMount(void) {
	if (MSCDEX_RemoveDrive(driveLetter)) {
		delete this;
		return 0;
	}
	return 2;
}
