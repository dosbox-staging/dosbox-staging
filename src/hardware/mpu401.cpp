#include <string.h>
#include "dosbox.h"
#include "inout.h"
#include "pic.h"
#include "setup.h"
#include "cpu.h"
#include "callback.h"

void MIDI_RawOutByte(Bit8u data);
bool MIDI_Available(void);

static Bitu call_irq9;
static void MPU401_Event(Bitu);
static void MPU401_Reset(void);
static void MPU401_EOIHandler(void);

#define MPU_QUEUE 32
#define MPU_RQ_QUEUE 64
#define TIMECONSTANT 60000000

enum MpuMode { M_UART,M_INTELLIGENT } ;
enum MpuDataType {UNSET,OVERFLOW,MARK,MIDI_SYS,MIDI_NORM,MIDI_DATA,COMMAND};

/////////////////////////////////////////////////////////////////////////////
// I/O
/////////////////////////////////////////////////////////////////////////////

#define MPU_STATUS_DSR   (1 << 7)
#define MPU_STATUS_DRR   (1 << 6)
#define MPU_STATUS_PAD   (0xff & (~(MPU_STATUS_DRR | MPU_STATUS_DSR)))

#define MK_MPU_STATUS(dsr, drr)\
  (((dsr) ? 0 : MPU_STATUS_DSR) | ((drr) ? 0 : MPU_STATUS_DRR) | MPU_STATUS_PAD)


/////////////////////////////////////////////////////////////////////////////
// Commands
/////////////////////////////////////////////////////////////////////////////

/** Copyright notice for MPU-401 intelligent-mode command constants *********

  MPU-401 MIDI Interface Module v1.0
  Copyright (c) 1991, Robin Davies. All Rights Reserved.

    Robin Davies
    224 3rd Avenue
    Ottawa, Ontario
    Canada. K1S 2K3.

  updated by:

  Larry Troxler, Compuserve 73520,1736


  15.02.2004: updated and implemented intelligent mode

****************************************************************************/

// Start/Stop Commands

#define CMD_MIDI_STOP                       0x01
#define CMD_MIDI_START                      0x02
#define CMD_MIDI_CONTINUE                   0x03

#define CMD_PLAY_STOP                       0x04
#define CMD_PLAY_START                      0x08
#define CMD_PLAY_CONTINUE                   0x0c

#define CMD_RECORD_STOP                     0x10
#define CMD_RECORD_START                    0x20

// Commands

#define CMD_DISABLE_ALL_NOTES_OFF           0x30
#define CMD_DISABLE_REAL_TIME_OUT           0x32
#define CMD_DISABLE_ALL_THRU_OFF            0x33
#define CMD_TIMING_BYTE_ALWAYS              0x34
#define CMD_MODE_MESS_ON                    0x35
#define CMD_EXCLUSIVE_THRU_ON               0x37
#define CMD_COMMON_TO_HOST_ON               0x38
#define CMD_REAL_TIME_TO_HOST_ON            0x39
#define CMD_UART_MODE                       0x3f

#define CMD_INT_CLOCK                       0x80
#define CMD_FSK_CLOCK                       0x81
#define CMD_MIDI_CLOCK                      0x82
#define CMD_METRONOME_ON                    0x83
#define CMD_METRONOME_OFF                   0x84
#define CMD_METRONOME_W_ACCENTS             0x85
#define CMD_BENDER_OFF                      0x86
#define CMD_BENDER_ON                       0x87
#define CMD_MIDI_THRU_OFF                   0x88
#define CMD_MIDI_THRU_ON                    0x89
#define CMD_DATA_IN_STOP_MODE_OFF           0x8a
#define CMD_DATA_IN_STOP_MODE_ON            0x8b
#define CMD_SEND_MEASURE_END_OFF            0x8c
#define CMD_SEND_MEASURE_END_ON             0x8d
#define CMD_CONDUCTOR_OFF                   0x8e
#define CMD_CONDUCTOR_ON                    0x8f
#define CMD_REAL_TIME_AFFECTION_OFF         0x90
#define CMD_REAL_TIME_AFFECTION_ON          0x91
#define CMD_FSK_TO_INTERNAL                 0x92
#define CMD_FSK_TO_MIDI                     0x93
#define CMD_CLOCK_TO_HOST_OFF               0x94
#define CMD_CLOCK_TO_HOST_ON                0x95
#define CMD_EXCLUSIVE_TO_HOST_OFF           0x96
#define CMD_EXCLUSIVE_TO_HOST_ON            0x97

#define CMD_RESET_RELATIVE_TEMPO            0xb1
#define CMD_CLEAR_PLAY_COUNTERS             0xb8
#define CMD_CLEAR_PLAY_MAP                  0xb9
#define CMD_CLEAR_RECORD_COUNTER            0xba
#define CMD_TIMEBASE_48                     0xc2
#define CMD_TIMEBASE_72                     0xc3
#define CMD_TIMEBASE_96                     0xc4
#define CMD_TIMEBASE_120                    0xc5
#define CMD_TIMEBASE_144                    0xc6
#define CMD_TIMEBASE_168                    0xc7
#define CMD_TIMEBASE_192                    0xc8

#define CMD_REQUEST_TO_SEND_DATA            0xd0 /* + track #! */
#define CMD_REQUEST_TO_SEND_SYSTEM_MSG      0xdf

#define CMD_SET_TEMPO                       0xe0
#define CMD_RELATIVE_TEMPO                  0xe1
#define CMD_RELATIVE_TEMPO_GRADUATION       0xe2
#define CMD_MIDI_METRONOME                  0xe4
#define CMD_MEASURE_LENGTH                  0xe6
#define CMD_INTERNAL_CLOCK_LENGTH_TO_HOST   0xe7
#define CMD_ACTIVE_TRACK_MASK               0xec
#define CMD_SEND_PLAY_COUNTER_MASK          0xed
#define CMD_MIDI_CHANNEL_MASK_LO            0xee
#define CMD_MIDI_CHANNEL_MASK_HI            0xef

#define CMD_EOX                             0xf7 
#define CMD_TIMING_OVERFLOW                 0xf8 
#define CMD_MPU_MARK                        0xfc 
#define CMD_RESET                           0xff

// Commands that return data

#define CMD_REQUEST_PLAY_COUNTER            0xa0 /* + track # */
#define CMD_REQUEST_AND_CLEAR_REC_COUNTER   0xab
#define CMD_REQUEST_VERSION                 0xac
#define CMD_REQUEST_REVISION                0xad
#define CMD_REQUEST_TEMPO                   0xaf


/////////////////////////////////////////////////////////////////////////////
// Messages
/////////////////////////////////////////////////////////////////////////////

#define MSG_TIMING_OVERFLOW                 0xf8
#define MSG_ALL_END                         0xfc
#define MSG_CLOCK_TO_HOST                   0xfd
#define MSG_CMD_ACK                         0xfe
#define MSG_REQUEST_DATA                    0xf0
#define MSG_REQUEST_COMMAND                 0xf9

#define MPU_VERSION                         0x15
#define MPU_REVISION                        0x01

/////////////////////////////////////////////////////////////////////////////

static struct {
	bool intelligent;
	MpuMode mode;
	Bitu irq;
	Bit8u queue[MPU_QUEUE];
	Bitu queue_pos,queue_used;
	struct type_t{
		Bits counter;
		Bit8u value[8];
		Bit8u vlength;
		MpuDataType type;
	} playbuf[8],condbuf;
	struct {
		bool conductor,cond_req,cond_set;
		bool allnotes,realtime,allthru;
		bool playing;
		bool wsd,wsm;
		bool midi_thru;
		bool run_irq,irq_pending;
		Bits data_onoff;
		Bitu command_byte;
		Bit8u tmask,cmask,amask;
		Bit16u midi_mask;
		Bit16u req_mask;
		Bit8u channel;
	} state;
	struct {
		Bit8u timebase,old_timebase;
		Bit8u tempo,old_tempo;
		Bit8u tempo_rel,old_tempo_rel;
		Bit8u tempo_grad;
		Bit8u cth_rate,cth_counter;
		bool clock_to_host,cth_active;
	} clock;
} mpu;


static void QueueByte(Bit8u data) {
	if (mpu.queue_used<MPU_QUEUE) {
		Bitu pos=mpu.queue_used+mpu.queue_pos;
		if (mpu.queue_pos>=MPU_QUEUE) mpu.queue_pos-=MPU_QUEUE;
		if (pos>=MPU_QUEUE) pos-=MPU_QUEUE;
		mpu.queue_used++;
		mpu.queue[pos]=data;
	} else LOG(LOG_MISC,LOG_NORMAL)("MPU401:Data queue full");
}

static void ClrQueue(void) {
	mpu.queue_used=0;
	mpu.queue_pos=0;
}

static Bit8u MPU401_ReadStatus(Bit32u port) {
	Bit8u ret=0x3f;	/* Bits 6 and 7 clear */
	if (!mpu.queue_used) ret|=0x80;
	return ret;
}

static void MPU401_WriteCommand(Bit32u port,Bit8u val) {
	LOG(LOG_MISC,LOG_NORMAL)("MPU-401:Command %x",val);
	if (val && val<=0x2f) {
		switch (val&3) {
			case 1: {MIDI_RawOutByte(0xfc);break;}
			case 2: {MIDI_RawOutByte(0xfa);break;}
			case 3: {MIDI_RawOutByte(0xfb);break;}
		}
		if (val&0x20) LOG(LOG_MISC,LOG_ERROR)("MPU-401:Unhandled Recording Command %x",val);
		switch (val&0xc) {
			case  0x4: /* Stop */
				PIC_RemoveEvents(MPU401_Event);
				mpu.state.playing=false;
				ClrQueue();
				break;
			case 0x8: /* Play */
				mpu.state.playing=true;
				PIC_RemoveEvents(MPU401_Event);
				PIC_AddEvent(MPU401_Event,TIMECONSTANT/(mpu.clock.tempo*mpu.clock.timebase));
				mpu.state.irq_pending=false;
				break;
		}
	}
	else if (val>=0xa0 && val<=0xa7) {/* Request play counter */
		if (mpu.state.cmask&(1<<(val&7))) QueueByte(mpu.playbuf[val&7].counter);
	} 
	else if (val>=0xd0 && val<=0xd7) { /* Request to send data */
		mpu.state.channel=val&7;
		//if (!mpu.playbuf[mpu.state.channel].active)
		mpu.state.wsd=true;
		mpu.state.wsm=false;
	} 
	else
	switch (val) {
		case CMD_REQUEST_TO_SEND_SYSTEM_MSG:
			mpu.state.wsd=false;
			mpu.state.wsm=true;
			break;
		case CMD_CONDUCTOR_ON:
			mpu.state.cond_set=true;
			break;
		case CMD_CONDUCTOR_OFF:
			mpu.state.cond_set=false;
			break;
		case CMD_CLOCK_TO_HOST_OFF:
			mpu.clock.clock_to_host=false;
			break;
		case CMD_CLOCK_TO_HOST_ON:
			mpu.clock.clock_to_host=true;
			break;
		case CMD_REAL_TIME_AFFECTION_OFF:
			break;
		case CMD_REAL_TIME_AFFECTION_ON:
			LOG(LOG_MISC,LOG_ERROR)("MPU401:Unimplemented:Realtime affection:ON");
			break;
		case CMD_MIDI_THRU_OFF:
			mpu.state.midi_thru=false;
			break;
		case CMD_MIDI_THRU_ON:
			mpu.state.midi_thru=true;
			break;
		case CMD_TIMEBASE_48: /* Internal clock resolution per beat */
			mpu.clock.timebase=48;
			break;
		case CMD_TIMEBASE_72:
			mpu.clock.timebase=72;
			break;
		case CMD_TIMEBASE_96:
			mpu.clock.timebase=96;
			break;
		case CMD_TIMEBASE_120:
			mpu.clock.timebase=120;
			break;
		case CMD_TIMEBASE_144:
			mpu.clock.timebase=144;
			break;
		case CMD_TIMEBASE_168:
			mpu.clock.timebase=168;
			break;
		case CMD_TIMEBASE_192:
			mpu.clock.timebase=192;
			break;
		/* Commands with data byte */
		case CMD_MIDI_METRONOME: 
		case CMD_MEASURE_LENGTH:
		case CMD_RELATIVE_TEMPO:
		case CMD_SET_TEMPO:
		case CMD_RELATIVE_TEMPO_GRADUATION:
		case CMD_INTERNAL_CLOCK_LENGTH_TO_HOST:
		case CMD_ACTIVE_TRACK_MASK:
		case CMD_SEND_PLAY_COUNTER_MASK:
		case CMD_MIDI_CHANNEL_MASK_LO:
		case CMD_MIDI_CHANNEL_MASK_HI:
			mpu.state.command_byte=val;
			break;
		/* Commands Returning Data */
		case CMD_REQUEST_VERSION:
			QueueByte(MSG_CMD_ACK);
			QueueByte(MPU_VERSION);
			return;
		case CMD_REQUEST_REVISION:
			QueueByte(MSG_CMD_ACK);
			QueueByte(MPU_REVISION);
			return;
		case CMD_REQUEST_TEMPO:
			QueueByte(MSG_CMD_ACK);
			QueueByte(mpu.clock.tempo);
			return;
		case CMD_REQUEST_AND_CLEAR_REC_COUNTER:
			QueueByte(MSG_CMD_ACK);
			QueueByte(0);
			return;
		case CMD_RESET_RELATIVE_TEMPO:
			mpu.clock.tempo_rel=40;
			break;
		case CMD_CLEAR_PLAY_MAP:
			mpu.state.tmask=0;
		case CMD_CLEAR_PLAY_COUNTERS:
			for (Bitu i=0xb0;i<0xbf;i++) {//All notes off
				MIDI_RawOutByte(i);
				MIDI_RawOutByte(0x7b);
				MIDI_RawOutByte(0);
			}
			for (Bitu i=0;i<8;i++) {
				mpu.playbuf[i].counter=0;
				mpu.playbuf[i].type=UNSET;
			}
			mpu.condbuf.counter=0;
			mpu.condbuf.type=OVERFLOW;
			if (!(mpu.state.conductor=mpu.state.cond_set)) mpu.state.cond_req=0;
			mpu.state.amask=mpu.state.tmask;
			mpu.state.req_mask=0;
			break;
		case CMD_RESET:			/* Reset MPU401 */
			MPU401_Reset();
			if (mpu.intelligent) {
				QueueByte(MSG_CMD_ACK); //additional ACK for interrupt routine
				mpu.state.irq_pending=true;
				PIC_ActivateIRQ(mpu.irq); //UNDOCUMENTED
			}
			break;
		/* Initialization Commands */
		case CMD_UART_MODE:
			mpu.mode=M_UART;
			break;
		case CMD_DISABLE_ALL_NOTES_OFF:
			mpu.state.allnotes=false;
			break;
		case CMD_DISABLE_REAL_TIME_OUT:
			mpu.state.realtime=false;
			break;
		case CMD_DISABLE_ALL_THRU_OFF:
			mpu.state.allthru=false;
			break;
		default:
			LOG(LOG_MISC,LOG_NORMAL)("MPU401:Unhandled command %X",val);
	}
	QueueByte(MSG_CMD_ACK);
}

static Bit8u MPU401_ReadData(Bit32u port) {
	Bit8u ret=MSG_CMD_ACK;
	if (mpu.queue_used) {
		ret=mpu.queue[mpu.queue_pos];
		if (mpu.queue_pos>=MPU_QUEUE) mpu.queue_pos-=MPU_QUEUE;
		mpu.queue_pos++;mpu.queue_used--;
	}
	if (ret>=0xf0 && ret<=0xf7) {
		mpu.state.channel=ret&7;
		mpu.state.data_onoff=0;
		mpu.state.cond_req=false;
		mpu.playbuf[mpu.state.channel].counter=0;
	}
	if (ret==MSG_REQUEST_COMMAND) {
		mpu.state.data_onoff=0;
		mpu.state.cond_req=true;
		mpu.condbuf.counter=0;
	}
	if (ret==MSG_ALL_END || ret==MSG_CLOCK_TO_HOST) {
		mpu.state.data_onoff=-1;
	}
	return ret;
}

static void MPU401_WriteData(Bit32u port,Bit8u val) {
	if (mpu.mode==M_UART) {MIDI_RawOutByte(val);return;}
	switch (mpu.state.command_byte) {
		case 0:
			break;
		case CMD_SET_TEMPO:
			mpu.state.command_byte=0;
			mpu.clock.tempo=val;
			return;
		case CMD_INTERNAL_CLOCK_LENGTH_TO_HOST:
			mpu.state.command_byte=0;
			mpu.clock.cth_rate=val>>2;
			return;
		case CMD_ACTIVE_TRACK_MASK:
			mpu.state.command_byte=0;
			mpu.state.tmask=val;
			return;
		case CMD_SEND_PLAY_COUNTER_MASK:
			mpu.state.command_byte=0;
			mpu.state.cmask=val;
			return;
		case CMD_MIDI_CHANNEL_MASK_LO:
			mpu.state.command_byte=0;
			mpu.state.midi_mask&=0xff00;
			mpu.state.midi_mask|=val;
			return;
		case CMD_MIDI_CHANNEL_MASK_HI:
			mpu.state.command_byte=0;
			mpu.state.midi_mask&=0x00ff;
			mpu.state.midi_mask|=((Bit16u)val)<<8;
			return;
		//case CMD_RELATIVE_TEMPO:
		//case CMD_RELATIVE_TEMPO_GRADUATION:
		//case CMD_MIDI_METRONOME:
		//case CMD_MIDI_MEASURE:
		default:
			mpu.state.command_byte=0;
			return;
	}
	if (mpu.state.wsd) {
		if (val>=0xf0 && !mpu.state.allthru) return;
		MIDI_RawOutByte(val);
		mpu.state.wsd=0;
		return;
	}
	if (mpu.state.wsm) {
		if (val==CMD_EOX) {mpu.state.wsm=0;return;}
		if (val>=0xf0 && !mpu.state.allthru) return;
		MIDI_RawOutByte(val);
		return;
	}
	if (mpu.state.cond_req) { /* Command */
		switch (mpu.state.data_onoff) {
			case 0: /* Timing byte */
				mpu.condbuf.vlength=0;
				if (val<0xf0) mpu.state.data_onoff=1;
				else if (val==0xf8) {
					mpu.state.data_onoff=-1;
					mpu.condbuf.type=OVERFLOW;
				} else return;
				mpu.condbuf.counter=val;
				break;
			case 1:
				mpu.condbuf.type=COMMAND;
				if ((val&0xd0)==0xd0) 
					LOG(LOG_MISC,LOG_ERROR)("'Want to send data' used with conductor");
				mpu.condbuf.value[mpu.condbuf.vlength]=val;
				mpu.condbuf.vlength++;
			default:
				break;
		}
		return;
	}
	Bitu posd;
	switch (mpu.state.data_onoff) { /* Data */
		case   -1:
			return;
		case    0: /* Timing byte */
			mpu.playbuf[mpu.state.channel].vlength=0;
			if (val<0xf0) {mpu.state.data_onoff=1;}
			else if (val==0xf8) {
				mpu.state.data_onoff=-1;
				mpu.playbuf[mpu.state.channel].type=OVERFLOW;
			} else {
				mpu.playbuf[mpu.state.channel].counter=0xf8;
				mpu.state.data_onoff=-1;
				return;
			}
			mpu.playbuf[mpu.state.channel].counter=val;
			break;
		case    1:
			mpu.playbuf[mpu.state.channel].vlength++;
			if ((posd=mpu.playbuf[mpu.state.channel].vlength)>8) return;
			mpu.playbuf[mpu.state.channel].value[posd-1]=val;
			if (posd==1) {
				switch (val&0xf0) {
					case 0xf0:
						mpu.playbuf[mpu.state.channel].type=(val>0xf7 ? MARK : MIDI_SYS);
						break;
					case 0xe0: case 0xd0: case 0xc0: case 0xb0: case 0xa0: case 0x90: case 0x80:
						mpu.playbuf[mpu.state.channel].type=MIDI_NORM;
						break;
					default:
						mpu.playbuf[mpu.state.channel].type=MIDI_DATA;
				}
			}
	}
}

static void MPU401_IntelligentOut(Bit8u chan) {
	Bitu val;
	switch (mpu.playbuf[chan].type) {
		case UNSET:
		case OVERFLOW:
			break;
		case MARK:
			val=mpu.playbuf[chan].value[0];
			if (val==0xfc) {MIDI_RawOutByte(val);mpu.state.amask&=~(1<<chan);}
			break;
		case MIDI_SYS:
		case MIDI_NORM://TODO apply channel mask and other filtering
		case MIDI_DATA:
			for (int i=0;i<mpu.playbuf[chan].vlength;i++)
				MIDI_RawOutByte(mpu.playbuf[chan].value[i]);
			break;
	}
}

static void MPU401_Event(Bitu val) {
	if (mpu.mode==M_UART) return;
	for (Bitu i=0;i<8;i++) {/* Decrease play counters */
		if (mpu.state.amask&(1<<(i))) {
			if ((mpu.playbuf[i].counter--)<0) {
				MPU401_IntelligentOut(i);
				if (!(mpu.state.amask&(1<<(i)))) {
					if (mpu.state.amask==0) mpu.state.req_mask|=(1<<12);
					continue;
				} 
				mpu.playbuf[i].vlength=0;
				mpu.playbuf[i].type=UNSET;
				mpu.playbuf[i].counter=MSG_TIMING_OVERFLOW;
				mpu.state.req_mask|=(1<<i);
			}
		}
	}		
	if (mpu.state.conductor) {
		if (mpu.condbuf.counter--<0) {
			if (mpu.condbuf.type!=OVERFLOW) {
				MPU401_WriteCommand(0x331,mpu.condbuf.value[0]);
				if (mpu.state.command_byte) MPU401_WriteData(0x330,mpu.condbuf.value[1]);
			}
			mpu.condbuf.vlength=0;
			mpu.condbuf.type=OVERFLOW;
			mpu.condbuf.counter=MSG_TIMING_OVERFLOW;
			mpu.state.req_mask|=(1<<9);
		}
	}
	if (mpu.clock.clock_to_host) {
		mpu.clock.cth_counter++;
		if (mpu.clock.cth_counter >= mpu.clock.cth_rate) {
			mpu.clock.cth_counter=0;
			mpu.state.req_mask|=(1<<13);
		}
	}
	if (!mpu.state.irq_pending && mpu.state.req_mask) MPU401_EOIHandler();

	PIC_RemoveEvents(MPU401_Event);
	Bitu new_time;
	if ((new_time=mpu.clock.tempo*mpu.clock.timebase)==0) return;
	PIC_AddEvent(MPU401_Event,TIMECONSTANT/new_time);
}


static void MPU401_EOIHandler(void)
{
	mpu.state.irq_pending=false;
	if (!mpu.state.req_mask) return;
	ClrQueue();
	Bitu i=0;
	do {
		if (mpu.state.req_mask&(1<<i)) {
			QueueByte(0xf0+i);
			mpu.state.req_mask&=~(1<<i);
			break;
		}
	} while ((i++)<16);
	#if 1
		CPU_CycleLeft+=CPU_Cycles;
		CPU_Cycles=0;
	#endif
	PIC_ActivateIRQ(mpu.irq);
	mpu.state.irq_pending=true;
}

static Bitu INT71_Handler() {
	CALLBACK_RunRealInt(0xa);
	IO_Write(0xa0,0x61);
	MPU401_EOIHandler();
return CBRET_NONE;
}

static void MPU401_Reset(void) {
	PIC_DeActivateIRQ(mpu.irq);
	mpu.mode=(mpu.intelligent ? M_INTELLIGENT : M_UART);
	mpu.state.wsd=false;
	mpu.state.wsm=false;
	mpu.state.conductor=false;
	mpu.state.cond_req=false;
	mpu.state.cond_set=false;
	mpu.state.allnotes=true;
	mpu.state.allthru=true;
	mpu.state.realtime=true;
	mpu.state.playing=false;
	mpu.state.run_irq=false;
	mpu.state.irq_pending=false;
	mpu.state.cmask=0xff;
	mpu.state.amask=mpu.state.tmask=0;
	mpu.state.midi_mask=0xffff;
	mpu.state.data_onoff=0;
	mpu.state.command_byte=0;
	mpu.clock.tempo=mpu.clock.old_tempo=160;
	mpu.clock.timebase=mpu.clock.old_timebase=120;
	mpu.clock.tempo_rel=mpu.clock.old_tempo_rel=40;
	mpu.clock.tempo_grad=0;
	mpu.clock.clock_to_host=false;
	mpu.clock.cth_rate=60;
	mpu.clock.cth_counter=0;
	ClrQueue();
	mpu.state.req_mask=0;
	mpu.condbuf.counter=0;
	mpu.condbuf.type=OVERFLOW;
	for (Bitu i=0;i<8;i++) {mpu.playbuf[i].type=UNSET;mpu.playbuf[i].counter=0;}
}

void MPU401_Init(Section* sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	if(!section->Get_bool("mpu401")) return;

	if (!MIDI_Available()) return;

	IO_RegisterWriteHandler(0x330,&MPU401_WriteData,"MPU401");
	IO_RegisterWriteHandler(0x331,&MPU401_WriteCommand,"MPU401");
	IO_RegisterReadHandler(0x330,&MPU401_ReadData,"MPU401");
	IO_RegisterReadHandler(0x331,&MPU401_ReadStatus,"MPU401");

	mpu.queue_used=0;
	mpu.queue_pos=0;
	mpu.mode=M_UART;

	if (!(mpu.intelligent=section->Get_bool("intelligent"))) return;
	mpu.irq=2;
	PIC_RegisterIRQ(mpu.irq,0,"MPU401");
	call_irq9=CALLBACK_Allocate(); //allocate handler for irq 9
	CALLBACK_Setup(call_irq9,&INT71_Handler,CB_IRET);
	RealSetVec(0x71,CALLBACK_RealPointer(call_irq9));
	MPU401_Reset();
}
