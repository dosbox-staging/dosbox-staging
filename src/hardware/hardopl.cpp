#include "config.h"
#include "../libs/porttalk/porttalk.h"
#include "inout.h"
#include "logging.h"
#include "pic.h"
#include "hardware.h"
#include <stdio.h>

#if defined (WIN32) || defined (LINUX)

static Bit16s hardopldiff;
static bool isCMS;
static FILE * logfp = NULL;

static void write_hwio(Bitu port,Bitu val,Bitu /*iolen*/) {
	if(port<0x388) port += hardopldiff;
	//LOG_MSG("write port %x",port);
	outportb(port, val);
}

static Bitu read_hwio(Bitu port,Bitu /*iolen*/) {
	if(port<0x388) port += hardopldiff;
	//LOG_MSG("read port %x",port);
	Bitu retval = inportb(port);
	return retval;

}

// handlers for Gameblaster passthrough

static void write_hwcmsio(Bitu port,Bitu val,Bitu /*iolen*/) {
	if(logfp) fprintf(logfp,"%4.3f w % 3x % 2x\r\n",PIC_FullIndex(),port,val);
	outportb(port + hardopldiff, val);
}

static Bitu read_hwcmsio(Bitu port,Bitu /*iolen*/) {
	Bitu retval = inportb(port + hardopldiff);
	if(logfp) fprintf(logfp,"%4.3f r\t\t% 3x % 2x\r\n",PIC_FullIndex(),port,retval);
	return retval;
}

bool hwopl_dirty=false;

static IO_ReadHandleObject* hwOPL_ReadHandler[16] ;
static IO_WriteHandleObject* hwOPL_WriteHandler[16];

const Bit16u oplports[]={
		0x0,0x1,0x2,0x3,0x8,0x9,
		0x388,0x389,0x38A,0x38B};

void HARDOPL_Init(Bitu hardwareaddr, Bitu blasteraddr, bool isCMSp) {
	isCMS = isCMSp;

	if(!(hardwareaddr==0x210 || hardwareaddr==0x220 || hardwareaddr==0x230 ||
		 hardwareaddr==0x240 || hardwareaddr==0x250 || hardwareaddr==0x260 ||
		 hardwareaddr==0x280)) {
			LOG_MSG("OPL passthrough: Invalid hardware base address. Aborting.");
			return;
		 }
	
	if(!initPorttalk()) {
#ifdef WIN32		
		LOG_MSG("OPL passthrough: Porttalk not loaded. Aborting.");
#endif
#ifdef LINUX
		LOG_MSG("OPL passthrough: permission denied. Aborting.");
#endif
		return;
	}

	hardopldiff=hardwareaddr-blasteraddr;
	hwopl_dirty=true;

	// map the port
	LOG_MSG("Port mappings hardware -> DOSBox:");

	if(isCMS) {
		logfp=OpenCaptureFile("Portlog",".portlog.txt");
		hwOPL_ReadHandler[0]=new IO_ReadHandleObject();
		hwOPL_WriteHandler[0]=new IO_WriteHandleObject();
		hwOPL_ReadHandler[0]->Install(blasteraddr, read_hwcmsio, IO_MB, 16);
		hwOPL_WriteHandler[0]->Install(blasteraddr, write_hwcmsio, IO_MB, 16);
		for(int i = 0; i < 0x10; i++)	{
			addIOPermission(hardwareaddr+i);
		}
		LOG_MSG("%x-%x -> %x-%x",hardwareaddr,hardwareaddr+15,blasteraddr,blasteraddr+15);
	} else {
		for(int i = 0; i < 10; i++)	{
			hwOPL_ReadHandler[i]=new IO_ReadHandleObject();
			hwOPL_WriteHandler[i]=new IO_WriteHandleObject();
			Bit16u port=oplports[i];
			if(i<6) port+=blasteraddr; 
			hwOPL_ReadHandler[i]->Install(port,read_hwio,IO_MB);
			hwOPL_WriteHandler[i]->Install(port,write_hwio,IO_MB);

			if(i<6) port+=hardopldiff;
			LOG_MSG("%x -> %x",port,i<6?port-hardopldiff:port);
			addIOPermission(port);
		}
	}
	setPermissionList();
}

void HWOPL_Cleanup() {
	if(logfp) fclose(logfp);
	if(hwopl_dirty) {
		for(int i = 0; i < 10; i++) {
			delete hwOPL_ReadHandler[i];
			delete hwOPL_WriteHandler[i];
		}
		hwopl_dirty=false;
	}
}
#else

void HWOPL_Cleanup() {}
void HARDOPL_Init(Bitu hardwareaddr, Bitu blasteraddr, bool isCMSp) {
	LOG_MSG("OPL passthrough is not supported on this operating system.");
}
#endif
