#include <string.h>
#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "dma.h"
#include "pic.h"
#include "hardware.h"
#include "setup.h"
#include "programs.h"

void MIDI_RawOutByte(Bit8u data);
bool MIDI_Available(void);

#define MPU_QUEUE 32
enum MpuMode { M_UART,M_INTELLIGENT } ;

#define CMD_RET_ACK 0xfe

static struct {
	MpuMode mode;
	Bit8u queue[MPU_QUEUE];
	Bitu queue_pos,queue_used;
	
} mpu;

static void QueueByte(Bit8u data) {
	if (mpu.queue_used<MPU_QUEUE) {
		Bitu pos=mpu.queue_used+mpu.queue_pos;
		if (pos>=MPU_QUEUE) pos-=MPU_QUEUE;
		mpu.queue_used++;
		mpu.queue[pos]=data;
	} else LOG(LOG_MISC,"MPU401:Data queue full");
}

static void ClrQueue(void) {
	mpu.queue_used=0;
	mpu.queue_pos=0;
}

static void MPU401_WriteCommand(Bit32u port,Bit8u val) {
	switch (val) {
	case 0x3f:		/* Switch to UART Mode */
		mpu.mode=M_UART;
		QueueByte(CMD_RET_ACK);
		break;
	case 0xff:		/* Reset Commmand */
		ClrQueue();
		QueueByte(CMD_RET_ACK);
		break;
	default:
		LOG(LOG_MISC,"MPU401:Unhandled command %X",val);
		break;
	}

}

static Bit8u MPU401_ReadStatus(Bit32u port) {

	Bit8u ret=0x3f;	/* Bith 6 and 7 clear */
	if (!mpu.queue_used) ret|=0x80;
	return ret;
}


static void MPU401_WriteData(Bit32u port,Bit8u val) {
	MIDI_RawOutByte(val);
}

static Bit8u MPU401_ReadData(Bit32u port) {
	Bit8u ret=CMD_RET_ACK;
	if (mpu.queue_used) {
		ret=mpu.queue[mpu.queue_pos];
		mpu.queue_pos++;
		if (mpu.queue_pos>=MPU_QUEUE) mpu.queue_pos-=MPU_QUEUE;
		mpu.queue_used--;
	}
	return ret;
}


void MPU401_Init(Section* sec) {
	if (!MIDI_Available()) return;

	IO_RegisterWriteHandler(0x330,&MPU401_WriteData,"MPU401");
	IO_RegisterWriteHandler(0x331,&MPU401_WriteCommand,"MPU401");
	IO_RegisterReadHandler(0x330,&MPU401_ReadData,"MPU401");
	IO_RegisterReadHandler(0x331,&MPU401_ReadStatus,"MPU401");

	mpu.queue_used=0;
	mpu.queue_pos=0;
}


