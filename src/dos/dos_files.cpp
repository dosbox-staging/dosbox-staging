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

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "dosbox.h"
#include "mem.h"
#include "cpu.h"
#include "dos_inc.h"
#include "drives.h"
#include "cross.h"

#define DOS_FILESTART 4


DOS_File * Files[DOS_FILES];
DOS_Drive * Drives[DOS_DRIVES];
static Bit8u CurrentDrive=2;			//Init on C:


Bit8u DOS_GetDefaultDrive(void) {
	return CurrentDrive;
}

void DOS_SetDefaultDrive(Bit8u drive) {
	CurrentDrive=drive;
}

bool DOS_MakeName(char * name,char * fullname,Bit8u * drive) {
//TODO Hope this is ok :)
	char upname[DOS_PATHLENGTH];
	Bit32u r=0;Bit32u w=0;Bit32u namestart=0;
	bool hasdrive=false;
	*drive=CurrentDrive;
	char tempdir[128];
//TODO Maybe check for illegal characters
	while (name[r]!=0 && (r<DOS_PATHLENGTH)) {
		Bit8u c=name[r++];
		if ((c>='a') && (c<='z')) {upname[w++]=c-32;continue;}
		if ((c>='A') && (c<='Z')) {upname[w++]=c;continue;}
		if ((c>='0') && (c<='9')) {upname[w++]=c;continue;}
		switch (c) {
		case ':':
			if (hasdrive) { DOS_SetError(DOSERR_PATH_NOT_FOUND);return false; }
			else hasdrive=true;
			if ((upname[0]>='A') && (upname[0]<='Z')) {
				*drive=upname[0]-'A';
				w=0;
			} else {
				DOS_SetError(DOSERR_PATH_NOT_FOUND);return false;
			}
			break;
		case '/':
			upname[w++]='\\';
			break;
		case ' ':
			break;
		case '\\':	case '$':	case '#':	case '@':	case '(':	case ')':
		case '!':	case '%':	case '{':	case '}':	case '`':	case '~':
		case '_':	case '-':	case '.':	case '*':	case '?':	case '&':
			upname[w++]=c;
			break;
		default:
			DOS_SetError(DOSERR_PATH_NOT_FOUND);return false;
			break;
		}
	}
	upname[w]=0;
	/* This should get us an upcase filename and no incorrect chars */
	/* Now parse the new file name to make the final filename */
	if ((*drive>=26)) {
		DOS_SetError(DOSERR_INVALID_DRIVE);return false;	
	};
	if (!Drives[*drive]) {
		DOS_SetError(DOSERR_INVALID_DRIVE);return false;	
	};
	if (upname[0]!='\\') strcpy(fullname,Drives[*drive]->curdir);
	else fullname[0]=0;
	Bit32u lastdir=0;Bit32u t=0;
	while (fullname[t]!=0) {
	if ((fullname[t]=='\\') && (fullname[t+1]!=0))lastdir=t;
		t++;
	};
	r=0;w=0;
	tempdir[0]=0;
	bool stop=false;
	while (!stop) {
		if (upname[r]==0) stop=true;
		if ((upname[r]=='\\') || (upname[r]==0)){
			tempdir[w]=0;
			if (tempdir[0]==0) { w=0;r++;continue;}
			if (strcmp(tempdir,".")==0) {
				tempdir[0]=0;			
				w=0;r++;
				continue;
			}
			if (strcmp(tempdir,"..")==0) {
				fullname[lastdir]=0;
				Bit32u t=0;lastdir=0;
				while (fullname[t]!=0) {
					if ((fullname[t]=='\\') && (fullname[t+1]!=0))lastdir=t;
					t++;
				}
				tempdir[0]=0;
				w=0;r++;
				continue;
			}
			lastdir=strlen(fullname);
			//TODO Maybe another check for correct type because of .... stuff
			if (lastdir!=0) strcat(fullname,"\\");
			strcat(fullname,tempdir);

			tempdir[0]=0;
			w=0;r++;
			continue;
		}
		tempdir[w++]=upname[r++];
	}
	return true;	
}

bool DOS_GetCurrentDir(Bit8u drive,Bit8u * buffer) {
	if (drive==0) drive=DOS_GetDefaultDrive();
	else drive--;
	if ((drive>DOS_DRIVES) || (!Drives[drive])) {
		DOS_SetError(DOSERR_INVALID_DRIVE);
		return false;
	}
	strcpy((char *) buffer,Drives[drive]->curdir);
	return true;
}

bool DOS_ChangeDir(char * dir) {
	Bit8u drive;char fulldir[DOS_PATHLENGTH];
	if (!DOS_MakeName(dir,fulldir,&drive)) return false;
	if (Drives[drive]->TestDir(fulldir)) {
		strcpy(Drives[drive]->curdir,fulldir);
		return true;
	} else {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
	}
	return false;
}

bool DOS_MakeDir(char * dir) {
	Bit8u drive;char fulldir[DOS_PATHLENGTH];
	if (!DOS_MakeName(dir,fulldir,&drive)) return false;
	return Drives[drive]->MakeDir(fulldir);
}

bool DOS_RemoveDir(char * dir) {
	Bit8u drive;char fulldir[DOS_PATHLENGTH];
	if (!DOS_MakeName(dir,fulldir,&drive)) return false;
	return Drives[drive]->RemoveDir(fulldir);
}

bool DOS_Rename(char * oldname,char * newname) {
	Bit8u driveold;char fullold[DOS_PATHLENGTH];
	Bit8u drivenew;char fullnew[DOS_PATHLENGTH];
	if (!DOS_MakeName(oldname,fullold,&driveold)) return false;
	if (!DOS_MakeName(newname,fullnew,&drivenew)) return false;
	//TODO Test for different drives maybe
	if (Drives[drivenew]->Rename(fullold,fullnew)) return true;
	DOS_SetError(DOSERR_FILE_NOT_FOUND);
	return false;
};

bool DOS_FindFirst(char * search,Bit16u attr) {
	Bit8u drive;char fullsearch[DOS_PATHLENGTH];
	if (!DOS_MakeName(search,fullsearch,&drive)) return false;
	DTA_FindBlock * dtablock=(DTA_FindBlock *)Real2Host(dos.dta);
	dtablock->sattr=attr | DOS_ATTR_ARCHIVE;
	dtablock->sdrive=drive;
	return Drives[drive]->FindFirst(fullsearch,dtablock);
};

bool DOS_FindNext(void) {
	DTA_FindBlock * dtablock=(DTA_FindBlock *)Real2Host(dos.dta);
	return Drives[dtablock->sdrive]->FindNext(dtablock);
};


bool DOS_ReadFile(Bit16u entry,Bit8u * data,Bit16u * amount) {
	Bit32u handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
//TODO maybe another code :)
/*
	if (!(Files[handle]->flags & OPEN_READ)) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
*/
	Bit16u toread=*amount;
	bool ret=Files[handle]->Read(data,&toread);
	*amount=toread;
	return ret;
};

bool DOS_WriteFile(Bit16u entry,Bit8u * data,Bit16u * amount) {
	Bit32u handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
//TODO maybe another code :)
/*
	if (!(Files[handle]->flags & OPEN_WRITE)) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
*/
	Bit16u towrite=*amount;
	bool ret=Files[handle]->Write(data,&towrite);
	*amount=towrite;
	return ret;
};

bool DOS_SeekFile(Bit16u entry,Bit32u * pos,Bit32u type) {
	Bit32u handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	return Files[handle]->Seek(pos,type);
};

bool DOS_CloseFile(Bit16u entry) {
	Bit32u handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
//TODO Figure this out with devices :)	

	PSP * psp=(PSP *)real_off(dos.psp,0);
	Bit8u * table=Real2Host(psp->file_table);
	table[entry]=0xFF;
	/* Devices won't allow themselves to be closed or killed */
	if (Files[handle]->Close()) {
		delete Files[handle];
		Files[handle]=0;
	}
	return true;
}

bool DOS_CreateFile(char * name,Bit16u attributes,Bit16u * entry) {
	char fullname[DOS_PATHLENGTH];Bit8u drive;
	PSP * psp=(PSP *)real_off(dos.psp,0);
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	/* Check for a free file handle */
	Bit8u handle=DOS_FILES;Bit8u i;
	for (i=0;i<DOS_FILES;i++) {
		if (!Files[i]) {
			handle=i;
			break;
		}
	}
	if (handle==DOS_FILES) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}
	/* We have a position in the main table now find one in the psp table */
	Bit8u * table=Real2Host(psp->file_table);
	*entry=0xff;
	for (i=0;i<psp->max_files;i++) {
		if (table[i]==0xFF) {
			*entry=i;
			break;
		}
	}
	if (*entry==0xff) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}
	bool foundit=Drives[drive]->FileCreate(&Files[handle],fullname,attributes);
	if (foundit) { 
		table[*entry]=handle;
		return true;
	} else {
		return false;
	}
}

bool DOS_OpenFile(char * name,Bit8u flags,Bit16u * entry) {
	/* First check for devices */
	PSP * psp=(PSP *)real_off(dos.psp,0);
	Bit8u handle=DOS_FindDevice((char *)name);
	bool device=false;char fullname[DOS_PATHLENGTH];Bit8u drive;Bit8u i;
	if (handle!=255) {
		device=true;
	} else {
		/* First check if the name is correct */
		if (!DOS_MakeName(name,fullname,&drive)) return false;
		/* Check for a free file handle */
			for (i=0;i<DOS_FILES;i++) {
			if (!Files[i]) {
				handle=i;
				break;
			}
		}
		if (handle==255) {
			DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
			return false;
		}
	}
	/* We have a position in the main table now find one in the psp table */
	Bit8u * table=Real2Host(psp->file_table);
	*entry=0xff;
	for (i=0;i<psp->max_files;i++) {
		if (table[i]==0xFF) {
			*entry=i;
			break;
		}
	}
	if (*entry==0xff) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}
	bool exists=false;
	if (!device) exists=Drives[drive]->FileOpen(&Files[handle],fullname,flags);
	if (exists || device ) { 
		table[*entry]=handle;
		return true;
	} else {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
}

bool DOS_UnlinkFile(char * name) {
	char fullname[DOS_PATHLENGTH];Bit8u drive;
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	return Drives[drive]->FileUnlink(fullname);
}

bool DOS_GetFileAttr(char * name,Bit16u * attr) {
	char fullname[DOS_PATHLENGTH];Bit8u drive;
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	if (Drives[drive]->GetFileAttr(fullname,attr)) {
		return true;
	} else {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
}

bool DOS_Canonicalize(char * name,Bit8u * big) {
//TODO Add Better support for devices and shit but will it be needed i doubt it :) 
	Bit8u drive;
	char fullname[DOS_PATHLENGTH];
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	big[0]=drive+'A';
	big[1]=':';
	big[2]='\\';
	strcpy((char *)&big[3],fullname);
	return true;
};	

bool DOS_GetFreeDiskSpace(Bit8u drive,Bit16u * bytes,Bit16u * sectors,Bit16u * clusters,Bit16u * free) {
	if (drive==0) drive=DOS_GetDefaultDrive();
	else drive--;
	if ((drive>DOS_DRIVES) || (!Drives[drive])) {
		DOS_SetError(DOSERR_INVALID_DRIVE);
		return false;
	}
	return Drives[drive]->FreeSpace(bytes,sectors,clusters,free);
}

bool DOS_DuplicateEntry(Bit16u entry,Bit16u * newentry) {
	Bit8u handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	PSP * psp=(PSP *)real_off(dos.psp,0);
	Bit8u * table=Real2Host(psp->file_table);
	*newentry=0xff;
	for (Bit16u i=0;i<psp->max_files;i++) {
		if (table[i]==0xFF) {
			*newentry=i;
			break;
		}
	}
	if (*newentry==0xff) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}
	table[*newentry]=handle;
	return true;
};


bool DOS_CreateTempFile(char * name,Bit16u * entry) {

/* First add random crap to the end of the name and try to open */
	/* Todo maybe check for euhm existence of the path name */
	char * tempname;
	tempname=name+strlen(name);
	do {
		Bit32u i;
		for (i=0;i<8;i++) {
			tempname[i]=(rand()%26)+'A';
		}
		tempname[8]='.';
		for (i=9;i<12;i++) {
			tempname[i]=(rand()%26)+'A';
		}
		tempname[13]=0;
	} while (!DOS_CreateFile(name,0,entry));
	return true;
}
#if 1
static bool FCB_MakeName (DOS_FCB* fcb, char* outname, Bit8u* outdrive){
	char naam[15];
	Bit8s teller=0;
	Bit8u drive=fcb->Get_drive();
	if(drive!=0){ 
		naam[0]=(drive-1)+'A';
		naam[1]=':';
		naam[2]='\0';}
    else{
		naam[0]='\0';
    };
	char temp[10];
	fcb->Get_filename(temp);
	temp[8]='.';
	temp[9]='\0';
	strcat(naam,temp);
	char ext[4];
	fcb->Get_ext(ext);
	ext[3]='\0';
	strcat(naam,ext);
    return DOS_MakeName(naam,outname, outdrive);
}


bool DOS_FCBOpen(Bit16u seg,Bit16u offset) { 
	DOS_FCB fcb(seg,offset);
	Bit8u drive;
	char fullname[DOS_PATHLENGTH];
	if(!FCB_MakeName (&fcb, fullname, &drive)) return false;
    if(!Drives[drive]->FileExists(fullname)) return false; //not really needed as stat will error.

  struct stat stat_block;
  if(!Drives[drive]->FileStat(fullname, &stat_block)) return false; 
  fcb.Set_filesize((Bit32u)stat_block.st_size);
  Bit16u constant = 0;
  fcb.Set_current_block(constant);
  constant=0x80;
  fcb.Set_record_size(constant);
    struct tm *time;
    if((time=localtime(&stat_block.st_mtime))!=0){

	    constant=(time->tm_hour<<11)+(time->tm_min<<5)+(time->tm_sec/2); /* standard way. */
	    fcb.Set_time(constant);
        constant=((time->tm_year-80)<<9)+((time->tm_mon+1)<<5)+(time->tm_mday);
        fcb.Set_date(constant);
    }
    else{
        constant=6;
	    fcb.Set_time(constant);
        constant=4;
        fcb.Set_date(constant);
    }
  fcb.Set_drive(drive +1);
  return true;
  }

bool DOS_FCBClose(Bit16u seg,Bit16u offset) 
{   DOS_FCB fcb(seg,offset);
	Bit8u drive;
	char fullname[DOS_PATHLENGTH];
	if(!FCB_MakeName (&fcb, fullname, &drive)) return false;
    if(!Drives[drive]->FileExists(fullname)) return false;
	
	return true;}

bool DOS_FCBFindFirst(Bit16u seg,Bit16u offset)
{
	DOS_FCB* fcb = new DOS_FCB(seg,offset);
	Bit8u drive;
	Bitu i;
	char fullname[DOS_PATHLENGTH];
	FCB_MakeName (fcb, fullname, &drive);
	DTA_FindBlock * dtablock=(DTA_FindBlock *)Real2Host(dos.dta);
	if(Drives[drive]->FindFirst(fullname,dtablock)==false) return false;
  
	char naam[8];
	char ext[3];
	char * point=strrchr(dtablock->name,'.');
	if(point==NULL|| *(point+1)=='\0') {
		ext[0]=' ';
		ext[1]=' ';
		ext[2]=' ';
	}else{
		strcpy(ext,(point+1));
		i=strlen((point+1));
		while(i!=3) {ext[i]=' ';i++;}
	}

	if(point!=NULL) *point='\0';
  
	strcpy (naam,dtablock->name);
	i=strlen(dtablock->name);
	while(i!=8) {naam[i]=' '; i++;}
	DOS_FCB* fcbout= new DOS_FCB((PhysPt)Real2Host(dos.dta));
	fcbout->Set_drive(drive +1);
	fcbout->Set_filename(naam);
	fcbout->Set_ext(ext);
	return true;

}
bool DOS_FCBFindNext(Bit16u seg,Bit16u offset)
{
  DOS_FCB* fcb = new DOS_FCB(seg,offset);
  Bit8u drive;
  Bitu i;
  char fullname[DOS_PATHLENGTH];
  FCB_MakeName (fcb, fullname, &drive);
  DTA_FindBlock * dtablock=(DTA_FindBlock *)Real2Host(dos.dta);
  if(Drives[dtablock->sdrive]->FindNext(dtablock)==false) return false;
  char naam[8];
  char ext[3];
  char * point=strrchr(dtablock->name,'.');
  if(point==NULL|| *(point+1)=='\0') {
	ext[0]=' ';
	ext[1]=' ';
	ext[2]=' ';
	}else
  {
		strcpy(ext,point+1);
		i=strlen(point+1); 
		while(i!=3) {ext[i]=' ';i++;}
  }

  if(point!=NULL) *point='\0';
  
  strcpy (naam,dtablock->name);
  i=strlen(dtablock->name);
  while(i!=8) {naam[i]=' '; i++;}
  DOS_FCB* fcbout= new DOS_FCB((PhysPt)Real2Host(dos.dta));
  fcbout->Set_drive(drive +1);
  fcbout->Set_filename(naam);
  fcbout->Set_ext(ext);
  return true;
  }

#endif

bool DOS_FileExists(char * name) {

	char fullname[DOS_PATHLENGTH];Bit8u drive;
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	return Drives[drive]->FileExists(fullname);
}


bool DOS_SetDrive(Bit8u drive) {
	if (Drives[drive]) {
		DOS_SetDefaultDrive(drive);
		return true;

	} else {
		return false;
	}
};


void DOS_SetupFiles (void) {
	/* Setup the File Handles */
	Bit32u i;
	for (i=0;i<DOS_FILES;i++) {
		Files[i]=0;
	}
	/* Setup the Virtual Disk System */
	for (i=0;i<DOS_DRIVES;i++) {
		Drives[i]=0;
	}
	Drives[25]=new Virtual_Drive();
}





