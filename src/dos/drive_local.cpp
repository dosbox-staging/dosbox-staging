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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "dosbox.h"
#include "dos_inc.h"
#include "drives.h"
#include "support.h"
#include "cross.h"


class localFile : public DOS_File {
public:
	localFile(FILE * handle,Bit16u devinfo);
	bool Read(Bit8u * data,Bit16u * size);
	bool Write(Bit8u * data,Bit16u * size);
	bool Seek(Bit32u * pos,Bit32u type);
	bool Close();
	Bit16u GetInformation(void);
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
	if (!hand) return false;
	dirCache.AddEntry(newname);
	/* Make the 16 bit device information */
	*file=new localFile(hand,0x202);
	return true;
};

bool localDrive::FileOpen(DOS_File * * file,char * name,Bit32u flags) {
	char * type;
	switch (flags) {
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
	Bit32u err=errno;
	if (!hand) return false;
	*file=new localFile(hand,0x202);
//	(*file)->SetFileName(newname);
	return true;
};

bool localDrive::FileUnlink(char * name) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	if (!unlink(dirCache.GetExpandName(newname))) {
		dirCache.CacheOut(newname);
		return true;
	};
	return false;
};


bool localDrive::FindFirst(char * _dir,DOS_DTA & dta) {
	
	strcpy(srch_dir,basedir);
	strcat(srch_dir,_dir);
	CROSS_FILENAME(srch_dir);

	char end[2]={CROSS_FILESPLIT,0};
	if (srch_dir[strlen(srch_dir)-1]!=CROSS_FILESPLIT) strcat(srch_dir,end);
	
	if (!dirCache.OpenDir(srch_dir)) return false;
	return FindNext(dta);
}


bool localDrive::FindNext(DOS_DTA & dta) {
	
	struct dirent dir_ent;
	struct stat stat_block;

	Bit8u srch_attr;char srch_pattern[DOS_NAMELENGTH_ASCII];
	Bit8u find_attr;

	dta.GetSearchParams(srch_attr,srch_pattern);
	
again:
	if (!dirCache.ReadDir(&dir_ent,&stat_block)) return false;

	if(!WildFileCmp(dir_ent.d_name,srch_pattern)) goto again;

	if(S_ISDIR(stat_block.st_mode)) find_attr=DOS_ATTR_DIRECTORY;
	else find_attr=DOS_ATTR_ARCHIVE;
 	if (~srch_attr & find_attr & (DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM)) goto again;

	if(S_ISDIR(stat_block.st_mode)) find_attr=DOS_ATTR_DIRECTORY;
	else find_attr=DOS_ATTR_ARCHIVE;
 	if (~srch_attr & find_attr & (DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM)) goto again;
	
	/*file is okay, setup everything to be copied in DTA Block */
	char find_name[DOS_NAMELENGTH_ASCII];Bit16u find_date,find_time;Bit32u find_size;

	if(strlen(dir_ent.d_name)<DOS_NAMELENGTH_ASCII){
		strcpy(find_name,dir_ent.d_name);
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
	FILE * hand=fopen(newname,"rb");
	if (hand) {
		fclose(hand);
		*attr=DOS_ATTR_ARCHIVE;
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
	if (temp==0) dirCache.CacheOut(newdir,true);
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
	char newnew[CROSS_LEN];
	strcpy(newnew,basedir);
	strcat(newnew,newname);
	CROSS_FILENAME(newnew);
	int temp=rename(dirCache.GetExpandName(newold),dirCache.GetExpandName(newnew));
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
		stat_block->date=DOS_PackDate(time->tm_year,time->tm_mon,time->tm_mday);
	} else {

	}
	stat_block->size=(Bit32u)temp_stat.st_size;
	return true;
}


Bit8u localDrive::GetMediaByte(void) {
	return allocation.mediaid;
}

localDrive::localDrive(const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid) {
	strcpy(basedir,startdir);
	sprintf(info,"local directory %s",startdir);
	srch_opendir=NULL;
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
	fpos_t temppos;
	int ret=fseek(fhandle,*pos,seektype);
	fgetpos(fhandle,&temppos);
//TODO Hope we don't encouter files with 64 bits size
	Bit32u * fake_pos=(Bit32u*)&temppos;
	*pos=*fake_pos;
	last_action=NONE;
	return true;
}

bool localFile::Close() {
	
	fclose(fhandle);
	return true;
}

Bit16u localFile::GetInformation(void) {
	return info;
}
	

localFile::localFile(FILE * handle,Bit16u devinfo) {
	fhandle=handle;
	info=devinfo;
	struct stat temp_stat;
	fstat(fileno(handle),&temp_stat);
	struct tm * ltime;
	if((ltime=localtime(&temp_stat.st_mtime))!=0) {
		time=DOS_PackTime(ltime->tm_hour,ltime->tm_min,ltime->tm_sec);
		date=DOS_PackDate(ltime->tm_year,ltime->tm_mon,ltime->tm_mday);
	} else {
		time=0;date=0;
	}
	size=(Bit32u)temp_stat.st_size;
	attr=DOS_ATTR_ARCHIVE;
	last_action=NONE;
}


