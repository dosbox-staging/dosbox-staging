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
#include "dosbox.h"
#include "dos_system.h"
#include "drives.h"
#include "support.h"

struct VFILE_Block {
	char * name;
	Bit8u * data;
	Bit32u size;
	VFILE_Block * next;
};


static VFILE_Block * first_file;	

void VFILE_Register(char * name,Bit8u * data,Bit32u size) {
	VFILE_Block * new_file=new VFILE_Block;
	new_file->name=name;
	new_file->data=data;
	new_file->size=size;
	new_file->next=first_file;
	first_file=new_file;
}


class Virtual_File : public DOS_File {
public:
	Virtual_File(Bit8u * in_data,Bit32u in_size);
	bool Read(Bit8u * data,Bit16u * size);
	bool Write(Bit8u * data,Bit16u * size);
	bool Seek(Bit32u * pos,Bit32u type);
	bool Close();
	Bit16u GetInformation(void);
private:
	Bit32u file_size;
	Bit32u file_pos;
	Bit8u * file_data;
};


Virtual_File::Virtual_File(Bit8u * in_data,Bit32u in_size) {
	file_size=in_size;
	file_data=in_data;
	file_pos=0;
}

bool Virtual_File::Read(Bit8u * data,Bit16u * size) {
	Bit32u left=file_size-file_pos;
	if (left<=*size) { 
		memcpy(data,&file_data[file_pos],left);
		*size=(Bit16u)left;
	} else {
		memcpy(data,&file_data[file_pos],*size);
	}
	file_pos+=*size;
	return true;
};

bool Virtual_File::Write(Bit8u * data,Bit16u * size){
/* Not really writeable */
	return false;
};

bool Virtual_File::Seek(Bit32u * new_pos,Bit32u type){
	switch (type) {
	case DOS_SEEK_SET:
		if (*new_pos<=file_size) file_pos=*new_pos;
		else return false;
		break;
	case DOS_SEEK_CUR:
		if ((*new_pos+file_pos)<=file_size) file_pos=*new_pos+file_pos;
		else return false;
		break;
	case DOS_SEEK_END:
		if (*new_pos<=file_size) file_pos=file_size-*new_pos;
		else return false;
		break;
	}
	*new_pos=file_pos;
	return true;
};

bool Virtual_File::Close(){
	return true;
};


Bit16u Virtual_File::GetInformation(void) {
	return 0;
}


Virtual_Drive::Virtual_Drive() {
	strcpy(info,"Internal Virtual Drive");
	search_file=0;
}


bool Virtual_Drive::FileOpen(DOS_File * * file,char * name,Bit32u flags) {
/* Scan through the internal list of files */
	VFILE_Block * cur_file=first_file;
	while (cur_file) {
		if (strcasecmp(name,cur_file->name)==0) {
		/* We have a match */
			*file=new Virtual_File(cur_file->data,cur_file->size);
			return true;
		}
		cur_file=cur_file->next;
	}
	return false;
}

bool Virtual_Drive::FileCreate(DOS_File * * file,char * name,Bit16u attributes) {
	return false;
}

bool Virtual_Drive::FileUnlink(char * name) {
	return false;
}

bool Virtual_Drive::RemoveDir(char * dir) {
	return false;
}

bool Virtual_Drive::MakeDir(char * dir) {
	return false;
}

bool Virtual_Drive::TestDir(char * dir) {
	return false;
}

static void FillDTABlock(DTA_FindBlock * dta,VFILE_Block * fill_file) {
	strcpy(dta->name,fill_file->name);
	dta->size=fill_file->size;
	dta->attr=DOS_ATTR_ARCHIVE;
	dta->time=2;
	dta->date=6;
}

bool Virtual_Drive::FindFirst(char * search,DTA_FindBlock * dta) {
	search_file=first_file;
	strcpy(search_string,search);
	while (search_file) {
		if (wildcmp(search_string,search_file->name)) {
			FillDTABlock(dta,search_file);
			search_file=search_file->next;
			return true;
		}
		search_file=search_file->next;
	}
	return false;
}

bool Virtual_Drive::FindNext(DTA_FindBlock * dta) {
	while (search_file) {
		if (wildcmp(search_string,search_file->name)) {
			FillDTABlock(dta,search_file);
			search_file=search_file->next;
			return true;
		}
		search_file=search_file->next;
	}
	return false;
}

bool Virtual_Drive::GetFileAttr(char * name,Bit16u * attr) {
	return false;
}

bool Virtual_Drive::Rename(char * oldname,char * newname) {
	return false;
}

bool Virtual_Drive::FreeSpace(Bit16u * bytes,Bit16u * sectors,Bit16u * clusters,Bit16u * free) {
	/* Always report 100 mb free should be enough */
	/* Total size is always 1 gb */
	*bytes=512;
	*sectors=127;
	*clusters=16513;
	*free=00;
	return true;
}

