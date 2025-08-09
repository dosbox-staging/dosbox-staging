// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cpu/mmx.h"

extern uint32_t* lookupRMEAregd[256];

#define LoadMb(off)      mem_readb_inline(off)
#define LoadMw(off)      mem_readw_inline(off)
#define LoadMd(off)      mem_readd_inline(off)
#define LoadMq(off)      mem_readq_inline(off)
#define SaveMb(off, val) mem_writeb_inline(off, val)
#define SaveMw(off, val) mem_writew_inline(off, val)
#define SaveMd(off, val) mem_writed_inline(off, val)
#define SaveMq(off, val) mem_writeq_inline(off, val)

static void mmx_movd_pqed(const Bitu rm, const PhysPt eaa = 0)
{
	auto rmrq = lookupRMregMM[rm];
	if (rm >= 0xc0) {
		auto eard   = lookupRMEAregd[rm];
		rmrq->ud.d0 = *(uint32_t*)eard;
		rmrq->ud.d1 = 0;
	} else {
		rmrq->ud.d0 = LoadMd(eaa);
		rmrq->ud.d1 = 0;
	}
}

// CASE_0F_MMX(0x6e) // MOVD Pq,Ed
static void dyn_mmx_movd_pqed()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_movd_pqed, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_movd_pqed, decode.modrm.val);
	}
}

static void mmx_movd_edpq(const Bitu rm, const PhysPt eaa = 0)
{
	auto rmrq = lookupRMregMM[rm];
	if (rm >= 0xc0) {
		auto eard        = lookupRMEAregd[rm];
		*(uint32_t*)eard = rmrq->ud.d0;
	} else {
		SaveMd(eaa, rmrq->ud.d0);
	}
}

// CASE_0F_MMX(0x7e) // MOVD Ed,Pq
static void dyn_mmx_movd_edpq()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_movd_edpq, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_movd_edpq, decode.modrm.val);
	}
}

static void mmx_movq_pqqq(const Bitu rm, const PhysPt eaa = 0)
{
	auto dest = lookupRMregMM[rm];
	if (rm >= 0xc0) {
		auto src = reg_mmx[rm & 7];
		dest->q  = src->q;
	} else {
		dest->q = LoadMq(eaa);
	}
}

// CASE_0F_MMX(0x6f) // MOVQ Pq,Qq
static void dyn_mmx_movq_pqqq()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_movq_pqqq, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_movq_pqqq, decode.modrm.val);
	}
}

static void mmx_movq_qqpq(const Bitu rm, const PhysPt eaa = 0)
{
	auto dest = lookupRMregMM[rm];
	if (rm >= 0xc0) {
		auto src = reg_mmx[rm & 7];
		src->q   = dest->q;
	} else {
		SaveMq(eaa, dest->q);
	}
}

// CASE_0F_MMX(0x7f) // MOVQ Qq,Pq
static void dyn_mmx_movq_qqpq()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_movq_qqpq, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_movq_qqpq, decode.modrm.val);
	}
}

static void mmx_paddb(const Bitu rm, const PhysPt eaa)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_paddb(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xFC) // PADDB Pq,Qq
static void dyn_mmx_paddb()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_paddb, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_paddb, decode.modrm.val);
	}
}

static void mmx_paddw(const Bitu rm, const PhysPt eaa)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_paddw(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xFD) // PADDW Pq,Qq
static void dyn_mmx_paddw()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_paddw, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_paddw, decode.modrm.val);
	}
}

static void mmx_paddd(const Bitu rm, const PhysPt eaa)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_paddd(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xFE) // PADDD Pq,Qq
static void dyn_mmx_paddd()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_paddd, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_paddd, decode.modrm.val);
	}
}

static void mmx_paddsb(const Bitu rm, const PhysPt eaa)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_paddsb(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xEC) // PADDSB Pq,Qq
static void dyn_mmx_paddsb()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_paddsb, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_paddsb, decode.modrm.val);
	}
}

static void mmx_paddsw(const Bitu rm, const PhysPt eaa)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_paddsw(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xED) // PADDSW Pq,Qq
static void dyn_mmx_paddsw()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_paddsw, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_paddsw, decode.modrm.val);
	}
}

static void mmx_paddusb(const Bitu rm, const PhysPt eaa)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_paddusb(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xDC) // PADDUSB Pq,Qq
static void dyn_mmx_paddusb()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_paddusb, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_paddusb, decode.modrm.val);
	}
}

static void mmx_paddusw(const Bitu rm, const PhysPt eaa)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_paddusw(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xDD) // PADDUSW Pq,Qq
static void dyn_mmx_paddusw()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_paddusw, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_paddusw, decode.modrm.val);
	}
}

static void mmx_psubb(const Bitu rm, const PhysPt eaa)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_psubb(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xF8) // PSUBB Pq,Qq
static void dyn_mmx_psubb()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_psubb, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_psubb, decode.modrm.val);
	}
}

static void mmx_psubw(const Bitu rm, const PhysPt eaa)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_psubw(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xF9) // PSUBW Pq,Qq
static void dyn_mmx_psubw()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_psubw, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_psubw, decode.modrm.val);
	}
}

static void mmx_psubsb(const Bitu rm, const PhysPt eaa)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_psubsb(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xE8) // PSUBSB Pq,Qq
static void dyn_mmx_psubsb()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_psubsb, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_psubsb, decode.modrm.val);
	}
}

static void mmx_psubsw(const Bitu rm, const PhysPt eaa)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_psubsw(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xE9) // PSUBSW Pq,Qq
static void dyn_mmx_psubsw()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_psubsw, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_psubsw, decode.modrm.val);
	}
}

static void mmx_psubusb(const Bitu rm, const PhysPt eaa)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_psubusb(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xD8) // PSUBUSB Pq,Qq
static void dyn_mmx_psubusb()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_psubusb, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_psubusb, decode.modrm.val);
	}
}

static void mmx_psubusw(const Bitu rm, const PhysPt eaa)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_psubusw(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xD9) // PSUBUSW Pq,Qq
static void dyn_mmx_psubusw()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_psubusw, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_psubusw, decode.modrm.val);
	}
}

static void mmx_psubd(const Bitu rm, const PhysPt eaa)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_psubd(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xFA) // PSUBD Pq,Qq
static void dyn_mmx_psubd()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_psubd, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_psubd, decode.modrm.val);
	}
}

static void mmx_pmaddwd(const Bitu rm, const PhysPt eaa)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_pmaddwd(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xF5) // PMADDWD Pq,Qq
static void dyn_mmx_pmaddwd()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_pmaddwd, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_pmaddwd, decode.modrm.val);
	}
}

static void mmx_pmulhw(const Bitu rm, const PhysPt eaa)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_pmulhw(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xE5) // PMULHW Pq,Qq
static void dyn_mmx_pmulhw()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_pmulhw, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_pmulhw, decode.modrm.val);
	}
}

static void mmx_pmullw(const Bitu rm, const PhysPt eaa)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_pmullw(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xD5) // PMULLW Pq,Qq
static void dyn_mmx_pmullw()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_pmullw, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_pmullw, decode.modrm.val);
	}
}

static void mmx_packuswb(const Bitu rm, const PhysPt eaa = 0)
{
	MMX_reg* dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}
	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_packuswb(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0x67) // PACKUSWB Pq,Qq
static void dyn_mmx_packuswb()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_packuswb, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_packuswb, decode.modrm.val);
	}
}

static void mmx_psllw_psrlw_psraw(const Bitu rm, const Bitu shift)
{
	uint8_t op    = (rm >> 3) & 7;
	MMX_reg* dest = reg_mmx[rm & 7];
	switch (op) {
	case 0x06: // PSLLW
	{
		const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
		const auto res = simde_m_psllwi(dest_m, shift);
		dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	} break;
	case 0x02: // PSRLW
	{
		const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
		const auto res = simde_m_psrlwi(dest_m, shift);
		dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	} break;
	case 0x04: // PSRAW
	{
		const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
		const auto res = simde_m_psrawi(dest_m, shift);
		dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	} break;
	}
}

// CASE_0F_MMX(0x71) // PSLLW/PSRLW/PSRAW Pq,Ib
static void dyn_mmx_psllw_psrlw_psraw()
{
	dyn_get_modrm();
	const auto shift = decode_fetchb();

	gen_call_function_II((void*)mmx_psllw_psrlw_psraw, decode.modrm.val, shift);
}

static void mmx_pslld_psrld_psrad(const Bitu rm, const Bitu shift)
{
	uint8_t op    = (rm >> 3) & 7;
	MMX_reg* dest = reg_mmx[rm & 7];
	switch (op) {
	case 0x06: // PSLLD
	{
		const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
		const auto res = simde_m_pslldi(dest_m, shift);
		dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	} break;
	case 0x02: // PSRLD
	{
		const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
		const auto res = simde_m_psrldi(dest_m, shift);
		dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	} break;
	case 0x04: // PSRAD
	{
		const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
		const auto res = simde_m_psradi(dest_m, shift);
		dest->q        = static_cast<uint64_t>(simde_m_to_int64(res));
	} break;
	}
}

// CASE_0F_MMX(0x72) // PSLLD/PSRLD/PSRAD Pq,Ib
static void dyn_mmx_pslld_psrld_psrad()
{
	dyn_get_modrm();
	const auto shift = decode_fetchb();

	gen_call_function_II((void*)mmx_pslld_psrld_psrad, decode.modrm.val, shift);
}

static void mmx_pslld(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_pslld(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xf2) // PSLLD Pq,Qq
static void dyn_mmx_pslld()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_pslld, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_pslld, decode.modrm.val);
	}
}

static void mmx_psllq(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_psllq(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xf3) // PSLLQ Pq,Qq
static void dyn_mmx_psllq()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_psllq, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_psllq, decode.modrm.val);
	}
}

static void mmx_psrld(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_psrld(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xd2) // PSRLD Pq,Qq
static void dyn_mmx_psrld()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_psrld, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_psrld, decode.modrm.val);
	}
}

static void mmx_pcmpeqb(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_pcmpeqb(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0x74) // PCMPEQB Pq,Qq
static void dyn_mmx_pcmpeqb()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_pcmpeqb, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_pcmpeqb, decode.modrm.val);
	}
}

static void mmx_pcmpeqw(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_pcmpeqw(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0x75) // PCMPEQW Pq,Qq
static void dyn_mmx_pcmpeqw()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_pcmpeqw, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_pcmpeqw, decode.modrm.val);
	}
}

static void mmx_pcmpeqd(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_pcmpeqd(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0x76) // PCMPEQD Pq,Qq
static void dyn_mmx_pcmpeqd()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_pcmpeqd, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_pcmpeqd, decode.modrm.val);
	}
}

static void mmx_pcmpgtb(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_pcmpgtb(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0x64) // PCMPGTB Pq,Qq
static void dyn_mmx_pcmpgtb()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_pcmpgtb, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_pcmpgtb, decode.modrm.val);
	}
}

static void mmx_pcmpgtw(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_pcmpgtw(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0x65) // PCMPGTW Pq,Qq
static void dyn_mmx_pcmpgtw()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_pcmpgtw, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_pcmpgtw, decode.modrm.val);
	}
}

static void mmx_pcmpgtd(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_pcmpgtd(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0x66) // PCMPGTD Pq,Qq
static void dyn_mmx_pcmpgtd()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_pcmpgtd, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_pcmpgtd, decode.modrm.val);
	}
}

static void mmx_packsswb(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_packsswb(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0x63) // PACKSSWB Pq,Qq
static void dyn_mmx_packsswb()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_packsswb, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_packsswb, decode.modrm.val);
	}
}

static void mmx_packssdw(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_packssdw(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0x6B) // PACKSSDW Pq,Qq
static void dyn_mmx_packssdw()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_packssdw, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_packssdw, decode.modrm.val);
	}
}

static void mmx_punpckhbw(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_punpckhbw(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0x68) // PUNPCKHBW Pq,Qq
static void dyn_mmx_punpckhbw()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_punpckhbw, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_punpckhbw, decode.modrm.val);
	}
}

static void mmx_punpcklbw(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_punpcklbw(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0x60) // PUNPCKLBW Pq,Qq
static void dyn_mmx_punpcklbw()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_punpcklbw, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_punpcklbw, decode.modrm.val);
	}
}

static void mmx_punpckhwd(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_punpckhwd(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0x69) // PUNPCKHWD Pq,Qq
static void dyn_mmx_punpckhwd()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_punpckhwd, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_punpckhwd, decode.modrm.val);
	}
}

static void mmx_punpcklwd(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_punpcklwd(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0x61) // PUNPCKLWD Pq,Qq
static void dyn_mmx_punpcklwd()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_punpcklwd, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_punpcklwd, decode.modrm.val);
	}
}

static void mmx_punpckldq(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_punpckldq(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0x62) // PUNPCKLDQ Pq,Qq
static void dyn_mmx_punpckldq()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_punpckldq, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_punpckldq, decode.modrm.val);
	}
}

static void mmx_punpckhdq(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_punpckhdq(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0x6A) // PUNPCKHDQ Pq,Qq
static void dyn_mmx_punpckhdq()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_punpckhdq, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_punpckhdq, decode.modrm.val);
	}
}

static void mmx_psllw(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_psllw(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xf1) // PSLLW Pq,Qq
static void dyn_mmx_psllw()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_psllw, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_psllw, decode.modrm.val);
	}
}

static void mmx_psrlw(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_psrlw(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xd1) // PSRLW Pq,Qq
static void dyn_mmx_psrlw()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_psrlw, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_psrlw, decode.modrm.val);
	}
}

static void mmx_psrlq(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_psrlq(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xd3) // PSRLQ Pq,Qq
static void dyn_mmx_psrlq()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_psrlq, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_psrlq, decode.modrm.val);
	}
}

static void mmx_psllq_psrlq(const Bitu rm, const Bitu shift)
{
	auto dest = reg_mmx[rm & 7];
	if (shift > 63) {
		dest->q = 0;
	} else {
		uint8_t op = rm & 0x20;
		if (op) {
			dest->q <<= shift;
		} else {
			dest->q >>= shift;
		}
	}
}

// CASE_0F_MMX(0x73) // PSLLQ/PSRLQ Pq,Ib
static void dyn_mmx_psllq_psrlq()
{
	dyn_get_modrm();
	const uint8_t shift = decode_fetchb();

	gen_call_function_II((void*)mmx_psllq_psrlq, decode.modrm.val, shift);
}

static void mmx_psraw(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_psraw(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xe1) // PSRAW Pq,Qq
static void dyn_mmx_psraw()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_psraw, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_psraw, decode.modrm.val);
	}
}

static void mmx_psrad(const Bitu rm, const PhysPt eaa)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_psrad(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xe2) // PSRAD Pq,Qq
static void dyn_mmx_psrad()
{
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_psrad, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_psrad, decode.modrm.val);
	}
}

static void mmx_por(const Bitu rm, const PhysPt eaa = 0)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_por(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xeb) // POR Pq,Qq
static void dyn_mmx_por()
{
	dyn_get_modrm();
	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_por, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_por, decode.modrm.val);
	}
}

static void mmx_pxor(const Bitu rm, const PhysPt eaa = 0)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_pxor(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xef) // PXOR Pq,Qq
static void dyn_mmx_pxor()
{
	dyn_get_modrm();
	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_pxor, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_pxor, decode.modrm.val);
	}
}

static void mmx_pand(const Bitu rm, const PhysPt eaa = 0)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_pand(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xdb) // PAND Pq,Qq
static void dyn_mmx_pand()
{
	dyn_get_modrm();
	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_pand, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_pand, decode.modrm.val);
	}
}

static void mmx_pandn(const Bitu rm, const PhysPt eaa = 0)
{
	auto dest = lookupRMregMM[rm];
	MMX_reg src;
	if (rm >= 0xc0) {
		src.q = reg_mmx[rm & 7]->q;
	} else {
		src.q = LoadMq(eaa);
	}

	const auto src_m  = simde_m_from_int64(static_cast<int64_t>(src.q));
	const auto dest_m = simde_m_from_int64(static_cast<int64_t>(dest->q));
	const auto res    = simde_m_pandn(dest_m, src_m);
	dest->q           = static_cast<uint64_t>(simde_m_to_int64(res));
}

// CASE_0F_MMX(0xdf) // PANDN Pq,Qq
static void dyn_mmx_pandn()
{
	dyn_get_modrm();
	if (decode.modrm.mod < 3) {
		dyn_fill_ea(FC_ADDR);
		gen_call_function_IR((void*)mmx_pandn, decode.modrm.val, FC_ADDR);
	} else {
		gen_call_function_I((void*)mmx_pandn, decode.modrm.val);
	}
}

// 0x77 - EMMS
static void dyn_mmx_emms()
{
	gen_call_function_raw((void*)setFPUTagEmpty);
}
