// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "callback.h"
#include "dos_inc.h"
#include "dosbox.h"
#include "mem.h"
#include "regs.h"
#include <cstring>

bool DOS_IOCTL(void) {
//	LOG(LOG_IOCTL,LOG_WARN)("%X %X %X %X",reg_ax,reg_bx,reg_cx,reg_dx);
	uint8_t handle=0; // DOS managed up to 256 file handles (and our 'Files' array is 8-bit)
	uint8_t drive=0;
	/* calls 0-4,6,7,10,12,16 use a file handle */
	if ((reg_al<4) || (reg_al==0x06) || (reg_al==0x07) || (reg_al==0x0a) || (reg_al==0x0c) || (reg_al==0x10)) {
		handle=RealHandle(reg_bx);
		if (handle>=DOS_FILES) {
			DOS_SetError(DOSERR_INVALID_HANDLE);
			return false;
		}
		if (!Files[handle]) {
			DOS_SetError(DOSERR_INVALID_HANDLE);
			return false;
		}
	} else if (reg_al<0x12) { 				/* those use a diskdrive except 0x0b */
		if (reg_al!=0x0b) {
			drive=reg_bl;if (!drive) drive = DOS_GetDefaultDrive();else drive--;
			if( (drive >= 2) && !(( drive < DOS_DRIVES ) && Drives[drive]) ) {
				DOS_SetError(DOSERR_INVALID_DRIVE);
				return false;
			}
		}
	} else {
		LOG(LOG_DOSMISC,LOG_ERROR)("DOS:IOCTL Call %2X unhandled",reg_al);
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
	}
	switch(reg_al) {
	case 0x00:		/* Get Device Information */
		if (Files[handle]->GetInformation() & 0x8000) {	//Check for device
			reg_dx = Files[handle]->GetInformation() & ~EXT_DEVICE_BIT;
		} else {
			uint8_t hdrive=Files[handle]->GetDrive();
			if (hdrive==0xff) {
				LOG(LOG_IOCTL,LOG_NORMAL)("00:No drive set");
				hdrive=2;	// defaulting to C:
			}
			/* return drive number in lower 5 bits for block devices */
			reg_dx=(Files[handle]->GetInformation()&0xffe0)|hdrive;
		}
		reg_ax=reg_dx; //Destroyed officially
		return true;
	case 0x01:		/* Set Device Information */
		if (reg_dh != 0) {
			DOS_SetError(DOSERR_DATA_INVALID);
			return false;
		} else {
			if (Files[handle]->GetInformation() & 0x8000) {	//Check for device
				const auto device_ptr = dynamic_cast<DOS_Device*>(
				        Files[handle].get());
				assert(device_ptr);
				reg_al = device_ptr->GetStatus(true);
			} else {
				DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
				return false;
			}
		}
		return true;
	case 0x02:		/* Read from Device Control Channel */
		if (Files[handle]->GetInformation() & 0xc000) {
			/* is character device with IOCTL support */
			PhysPt bufptr=PhysicalMake(SegValue(ds),reg_dx);
			uint16_t retcode=0;
			const auto device_ptr = dynamic_cast<DOS_Device*>(
			        Files[handle].get());
			assert(device_ptr);
			if (device_ptr->ReadFromControlChannel(bufptr, reg_cx, &retcode)) {
				reg_ax = retcode;
				return true;
			}
		}
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
	case 0x03:		/* Write to Device Control Channel */
		if (Files[handle]->GetInformation() & 0xc000) {
			/* is character device with IOCTL support */
			PhysPt bufptr=PhysicalMake(SegValue(ds),reg_dx);
			uint16_t retcode=0;
			const auto device_ptr = dynamic_cast<DOS_Device*>(
			        Files[handle].get());
			assert(device_ptr);
			if (device_ptr->WriteToControlChannel(bufptr, reg_cx, &retcode)) {
				reg_ax = retcode;
				return true;
			}
		}
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
	case 0x06:      /* Get Input Status */
		if (Files[handle]->GetInformation() & 0x8000) {		//Check for device
			reg_al=(Files[handle]->GetInformation() & 0x40) ? 0x0 : 0xff;
		} else { // FILE
			uint32_t oldlocation=0;
			Files[handle]->Seek(&oldlocation, DOS_SEEK_CUR);
			uint32_t endlocation=0;
			Files[handle]->Seek(&endlocation, DOS_SEEK_END);
			if(oldlocation < endlocation){//Still data available
				reg_al=0xff;
			} else {
				reg_al=0x0; //EOF or beyond
			}
			Files[handle]->Seek(&oldlocation, DOS_SEEK_SET); //restore filelocation
			LOG(LOG_IOCTL,LOG_NORMAL)("06:Used Get Input Status on regular file with handle %u",handle);
		}
		return true;
	case 0x07:		/* Get Output Status */
		if (Files[handle]->GetInformation() & EXT_DEVICE_BIT) {
			const auto device_ptr = dynamic_cast<DOS_Device*>(
			        Files[handle].get());
			assert(device_ptr);
			reg_al = device_ptr->GetStatus(false);
			return true;
		}
		LOG(LOG_IOCTL, LOG_NORMAL)("07:Fakes output status is ready for handle %u", handle);
		reg_al = 0xff;
		return true;
	case 0x08:		/* Check if block device removable */
		/* cdrom drives and drive a&b are removable */
		if (drive < 2) reg_ax=0;
		else if (!Drives[drive]->IsRemovable()) reg_ax=1;
		else {
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			return false;
		}
		return true;
	case 0x09:		/* Check if block device remote */
		if ((drive >= 2) && Drives[drive]->IsRemote()) {
			reg_dx=0x1000;	// device is remote
			// undocumented bits always clear
		} else {
			reg_dx=0x0802;	// Open/Close supported; 32bit access supported (any use? fixes Fable installer)
			// undocumented bits from device attribute word
			// TODO Set bit 9 on drives that don't support direct I/O
		}
		reg_ax=0x300;
		return true;
	case 0x0B:		/* Set sharing retry count */
		if (reg_dx==0) {
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			return false;
		}
		return true;
	case 0x0D:		/* Generic block device request */
		{
			if (drive < 2 && !Drives[drive]) {
				DOS_SetError(DOSERR_ACCESS_DENIED);
				return false;
			}
			if (reg_ch != 0x08 || Drives[drive]->IsRemovable()) {
				DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
				return false;
			}
			PhysPt ptr	= SegPhys(ds)+reg_dx;
			switch (reg_cl) {
			case 0x60:		/* Get Device parameters */
				//mem_writeb(ptr+0,0);					// special functions (call value)
				mem_writeb(ptr+1,(drive>=2)?0x05:0x07);	// type: hard disk(5), 1.44 floppy(7)
				mem_writew(ptr+2,(drive>=2)?0x01:0x00);	// attributes: bit 0 set for nonremovable
				mem_writew(ptr+4,0x0000);				// num of cylinders
				mem_writeb(ptr+6,0x00);					// media type (00=other type)
				// bios parameter block following
				mem_writew(ptr+7,0x0200);				// bytes per sector (Win3 File Mgr. uses it)
				break;
			case 0x46:	/* Set volume serial number */
				break;
			case 0x66:	/* Get volume serial number */
				{			
					char const* bufin=Drives[drive]->GetLabel();
					char buffer[11];memset(buffer,' ',11);

					char const* find_ext=strchr(bufin,'.');
					if (find_ext) {
						Bitu size=(Bitu)(find_ext-bufin);
						if (size>8) size=8;
						memcpy(buffer,bufin,size);
						find_ext++;
						memcpy(buffer+8,find_ext,(strlen(find_ext)>3) ? 3 : strlen(find_ext)); 
					} else {
						memcpy(buffer,bufin,(strlen(bufin) > 8) ? 8 : strlen(bufin));
					}
			
					char buf2[8]={ 'F','A','T','1','6',' ',' ',' '};
					if(drive<2) buf2[4] = '2'; //FAT12 for floppies

					//mem_writew(ptr+0,0);			//Info level (call value)
					mem_writed(ptr+2,0x1234);		//Serial number
					MEM_BlockWrite(ptr+6,buffer,11);//volumename
					MEM_BlockWrite(ptr+0x11,buf2,8);//filesystem
				}
				break;
			default	:	
				LOG(LOG_IOCTL,LOG_ERROR)("DOS:IOCTL Call 0D:%2X Drive %2X unhandled",reg_cl,drive);
				DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
				return false;
			}
			reg_ax=0;
			return true;
		}
	case 0x0E:			/* Get Logical Drive Map */
		if (drive < 2) {
			if (Drives[drive]) reg_al=drive+1;
			else reg_al=1;
		} else if (Drives[drive]->IsRemovable()) {
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			return false;
		} else reg_al=0;	/* Only 1 logical drive assigned */
		reg_ah=0x07;
		return true;
	default:
		LOG(LOG_DOSMISC,LOG_ERROR)("DOS:IOCTL Call %2X unhandled",reg_al);
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		break;
	}
	return false;
}


bool DOS_GetSTDINStatus(void) {
	uint32_t handle=RealHandle(STDIN);
	if (handle==0xFF) return false;
	if (Files[handle] && (Files[handle]->GetInformation() & 64)) return false;
	return true;
}
