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
#include "dos_system.h"
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
	Bit16u info;
};


bool localDrive:: FileCreate(DOS_File * * file,char * name,Bit16u attributes) {
//TODO Maybe care for attributes but not likely
	char newname[512];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	FILE * hand=fopen(newname,"wb+");
	if (!hand) return false;
	/* Make the 16 bit device information */
	*file=new localFile(hand,0x202);
	return true;
};

bool localDrive::FileOpen(DOS_File * * file,char * name,Bit32u flags) {
	char * type;
	switch (flags) {
	case OPEN_READ:type="rb";break;
	case OPEN_WRITE:type="rb+";break;
	case OPEN_READWRITE:type="rb+";break;
	default:
//TODO FIX IT
		type="rb+";
//		return false;

	};
	char newname[512];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	FILE * hand=fopen(newname,type);
	Bit32u err=errno;
	if (!hand) return false;
	*file=new localFile(hand,0x202);
	return true;
};

bool localDrive::FileUnlink(char * name) {
	char newname[512];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	if (!unlink(newname)) return true;
	return false;
};


bool localDrive::FindFirst(char * search,DTA_FindBlock * dta) {
	//TODO Find some way for lowcase and highcase drives oneday 
	char name[512];
	strcpy(name,basedir);
	strcat(name,search);
	CROSS_FILENAME(name);
	char * last=strrchr(name, CROSS_FILESPLIT);
	*last=0;
	last++;
	/* Check the wildcard string for an extension */
	strcpy(wild_name,last);
	wild_ext=strrchr(wild_name,'.');
	if (wild_ext) {
		*wild_ext++=0;
	}
	strcpy(directory,name);
/* make sure / is last sign */
	if (pdir) closedir(pdir);
	if(directory[(strlen(directory)-1)]!=CROSS_FILESPLIT) strcat(directory, "/");  
	if((pdir=opendir(directory))==NULL) return false;
	return FindNext(dta);
}

bool localDrive::FindNext(DTA_FindBlock * dta) {
	Bit8u tempattr=0;
	struct dirent *tempdata;
	struct stat stat_block;
	char werkbuffer[512];
	
	if(pdir==NULL){
		return false;
	};
	start:
	if((tempdata=readdir(pdir))==NULL) {
		closedir(pdir);
		pdir=NULL;
		return false;
	}
	strcpy(werkbuffer,tempdata->d_name);
	if (wild_ext) {
		char * ext=strrchr(werkbuffer,'.');
		if (!ext) ext="*";
		else *ext++=0;
		if(!wildcmp(wild_ext,ext)) goto start;
	}
	if(!wildcmp(wild_name,werkbuffer)) goto start;
	werkbuffer[0]='\0';
	strcpy(werkbuffer,directory);
	strcat(werkbuffer,tempdata->d_name);
	if(stat(werkbuffer,&stat_block)!=0){
		/*nu is er iets fout!*/ exit(1);
	}
	if(S_ISDIR(stat_block.st_mode)) tempattr=DOS_ATTR_DIRECTORY;
	else tempattr=DOS_ATTR_ARCHIVE;
	if(!(dta->sattr & tempattr)) goto start;
	/*file is oke so filldtablok */
	if(strlen(tempdata->d_name)<=DOS_NAMELENGTH) strcpy(dta->name,upcase(tempdata->d_name));
	dta->attr=tempattr;
	dta->size=(Bit32u) stat_block.st_size;
	struct tm *time;
    if((time=localtime(&stat_block.st_mtime))!=0){
    
	    dta->time=(time->tm_hour<<11)+(time->tm_min<<5)+(time->tm_sec/2); /* standard way. */
	    dta->date=((time->tm_year-80)<<9)+((time->tm_mon+1)<<5)+(time->tm_mday);
    }else {
        dta->time=6; 
        dta->date=4;
    }
	return true;
}

bool localDrive::GetFileAttr(char * name,Bit16u * attr) {
	char newname[512];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
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
	char newdir[512];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);
#if defined (WIN32)						/* MS Visual C++ */
	int temp=mkdir(newdir);
#else
	int temp=mkdir(newdir,0);
#endif
	return (temp==0);
}

bool localDrive::RemoveDir(char * dir) {
	char newdir[512];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);
	int temp=rmdir(newdir);
	return (temp==0);
}

bool localDrive::TestDir(char * dir) {
	char newdir[512];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);
	int temp=access(newdir,F_OK);
	return (temp==0);
}

bool localDrive::Rename(char * oldname,char * newname) {
	char newold[512];
	strcpy(newold,basedir);
	strcat(newold,oldname);
	CROSS_FILENAME(newold);
	char newnew[512];
	strcpy(newnew,basedir);
	strcat(newnew,newname);
	CROSS_FILENAME(newnew);
	int temp=rename(newold,newnew);
	return (temp==0);

};

bool localDrive::FreeSpace(Bit16u * bytes,Bit16u * sectors,Bit16u * clusters,Bit16u * free) {
	/* Always report 100 mb free should be enough */
	/* Total size is always 1 gb */
	*bytes=512;
	*sectors=127;
	*clusters=16513;
	*free=1700;
	return true;
};

bool localDrive::FileExists(const char* name) const {
	char newname[512];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	FILE* Temp=fopen(newname,"rb");
	if(Temp==NULL) return false;
	fclose(Temp);
	return true;
}

bool localDrive::FileStat(const char* name, struct stat* const stat_block) const {
	char newname[512];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	if(stat(newname,stat_block)!=0) return false;
	return true;

}



localDrive::localDrive(char * startdir) {
	strcpy(basedir,startdir);
	sprintf(info,"local directory %s",startdir);
	pdir=NULL;
}


bool localFile::Read(Bit8u * data,Bit16u * size) {
	*size=fread(data,1,*size,fhandle);
	return true;
};

bool localFile::Write(Bit8u * data,Bit16u * size) {
	*size=fwrite(data,1,*size,fhandle);
	return true;
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
}


