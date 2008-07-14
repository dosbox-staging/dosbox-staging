/*
 *  Copyright (C) 2002-2007  The DOSBox Team
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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <dirent.h>

#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "pic.h"
#include "hardware.h"
#include "setup.h"
#include "mapper.h"
#include "mem.h"

/* 
	Thanks to vdmsound for nice simple way to implement this
*/

#define logerror


#ifdef _MSC_VER
#pragma pack (1)
#endif

#define HW_OPL2 0
#define HW_DUALOPL2 1
#define HW_OPL3 2

struct RawHeader {
	Bit8u id[8];				/* 0x00, "DBRAWOPL" */
	Bit16u versionHigh;			/* 0x08, size of the data following the m */
	Bit16u versionLow;			/* 0x0a, size of the data following the m */
	Bit32u commands;			/* 0x0c, Bit32u amount of command/data pairs */
	Bit32u milliseconds;		/* 0x10, Bit32u Total milliseconds of data in this chunk */
	Bit8u hardware;				/* 0x14, Bit8u Hardware Type 0=opl2,1=dual-opl2,2=opl3 */
	Bit8u format;				/* 0x15, Bit8u Format 0=cmd/data interleaved, 1 maybe all cdms, followed by all data */
	Bit8u compression;			/* 0x16, Bit8u Compression Type, 0 = No Compression */
	Bit8u delay256;				/* 0x17, Bit8u Delay 1-256 msec command */
	Bit8u delayShift8;			/* 0x18, Bit8u (delay + 1)*256 */			
	Bit8u conversionTableSize;	/* 0x191, Bit8u Raw Conversion Table size */
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack()
#endif
/*
	The Raw Tables is < 128 and is used to convert raw commands into a full register index 
	When the high bit of a raw command is set it indicates the cmd/data pair is to be sent to the 2nd port
	After the conversion table the raw data follows immediatly till the end of the chunk
*/

typedef Bit8u RawCache[2][256];


//Table to map the opl register to one <127 for dro saving
class RawCapture {
	//127 entries to go from raw data to registers
	Bit8u ToReg[127];
	//How many entries in the ToPort are used
	Bit8u RawUsed;
	//256 entries to go from port index to raw data
	Bit8u ToRaw[256];
	Bit8u delay256;
	Bit8u delayShift8;
	RawHeader header;

	FILE*	handle;				//File used for writing
	Bit32u	startTicks;			//Start used to check total raw length on end
	Bit32u	lastTicks;			//Last ticks when last last cmd was added
	Bit8u	buf[1024];	//16 added for delay commands and what not
	Bit32u	bufUsed;
	Bit8u	cmd[2];				//Last cmd's sent to either ports
	bool	doneOpl3;
	bool	doneDualOpl2;

	RawCache* cache;

	void MakeEntry( Bit8u reg, Bit8u& raw ) {
		ToReg[ raw ] = reg;
		ToRaw[ reg ] = raw;
		raw++;
	}
	void MakeTables( void ) {
		Bit8u index = 0;
		memset( ToReg, 0xff, sizeof ( ToReg ) );
		memset( ToRaw, 0xff, sizeof ( ToRaw ) );
		//Select the entries that are valid and the index is the mapping to the index entry
		MakeEntry( 0x01, index );					//0x01: Waveform select
		MakeEntry( 0x04, index );					//104: Four-Operator Enable
		MakeEntry( 0x05, index );					//105: OPL3 Mode Enable
		MakeEntry( 0x08, index );					//08: CSW / NOTE-SEL
		MakeEntry( 0xbd, index );					//BD: Tremolo Depth / Vibrato Depth / Percussion Mode / BD/SD/TT/CY/HH On
		//Add the 32 byte range that hold the 18 operators
		for ( int i = 0 ; i < 24; i++ ) {
			if ( (i & 7) < 6 ) {
				MakeEntry(0x20 + i, index );		//20-35: Tremolo / Vibrato / Sustain / KSR / Frequency Multiplication Facto
				MakeEntry(0x40 + i, index );		//40-55: Key Scale Level / Output Level 
				MakeEntry(0x60 + i, index );		//60-75: Attack Rate / Decay Rate 
				MakeEntry(0x80 + i, index );		//80-95: Sustain Level / Release Rate
				MakeEntry(0xe0 + i, index );		//E0-F5: Waveform Select
			}
		}
		//Add the 9 byte range that hold the 9 channels
		for ( int i = 0 ; i < 9; i++ ) {
			MakeEntry(0xa0 + i, index );			//A0-A8: Frequency Number
			MakeEntry(0xb0 + i, index );			//B0-B8: Key On / Block Number / F-Number(hi bits) 
			MakeEntry(0xc0 + i, index );			//C0-C8: FeedBack Modulation Factor / Synthesis Type
		}
		//Store the amount of bytes the table contains
		RawUsed = index;
//		assert( RawUsed <= 127 );
		delay256 = RawUsed;
		delayShift8 = RawUsed+1; 
	}

	void ClearBuf( void ) {
		fwrite( buf, 1, bufUsed, handle );
		header.commands += bufUsed / 2;
		bufUsed = 0;
	}
	void AddBuf( Bit8u raw, Bit8u val ) {
		buf[bufUsed++] = raw;
		buf[bufUsed++] = val;
		if ( bufUsed >= sizeof( buf ) ) {
			ClearBuf();
		}
	}
	void AddWrite( Bit8u index, Bit8u reg, Bit8u val ) {
		/*
			Do some special checks if we're doing opl3 or dualopl2 commands
			Although you could pretty much just stick to always doing opl3 on the player side
		*/
		if ( index ) {
			//opl3 enabling will always override dual opl
			if ( header.hardware != HW_OPL3 && reg == 4 && val && (*cache)[1][5] ) {
				header.hardware = HW_OPL3;
			} 
			if ( header.hardware == HW_OPL2 && reg >= 0xb0 && reg <=0xb8 && val ) {
				header.hardware = HW_DUALOPL2;
			}
		}
		Bit8u raw = ToRaw[reg];
		if ( raw == 0xff )
			return;
		if ( index )
			raw |= 128;
		AddBuf( raw, val );
	}
	void WriteCache( void  ) {
		Bitu i, val;
		/* Check the registers to add */
		for (i=0;i<256;i++) {
			//Skip the note on entries
			if (i>=0xb0 && i<=0xb8) 
				continue;
			val = (*cache)[0][i];
			if (val) {
				AddWrite( 0, i, val );
			}
			val = (*cache)[1][i];
			if (val) {
				AddWrite( 1, i, val );
			}
		}
	}
	void InitHeader( void ) {
		memset( &header, 0, sizeof( header ) );
		memcpy( header.id, "DBRAWOPL", 8 );
		header.versionLow = 0;
		header.versionHigh = 2;
		header.delay256 = delay256;
		header.delayShift8 = delayShift8;
		header.conversionTableSize = RawUsed;
	}
	void CloseFile( void ) {
		if ( handle ) {
			ClearBuf();
			/* Endianize the header and write it to beginning of the file */
			var_write( &header.versionHigh, header.versionHigh );
			var_write( &header.versionLow, header.versionLow );
			var_write( &header.commands, header.commands );
			var_write( &header.milliseconds, header.milliseconds );
			fseek( handle, 0, SEEK_SET );
			fwrite( &header, 1, sizeof( header ), handle );
			fclose( handle );
			handle = 0;
		}
	}
public:
	bool DoWrite( Bit8u index, Bit8u reg, Bit8u val ) {
		//Check the raw index for this register if we actually have to save it
		if ( handle ) {
			/*
				Check if we actually care for this to be logged, else just ignore it
			*/
			Bit8u raw = ToRaw[reg];
			if ( raw == 0xff ) {
				return true;
			}
			/* Check if this command will not just replace the same value 
			   in a reg that doesn't do anything with it
			*/
			if ( (*cache)[index][reg] == val )
				return true;
			/* Check how much time has passed */
			Bitu passed = PIC_Ticks - lastTicks;
			lastTicks = PIC_Ticks;
			header.milliseconds += passed;

			//if ( passed > 0 ) LOG_MSG( "Delay %d", passed ) ;
			
			// If we passed more than 30 seconds since the last command, we'll restart the the capture
			if ( passed > 30000 ) {
				CloseFile();
				goto skipWrite; 
			}
			while (passed > 0) {
				if (passed < 257) {			//1-256 millisecond delay
					AddBuf( delay256, passed - 1 );
					passed = 0;
				} else {
					Bitu shift = (passed >> 8);
					passed -= shift << 8;
					AddBuf( delayShift8, shift - 1 );
				}
			}
			AddWrite( index, reg, val );
			return true;
		}
skipWrite:
		//Not yet capturing to a file here
		//Check for commands that would start capturing, if it's not one of them return
		if ( !(
			//note on in any channel 
			( reg>=0xb0 && reg<=0xb8 && (val&0x020) ) ||
			//Percussion mode enabled and a note on in any percussion instrument
			( reg == 0xbd && ( (val&0x3f) > 0x20 ) )
		)) {
			return true;
		}
	  	handle = OpenCaptureFile("Raw Opl",".dro");
		if (!handle)
			return false;
		InitHeader();
		//Prepare space at start of the file for the header
		fwrite( &header, 1, sizeof(header), handle );
		/* write the Raw To Reg table */
		fwrite( &ToReg, 1, RawUsed, handle );
		/* Write the cache of last commands */
		WriteCache( );
		/* Write the command that triggered this */
		AddWrite( index, reg, val );
		//Init the timing information for the next commands
		lastTicks = PIC_Ticks;	
		startTicks = PIC_Ticks;
		return true;
	}
	RawCapture( RawCache* _cache ) {
		cache = _cache;
		handle = 0;
		bufUsed = 0;
		MakeTables();
	}
	~RawCapture() {
		CloseFile();
	}

};

#ifdef _MSC_VER
  /* Disable recurring warnings */
# pragma warning ( disable : 4018 )
# pragma warning ( disable : 4244 )
#endif

struct __MALLOCPTR {
	void* m_ptr;

	__MALLOCPTR(void) : m_ptr(NULL) { }
	__MALLOCPTR(void* src) : m_ptr(src) { }
	void* operator=(void* rhs) { return (m_ptr = rhs); }
	operator int*() const { return (int*)m_ptr; }
	operator int**() const { return (int**)m_ptr; }
	operator char*() const { return (char*)m_ptr; }
};

namespace OPL2 {
	#define HAS_YM3812 1
	#include "fmopl.c"
	void TimerOver(Bitu val){
		YM3812TimerOver(val>>8,val & 0xff);
	}
	void TimerHandler(int channel,double interval_Sec) {
		if (interval_Sec==0.0) return;
		PIC_AddEvent(TimerOver,1000.0f*interval_Sec,channel);		
	}
}
#undef OSD_CPU_H
#undef TL_TAB_LEN
namespace THEOPL3 {
	#define HAS_YMF262 1
	#include "ymf262.c"
	void TimerOver(Bitu val){
		YMF262TimerOver(val>>8,val & 0xff);
	}
	void TimerHandler(int channel,double interval_Sec) {
		if (interval_Sec==0.0) return;
		PIC_AddEvent(TimerOver,1000.0f*interval_Sec,channel);		
	}
}

#define OPL2_INTERNAL_FREQ    3600000   // The OPL2 operates at 3.6MHz
#define OPL3_INTERNAL_FREQ    14400000  // The OPL3 operates at 14.4MHz

#define RAW_SIZE 1024
static struct {
	bool active;
	OPL_Mode mode;
	MixerChannel * chan;
	Bit32u last_used;				//Ticks when adlib was last used to turn of mixing after a few second
	Bit16s mixbuf[2][128];			//Mix buffer to mix dual opl2 into final stream
	Bit8u cmd[2];					//Last cmd written to either index
	RawCache cache;
	RawCapture* raw;
} opl;

static void OPL_CallBack(Bitu len) {
	/* Check for size to update and check for 1 ms updates to the opl registers */
	Bitu i;
	switch(opl.mode) {
	case OPL_opl2:
		OPL2::YM3812UpdateOne(0,(OPL2::INT16 *)MixTemp,len);
		opl.chan->AddSamples_m16(len,(Bit16s*)MixTemp);
		break;
	case OPL_opl3:
		THEOPL3::YMF262UpdateOne(0,(OPL2::INT16 *)MixTemp,len);
		opl.chan->AddSamples_s16(len,(Bit16s*)MixTemp);
		break;
	case OPL_dualopl2:
		OPL2::YM3812UpdateOne(0,(OPL2::INT16 *)opl.mixbuf[0],len);
		OPL2::YM3812UpdateOne(1,(OPL2::INT16 *)opl.mixbuf[1],len);
		for (i=0;i<len;i++) {
			((Bit16s*)MixTemp)[i*2+0]=opl.mixbuf[0][i];
			((Bit16s*)MixTemp)[i*2+1]=opl.mixbuf[1][i];
		}
		opl.chan->AddSamples_s16(len,(Bit16s*)MixTemp);
		break;
	}
	if ((PIC_Ticks-opl.last_used)>30000) {
		opl.chan->Enable(false);
		opl.active=false;
	}
}

static Bitu OPL_Read(Bitu port,Bitu iolen) {
	Bitu addr=port & 3;
	switch (opl.mode) {
	case OPL_opl2:
		return OPL2::YM3812Read(0,addr);
	case OPL_dualopl2:
		return OPL2::YM3812Read(addr>>1,addr);
	case OPL_opl3:
		return THEOPL3::YMF262Read(0,addr);
	}
	return 0xff;
}

void OPL_Write(Bitu port,Bitu val,Bitu iolen) {
	opl.last_used=PIC_Ticks;
	if (!opl.active) {
		opl.active=true;
		opl.chan->Enable(true);
	}
	port&=3;
	Bitu index = port>>1;
	if ( port&1 ) {
		Bit8u cmd = opl.cmd[index];
		if ( opl.raw )
			opl.raw->DoWrite( index, cmd, val );
		//Write to the cache after, so the raw can check for unchanged values
		opl.cache[index][cmd]=val;
	} else {
		opl.cmd[ index ] = val;
	}
	switch (opl.mode) {
	case OPL_opl2:
		OPL2::YM3812Write(0,port,val);
		break;
	case OPL_opl3:
		THEOPL3::YMF262Write(0,port,val);
		break;
	case OPL_dualopl2:
		OPL2::YM3812Write( index,port,val);
		break;
	}
}

static void OPL_SaveRawEvent(bool pressed) {
	if (!pressed)
		return;
	/* Check for previously opened wave file */
	if ( opl.raw ) {
		delete opl.raw;
		opl.raw = 0;
		LOG_MSG("Stopped Raw OPL capturing.");
	} else {
		LOG_MSG("Preparing to capture Raw OPL, will start with first note played.");
		opl.raw = new RawCapture( &opl.cache );
	}
}

class OPL: public Module_base {
private:
	IO_ReadHandleObject ReadHandler[3];
	IO_WriteHandleObject WriteHandler[3];
	MixerObject MixerChan;
public:
	static OPL_Mode oplmode;

	OPL(Section* configuration):Module_base(configuration) {
		Section_prop * section=static_cast<Section_prop *>(configuration);
		Bitu base = section->Get_hex("sbbase");
		Bitu rate = section->Get_int("oplrate");
		if (OPL2::YM3812Init(2,OPL2_INTERNAL_FREQ,rate)) {
			E_Exit("Can't create OPL2 Emulator");	
		};
		OPL2::YM3812SetTimerHandler(0,OPL2::TimerHandler,0);
		OPL2::YM3812SetTimerHandler(1,OPL2::TimerHandler,256);
		if (THEOPL3::YMF262Init(1,OPL3_INTERNAL_FREQ,rate)) {
			E_Exit("Can't create OPL3 Emulator");	
		};
		THEOPL3::YMF262SetTimerHandler(0,THEOPL3::TimerHandler,0);
		WriteHandler[0].Install(0x388,OPL_Write,IO_MB,4);
		ReadHandler[0].Install(0x388,OPL_Read,IO_MB,4);
		if (oplmode>=OPL_dualopl2) {
			WriteHandler[1].Install(base,OPL_Write,IO_MB,4);
			ReadHandler[1].Install(base,OPL_Read,IO_MB,4);
		}

		WriteHandler[2].Install(base+8,OPL_Write,IO_MB,2);
		ReadHandler[2].Install(base+8,OPL_Read,IO_MB,2);

		opl.raw = 0;
		opl.active=false;
		opl.last_used=0;
		opl.mode=oplmode;
		memset(&opl.raw,0,sizeof(opl.raw));
		opl.chan=MixerChan.Install(OPL_CallBack,rate,"FM");
		MAPPER_AddHandler(OPL_SaveRawEvent,MK_f7,MMOD1|MMOD2,"caprawopl","Cap OPL");
	}
	~OPL() {
		if (opl.raw) 
			delete opl.raw;
		OPL2::YM3812Shutdown();
		THEOPL3::YMF262Shutdown();
	}
};

static OPL* test;

//Initialize static members
OPL_Mode OPL::oplmode=OPL_none;

void OPL_Init(Section* sec,OPL_Mode oplmode) {
	OPL::oplmode = oplmode;
	test = new OPL(sec);
}

void OPL_ShutDown(Section* sec){
	delete test;
}
