#include "config.h"
#include "logging.h"

#if defined (_MSC_VER) 
/*
void outportb(Bit32u portid, Bit8u value) {
  __asm mov edx,portid
  __asm mov al,value
  __asm out dx,al
}

Bit8u inportb(Bit32u portid) {
  Bit8u value;
  
  __asm mov edx,portid
  __asm in al,dx
  __asm mov value,al
  return value;
}

void outportw(Bit32u portid, Bit16u value) {
  __asm mov edx,portid
  __asm mov ax,value
  __asm out dx,ax
}

Bit16u inportw(Bit32u portid) {
  Bit16u value;
  
  __asm mov edx,portid
  __asm in ax,dx
  __asm mov value,ax
  return value;
}

void outportd(Bit32u portid, Bit32u value) {
  __asm mov edx,portid
  __asm mov eax,value
  __asm out dx,eax
}

Bit32u inportd(Bit32u portid) {
  Bit32u value;
  
  __asm mov edx,portid
  __asm in eax,dx
  __asm mov value,eax
  return value;
}
*/
#else
/*void outportb(Bit32u portid, Bit8u value) {
   __asm__ volatile (
      "movl   %0,%%edx   \n"
      "movb   %1,%%al      \n"
      "outb   %%al,%%dx   "
      :
      :   "r" (portid), "r" (value)
      :   "edx", "al"
   );
}
Bit8u inportb(Bit32u portid) {
   Bit8u value;
   __asm__ volatile (
      "movl   %1,%%edx   \n"
      "inb   %%dx,%%al   \n"
      "movb   %%al,%0      "
      :   "=m" (value)
      :   "r" (portid)
      :   "edx", "al", "memory"
   );
  return value;
}*/
#endif

#ifdef WIN32

// WIN specific
#include "sdl.h"
#include <windows.h>
#include <winioctl.h> // NEEDED by GCC
#include "porttalk.h"
#include <stdio.h>
#include <process.h>

// PortTalk_IOCTL.h can be downloaded with PortTalk
#include "PortTalk_IOCTL.h"

typedef struct driverpermstruct {
	Bit16u offset;
	Bit8u value;
} permblock;

static HANDLE porttalkhandle=INVALID_HANDLE_VALUE;
static Bit8u ioperm[8192];
static bool isNT = false;

bool initPorttalk() {
	// handles neded for starting service
	SC_HANDLE  ServiceManager = NULL;
	SC_HANDLE  PorttalkService = NULL;

	// check which platform we are on
	OSVERSIONINFO osvi;
	memset(&osvi,0,sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
	if (! GetVersionEx ( (OSVERSIONINFO *) &osvi) ) {
		LOG_MSG("GET VERSION failed!");
		return false;
	}
	if(osvi.dwPlatformId==2) isNT=true;
	
	if(isNT && porttalkhandle==INVALID_HANDLE_VALUE) {
		porttalkhandle = CreateFile("\\\\.\\PortTalk", GENERIC_READ,
				0, NULL,OPEN_EXISTING, FILE_ATTRIBUTE_DEVICE, NULL);

		if (porttalkhandle == INVALID_HANDLE_VALUE) {
			Bitu retval=0;
			// Porttalk service is not started. Attempt to start it.
			ServiceManager = OpenSCManager (NULL,	// NULL is local machine
					NULL,							// default database
					SC_MANAGER_ENUMERATE_SERVICE);	// desired access
			
			if(ServiceManager==NULL) {
				// No rights to enumerate services
				LOG_MSG("You do not have the rights to enumerate services.");
				return false;
			}
			PorttalkService = OpenService(ServiceManager,
                                  "PortTalk",		// service name
                                  SERVICE_START);	// desired access
			
			if(PorttalkService==NULL) {
				// get causes
				switch (retval=GetLastError()) {
                case ERROR_ACCESS_DENIED:
					LOG_MSG("You do not have the rights to enumerate services.");
					break;
                case ERROR_SERVICE_DOES_NOT_EXIST:
					LOG_MSG("Porttalk service is not installed.");
					break;
				default:
					LOG_MSG("Error %d occured accessing porttalk dirver.",retval);
					break;
				}
				goto error;
			}

			// start it
			retval = StartService (PorttalkService,
				0,             // number of arguments
				NULL);         // pointer to arguments
			if(!retval) {
				// couldn't start it
				if((retval=GetLastError())!=ERROR_SERVICE_ALREADY_RUNNING) {
					LOG_MSG("Could not start Porttalk service. Error %d.",retval);
					goto error;
				}
			}
			CloseServiceHandle(PorttalkService);
			CloseServiceHandle(ServiceManager);

			// try again
			porttalkhandle = CreateFile("\\\\.\\PortTalk", GENERIC_READ,
				0, NULL,OPEN_EXISTING, FILE_ATTRIBUTE_DEVICE, NULL);
			
			if (porttalkhandle == INVALID_HANDLE_VALUE) {
				// bullshit
				LOG_MSG(
					"Porttalk driver could not be opened after being started successully.");
				return false;
			}

		}
		for(int i = 0; i < sizeof(ioperm); i++) ioperm[i]=0xff;
		int retval;

		DeviceIoControl(	porttalkhandle,
				IOCTL_IOPM_RESTRICT_ALL_ACCESS,
				NULL,0,
				NULL,0,
				(LPDWORD)&retval,
				NULL);
	}
	return true;
error:
	if(PorttalkService) CloseServiceHandle(PorttalkService);
	if(ServiceManager) CloseServiceHandle(ServiceManager);
	return false;
}
void addIOPermission(Bit16u port) {
	if(isNT)
		ioperm[(port>>3)]&=(~(1<<(port&0x7)));
}

bool setPermissionList() {
	if(!isNT) return true;
	if(porttalkhandle!=INVALID_HANDLE_VALUE) {
		permblock b;
		int pid = _getpid();
		int reetval=0;
		Bit32u retval=0;
		//output permission list to driver
		for(int i = 0; i < sizeof(ioperm);i++) {
			b.offset=i;
			b.value=ioperm[i];
			
			retval=DeviceIoControl(	porttalkhandle,
							IOCTL_SET_IOPM,
							(LPDWORD)&b,3,
							NULL,0,
							(LPDWORD)&reetval,
							NULL);
			if(retval==0) return false;
		}
		
		
		reetval=DeviceIoControl(	porttalkhandle,
							IOCTL_ENABLE_IOPM_ON_PROCESSID,
							(LPDWORD)&pid,4,
							NULL,0,
							(LPDWORD)&retval,
							NULL);
		SDL_Delay(100);
		return reetval!=0;
	}
	else return false;
}
#endif

#ifdef LINUX
// This Linux ioperm only works up to port 0x3FF
#include <sys/perm.h>

bool initPorttalk() {
	if(ioperm(0x3da,1,1) < 0) return false;
	return true;
}

void addIOPermission(Bit16u port) {
	ioperm(port,1,1);
}

bool setPermissionList() {
	return true;
}

#endif
