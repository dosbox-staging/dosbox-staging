#ifndef __FMOPL_H_
#define __FMOPL_H_

/* --- select emulation chips --- */
#define BUILD_YM3812 (HAS_YM3812)
#define BUILD_YM3526 (HAS_YM3526)
#define BUILD_Y8950  (HAS_Y8950)

/* --- system optimize --- */
/* select bit size of output : 8 or 16 */
#define OPL_SAMPLE_BITS 16

/* compiler dependence */
#ifndef OSD_CPU_H
#define OSD_CPU_H
typedef unsigned char	UINT8;   /* unsigned  8bit */
typedef unsigned short	UINT16;  /* unsigned 16bit */
typedef unsigned int	UINT32;  /* unsigned 32bit */
typedef signed char		INT8;    /* signed  8bit   */
typedef signed short	INT16;   /* signed 16bit   */
typedef signed int		INT32;   /* signed 32bit   */
#endif

#if (OPL_SAMPLE_BITS==16)
typedef INT16 OPLSAMPLE;
#endif
#if (OPL_SAMPLE_BITS==8)
typedef unsigned char  OPLSAMPLE;
#endif


#if BUILD_Y8950
#include "ymdeltat.h"
#endif

typedef void (*OPL_TIMERHANDLER)(int channel,double interval_Sec);
typedef void (*OPL_IRQHANDLER)(int param,int irq);
typedef void (*OPL_UPDATEHANDLER)(int param,int min_interval_us);
typedef void (*OPL_PORTHANDLER_W)(int param,unsigned char data);
typedef unsigned char (*OPL_PORTHANDLER_R)(int param);

/* !!!!! here is private section , do not access there member direct !!!!! */

#define OPL_TYPE_WAVESEL   0x01  /* waveform select		*/
#define OPL_TYPE_ADPCM     0x02  /* DELTA-T ADPCM unit	*/
#define OPL_TYPE_KEYBOARD  0x04  /* keyboard interface	*/
#define OPL_TYPE_IO        0x08  /* I/O port			*/

/* Saving is necessary for member of the 'R' mark for suspend/resume */
/* ---------- OPL slot  ---------- */
typedef struct fm_opl_slot {
	const UINT32 *AR;	/* attack rate tab :&eg_table[AR<<2]*/
	const UINT32 *DR;	/* decay rate tab  :&eg_table[DR<<2]*/
	const UINT32 *RR;	/* release rate tab:&eg_table[RR<<2]*/
	UINT8	KSR;		/* key scale rate					*/
	UINT8	ARval;		/* current AR						*/
	UINT8	ksl;		/* keyscale level					*/
	UINT8	ksr;		/* key scale rate  :kcode>>KSR		*/
	UINT8	mul;		/* multiple        :ML_TABLE[ML]	*/

	/* Phase Generator */
	UINT32	Cnt;		/* frequency count					*/
	UINT32	Incr;		/* frequency step					*/

	/* Envelope Generator */
	UINT8	eg_type;	/* percussive/non-percussive mode	*/
	UINT8	state;		/* phase type						*/
	UINT32	TL;			/* total level     :TL << 3			*/
	INT32	TLL;		/* adjusted now TL					*/
	INT32	volume;		/* envelope counter					*/
	UINT32	sl;			/* sustain level   :SL_TABLE[SL]	*/
	UINT32	delta_ar;	/* envelope step for Attack			*/
	UINT32	delta_dr;	/* envelope step for Decay			*/
	UINT32	delta_rr;	/* envelope step for Release		*/

	UINT32	key;		/* 0 = KEY OFF, >0 = KEY ON			*/

	/* LFO */
	UINT32	AMmask;		/* LFO Amplitude Modulation enable mask */
	UINT8	vib;		/* LFO Phase Modulation enable flag (active high)*/

	/* waveform select */
	unsigned int *wavetable;
}OPL_SLOT;

/* ---------- OPL one of channel  ---------- */
typedef struct fm_opl_channel {
	OPL_SLOT SLOT[2];
	UINT8   FB;			/* feedback shift value				*/
	INT32   *connect1;	/* slot1 output pointer				*/
	INT32   op1_out[2];	/* slot1 output for feedback		*/

	/* phase generator state */
	UINT32  block_fnum;	/* block+fnum						*/
	UINT32  fc;			/* Freq. Increment base				*/
	UINT32  ksl_base;	/* KeyScaleLevel Base step			*/
	UINT8   kcode;		/* key code (for key scaling)		*/

	UINT8   CON;		/* connection (algorithm) type		*/
} OPL_CH;

/* OPL state */
typedef struct fm_opl_f {
	/* FM channel slots */
	OPL_CH P_CH[9];		/* OPL/OPL2 chips have 9 channels	*/

	UINT8 rhythm;		/* Rhythm mode						*/

	UINT32 eg_tab[16+64+16];	/* EG rate table: 16 (dummy) + 64 rates + 16 RKS */
	UINT32 fn_tab[1024];	/* fnumber -> increment counter */

	/* LFO */
	UINT8  lfo_am_depth;
	UINT8  lfo_pm_depth_range;
	UINT32 lfo_am_cnt;
	UINT32 lfo_am_inc;
	UINT32 lfo_pm_cnt;
	UINT32 lfo_pm_inc;

	UINT32	noise_rng;	/* 23 bit noise shift register		*/
	UINT32	noise_p;	/* current noise 'phase'			*/
	UINT32	noise_f;	/* current noise period				*/

	UINT8 wavesel;		/* waveform select enable flag		*/

	int T[2];			/* timer counters					*/
	UINT8 st[2];		/* timer enable						*/

#if BUILD_Y8950
	/* Delta-T ADPCM unit (Y8950) */

	YM_DELTAT *deltat;

	/* Keyboard / I/O interface unit*/
	UINT8 portDirection;
	UINT8 portLatch;
	OPL_PORTHANDLER_R porthandler_r;
	OPL_PORTHANDLER_W porthandler_w;
	int port_param;
	OPL_PORTHANDLER_R keyboardhandler_r;
	OPL_PORTHANDLER_W keyboardhandler_w;
	int keyboard_param;
#endif

	/* external event callback handlers */
	OPL_TIMERHANDLER  TimerHandler;		/* TIMER handler	*/
	int TimerParam;						/* TIMER parameter	*/
	OPL_IRQHANDLER    IRQHandler;		/* IRQ handler		*/
	int IRQParam;						/* IRQ parameter	*/
	OPL_UPDATEHANDLER UpdateHandler;	/* stream update handler   */
	int UpdateParam;					/* stream update parameter */

	UINT8 type;			/* chip type						*/
	UINT8 address;		/* address register					*/
	UINT8 status;		/* status flag						*/
	UINT8 statusmask;	/* status mask						*/
	UINT8 mode;			/* Reg.08 : CSM,notesel,etc.		*/

	int clock;			/* master clock  (Hz)				*/
	int rate;			/* sampling rate (Hz)				*/
	double freqbase;	/* frequency base					*/
	double TimerBase;	/* Timer base time (==sampling time)*/
} FM_OPL;


/* ---------- Generic interface section ---------- */
#define OPL_TYPE_YM3526 (0)
#define OPL_TYPE_YM3812 (OPL_TYPE_WAVESEL)
#define OPL_TYPE_Y8950  (OPL_TYPE_ADPCM|OPL_TYPE_KEYBOARD|OPL_TYPE_IO)

FM_OPL *OPLCreate(int type, int clock, int rate);
void OPLDestroy(FM_OPL *OPL);
void OPLSetTimerHandler(FM_OPL *OPL,OPL_TIMERHANDLER TimerHandler,int channelOffset);
void OPLSetIRQHandler(FM_OPL *OPL,OPL_IRQHANDLER IRQHandler,int param);
void OPLSetUpdateHandler(FM_OPL *OPL,OPL_UPDATEHANDLER UpdateHandler,int param);
/* Y8950 port handlers */
void OPLSetPortHandler(FM_OPL *OPL,OPL_PORTHANDLER_W PortHandler_w,OPL_PORTHANDLER_R PortHandler_r,int param);
void OPLSetKeyboardHandler(FM_OPL *OPL,OPL_PORTHANDLER_W KeyboardHandler_w,OPL_PORTHANDLER_R KeyboardHandler_r,int param);

void OPLResetChip(FM_OPL *OPL);
int OPLWrite(FM_OPL *OPL,int a,int v);
unsigned char OPLRead(FM_OPL *OPL,int a);
int OPLTimerOver(FM_OPL *OPL,int c);


/* YM3626/YM3812 local section */
void YM3812UpdateOne(FM_OPL *OPL, INT16 *buffer, int length);

void Y8950UpdateOne(FM_OPL *OPL, INT16 *buffer, int length);

#endif
