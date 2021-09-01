/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include "programs.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "bios_disk.h"
#include "bios.h"
#include "callback.h"
#include "cdrom.h"
#include "control.h"
#include "cross.h"
#include "dma.h"
#include "dos_system.h"
#include "drives.h"
#include "fs_utils.h"
#include "inout.h"
#include "mapper.h"
#include "mem.h"
#include "program_autotype.h"
#include "program_choice.h"
#include "program_help.h"
#include "program_imgmount.h"
#include "program_ls.h"
#include "program_mount.h"
#include "regs.h"
#include "setup.h"
#include "shell.h"
#include "support.h"
#include "string_utils.h"
#include "../ints/int10.h"

#if C_DEBUG
Bitu DEBUG_EnableDebugger(void);
#endif

class MEM final : public Program {
public:
	void Run(void) {
		/* Show conventional Memory */
		WriteOut("\n");

		Bit16u umb_start=dos_infoblock.GetStartOfUMBChain();
		Bit8u umb_flag=dos_infoblock.GetUMBChainState();
		Bit8u old_memstrat=DOS_GetMemAllocStrategy()&0xff;
		if (umb_start!=0xffff) {
			if ((umb_flag&1)==1) DOS_LinkUMBsToMemChain(0);
			DOS_SetMemAllocStrategy(0);
		}

		Bit16u seg,blocks;blocks=0xffff;
		DOS_AllocateMemory(&seg,&blocks);
		WriteOut(MSG_Get("PROGRAM_MEM_CONVEN"),blocks*16/1024);

		if (umb_start!=0xffff) {
			DOS_LinkUMBsToMemChain(1);
			DOS_SetMemAllocStrategy(0x40);	// search in UMBs only

			Bit16u largest_block=0,total_blocks=0,block_count=0;
			for (;; block_count++) {
				blocks=0xffff;
				DOS_AllocateMemory(&seg,&blocks);
				if (blocks==0) break;
				total_blocks+=blocks;
				if (blocks>largest_block) largest_block=blocks;
				DOS_AllocateMemory(&seg,&blocks);
			}

			Bit8u current_umb_flag=dos_infoblock.GetUMBChainState();
			if ((current_umb_flag&1)!=(umb_flag&1)) DOS_LinkUMBsToMemChain(umb_flag);
			DOS_SetMemAllocStrategy(old_memstrat);	// restore strategy

			if (block_count>0) WriteOut(MSG_Get("PROGRAM_MEM_UPPER"),total_blocks*16/1024,block_count,largest_block*16/1024);
		}

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
		char emm[9] = { 'E','M','M','X','X','X','X','0',0 };
		if (DOS_OpenFile(emm,0,&handle)) {
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


class BOOT final : public Program {
private:

	FILE *getFSFile_mounted(char const* filename, Bit32u *ksize, Bit32u *bsize, Bit8u *error) {
		//if return NULL then put in error the errormessage code if an error was requested
		bool tryload = (*error)?true:false;
		*error = 0;
		Bit8u drive;
		FILE *tmpfile;
		char fullname[DOS_PATHLENGTH];

		localDrive* ldp=0;
		if (!DOS_MakeName(const_cast<char*>(filename),fullname,&drive)) return NULL;

		try {
			ldp=dynamic_cast<localDrive*>(Drives[drive]);
			if (!ldp) return NULL;

			tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
			if (tmpfile == NULL) {
				if (!tryload) *error=1;
				return NULL;
			}

			// get file size
			fseek(tmpfile,0L, SEEK_END);
			*ksize = (ftell(tmpfile) / 1024);
			*bsize = ftell(tmpfile);
			fclose(tmpfile);

			tmpfile = ldp->GetSystemFilePtr(fullname, "rb+");
			if (tmpfile == NULL) {
//				if (!tryload) *error=2;
//				return NULL;
				WriteOut(MSG_Get("PROGRAM_BOOT_WRITE_PROTECTED"));
				tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
				if (tmpfile == NULL) {
					if (!tryload) *error=1;
					return NULL;
				}
			}

			return tmpfile;
		}
		catch(...) {
			return NULL;
		}
	}

	FILE *getFSFile(char const * filename, Bit32u *ksize, Bit32u *bsize,bool tryload=false) {
		Bit8u error = tryload?1:0;
		FILE* tmpfile = getFSFile_mounted(filename,ksize,bsize,&error);
		if (tmpfile) return tmpfile;
		//File not found on mounted filesystem. Try regular filesystem
		std::string filename_s(filename);
		Cross::ResolveHomedir(filename_s);
		tmpfile = fopen_wrap(filename_s.c_str(),"rb+");
		if (!tmpfile) {
			if ( (tmpfile = fopen_wrap(filename_s.c_str(),"rb")) ) {
				//File exists; So can't be opened in correct mode => error 2
//				fclose(tmpfile);
//				if (tryload) error = 2;
				WriteOut(MSG_Get("PROGRAM_BOOT_WRITE_PROTECTED"));
				fseek(tmpfile,0L, SEEK_END);
				*ksize = (ftell(tmpfile) / 1024);
				*bsize = ftell(tmpfile);
				return tmpfile;
			}
			// Give the delayed errormessages from the mounted variant (or from above)
			if (error == 1) WriteOut(MSG_Get("PROGRAM_BOOT_NOT_EXIST"));
			if (error == 2) WriteOut(MSG_Get("PROGRAM_BOOT_NOT_OPEN"));
			return NULL;
		}
		fseek(tmpfile,0L, SEEK_END);
		*ksize = (ftell(tmpfile) / 1024);
		*bsize = ftell(tmpfile);
		return tmpfile;
	}

	void printError(void) {
		WriteOut(MSG_Get("PROGRAM_BOOT_PRINT_ERROR"), PRIMARY_MOD_NAME);
	}

	void disable_umb_ems_xms(void) {
		Section* dos_sec = control->GetSection("dos");
		dos_sec->ExecuteDestroy(false);
		char test[20];
		safe_strcpy(test, "umb=false");
		dos_sec->HandleInputline(test);
		safe_strcpy(test, "xms=false");
		dos_sec->HandleInputline(test);
		safe_strcpy(test, "ems=false");
		dos_sec->HandleInputline(test);
		dos_sec->ExecuteInit(false);
     }

public:

	void Run(void) {
		//Hack To allow long commandlines
		ChangeToLongCmd();
		/* In secure mode don't allow people to boot stuff.
		 * They might try to corrupt the data on it */
		if (control->SecureMode()) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
			return;
		}

		FILE *usefile_1=NULL;
		FILE *usefile_2=NULL;
		Bitu i=0;
		Bit32u floppysize=0;
		Bit32u rombytesize_1=0;
		Bit32u rombytesize_2=0;
		char drive = 'A';
		std::string cart_cmd="";

		if (!cmd->GetCount()) {
			printError();
			return;
		}
		while (i<cmd->GetCount()) {
			if (cmd->FindCommand(i+1, temp_line)) {
				if ((temp_line == "-l") || (temp_line == "-L")) {
					/* Specifying drive... next argument then is the drive */
					i++;
					if (cmd->FindCommand(i+1, temp_line)) {
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

				if ((temp_line == "-e") || (temp_line == "-E")) {
					/* Command mode for PCJr cartridges */
					i++;
					if (cmd->FindCommand(i + 1, temp_line)) {
						for (size_t ct = 0;ct < temp_line.size();ct++) temp_line[ct] = toupper(temp_line[ct]);
						cart_cmd = temp_line;
					} else {
						printError();
						return;
					}
					i++;
					continue;
				}

				if ( i >= MAX_SWAPPABLE_DISKS ) {
					return; //TODO give a warning.
				}
				WriteOut(MSG_Get("PROGRAM_BOOT_IMAGE_OPEN"), temp_line.c_str());
				Bit32u rombytesize;
				FILE *usefile = getFSFile(temp_line.c_str(), &floppysize, &rombytesize);
				if (usefile != NULL) {
					diskSwap[i].reset(new imageDisk(usefile, temp_line.c_str(), floppysize, false));
					if (usefile_1==NULL) {
						usefile_1=usefile;
						rombytesize_1=rombytesize;
					} else {
						usefile_2=usefile;
						rombytesize_2=rombytesize;
					}
				} else {
					WriteOut(MSG_Get("PROGRAM_BOOT_IMAGE_NOT_OPEN"), temp_line.c_str());
					return;
				}

			}
			i++;
		}

		swapInDisks(0);

		if (!imageDiskList[drive_index(drive)]) {
			WriteOut(MSG_Get("PROGRAM_BOOT_UNABLE"), drive);
			return;
		}

		bootSector bootarea;
		imageDiskList[drive_index(drive)]->Read_Sector(0, 0, 1,
		                                               reinterpret_cast<uint8_t *>(&bootarea));
		if ((bootarea.rawdata[0] == 0x50) && (bootarea.rawdata[1] == 0x43) &&
		    (bootarea.rawdata[2] == 0x6a) && (bootarea.rawdata[3] == 0x72)) {
			if (machine != MCH_PCJR) {
				WriteOut(MSG_Get("PROGRAM_BOOT_CART_WO_PCJR"));
			}
			else {
				Bit8u rombuf[65536];
				Bits cfound_at=-1;
				if (!cart_cmd.empty()) {
					/* read cartridge data into buffer */
					fseek(usefile_1,0x200L, SEEK_SET);
					if (fread(rombuf, 1, rombytesize_1-0x200, usefile_1) < rombytesize_1 - 0x200) {
						LOG_MSG("Failed to read sufficient cartridge data");
						fclose(usefile_1);
						return;
					}
					char cmdlist[1024];
					cmdlist[0]=0;
					Bitu ct=6;
					Bits clen=rombuf[ct];
					char buf[257];
					if (cart_cmd=="?") {
						while (clen!=0) {
							strncpy(buf,(char*)&rombuf[ct+1],clen);
							buf[clen]=0;
							upcase(buf);
							safe_strcat(cmdlist, " ");
							safe_strcat(cmdlist, buf);
							ct += 1 + clen + 3;
							if (ct>sizeof(cmdlist)) break;
							clen=rombuf[ct];
						}
						if (ct>6) {
							WriteOut(MSG_Get("PROGRAM_BOOT_CART_LIST_CMDS"),cmdlist);
						} else {
							WriteOut(MSG_Get("PROGRAM_BOOT_CART_NO_CMDS"));
						}
						for (auto &disk : diskSwap)
							disk.reset();
						//fclose(usefile_1); //delete diskSwap closes the file
						return;
					} else {
						while (clen!=0) {
							strncpy(buf,(char*)&rombuf[ct+1],clen);
							buf[clen]=0;
							upcase(buf);
							safe_strcat(cmdlist, " ");
							safe_strcat(cmdlist, buf);
							ct += 1 + clen;

							if (cart_cmd==buf) {
								cfound_at=ct;
								break;
							}

							ct+=3;
							if (ct>sizeof(cmdlist)) break;
							clen=rombuf[ct];
						}
						if (cfound_at<=0) {
							if (ct>6) {
								WriteOut(MSG_Get("PROGRAM_BOOT_CART_LIST_CMDS"),cmdlist);
							} else {
								WriteOut(MSG_Get("PROGRAM_BOOT_CART_NO_CMDS"));
							}
							for (auto &disk : diskSwap)
								disk.reset();
							//fclose(usefile_1); //Delete diskSwap closes the file
							return;
						}
					}
				}

				disable_umb_ems_xms();
				MEM_PreparePCJRCartRom();

				if (usefile_1==NULL) return;

				Bit32u sz1,sz2;
				FILE *tfile = getFSFile("system.rom", &sz1, &sz2, true);
				if (tfile!=NULL) {
					fseek(tfile, 0x3000L, SEEK_SET);
					Bit32u drd=(Bit32u) fread(rombuf, 1, 0xb000, tfile);
					if (drd==0xb000) {
						for ( i = 0; i < 0xb000; i++) phys_writeb(0xf3000 + i, rombuf[i]);
					}
					fclose(tfile);
				}

				if (usefile_2!=NULL) {
					fseek(usefile_2, 0x0L, SEEK_SET);
					if (fread(rombuf, 1, 0x200, usefile_2) < 0x200) {
						LOG_MSG("Failed to read sufficient ROM data");
						fclose(usefile_2);
						return;
					}

					PhysPt romseg_pt=host_readw(&rombuf[0x1ce])<<4;

					/* read cartridge data into buffer */
					fseek(usefile_2, 0x200L, SEEK_SET);
					if (fread(rombuf, 1, rombytesize_2-0x200, usefile_2) < rombytesize_2 - 0x200) {
						LOG_MSG("Failed to read sufficient ROM data");
						fclose(usefile_2);
						return;
					}

					//fclose(usefile_2); //usefile_2 is in diskSwap structure which should be deleted to close the file

					/* write cartridge data into ROM */
					for (i = 0; i < rombytesize_2 - 0x200; i++) phys_writeb(romseg_pt + i, rombuf[i]);
				}

				fseek(usefile_1, 0x0L, SEEK_SET);
				if (fread(rombuf, 1, 0x200, usefile_1) < 0x200) {
					LOG_MSG("Failed to read sufficient cartridge data");
					fclose(usefile_1);
					return;
				}

				Bit16u romseg=host_readw(&rombuf[0x1ce]);

				/* read cartridge data into buffer */
				fseek(usefile_1,0x200L, SEEK_SET);
				if (fread(rombuf, 1, rombytesize_1-0x200, usefile_1) < rombytesize_1 - 0x200) {
					LOG_MSG("Failed to read sufficient cartridge data");
					fclose(usefile_1);
					return;
				}
				//fclose(usefile_1); //usefile_1 is in diskSwap structure which should be deleted to close the file

				/* write cartridge data into ROM */
				for (i = 0; i < rombytesize_1 - 0x200; i++) phys_writeb((romseg << 4) + i, rombuf[i]);

				//Close cardridges
				for (auto &disk : diskSwap)
					disk.reset();

				if (cart_cmd.empty()) {
					Bit32u old_int18=mem_readd(0x60);
					/* run cartridge setup */
					SegSet16(ds,romseg);
					SegSet16(es,romseg);
					SegSet16(ss,0x8000);
					reg_esp=0xfffe;
					CALLBACK_RunRealFar(romseg,0x0003);

					Bit32u new_int18=mem_readd(0x60);
					if (old_int18!=new_int18) {
						/* boot cartridge (int18) */
						SegSet16(cs,RealSeg(new_int18));
						reg_ip = RealOff(new_int18);
					}
				} else {
					if (cfound_at>0) {
						/* run cartridge setup */
						SegSet16(ds,dos.psp());
						SegSet16(es,dos.psp());
						CALLBACK_RunRealFar(romseg,cfound_at);
					}
				}
			}
		} else {
			disable_umb_ems_xms();
			MEM_RemoveEMSPageFrame();
			WriteOut(MSG_Get("PROGRAM_BOOT_BOOT"), drive);
			for (i = 0; i < 512; i++) real_writeb(0, 0x7c00 + i, bootarea.rawdata[i]);

			/* create appearance of floppy drive DMA usage (Demon's Forge) */
			if (!IS_TANDY_ARCH && floppysize!=0) GetDMAChannel(2)->tcount=true;

			/* revector some dos-allocated interrupts */
			real_writed(0,0x01*4,0xf000ff53);
			real_writed(0,0x03*4,0xf000ff53);

			SegSet16(cs, 0);
			reg_ip = 0x7c00;
			SegSet16(ds, 0);
			SegSet16(es, 0);
			/* set up stack at a safe place */
			SegSet16(ss, 0x7000);
			reg_esp = 0x100;
			reg_esi = 0;
			reg_ecx = 1;
			reg_ebp = 0;
			reg_eax = 0;
			reg_edx = 0; //Head 0 drive 0
			reg_ebx= 0x7c00; //Real code probably uses bx to load the image
		}
	}
};

static void BOOT_ProgramStart(Program * * make) {
	*make=new BOOT;
}


class LOADROM final : public Program {
public:
	void Run(void) {
		if (!(cmd->FindCommand(1, temp_line))) {
			WriteOut(MSG_Get("PROGRAM_LOADROM_SPECIFY_FILE"));
			return;
		}

		Bit8u drive;
		char fullname[DOS_PATHLENGTH];
		localDrive* ldp=0;
		if (!DOS_MakeName((char *)temp_line.c_str(),fullname,&drive)) return;

		try {
			/* try to read ROM file into buffer */
			ldp=dynamic_cast<localDrive*>(Drives[drive]);
			if (!ldp) return;

			FILE *tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
			if (tmpfile == NULL) {
				WriteOut(MSG_Get("PROGRAM_LOADROM_CANT_OPEN"));
				return;
			}
			fseek(tmpfile, 0L, SEEK_END);
			if (ftell(tmpfile)>0x8000) {
				WriteOut(MSG_Get("PROGRAM_LOADROM_TOO_LARGE"));
				fclose(tmpfile);
				return;
			}
			fseek(tmpfile, 0L, SEEK_SET);
			Bit8u rom_buffer[0x8000];
			Bitu data_read = fread(rom_buffer, 1, 0x8000, tmpfile);
			fclose(tmpfile);

			/* try to identify ROM type */
			PhysPt rom_base = 0;
			if (data_read >= 0x4000 && rom_buffer[0] == 0x55 && rom_buffer[1] == 0xaa &&
				(rom_buffer[3] & 0xfc) == 0xe8 && strncmp((char*)(&rom_buffer[0x1e]), "IBM", 3) == 0) {

				if (!IS_EGAVGA_ARCH) {
					WriteOut(MSG_Get("PROGRAM_LOADROM_INCOMPATIBLE"));
					return;
				}
				rom_base = PhysMake(0xc000, 0); // video BIOS
			}
			else if (data_read == 0x8000 && rom_buffer[0] == 0xe9 && rom_buffer[1] == 0x8f &&
				rom_buffer[2] == 0x7e && strncmp((char*)(&rom_buffer[0x4cd4]), "IBM", 3) == 0) {

				rom_base = PhysMake(0xf600, 0); // BASIC
			}

			if (rom_base) {
				/* write buffer into ROM */
				for (Bitu i=0; i<data_read; i++) phys_writeb(rom_base + i, rom_buffer[i]);

				if (rom_base == 0xc0000) {
					/* initialize video BIOS */
					phys_writeb(PhysMake(0xf000, 0xf065), 0xcf);
					reg_flags &= ~FLAG_IF;
					CALLBACK_RunRealFar(0xc000, 0x0003);
					LOG_MSG("Video BIOS ROM loaded and initialized.");
				}
				else WriteOut(MSG_Get("PROGRAM_LOADROM_BASIC_LOADED"));
			}
			else WriteOut(MSG_Get("PROGRAM_LOADROM_UNRECOGNIZED"));
		}
		catch(...) {
			return;
		}
	}
};

static void LOADROM_ProgramStart(Program * * make) {
	*make=new LOADROM;
}

#if C_DEBUG
class BIOSTEST final : public Program {
public:
	void Run(void) {
		if (!(cmd->FindCommand(1, temp_line))) {
			WriteOut("Must specify BIOS file to load.\n");
			return;
		}

		Bit8u drive;
		char fullname[DOS_PATHLENGTH];
		localDrive* ldp = 0;
		if (!DOS_MakeName((char *)temp_line.c_str(), fullname, &drive)) return;

		try {
			/* try to read ROM file into buffer */
			ldp = dynamic_cast<localDrive*>(Drives[drive]);
			if (!ldp) return;

			FILE *tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
			if (tmpfile == NULL) {
				WriteOut("Can't open a file");
				return;
			}
			fseek(tmpfile, 0L, SEEK_END);
			if (ftell(tmpfile) > 64 * 1024) {
				WriteOut("BIOS File too large");
				fclose(tmpfile);
				return;
			}
			fseek(tmpfile, 0L, SEEK_SET);
			Bit8u buffer[64*1024];
			Bitu data_read = fread(buffer, 1, sizeof( buffer), tmpfile);
			fclose(tmpfile);

			Bit32u rom_base = PhysMake(0xf000, 0); // override regular dosbox bios
			/* write buffer into ROM */
			for (Bitu i = 0; i < data_read; i++) phys_writeb(rom_base + i, buffer[i]);

			//Start executing this bios
			memset(&cpu_regs, 0, sizeof(cpu_regs));
			memset(&Segs, 0, sizeof(Segs));


			SegSet16(cs, 0xf000);
			reg_eip = 0xfff0;
		}
		catch (...) {
			return;
		}
	}
};

static void BIOSTEST_ProgramStart(Program * * make) {
	*make = new BIOSTEST;
}

#endif

// LOADFIX

class LOADFIX final : public Program {
public:
	void Run(void);
};

void LOADFIX::Run(void)
{
	Bit16u commandNr = 1;
	Bit16u kb = 64;
	if (cmd->FindCommand(commandNr, temp_line)) {
		if (temp_line[0] == '-') {
			const auto ch = std::toupper(temp_line[1]);
			if ((ch == 'D') || (ch == 'F')) {
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
			safe_strcpy(filename, temp_line.c_str());
			// Setup commandline
			char args[256+1];
			args[0] = 0;
			bool found = cmd->FindCommand(commandNr++,temp_line);
			while (found) {
				if (strlen(args)+temp_line.length()+1>256) break;
				safe_strcat(args, temp_line.c_str());
				found = cmd->FindCommand(commandNr++,temp_line);
				if (found)
					safe_strcat(args, " ");
			}
			// Use shell to start program
			DOS_Shell shell;
			shell.Execute(filename,args);
			DOS_FreeMemory(segment);
			WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOC"),kb);
		}
	} else {
		WriteOut(MSG_Get("PROGRAM_LOADFIX_ERROR"),kb);
	}
}

static void LOADFIX_ProgramStart(Program * * make) {
	*make=new LOADFIX;
}

// RESCAN

class RESCAN final : public Program {
public:
	void Run(void);
};

void RESCAN::Run(void)
{
	bool all = false;

	Bit8u drive = DOS_GetDefaultDrive();

	if (cmd->FindCommand(1,temp_line)) {
		//-A -All /A /All
		if (temp_line.size() >= 2 &&
		    (temp_line[0] == '-' || temp_line[0] == '/') &&
		    (temp_line[1] == 'a' || temp_line[1] == 'A')) {
			all = true;
		} else if (temp_line.size() == 2 && temp_line[1] == ':') {
			lowcase(temp_line);
			drive  = temp_line[0] - 'a';
		}
	}
	// Get current drive
	if (all) {
		for (Bitu i =0; i<DOS_DRIVES; i++) {
			if (Drives[i]) Drives[i]->EmptyCache();
		}
		WriteOut(MSG_Get("PROGRAM_RESCAN_SUCCESS"));
	} else {
		if (drive < DOS_DRIVES && Drives[drive]) {
			Drives[drive]->EmptyCache();
			WriteOut(MSG_Get("PROGRAM_RESCAN_SUCCESS"));
		}
	}
}

static void RESCAN_ProgramStart(Program * * make) {
	*make=new RESCAN;
}

class INTRO final : public Program {
private:
	void WriteOutProgramIntroSpecial()
	{
		WriteOut(MSG_Get("PROGRAM_INTRO_SPECIAL"), MMOD2_NAME,
		         MMOD2_NAME, PRIMARY_MOD_NAME, PRIMARY_MOD_PAD,
		         PRIMARY_MOD_NAME, PRIMARY_MOD_PAD, PRIMARY_MOD_NAME,
		         PRIMARY_MOD_PAD, PRIMARY_MOD_NAME, PRIMARY_MOD_PAD,
		         PRIMARY_MOD_NAME, PRIMARY_MOD_PAD, PRIMARY_MOD_NAME,
		         PRIMARY_MOD_PAD, PRIMARY_MOD_NAME, PRIMARY_MOD_PAD,
		         PRIMARY_MOD_NAME, PRIMARY_MOD_PAD, PRIMARY_MOD_NAME,
		         PRIMARY_MOD_PAD, MMOD2_NAME);
	}

public:
	void DisplayMount(void) {
		/* Basic mounting has a version for each operating system.
		 * This is done this way so both messages appear in the language file*/
		WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_START"));
#if (WIN32)
		WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_WINDOWS"));
#else
		WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_OTHER"));
#endif
		WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_END"));
	}

	void Run(void) {
		/* Only run if called from the first shell (Xcom TFTD runs any intro file in the path) */
		if (DOS_PSP(dos.psp()).GetParent() != DOS_PSP(DOS_PSP(dos.psp()).GetParent()).GetParent()) return;
		if (cmd->FindExist("cdrom",false)) {
#ifdef WIN32
			WriteOut(MSG_Get("PROGRAM_INTRO_CDROM_WINDOWS"));
#else
			WriteOut(MSG_Get("PROGRAM_INTRO_CDROM_OTHER"));
#endif
			return;
		}
		if (cmd->FindExist("mount",false)) {
			WriteOut("\033[2J");//Clear screen before printing
			DisplayMount();
			return;
		}
		if (cmd->FindExist("special",false)) {
			WriteOutProgramIntroSpecial();
			return;
		}
		/* Default action is to show all pages */
		WriteOut(MSG_Get("PROGRAM_INTRO"));
		Bit8u c;Bit16u n=1;
		DOS_ReadFile (STDIN,&c,&n);
		DisplayMount();
		DOS_ReadFile (STDIN,&c,&n);
#ifdef WIN32
		WriteOut(MSG_Get("PROGRAM_INTRO_CDROM_WINDOWS"));
#else
		WriteOut(MSG_Get("PROGRAM_INTRO_CDROM_OTHER"));
#endif
		DOS_ReadFile(STDIN, &c, &n);
		WriteOutProgramIntroSpecial();
	}
};

static void INTRO_ProgramStart(Program * * make) {
	*make=new INTRO;
}

Bitu DOS_SwitchKeyboardLayout(const char* new_layout, Bit32s& tried_cp);
Bitu DOS_LoadKeyboardLayout(const char * layoutname, Bit32s codepage, const char * codepagefile);
const char* DOS_GetLoadedLayout(void);

class KEYB final : public Program {
public:
	void Run(void);
};

void KEYB::Run(void) {
	if (cmd->FindCommand(1,temp_line)) {
		if (cmd->FindString("?",temp_line,false)) {
			WriteOut(MSG_Get("PROGRAM_KEYB_SHOWHELP"));
		} else {
			/* first parameter is layout ID */
			Bitu keyb_error=0;
			std::string cp_string;
			Bit32s tried_cp = -1;
			if (cmd->FindCommand(2,cp_string)) {
				/* second parameter is codepage number */
				tried_cp=atoi(cp_string.c_str());
				char cp_file_name[256];
				if (cmd->FindCommand(3,cp_string)) {
					/* third parameter is codepage file */
					safe_strcpy(cp_file_name, cp_string.c_str());
				} else {
					/* no codepage file specified, use automatic selection */
					safe_strcpy(cp_file_name, "auto");
				}

				keyb_error=DOS_LoadKeyboardLayout(temp_line.c_str(), tried_cp, cp_file_name);
			} else {
				keyb_error=DOS_SwitchKeyboardLayout(temp_line.c_str(), tried_cp);
			}
			switch (keyb_error) {
				case KEYB_NOERROR:
					WriteOut(MSG_Get("PROGRAM_KEYB_NOERROR"),temp_line.c_str(),dos.loaded_codepage);
					break;
				case KEYB_FILENOTFOUND:
					WriteOut(MSG_Get("PROGRAM_KEYB_FILENOTFOUND"),temp_line.c_str());
					WriteOut(MSG_Get("PROGRAM_KEYB_SHOWHELP"));
					break;
				case KEYB_INVALIDFILE:
					WriteOut(MSG_Get("PROGRAM_KEYB_INVALIDFILE"),temp_line.c_str());
					break;
				case KEYB_LAYOUTNOTFOUND:
					WriteOut(MSG_Get("PROGRAM_KEYB_LAYOUTNOTFOUND"),temp_line.c_str(),tried_cp);
					break;
				case KEYB_INVALIDCPFILE:
					WriteOut(MSG_Get("PROGRAM_KEYB_INVCPFILE"),temp_line.c_str());
					WriteOut(MSG_Get("PROGRAM_KEYB_SHOWHELP"));
					break;
				default:
				        LOG(LOG_DOSMISC, LOG_ERROR)("KEYB:Invalid returncode %x",
				         static_cast<uint32_t>(keyb_error));
				        break;
			}
		}
	} else {
		/* no parameter in the command line, just output codepage info and possibly loaded layout ID */
		const char* layout_name = DOS_GetLoadedLayout();
		if (layout_name==NULL) {
			WriteOut(MSG_Get("PROGRAM_KEYB_INFO"),dos.loaded_codepage);
		} else {
			WriteOut(MSG_Get("PROGRAM_KEYB_INFO_LAYOUT"),dos.loaded_codepage,layout_name);
		}
	}
}

static void KEYB_ProgramStart(Program * * make) {
	*make=new KEYB;
}

void DOS_SetupPrograms(void) {
	/*Add Messages */

	MSG_Add("PROGRAM_MOUNT_CDROMS_FOUND","CDROMs found: %d\n");
	MSG_Add("PROGRAM_MOUNT_STATUS_DRIVE", "Drive");
	MSG_Add("PROGRAM_MOUNT_STATUS_TYPE", "Type");
	MSG_Add("PROGRAM_MOUNT_STATUS_LABEL", "Label");
	MSG_Add("PROGRAM_MOUNT_STATUS_2","Drive %c is mounted as %s\n");
	MSG_Add("PROGRAM_MOUNT_STATUS_1", "The currently mounted drives are:\n");
	MSG_Add("PROGRAM_MOUNT_ERROR_1","Directory %s doesn't exist.\n");
	MSG_Add("PROGRAM_MOUNT_ERROR_2","%s isn't a directory\n");
	MSG_Add("PROGRAM_MOUNT_ILL_TYPE","Illegal type %s\n");
	MSG_Add("PROGRAM_MOUNT_ALREADY_MOUNTED","Drive %c already mounted with %s\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED","Drive %c isn't mounted.\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_SUCCESS","Drive %c has successfully been removed.\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL","Virtual Drives can not be unMOUNTed.\n");
	MSG_Add("PROGRAM_MOUNT_DRIVEID_ERROR", "'%c' is not a valid drive identifier.\n");
	MSG_Add("PROGRAM_MOUNT_WARNING_WIN","\033[31;1mMounting c:\\ is NOT recommended. Please mount a (sub)directory next time.\033[0m\n");
	MSG_Add("PROGRAM_MOUNT_WARNING_OTHER","\033[31;1mMounting / is NOT recommended. Please mount a (sub)directory next time.\033[0m\n");
	MSG_Add("PROGRAM_MOUNT_NO_OPTION", "Warning: Ignoring unsupported option '%s'.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_NO_BASE","A normal directory needs to be MOUNTed first before an overlay can be added on top.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_INCOMPAT_BASE","The overlay is NOT compatible with the drive that is specified.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_MIXED_BASE","The overlay needs to be specified using the same addressing as the underlying drive. No mixing of relative and absolute paths.");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_SAME_AS_BASE","The overlay directory can not be the same as underlying drive.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_GENERIC_ERROR","Something went wrong.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_STATUS","Overlay %s on drive %c mounted.\n");
	MSG_Add("PROGRAM_MOUNT_MOVE_Z_ERROR_1", "Can't move drive Z. Drive %c is mounted already.\n");

	MSG_Add("PROGRAM_MEM_CONVEN", "%10d kB free conventional memory\n");
	MSG_Add("PROGRAM_MEM_EXTEND", "%10d kB free extended memory\n");
	MSG_Add("PROGRAM_MEM_EXPAND", "%10d kB free expanded memory\n");
	MSG_Add("PROGRAM_MEM_UPPER",
	        "%10d kB free upper memory in %d blocks (largest UMB %d kB)\n");

	MSG_Add("PROGRAM_LOADFIX_ALLOC", "%d kB allocated.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOC", "%d kB freed.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOCALL","Used memory freed.\n");
	MSG_Add("PROGRAM_LOADFIX_ERROR","Memory allocation error.\n");

	MSG_Add("MSCDEX_SUCCESS","MSCDEX installed.\n");
	MSG_Add("MSCDEX_ERROR_MULTIPLE_CDROMS","MSCDEX: Failure: Drive-letters of multiple CD-ROM drives have to be continuous.\n");
	MSG_Add("MSCDEX_ERROR_NOT_SUPPORTED","MSCDEX: Failure: Not yet supported.\n");
	MSG_Add("MSCDEX_ERROR_PATH","MSCDEX: Specified location is not a CD-ROM drive.\n");
	MSG_Add("MSCDEX_ERROR_OPEN","MSCDEX: Failure: Invalid file or unable to open.\n");
	MSG_Add("MSCDEX_TOO_MANY_DRIVES","MSCDEX: Failure: Too many CD-ROM drives (max: 5). MSCDEX Installation failed.\n");
	MSG_Add("MSCDEX_LIMITED_SUPPORT","MSCDEX: Mounted subdirectory: limited support.\n");
	MSG_Add("MSCDEX_INVALID_FILEFORMAT","MSCDEX: Failure: File is either no ISO/CUE image or contains errors.\n");
	MSG_Add("MSCDEX_UNKNOWN_ERROR","MSCDEX: Failure: Unknown error.\n");
	MSG_Add("MSCDEX_WARNING_NO_OPTION", "MSCDEX: Warning: Ignoring unsupported option '%s'.\n");

	MSG_Add("PROGRAM_RESCAN_SUCCESS","Drive cache cleared.\n");

	MSG_Add("PROGRAM_INTRO",
		"\033[2J\033[32;1mWelcome to DOSBox Staging\033[0m, an x86 emulator with sound and graphics.\n"
		"DOSBox creates a shell for you which looks like old plain DOS.\n"
		"\n"
		"For information about basic mount type \033[34;1mintro mount\033[0m\n"
		"For information about CD-ROM support type \033[34;1mintro cdrom\033[0m\n"
		"For information about special keys type \033[34;1mintro special\033[0m\n"
		"For more imformation, visit DOSBox Staging wiki:\033[34;1m\n"
		"https://github.com/dosbox-staging/dosbox-staging/wiki\033[0m\n"
		"\n"
		"\033[31;1mDOSBox will stop/exit without a warning if an error occurred!\033[0m\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_START",
		"\033[2J\033[32;1mHere are some commands to get you started:\033[0m\n"
		"Before you can use the files located on your own filesystem,\n"
		"You have to mount the directory containing the files.\n"
		"\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_WINDOWS",
		"\033[44;1m\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB\n"
		"\xBA \033[32mmount c c:\\dosgames\\\033[37m will create a C drive with c:\\dosgames as contents.\xBA\n"
		"\xBA                                                                         \xBA\n"
		"\xBA \033[32mc:\\dosgames\\\033[37m is an example. Replace it with your own games directory.  \033[37m \xBA\n"
		"\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC\033[0m\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_OTHER",
		"\033[44;1m\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB\n"
		"\xBA \033[32mmount c ~/dosgames\033[37m will create a C drive with ~/dosgames as contents.\xBA\n"
		"\xBA                                                                      \xBA\n"
		"\xBA \033[32m~/dosgames\033[37m is an example. Replace it with your own games directory.\033[37m  \xBA\n"
		"\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC\033[0m\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_END",
		"After successfully mounting the disk you can type \033[34;1mc:\033[0m to go to your freshly\n"
		"mounted C-drive. Typing \033[34;1mdir\033[0m there will show its contents."
		" \033[34;1mcd\033[0m will allow you to\n"
		"enter a directory (recognised by the \033[33;1m[]\033[0m in a directory listing).\n"
		"You can run programs/files with extensions \033[31m.exe .bat\033[0m and \033[31m.com\033[0m.\n"
		);
	MSG_Add("PROGRAM_INTRO_CDROM_WINDOWS",
	        "\033[2J\033[32;1mHow to mount a virtual CD-ROM Drive in DOSBox:\033[0m\n"
	        "DOSBox provides CD-ROM emulation on several levels.\n"
	        "\n"
	        "This works on all normal directories, installs MSCDEX and marks the files\n"
	        "read-only. Usually this is enough for most games:\n"
	        "\n"
	        "\033[34;1mmount D C:\\example -t cdrom\033[0m\n"
	        "\n"
	        "If it doesn't work you might have to tell DOSBox the label of the CD-ROM:\n"
	        "\n"
	        "\033[34;1mmount D C:\\example -t cdrom -label CDLABEL\033[0m\n"
	        "\n"
	        "Additionally, you can use imgmount to mount iso or cue/bin images:\n"
	        "\n"
	        "\033[34;1mimgmount D C:\\cd.iso -t cdrom\033[0m\n"
	        "\n"
	        "\033[34;1mimgmount D C:\\cd.cue -t cdrom\033[0m\n");
	MSG_Add("PROGRAM_INTRO_CDROM_OTHER",
	        "\033[2J\033[32;1mHow to mount a virtual CD-ROM Drive in DOSBox:\033[0m\n"
	        "DOSBox provides CD-ROM emulation on several levels.\n"
	        "\n"
	        "This works on all normal directories, installs MSCDEX and marks the files\n"
	        "read-only. Usually this is enough for most games:\n"
	        "\n"
	        "\033[34;1mmount D ~/example -t cdrom\033[0m\n"
	        "\n"
	        "If it doesn't work you might have to tell DOSBox the label of the CD-ROM:\n"
	        "\n"
	        "\033[34;1mmount D ~/example -t cdrom -label CDLABEL\033[0m\n"
	        "\n"
	        "Additionally, you can use imgmount to mount iso or cue/bin images:\n"
	        "\n"
	        "\033[34;1mimgmount D ~/cd.iso -t cdrom\033[0m\n"
	        "\n"
	        "\033[34;1mimgmount D ~/cd.cue -t cdrom\033[0m\n");
	MSG_Add("PROGRAM_INTRO_SPECIAL",
	        "\033[2J\033[32;1mSpecial keys:\033[0m\n"
	        "These are the default keybindings.\n"
	        "They can be changed in the \033[33mkeymapper\033[0m.\n"
	        "\n"
	        "\033[33;1m%s+Enter\033[0m  Switch between fullscreen and window mode.\n"
	        "\033[33;1m%s+Pause\033[0m  Pause/Unpause emulator.\n"
	        "\033[33;1m%s+F1\033[0m   %s Start the \033[33mkeymapper\033[0m.\n"
	        "\033[33;1m%s+F4\033[0m   %s Swap mounted disk image, update directory cache for all drives.\n"
	        "\033[33;1m%s+F5\033[0m   %s Save a screenshot.\n"
	        "\033[33;1m%s+F6\033[0m   %s Start/Stop recording sound output to a wave file.\n"
	        "\033[33;1m%s+F7\033[0m   %s Start/Stop recording video output to a zmbv file.\n"
	        "\033[33;1m%s+F9\033[0m   %s Shutdown emulator.\n"
	        "\033[33;1m%s+F10\033[0m  %s Capture/Release the mouse.\n"
	        "\033[33;1m%s+F11\033[0m  %s Slow down emulation.\n"
	        "\033[33;1m%s+F12\033[0m  %s Speed up emulation.\n"
	        "\033[33;1m%s+F12\033[0m    Unlock speed (turbo button/fast forward).\n");

	MSG_Add("PROGRAM_BOOT_NOT_EXIST","Bootdisk file does not exist.  Failing.\n");
	MSG_Add("PROGRAM_BOOT_NOT_OPEN","Cannot open bootdisk file.  Failing.\n");
	MSG_Add("PROGRAM_BOOT_WRITE_PROTECTED","Image file is read-only! Might create problems.\n");
	MSG_Add("PROGRAM_BOOT_PRINT_ERROR",
	        "This command boots DOSBox from either a floppy or hard disk image.\n\n"
	        "For this command, one can specify a succession of floppy disks swappable\n"
	        "by pressing %s+F4, and -l specifies the mounted drive to boot from.  If\n"
	        "no drive letter is specified, this defaults to booting from the A drive.\n"
	        "The only bootable drive letters are A, C, and D.  For booting from a hard\n"
	        "drive (C or D), the image should have already been mounted using the\n"
	        "\033[34;1mIMGMOUNT\033[0m command.\n\n"
	        "The syntax of this command is:\n\n"
	        "\033[34;1mBOOT [diskimg1.img diskimg2.img] [-l driveletter]\033[0m\n");
	MSG_Add("PROGRAM_BOOT_UNABLE","Unable to boot off of drive %c");
	MSG_Add("PROGRAM_BOOT_IMAGE_OPEN","Opening image file: %s\n");
	MSG_Add("PROGRAM_BOOT_IMAGE_NOT_OPEN","Cannot open %s");
	MSG_Add("PROGRAM_BOOT_BOOT","Booting from drive %c...\n");
	MSG_Add("PROGRAM_BOOT_CART_WO_PCJR","PCjr cartridge found, but machine is not PCjr");
	MSG_Add("PROGRAM_BOOT_CART_LIST_CMDS", "Available PCjr cartridge commands: %s");
	MSG_Add("PROGRAM_BOOT_CART_NO_CMDS", "No PCjr cartridge commands found");

	MSG_Add("PROGRAM_LOADROM_SPECIFY_FILE","Must specify ROM file to load.\n");
	MSG_Add("PROGRAM_LOADROM_CANT_OPEN","ROM file not accessible.\n");
	MSG_Add("PROGRAM_LOADROM_TOO_LARGE","ROM file too large.\n");
	MSG_Add("PROGRAM_LOADROM_INCOMPATIBLE","Video BIOS not supported by machine type.\n");
	MSG_Add("PROGRAM_LOADROM_UNRECOGNIZED","ROM file not recognized.\n");
	MSG_Add("PROGRAM_LOADROM_BASIC_LOADED","BASIC ROM loaded.\n");

	MSG_Add("SHELL_CMD_IMGMOUNT_HELP",
	        "mounts compact disc image(s) or floppy disk image(s) to a given drive letter.\n");

	MSG_Add("SHELL_CMD_IMGMOUNT_HELP_LONG",
	        "Mount a CD-ROM, floppy, or disk image to a drive letter.\n"
	        "\n"
	        "Usage:\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mDRIVE\033[0m \033[36;1mCDROM-SET\033[0m [CDROM-SET2 [..]] [-fs iso] -t cdrom|iso\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mDRIVE\033[0m \033[36;1mIMAGEFILE\033[0m [IMAGEFILE2 [..]] [-fs fat] -t hdd|floppy\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mDRIVE\033[0m \033[36;1mBOOTIMAGE\033[0m [-fs fat|none] -t hdd -size GEOMETRY\n"
	        "  \033[32;1mimgmount\033[0m -u \033[37;1mDRIVE\033[0m  (unmounts the DRIVE's image)\n"
	        "\n"
	        "Where:\n"
	        "  \033[37;1mDRIVE\033[0m     is the drive letter where the image will be mounted: a, c, d, ...\n"
	        "  \033[36;1mCDROM-SET\033[0m is an ISO, CUE+BIN, CUE+ISO, or CUE+ISO+FLAC/OPUS/OGG/MP3/WAV\n"
	        "  \033[36;1mIMAGEFILE\033[0m is a hard drive or floppy image in FAT16 or FAT12 format\n"
	        "  \033[36;1mBOOTIMAGE\033[0m is a bootable disk image with specified -size GEOMETRY:\n"
	        "            bytes-per-sector,sectors-per-head,heads,cylinders\n"
	        "Notes:\n"
	        "  - %s+F4 swaps & mounts the next CDROM-SET or IMAGEFILE, if provided.\n"
	        "\n"
	        "Examples:\n"
#if defined(WIN32)
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1mC:\\games\\doom.iso\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1mcd/quake1.cue\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mA\033[0m \033[36;1mfloppy1.img floppy2.img floppy3.img\033[0m -t floppy\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1mC:\\dos\\c_drive.img\033[0m -t hdd\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1mbootable.img\033[0m -t hdd -fs none -size 512,63,32,1023\n"
#elif defined(MACOSX)
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1m/Users/USERNAME/games/doom.iso\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1mcd/quake1.cue\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mA\033[0m \033[36;1mfloppy1.img floppy2.img floppy3.img\033[0m -t floppy\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1m~/dos/c_drive.img\033[0m -t hdd\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1mbootable.img\033[0m -t hdd -fs none -size 512,63,32,1023\n"
#else
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1m/home/USERNAME/games/doom.iso\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1mcd/quake1.cue\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mA\033[0m \033[36;1mfloppy1.img floppy2.img floppy3.img\033[0m -t floppy\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1m~/dos/c_drive.img\033[0m -t hdd\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1mbootable.img\033[0m -t hdd -fs none -size 512,63,32,1023\n"
#endif
	);

	MSG_Add("SHELL_CMD_MOUNT_HELP",
	        "maps physical folders or drives to a virtual drive letter.\n");

	MSG_Add("SHELL_CMD_MOUNT_HELP_LONG",
	        "Mount a directory from the host OS to a drive letter.\n"
	        "\n"
	        "Usage:\n"
	        "  \033[32;1mmount\033[0m \033[37;1mDRIVE\033[0m \033[36;1mDIRECTORY\033[0m [-t TYPE] [-freesize SIZE] [-label LABEL]\n"
	        "  \033[32;1mmount\033[0m -u \033[37;1mDRIVE\033[0m  (unmounts the DRIVE's directory)\n"
	        "\n"
	        "Where:\n"
	        "  \033[37;1mDRIVE\033[0m     the drive letter where the directory will be mounted: A, C, D, ...\n"
	        "  \033[36;1mDIRECTORY\033[0m is the directory on the host OS to be mounted\n"
	        "  TYPE      type of the directory to mount: dir, floppy, cdrom, or overlay\n"
	        "  SIZE      free space for the virtual drive (KiB for floppies, MiB otherwise)\n"
	        "  LABEL     drive label name to be used\n"
	        "\n"
	        "Notes:\n"
	        "  - '-t overlay' redirects writes for mounted drive to another directory.\n"
	        "  - Additional options are described in the manual (README file, chapter 4).\n"
	        "\n"
	        "Examples:\n"
#if defined(WIN32)
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1mC:\\dosgames\033[0m\n"
	        "  \033[32;1mmount\033[0m \033[37;1mD\033[0m \033[36;1mD:\\\033[0m -t cdrom\n"
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1mmy_savegame_files\033[0m -t overlay\n"
#elif defined(MACOSX)
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1m~/dosgames\033[0m\n"
	        "  \033[32;1mmount\033[0m \033[37;1mD\033[0m \033[36;1m\"/Volumes/Game CD\"\033[0m -t cdrom\n"
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1mmy_savegame_files\033[0m -t overlay\n"
#else
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1m~/dosgames\033[0m\n"
	        "  \033[32;1mmount\033[0m \033[37;1mD\033[0m \033[36;1m\"/media/USERNAME/Game CD\"\033[0m -t cdrom\n"
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1mmy_savegame_files\033[0m -t overlay\n"
#endif
	);

	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_DRIVE",
	        "Must specify drive letter to mount image at.\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY2",
	        "Must specify drive number (0 or 3) to mount image at (0,1=fda,fdb;2,3=hda,hdb).\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_GEOMETRY",
		"For \033[33mCD-ROM\033[0m images:   \033[34;1mIMGMOUNT drive-letter location-of-image -t iso\033[0m\n"
		"\n"
		"For \033[33mhardrive\033[0m images: Must specify drive geometry for hard drives:\n"
		"bytes_per_sector, sectors_per_cylinder, heads_per_cylinder, cylinder_count.\n"
		"\033[34;1mIMGMOUNT drive-letter location-of-image -size bps,spc,hpc,cyl\033[0m\n");
	MSG_Add("PROGRAM_IMGMOUNT_INVALID_IMAGE","Could not load image file.\n"
		"Check that the path is correct and the image is accessible.\n");
	MSG_Add("PROGRAM_IMGMOUNT_INVALID_GEOMETRY","Could not extract drive geometry from image.\n"
		"Use parameter -size bps,spc,hpc,cyl to specify the geometry.\n");
	MSG_Add("PROGRAM_IMGMOUNT_TYPE_UNSUPPORTED",
	        "Type '%s' is unsupported. Specify 'floppy', 'hdd', 'cdrom', or 'iso'.\n");
	MSG_Add("PROGRAM_IMGMOUNT_FORMAT_UNSUPPORTED","Format \"%s\" is unsupported. Specify \"fat\" or \"iso\" or \"none\".\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_FILE","Must specify file-image to mount.\n");
	MSG_Add("PROGRAM_IMGMOUNT_FILE_NOT_FOUND","Image file not found.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MOUNT","To mount directories, use the \033[34;1mMOUNT\033[0m command, not the \033[34;1mIMGMOUNT\033[0m command.\n");
	MSG_Add("PROGRAM_IMGMOUNT_ALREADY_MOUNTED","Drive already mounted at that letter.\n");
	MSG_Add("PROGRAM_IMGMOUNT_CANT_CREATE","Can't create drive from file.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MOUNT_NUMBER","Drive number %d mounted as %s\n");
	MSG_Add("PROGRAM_IMGMOUNT_NON_LOCAL_DRIVE", "The image must be on a host or local drive.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MULTIPLE_NON_CUEISO_FILES", "Using multiple files is only supported for cue/iso images.\n");

	MSG_Add("PROGRAM_KEYB_INFO","Codepage %i has been loaded\n");
	MSG_Add("PROGRAM_KEYB_INFO_LAYOUT","Codepage %i has been loaded for layout %s\n");
	MSG_Add("PROGRAM_KEYB_SHOWHELP",
		"\033[32;1mKEYB\033[0m [keyboard layout ID[ codepage number[ codepage file]]]\n\n"
		"Some examples:\n"
		"  \033[32;1mKEYB\033[0m: Display currently loaded codepage.\n"
		"  \033[32;1mKEYB\033[0m sp: Load the spanish (SP) layout, use an appropriate codepage.\n"
		"  \033[32;1mKEYB\033[0m sp 850: Load the spanish (SP) layout, use codepage 850.\n"
		"  \033[32;1mKEYB\033[0m sp 850 mycp.cpi: Same as above, but use file mycp.cpi.\n");
	MSG_Add("PROGRAM_KEYB_NOERROR","Keyboard layout %s loaded for codepage %i\n");
	MSG_Add("PROGRAM_KEYB_FILENOTFOUND","Keyboard file %s not found\n\n");
	MSG_Add("PROGRAM_KEYB_INVALIDFILE","Keyboard file %s invalid\n");
	MSG_Add("PROGRAM_KEYB_LAYOUTNOTFOUND","No layout in %s for codepage %i\n");
	MSG_Add("PROGRAM_KEYB_INVCPFILE","None or invalid codepage file for layout %s\n\n");

	PROGRAMS_MakeFile("AUTOTYPE.COM", AUTOTYPE_ProgramStart);
#if C_DEBUG
	PROGRAMS_MakeFile("BIOSTEST.COM", BIOSTEST_ProgramStart);
#endif
	PROGRAMS_MakeFile("BOOT.COM", BOOT_ProgramStart);
	PROGRAMS_MakeFile("CHOICE.COM", CHOICE_ProgramStart);
	PROGRAMS_MakeFile("HELP.COM", HELP_ProgramStart);
	PROGRAMS_MakeFile("IMGMOUNT.COM", IMGMOUNT_ProgramStart);
	PROGRAMS_MakeFile("INTRO.COM", INTRO_ProgramStart);
	PROGRAMS_MakeFile("KEYB.COM", KEYB_ProgramStart);
	PROGRAMS_MakeFile("LOADFIX.COM", LOADFIX_ProgramStart);
	PROGRAMS_MakeFile("LOADROM.COM", LOADROM_ProgramStart);
	PROGRAMS_MakeFile("LS.COM", LS_ProgramStart);
	PROGRAMS_MakeFile("MEM.COM", MEM_ProgramStart);
	PROGRAMS_MakeFile("MOUNT.COM", MOUNT_ProgramStart);
	PROGRAMS_MakeFile("RESCAN.COM", RESCAN_ProgramStart);
}
