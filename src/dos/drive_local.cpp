/*
 *  Copyright (C) 2002-2004  The DOSBox Team
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

/* $Id: drive_local.cpp,v 1.49 2004-08-04 09:12:53 qbix79 Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include "dosbox.h"
#include "dos_inc.h"
#include "drives.h"
#include "support.h"
#include "cross.h"


class localFile : public DOS_File {
public:
	localFile(const char* name, FILE * handle,Bit16u devinfo);
	bool Read(Bit8u * data,Bit16u * size);
	bool Write(Bit8u * data,Bit16u * size);
	bool Seek(Bit32u * pos,Bit32u type);
	bool Close();
	Bit16u GetInformation(void);
	bool UpdateDateTimeFromHost(void);   
private:
	FILE * fhandle;
	enum { NONE,READ,WRITE } last_action;
	Bit16u info;
};


bool localDrive::FileCreate(DOS_File * * file,char * name,Bit16u attributes) {
//TODO Maybe care for attributes but not likely
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	FILE * hand=fopen(dirCache.GetExpandName(newname),"wb+");
	if (!hand){
		LOG_MSG("Warning: file creation failed: %s",newname);
		return false;
	}
   
	dirCache.AddEntry(newname, true);
	/* Make the 16 bit device information */
	*file=new localFile(name,hand,0x202);

	return true;
};

bool localDrive::FileOpen(DOS_File * * file,char * name,Bit32u flags) {
	char * type;
	switch (flags &3) {
	case OPEN_READ:type="rb"; break;
	case OPEN_WRITE:type="rb+"; break;
	case OPEN_READWRITE:type="rb+"; break;
	default:
//TODO FIX IT
		type="rb+";
//		return false;

	};
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

	FILE * hand=fopen(newname,type);
//	Bit32u err=errno;
	if (!hand) { 
		if((flags&3) != OPEN_READ) {
			FILE * hmm=fopen(newname,"rb");
			if (hmm) {
				fclose(hmm);
				LOG_MSG("Warning: file %s exists and failed to open in write mode.\nPlease Remove write-protection",newname);
			}
		}
		return false;
	}
   
	*file=new localFile(name,hand,0x202);
	(*file)->flags=flags;  //for the inheritance flag and maybe check for others.
//	(*file)->SetFileName(newname);
	return true;
};

FILE * localDrive::GetSystemFilePtr(char * name, char * type) {

	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

	return fopen(newname,type);
}

bool localDrive::FileUnlink(char * name) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	if (!unlink(dirCache.GetExpandName(newname))) {
		dirCache.DeleteEntry(newname);
		return true;
	};
	return false;
};


bool localDrive::FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst) {

	char tempDir[CROSS_LEN];
	strcpy(tempDir,basedir);
	strcat(tempDir,_dir);
	CROSS_FILENAME(tempDir);

	if (allocation.mediaid==0xF0 ) {
		EmptyCache(); //rescan floppie-content on each findfirst
	}
    
	char end[2]={CROSS_FILESPLIT,0};
	if (tempDir[strlen(tempDir)-1]!=CROSS_FILESPLIT) strcat(tempDir,end);
	
	Bitu id;
	if (!dirCache.FindFirst(tempDir,id))
	{
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	strcpy(srchInfo[id].srch_dir,tempDir);
	dta.SetDirID(id);
	
	Bit8u sAttr;
	dta.GetSearchParams(sAttr,tempDir);
	if ( (sAttr & DOS_ATTR_VOLUME) && ( (*_dir==0) || fcb_findfirst ) ) {
	// Get Volume Label (DOS_ATTR_VOLUME) and only in basedir
	// or it's a fcb findfirst as that always returns label
		if ( strcmp(dirCache.GetLabel(), "") == 0 ) {
			LOG(LOG_DOSMISC,LOG_ERROR)("DRIVELABEL REQUESTED: none present, returned  NOLABEL");
			dta.SetResult("NO_LABEL",0,0,0,DOS_ATTR_VOLUME);
			return true;
		}
	    
		if (WildFileCmp(dirCache.GetLabel(),tempDir)) {
			// Get Volume Label
			dta.SetResult(dirCache.GetLabel(),0,0,0,DOS_ATTR_VOLUME);
			return true;
		}
	}
	return FindNext(dta);
}

bool localDrive::FindNext(DOS_DTA & dta) {

	char * dir_ent;
	struct stat stat_block;
	char full_name[CROSS_LEN];

	Bit8u srch_attr;char srch_pattern[DOS_NAMELENGTH_ASCII];
	Bit8u find_attr;

	dta.GetSearchParams(srch_attr,srch_pattern);
	
	Bitu id = dta.GetDirID();

again:
	if (!dirCache.FindNext(id,dir_ent)) {
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;
	}

	if(!WildFileCmp(dir_ent,srch_pattern)) goto again;

	strcpy(full_name,srchInfo[id].srch_dir);
	strcat(full_name,dir_ent);
	if (stat(dirCache.GetExpandName(full_name),&stat_block)!=0) {
		goto again;
	}	

	if(S_ISDIR(stat_block.st_mode)) find_attr=DOS_ATTR_DIRECTORY;
	else find_attr=DOS_ATTR_ARCHIVE;
 	if (~srch_attr & find_attr & (DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM)) goto again;

	if(S_ISDIR(stat_block.st_mode)) find_attr=DOS_ATTR_DIRECTORY;
	else find_attr=DOS_ATTR_ARCHIVE;
 	if (~srch_attr & find_attr & (DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM)) goto again;
	
	/*file is okay, setup everything to be copied in DTA Block */
	char find_name[DOS_NAMELENGTH_ASCII];Bit16u find_date,find_time;Bit32u find_size;

	if(strlen(dir_ent)<DOS_NAMELENGTH_ASCII){
		strcpy(find_name,dir_ent);
		upcase(find_name);
	} 

	find_size=(Bit32u) stat_block.st_size;
	struct tm *time;
    if((time=localtime(&stat_block.st_mtime))!=0){
		find_date=DOS_PackDate(time->tm_year+1900,time->tm_mon+1,time->tm_mday);
		find_time=DOS_PackTime(time->tm_hour,time->tm_min,time->tm_sec);
    }else {
        find_time=6; 
        find_date=4;
    }
	dta.SetResult(find_name,find_size,find_date,find_time,find_attr);
	return true;
}

bool localDrive::GetFileAttr(char * name,Bit16u * attr) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

	struct stat status;
	if (stat(newname,&status)==0) {
		*attr=DOS_ATTR_ARCHIVE;
		if(status.st_mode & S_IFDIR) *attr|=DOS_ATTR_DIRECTORY;
		return true;
	}
	*attr=0;
	return false; 
};

bool localDrive::MakeDir(char * dir) {
	char newdir[CROSS_LEN];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);
#if defined (WIN32)						/* MS Visual C++ */
	int temp=mkdir(dirCache.GetExpandName(newdir));
#else
	int temp=mkdir(dirCache.GetExpandName(newdir),0700);
#endif
	if (temp==0) dirCache.CacheOut(newdir,true);
	// if dir already exists, return success too.
	return (temp==0) || ((temp!=0) && (errno==EEXIST));
}

bool localDrive::RemoveDir(char * dir) {
	char newdir[CROSS_LEN];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);
	int temp=rmdir(dirCache.GetExpandName(newdir));
	if (temp==0) dirCache.DeleteEntry(newdir,true);
	return (temp==0);
}

bool localDrive::TestDir(char * dir) {
	char newdir[CROSS_LEN];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);
	dirCache.ExpandName(newdir);
	// Skip directory test, if "\"
	Bit16u len = strlen(newdir);
	if ((len>0) && (newdir[len-1]!='\\')) {
		// It has to be a directory !
		struct stat test;
		if (stat(newdir,&test)==-1)			return false;
		if ((test.st_mode & S_IFDIR)==0)	return false;
	};
	int temp=access(newdir,F_OK);
	return (temp==0);
}

bool localDrive::Rename(char * oldname,char * newname) {
	char newold[CROSS_LEN];
	strcpy(newold,basedir);
	strcat(newold,oldname);
	CROSS_FILENAME(newold);
	dirCache.ExpandName(newold);
	
	char newnew[CROSS_LEN];
	strcpy(newnew,basedir);
	strcat(newnew,newname);
	CROSS_FILENAME(newnew);
	int temp=rename(newold,dirCache.GetExpandName(newnew));
	if (temp==0) dirCache.CacheOut(newnew);
	return (temp==0);

};

bool localDrive::AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters) {
	/* Always report 100 mb free should be enough */
	/* Total size is always 1 gb */
	*_bytes_sector=allocation.bytes_sector;
	*_sectors_cluster=allocation.sectors_cluster;
	*_total_clusters=allocation.total_clusters;
	*_free_clusters=allocation.free_clusters;
	return true;
};

bool localDrive::FileExists(const char* name) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	FILE* Temp=fopen(newname,"rb");
	if(Temp==NULL) return false;
	fclose(Temp);
	return true;
}

bool localDrive::FileStat(const char* name, FileStat_Block * const stat_block) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	struct stat temp_stat;
	if(stat(newname,&temp_stat)!=0) return false;
	/* Convert the stat to a FileStat */
	struct tm *time;
	if((time=localtime(&temp_stat.st_mtime))!=0) {
		stat_block->time=DOS_PackTime(time->tm_hour,time->tm_min,time->tm_sec);
		stat_block->date=DOS_PackDate(time->tm_year+1900,time->tm_mon+1,time->tm_mday);
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

localDrive::localDrive(const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid) {
	strcpy(basedir,startdir);
	sprintf(info,"local directory %s",startdir);
	allocation.bytes_sector=_bytes_sector;
	allocation.sectors_cluster=_sectors_cluster;
	allocation.total_clusters=_total_clusters;
	allocation.free_clusters=_free_clusters;
	allocation.mediaid=_mediaid;

	dirCache.SetBaseDir(basedir);
}


//TODO Maybe use fflush, but that seemed to fuck up in visual c
bool localFile::Read(Bit8u * data,Bit16u * size) {
	if (last_action==WRITE) fseek(fhandle,ftell(fhandle),SEEK_SET);
	last_action=READ;
	*size=fread(data,1,*size,fhandle);
	return true;
};

bool localFile::Write(Bit8u * data,Bit16u * size) {
	if (last_action==READ) fseek(fhandle,ftell(fhandle),SEEK_SET);
	last_action=WRITE;
	if(*size==0){  
        return (!ftruncate(fileno(fhandle),ftell(fhandle)));
    }
    else 
    {
		*size=fwrite(data,1,*size,fhandle);
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
	int ret=fseek(fhandle,*pos,seektype);
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
		fclose(fhandle);
		fhandle = 0;
		open = false;
	};
	return true;
}

Bit16u localFile::GetInformation(void) {
	return info;
}
	

localFile::localFile(const char* _name, FILE * handle,Bit16u devinfo) {
	fhandle=handle;
	info=devinfo;
	struct stat temp_stat;
	fstat(fileno(handle),&temp_stat);
	struct tm * ltime;
	if((ltime=localtime(&temp_stat.st_mtime))!=0) {
		time=DOS_PackTime(ltime->tm_hour,ltime->tm_min,ltime->tm_sec);
		date=DOS_PackDate(ltime->tm_year+1900,ltime->tm_mon+1,ltime->tm_mday);
	} else {
		time=1;date=1;
	}
	size=(Bit32u)temp_stat.st_size;
	attr=DOS_ATTR_ARCHIVE;
	last_action=NONE;

	open=true;
	name=0;
	SetName(_name);
}

bool localFile::UpdateDateTimeFromHost(void) {
	if(!open) return false;
	struct stat temp_stat;
	fstat(fileno(fhandle),&temp_stat);
	struct tm * ltime;
	if((ltime=localtime(&temp_stat.st_mtime))!=0) {
		time=DOS_PackTime(ltime->tm_hour,ltime->tm_min,ltime->tm_sec);
		date=DOS_PackDate(ltime->tm_year+1900,ltime->tm_mon+1,ltime->tm_mday);
	} else {
		time=1;date=1;
	}
	return true;
}


// ********************************************
// CDROM DRIVE
// ********************************************

int  MSCDEX_AddDrive(char driveLetter, const char* physicalPath, Bit8u& subUnit);
bool MSCDEX_HasMediaChanged(Bit8u subUnit);
bool MSCDEX_GetVolumeName(Bit8u subUnit, char* name);


cdromDrive::cdromDrive(const char driveLetter, const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid, int& error)
		   :localDrive(startdir,_bytes_sector,_sectors_cluster,_total_clusters,_free_clusters,_mediaid)
{
	// Init mscdex
	error = MSCDEX_AddDrive(driveLetter,startdir,subUnit);
	strcpy(info,"CDRom.");
	// Get Volume Label
	char name[32];
	if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name);
};

bool cdromDrive::FileOpen(DOS_File * * file,char * name,Bit32u flags)
{
	if (flags==OPEN_READWRITE) {
		flags = OPEN_READ;
	} else if (flags==OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	return localDrive::FileOpen(file,name,flags);
};

bool cdromDrive::FileCreate(DOS_File * * file,char * name,Bit16u attributes)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool cdromDrive::FileUnlink(char * name)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool cdromDrive::RemoveDir(char * dir)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool cdromDrive::MakeDir(char * dir)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool cdromDrive::Rename(char * oldname,char * newname)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool cdromDrive::GetFileAttr(char * name,Bit16u * attr)
{
	bool result = localDrive::GetFileAttr(name,attr);
	if (result) *attr |= DOS_ATTR_READ_ONLY;
	return result;
};

bool cdromDrive::FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst)
{
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name);
	}
	return localDrive::FindFirst(_dir,dta);
};

void cdromDrive::SetDir(const char* path)
{
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name);
	}
	localDrive::SetDir(path);
};

bool cdromDrive::isRemote(void) {
	return true;
}
