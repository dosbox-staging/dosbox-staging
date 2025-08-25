// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CALLBACK_H
#define DOSBOX_CALLBACK_H

#include "config/setup.h"
#include "hardware/memory.h"

typedef Bitu (*Callback_Handler)();
extern Callback_Handler Callback_Handlers[];

enum {
	CB_RETN,
	CB_RETF,
	CB_RETF8,
	CB_RETF_STI,
	CB_RETF_CLI,
	CB_IRET,
	CB_IRETD,
	CB_IRET_STI,
	CB_IRET_EOI_PIC1,
	CB_IRQ0,
	CB_IRQ1,
	CB_IRQ9,
	CB_IRQ12,
	CB_IRQ12_RET,
	CB_IRQ6_PCJR,
	CB_MOUSE,
	CB_INT29,
	CB_INT16,
	CB_HOOKABLE,
	CB_TDE_IRET,
	CB_IPXESR,
	CB_IPXESR_RET,
	CB_INT21,
	CB_INT13,
	CB_VESA_WAIT,
	CB_VESA_PM
};

using callback_number_t = uint8_t;
#define CB_MAX		128
#define CB_SIZE		32
#define CB_SEG		0xF000
#define CB_SOFFSET	0x1000

enum {
	CBRET_NONE=0,CBRET_STOP=1
};

static inline RealPt CALLBACK_RealPointer(callback_number_t cb_number)
{
	const auto offset = CB_SOFFSET + cb_number * CB_SIZE;
	return RealMake(CB_SEG, static_cast<uint16_t>(offset));
}
static inline PhysPt CALLBACK_PhysPointer(callback_number_t cb_number)
{
	const auto offset = CB_SOFFSET + cb_number * CB_SIZE;
	return PhysicalMake(CB_SEG, static_cast<uint16_t>(offset));
}

static inline PhysPt CALLBACK_GetBase() {
	return (CB_SEG << 4) + CB_SOFFSET;
}

void CALLBACK_Init(Section* sec);

callback_number_t CALLBACK_Allocate();
void CALLBACK_DeAllocate(const callback_number_t in);

void CALLBACK_Idle();

void CALLBACK_RunRealInt(uint8_t intnum);
void CALLBACK_RunRealFar(uint16_t seg,uint16_t off);

bool CALLBACK_Setup(callback_number_t cb_number, Callback_Handler handler, Bitu type, const char* descr);
callback_number_t CALLBACK_Setup(callback_number_t cb_number, Callback_Handler handler, Bitu type, PhysPt addr,
                                 const char* descr);

const char* CALLBACK_GetDescription(callback_number_t cb_number);

void CALLBACK_SCF(bool val);
void CALLBACK_SZF(bool val);
void CALLBACK_SIF(bool val);

extern callback_number_t call_priv_io;

class CALLBACK_HandlerObject {
private:
	bool installed = false;
	enum { NONE, SETUP, SETUPAT } m_type = NONE;
	struct {
		RealPt old_vector = 0;
		bool installed = false;
		uint8_t interrupt = 0;
	} vectorhandler = {};

	callback_number_t m_cb_number = 0;

public:
	CALLBACK_HandlerObject() : installed(false), m_type(NONE), vectorhandler{0, 0, false}, m_cb_number(0) {}

	virtual ~CALLBACK_HandlerObject();

	//Install and allocate a callback.
	void Install(Callback_Handler handler,Bitu type,const char* description);
	void Install(Callback_Handler handler,Bitu type,PhysPt addr,const char* description);

	void Uninstall();

	//Only allocate a callback number
	void Allocate(Callback_Handler handler,const char* description=nullptr);
	uint16_t Get_callback() const {
		return m_cb_number;
	}
	RealPt Get_RealPointer() const {
		return CALLBACK_RealPointer(m_cb_number);
	}
	void Set_RealVec(uint8_t vec);
};

#endif // DOSBOX_CALLBACK_H
