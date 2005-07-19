/*
 *  Copyright (C) 2002-2005  The DOSBox Team
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

/* $Id: dos_programs.cpp,v 1.37 2005-07-19 19:45:16 qbix79 Exp $ */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "programs.h"
#include "support.h"
#include "drives.h"
#include "cross.h"
#include "regs.h"
#include "callback.h"
#include "cdrom.h"
#include "dos_system.h"
#include "dos_inc.h"
#include "bios.h"


void MSCDEX_SetCDInterface(int intNr, int forceCD);

class MOUNT : public Program {
public:
	void Run(void)
	{
		DOS_Drive * newdrive;char drive;
		std::string label;
		std::string umount;
		/* Check for unmounting */
		if (cmd->FindString("-u",umount,false)) {
			umount[0] = toupper(umount[0]);
			int drive = umount[0]-'A';
				if(drive < DOS_DRIVES && Drives[drive]) {
					if(drive == DOS_GetDefaultDrive()) {
						WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_CURRENT"));
						return;
					}
					try { /* Check if virtualdrive */
						if( dynamic_cast<localDrive*>(Drives[drive]) == 0 ) throw 0;
					}
					catch(...) {
						WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL"));
						return;
					}
					WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_SUCCES"),umount[0]);
					delete Drives[drive];
					Drives[drive] = 0;
				} else {
					WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED"),umount[0]);
				}
			return;
		}
	   
		// Show list of cdroms
		if (cmd->FindExist("-cd",false)) {
			int num = SDL_CDNumDrives();
   			WriteOut(MSG_Get("PROGRAM_MOUNT_CDROMS_FOUND"),num);
			for (int i=0; i<num; i++) {
				WriteOut("%2d. %s\n",i,SDL_CDName(i));
			};
			return;
		}

		/* Parse the command line */
		/* if the command line is empty show current mounts */
		if (!cmd->GetCount()) {
			WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_1"));
			for (int d=0;d<DOS_DRIVES;d++) {
				if (Drives[d]) {
					WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"),d+'A',Drives[d]->GetInfo());
				}
			}
			return;
		}

		std::string type="dir";
		cmd->FindString("-t",type,true);
		if (type=="floppy" || type=="dir" || type=="cdrom") {
			Bit16u sizes[4];
			Bit8u mediaid;
			std::string str_size;
			if (type=="floppy") {
				str_size="512,1,2847,2847";/* All space free */
				mediaid=0xF0;		/* Floppy 1.44 media */
			} else if (type=="dir") {
				str_size="512,127,16513,1700";
				mediaid=0xF8;		/* Hard Disk */
			} else if (type=="cdrom") {
				str_size="650,127,16513,1700";
				mediaid=0xF8;		/* Hard Disk */
			} else {
				WriteOut(MSG_Get("PROGAM_MOUNT_ILL_TYPE"),type.c_str());
				return;
			}
			/* Parse the free space in mb's */
			std::string mb_size;
			if(cmd->FindString("-freesize",mb_size,true)) {
				char teststr[1024];
				Bit16u sizemb = static_cast<Bit16u>(atoi(mb_size.c_str()));
				sprintf(teststr,"512,127,16513,%d",sizemb*1024*1024/(512*127));
				str_size=teststr;
			}
		   
			cmd->FindString("-size",str_size,true);
			char number[20];const char * scan=str_size.c_str();
			Bitu index=0;Bitu count=0;
			/* Parse the str_size string */
			while (*scan) {
				if (*scan==',') {
					number[index]=0;sizes[count++]=atoi(number);
					index=0;
				} else number[index++]=*scan;
				scan++;
			}
			number[index]=0;sizes[count++]=atoi(number);
		
			// get the drive letter
			cmd->FindCommand(1,temp_line);
			if ((temp_line.size() > 2) || ((temp_line.size()>1) && (temp_line[1]!=':'))) goto showusage;
			drive=toupper(temp_line[0]);
			if (!isalpha(drive)) goto showusage;

			if (!cmd->FindCommand(2,temp_line)) goto showusage;
			if (!temp_line.size()) goto showusage;
#if defined (WIN32)
			/* Removing trailing backslash if not root dir so stat will succeed */
			if(temp_line.size() > 3 && temp_line[temp_line.size()-1]=='\\') temp_line.erase(temp_line.size()-1,1);
#endif
			struct stat test;
			if (stat(temp_line.c_str(),&test)) {
				WriteOut(MSG_Get("PROGRAM_MOUNT_ERROR_1"),temp_line.c_str());
				return;
			}
			/* Not a switch so a normal directory/file */
			if (!(test.st_mode & S_IFDIR)) {
				WriteOut(MSG_Get("PROGRAM_MOUNT_ERROR_2"),temp_line.c_str());
				return;
			}
			if (temp_line[temp_line.size()-1]!=CROSS_FILESPLIT) temp_line+=CROSS_FILESPLIT;
			Bit8u bit8size=(Bit8u) sizes[1];
			if (type=="cdrom") {
				int num = -1;
				cmd->FindInt("-usecd",num,true);
				int error;
				if (cmd->FindExist("-aspi",false))	MSCDEX_SetCDInterface(CDROM_USE_ASPI, num);	else
				if (cmd->FindExist("-ioctl",false)) MSCDEX_SetCDInterface(CDROM_USE_IOCTL, num);
				else								MSCDEX_SetCDInterface(CDROM_USE_SDL, num);
				newdrive  = new cdromDrive(drive,temp_line.c_str(),sizes[0],bit8size,sizes[2],0,mediaid,error);
				// Check Mscdex, if it worked out...
				switch (error) {
					case 0  :	WriteOut(MSG_Get("MSCDEX_SUCCESS"));				break;
					case 1  :	WriteOut(MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS"));	break;
					case 2  :	WriteOut(MSG_Get("MSCDEX_ERROR_NOT_SUPPORTED"));	break;
					case 3  :	WriteOut(MSG_Get("MSCDEX_ERROR_PATH"));				break;
					case 4  :	WriteOut(MSG_Get("MSCDEX_TOO_MANY_DRIVES"));		break;
					case 5  :	WriteOut(MSG_Get("MSCDEX_LIMITED_SUPPORT"));		break;
					default :	WriteOut(MSG_Get("MSCDEX_UNKNOWN_ERROR"));			break;
				};
			} else {
				newdrive=new localDrive(temp_line.c_str(),sizes[0],bit8size,sizes[2],sizes[3],mediaid);
			}
		} else {
			WriteOut(MSG_Get("PROGRAM_MOUNT_ILL_TYPE"),type.c_str());
			return;
		}
		if (Drives[drive-'A']) {
			WriteOut(MSG_Get("PROGRAM_MOUNT_ALLREADY_MOUNTED"),drive,Drives[drive-'A']->GetInfo());
			if (newdrive) delete newdrive;
			return;
		}
		if (!newdrive) E_Exit("DOS:Can't create drive");
		Drives[drive-'A']=newdrive;
		/* Set the correct media byte in the table */
		mem_writeb(Real2Phys(dos.tables.mediaid)+drive-'A',newdrive->GetMediaByte());
		WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"),drive,newdrive->GetInfo());
		/* check if volume label is given and don't allow it to updated in the future */
		if (cmd->FindString("-label",label,true)) newdrive->dirCache.SetLabel(label.c_str(),false);
		return;
showusage:
		WriteOut(MSG_Get("PROGRAM_MOUNT_USAGE"));
		return;
	}
};

static void MOUNT_ProgramStart(Program * * make) {
	*make=new MOUNT;
}

class MEM : public Program {
public:
	void Run(void) {
		/* Show conventional Memory */
		WriteOut("\n");
		Bit16u seg,blocks;blocks=0xffff;
		DOS_AllocateMemory(&seg,&blocks);
		WriteOut(MSG_Get("PROGRAM_MEM_CONVEN"),blocks*16/1024);
		/* Test for and show free XMS */
		reg_ax=0x4300;CALLBACK_RunRealInt(0x2f);
		if (reg_al==0x80) {
			reg_ax=0x4310;CALLBACK_RunRealInt(0x2f);
			Bit16u xms_seg=SegValue(es);Bit16u xms_off=reg_bx;
			reg_ah=8;
			CALLBACK_RunRealFar(xms_seg,xms_off);
			if (!reg_bl) {
				WriteOut(MSG_Get("PROGRAM_MEM_EXTEND"),reg_dx);
			}
		}	
		/* Test for and show free EMS */
		Bit16u handle;
		if (DOS_OpenFile("EMMXXXX0",0,&handle)) {
			DOS_CloseFile(handle);
			reg_ah=0x42;
			CALLBACK_RunRealInt(0x67);
			WriteOut(MSG_Get("PROGRAM_MEM_EXPAND"),reg_bx*16);
		}
	}
};


static void MEM_ProgramStart(Program * * make) {
	*make=new MEM;
}

extern Bit32u floppytype;



class BOOT : public Program {
private:
	FILE *getFSFile(Bit8u * filename, Bit32u *ksize, Bit32u *bsize) {
		Bit8u drive;
		FILE *tmpfile;
		char fullname[DOS_PATHLENGTH];

		localDrive* ldp=0;
		if (!DOS_MakeName((char *)filename,fullname,&drive)) return NULL;

		try {		
			ldp=dynamic_cast<localDrive*>(Drives[drive]);
			if(!ldp) return NULL;

			tmpfile = ldp->GetSystemFilePtr(fullname, "r");
			if(tmpfile == NULL) {
				WriteOut(MSG_Get("PROGRAM_BOOT_NOT_EXIST"));
				return NULL;
			}
			fclose(tmpfile);
			tmpfile = ldp->GetSystemFilePtr(fullname, "rb+");
			if(tmpfile == NULL) {
				WriteOut(MSG_Get("PROGRAM_BOOT_NOT_OPEN"));
				return NULL;
			}

			fseek(tmpfile,0L, SEEK_END);
			*ksize = (ftell(tmpfile) / 1024);
			*bsize = ftell(tmpfile);
			return tmpfile;
		}
		catch(...) {
			return NULL;
		}

	}

	void printError(void) {
		WriteOut(MSG_Get("PROGRAM_BOOT_PRINT_ERROR"));
	}


public:

	void Run(void) {
		FILE *usefile;
		Bitu i; 
		Bit32u floppysize, rombytesize;
		Bit8u drive;

		if(!cmd->GetCount()) {
			printError();
			return;
		}
		i=0;
		drive = 'A';
		while(i<cmd->GetCount()) {
			if(cmd->FindCommand(i+1, temp_line)) {
				if(temp_line == "-l") {
					/* Specifying drive... next argument then is the drive */
					i++;
					if(cmd->FindCommand(i+1, temp_line)) {
						drive=toupper(temp_line[0]);
						if ((drive != 'A') && (drive != 'C') && (drive != 'D')) {
							printError();
							return;
						}

					} else {
						printError();
						return;
					}
					i++;
					continue;
				}

				WriteOut(MSG_Get("PROGRAM_BOOT_IMAGE_OPEN"), temp_line.c_str());
				usefile = getFSFile((Bit8u *)temp_line.c_str(), &floppysize, &rombytesize);
				if(usefile != NULL) {
					if(diskSwap[i] != NULL) delete diskSwap[i];
					diskSwap[i] = new imageDisk(usefile, (Bit8u *)temp_line.c_str(), floppysize, false);

				} else {
					WriteOut(MSG_Get("PROGRAM_BOOT_IMAGE_NOT_OPEN"), temp_line.c_str());
					return;
				}

			}
			i++;
		}

		swapPosition = 0;

		swapInDisks();

		if(imageDiskList[drive-65]==NULL) {
			WriteOut(MSG_Get("PROGRAM_BOOT_UNABLE"), drive);
			return;
		}

		WriteOut(MSG_Get("PROGRAM_BOOT_BOOT"),"Booting from drive %c...\n", drive);
		
		bootSector bootarea;
		imageDiskList[drive-65]->Read_Sector(0,0,1,(Bit8u *)&bootarea);
		for(i=0;i<512;i++) real_writeb(0, 0x7c00 + i, bootarea.rawdata[i]);

		

		SegSet16(cs, 0);
		reg_ip = 0x7c00;

				

		/* Most likely a PCJr ROM */
		/* Write it to E000:0000 */
		/* Code inoperable at the moment */

		/*
		Bit8u rombuff[65536];
		fseek(tmpfile,512L, SEEK_SET);
		rombytesize-=512;
		fread(rombuff, 1, rombytesize, tmpfile);
		fclose(tmpfile);
		for(i=0;i<rombytesize;i++) real_writeb(0xe000,i,rombuff[i]);
		SegSet16(cs,0xe000);
		SegSet16(ds,0x0060);
		SegSet16(es,0x0060);
		SegSet16(ss,0x0060);
		reg_ip = 0x0;
		reg_sp = 0x0;
		reg_cx = 0xffff;
		DEBUG_EnableDebugger();
		*/
	}
};


static void BOOT_ProgramStart(Program * * make) {
	*make=new BOOT;
}



// LOADFIX

class LOADFIX : public Program {
public:
	void Run(void);
};

void LOADFIX::Run(void) 
{
	Bit16u commandNr	= 1;
	Bit16u kb			= 64;
	if (cmd->FindCommand(commandNr,temp_line)) {
		if (temp_line[0]=='-') {
			char ch = temp_line[1];
			if ((*upcase(&ch)=='D') || (*upcase(&ch)=='F')) {
				// Deallocate all
				DOS_FreeProcessMemory(0x40);
				WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOCALL"),kb);
				return;
			} else {
				// Set mem amount to allocate
				kb = atoi(temp_line.c_str()+1);
				if (kb==0) kb=64;
				commandNr++;
			}
		}
	}
	// Allocate Memory
	Bit16u segment;
	Bit16u blocks = kb*1024/16;
	if (DOS_AllocateMemory(&segment,&blocks)) {
		DOS_MCB mcb((Bit16u)(segment-1));
		mcb.SetPSPSeg(0x40);			// use fake segment
		WriteOut(MSG_Get("PROGRAM_LOADFIX_ALLOC"),kb);
		// Prepare commandline...
		if (cmd->FindCommand(commandNr++,temp_line)) {
			// get Filename
			char filename[128];
			safe_strncpy(filename,temp_line.c_str(),128);
			// Setup commandline
			bool ok;
			char args[256];
			args[0] = 0;
			do {
				ok = cmd->FindCommand(commandNr++,temp_line);
				strncat(args,temp_line.c_str(),256);
				strncat(args," ",256);
			} while (ok);			
			// Use shell to start program
			DOS_Shell shell;
			shell.Execute(filename,args);
			DOS_FreeMemory(segment);		
			WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOC"),kb);
		}
	} else {
		WriteOut(MSG_Get("PROGRAM_LOADFIX_ERROR"),kb);	
	}
};

static void LOADFIX_ProgramStart(Program * * make) {
	*make=new LOADFIX;
}

// RESCAN

class RESCAN : public Program {
public:
	void Run(void);
};

void RESCAN::Run(void) 
{
	// Get current drive
	Bit8u drive = DOS_GetDefaultDrive();
	if (Drives[drive]) {
		Drives[drive]->EmptyCache();
		WriteOut(MSG_Get("PROGRAM_RESCAN_SUCCESS"));
	}
};

static void RESCAN_ProgramStart(Program * * make) {
	*make=new RESCAN;
}

class INTRO : public Program {
public:
	void Run(void) {
		if(cmd->FindExist("cdrom",false)) {
			WriteOut(MSG_Get("PROGRAM_INTRO_CDROM"));		
			return;
		}
		WriteOut(MSG_Get("PROGRAM_INTRO"));
	}
};

static void INTRO_ProgramStart(Program * * make) {
	*make=new INTRO;
}

class IMGMOUNT : public Program {
public:
	void Run(void)
	{
		DOS_Drive * newdrive;
		imageDisk * newImage;
		Bit32u imagesize;
		char drive;
		std::string label;

		std::string type="hdd";
		std::string fstype="fat";
		cmd->FindString("-t",type,true);
		cmd->FindString("-fs",fstype,true);
		Bit8u mediaid;
		if (type=="floppy" || type=="hdd" || type=="iso") {
			Bit16u sizes[4];
			
			std::string str_size;
			mediaid=0xF8;		

			if (type=="floppy") {
				mediaid=0xF0;		
			} else if (type=="cdrom" || type=="iso") {
				str_size="650,127,16513,1700";
				mediaid=0xF8;		
				fstype = "iso";
			} 
			cmd->FindString("-size",str_size,true);
			if ((type=="hdd") && (str_size.size()==0)) {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_GEOMETRY"));
				return;
			}
			char number[20];
			const char * scan=str_size.c_str();
			Bitu index=0;Bitu count=0;
			
			while (*scan) {
				if (*scan==',') {
					number[index]=0;sizes[count++]=atoi(number);
					index=0;
				} else number[index++]=*scan;
				scan++;
			}
			number[index]=0;sizes[count++]=atoi(number);
		
			if(fstype=="fat" || fstype=="iso") {
				// get the drive letter
				cmd->FindCommand(1,temp_line);
				if ((temp_line.size() > 2) || ((temp_line.size()>1) && (temp_line[1]!=':'))) {
					WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_DRIVE"));
					return;
				}
				drive=toupper(temp_line[0]);
				if (!isalpha(drive)) {
					WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_DRIVE"));
					return;
				}
			} else if (fstype=="none") {
				cmd->FindCommand(1,temp_line);
				if ((temp_line.size() > 1) || (!isdigit(temp_line[0]))) {
					WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY2"));
					return;
				}
				drive=temp_line[0]-'0';
				if(drive>3) {
					WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY2"));
					return;
				}
			} else {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_FORMAT_UNSUPPORTED"));
				return;
			}

			if (!cmd->FindCommand(2,temp_line)) {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_FILE"));
				return;
			}
			if (!temp_line.size()) {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_FILE"));
				return;
			}
			struct stat test;
			if (stat(temp_line.c_str(),&test)) {
				// convert dosbox filename to system filename
				char fullname[CROSS_LEN];
				char tmp[CROSS_LEN];
				safe_strncpy(tmp, temp_line.c_str(), CROSS_LEN);
				
				Bit8u drive;
				if (!DOS_MakeName(tmp, fullname, &drive) || strncmp(Drives[drive]->GetInfo(),"local directory",15)) {
					WriteOut(MSG_Get("PROGRAM_IMGMOUNG_FILE_NOT_FOUND"));
					return;
				}
				
				localDrive *ldp = (localDrive*)Drives[drive];
				ldp->GetSystemFilename(tmp, fullname);
				temp_line = tmp;
				
				if (stat(temp_line.c_str(),&test)) {
					WriteOut(MSG_Get("PROGRAM_IMGMOUNG_FILE_NOT_FOUND"));
					return;
				}
			}
			
			if ((test.st_mode & S_IFDIR)) {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNG_MOUNT"));
				return;
			}

			if(fstype=="fat") {
				newdrive=new fatDrive(temp_line.c_str(),sizes[0],sizes[1],sizes[2],sizes[3],0);
			} else if (fstype=="iso") {
				int error;
				MSCDEX_SetCDInterface(CDROM_USE_SDL, -1); 
				newdrive = new isoDrive(drive, temp_line.c_str(), mediaid, error);
				switch (error) {
					case 0  :	WriteOut(MSG_Get("MSCDEX_SUCCESS"));			break;
					case 1  :	WriteOut(MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS"));	break;
					case 2  :	WriteOut(MSG_Get("MSCDEX_ERROR_NOT_SUPPORTED"));	break;
					case 3  :	WriteOut(MSG_Get("MSCDEX_ERROR_PATH"));			break;
					case 4  :	WriteOut(MSG_Get("MSCDEX_TOO_MANY_DRIVES"));		break;
					case 5  :	WriteOut(MSG_Get("MSCDEX_LIMITED_SUPPORT"));		break;
					default :	WriteOut(MSG_Get("MSCDEX_UNKNOWN_ERROR"));		break;
				};
				if (error) {
					delete newdrive;
					return;
				}
			} else {
				FILE *newDisk = fopen(temp_line.c_str(), "rb+");
				fseek(newDisk,0L, SEEK_END);
				imagesize = (ftell(newDisk) / 1024);

				newImage = new imageDisk(newDisk, (Bit8u *)temp_line.c_str(), imagesize, (imagesize > 2880));
				if(imagesize>2880) newImage->Set_Geometry(sizes[2],sizes[3],sizes[1],sizes[0]);
			}
		}
		if(fstype=="fat") {
			if (Drives[drive-'A']) {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_ALLREADY_MOUNTED"));
				if (newdrive) delete newdrive;
				return;
			}
			if (!newdrive) WriteOut(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE"));
			Drives[drive-'A']=newdrive;
			// Set the correct media byte in the table 
			mem_writeb(Real2Phys(dos.tables.mediaid)+drive-'A',mediaid);
			WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"),drive,temp_line.c_str());
			if(((fatDrive *)newdrive)->loadedDisk->hardDrive) {
				if(imageDiskList[2] == NULL) {
					imageDiskList[2] = ((fatDrive *)newdrive)->loadedDisk;
					updateDPT();
					return;
				}
				if(imageDiskList[3] == NULL) {
					imageDiskList[3] = ((fatDrive *)newdrive)->loadedDisk;
					updateDPT();
					return;
				}
			}
			if(!((fatDrive *)newdrive)->loadedDisk->hardDrive) {
				imageDiskList[0] = ((fatDrive *)newdrive)->loadedDisk;
			}
		} else if (fstype=="iso") {
			if (Drives[drive-'A']) {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_ALLREADY_MOUNTED"));
				if (newdrive) delete newdrive;
				return;
			}
			if (!newdrive) WriteOut(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE"));
			Drives[drive-'A']=newdrive;
			// Set the correct media byte in the table 
			mem_writeb(Real2Phys(dos.tables.mediaid)+drive-'A',mediaid);
			WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"),drive,temp_line.c_str());
		} else if (fstype=="none") {
			if(imageDiskList[drive] != NULL) delete imageDiskList[drive];
			imageDiskList[drive] = newImage;
			updateDPT();
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_MOUNT_NUMBER"),drive,temp_line.c_str());
		}

		// check if volume label is given
		//if (cmd->FindString("-label",label,true)) newdrive->dirCache.SetLabel(label.c_str());
		return;
	}
};

void IMGMOUNT_ProgramStart(Program * * make) {
	*make=new IMGMOUNT;
}


void DOS_SetupPrograms(void) {
	/*Add Messages */

	MSG_Add("PROGRAM_MOUNT_CDROMS_FOUND","CDROMs found: %d\n");
	MSG_Add("PROGRAM_MOUNT_STATUS_2","Drive %c is mounted as %s\n");
	MSG_Add("PROGRAM_MOUNT_STATUS_1","Current mounted drives are:\n");
	MSG_Add("PROGRAM_MOUNT_ERROR_1","Directory %s doesn't exist.\n");
	MSG_Add("PROGRAM_MOUNT_ERROR_2","%s isn't a directory\n");
	MSG_Add("PROGRAM_MOUNT_ILL_TYPE","Illegal type %s\n");
	MSG_Add("PROGRAM_MOUNT_ALLREADY_MOUNTED","Drive %c already mounted with %s\n");
	MSG_Add("PROGRAM_MOUNT_USAGE","Usage \033[34;1mMOUNT Drive-Letter Local-Directory\033[0m\nSo a MOUNT c c:\\windows mounts windows directory as the c: drive in DOSBox\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_CURRENT","You can not unMOUNT the active drive.\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED","Drive %c isn't mounted.\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_SUCCES","Drive %c has succesfully been removed.\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL","Virtual Drives can not be unMOUNTed.\n");
   
	MSG_Add("PROGRAM_MEM_CONVEN","%10d Kb free conventional memory\n");
	MSG_Add("PROGRAM_MEM_EXTEND","%10d Kb free extended memory\n");
	MSG_Add("PROGRAM_MEM_EXPAND","%10d Kb free expanded memory\n");

	MSG_Add("PROGRAM_LOADFIX_ALLOC","%d kb allocated.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOC","%d kb freed.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOCALL","Used memory freed.\n");
	MSG_Add("PROGRAM_LOADFIX_ERROR","Memory allocation error.\n");

	MSG_Add("MSCDEX_SUCCESS","MSCDEX installed.\n");
	MSG_Add("MSCDEX_ERROR_MULTIPLE_CDROMS","MSCDEX: Failure: Drive-letters of multiple CDRom-drives have to be continuous.\n");
	MSG_Add("MSCDEX_ERROR_NOT_SUPPORTED","MSCDEX: Failure: Not yet supported.\n");
	MSG_Add("MSCDEX_ERROR_PATH","MSCDEX: Failure: Path not valid.\n");
	MSG_Add("MSCDEX_TOO_MANY_DRIVES","MSCDEX: Failure: Too many CDRom-drives (max: 5). MSCDEX Installation failed.\n");
	MSG_Add("MSCDEX_LIMITED_SUPPORT","MSCDEX: Mounted subdirectory: limited support.\n");
	MSG_Add("MSCDEX_UNKNOWN_ERROR","MSCDEX: Failure: Unknown error.\n");

	MSG_Add("PROGRAM_RESCAN_SUCCESS","Drive cache cleared.\n");

	MSG_Add("PROGRAM_INTRO",
		"\033[2J\033[33;1mWelcome to DOSBox\033[0m, an x86 emulator with sound and graphics.\n"
		"DOSBox creates a shell for you which looks like old plain DOS.\n"
		"\n"	    
		"Here are some commands to get you started:\n"
		"Before you can use the files located on your own filesystem,\n"
		"You have to mount the directory containing the files.\n"
		"For \033[1mWindows\033[0m:\n"
		"\033[34;1mmount c c:\\dosprog\033[0m will create a C drive in DOSBox with c:\\dosprog as contents.\n"
		"\n"
		"For \033[1mother platforms\033[0m:\n"
		"\033[34;1mmount c /home/user/dosprog\033[0m will do the same.\n"
		"\n"
		"When the mount has succesfully completed you can type \033[34;1mc:\033[0m to go to your freshly\n"
		"mounted C-drive. Typing \033[34;1mdir\033[0m there will show its contents."
		" \033[34;1mcd\033[0m will allow you to\n"
		"enter a directory (recognised by the \033[1m[]\033[0m in a directory listing).\n"
		"You can run programs/files which end with \033[31m.exe .bat\033[0m and \033[31m.com\033[0m.\n"
		"\n"
		"For information about CD-ROM support type \033[34;1mintro cdrom\033[0m\n"
		"\n"
		"\033[32;1mDOSBox will stop/exit without a warning if an error occured!\033[0m\n"
		);
	MSG_Add("PROGRAM_INTRO_CDROM",
		"\033[2J\033[32;1mHow to mount a Real/Virtual CD-ROM Drive in DOSBox:\033[0m\n"
		"DOSBox provides CD-ROM emulation on several levels.\n"
		"\n"
		"The \033[33mbasic\033[0m level works on all CD-ROM drives and normal directories.\n"
		"It installs MSCDEX and marks the files read-only.\n"
		"Usually this is enough for most games:\n"
		"\033[34;1mmount d \033[0;31mD:\\\033[34;1m -t cdrom\033[0m   or   \033[34;1mmount d C:\\example -t cdrom\033[0m\n"
		"If it doesn't work you might have to tell DOSBox the label of the CD-ROM:\n"
		"\033[34;1mmount d C:\\example -t cdrom -label CDLABEL\033[0m\n"
		"\n"
		"The \033[33mnext\033[0m level adds some low-level support.\n"
		"Therefore only works on CD-ROM drives:\n"
		"\033[34;1mmount d \033[0;31mD:\\\033[34;1m -t cdrom -usecd \033[33m0\033[0m\n"
		"\n"
		"The \033[33mlast\033[0m level of support depends on your Operating System:\n"
		"For \033[1mWindows 2000\033[0m, \033[1mWindows XP\033[0m and \033[1mLinux\033[0m:\n"
		"\033[34;1mmount d \033[0;31mD:\\\033[34;1m -t cdrom -usecd \033[33m0 \033[34m-ioctl\033[0m\n"
		"For \033[1mWindows 9x\033[0m with a ASPI layer installed:\n"
		"\033[34;1mmount d \033[0;31mD:\\\033[34;1m -t cdrom -usecd \033[33m0 \033[34m-aspi\033[0m\n"
		"\n"
		"Replace \033[0;31mD:\\\033[0m with the location of your CD-ROM.\n"
		"Replace the \033[33;1m0\033[0m in \033[34;1m-usecd \033[33m0\033[0m with the number reported for your CD-ROM if you type:\n"
		"\033[34;1mmount -cd\033[0m\n"
		);
	MSG_Add("PROGRAM_BOOT_NOT_EXIST","Bootdisk file does not exist.  Failing.\n");
	MSG_Add("PROGRAM_BOOT_NOT_OPEN","Cannot open bootdisk file.  Failing.\n");
	MSG_Add("PROGRAM_BOOT_PRINT_ERROR","This command boots DosBox from either a floppy or hard disk image.\n\n"
		"For this command, one can specify a succession of floppy disks swappable\n"
		"by pressing Ctrl-F4, and -l specifies the mounted drive to boot from.  If\n"
		"no drive letter is specified, this defaults to booting from the A drive.\n"
		"The only bootable drive letters are A, C, and D.  For booting from a hard\n"
		"drive (C or D), the image should have already been mounted using the\n"
		"\033[34;1mIMGMOUNT\033[0m command.\n\n"
		"The syntax of this command is:\n\n"
		"\033[34;1mBOOT [diskimg1.img diskimg2.img] [-l driveletter]\033[0m\n"
		);
	MSG_Add("PROGRAM_BOOT_UNABLE","Unable to boot off of drive %c");
	MSG_Add("PROGRAM_BOOT_IMAGE_OPEN","Opening image file: %s\n");
	MSG_Add("PROGRAM_BOOT_IMAGE_NOT_OPEN","Cannot open %s");
	MSG_Add("PROGRAM_BOOT_BOOT","Booting from drive %c...\n");

	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_DRIVE","Must specify drive letter to mount image at.\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY2","Must specify drive number (0 or 3) to mount image at (0,1=fda,fdb;2,3=hda,hdb).\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_GEOMETRY",
		"For \033[33mCD-ROM\033[0m images:   \033[34;1mimgmount Drive-Letter location-of-image -t iso\033[0m\n"
		"\n"
		"For \033[33mhardrive\033[0m images: Must specify drive geometry for hard drives:\n"
		"bytes_per_sector, sectors_per_cylinder, heads_per_cylinder, cylinder_count.\n");
	MSG_Add("PROGRAM_IMGMOUNT_FORMAT_UNSUPPORTED","Format \"%s\" is unsupported. Specify \"fat\" or \"iso\" or \"none\".\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_FILE","Must specify file-image to mount.\n");
	MSG_Add("PROGRAM_IMGMOUNG_FILE_NOT_FOUND","Image file not found.\n");
	MSG_Add("PROGRAM_IMGMOUNG_MOUNT","To mount directories, use the \033[34;1mMOUNT\033[0m command, not the \033[34;1mIMGMOUNT\033[0m command.\n");
	MSG_Add("PROGRAM_IMGMOUNT_ALLREADY_MOUNTED","Drive already mounted at that letter.\n");
	MSG_Add("PROGRAM_IMGMOUNT_CANT_CREATE","Can't create drive from file.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MOUNT_NUMBER","Drive number %d mounted as %s\n");

	/*regular setup*/
	PROGRAMS_MakeFile("MOUNT.COM",MOUNT_ProgramStart);
	PROGRAMS_MakeFile("MEM.COM",MEM_ProgramStart);
	PROGRAMS_MakeFile("LOADFIX.COM",LOADFIX_ProgramStart);
	PROGRAMS_MakeFile("RESCAN.COM",RESCAN_ProgramStart);
	PROGRAMS_MakeFile("INTRO.COM",INTRO_ProgramStart);
	PROGRAMS_MakeFile("BOOT.COM",BOOT_ProgramStart);
	PROGRAMS_MakeFile("IMGMOUNT.COM", IMGMOUNT_ProgramStart);
}
