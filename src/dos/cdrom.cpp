/*
 *  Copyright (C) 2002-2003  The DOSBox Team
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

// ASPI support for WIN32 CDROM

#ifdef WIN32

#include <string.h>
#include <ctype.h>
#include "cdrom.h"
#include "scsidefs.h"			// Aspi stuff

#include "dosbox.h"

CDROM_Interface_Aspi::CDROM_Interface_Aspi(void)
{
	hASPI					= NULL;
	hEvent					= NULL;
	pGetASPI32SupportInfo	= NULL;
	pSendASPI32Command		= NULL;
	memset(&oldLeadOut,0,sizeof(oldLeadOut));
};

CDROM_Interface_Aspi::~CDROM_Interface_Aspi(void)
{
	// Stop Audio
	StopAudio();

	pGetASPI32SupportInfo	= NULL;	// clear funcs
	pSendASPI32Command		= NULL;

	if (hASPI) {					// free aspi
		FreeLibrary(hASPI);
		hASPI=NULL;
	}
};

bool GetRegistryValue(HKEY& hKey,char* valueName, char* buffer, ULONG bufferSize)
// hKey has to be open
{	
	// Read subkey
	ULONG valType;
	ULONG result;
	result = RegQueryValueEx(hKey,valueName,NULL,&valType,(unsigned char*)&buffer[0],&bufferSize);
	return (result == ERROR_SUCCESS);
};

BYTE CDROM_Interface_Aspi::GetHostAdapter(char* hardwareID)
{
	SRB_HAInquiry sh;
	SRB_GDEVBlock sd;
	DWORD d		= pGetASPI32SupportInfo();
	int cnt		= LOBYTE(LOWORD(d));
	int i,j,k,max;

	for(i=0; i<cnt; i++) {
		memset(&sh, 0, sizeof(sh));
		sh.SRB_Cmd  = SC_HA_INQUIRY;
		sh.SRB_HaId = i;
		pSendASPI32Command((LPSRB)&sh);
		if (sh.SRB_Status!=SS_COMP) continue;
		
		// Indicates the maximum number of targets the adapter supports
		// If the value is not 8 or 16, then it should be assumed max target is 8
		max = (int)sh.HA_Unique[3];
		if ((max!=8) && (max!=16)) max = 8;
		
		for(j=0; j<max; j++) {
			for(k=0; k<8; k++) {
				memset(&sd, 0, sizeof(sd));
				sd.SRB_Cmd    = SC_GET_DEV_TYPE;
				sd.SRB_HaId   = i;
				sd.SRB_Target = j;
				sd.SRB_Lun    = k;
				pSendASPI32Command((LPSRB)&sd);
				if (sd.SRB_Status == SS_COMP) {
					if (sd.SRB_DeviceType == DTYPE_CDROM) {						
						if ((target==j) && (lun==k)) {
							LOG(LOG_MISC,"SCSI: Getting Hardware vendor.");								
							// "Hardware ID = vendor" match ?
							char vendor[64];
							if (GetVendor(i,target,lun,vendor)) {
								LOG(LOG_MISC,"SCSI: Vendor : %s",vendor);	
								if (strstr(strupr(hardwareID),strupr(vendor))) {
									LOG(LOG_MISC,"SCSI: Host Adapter found: %d",i);								
									return i;								
								}
							};
						}
					}
				}
			}
		}
	}
	LOG(LOG_ERROR,"SCSI: Host Adapter not found: %d",i);									
	return 0;
};

bool CDROM_Interface_Aspi::ScanRegistryFindKey(HKEY& hKeyBase)
// hKey has to be open
{
	FILETIME	time;
	ULONG		result,newKeyResult;
	char		subKey[256];
	char		buffer[256];
	ULONG		bufferSize = 256;
	ULONG		subKeySize = 256;
	HKEY		hNewKey;
	
	ULONG index = 0;
	do {
		result = RegEnumKeyEx (hKeyBase,index,&subKey[0],&subKeySize,NULL,NULL,0,&time);
		if (result==ERROR_SUCCESS) {
			// Open Key...
			newKeyResult = RegOpenKeyEx (hKeyBase,subKey,0,KEY_READ,&hNewKey);
			if (newKeyResult==ERROR_SUCCESS) {
				if (GetRegistryValue(hNewKey,"CurrentDriveLetterAssignment",buffer,256)) {
					LOG(LOG_MISC,"SCSI: Drive Letter found: %s",buffer);					
					// aha, something suspicious...
					if (buffer[0]==letter) {
						char hardwareID[256];
						// found it... lets see if we can get the scsi values				
						bool v1 = GetRegistryValue(hNewKey,"SCSILUN",buffer,256);
						LOG(LOG_MISC,"SCSI: SCSILUN found: %s",buffer);					
						lun		= buffer[0]-'0';
						bool v2 = GetRegistryValue(hNewKey,"SCSITargetID",buffer,256);
						LOG(LOG_MISC,"SCSI: SCSITargetID found: %s",buffer);					
						target  = buffer[0]-'0';
						bool v3 = GetRegistryValue(hNewKey,"HardwareID",hardwareID,256);
						RegCloseKey(hNewKey);
						if (v1 && v2 && v3) {	
							haId = GetHostAdapter(hardwareID);
							return true;
						};
					}
				};
			};
			RegCloseKey(hNewKey);
		};
		index++;
	} while ((result==ERROR_SUCCESS) || (result==ERROR_MORE_DATA));
	return false;
};

bool CDROM_Interface_Aspi::GetVendor(BYTE HA_num, BYTE SCSI_Id, BYTE SCSI_Lun, char* szBuffer) 
{
	SRB_ExecSCSICmd srbExec;
	memset ( &srbExec, 0, sizeof ( SRB_ExecSCSICmd ) );

	hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);

	srbExec.SRB_Cmd			= SC_EXEC_SCSI_CMD ;
	srbExec.SRB_HaId		= HA_num;
	srbExec.SRB_Flags		= SRB_DIR_IN | SRB_EVENT_NOTIFY;
	srbExec.SRB_Target		= SCSI_Id;
	srbExec.SRB_Lun			= SCSI_Lun;
	srbExec.SRB_BufLen		= 36;
	srbExec.SRB_BufPointer	= (unsigned char*)szBuffer;
	srbExec.SRB_SenseLen	= SENSE_LEN;
	srbExec.SRB_CDBLen		= 6;
	srbExec.SRB_PostProc	= (LPVOID)hEvent;
	srbExec.CDBByte [ 0 ]	= SCSI_INQUIRY;
	srbExec.CDBByte [ 4 ]	= 36;  // allocation length per szBuffer [ ]

	ResetEvent(hEvent);
	int dwStatus = pSendASPI32Command ((LPSRB)&srbExec);
//	LOG(LOG_MISC|LOG_ERROR,"SCSI: Get vendor command send");					
	
	if (dwStatus==SS_PENDING) WaitForSingleObject(hEvent,30000);
//	LOG(LOG_MISC|LOG_ERROR,"SCSI: Pending done.");					

	CloseHandle(hEvent);
	if (srbExec.SRB_Status != SS_COMP) {
		strcpy (szBuffer, "error" );
		return false;
	} else {
		strncpy(szBuffer,szBuffer+8,25);
		szBuffer[25] = 0;
		int len = strlen(szBuffer);
		for (int i=0; i<len; i++) if (szBuffer[i]<=32) szBuffer[i]='_';
	};
	return true;
}

bool CDROM_Interface_Aspi::ScanRegistry(HKEY& hKeyBase)
// hKey has to be open
{
	FILETIME	time;
	ULONG		result,newKeyResult;
	char		subKey[256];
	ULONG		subKeySize= 256;
	HKEY		hNewKey;
	
	ULONG index = 0;
	do {
		result = RegEnumKeyEx (hKeyBase,index,&subKey[0],&subKeySize,NULL,NULL,0,&time);
		if ((result==ERROR_SUCCESS) || (result==ERROR_MORE_DATA)) {
			// Open Key...
			newKeyResult = RegOpenKeyEx (hKeyBase,subKey,0,KEY_READ,&hNewKey);
			if (newKeyResult==ERROR_SUCCESS) {
				bool found = ScanRegistryFindKey(hNewKey);
				RegCloseKey(hNewKey);
				if (found) return true;
			};
			RegCloseKey(hNewKey);
		};
		index++;
	} while ((result==ERROR_SUCCESS) || (result==ERROR_MORE_DATA));
	return false;
};

bool CDROM_Interface_Aspi::SetDevice(char* path)
{
	// load WNASPI32.DLL
	hASPI = LoadLibrary ( "WNASPI32.DLL" );
	if (!hASPI) return false;
	// Get Pointer to ASPI funcs
    pGetASPI32SupportInfo	= (DWORD(*)(void))GetProcAddress(hASPI,"GetASPI32SupportInfo");
    pSendASPI32Command		= (DWORD(*)(LPSRB))GetProcAddress(hASPI,"SendASPI32Command");
	if (!pGetASPI32SupportInfo || !pSendASPI32Command) return false;
	// Letter
	letter = toupper(path[0]);

	// Check OS
	OSVERSIONINFO osi;
	osi.dwOSVersionInfoSize = sizeof(osi);
	GetVersionEx(&osi);
	if ((osi.dwPlatformId==VER_PLATFORM_WIN32_NT) && (osi.dwMajorVersion>4)) {
		if (GetDriveType(path)==DRIVE_CDROM) {	
			// WIN XP/NT/2000
			int iDA,iDT,iDL;
			letter = path[0];
			HANDLE hF = OpenIOCTLFile(letter,FALSE);
			GetIOCTLAdapter(hF,&iDA,&iDT,&iDL);
			CloseHandle(hF);
			// Set SCSI IDs
			haId	= iDA;
			target	= iDT;
			lun		= iDL;
			return true;
		}
	} else {
		// win 95/98/ME have to scan the registry...
		// lets hope the layout is always the same... i dunno...
		char key[2048];
		HKEY hKeyBase;
		bool found = false;
		strcpy(key,"ENUM\\SCSI");
		if (RegOpenKeyEx (HKEY_LOCAL_MACHINE,key,0,KEY_READ,&hKeyBase)==ERROR_SUCCESS) {
			found = ScanRegistry(hKeyBase);
		};	
		RegCloseKey(hKeyBase);
		return found;
	} 
	return false;
};

bool CDROM_Interface_Aspi::GetAudioTracks(int& stTrack, int& endTrack, TMSF& leadOut) 
{
	TOC toc;
	if (GetTOC((LPTOC)&toc) == SS_COMP) {
		stTrack		= toc.cFirstTrack;
		endTrack	= toc.cLastTrack;
		leadOut.min	= (unsigned char)(toc.tracks[endTrack].lAddr >>  8) &0xFF;
		leadOut.sec	= (unsigned char)(toc.tracks[endTrack].lAddr >> 16) &0xFF;
		leadOut.fr	= (unsigned char)(toc.tracks[endTrack].lAddr >> 24) &0xFF;
		return true;
	}
	return false;
};

bool CDROM_Interface_Aspi::GetAudioTrackInfo	(int track, TMSF& start, unsigned char& attr)
{
	TOC toc;
	if (GetTOC((LPTOC)&toc) == SS_COMP) {
		start.min	= (unsigned char)(toc.tracks[track-1].lAddr >>  8) &0xFF;
		start.sec	= (unsigned char)(toc.tracks[track-1].lAddr >> 16) &0xFF;
		start.fr	= (unsigned char)(toc.tracks[track-1].lAddr >> 24) &0xFF;
		attr		= toc.tracks[track-1].cAdrCtrl;
		return true;
	};		
	return false;
};

HANDLE CDROM_Interface_Aspi::OpenIOCTLFile(char cLetter,BOOL bAsync)
{
	HANDLE hF;
	char szFName[16];
	OSVERSIONINFO ov;
	DWORD dwFlags;
	DWORD dwIOCTLAttr;
//	if(bAsync) dwIOCTLAttr=FILE_FLAG_OVERLAPPED;
//	else       
	dwIOCTLAttr=0;

	memset(&ov,0,sizeof(OSVERSIONINFO));
	ov.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
	GetVersionEx(&ov);

	if ((ov.dwPlatformId==VER_PLATFORM_WIN32_NT) && (ov.dwMajorVersion>4))
		dwFlags = GENERIC_READ|GENERIC_WRITE;            // add gen write on W2k/XP
	else
		dwFlags = GENERIC_READ;

	wsprintf(szFName, "\\\\.\\%c:",cLetter);

	hF=CreateFile(szFName,dwFlags,FILE_SHARE_READ,        // open drive
				NULL,OPEN_EXISTING,dwIOCTLAttr,NULL);

	if (hF==INVALID_HANDLE_VALUE) {
		dwFlags^=GENERIC_WRITE;                         // mmm... no success
		hF=CreateFile(szFName,dwFlags,FILE_SHARE_READ,      // -> open drive again
					NULL,OPEN_EXISTING,dwIOCTLAttr,NULL);
		if (hF==INVALID_HANDLE_VALUE) return NULL;
	}
	return hF;                                          
}

void CDROM_Interface_Aspi::GetIOCTLAdapter(HANDLE hF,int * iDA,int * iDT,int * iDL)
{
	char szBuf[1024];
	PSCSI_ADDRESS pSA;
	DWORD dwRet;

	*iDA=*iDT=*iDL=-1;
	if(hF==NULL) return;

	memset(szBuf,0,1024);

	pSA=(PSCSI_ADDRESS)szBuf;
	pSA->Length=sizeof(SCSI_ADDRESS);
                                               
	if(!DeviceIoControl(hF,IOCTL_SCSI_GET_ADDRESS,NULL,
					 0,pSA,sizeof(SCSI_ADDRESS),
					 &dwRet,NULL))
	return;

	*iDA = pSA->PortNumber;
	*iDT = pSA->TargetId;
	*iDL = pSA->Lun;
}

DWORD CDROM_Interface_Aspi::GetTOC(LPTOC toc)
{
	SRB_ExecSCSICmd s;DWORD dwStatus;

	hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);

	memset(&s,0,sizeof(s));

	s.SRB_Cmd        = SC_EXEC_SCSI_CMD;
	s.SRB_HaId       = haId;
	s.SRB_Target     = target;
	s.SRB_Lun        = lun;
	s.SRB_Flags      = SRB_DIR_IN | SRB_EVENT_NOTIFY;
	s.SRB_BufLen     = sizeof(*toc);
	s.SRB_BufPointer = (BYTE FAR *)toc;
	s.SRB_SenseLen   = SENSE_LEN;
	s.SRB_CDBLen     = 0x0A;
	s.SRB_PostProc   = (LPVOID)hEvent;
	s.CDBByte[0]     = SCSI_READ_TOC;
	s.CDBByte[1]     = 0x02; // 0x02 for MSF
	s.CDBByte[7]     = 0x03;
	s.CDBByte[8]     = 0x24;

	ResetEvent(hEvent);
	dwStatus=pSendASPI32Command((LPSRB)&s);

	if (dwStatus==SS_PENDING) WaitForSingleObject(hEvent,30000);

	CloseHandle(hEvent);

	return (s.SRB_Status==SS_COMP);
}

bool CDROM_Interface_Aspi::PlayAudioSector(unsigned long start,unsigned long len)
{
	SRB_ExecSCSICmd s;DWORD dwStatus;

	hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);

	memset(&s,0,sizeof(s));
	s.SRB_Cmd        = SC_EXEC_SCSI_CMD;
	s.SRB_HaId       = haId;
	s.SRB_Target     = target;
	s.SRB_Lun        = lun;
	s.SRB_Flags      = SRB_DIR_IN | SRB_EVENT_NOTIFY;
	s.SRB_BufLen     = 0;
	s.SRB_BufPointer = 0;
	s.SRB_SenseLen   = SENSE_LEN;
	s.SRB_CDBLen     = 12;
	s.SRB_PostProc   = (LPVOID)hEvent;

	s.CDBByte[0]     = SCSI_PLAYAUD_12;
	s.CDBByte[1]     = lun << 5;
	s.CDBByte[2]     = (unsigned char)((start >> 24) & 0xFF);
	s.CDBByte[3]     = (unsigned char)((start >> 16) & 0xFF);
	s.CDBByte[4]     = (unsigned char)((start >> 8) & 0xFF);
	s.CDBByte[5]     = (unsigned char)((start & 0xFF));
	s.CDBByte[6]     = (unsigned char)((len >> 24) & 0xFF);
	s.CDBByte[7]     = (unsigned char)((len >> 16) & 0xFF);
	s.CDBByte[8]     = (unsigned char)((len >> 8) & 0xFF);
	s.CDBByte[9]     = (unsigned char)(len & 0xFF);

	ResetEvent(hEvent);

	dwStatus = pSendASPI32Command((LPSRB)&s);

	if(dwStatus==SS_PENDING) WaitForSingleObject(hEvent,10000);

	CloseHandle(hEvent);

	return s.SRB_Status==SS_COMP;
}

bool CDROM_Interface_Aspi::StopAudio(void)
{
	return PauseAudio(false);
};

bool CDROM_Interface_Aspi::PauseAudio(bool resume)
{
	SRB_ExecSCSICmd s;DWORD dwStatus;

	hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);

	memset(&s,0,sizeof(s));

	s.SRB_Cmd        = SC_EXEC_SCSI_CMD;
	s.SRB_HaId       = haId;
	s.SRB_Target     = target;
	s.SRB_Lun        = lun;
	s.SRB_Flags      = SRB_DIR_IN | SRB_EVENT_NOTIFY;
	s.SRB_BufLen     = 0x00;
	s.SRB_SenseLen   = SENSE_LEN;
	s.SRB_CDBLen     = 0x0A;
	s.SRB_PostProc   = (LPVOID)hEvent;
	s.CDBByte[0]     = 0x4B;
	s.CDBByte[8]     = (unsigned char)resume;				// Pause

	ResetEvent(hEvent);
	dwStatus=pSendASPI32Command((LPSRB)&s);

	if (dwStatus==SS_PENDING) WaitForSingleObject(hEvent,30000);

	CloseHandle(hEvent);

	return (s.SRB_Status==SS_COMP);
};

bool CDROM_Interface_Aspi::GetAudioSub(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos)
{
	SUB_Q_CURRENT_POSITION pos;
	SRB_ExecSCSICmd s;DWORD dwStatus;
	
	hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);

	memset(&s,0,sizeof(s));

	s.SRB_Cmd        = SC_EXEC_SCSI_CMD;
	s.SRB_HaId       = haId;
	s.SRB_Target     = target;
	s.SRB_Lun        = lun;
	s.SRB_Flags      = SRB_DIR_IN | SRB_EVENT_NOTIFY;
	s.SRB_SenseLen   = SENSE_LEN;

	s.SRB_BufLen     = sizeof(pos);
	s.SRB_BufPointer = (BYTE FAR *)&pos;
	s.SRB_CDBLen     = 10;
	s.SRB_PostProc   = (LPVOID)hEvent;

	s.CDBByte[0]     = SCSI_SUBCHANNEL;
	s.CDBByte[1]     = (lun<<5)|2;   // lun & msf
	s.CDBByte[2]     = 0x40;            // subq
	s.CDBByte[3]     = 0x01;            // curr pos info
	s.CDBByte[6]     = 0;               // track number (only in isrc mode, ignored)
	s.CDBByte[7]     = 0;               // alloc len
	s.CDBByte[8]     = sizeof(pos);		

	ResetEvent(hEvent);

	dwStatus = pSendASPI32Command((LPSRB)&s);

	if (dwStatus==SS_PENDING) WaitForSingleObject(hEvent,0xFFFFFFFF);

	CloseHandle(hEvent);
	
	if (s.SRB_Status!=SS_COMP) return false;

	attr		= (pos.ADR<<4) | pos.Control;
	track		= pos.TrackNumber;
	index		= pos.IndexNumber;
	absPos.min	= pos.AbsoluteAddress[1];
	absPos.sec	= pos.AbsoluteAddress[2];
	absPos.fr	= pos.AbsoluteAddress[3];
	relPos.min	= pos.TrackRelativeAddress[1];
	relPos.sec	= pos.TrackRelativeAddress[2];
	relPos.fr	= pos.TrackRelativeAddress[3];
	
	return true;
};

bool CDROM_Interface_Aspi::GetUPC(unsigned char& attr, char* upcdata)
{
	SUB_Q_MEDIA_CATALOG_NUMBER upc;
	SRB_ExecSCSICmd s;DWORD dwStatus;

	hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);

	memset(&s,0,sizeof(s));
	
	s.SRB_Cmd        = SC_EXEC_SCSI_CMD;
	s.SRB_HaId       = haId;
	s.SRB_Target     = target;
	s.SRB_Lun        = lun;
	s.SRB_Flags      = SRB_DIR_IN | SRB_EVENT_NOTIFY;
	s.SRB_SenseLen   = SENSE_LEN;

	s.SRB_BufLen     = sizeof(upc);
	s.SRB_BufPointer = (BYTE FAR *)&upc;
	s.SRB_CDBLen     = 10;
	s.SRB_PostProc   = (LPVOID)hEvent;

	s.CDBByte[0]     = SCSI_SUBCHANNEL;
	s.CDBByte[1]     = (lun<<5)|2;   // lun & msf
	s.CDBByte[2]     = 0x40;            // subq
	s.CDBByte[3]     = 0x02;            // get upc
	s.CDBByte[6]     = 0;               // track number (only in isrc mode, ignored)
	s.CDBByte[7]     = 0;               // alloc len
	s.CDBByte[8]     = sizeof(upc);		

	ResetEvent(hEvent);

	dwStatus = pSendASPI32Command((LPSRB)&s);

	if (dwStatus==SS_PENDING) WaitForSingleObject(hEvent,0xFFFFFFFF);

	CloseHandle(hEvent);

	if (s.SRB_Status!=SS_COMP) return false;

//	attr = (upc.ADR<<4) | upc.Control;
	attr	= 0;
	int pos	= 0;
	// Convert to mscdex format
//	for (int i=0; i<6; i++) upcdata[i] = (upc.MediaCatalog[pos++]<<4)+(upc.MediaCatalog[pos++]&0x0F);
//	upcdata[6] = (upc.MediaCatalog[pos++]<<4);
	for (int i=0; i<7; i++) upcdata[i] = upc.MediaCatalog[i];

	return true;
};

bool CDROM_Interface_Aspi::GetAudioStatus(bool& playing, bool& pause)
{
	playing = pause = false;
	
	SUB_Q_HEADER sub;
	SRB_ExecSCSICmd s;DWORD dwStatus;

	hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);

	memset(&s,0,sizeof(s));
	
	s.SRB_Cmd        = SC_EXEC_SCSI_CMD;
	s.SRB_HaId       = haId;
	s.SRB_Target     = target;
	s.SRB_Lun        = lun;
	s.SRB_Flags      = SRB_DIR_IN | SRB_EVENT_NOTIFY;
	s.SRB_SenseLen   = SENSE_LEN;

	s.SRB_BufLen     = sizeof(sub);
	s.SRB_BufPointer = (BYTE FAR *)&sub;
	s.SRB_CDBLen     = 10;
	s.SRB_PostProc   = (LPVOID)hEvent;

	s.CDBByte[0]     = SCSI_SUBCHANNEL;
	s.CDBByte[1]     = (lun<<5)|2;   // lun & msf
	s.CDBByte[2]     = 0x00;            // no subq
	s.CDBByte[3]     = 0x00;            // dont care
	s.CDBByte[6]     = 0;               // track number (only in isrc mode, ignored)
	s.CDBByte[7]     = 0;               // alloc len
	s.CDBByte[8]     = sizeof(sub);		

	ResetEvent(hEvent);

	dwStatus = pSendASPI32Command((LPSRB)&s);

	if (dwStatus==SS_PENDING) WaitForSingleObject(hEvent,0xFFFFFFFF);

	CloseHandle(hEvent);

	if (s.SRB_Status!=SS_COMP) return false;

	playing			= (sub.AudioStatus==0x11);
	pause			= (sub.AudioStatus==0x12);

	return true;
};

bool CDROM_Interface_Aspi::LoadUnloadMedia(bool unload)
{
	SRB_ExecSCSICmd s;DWORD dwStatus;

	hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);

	memset(&s,0,sizeof(s));
	
	s.SRB_Cmd        = SC_EXEC_SCSI_CMD;
	s.SRB_HaId       = haId;
	s.SRB_Target     = target;
	s.SRB_Lun        = lun;
	s.SRB_Flags      = SRB_DIR_IN | SRB_EVENT_NOTIFY;
	s.SRB_SenseLen   = SENSE_LEN;

	s.SRB_BufLen     = 0;
	s.SRB_BufPointer = 0;
	s.SRB_CDBLen     = 14;
	s.SRB_PostProc   = (LPVOID)hEvent;

	s.CDBByte[0]     = SCSI_LOAD_UN;
	s.CDBByte[1]     = (lun<<5)|1; // lun & immediate
	s.CDBByte[4]     = (unload ? 0x02:0x03);		// unload/load media
	
	ResetEvent(hEvent);

	dwStatus = pSendASPI32Command((LPSRB)&s);

	if (dwStatus==SS_PENDING) WaitForSingleObject(hEvent,0xFFFFFFFF);

	CloseHandle(hEvent);

	if (s.SRB_Status!=SS_COMP) return false;

	return true;
};

bool CDROM_Interface_Aspi::GetMediaTrayStatus(bool& mediaPresent, bool& mediaChanged, bool& trayOpen)
{
	// Seems not possible to get this values using ioctl...
	int		track1,track2;
	TMSF	leadOut;
	// If we can read, there's a media
	mediaPresent = GetAudioTracks(track1, track2, leadOut),
	trayOpen	 = !mediaPresent;
	mediaChanged = (oldLeadOut.min!=leadOut.min) || (oldLeadOut.sec!=leadOut.sec) || (oldLeadOut.fr!=leadOut.fr);
	// Save old values
	oldLeadOut.min = leadOut.min;
	oldLeadOut.sec = leadOut.sec;
	oldLeadOut.fr  = leadOut.fr;
	// always success
	return true;
};

bool CDROM_Interface_Aspi::ReadSectors(void* buffer, bool raw, unsigned long sector, unsigned long num)
{
	SRB_ExecSCSICmd s;DWORD dwStatus;

	hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);

	memset(&s,0,sizeof(s));

	// FIXME : Is there a method to get cooked sectors with aspi ???
	//         all combination i tried were failing.
	//         so we have to allocate extra mem and copy data to buffer if in cooked mode
	char*		inPtr = (char*)buffer;
	if (!raw)	inPtr = new char[num*2352];
	if (!inPtr) return false;

	s.SRB_Cmd        = SC_EXEC_SCSI_CMD;
	s.SRB_HaId       = haId;
	s.SRB_Target     = target;
	s.SRB_Lun        = lun;
	s.SRB_Flags      = SRB_DIR_IN | SRB_EVENT_NOTIFY;
	s.SRB_SenseLen   = SENSE_LEN;

	s.SRB_BufLen     = 2352*num; //num*(raw?2352:2048);
	s.SRB_BufPointer = (BYTE FAR*)inPtr;
	s.SRB_CDBLen     = 12;
	s.SRB_PostProc   = (LPVOID)hEvent;

	s.CDBByte[0]     = 0xBE;
	s.CDBByte[2]     = (unsigned char)((sector >> 24) & 0xFF);
	s.CDBByte[3]     = (unsigned char)((sector >> 16) & 0xFF);
	s.CDBByte[4]     = (unsigned char)((sector >> 8) & 0xFF);
	s.CDBByte[5]     = (unsigned char)((sector & 0xFF));
	s.CDBByte[6]     = (unsigned char)((num >> 16) & 0xFF);
	s.CDBByte[7]     = (unsigned char)((num >>  8) & 0xFF);
	s.CDBByte[8]     = (unsigned char) (num & 0xFF);
	s.CDBByte[9]	 = (raw?0xF0:0x10);

	ResetEvent(hEvent);

	dwStatus = pSendASPI32Command((LPSRB)&s);

	if (dwStatus==SS_PENDING) WaitForSingleObject(hEvent,0xFFFFFFFF);

	CloseHandle(hEvent);

	if (s.SRB_Status!=SS_COMP) {
		if (!raw) delete[] inPtr;
		return false;
	}

	if (!raw) {
		// copy user data to buffer
		char* source = inPtr;
		source+=16; // jump 16 bytes
		char* outPtr = (char*)buffer;
		for (unsigned long i=0; i<num; i++) {
			memcpy(outPtr,source,2048);
			outPtr+=COOKED_SECTOR_SIZE;
			source+=RAW_SECTOR_SIZE;
		};
		delete[] inPtr;
	};	
	return true;
};

int CDROM_GetMountType(char* path)
// 0 - physical CDROM
// 1 - Iso file
// 2 - subdirectory
{
	// 1. Smells like a real cdrom 
	if ((strlen(path)<=3) && (path[2]=='\\') && (strchr(path,'\\')==strrchr(path,'\\')) && 	(GetDriveType(path)==DRIVE_CDROM)) return 0;
	// 2. Iso file ?
	// FIXME : How to detect them ?
	// return 1;
	// 3. bah, ordinary directory
	return 2;
};

#else

#include "cdrom.h"

#endif

// ******************************************************
// Fake CDROM
// ******************************************************

bool CDROM_Interface_Fake :: GetAudioTracks	(int& stTrack, int& end, TMSF& leadOut)
{
	stTrack = end = 1;
	leadOut.min	= 60;
	leadOut.sec = leadOut.fr = 0;
	return true;
};

bool CDROM_Interface_Fake :: GetAudioTrackInfo	(int track, TMSF& start, unsigned char& attr)
{
	if (track>1) return false;
	start.min = start.fr = 0;
	start.sec = 2;
	attr	  = 0x60; // data / permitted
	return true;
};

bool CDROM_Interface_Fake :: GetAudioSub (unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos)
{
	attr	= 0;
	track	= index = 1;
	relPos.min = relPos.fr = 0; relPos.sec = 2;
	absPos.min = absPos.fr = 0; absPos.sec = 2;
	return true;
}

bool CDROM_Interface_Fake :: GetAudioStatus	(bool& playing, bool& pause) 
{
	playing = pause = false;
	return true;
}

bool CDROM_Interface_Fake :: GetMediaTrayStatus	(bool& mediaPresent, bool& mediaChanged, bool& trayOpen)
{
	mediaPresent = true;
	mediaChanged = false;
	trayOpen	 = false;
	return true;
};


