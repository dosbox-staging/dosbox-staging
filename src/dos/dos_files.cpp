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
#include <ctype.h>

#include "dosbox.h"
#include "mem.h"
#include "cpu.h"
#include "dos_inc.h"
#include "drives.h"
#include "cross.h"

#define DOS_FILESTART 4

#define FCB_SUCCESS     0
#define FCB_READ_NODATA	1
#define FCB_READ_PARTIAL 3
#define FCB_ERR_NODATA  1
#define FCB_ERR_EOF     3
#define FCB_ERR_WRITE   1


DOS_File * Files[DOS_FILES];
DOS_Drive * Drives[DOS_DRIVES];

Bit8u DOS_GetDefaultDrive(void) {
	return dos.current_drive;
}

void DOS_SetDefaultDrive(Bit8u drive) {
	if (drive<=DOS_DRIVES) dos.current_drive=drive;
}

bool DOS_MakeName(char * name,char * fullname,Bit8u * drive) {
	char tempdir[DOS_PATHLENGTH];
	char upname[DOS_PATHLENGTH];
	Bitu r,w;
	*drive=dos.current_drive;
	/* First get the drive */
	if (name[1]==':') {
		*drive=(name[0] | 0x20)-'a';
		name+=2;
	}
	if (*drive>=DOS_DRIVES || !Drives[*drive]) { 
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false; 
	}
	r=0;w=0;
	while (name[r]!=0 && (r<DOS_PATHLENGTH)) {
		Bit8u c=name[r++];
		if ((c>='a') && (c<='z')) {upname[w++]=c-32;continue;}
		if ((c>='A') && (c<='Z')) {upname[w++]=c;continue;}
		if ((c>='0') && (c<='9')) {upname[w++]=c;continue;}
		switch (c) {
		case '/':
			upname[w++]='\\';
			break;
		case ' ':
			break;
		case '\\':	case '$':	case '#':	case '@':	case '(':	case ')':
		case '!':	case '%':	case '{':	case '}':	case '`':	case '~':
		case '_':	case '-':	case '.':	case '*':	case '?':	case '&':
		case '\'':
			upname[w++]=c;
			break;
		default:
			DOS_SetError(DOSERR_PATH_NOT_FOUND);return false;
			break;
		}
	}
	if (r>=DOS_PATHLENGTH) { DOS_SetError(DOSERR_PATH_NOT_FOUND);return false; }
	upname[w]=0;
	/* Now parse the new file name to make the final filename */
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
			char * ext=strchr(tempdir,'.');
			if (ext) {
				ext[4]=0;
				Bitu blah=strlen(tempdir);
				if (strlen(tempdir)>12) memmove(tempdir+8,ext,5);
			} else tempdir[8]=0;
			strcat(fullname,tempdir);
			tempdir[0]=0;
			w=0;r++;
			continue;
		}
		tempdir[w++]=upname[r++];
	}
	return true;	
}

bool DOS_GetCurrentDir(Bit8u drive,char * buffer) {
	if (drive==0) drive=DOS_GetDefaultDrive();
	else drive--;
	if ((drive>DOS_DRIVES) || (!Drives[drive])) {
		DOS_SetError(DOSERR_INVALID_DRIVE);
		return false;
	}
	strcpy(buffer,Drives[drive]->curdir);
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
}

bool DOS_FindFirst(char * search,Bit16u attr) {
	DOS_DTA dta(dos.dta);
	Bit8u drive;char fullsearch[DOS_PATHLENGTH];
	char dir[DOS_PATHLENGTH];char pattern[DOS_PATHLENGTH];
	if (!DOS_MakeName(search,fullsearch,&drive)) return false;
	/* Split the search in dir and pattern */
	char * find_last;
	find_last=strrchr(fullsearch,'\\');
	if (!find_last) {	/*No dir */
		strcpy(pattern,fullsearch);
		dir[0]=0;
	} else {
		*find_last=0;
		strcpy(pattern,find_last+1);
		strcpy(dir,fullsearch);
	}		
	dta.SetupSearch(drive,(Bit8u)attr,pattern);
	if (Drives[drive]->FindFirst(dir,dta)) return true;
	DOS_SetError(DOSERR_NO_MORE_FILES);
	return false;
}

bool DOS_FindNext(void) {
	DOS_DTA dta(dos.dta);
	if (Drives[dta.GetSearchDrive()]->FindNext(dta)) return true;
	DOS_SetError(DOSERR_NO_MORE_FILES);
	return false;
}


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

	DOS_PSP psp(dos.psp);
	if (entry>STDPRN) psp.SetFileHandle(entry,0xff);

	/* Devices won't allow themselves to be closed or killed */
	if (Files[handle]->Close()) {
		delete Files[handle];
		Files[handle]=0;
	}
	return true;
}

bool DOS_CreateFile(char * name,Bit16u attributes,Bit16u * entry) {
	char fullname[DOS_PATHLENGTH];Bit8u drive;
	DOS_PSP psp(dos.psp);
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
	*entry = psp.FindFreeFileEntry();
	if (*entry==0xff) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}
	bool foundit=Drives[drive]->FileCreate(&Files[handle],fullname,attributes);
	if (foundit) { 
		psp.SetFileHandle(*entry,handle);
		return true;
	} else {
		return false;
	}
}

bool DOS_OpenFile(char * name,Bit8u flags,Bit16u * entry) {
	/* First check for devices */
	if (flags>2) LOG_DEBUG("Special file open command %X file %s",flags,name);
	flags&=3;
	DOS_PSP psp(dos.psp);
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
	*entry = psp.FindFreeFileEntry();
	if (*entry==0xff) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}
	bool exists=false;
	if (!device) exists=Drives[drive]->FileOpen(&Files[handle],fullname,flags);
	if (exists || device ) { 
		psp.SetFileHandle(*entry,handle);
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

bool DOS_Canonicalize(char * name,char * big) {
//TODO Add Better support for devices and shit but will it be needed i doubt it :) 
	Bit8u drive;
	char fullname[DOS_PATHLENGTH];
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	big[0]=drive+'A';
	big[1]=':';
	big[2]='\\';
	strcpy(&big[3],fullname);
	return true;
}

bool DOS_GetFreeDiskSpace(Bit8u drive,Bit16u * bytes,Bit8u * sectors,Bit16u * clusters,Bit16u * free) {
	if (drive==0) drive=DOS_GetDefaultDrive();
	else drive--;
	if ((drive>DOS_DRIVES) || (!Drives[drive])) {
		DOS_SetError(DOSERR_INVALID_DRIVE);
		return false;
	}
	return Drives[drive]->AllocationInfo(bytes,sectors,clusters,free);
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
	DOS_PSP psp(dos.psp);
	*newentry = psp.FindFreeFileEntry();
	if (*newentry==0xff) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}
	psp.SetFileHandle(*newentry,handle);
	return true;
};

bool DOS_ForceDuplicateEntry(Bit16u entry,Bit16u newentry) {
	Bit8u orig=RealHandle(entry);
	if (orig>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[orig]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	Bit8u newone=RealHandle(newentry);
	if (newone>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (Files[newone]) {
		DOS_CloseFile(newentry);
		return false;
	};
	DOS_PSP psp(dos.psp);
	psp.SetFileHandle(newentry,(Bit8u)entry);
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
		tempname[12]=0;
	} while (!DOS_CreateFile(name,0,entry));
	return true;
}



static bool FCB_MakeName2 (DOS_FCB & fcb, char* outname, Bit8u* outdrive){
	char short_name[DOS_FCBNAME];
	fcb.GetName(short_name);
	return DOS_MakeName(short_name,outname, outdrive);
}

#define FCB_SEP ":.;,=+"
#define ILLEGAL ":.;,=+ \t/\"[]<>|"

static bool isvalid(const char in){
	const char ill[]=ILLEGAL;    
	return (Bit8u(in)>0x1F) && (!strchr(ill,in));
}

static void vullen (char* veld,char* pveld){
    for(Bitu i=(pveld-veld);i<strlen(veld);i++){
        *(veld+i)='?';
    }
    return;
}

#define PARSE_SEP_STOP          0x01
#define PARSE_DFLT_DRIVE        0x02
#define PARSE_BLNK_FNAME        0x04
#define PARSE_BLNK_FEXT         0x08

#define PARSE_RET_NOWILD        0
#define PARSE_RET_WILD          1
#define PARSE_RET_BADDRIVE      0xff

Bit8u FCB_Parsename(Bit16u seg,Bit16u offset,Bit8u parser ,char *string, Bit8u *change){
    
	char * string_begin=string;Bit8u ret=0;
    DOS_FCB fcb(seg,offset);
	bool hasdrive,hasname,hasext;
	hasdrive=hasname=hasext=false;
	Bitu index;bool finished;Bit8u fill;
/* First get the old data from the fcb */
#pragma pack (1)
	union {
		struct {
			char drive[2];
			char name[9];
			char ext[4];
		} part GCC_ATTRIBUTE (packed) ;
		char full[DOS_FCBNAME];
	} fcb_name;
#pragma pack()
	/* Get the old information from the previous fcb */
	fcb.GetName(fcb_name.full);fcb_name.part.drive[1]=0;fcb_name.part.name[8]=0;fcb_name.part.ext[3]=0;
	/* Strip of the leading sepetaror */
	if((parser & PARSE_SEP_STOP) && *string)  {       //ignore leading seperator
		char sep[] = FCB_SEP;char a[2];
		a[0]= *string;a[1]='\0';
		if (strcspn(a,sep)==0) string++;
	} 
	/* strip leading spaces */
	while((*string==' ')||(*string=='\t')) string++;
	/* Check for a drive */
	if (string[1]==':') {
		fcb_name.part.drive[0]=0;
		hasdrive=true;
		if (isalpha(string[0]) && Drives[toupper(string[0])-'A']) {
			fcb_name.part.drive[0]=toupper(string[0])-'A'+1;
		} else ret=0xff;
		string+=2;
	}
	/* Special checks for . and .. */
	if (string[0]=='.') {
		string++;
		if (!string[0])	{
			hasname=true;
			ret=PARSE_RET_NOWILD;
			strcpy(fcb_name.part.name,".       ");
			goto savefcb;
		}
		if (string[1]=='.' && !string[1])	{
			string++;
			hasname=true;
			ret=PARSE_RET_NOWILD;
			strcpy(fcb_name.part.name,"..      ");
			goto savefcb;
		}
		goto checkext;
	}
	/* Copy the name */	
	hasname=true;finished=false;fill=' ';index=0;
	while (index<8) {
		if (!finished) {
			if (string[0]=='*') {fill='?';fcb_name.part.name[index]='?';if (!ret) ret=1;finished=true;}
			else if (string[0]=='?') {fcb_name.part.name[index]='?';if (!ret) ret=1;}
			else if (isvalid(string[0])) {fcb_name.part.name[index]=toupper(string[0]);}
			else { finished=true;continue; }
			string++;
		} else {
			fcb_name.part.name[index]=fill;
		}
		index++;
	}
	if (!(string[0]=='.')) goto savefcb;
	string++;
checkext:
	/* Copy the extension */
	hasext=true;finished=false;fill=' ';index=0;
	while (index<3) {
		if (!finished) {
			if (string[0]=='*') {fill='?';fcb_name.part.ext[index]='?';finished=true;}
			else if (string[0]=='?') {fcb_name.part.ext[index]='?';if (!ret) ret=1;}
			else if (isvalid(string[0])) {fcb_name.part.ext[index]=toupper(string[0]);}
			else { finished=true;continue; }
			string++;
		} else {
			fcb_name.part.ext[index]=fill;
		}
		index++;
	}
savefcb:
	if (!hasdrive & !(parser & PARSE_DFLT_DRIVE)) fcb_name.part.drive[0]=dos.current_drive+1; 
	if (!hasname & !(parser & PARSE_BLNK_FNAME)) strcpy(fcb_name.part.name,"        ");
	if (!hasext & !(parser & PARSE_BLNK_FEXT)) strcpy(fcb_name.part.ext,"   ");		
	fcb.SetName(fcb_name.part.drive[0],fcb_name.part.name,fcb_name.part.ext);
	*change=(Bit8u)(string-string_begin);
	return ret;
}

static void DTAExtendName(char * name,char * filename,char * ext) {
	char * find=strchr(name,'.');
	if (find) {
		strcpy(ext,find+1);
		*find=0;
	} else ext[0]=0;
	strcpy(filename,name);
	Bitu i;
	for (i=strlen(name);i<8;i++) filename[i]=' ';filename[8]=0;
	for (i=strlen(ext);i<3;i++) ext[i]=' ';ext[3]=0;
}

static void SaveFindResult(DOS_FCB & find_fcb) {
	DOS_DTA find_dta(dos.tables.tempdta);
	char name[DOS_NAMELENGTH_ASCII];Bit32u size;Bit16u date;Bit16u time;Bit8u attr;Bit8u drive;
	char file_name[9];char ext[4];
	find_dta.GetResult(name,size,date,time,attr);
	drive=find_fcb.GetDrive();
	/* Create a correct file and extention */
	DTAExtendName(name,file_name,ext);	
	DOS_FCB fcb(RealSeg(dos.dta),RealOff(dos.dta));
	fcb.Create(find_fcb.Extended());
	fcb.SetName(drive,file_name,ext);
	fcb.SetSizeDateTime(size,date,time);
}

bool DOS_FCBCreate(Bit16u seg,Bit16u offset) { 
	DOS_FCB fcb(seg,offset);
	char shortname[DOS_FCBNAME];Bit16u handle;
	fcb.GetName(shortname);
	if (!DOS_CreateFile(shortname,2,&handle)) return false;
	fcb.FileOpen((Bit8u)handle);
	return true;
}

bool DOS_FCBOpen(Bit16u seg,Bit16u offset) { 
	DOS_FCB fcb(seg,offset);
	char shortname[DOS_FCBNAME];Bit16u handle;
	fcb.GetName(shortname);
	if (!DOS_OpenFile(shortname,2,&handle)) return false;
	fcb.FileOpen((Bit8u)handle);
	return true;
}

bool DOS_FCBClose(Bit16u seg,Bit16u offset) {
	DOS_FCB fcb(seg,offset);
	Bit8u fhandle;
	fcb.FileClose(fhandle);
	DOS_CloseFile(fhandle);
	return true;
}

bool DOS_FCBFindFirst(Bit16u seg,Bit16u offset)
{
	DOS_FCB fcb(seg,offset);
	RealPt old_dta=dos.dta;dos.dta=dos.tables.tempdta;
	char name[DOS_FCBNAME];fcb.GetName(name);
	bool ret=DOS_FindFirst(name,DOS_ATTR_ARCHIVE);
	dos.dta=old_dta;
	if (ret) SaveFindResult(fcb);
	return ret;
}

bool DOS_FCBFindNext(Bit16u seg,Bit16u offset)
{
	DOS_FCB fcb(seg,offset);
	RealPt old_dta=dos.dta;dos.dta=dos.tables.tempdta;
	bool ret=DOS_FindNext();
	dos.dta=old_dta;
	if (ret) SaveFindResult(fcb);
	return ret;
}

Bit8u DOS_FCBRead(Bit16u seg,Bit16u offset,Bit16u recno) {
	DOS_FCB fcb(seg,offset);
	Bit8u fhandle,cur_rec;Bit16u cur_block,rec_size;
	fcb.GetSeqData(fhandle,rec_size);
	fcb.GetRecord(cur_block,cur_rec);
	Bit32u pos=((cur_block*128)+cur_rec)*rec_size;
	if (!DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET)) return FCB_READ_NODATA; 
	Bit16u toread=rec_size;
	if (!DOS_ReadFile(fhandle,dos_copybuf,&toread)) return FCB_READ_NODATA;
	if (toread==0) return FCB_READ_NODATA;
	if (toread<rec_size) {
		PhysPt fill=Real2Phys(dos.dta)+toread;
		Bitu i=rec_size-toread;
		for (;i>0;i--) mem_writeb(fill++,0);
	}
	MEM_BlockWrite(Real2Phys(dos.dta)+recno*rec_size,dos_copybuf,rec_size);
	if (++cur_rec>127) { cur_block++;cur_rec=0; }
	fcb.SetRecord(cur_block,cur_rec);
	if (toread==rec_size) return FCB_SUCCESS;
	if (toread==0) return FCB_READ_NODATA;
	return FCB_READ_PARTIAL;
}

Bit8u DOS_FCBWrite(Bit16u seg,Bit16u offset,Bit16u recno)
{
	DOS_FCB fcb(seg,offset);
	Bit8u fhandle,cur_rec;Bit16u cur_block,rec_size;
	fcb.GetSeqData(fhandle,rec_size);
	fcb.GetRecord(cur_block,cur_rec);
	Bit32u pos=((cur_block*128)+cur_rec)*rec_size;
	if (!DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET)) return FCB_ERR_WRITE; 
	MEM_BlockRead(Real2Phys(dos.dta)+recno*rec_size,dos_copybuf,rec_size);
	Bit16u towrite=rec_size;
	if (!DOS_WriteFile(fhandle,dos_copybuf,&towrite)) return FCB_ERR_WRITE;
	Bit32u size;Bit16u date,time;
	fcb.GetSizeDateTime(size,date,time);
	if (pos+towrite>size) size=pos+towrite;
	//TODO Set time to current time?
	fcb.SetSizeDateTime(size,date,time);
	if (++cur_rec>127) { cur_block++;cur_rec=0; }	
	fcb.SetRecord(cur_block,cur_rec);
	return FCB_SUCCESS;
}

Bit8u DOS_FCBRandomRead(Bit16u seg,Bit16u offset,Bit16u numRec,bool restore) {
/* if restore is true :random read else random blok read. 
 * random read updates old block and old record to reflect the random data
 * before the read!!!!!!!!! and the random data is not updated! (user must do this)
 * Random block read updates these fields to reflect the state after the read!
 */

	DOS_FCB fcb(seg,offset);
	Bit32u random;Bit16u old_block;Bit8u old_rec;Bit8u error;

	/* Set the correct record from the random data */
	fcb.GetRandom(random);
	fcb.SetRecord((Bit16u)(random / 128),(Bit8u)(random & 127));
    if (restore) fcb.GetRecord(old_block,old_rec);//store this for after the read.
	// Read records
	for (int i=0; i<numRec; i++) {
		error = DOS_FCBRead(seg,offset,i);
		if (error!=0x00) break;
	}
	Bit16u new_block;Bit8u new_rec;
	fcb.GetRecord(new_block,new_rec);
	if (restore) fcb.SetRecord(old_block,old_rec);
	/* Update the random record pointer with new position only when restore is false*/
    if(!restore) fcb.SetRandom(new_block*128+new_rec); 
    return error;
}

Bit8u DOS_FCBRandomWrite(Bit16u seg,Bit16u offset,Bit16u numRec,bool restore) {
/* see FCB_RandomRead */
	DOS_FCB fcb(seg,offset);
	Bit32u random;Bit16u old_block;Bit8u old_rec;Bit8u error;

	/* Set the correct record from the random data */
	fcb.GetRandom(random);
	fcb.SetRecord((Bit16u)(random / 128),(Bit8u)(random & 127));
    if (restore) fcb.GetRecord(old_block,old_rec);
	/* Write records */
	for (int i=0; i<numRec; i++) {
		error = DOS_FCBWrite(seg,offset,i);// dos_fcbwrite return 0 false when true...
		if (error!=0x00) break;
	}
	Bit16u new_block;Bit8u new_rec;
	fcb.GetRecord(new_block,new_rec);
	if (restore) fcb.SetRecord(old_block,old_rec);
	/* Update the random record pointer with new position only when restore is false */
    if(!restore) fcb.SetRandom(new_block*128+new_rec); 
	return error;
}

bool DOS_FCBGetFileSize(Bit16u seg,Bit16u offset,Bit16u numRec) {
	char shortname[DOS_PATHLENGTH];Bit16u entry;Bit8u handle;Bit16u rec_size;
	DOS_FCB fcb(seg,offset);
	fcb.GetName(shortname);
	if (!DOS_OpenFile(shortname,0,&entry)) return false;
	handle=RealHandle(entry);
	Bit32u size=Files[handle]->size;
	DOS_CloseFile(entry);fcb.GetSeqData(handle,rec_size);
	Bit32u random=(size/rec_size);
	if (size % rec_size) random++;
	fcb.SetRandom(random);
	return true;
}

bool DOS_FCBDeleteFile(Bit16u seg,Bit16u offset){
	DOS_FCB fcb(seg,offset);
	char shortname[DOS_FCBNAME];
	fcb.GetName(shortname);
	return DOS_UnlinkFile(shortname);
}

bool DOS_FCBRenameFile(Bit16u seg, Bit16u offset){
	DOS_FCB fcbold(seg,offset);
	DOS_FCB fcbnew(seg,offset+16);
	char oldname[DOS_FCBNAME];
	char newname[DOS_FCBNAME];
	fcbold.GetName(oldname);fcbnew.GetName(newname);
	return DOS_Rename(oldname,newname);
}

void DOS_FCBSetRandomRecord(Bit16u seg, Bit16u offset) {
	DOS_FCB fcb(seg,offset);
	Bit16u block;Bit8u rec;
	fcb.GetRecord(block,rec);
	fcb.SetRandom(block*128+rec);
}


bool DOS_FileExists(char * name) {
	char fullname[DOS_PATHLENGTH];Bit8u drive;
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	return Drives[drive]->FileExists(fullname);
}

bool DOS_GetAllocationInfo(Bit8u drive,Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters) {
	if (!drive) drive=dos.current_drive;
	else drive--;
	if (!Drives[drive]) return false;
	Bit16u _free_clusters;
	Drives[drive]->AllocationInfo(_bytes_sector,_sectors_cluster,_total_clusters,&_free_clusters);
	SegSet16(ds,RealSeg(dos.tables.mediaid));
	reg_bx=RealOff(dos.tables.mediaid+drive);
	return true;
}

bool DOS_SetDrive(Bit8u drive) {
	if (Drives[drive]) {
		DOS_SetDefaultDrive(drive);
		return true;
	} else {
		return false;
	}
};

bool DOS_GetFileDate(Bit16u entry, Bit16u* otime, Bit16u* odate)
{
	Bit32u handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	struct stat stat_block;
	if (fstat(handle, &stat_block)!=0) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false; 
	}
	struct tm *time;
	if ((time=localtime(&stat_block.st_mtime))!=0) {
	    *otime = (time->tm_hour<<11)+(time->tm_min<<5)+(time->tm_sec/2); /* standard way. */
        *odate = ((time->tm_year-80)<<9)+((time->tm_mon+1)<<5)+(time->tm_mday);
    } else {
        *otime = 6;
        *odate = 4;
    }
	return true;
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





