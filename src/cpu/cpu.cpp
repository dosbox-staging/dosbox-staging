// SPDX-FileCopyrightText:  2021-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cpu/cpu.h"

#include <cassert>
#include <cstddef>
#include <sstream>
#include <memory>

#include "config/config.h"
#include "config/setup.h"
#include "cpu/cpu.h"
#include "cpu/paging.h"
#include "debugger/debugger.h"
#include "dos/programs.h"
#include "fpu/fpu.h"
#include "gui/mapper.h"
#include "gui/titlebar.h"
#include "hardware/pic.h"
#include "lazyflags.h"
#include "misc/support.h"
#include "misc/video.h"
#include "shell/command_line.h"
#include "utils/math_utils.h"
#include "utils/string_utils.h"

#if 1
	#undef LOG
	#if defined(_MSC_VER)
		#define LOG_CONSUME(...) sizeof(__VA_ARGS__)
		#define LOG(X, Y)        LOG_CONSUME
	#else
		#define LOG(X, Y) CPU_LOG
		#define CPU_LOG(...)
	#endif
#endif

CPU_Regs cpu_regs = {};
CPUBlock cpu      = {};
Segments Segs     = {};

// Current cycles values
int CPU_Cycles    = 0;
int CPU_CycleLeft = CpuCyclesRealModeDefault;

// Cycles settings for both "legacy" and "modern" modes
int CPU_CycleMax      = CpuCyclesRealModeDefault;
int CPU_CyclePercUsed = 100;
int CPU_CycleLimit    = -1;

static int old_cycle_max       = CpuCyclesRealModeDefault;
static bool legacy_cycles_mode = false;

// Cycles settings for 'modern mode' only
static struct {
	// Fixed real mode cycles or "max cycles" if not set
	std::optional<int> real_mode = {};

	// Fixed protected mode cycles or "max cycles" if not set
	std::optional<int> protected_mode = {};

	// If true, the real mode setting controls protected mode as well
	bool protected_mode_auto = false;

	// If true, CPU throttling is enabled for both real and protected mode
	// for fixed cycles modes. The flag is ignored in "max cycle" mode (as
	// that always throttles implicitly).
	bool throttle = false;

} modern_cycles_config = {};

enum class CpuMode { Real, Protected };

// Cycle up & down counts
static constexpr auto CpuCycleStepMin     = 1;
static constexpr auto CpuCycleStepMax     = 1'000'000;
static constexpr auto DefaultCpuCycleUp   = 10;
static constexpr auto DefaultCpuCycleDown = 20;

static int cpu_cycle_up   = 0;
static int cpu_cycle_down = 0;

static bool should_hlt_on_idle = false;

int64_t CPU_IODelayRemoved = 0;

CPU_Decoder* cpudecoder;

bool CPU_CycleAutoAdjust = false;

CpuAutoDetermineMode auto_determine_mode      = {};
CpuAutoDetermineMode last_auto_determine_mode = {};

ArchitectureType CPU_ArchitectureType = ArchitectureType::Mixed;

// ID and AC flags may be toggled depending on emulated CPU architecture
static Bitu cpu_extflags_toggle = 0;

Bitu CPU_PrefetchQueueSize = 0;

void CPU_Core_Full_Init();
void CPU_Core_Normal_Init();
void CPU_Core_Simple_Init();

#if C_DYNAMIC_X86
void CPU_Core_Dyn_X86_Init();
void CPU_Core_Dyn_X86_Cache_Init(bool enable_cache);
void CPU_Core_Dyn_X86_Cache_Close();
void CPU_Core_Dyn_X86_SetFPUMode(bool dh_fpu);

#elif C_DYNREC
void CPU_Core_Dynrec_Init();
void CPU_Core_Dynrec_Cache_Init(bool enable_cache);
void CPU_Core_Dynrec_Cache_Close();
#endif

/* In debug mode exceptions are tested and dosbox exits when
 * a unhandled exception state is detected.
 * USE CHECK_EXCEPT to raise an exception in that case to see if that exception
 * solves the problem.
 *
 * In non-debug mode dosbox doesn't do detection (and hence doesn't crash at
 * that point). (game might crash later due to the unhandled exception) */

#if C_DEBUGGER
// #define CPU_CHECK_EXCEPT 1
// #define CPU_CHECK_IGNORE 1
 /* Use CHECK_EXCEPT when something doesn't work to see if a exception is
 * needed that isn't enabled by default.*/
#else
/* NORMAL NO CHECKING => More Speed */
#define CPU_CHECK_IGNORE 1
#endif /* C_DEBUGGER */

#if defined(CPU_CHECK_IGNORE)
#define CPU_CHECK_COND(cond,msg,exc,sel) {	\
	if (cond) do {} while (0);				\
}
#elif defined(CPU_CHECK_EXCEPT)
#define CPU_CHECK_COND(cond,msg,exc,sel) {	\
	if (cond) {					\
		CPU_Exception(exc,sel);		\
		return;				\
	}					\
}
#else
#define CPU_CHECK_COND(cond,msg,exc,sel) {	\
	if (cond) E_Exit(msg);			\
}
#endif

static bool is_dynamic_core_active()
{
#if C_DYNAMIC_X86
	return (cpudecoder == &CPU_Core_Dyn_X86_Run);
#elif C_DYNREC
	return (cpudecoder == &CPU_Core_Dynrec_Run);
#else
	return false;
#endif
}

static void maybe_display_max_cycles_warning()
{
	static bool displayed_warning = false;

	if (!displayed_warning) {
		LOG_WARNING(
		        "CPU: Switched to max cycles. Try setting a fixed cycles "
		        "value if you're getting audio drop-outs or the programs "
		        "runs too fast.");

		displayed_warning = true;
	}
}

static bool maybe_display_switch_to_dynamic_core_warning(const int cycles)
{
	constexpr auto CyclesThreshold = 20000;

	if (!is_dynamic_core_active() && cycles > CyclesThreshold) {
		LOG_WARNING(
		        "CPU: Setting fixed %d cycles. Try setting 'core = dynamic' "
		        "for increased performance if you need more than %d cycles.",
		        cycles,
		        CyclesThreshold);
		return true;

	} else {
		return false;
	}
}

static void set_modern_cycles_config(const CpuMode mode)
{
	auto& conf = modern_cycles_config;

	const auto new_cycles = [&]() -> std::optional<int> {
		switch (mode) {
		case CpuMode::Real: return conf.real_mode;
		case CpuMode::Protected:
			if (conf.protected_mode_auto) {
				// 'cpu_cycles' controls both real and protected mode
				return conf.real_mode;
			} else {
				return conf.protected_mode;
			}
		}
		assert(false);
		return {};
	}();

	const auto fixed_cycles = new_cycles.has_value();
	if (fixed_cycles) {
		if (conf.throttle) {
			CPU_CycleAutoAdjust = true;
			CPU_CyclePercUsed   = 100;
			CPU_CycleLimit      = *new_cycles;
		} else {
			CPU_CycleAutoAdjust = false;
			CPU_CycleMax        = *new_cycles;
		}
		maybe_display_switch_to_dynamic_core_warning(*new_cycles);

	} else {
		// "max cycles" mode
		CPU_CycleAutoAdjust = true;
		CPU_CyclePercUsed   = 100;
		CPU_CycleLimit      = -1;

		maybe_display_max_cycles_warning();
	}

	CPU_Cycles    = 0;
	CPU_CycleLeft = 0;

	const auto real_mode_with_max_cycles = (mode == CpuMode::Real &&
	                                        !fixed_cycles);
	if (!real_mode_with_max_cycles) {
		auto_determine_mode.auto_cycles = true;
	}
}

static bool is_protected_mode_program = false;

void CPU_RestoreRealModeCyclesConfig()
{
	if (cpu.pmode || (!last_auto_determine_mode.auto_core &&
	                  !last_auto_determine_mode.auto_cycles)) {
		return;
	}

	auto_determine_mode      = last_auto_determine_mode;
	last_auto_determine_mode = {};

	if (auto_determine_mode.auto_cycles) {
		if (legacy_cycles_mode) {
			CPU_CycleAutoAdjust = false;
			CPU_CycleLeft       = 0;
			CPU_Cycles          = 0;
			CPU_CycleMax        = old_cycle_max;
		} else {
			set_modern_cycles_config(CpuMode::Real);
		}

		is_protected_mode_program = false;
		TITLEBAR_NotifyCyclesChanged();
	}
#if C_DYNAMIC_X86 || C_DYNREC
	if (auto_determine_mode.auto_core) {
		cpudecoder = &CPU_Core_Normal_Run;

		CPU_CycleLeft = 0;
		CPU_Cycles    = 0;
	}
#endif
}

void Descriptor::Load(PhysPt address) {
	cpu.mpl=0;
	auto data = (uint32_t*)&saved;
	*data	  = mem_readd(address);
	*(data+1) = mem_readd(address+4);
	cpu.mpl=3;
}

void Descriptor::Save(PhysPt address) {
	cpu.mpl=0;
	auto data = (uint32_t*)&saved;
	mem_writed(address,*data);
	mem_writed(address+4,*(data+1));
	cpu.mpl=03;
}

void CPU_Push16(const uint16_t value)
{
	const uint32_t new_esp = (reg_esp & cpu.stack.notmask) |
	                         ((reg_esp - 2) & cpu.stack.mask);

	mem_writew(SegPhys(ss) + (new_esp & cpu.stack.mask), value);

	reg_esp = new_esp;
}

void CPU_Push32(const uint32_t value)
{
	const uint32_t new_esp = (reg_esp & cpu.stack.notmask) |
	                         ((reg_esp - 4) & cpu.stack.mask);

	mem_writed(SegPhys(ss) + (new_esp & cpu.stack.mask), value);

	reg_esp = new_esp;
}

uint16_t CPU_Pop16()
{
	const auto val = check_cast<uint16_t>(
	        mem_readw(SegPhys(ss) + (reg_esp & cpu.stack.mask)));

	reg_esp = (reg_esp & cpu.stack.notmask) | ((reg_esp + 2) & cpu.stack.mask);

	return val;
}

uint32_t CPU_Pop32()
{
	const auto val = check_cast<uint32_t>(
	        mem_readd(SegPhys(ss) + (reg_esp & cpu.stack.mask)));

	reg_esp = (reg_esp & cpu.stack.notmask) | ((reg_esp + 4) & cpu.stack.mask);

	return val;
}

void CPU_SetFlags(const uint32_t word, uint32_t mask)
{
	// ID-flag and AC-flag can be toggled on  CPUID-supporting CPUs
	mask |= cpu_extflags_toggle;

	reg_flags     = (reg_flags & ~mask) | (word & mask) | 2;
	cpu.direction = 1 - ((reg_flags & FLAG_DF) >> 9);
}

void CPU_SetFlagsd(const uint32_t word)
{
	const auto mask = cpu.cpl ? FMASK_NORMAL : FMASK_ALL;
	CPU_SetFlags(word, mask);
}

void CPU_SetFlagsw(const uint32_t word)
{
	const auto mask = (cpu.cpl ? FMASK_NORMAL : FMASK_ALL) & 0xffff;
	CPU_SetFlags(word, mask);
}

bool CPU_PrepareException(Bitu which, Bitu error)
{
	cpu.exception.which = which;
	cpu.exception.error=error;
	return true;
}

bool CPU_CLI() {
	if (cpu.pmode && ((!GETFLAG(VM) && (GETFLAG_IOPL<cpu.cpl)) || (GETFLAG(VM) && (GETFLAG_IOPL<3)))) {
		return CPU_PrepareException(EXCEPTION_GP,0);
	} else {
		SETFLAGBIT(IF,false);
		return false;
	}
}

bool CPU_STI() {
	if (cpu.pmode && ((!GETFLAG(VM) && (GETFLAG_IOPL<cpu.cpl)) || (GETFLAG(VM) && (GETFLAG_IOPL<3)))) {
		return CPU_PrepareException(EXCEPTION_GP,0);
	} else {
 		SETFLAGBIT(IF,true);
		return false;
	}
}

bool CPU_POPF(Bitu use32) {
	if (cpu.pmode && GETFLAG(VM) && (GETFLAG(IOPL)!=FLAG_IOPL)) {
		/* Not enough privileges to execute POPF */
		return CPU_PrepareException(EXCEPTION_GP,0);
	}
	Bitu mask=FMASK_ALL;
	/* IOPL field can only be modified when CPL=0 or in real mode: */
	if (cpu.pmode && (cpu.cpl>0)) mask &= (~FLAG_IOPL);
	if (cpu.pmode && !GETFLAG(VM) && (GETFLAG_IOPL<cpu.cpl)) mask &= (~FLAG_IF);
	if (use32)
		CPU_SetFlags(CPU_Pop32(),mask);
	else CPU_SetFlags(CPU_Pop16(),mask & 0xffff);
	DestroyConditionFlags();
	return false;
}

bool CPU_PUSHF(Bitu use32) {
	if (cpu.pmode && GETFLAG(VM) && (GETFLAG(IOPL)!=FLAG_IOPL)) {
		/* Not enough privileges to execute PUSHF */
		return CPU_PrepareException(EXCEPTION_GP,0);
	}
	FillFlags();
	if (use32)
		CPU_Push32(reg_flags & 0xfcffff);
	else CPU_Push16(reg_flags);
	return false;
}

static void cpu_check_segments()
{
	bool needs_invalidation = false;
	Descriptor desc;
	if (!cpu.gdt.GetDescriptor(SegValue(es),desc)) needs_invalidation = true;
	else switch (desc.Type()) {
		case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
		case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
		case DESC_CODE_N_NC_A:  	case DESC_CODE_N_NC_NA:	case DESC_CODE_R_NC_A:  	case DESC_CODE_R_NC_NA:
			if (cpu.cpl > desc.DPL()) needs_invalidation = true;
			break;
		default: break;	}
	if (needs_invalidation) CPU_SetSegGeneral(es,0);

	needs_invalidation = false;
	if (!cpu.gdt.GetDescriptor(SegValue(ds),desc)) needs_invalidation = true;
	else switch (desc.Type()) {
		case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
		case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
		case DESC_CODE_N_NC_A:  	case DESC_CODE_N_NC_NA:	case DESC_CODE_R_NC_A:  	case DESC_CODE_R_NC_NA:
			if (cpu.cpl > desc.DPL()) needs_invalidation = true;
			break;
		default: break;	}
	if (needs_invalidation) CPU_SetSegGeneral(ds,0);

	needs_invalidation = false;
	if (!cpu.gdt.GetDescriptor(SegValue(fs),desc)) needs_invalidation = true;
	else switch (desc.Type()) {
		case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
		case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
		case DESC_CODE_N_NC_A:  	case DESC_CODE_N_NC_NA:	case DESC_CODE_R_NC_A:  	case DESC_CODE_R_NC_NA:
			if (cpu.cpl > desc.DPL()) needs_invalidation = true;
			break;
		default: break;	}
	if (needs_invalidation) CPU_SetSegGeneral(fs,0);

	needs_invalidation = false;
	if (!cpu.gdt.GetDescriptor(SegValue(gs),desc)) needs_invalidation = true;
	else switch (desc.Type()) {
		case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
		case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
		case DESC_CODE_N_NC_A:  	case DESC_CODE_N_NC_NA:	case DESC_CODE_R_NC_A:  	case DESC_CODE_R_NC_NA:
			if (cpu.cpl > desc.DPL()) needs_invalidation = true;
			break;
		default: break;	}
	if (needs_invalidation) CPU_SetSegGeneral(gs,0);
}

class TaskStateSegment {
public:
	TaskStateSegment() = default;

	bool IsValid() const { return valid; }

	Bitu Get_back() {
		cpu.mpl=0;
		uint16_t backlink=mem_readw(base);
		cpu.mpl=3;
		return backlink;
	}
	void SaveSelector() {
		cpu.gdt.SetDescriptor(selector,desc);
	}
	void Get_SSx_ESPx(Bitu level,Bitu & _ss,Bitu & _esp) {
		cpu.mpl=0;
		if (is386) {
			PhysPt where=base+offsetof(TSS_32,esp0)+level*8;
			_esp=mem_readd(where);
			_ss=mem_readw(where+4);
		} else {
			PhysPt where=base+offsetof(TSS_16,sp0)+level*4;
			_esp=mem_readw(where);
			_ss=mem_readw(where+2);
		}
		cpu.mpl=3;
	}
	bool SetSelector(Bitu new_sel) {
		valid=false;
		if ((new_sel & 0xfffc)==0) {
			selector=0;
			base=0;
			limit=0;
			is386=1;
			return true;
		}
		if (new_sel&4) return false;
		if (!cpu.gdt.GetDescriptor(new_sel,desc)) return false;
		switch (desc.Type()) {
			case DESC_286_TSS_A:		case DESC_286_TSS_B:
			case DESC_386_TSS_A:		case DESC_386_TSS_B:
				break;
			default:
				return false;
		}
		if (!desc.saved.seg.p) return false;
		selector=new_sel;
		valid=true;
		base=desc.GetBase();
		limit=desc.GetLimit();
		is386=desc.Is386();
		return true;
	}

	TSS_Descriptor desc = {};
	Bitu selector = 0;
	PhysPt base = 0;
	Bitu limit = 0;
	Bitu is386 = 0;
	bool valid = false;
};

static TaskStateSegment cpu_tss;

enum TSwitchType {
	TSwitch_JMP,TSwitch_CALL_INT,TSwitch_IRET
};

static bool cpu_switch_task(Bitu new_tss_selector,TSwitchType tstype,Bitu old_eip)
{
	FillFlags();
	TaskStateSegment new_tss;
	if (!new_tss.SetSelector(new_tss_selector))
		E_Exit("Illegal TSS for switch, selector=%" sBitfs(x) ", switchtype=%x",new_tss_selector,tstype);
	if (tstype==TSwitch_IRET) {
		if (!new_tss.desc.IsBusy())
			E_Exit("TSS not busy for IRET");
	} else {
		if (new_tss.desc.IsBusy())
			E_Exit("TSS busy for JMP/CALL/INT");
	}
	Bitu new_cr3=0;
	Bitu new_eax,new_ebx,new_ecx,new_edx,new_esp,new_ebp,new_esi,new_edi;
	Bitu new_es,new_cs,new_ss,new_ds,new_fs,new_gs;
	Bitu new_ldt,new_eip,new_eflags;
	/* Read new context from new TSS */
	if (new_tss.is386) {
		new_cr3=mem_readd(new_tss.base+offsetof(TSS_32,cr3));
		new_eip=mem_readd(new_tss.base+offsetof(TSS_32,eip));
		new_eflags=mem_readd(new_tss.base+offsetof(TSS_32,eflags));
		new_eax=mem_readd(new_tss.base+offsetof(TSS_32,eax));
		new_ecx=mem_readd(new_tss.base+offsetof(TSS_32,ecx));
		new_edx=mem_readd(new_tss.base+offsetof(TSS_32,edx));
		new_ebx=mem_readd(new_tss.base+offsetof(TSS_32,ebx));
		new_esp=mem_readd(new_tss.base+offsetof(TSS_32,esp));
		new_ebp=mem_readd(new_tss.base+offsetof(TSS_32,ebp));
		new_edi=mem_readd(new_tss.base+offsetof(TSS_32,edi));
		new_esi=mem_readd(new_tss.base+offsetof(TSS_32,esi));

		new_es=mem_readw(new_tss.base+offsetof(TSS_32,es));
		new_cs=mem_readw(new_tss.base+offsetof(TSS_32,cs));
		new_ss=mem_readw(new_tss.base+offsetof(TSS_32,ss));
		new_ds=mem_readw(new_tss.base+offsetof(TSS_32,ds));
		new_fs=mem_readw(new_tss.base+offsetof(TSS_32,fs));
		new_gs=mem_readw(new_tss.base+offsetof(TSS_32,gs));
		new_ldt=mem_readw(new_tss.base+offsetof(TSS_32,ldt));
	} else {
		E_Exit("286 task switch");
		/*
		Dead code after exit
		--------------------
		new_cr3=0;
		new_eip=0;
		new_eflags=0;
		new_eax=0;	new_ecx=0;	new_edx=0;	new_ebx=0;
		new_esp=0;	new_ebp=0;	new_edi=0;	new_esi=0;

		new_es=0;	new_cs=0;	new_ss=0;	new_ds=0;
		new_fs=0;	new_gs=0; new_ldt=0;
		*/
	}

	/* Check if we need to clear busy bit of old TASK */
	if (tstype==TSwitch_JMP || tstype==TSwitch_IRET) {
		cpu_tss.desc.SetBusy(false);
		cpu_tss.SaveSelector();
	}
	uint32_t old_flags = reg_flags;
	if (tstype==TSwitch_IRET) old_flags &= (~FLAG_NT);

	/* Save current context in current TSS */
	if (cpu_tss.is386) {
		mem_writed(cpu_tss.base+offsetof(TSS_32,eflags),old_flags);
		mem_writed(cpu_tss.base+offsetof(TSS_32,eip),old_eip);

		mem_writed(cpu_tss.base+offsetof(TSS_32,eax),reg_eax);
		mem_writed(cpu_tss.base+offsetof(TSS_32,ecx),reg_ecx);
		mem_writed(cpu_tss.base+offsetof(TSS_32,edx),reg_edx);
		mem_writed(cpu_tss.base+offsetof(TSS_32,ebx),reg_ebx);
		mem_writed(cpu_tss.base+offsetof(TSS_32,esp),reg_esp);
		mem_writed(cpu_tss.base+offsetof(TSS_32,ebp),reg_ebp);
		mem_writed(cpu_tss.base+offsetof(TSS_32,esi),reg_esi);
		mem_writed(cpu_tss.base+offsetof(TSS_32,edi),reg_edi);

		mem_writed(cpu_tss.base+offsetof(TSS_32,es),SegValue(es));
		mem_writed(cpu_tss.base+offsetof(TSS_32,cs),SegValue(cs));
		mem_writed(cpu_tss.base+offsetof(TSS_32,ss),SegValue(ss));
		mem_writed(cpu_tss.base+offsetof(TSS_32,ds),SegValue(ds));
		mem_writed(cpu_tss.base+offsetof(TSS_32,fs),SegValue(fs));
		mem_writed(cpu_tss.base+offsetof(TSS_32,gs),SegValue(gs));
	} else {
		E_Exit("286 task switch");
	}

	/* Setup a back link to the old TSS in new TSS */
	if (tstype==TSwitch_CALL_INT) {
		assert(new_tss.is386);
		mem_writed(new_tss.base + offsetof(TSS_32, back), cpu_tss.selector);

		/* And make the new task's eflag have the nested task bit */
		new_eflags|=FLAG_NT;
	}
	/* Set the busy bit in the new task */
	if (tstype==TSwitch_JMP || tstype==TSwitch_CALL_INT) {
		new_tss.desc.SetBusy(true);
		new_tss.SaveSelector();
	}

//	cpu.cr0|=CR0_TASKSWITCHED;
	if (new_tss_selector == cpu_tss.selector) {
		reg_eip = old_eip;
		new_cs = SegValue(cs);
		new_ss = SegValue(ss);
		new_ds = SegValue(ds);
		new_es = SegValue(es);
		new_fs = SegValue(fs);
		new_gs = SegValue(gs);
	} else {

		/* Setup the new cr3 */
		PAGING_SetDirBase(new_cr3);

		/* Load new context */
		assert(new_tss.is386);
		reg_eip = new_eip;
		CPU_SetFlags(new_eflags, FMASK_ALL | FLAG_VM);
		reg_eax = new_eax;
		reg_ecx = new_ecx;
		reg_edx = new_edx;
		reg_ebx = new_ebx;
		reg_esp = new_esp;
		reg_ebp = new_ebp;
		reg_edi = new_edi;
		reg_esi = new_esi;
	}
	/* Load the new selectors */
	if (reg_flags & FLAG_VM) {
		SegSet16(cs,new_cs);
		cpu.code.big=false;
		cpu.cpl=3;			//We don't have segment caches so this will do
	} else {
		/* Protected mode task */
		if (new_ldt!=0) CPU_LLDT(new_ldt);
		/* Load the new CS*/
		Descriptor cs_desc;
		cpu.cpl=new_cs & 3;
		if (!cpu.gdt.GetDescriptor(new_cs,cs_desc))
			E_Exit("Task switch with CS beyond limits");
		if (!cs_desc.saved.seg.p)
			E_Exit("Task switch with non present code-segment");
		switch (cs_desc.Type()) {
		case DESC_CODE_N_NC_A:		case DESC_CODE_N_NC_NA:
		case DESC_CODE_R_NC_A:		case DESC_CODE_R_NC_NA:
			if (cpu.cpl != cs_desc.DPL()) E_Exit("Task CS RPL != DPL");
			goto doconforming;
		case DESC_CODE_N_C_A:		case DESC_CODE_N_C_NA:
		case DESC_CODE_R_C_A:		case DESC_CODE_R_C_NA:
			if (cpu.cpl < cs_desc.DPL()) E_Exit("Task CS RPL < DPL");
doconforming:
			Segs.phys[cs]=cs_desc.GetBase();
			cpu.code.big=cs_desc.Big()>0;
			Segs.val[cs]=new_cs;
			break;
		default: E_Exit("Task switch CS Type 0x%x", cs_desc.Type());
		}
	}
	CPU_SetSegGeneral(es,new_es);
	CPU_SetSegGeneral(ss,new_ss);
	CPU_SetSegGeneral(ds,new_ds);
	CPU_SetSegGeneral(fs,new_fs);
	CPU_SetSegGeneral(gs,new_gs);
	if (!cpu_tss.SetSelector(new_tss_selector)) {
		LOG(LOG_CPU,LOG_NORMAL)("TaskSwitch: set tss selector %X failed",new_tss_selector);
	}
//	cpu_tss.desc.SetBusy(true);
//	cpu_tss.SaveSelector();
//	LOG_MSG("Task CPL %X CS:%X IP:%X SS:%X SP:%X eflags %x",cpu.cpl,SegValue(cs),reg_eip,SegValue(ss),reg_esp,reg_flags);
	return true;
}

bool CPU_IO_Exception(Bitu port,Bitu size) {
	if (cpu.pmode && ((GETFLAG_IOPL<cpu.cpl) || GETFLAG(VM))) {
		cpu.mpl=0;
		if (!cpu_tss.is386) goto doexception;
		PhysPt bwhere=cpu_tss.base+0x66;
		Bitu ofs=mem_readw(bwhere);
		if (ofs>cpu_tss.limit) goto doexception;
		bwhere=cpu_tss.base+ofs+(port/8);
		Bitu map=mem_readw(bwhere);
		Bitu mask=(0xffff>>(16-size)) << (port&7);
		if (map & mask) goto doexception;
		cpu.mpl=3;
	}
	return false;
doexception:
	cpu.mpl=3;
	LOG(LOG_CPU,LOG_NORMAL)("IO Exception port %X",port);
	return CPU_PrepareException(EXCEPTION_GP,0);
}

void CPU_DebugException(uint32_t triggers,Bitu oldeip) {
	cpu.drx[6] = (cpu.drx[6] & 0xFFFF1FF0) | triggers;
	CPU_Interrupt(EXCEPTION_DB,CPU_INT_EXCEPTION,oldeip);
}

void CPU_Exception(Bitu which,Bitu error ) {
//	LOG_MSG("Exception %d error %x",which,error);
	cpu.exception.error=error;
	CPU_Interrupt(which,CPU_INT_EXCEPTION | ((which>=8) ? CPU_INT_HAS_ERROR : 0),reg_eip);
}

static uint8_t last_interrupt;

uint8_t CPU_GetLastInterrupt()
{
	return last_interrupt;
}

void CPU_Interrupt(Bitu num,Bitu type,Bitu oldeip) {
	if (num == EXCEPTION_DB && (type&CPU_INT_EXCEPTION) == 0) {
		CPU_DebugException(0,oldeip); // DR6 bits need updating
		return;
	}
	last_interrupt = num;

	FillFlags();
#if C_DEBUGGER
	switch (num) {
	case 0xcd:
#if C_HEAVY_DEBUGGER
 		LOG(LOG_CPU,LOG_ERROR)("Call to interrupt 0xCD this is BAD");
//		DEBUG_HeavyWriteLogInstruction();
//		E_Exit("Call to interrupt 0xCD this is BAD");
#endif
		break;
	case 0x03:
		if (DEBUG_Breakpoint()) {
			CPU_Cycles=0;
			return;
		}
	};
#endif
	if (!cpu.pmode) {
		/* Save everything on a 16-bit stack */
		CPU_Push16(reg_flags & 0xffff);
		CPU_Push16(SegValue(cs));
		CPU_Push16(oldeip);
		SETFLAGBIT(IF,false);
		SETFLAGBIT(TF,false);
		/* Get the new CS:IP from vector table */
		PhysPt base=cpu.idt.GetBase();
		reg_eip=mem_readw(base+(num << 2));
		Segs.val[cs]=mem_readw(base+(num << 2)+2);
		Segs.phys[cs]=Segs.val[cs]<<4;
		cpu.code.big=false;
		return;
	} else {
		/* Protected Mode Interrupt */
		if ((reg_flags & FLAG_VM) && (type&CPU_INT_SOFTWARE) && !(type&CPU_INT_NOIOPLCHECK)) {
//			LOG_MSG("Software int in v86, AH %X IOPL %x",reg_ah,(reg_flags & FLAG_IOPL) >>12);
			if ((reg_flags & FLAG_IOPL)!=FLAG_IOPL) {
				CPU_Exception(EXCEPTION_GP,0);
				return;
			}
		}

		Descriptor gate;
		if (!cpu.idt.GetDescriptor(num<<3,gate)) {
			// zone66
			CPU_Exception(EXCEPTION_GP,num*8+2+((type&CPU_INT_SOFTWARE)?0:1));
			return;
		}

		if ((type&CPU_INT_SOFTWARE) && (gate.DPL()<cpu.cpl)) {
			// zone66, win3.x e
			CPU_Exception(EXCEPTION_GP,num*8+2);
			return;
		}


		switch (gate.Type()) {
		case DESC_286_INT_GATE:		case DESC_386_INT_GATE:
		case DESC_286_TRAP_GATE:	case DESC_386_TRAP_GATE:
			{
				CPU_CHECK_COND(!gate.saved.seg.p,
					"INT:Gate segment not present",
					EXCEPTION_NP,num*8+2+((type&CPU_INT_SOFTWARE)?0:1))

				Descriptor cs_desc;
				Bitu gate_sel=gate.GetSelector();
				Bitu gate_off=gate.GetOffset();
				CPU_CHECK_COND((gate_sel & 0xfffc)==0,
					"INT:Gate with CS zero selector",
					EXCEPTION_GP,(type&CPU_INT_SOFTWARE)?0:1)
				CPU_CHECK_COND(!cpu.gdt.GetDescriptor(gate_sel,cs_desc),
					"INT:Gate with CS beyond limit",
					EXCEPTION_GP,(gate_sel & 0xfffc)+((type&CPU_INT_SOFTWARE)?0:1))

				Bitu cs_dpl=cs_desc.DPL();
				CPU_CHECK_COND(cs_dpl>cpu.cpl,
					"Interrupt to higher privilege",
					EXCEPTION_GP,(gate_sel & 0xfffc)+((type&CPU_INT_SOFTWARE)?0:1))
				switch (cs_desc.Type()) {
				case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:
				case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
					if (cs_dpl<cpu.cpl) {
						/* Prepare for gate to inner level */
						CPU_CHECK_COND(!cs_desc.saved.seg.p,
							"INT:Inner level:CS segment not present",
							EXCEPTION_NP,(gate_sel & 0xfffc)+((type&CPU_INT_SOFTWARE)?0:1))
						CPU_CHECK_COND((reg_flags & FLAG_VM) && (cs_dpl!=0),
							"V86 interrupt calling codesegment with DPL>0",
							EXCEPTION_GP,gate_sel & 0xfffc)

						Bitu n_ss,n_esp;
						Bitu o_ss,o_esp;
						o_ss=SegValue(ss);
						o_esp=reg_esp;
						cpu_tss.Get_SSx_ESPx(cs_dpl,n_ss,n_esp);
						CPU_CHECK_COND((n_ss & 0xfffc)==0,
							"INT:Gate with SS zero selector",
							EXCEPTION_TS,(type&CPU_INT_SOFTWARE)?0:1)
						Descriptor n_ss_desc;
						CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_ss,n_ss_desc),
							"INT:Gate with SS beyond limit",
							EXCEPTION_TS,(n_ss & 0xfffc)+((type&CPU_INT_SOFTWARE)?0:1))
						CPU_CHECK_COND(((n_ss & 3)!=cs_dpl) || (n_ss_desc.DPL()!=cs_dpl),
							"INT:Inner level with CS_DPL!=SS_DPL and SS_RPL",
							EXCEPTION_TS,(n_ss & 0xfffc)+((type&CPU_INT_SOFTWARE)?0:1))

						// check if stack segment is a writable data segment
						switch (n_ss_desc.Type()) {
						case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
						case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
							break;
						default:
							E_Exit("INT:Inner level:Stack segment not writable.");		// or #TS(ss_sel+EXT)
						}
						CPU_CHECK_COND(!n_ss_desc.saved.seg.p,
							"INT:Inner level with nonpresent SS",
							EXCEPTION_SS,(n_ss & 0xfffc)+((type&CPU_INT_SOFTWARE)?0:1))

						// commit point
						Segs.phys[ss]=n_ss_desc.GetBase();
						Segs.val[ss]=n_ss;
						if (n_ss_desc.Big()) {
							cpu.stack.big=true;
							cpu.stack.mask=0xffffffff;
							cpu.stack.notmask=0;
							reg_esp=n_esp;
						} else {
							cpu.stack.big=false;
							cpu.stack.mask=0xffff;
							cpu.stack.notmask=0xffff0000;
							reg_sp=n_esp & 0xffff;
						}

						cpu.cpl=cs_dpl;
						if (gate.Type() & 0x8) {	/* 32-bit Gate */
							if (reg_flags & FLAG_VM) {
								CPU_Push32(SegValue(gs));SegSet16(gs,0x0);
								CPU_Push32(SegValue(fs));SegSet16(fs,0x0);
								CPU_Push32(SegValue(ds));SegSet16(ds,0x0);
								CPU_Push32(SegValue(es));SegSet16(es,0x0);
							}
							CPU_Push32(o_ss);
							CPU_Push32(o_esp);
						} else {					/* 16-bit Gate */
							if (reg_flags & FLAG_VM) E_Exit("V86 to 16-bit gate");
							CPU_Push16(o_ss);
							CPU_Push16(o_esp);
						}
//						LOG_MSG("INT:Gate to inner level SS:%X SP:%X",n_ss,n_esp);
						goto do_interrupt;
					}
					if (cs_dpl!=cpu.cpl)
						E_Exit("Non-conforming intra privilege INT with DPL!=CPL");
					[[fallthrough]];
				case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
				case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
					/* Prepare stack for gate to same priviledge */
					CPU_CHECK_COND(!cs_desc.saved.seg.p,
							"INT:Same level:CS segment not present",
						EXCEPTION_NP,(gate_sel & 0xfffc)+((type&CPU_INT_SOFTWARE)?0:1))
					if ((reg_flags & FLAG_VM) && (cs_dpl<cpu.cpl))
						E_Exit("V86 interrupt doesn't change to pl0");	// or #GP(cs_sel)

					// commit point
do_interrupt:
					if (gate.Type() & 0x8) {	/* 32-bit Gate */
						CPU_Push32(reg_flags);
						CPU_Push32(SegValue(cs));
						CPU_Push32(oldeip);
						if (type & CPU_INT_HAS_ERROR) CPU_Push32(cpu.exception.error);
					} else {					/* 16-bit gate */
						CPU_Push16(reg_flags & 0xffff);
						CPU_Push16(SegValue(cs));
						CPU_Push16(oldeip);
						if (type & CPU_INT_HAS_ERROR) CPU_Push16(cpu.exception.error);
					}
					break;
				default:
				        E_Exit("INT:Gate Selector points to illegal descriptor with type 0x%x",
				               cs_desc.Type());
			        }

				Segs.val[cs]=(gate_sel&0xfffc) | cpu.cpl;
				Segs.phys[cs]=cs_desc.GetBase();
				cpu.code.big=cs_desc.Big()>0;
				reg_eip=gate_off;

				if (!(gate.Type()&1)) {
					SETFLAGBIT(IF,false);
				}
				SETFLAGBIT(TF,false);
				SETFLAGBIT(NT,false);
				SETFLAGBIT(VM,false);
				LOG(LOG_CPU,LOG_NORMAL)("INT:Gate to %X:%X big %d %s",gate_sel,gate_off,cs_desc.Big(),gate.Type() & 0x8 ? "386" : "286");
				return;
			}
		case DESC_TASK_GATE:
			CPU_CHECK_COND(!gate.saved.seg.p,
				"INT:Gate segment not present",
				EXCEPTION_NP,num*8+2+((type&CPU_INT_SOFTWARE)?0:1))

			cpu_switch_task(gate.GetSelector(),TSwitch_CALL_INT,oldeip);
			if (type & CPU_INT_HAS_ERROR) {
				//TODO Be sure about this, seems somewhat unclear
				if (cpu_tss.is386) CPU_Push32(cpu.exception.error);
				else CPU_Push16(cpu.exception.error);
			}
			return;
		default:
			E_Exit("Illegal descriptor type 0x%x for int %" sBitfs(X),
			       gate.Type(), num);
		}
	}
	return ; // make compiler happy
}


void CPU_IRET(bool use32,Bitu oldeip) {
	if (!cpu.pmode) {					/* RealMode IRET */
		if (use32) {
			reg_eip=CPU_Pop32();
			SegSet16(cs,CPU_Pop32());
			CPU_SetFlags(CPU_Pop32(),FMASK_ALL);
		} else {
			reg_eip=CPU_Pop16();
			SegSet16(cs,CPU_Pop16());
			CPU_SetFlags(CPU_Pop16(),FMASK_ALL & 0xffff);
		}
		cpu.code.big=false;
		DestroyConditionFlags();
		return;
	} else {	/* Protected mode IRET */
		if (reg_flags & FLAG_VM) {
			if ((reg_flags & FLAG_IOPL)!=FLAG_IOPL) {
				// win3.x e
				CPU_Exception(EXCEPTION_GP,0);
				return;
			} else {
				if (use32) {
					uint32_t new_eip=mem_readd(SegPhys(ss) + (reg_esp & cpu.stack.mask));
					uint32_t tempesp=(reg_esp&cpu.stack.notmask)|((reg_esp+4)&cpu.stack.mask);
					uint32_t new_cs=mem_readd(SegPhys(ss) + (tempesp & cpu.stack.mask));
					tempesp=(tempesp&cpu.stack.notmask)|((tempesp+4)&cpu.stack.mask);
					uint32_t new_flags=mem_readd(SegPhys(ss) + (tempesp & cpu.stack.mask));
					reg_esp=(tempesp&cpu.stack.notmask)|((tempesp+4)&cpu.stack.mask);

					reg_eip=new_eip;
					SegSet16(cs,(uint16_t)(new_cs&0xffff));
					/* IOPL can not be modified in v86 mode by IRET */
					CPU_SetFlags(new_flags,FMASK_NORMAL|FLAG_NT);
				} else {
					uint16_t new_eip=mem_readw(SegPhys(ss) + (reg_esp & cpu.stack.mask));
					uint32_t tempesp=(reg_esp&cpu.stack.notmask)|((reg_esp+2)&cpu.stack.mask);
					uint16_t new_cs=mem_readw(SegPhys(ss) + (tempesp & cpu.stack.mask));
					tempesp=(tempesp&cpu.stack.notmask)|((tempesp+2)&cpu.stack.mask);
					uint16_t new_flags=mem_readw(SegPhys(ss) + (tempesp & cpu.stack.mask));
					reg_esp=(tempesp&cpu.stack.notmask)|((tempesp+2)&cpu.stack.mask);

					reg_eip=(uint32_t)new_eip;
					SegSet16(cs,new_cs);
					/* IOPL can not be modified in v86 mode by IRET */
					CPU_SetFlags(new_flags,FMASK_NORMAL|FLAG_NT);
				}
				cpu.code.big=false;
				DestroyConditionFlags();
				return;
			}
		}
		/* Check if this is task IRET */
		if (GETFLAG(NT)) {
			if (GETFLAG(VM)) E_Exit("Pmode IRET with VM bit set");
			CPU_CHECK_COND(!cpu_tss.IsValid(),
				"TASK Iret without valid TSS",
				EXCEPTION_TS,cpu_tss.selector & 0xfffc)
			if (!cpu_tss.desc.IsBusy()) {
				LOG(LOG_CPU,LOG_ERROR)("TASK Iret:TSS not busy");
			}
			Bitu back_link=cpu_tss.Get_back();
			cpu_switch_task(back_link,TSwitch_IRET,oldeip);
			return;
		}
		Bitu n_cs_sel,n_eip,n_flags;
		uint32_t tempesp;
		if (use32) {
			n_eip=mem_readd(SegPhys(ss) + (reg_esp & cpu.stack.mask));
			tempesp=(reg_esp&cpu.stack.notmask)|((reg_esp+4)&cpu.stack.mask);
			n_cs_sel=mem_readd(SegPhys(ss) + (tempesp & cpu.stack.mask)) & 0xffff;
			tempesp=(tempesp&cpu.stack.notmask)|((tempesp+4)&cpu.stack.mask);
			n_flags=mem_readd(SegPhys(ss) + (tempesp & cpu.stack.mask));
			tempesp=(tempesp&cpu.stack.notmask)|((tempesp+4)&cpu.stack.mask);

			if ((n_flags & FLAG_VM) && (cpu.cpl==0)) {
				// commit point
				reg_esp=tempesp;
				reg_eip=n_eip & 0xffff;
				Bitu n_ss,n_esp,n_es,n_ds,n_fs,n_gs;
				n_esp=CPU_Pop32();
				n_ss=CPU_Pop32() & 0xffff;
				n_es=CPU_Pop32() & 0xffff;
				n_ds=CPU_Pop32() & 0xffff;
				n_fs=CPU_Pop32() & 0xffff;
				n_gs=CPU_Pop32() & 0xffff;

				CPU_SetFlags(n_flags,FMASK_ALL | FLAG_VM);
				DestroyConditionFlags();
				cpu.cpl=3;

				CPU_SetSegGeneral(ss,n_ss);
				CPU_SetSegGeneral(es,n_es);
				CPU_SetSegGeneral(ds,n_ds);
				CPU_SetSegGeneral(fs,n_fs);
				CPU_SetSegGeneral(gs,n_gs);
				reg_esp=n_esp;
				cpu.code.big=false;
				SegSet16(cs,n_cs_sel);
				LOG(LOG_CPU,LOG_NORMAL)("IRET:Back to V86: CS:%X IP %X SS:%X SP %X FLAGS:%X",SegValue(cs),reg_eip,SegValue(ss),reg_esp,reg_flags);
				return;
			}
			if (n_flags & FLAG_VM) E_Exit("IRET from pmode to v86 with CPL!=0");
		} else {
			n_eip=mem_readw(SegPhys(ss) + (reg_esp & cpu.stack.mask));
			tempesp=(reg_esp&cpu.stack.notmask)|((reg_esp+2)&cpu.stack.mask);
			n_cs_sel=mem_readw(SegPhys(ss) + (tempesp & cpu.stack.mask));
			tempesp=(tempesp&cpu.stack.notmask)|((tempesp+2)&cpu.stack.mask);
			n_flags=mem_readw(SegPhys(ss) + (tempesp & cpu.stack.mask));
			n_flags|=(reg_flags & 0xffff0000);
			tempesp=(tempesp&cpu.stack.notmask)|((tempesp+2)&cpu.stack.mask);

			if (n_flags & FLAG_VM) E_Exit("VM Flag in 16-bit iret");
		}
		CPU_CHECK_COND((n_cs_sel & 0xfffc)==0,
			"IRET:CS selector zero",
			EXCEPTION_GP,0)
		Bitu n_cs_rpl=n_cs_sel & 3;
		Descriptor n_cs_desc;
		CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_cs_sel,n_cs_desc),
			"IRET:CS selector beyond limits",
			EXCEPTION_GP,n_cs_sel & 0xfffc)
		CPU_CHECK_COND(n_cs_rpl<cpu.cpl,
			"IRET to lower privilege",
			EXCEPTION_GP,n_cs_sel & 0xfffc)

		switch (n_cs_desc.Type()) {
		case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:
		case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
			CPU_CHECK_COND(n_cs_rpl!=n_cs_desc.DPL(),
				"IRET:NC:DPL!=RPL",
				EXCEPTION_GP,n_cs_sel & 0xfffc)
			break;
		case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
		case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
			CPU_CHECK_COND(n_cs_desc.DPL()>n_cs_rpl,
				"IRET:C:DPL>RPL",
				EXCEPTION_GP,n_cs_sel & 0xfffc)
			break;
		default:
			E_Exit("IRET:Illegal descriptor type 0x%x",
			       n_cs_desc.Type());
		}
		CPU_CHECK_COND(!n_cs_desc.saved.seg.p,
			"IRET with nonpresent code segment",
			EXCEPTION_NP,n_cs_sel & 0xfffc)

		if (n_cs_rpl==cpu.cpl) {
			/* Return to same level */

			// commit point
			reg_esp=tempesp;
			Segs.phys[cs]=n_cs_desc.GetBase();
			cpu.code.big=n_cs_desc.Big()>0;
			Segs.val[cs]=n_cs_sel;
			reg_eip=n_eip;

			Bitu mask=cpu.cpl ? (FMASK_NORMAL | FLAG_NT) : FMASK_ALL;
			if (GETFLAG_IOPL<cpu.cpl) mask &= (~FLAG_IF);
			CPU_SetFlags(n_flags,mask);
			DestroyConditionFlags();
			LOG(LOG_CPU,LOG_NORMAL)("IRET:Same level:%X:%X big %d",n_cs_sel,n_eip,cpu.code.big);
		} else {
			/* Return to outer level */
			Bitu n_ss,n_esp;
			if (use32) {
				n_esp=mem_readd(SegPhys(ss) + (tempesp & cpu.stack.mask));
				tempesp=(tempesp&cpu.stack.notmask)|((tempesp+4)&cpu.stack.mask);
				n_ss=mem_readd(SegPhys(ss) + (tempesp & cpu.stack.mask)) & 0xffff;
			} else {
				n_esp=mem_readw(SegPhys(ss) + (tempesp & cpu.stack.mask));
				tempesp=(tempesp&cpu.stack.notmask)|((tempesp+2)&cpu.stack.mask);
				n_ss=mem_readw(SegPhys(ss) + (tempesp & cpu.stack.mask));
			}
			CPU_CHECK_COND((n_ss & 0xfffc)==0,
				"IRET:Outer level:SS selector zero",
				EXCEPTION_GP,0)
			CPU_CHECK_COND((n_ss & 3)!=n_cs_rpl,
				"IRET:Outer level:SS rpl!=CS rpl",
				EXCEPTION_GP,n_ss & 0xfffc)
			Descriptor n_ss_desc;
			CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_ss,n_ss_desc),
				"IRET:Outer level:SS beyond limit",
				EXCEPTION_GP,n_ss & 0xfffc)
			CPU_CHECK_COND(n_ss_desc.DPL()!=n_cs_rpl,
				"IRET:Outer level:SS dpl!=CS rpl",
				EXCEPTION_GP,n_ss & 0xfffc)

			// check if stack segment is a writable data segment
			switch (n_ss_desc.Type()) {
			case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
				break;
			default:
				E_Exit("IRET:Outer level:Stack segment not writable");		// or #GP(ss_sel)
			}
			CPU_CHECK_COND(!n_ss_desc.saved.seg.p,
				"IRET:Outer level:Stack segment not present",
				EXCEPTION_NP,n_ss & 0xfffc)

			// commit point

			Segs.phys[cs]=n_cs_desc.GetBase();
			cpu.code.big=n_cs_desc.Big()>0;
			Segs.val[cs]=n_cs_sel;

			Bitu mask=cpu.cpl ? (FMASK_NORMAL | FLAG_NT) : FMASK_ALL;
			if (GETFLAG_IOPL<cpu.cpl) mask &= (~FLAG_IF);
			CPU_SetFlags(n_flags,mask);
			DestroyConditionFlags();

			cpu.cpl=n_cs_rpl;
			reg_eip=n_eip;

			Segs.val[ss]=n_ss;
			Segs.phys[ss]=n_ss_desc.GetBase();
			if (n_ss_desc.Big()) {
				cpu.stack.big=true;
				cpu.stack.mask=0xffffffff;
				cpu.stack.notmask=0;
				reg_esp=n_esp;
			} else {
				cpu.stack.big=false;
				cpu.stack.mask=0xffff;
				cpu.stack.notmask=0xffff0000;
				reg_sp=n_esp & 0xffff;
			}

			// borland extender, zrdx
			cpu_check_segments();

			LOG(LOG_CPU,LOG_NORMAL)("IRET:Outer level:%X:%X big %d",n_cs_sel,n_eip,cpu.code.big);
		}
		return;
	}
}


void CPU_JMP(bool use32,Bitu selector,Bitu offset,Bitu oldeip) {
	if (!cpu.pmode || (reg_flags & FLAG_VM)) {
		if (!use32) {
			reg_eip=offset&0xffff;
		} else {
			reg_eip=offset;
		}
		SegSet16(cs,selector);
		cpu.code.big=false;
		return;
	} else {
		CPU_CHECK_COND((selector & 0xfffc)==0,
			"JMP:CS selector zero",
			EXCEPTION_GP,0)
		Bitu rpl=selector & 3;
		Descriptor desc;
		CPU_CHECK_COND(!cpu.gdt.GetDescriptor(selector,desc),
			"JMP:CS beyond limits",
			EXCEPTION_GP,selector & 0xfffc)
		switch (desc.Type()) {
		case DESC_CODE_N_NC_A:		case DESC_CODE_N_NC_NA:
		case DESC_CODE_R_NC_A:		case DESC_CODE_R_NC_NA:
			CPU_CHECK_COND(rpl>cpu.cpl,
				"JMP:NC:RPL>CPL",
				EXCEPTION_GP,selector & 0xfffc)
			CPU_CHECK_COND(cpu.cpl!=desc.DPL(),
				"JMP:NC:RPL != DPL",
				EXCEPTION_GP,selector & 0xfffc)
			LOG(LOG_CPU,LOG_NORMAL)("JMP:Code:NC to %X:%X big %d",selector,offset,desc.Big());
			goto CODE_jmp;
		case DESC_CODE_N_C_A:		case DESC_CODE_N_C_NA:
		case DESC_CODE_R_C_A:		case DESC_CODE_R_C_NA:
			LOG(LOG_CPU,LOG_NORMAL)("JMP:Code:C to %X:%X big %d",selector,offset,desc.Big());
			CPU_CHECK_COND(cpu.cpl<desc.DPL(),
				"JMP:C:CPL < DPL",
				EXCEPTION_GP,selector & 0xfffc)
CODE_jmp:
			if (!desc.saved.seg.p) {
				// win
				CPU_Exception(EXCEPTION_NP,selector & 0xfffc);
				return;
			}

			/* Normal jump to another selector:offset */
			Segs.phys[cs]=desc.GetBase();
			cpu.code.big=desc.Big()>0;
			Segs.val[cs]=(selector & 0xfffc) | cpu.cpl;
			reg_eip=offset;
			return;
		case DESC_386_TSS_A:
			CPU_CHECK_COND(desc.DPL()<cpu.cpl,
				"JMP:TSS:dpl<cpl",
				EXCEPTION_GP,selector & 0xfffc)
			CPU_CHECK_COND(desc.DPL()<rpl,
				"JMP:TSS:dpl<rpl",
				EXCEPTION_GP,selector & 0xfffc)
			LOG(LOG_CPU,LOG_NORMAL)("JMP:TSS to %X",selector);
			cpu_switch_task(selector,TSwitch_JMP,oldeip);
			break;
		default:
			E_Exit("JMP Illegal descriptor type 0x%x", desc.Type());
		}
	}
}


void CPU_CALL(bool use32,Bitu selector,Bitu offset,Bitu oldeip) {
	if (!cpu.pmode || (reg_flags & FLAG_VM)) {
		if (!use32) {
			CPU_Push16(SegValue(cs));
			CPU_Push16(oldeip);
			reg_eip=offset&0xffff;
		} else {
			CPU_Push32(SegValue(cs));
			CPU_Push32(oldeip);
			reg_eip=offset;
		}
		cpu.code.big=false;
		SegSet16(cs,selector);
		return;
	} else {
		CPU_CHECK_COND((selector & 0xfffc)==0,
			"CALL:CS selector zero",
			EXCEPTION_GP,0)
		Bitu rpl=selector & 3;
		Descriptor call;
		CPU_CHECK_COND(!cpu.gdt.GetDescriptor(selector,call),
			"CALL:CS beyond limits",
			EXCEPTION_GP,selector & 0xfffc)
		/* Check for type of far call */
		switch (call.Type()) {
		case DESC_CODE_N_NC_A:case DESC_CODE_N_NC_NA:
		case DESC_CODE_R_NC_A:case DESC_CODE_R_NC_NA:
			CPU_CHECK_COND(rpl>cpu.cpl,
				"CALL:CODE:NC:RPL>CPL",
				EXCEPTION_GP,selector & 0xfffc)
			CPU_CHECK_COND(call.DPL()!=cpu.cpl,
				"CALL:CODE:NC:DPL!=CPL",
				EXCEPTION_GP,selector & 0xfffc)
			LOG(LOG_CPU,LOG_NORMAL)("CALL:CODE:NC to %X:%X",selector,offset);
			goto call_code;
		case DESC_CODE_N_C_A:case DESC_CODE_N_C_NA:
		case DESC_CODE_R_C_A:case DESC_CODE_R_C_NA:
			CPU_CHECK_COND(call.DPL()>cpu.cpl,
				"CALL:CODE:C:DPL>CPL",
				EXCEPTION_GP,selector & 0xfffc)
			LOG(LOG_CPU,LOG_NORMAL)("CALL:CODE:C to %X:%X",selector,offset);
call_code:
			if (!call.saved.seg.p) {
				// borland extender (RTM)
				CPU_Exception(EXCEPTION_NP,selector & 0xfffc);
				return;
			}
			// commit point
			if (!use32) {
				CPU_Push16(SegValue(cs));
				CPU_Push16(oldeip);
				reg_eip=offset & 0xffff;
			} else {
				CPU_Push32(SegValue(cs));
				CPU_Push32(oldeip);
				reg_eip=offset;
			}
			Segs.phys[cs]=call.GetBase();
			cpu.code.big=call.Big()>0;
			Segs.val[cs]=(selector & 0xfffc) | cpu.cpl;
			return;
		case DESC_386_CALL_GATE:
		case DESC_286_CALL_GATE:
			{
				CPU_CHECK_COND(call.DPL()<cpu.cpl,
					"CALL:Gate:Gate DPL<CPL",
					EXCEPTION_GP,selector & 0xfffc)
				CPU_CHECK_COND(call.DPL()<rpl,
					"CALL:Gate:Gate DPL<RPL",
					EXCEPTION_GP,selector & 0xfffc)
				CPU_CHECK_COND(!call.saved.seg.p,
					"CALL:Gate:Segment not present",
					EXCEPTION_NP,selector & 0xfffc)
				Descriptor n_cs_desc;
				Bitu n_cs_sel=call.GetSelector();

				CPU_CHECK_COND((n_cs_sel & 0xfffc)==0,
					"CALL:Gate:CS selector zero",
					EXCEPTION_GP,0)
				CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_cs_sel,n_cs_desc),
					"CALL:Gate:CS beyond limits",
					EXCEPTION_GP,n_cs_sel & 0xfffc)
				Bitu n_cs_dpl	= n_cs_desc.DPL();
				CPU_CHECK_COND(n_cs_dpl>cpu.cpl,
					"CALL:Gate:CS DPL>CPL",
					EXCEPTION_GP,n_cs_sel & 0xfffc)

				CPU_CHECK_COND(!n_cs_desc.saved.seg.p,
					"CALL:Gate:CS not present",
					EXCEPTION_NP,n_cs_sel & 0xfffc)

				Bitu n_eip		= call.GetOffset();
				switch (n_cs_desc.Type()) {
				case DESC_CODE_N_NC_A:case DESC_CODE_N_NC_NA:
				case DESC_CODE_R_NC_A:case DESC_CODE_R_NC_NA:
					/* Check if we goto inner priviledge */
					if (n_cs_dpl < cpu.cpl) {
						/* Get new SS:ESP out of TSS */
						Bitu n_ss_sel,n_esp;
						Descriptor n_ss_desc;
						cpu_tss.Get_SSx_ESPx(n_cs_dpl,n_ss_sel,n_esp);
						CPU_CHECK_COND((n_ss_sel & 0xfffc)==0,
							"CALL:Gate:NC:SS selector zero",
							EXCEPTION_TS,0)
						CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_ss_sel,n_ss_desc),
							"CALL:Gate:Invalid SS selector",
							EXCEPTION_TS,n_ss_sel & 0xfffc)
						CPU_CHECK_COND(((n_ss_sel & 3)!=n_cs_desc.DPL()) || (n_ss_desc.DPL()!=n_cs_desc.DPL()),
							"CALL:Gate:Invalid SS selector privileges",
							EXCEPTION_TS,n_ss_sel & 0xfffc)

						switch (n_ss_desc.Type()) {
						case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
						case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
							// writable data segment
							break;
						default:
							E_Exit("Call:Gate:SS no writable data segment");	// or #TS(ss_sel)
						}
						CPU_CHECK_COND(!n_ss_desc.saved.seg.p,
							"CALL:Gate:Stack segment not present",
							EXCEPTION_SS,n_ss_sel & 0xfffc)

						/* Load the new SS:ESP and save data on it */
						Bitu o_esp		= reg_esp;
						Bitu o_ss		= SegValue(ss);
						PhysPt o_stack  = SegPhys(ss)+(reg_esp & cpu.stack.mask);


						// catch pagefaults
						if (call.saved.gate.paramcount&31) {
							if (call.Type()==DESC_386_CALL_GATE) {
								for (Bits i=(call.saved.gate.paramcount&31)-1;i>=0;i--)
									mem_readd(o_stack+i*4);
							} else {
								for (Bits i=(call.saved.gate.paramcount&31)-1;i>=0;i--)
									mem_readw(o_stack+i*2);
							}
						}

						// commit point
						Segs.val[ss]=n_ss_sel;
						Segs.phys[ss]=n_ss_desc.GetBase();
						if (n_ss_desc.Big()) {
							cpu.stack.big=true;
							cpu.stack.mask=0xffffffff;
							cpu.stack.notmask=0;
							reg_esp=n_esp;
						} else {
							cpu.stack.big=false;
							cpu.stack.mask=0xffff;
							cpu.stack.notmask=0xffff0000;
							reg_sp=n_esp & 0xffff;
						}

						cpu.cpl = n_cs_desc.DPL();
						uint16_t oldcs    = SegValue(cs);
						/* Switch to new CS:EIP */
						Segs.phys[cs]	= n_cs_desc.GetBase();
						Segs.val[cs]	= (n_cs_sel & 0xfffc) | cpu.cpl;
						cpu.code.big	= n_cs_desc.Big()>0;
						reg_eip			= n_eip;
						if (!use32)	reg_eip&=0xffff;

						if (call.Type()==DESC_386_CALL_GATE) {
							CPU_Push32(o_ss);		//save old stack
							CPU_Push32(o_esp);
							if (call.saved.gate.paramcount&31)
								for (Bits i=(call.saved.gate.paramcount&31)-1;i>=0;i--)
									CPU_Push32(mem_readd(o_stack+i*4));
							CPU_Push32(oldcs);
							CPU_Push32(oldeip);
						} else {
							CPU_Push16(o_ss);		//save old stack
							CPU_Push16(o_esp);
							if (call.saved.gate.paramcount&31)
								for (Bits i=(call.saved.gate.paramcount&31)-1;i>=0;i--)
									CPU_Push16(mem_readw(o_stack+i*2));
							CPU_Push16(oldcs);
							CPU_Push16(oldeip);
						}

						break;
					} else if (n_cs_dpl > cpu.cpl)
						E_Exit("CALL:GATE:CS DPL>CPL");		// or #GP(sel)
					[[fallthrough]];
				case DESC_CODE_N_C_A:case DESC_CODE_N_C_NA:
				case DESC_CODE_R_C_A:case DESC_CODE_R_C_NA:
					// zrdx extender

					if (call.Type()==DESC_386_CALL_GATE) {
						CPU_Push32(SegValue(cs));
						CPU_Push32(oldeip);
					} else {
						CPU_Push16(SegValue(cs));
						CPU_Push16(oldeip);
					}

					/* Switch to new CS:EIP */
					Segs.phys[cs]	= n_cs_desc.GetBase();
					Segs.val[cs]	= (n_cs_sel & 0xfffc) | cpu.cpl;
					cpu.code.big	= n_cs_desc.Big()>0;
					reg_eip			= n_eip;
					if (!use32)	reg_eip&=0xffff;
					break;
				default:
					E_Exit("CALL:GATE:CS no executable segment");
				}
			}			/* Call Gates */
			break;
		case DESC_386_TSS_A:
			CPU_CHECK_COND(call.DPL()<cpu.cpl,
				"CALL:TSS:dpl<cpl",
				EXCEPTION_GP,selector & 0xfffc)
			CPU_CHECK_COND(call.DPL()<rpl,
				"CALL:TSS:dpl<rpl",
				EXCEPTION_GP,selector & 0xfffc)

			CPU_CHECK_COND(!call.saved.seg.p,
				"CALL:TSS:Segment not present",
				EXCEPTION_NP,selector & 0xfffc)

			LOG(LOG_CPU,LOG_NORMAL)("CALL:TSS to %X",selector);
			cpu_switch_task(selector,TSwitch_CALL_INT,oldeip);
			break;
		case DESC_DATA_EU_RW_NA:	// vbdos
		case DESC_INVALID:			// used by some installers
			CPU_Exception(EXCEPTION_GP,selector & 0xfffc);
			return;
		default:
			E_Exit("CALL:Descriptor type 0x%x unsupported", call.Type());
		}
	}
}


void CPU_RET(bool use32,Bitu bytes, [[maybe_unused]] Bitu oldeip) {
	if (!cpu.pmode || (reg_flags & FLAG_VM)) {
		Bitu new_ip,new_cs;
		if (!use32) {
			new_ip=CPU_Pop16();
			new_cs=CPU_Pop16();
		} else {
			new_ip=CPU_Pop32();
			new_cs=CPU_Pop32() & 0xffff;
		}
		reg_esp+=bytes;
		SegSet16(cs,new_cs);
		reg_eip=new_ip;
		cpu.code.big=false;
		return;
	} else {
		Bitu offset,selector;
		if (!use32) selector	= mem_readw(SegPhys(ss) + (reg_esp & cpu.stack.mask) + 2);
		else 		selector	= mem_readd(SegPhys(ss) + (reg_esp & cpu.stack.mask) + 4) & 0xffff;

		Descriptor desc;
		Bitu rpl=selector & 3;
		if(rpl < cpu.cpl) {
			// win setup
			CPU_Exception(EXCEPTION_GP,selector & 0xfffc);
			return;
		}

		CPU_CHECK_COND((selector & 0xfffc)==0,
			"RET:CS selector zero",
			EXCEPTION_GP,0)
		CPU_CHECK_COND(!cpu.gdt.GetDescriptor(selector,desc),
			"RET:CS beyond limits",
			EXCEPTION_GP,selector & 0xfffc)

		if (cpu.cpl==rpl) {
			/* Return to same level */
			switch (desc.Type()) {
			case DESC_CODE_N_NC_A:case DESC_CODE_N_NC_NA:
			case DESC_CODE_R_NC_A:case DESC_CODE_R_NC_NA:
				CPU_CHECK_COND(cpu.cpl!=desc.DPL(),
					"RET to NC segment of other privilege",
					EXCEPTION_GP,selector & 0xfffc)
				goto RET_same_level;
			case DESC_CODE_N_C_A:case DESC_CODE_N_C_NA:
			case DESC_CODE_R_C_A:case DESC_CODE_R_C_NA:
				CPU_CHECK_COND(desc.DPL()>cpu.cpl,
					"RET to C segment of higher privilege",
					EXCEPTION_GP,selector & 0xfffc)
				break;
			default:
				E_Exit("RET from illegal descriptor type 0x%x",
				       desc.Type());
			}
RET_same_level:
			if (!desc.saved.seg.p) {
				// borland extender (RTM)
				CPU_Exception(EXCEPTION_NP,selector & 0xfffc);
				return;
			}

			// commit point
			if (!use32) {
				offset=CPU_Pop16();
				selector=CPU_Pop16();
			} else {
				offset=CPU_Pop32();
				selector=CPU_Pop32() & 0xffff;
			}

			Segs.phys[cs]=desc.GetBase();
			cpu.code.big=desc.Big()>0;
			Segs.val[cs]=selector;
			reg_eip=offset;
			if (cpu.stack.big) {
				reg_esp+=bytes;
			} else {
				reg_sp+=bytes;
			}
			LOG(LOG_CPU,LOG_NORMAL)("RET - Same level to %X:%X RPL %X DPL %X",selector,offset,rpl,desc.DPL());
			return;
		} else {
			/* Return to outer level */
			switch (desc.Type()) {
			case DESC_CODE_N_NC_A:case DESC_CODE_N_NC_NA:
			case DESC_CODE_R_NC_A:case DESC_CODE_R_NC_NA:
				CPU_CHECK_COND(desc.DPL()!=rpl,
					"RET to outer NC segment with DPL!=RPL",
					EXCEPTION_GP,selector & 0xfffc)
				break;
			case DESC_CODE_N_C_A:case DESC_CODE_N_C_NA:
			case DESC_CODE_R_C_A:case DESC_CODE_R_C_NA:
				CPU_CHECK_COND(desc.DPL()>rpl,
					"RET to outer C segment with DPL>RPL",
					EXCEPTION_GP,selector & 0xfffc)
				break;
			default:
				E_Exit("RET from illegal descriptor type 0x%x",
				       desc.Type()); // or #GP(selector)
			}

			CPU_CHECK_COND(!desc.saved.seg.p,
				"RET:Outer level:CS not present",
				EXCEPTION_NP,selector & 0xfffc)

			// commit point
			Bitu n_esp,n_ss;
			if (use32) {
				offset=CPU_Pop32();
				selector=CPU_Pop32() & 0xffff;
				reg_esp+=bytes;
				n_esp = CPU_Pop32();
				n_ss = CPU_Pop32() & 0xffff;
			} else {
				offset=CPU_Pop16();
				selector=CPU_Pop16();
				reg_esp+=bytes;
				n_esp = CPU_Pop16();
				n_ss = CPU_Pop16();
			}

			CPU_CHECK_COND((n_ss & 0xfffc)==0,
				"RET to outer level with SS selector zero",
				EXCEPTION_GP,0)

			Descriptor n_ss_desc;
			CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_ss,n_ss_desc),
				"RET:SS beyond limits",
				EXCEPTION_GP,n_ss & 0xfffc)

			CPU_CHECK_COND(((n_ss & 3)!=rpl) || (n_ss_desc.DPL()!=rpl),
				"RET to outer segment with invalid SS privileges",
				EXCEPTION_GP,n_ss & 0xfffc)
			switch (n_ss_desc.Type()) {
			case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
				break;
			default:
				E_Exit("RET:SS selector type no writable data segment");	// or #GP(selector)
			}
			CPU_CHECK_COND(!n_ss_desc.saved.seg.p,
				"RET:Stack segment not present",
				EXCEPTION_SS,n_ss & 0xfffc)

			cpu.cpl = rpl;
			Segs.phys[cs]=desc.GetBase();
			cpu.code.big=desc.Big()>0;
			Segs.val[cs]=(selector&0xfffc) | cpu.cpl;
			reg_eip=offset;

			Segs.val[ss]=n_ss;
			Segs.phys[ss]=n_ss_desc.GetBase();
			if (n_ss_desc.Big()) {
				cpu.stack.big=true;
				cpu.stack.mask=0xffffffff;
				cpu.stack.notmask=0;
				reg_esp=n_esp+bytes;
			} else {
				cpu.stack.big=false;
				cpu.stack.mask=0xffff;
				cpu.stack.notmask=0xffff0000;
				reg_sp=(n_esp & 0xffff)+bytes;
			}

			cpu_check_segments();

//			LOG(LOG_MISC,LOG_ERROR)("RET - Higher level to %X:%X RPL %X DPL %X",selector,offset,rpl,desc.DPL());
			return;
		}
		LOG(LOG_CPU,LOG_NORMAL)("Prot ret %X:%X",selector,offset);
		return;
	}
}


Bitu CPU_SLDT() {
	return cpu.gdt.SLDT();
}

bool CPU_LLDT(Bitu selector) {
	if (!cpu.gdt.LLDT(selector)) {
		LOG(LOG_CPU,LOG_ERROR)("LLDT failed, selector=%X",selector);
		return true;
	}
	LOG(LOG_CPU,LOG_NORMAL)("LDT Set to %X",selector);
	return false;
}

Bitu CPU_STR() {
	return cpu_tss.selector;
}

bool CPU_LTR(Bitu selector) {
	if ((selector & 0xfffc)==0) {
		cpu_tss.SetSelector(selector);
		return false;
	}
	TSS_Descriptor desc;
	if ((selector & 4) || (!cpu.gdt.GetDescriptor(selector,desc))) {
		LOG(LOG_CPU,LOG_ERROR)("LTR failed, selector=%X",selector);
		return CPU_PrepareException(EXCEPTION_GP,selector);
	}

	if ((desc.Type()==DESC_286_TSS_A) || (desc.Type()==DESC_386_TSS_A)) {
		if (!desc.saved.seg.p) {
			LOG(LOG_CPU,LOG_ERROR)("LTR failed, selector=%X (not present)",selector);
			return CPU_PrepareException(EXCEPTION_NP,selector);
		}
		if (!cpu_tss.SetSelector(selector)) E_Exit("LTR failed, selector=%" sBitfs(X),selector);
		cpu_tss.desc.SetBusy(true);
		cpu_tss.SaveSelector();
	} else {
		/* Descriptor was no available TSS descriptor */
		LOG(LOG_CPU,LOG_NORMAL)("LTR failed, selector=%X (type=%X)",selector,desc.Type());
		return CPU_PrepareException(EXCEPTION_GP,selector);
	}
	return false;
}

void CPU_LGDT(Bitu limit,Bitu base) {
	LOG(LOG_CPU,LOG_NORMAL)("GDT Set to base:%X limit:%X",base,limit);
	cpu.gdt.SetLimit(limit);
	cpu.gdt.SetBase(base);
}

void CPU_LIDT(Bitu limit,Bitu base) {
	LOG(LOG_CPU,LOG_NORMAL)("IDT Set to base:%X limit:%X",base,limit);
	cpu.idt.SetLimit(limit);
	cpu.idt.SetBase(base);
}

Bitu CPU_SGDT_base() {
	return cpu.gdt.GetBase();
}
Bitu CPU_SGDT_limit() {
	return cpu.gdt.GetLimit();
}

Bitu CPU_SIDT_base() {
	return cpu.idt.GetBase();
}
Bitu CPU_SIDT_limit() {
	return cpu.idt.GetLimit();
}

void CPU_SET_CRX(Bitu cr, Bitu value)
{
	switch (cr) {
	case 0: {
		value |= CR0_FPUPRESENT;
		Bitu changed = cpu.cr0 ^ value;

		if (!changed) {
			return;
		}

		cpu.cr0 = value;

		if (value & CR0_PROTECTION) {
			cpu.pmode = true;
			LOG(LOG_CPU, LOG_NORMAL)("Protected mode");
			PAGING_Enable((value & CR0_PAGING) > 0);

			if (!auto_determine_mode.auto_core &&
			    !auto_determine_mode.auto_cycles) {
				break;
			}

			is_protected_mode_program = true;

#if C_DYNAMIC_X86
			if (auto_determine_mode.auto_core) {
				CPU_Core_Dyn_X86_Cache_Init(true);
				cpudecoder = &CPU_Core_Dyn_X86_Run;
			}
#elif C_DYNREC
			if (auto_determine_mode.auto_core) {
				CPU_Core_Dynrec_Cache_Init(true);
				cpudecoder = &CPU_Core_Dynrec_Run;
			}
#endif
			if (legacy_cycles_mode) {
				if (auto_determine_mode.auto_cycles) {
					CPU_CycleAutoAdjust = true;
					CPU_CycleLeft       = 0;
					CPU_Cycles          = 0;
					old_cycle_max       = CPU_CycleMax;

					TITLEBAR_NotifyCyclesChanged();
					maybe_display_max_cycles_warning();

				} else {
					TITLEBAR_RefreshTitle();
				}
			} else {
				// Modern cycles mode
				if (auto_determine_mode.auto_cycles) {
					set_modern_cycles_config(CpuMode::Protected);

					TITLEBAR_NotifyCyclesChanged();
				}
			}

			last_auto_determine_mode = auto_determine_mode;
			auto_determine_mode      = {};

		} else {
			cpu.pmode = false;
			if (value & CR0_PAGING) {
				LOG_MSG("Paging requested without PE=1");
			}

			PAGING_Enable(false);
			LOG(LOG_CPU, LOG_NORMAL)("Real mode");
		}
		break;
	}
	case 2: paging.cr2 = value; break;
	case 3: PAGING_SetDirBase(value); break;
	default:
		LOG(LOG_CPU, LOG_ERROR)("Unhandled MOV CR%d,%X", cr, value);
		break;
	}
}

bool CPU_WRITE_CRX(Bitu cr,Bitu value) {
	/* Check if privileged to access control registers */
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	if ((cr==1) || (cr>4)) return CPU_PrepareException(EXCEPTION_UD,0);
	if (CPU_ArchitectureType<ArchitectureType::Intel486OldSlow) {
		if (cr==4) return CPU_PrepareException(EXCEPTION_UD,0);
	}
	CPU_SET_CRX(cr,value);
	return false;
}

Bitu CPU_GET_CRX(Bitu cr) {
	switch (cr) {
	case 0:
		if (CPU_ArchitectureType>=ArchitectureType::Pentium) return cpu.cr0;
		else if (CPU_ArchitectureType>=ArchitectureType::Intel486OldSlow) return (cpu.cr0 & 0xe005003f);
		else return (cpu.cr0 | 0x7ffffff0);
	case 2:
		return paging.cr2;
	case 3:
		return PAGING_GetDirBase() & 0xfffff000;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled MOV XXX, CR%d",cr);
		break;
	}
	return 0;
}

bool CPU_READ_CRX(Bitu cr,uint32_t & retvalue) {
	/* Check if privileged to access control registers */
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	if ((cr==1) || (cr>4)) return CPU_PrepareException(EXCEPTION_UD,0);
	retvalue=CPU_GET_CRX(cr);
	return false;
}


bool CPU_WRITE_DRX(Bitu dr,Bitu value) {
	/* Check if privileged to access control registers */
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	switch (dr) {
	case 0:
	case 1:
	case 2:
	case 3:
		cpu.drx[dr]=value;
		break;
	case 4:
	case 6:
		cpu.drx[6]=(value|0xffff0ff0) & 0xffffefff;
		break;
	case 5:
	case 7:
		if (CPU_ArchitectureType<ArchitectureType::Pentium) {
			cpu.drx[7]=(value|0x400) & 0xffff2fff;
		} else {
			cpu.drx[7]=(value|0x400);
		}
		break;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled MOV DR%d,%X",dr,value);
		break;
	}
	return false;
}

bool CPU_READ_DRX(Bitu dr,uint32_t & retvalue) {
	/* Check if privileged to access control registers */
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	switch (dr) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 6:
	case 7:
		retvalue=cpu.drx[dr];
		break;
	case 4:
		retvalue=cpu.drx[6];
		break;
	case 5:
		retvalue=cpu.drx[7];
		break;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled MOV XXX, DR%d",dr);
		retvalue=0;
		break;
	}
	return false;
}

bool CPU_WRITE_TRX(Bitu tr,Bitu value) {
	/* Check if privileged to access control registers */
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	switch (tr) {
//	case 3:
	case 6:
	case 7:
		cpu.trx[tr]=value;
		return false;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled MOV TR%d,%X",tr,value);
		break;
	}
	return CPU_PrepareException(EXCEPTION_UD,0);
}

bool CPU_READ_TRX(Bitu tr,uint32_t & retvalue) {
	/* Check if privileged to access control registers */
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	switch (tr) {
//	case 3:
	case 6:
	case 7:
		retvalue=cpu.trx[tr];
		return false;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled MOV XXX, TR%d",tr);
		break;
	}
	return CPU_PrepareException(EXCEPTION_UD,0);
}


Bitu CPU_SMSW() {
	return cpu.cr0;
}

bool CPU_LMSW(Bitu word) {
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	word&=0xf;
	if (cpu.cr0 & 1) word|=1;
	word|=(cpu.cr0&0xfffffff0);
	CPU_SET_CRX(0,word);
	return false;
}

void CPU_ARPL(Bitu & dest_sel,Bitu src_sel) {
	FillFlags();
	if ((dest_sel & 3) < (src_sel & 3)) {
		dest_sel=(dest_sel & 0xfffc) + (src_sel & 3);
//		dest_sel|=0xff3f0000;
		SETFLAGBIT(ZF,true);
	} else {
		SETFLAGBIT(ZF,false);
	}
}

void CPU_LAR(Bitu selector,Bitu & ar) {
	FillFlags();
	if (selector & 0xfffc) {
		Descriptor desc;
		Bitu rpl=selector & 3;
		if (cpu.gdt.GetDescriptor(selector,desc)) {
			switch (desc.Type()) {
				case DESC_LDT:
				case DESC_TASK_GATE:

				case DESC_286_TSS_A:		case DESC_286_TSS_B:
				case DESC_286_CALL_GATE:

				case DESC_386_TSS_A:		case DESC_386_TSS_B:
				case DESC_386_CALL_GATE:

				case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:
				case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
				case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:
				case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
				case DESC_CODE_N_NC_A:		case DESC_CODE_N_NC_NA:
				case DESC_CODE_R_NC_A:		case DESC_CODE_R_NC_NA:
					if (desc.DPL()<cpu.cpl || desc.DPL()<rpl)
						break;
					[[fallthrough]];

				case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
				case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
					/* Valid descriptor */
					ar=desc.saved.fill[1] & 0x00ffff00;
					SETFLAGBIT(ZF,true);
					return;
			}
		}
	}

	SETFLAGBIT(ZF,false);
}

void CPU_LSL(Bitu selector,Bitu & limit) {
	FillFlags();
	if (selector & 0xfffc) {
		Descriptor desc;
		Bitu rpl=selector & 3;
		if (cpu.gdt.GetDescriptor(selector,desc)) {
			switch (desc.Type()) {
				case DESC_LDT:
				case DESC_286_TSS_A:
				case DESC_286_TSS_B:

				case DESC_386_TSS_A:
				case DESC_386_TSS_B:

				case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:
				case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
				case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:
				case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:

				case DESC_CODE_N_NC_A:		case DESC_CODE_N_NC_NA:
				case DESC_CODE_R_NC_A:		case DESC_CODE_R_NC_NA:
					if (desc.DPL()<cpu.cpl || desc.DPL()<rpl)
						break;
					[[fallthrough]];
				case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
				case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
					limit=desc.GetLimit();
					SETFLAGBIT(ZF,true);
					return;
			}
		}
	}

	SETFLAGBIT(ZF,false);
}

void CPU_VERR(Bitu selector) {
	FillFlags();
	if (selector == 0) {
		SETFLAGBIT(ZF,false);
		return;
	}
	Descriptor desc;Bitu rpl=selector & 3;
	if (!cpu.gdt.GetDescriptor(selector,desc)){
		SETFLAGBIT(ZF,false);
		return;
	}
	switch (desc.Type()){
	case DESC_CODE_R_C_A:		case DESC_CODE_R_C_NA:
		//Conforming readable code segments can be always read
		break;
	case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:
	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
	case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:
	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:

	case DESC_CODE_R_NC_A:		case DESC_CODE_R_NC_NA:
		if (desc.DPL()<cpu.cpl || desc.DPL() < rpl) {
			SETFLAGBIT(ZF,false);
			return;
		}
		break;
	default:
		SETFLAGBIT(ZF,false);
		return;
	}
	SETFLAGBIT(ZF,true);
}

void CPU_VERW(Bitu selector) {
	FillFlags();
	if (selector == 0) {
		SETFLAGBIT(ZF,false);
		return;
	}
	Descriptor desc;Bitu rpl=selector & 3;
	if (!cpu.gdt.GetDescriptor(selector,desc)){
		SETFLAGBIT(ZF,false);
		return;
	}
	switch (desc.Type()){
	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
		if (desc.DPL()<cpu.cpl || desc.DPL() < rpl) {
			SETFLAGBIT(ZF,false);
			return;
		}
		break;
	default:
		SETFLAGBIT(ZF,false);
		return;
	}
	SETFLAGBIT(ZF,true);
}

bool CPU_SetSegGeneral(SegNames seg,Bitu value) {
	value &= 0xffff;
	if (!cpu.pmode || (reg_flags & FLAG_VM)) {
		Segs.val[seg]=value;
		Segs.phys[seg]=value << 4;
		if (seg==ss) {
			cpu.stack.big=false;
			cpu.stack.mask=0xffff;
			cpu.stack.notmask=0xffff0000;
		}
		return false;
	} else {
		if (seg==ss) {
			// Stack needs to be non-zero
			if ((value & 0xfffc)==0) {
//				E_Exit("CPU_SetSegGeneral: Stack segment zero");
				return CPU_PrepareException(EXCEPTION_GP,0);
			}
			Descriptor desc;
			if (!cpu.gdt.GetDescriptor(value,desc)) {
//				E_Exit("CPU_SetSegGeneral: Stack segment beyond limits");
				return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);
			}
			if (((value & 3)!=cpu.cpl) || (desc.DPL()!=cpu.cpl)) {
//				E_Exit("CPU_SetSegGeneral: Stack segment with invalid privileges");
				return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);
			}

			switch (desc.Type()) {
			case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
				break;
			default:
				//Earth Siege 1
				return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);
			}

			if (!desc.saved.seg.p) {
//				E_Exit("CPU_SetSegGeneral: Stack segment not present");	// or #SS(sel)
				return CPU_PrepareException(EXCEPTION_SS,value & 0xfffc);
			}

			Segs.val[seg]=value;
			Segs.phys[seg]=desc.GetBase();
			if (desc.Big()) {
				cpu.stack.big=true;
				cpu.stack.mask=0xffffffff;
				cpu.stack.notmask=0;
			} else {
				cpu.stack.big=false;
				cpu.stack.mask=0xffff;
				cpu.stack.notmask=0xffff0000;
			}
		} else {
			if ((value & 0xfffc)==0) {
				Segs.val[seg]=value;
				Segs.phys[seg]=0;	// ??
				return false;
			}
			Descriptor desc;
			if (!cpu.gdt.GetDescriptor(value,desc)) {
				return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);
			}
			switch (desc.Type()) {
			case DESC_DATA_EU_RO_NA:		case DESC_DATA_EU_RO_A:
			case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RO_NA:		case DESC_DATA_ED_RO_A:
			case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
			case DESC_CODE_R_NC_A:			case DESC_CODE_R_NC_NA:
				if (((value & 3)>desc.DPL()) || (cpu.cpl>desc.DPL())) {
					// extreme pinball
					return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);
				}
				break;
			case DESC_CODE_R_C_A:			case DESC_CODE_R_C_NA:
				break;
			default:
				// gabriel knight
				return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);

			}
			if (!desc.saved.seg.p) {
				// win
				return CPU_PrepareException(EXCEPTION_NP,value & 0xfffc);
			}

			Segs.val[seg]=value;
			Segs.phys[seg]=desc.GetBase();
		}

		return false;
	}
}

bool CPU_PopSeg(SegNames seg,bool use32) {
	Bitu val=mem_readw(SegPhys(ss) + (reg_esp & cpu.stack.mask));
	Bitu addsp = use32 ? 0x04 : 0x02;
	//Calcullate this beforehande since the stack mask might change
	uint32_t new_esp  = (reg_esp&cpu.stack.notmask) | ((reg_esp + addsp)&cpu.stack.mask);
	if (CPU_SetSegGeneral(seg,val)) return true;
	reg_esp = new_esp;
	return false;
}

bool CPU_CPUID() {
	constexpr auto FpuEmulationEnabled = true;

	if (CPU_ArchitectureType < ArchitectureType::Intel486NewSlow) {
		return false;
	}
	switch (reg_eax) {
	case 0:	/* Vendor ID String and maximum level? */
		reg_eax=1;  /* Maximum level */
		reg_ebx='G' | ('e' << 8) | ('n' << 16) | ('u'<< 24);
		reg_edx='i' | ('n' << 8) | ('e' << 16) | ('I'<< 24);
		reg_ecx='n' | ('t' << 8) | ('e' << 16) | ('l'<< 24);
		break;
	case 1: // Get processor type/family/model/stepping and feature flags
		if ((CPU_ArchitectureType == ArchitectureType::Intel486NewSlow) ||
		    (CPU_ArchitectureType == ArchitectureType::Mixed)) {

			if (FpuEmulationEnabled) {
				reg_eax = 0x402; // Intel 486DX
				reg_edx = 0x1;   // FPU
			} else {
				reg_eax = 0x422; // Intel 486SX
				reg_edx = 0x0;   // no FPU
			}

			reg_ebx = 0; // Not supported
			reg_ecx = 0; // No features
			             //
		} else if (CPU_ArchitectureType == ArchitectureType::Pentium) {
			if (FpuEmulationEnabled) {
				// Intel Pentium P5 60/66 MHz D1-step
				reg_eax = 0x517;

				// FPU + Time Stamp Counter (RDTSC)
				reg_edx = 0x11;
			} else {
				// All Pentiums had FPU built-in, so when FPU is
				// disabled, we pretend to have early Pentium
				// model with FDIV bug present.

				// Intel Pentium P5 60/66 MHz B1-step
				reg_eax = 0x513;

				// Time Stamp Counter (RDTSC)
				reg_edx = 0x10;
			}

			reg_ebx = 0;     // Not supported
			reg_ecx = 0;     // No features
		} else if (CPU_ArchitectureType == ArchitectureType::PentiumMmx) {
			reg_eax = 0x543;      // Intel Pentium MMX
			reg_ebx = 0;          // Not supported
			reg_ecx = 0;          // No features
			reg_edx = 0x00800011; // FPU + Time Stamp Counter (RDTSC) + MMX
		} else {
			return false;
		}
		break;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled CPUID Function %x",reg_eax);
		reg_eax=0;
		reg_ebx=0;
		reg_ecx=0;
		reg_edx=0;
		break;
	}
	return true;
}

static Bits hlt_decode()
{
	// Once an interrupt occurs, it should change CPU core
	if (reg_eip != cpu.hlt.eip || SegValue(cs) != cpu.hlt.cs) {
		cpudecoder = cpu.hlt.old_decoder;
	} else {
		CPU_IODelayRemoved += CPU_Cycles;
		CPU_Cycles = 0;
	}
	return 0;
}

void CPU_HLT(Bitu oldeip)
{
	reg_eip = oldeip;
	CPU_IODelayRemoved += CPU_Cycles;
	CPU_Cycles          = 0;
	cpu.hlt.cs          = SegValue(cs);
	cpu.hlt.eip         = reg_eip;
	cpu.hlt.old_decoder = cpudecoder;
	cpudecoder          = &hlt_decode;
}

void CPU_ENTER(bool use32,Bitu bytes,Bitu level) {
	level&=0x1f;
	Bitu sp_index=reg_esp&cpu.stack.mask;
	Bitu bp_index=reg_ebp&cpu.stack.mask;
	if (!use32) {
		sp_index-=2;
		mem_writew(SegPhys(ss)+sp_index,reg_bp);
		reg_bp=(uint16_t)(reg_esp-2);
		if (level) {
			for (Bitu i=1;i<level;i++) {
				sp_index-=2;bp_index-=2;
				mem_writew(SegPhys(ss)+sp_index,mem_readw(SegPhys(ss)+bp_index));
			}
			sp_index-=2;
			mem_writew(SegPhys(ss)+sp_index,reg_bp);
		}
	} else {
		sp_index-=4;
        mem_writed(SegPhys(ss)+sp_index,reg_ebp);
		reg_ebp=(reg_esp-4);
		if (level) {
			for (Bitu i=1;i<level;i++) {
				sp_index-=4;bp_index-=4;
				mem_writed(SegPhys(ss)+sp_index,mem_readd(SegPhys(ss)+bp_index));
			}
			sp_index-=4;
			mem_writed(SegPhys(ss)+sp_index,reg_ebp);
		}
	}
	sp_index-=bytes;
	reg_esp=(reg_esp & cpu.stack.notmask) | (sp_index & cpu.stack.mask);
}

// Estimate the CPU speed in MHz given the amount of cycles emulated
static double get_estimated_cpu_mhz(const int cycles)
{
	assert(CPU_ArchitectureType >= ArchitectureType::Pentium);

	// These will result Pentium 100 needing around the same cycles as
	// estimated by the DOSBox X team on their wiki:
	// https://dosbox-x.com/wiki/Guide%3ACPU-settings-in-DOSBox%E2%80%90X#_cycles
	// and assumes Pentium MMX has roughly 16% better IPC, as stated here:
	// https://www.tomshardware.com/reviews/pentium-mmx-live-expectations,19.html

	constexpr double DivisorPentium    = 575.0;
	constexpr double DivisorPentiumMmx = DivisorPentium * 1.16;

	if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) {
		return cycles / DivisorPentium;
	} else {
		return cycles / DivisorPentiumMmx;
	}
}

static double get_estimated_cpu_mhz()
{
	// Although the Pentium was max 200 Mhz, but it was a very rare CPU,
	// 166 Mhz was much more common. Pentium MMX was probably max 266 MHz
	// for the desktop.
	constexpr double DefaultPentiumMhz    = 166.0;
	constexpr double DefaultPentiumMmxMhz = 266.0;

	if (CPU_CycleAutoAdjust) {
		if (CPU_CycleLimit > 0) {
			return get_estimated_cpu_mhz(CPU_CycleLimit);
		} else {
			if (CPU_ArchitectureType < ArchitectureType::PentiumMmx) {
				return DefaultPentiumMhz;
			} else {
				return DefaultPentiumMmxMhz;
			}
		}
	} else {
		return get_estimated_cpu_mhz(CPU_CycleMax);
	}
}

void CPU_ReadTSC()
{
	const auto cpu_mhz = get_estimated_cpu_mhz();

	const auto tsc_precise = PIC_FullIndex() * cpu_mhz * 1000;
	const auto tsc_rounded = static_cast<int64_t>(std::round(tsc_precise));

	reg_edx = static_cast<uint32_t>(tsc_rounded >> 32);
	reg_eax = static_cast<uint32_t>(tsc_rounded & 0xffffffff);
}

static int calc_cycles_increase(const int cycles, const int cycle_up)
{
	auto new_cycles = [&] {
		if (cycle_up < 100) {
			auto percentage_increment = 1 + static_cast<float>(cycle_up) /
			                                        100.0f;
			return iround(cycles * percentage_increment);
		} else {
			return cycles + cpu_cycle_up;
		}
	}();

	if (new_cycles == cycles) {
		++new_cycles;
	}
	return clamp(new_cycles, CpuCyclesMin, CpuCyclesMax);
}

static void cpu_increase_cycles_legacy()
{
	if (CPU_CycleAutoAdjust) {
		CPU_CyclePercUsed += 5;
		if (CPU_CyclePercUsed > 100) {
			CPU_CyclePercUsed = 100;
		}
		LOG_MSG("CPU: max %d percent.", CPU_CyclePercUsed);

	} else {
		CPU_CycleMax = calc_cycles_increase(CPU_CycleMax, cpu_cycle_up);
		CPU_CycleLeft = 0;
		CPU_Cycles    = 0;

		if (!maybe_display_switch_to_dynamic_core_warning(CPU_CycleMax)) {
			LOG_MSG("CPU: Fixed %d cycles", CPU_CycleMax);
		}
	}
}

static void sync_modern_cycles_settings()
{
	const auto cycles_val = [&] {
		auto& conf = modern_cycles_config;

		if (conf.real_mode) {
			return format_str("%d", *conf.real_mode);
		} else {
			return std::string("max");
		}
	}();

	set_section_property_value("cpu", "cpu_cycles", cycles_val);

	const auto cycles_protected_val = [&] {
		auto& conf = modern_cycles_config;

		if (conf.protected_mode_auto) {
			return std::string("auto");
		} else if (conf.protected_mode) {
			return format_str("%d", *conf.protected_mode);
		} else {
			return std::string("max");
		}
	}();

	set_section_property_value("cpu", "cpu_cycles_protected", cycles_protected_val);
}

static void cpu_increase_cycles_modern()
{
	auto& conf = modern_cycles_config;

	if (cpu.pmode) {
		if (conf.protected_mode_auto) {
			// 'cpu_cycles' controls both real and protected mode;
			// nothing to do if we're in 'max' mode.
			if (conf.real_mode) {
				conf.real_mode = calc_cycles_increase(*conf.real_mode,
				                                      cpu_cycle_up);
			}
		} else {
			// Nothing to do if we're in 'max' mode.
			if (conf.protected_mode) {
				conf.protected_mode = calc_cycles_increase(
				        *conf.protected_mode, cpu_cycle_up);
			}
		}
		sync_modern_cycles_settings();
		set_modern_cycles_config(CpuMode::Protected);

	} else {
		// Real mode
		if (conf.real_mode) {
			conf.real_mode = calc_cycles_increase(*conf.real_mode,
			                                      cpu_cycle_up);
			sync_modern_cycles_settings();
			set_modern_cycles_config(CpuMode::Real);
		}
	}
}

static void cpu_increase_cycles(bool pressed)
{
	if (!pressed) {
		return;
	}

	if (legacy_cycles_mode) {
		cpu_increase_cycles_legacy();
	} else {
		cpu_increase_cycles_modern();
	}

	TITLEBAR_NotifyCyclesChanged();
}

static int calc_cycles_decrease(const int cycles, const int cycle_down)
{
	auto new_cycles = [&] {
		if (cycle_down < 100) {
			auto percentage_decrement = 1 + static_cast<float>(cycle_down) /
			                                        100.0f;
			return iround(cycles / percentage_decrement);
		} else {
			return cycles - cpu_cycle_down;
		}
	}();

	if (new_cycles == cycles) {
		--new_cycles;
	}
	return clamp(new_cycles, CpuCyclesMin, CpuCyclesMax);
}

static void cpu_decrease_cycles_legacy()
{
	if (CPU_CycleAutoAdjust) {
		CPU_CyclePercUsed -= 5;

		if (CPU_CyclePercUsed <= 0) {
			CPU_CyclePercUsed = 1;
		}

		if (CPU_CyclePercUsed <= 70) {
			LOG_MSG("CPU: max %d percent cycles. If the game runs too fast, "
			        "try setting a fixed cycles value.",
			        CPU_CyclePercUsed);
		} else {
			LOG_MSG("CPU: max %d percent.", CPU_CyclePercUsed);
		}

	} else {
		CPU_CycleMax = calc_cycles_decrease(CPU_CycleMax, cpu_cycle_down);
		CPU_CycleLeft = 0;
		CPU_Cycles    = 0;

		LOG_MSG("CPU: Fixed %d cycles.", CPU_CycleMax);
	}
}

static void cpu_decrease_cycles_modern()
{
	auto& conf = modern_cycles_config;

	if (cpu.pmode) {
		if (conf.protected_mode_auto) {
			// 'cpu_cycles' controls both real and protected mode;
			// Nothing to do if we're in 'max' mode.
			if (conf.real_mode) {
				conf.real_mode = calc_cycles_decrease(*conf.real_mode,
				                                      cpu_cycle_down);
			}
		} else {
			// Nothing to do if we're in 'max' mode
			if (conf.protected_mode) {
				conf.protected_mode = calc_cycles_decrease(
				        *conf.protected_mode, cpu_cycle_down);
			}
		}
		sync_modern_cycles_settings();
		set_modern_cycles_config(CpuMode::Protected);

	} else {
		// Real mode
		if (conf.real_mode) {
			conf.real_mode = calc_cycles_decrease(*conf.real_mode,
			                                      cpu_cycle_down);
			sync_modern_cycles_settings();
			set_modern_cycles_config(CpuMode::Real);
		}
	}
}

static void cpu_decrease_cycles(bool pressed)
{
	if (!pressed) {
		return;
	}

	if (legacy_cycles_mode) {
		cpu_decrease_cycles_legacy();
	} else {
		cpu_decrease_cycles_modern();
	}

	TITLEBAR_NotifyCyclesChanged();
}

void CPU_ResetAutoAdjust()
{
	CPU_IODelayRemoved = 0;

	DOSBOX_SetTicksDone(0);
	DOSBOX_SetTicksScheduled(0);
}

std::string CPU_GetCyclesConfigAsString()
{
	if (legacy_cycles_mode) {
		std::string s = {};

		if (CPU_CycleAutoAdjust) {
			if (CPU_CycleLimit > 0) {
				s += format_str("max %d%% limit %d",
				                  CPU_CyclePercUsed,
				                  CPU_CycleLimit);
			} else {
				s += format_str("max %d%%", CPU_CyclePercUsed);
			}
		} else {
			s += format_str("%d", CPU_CycleMax);
		}

		s += " ";
		return s += MSG_GetTranslatedRaw("TITLEBAR_CYCLES_MS");

	} else {
		// Modern mode
		auto& conf = modern_cycles_config;

		std::string s = {};
		bool max_mode = false;

		auto format_cycles = [&](const std::optional<int> cycles) {
			if (cycles) {
				s += format_str("%d", *cycles);
			} else {
				s += "max";
				max_mode = true;
			}
		};

		if (is_protected_mode_program) {
			if (conf.protected_mode_auto) {
				// 'cpu_cycles' controls both real and protected
				// mode
				format_cycles(conf.real_mode);
			} else {
				format_cycles(conf.protected_mode);
			}
		} else {
			// Real mode
			format_cycles(conf.real_mode);
		}

		s += " ";
		s += MSG_GetTranslatedRaw("TITLEBAR_CYCLES_MS");

		if (modern_cycles_config.throttle && !max_mode) {
			s += " (";
			s += MSG_GetTranslatedRaw("TITLEBAR_CYCLES_THROTTLED");
			s += ")";
		}
		return s;
	}
}

class Cpu final {
private:
	static bool initialised;

public:
	Cpu(Section* sec)
	{
		if (initialised) {
			Configure(sec);
			return;
		}

		initialised = true;

		reg_eax = 0;
		reg_ebx = 0;
		reg_ecx = 0;
		reg_edx = 0;
		reg_edi = 0;
		reg_esi = 0;
		reg_ebp = 0;
		reg_esp = 0;

		SegSet16(cs, 0);
		SegSet16(ds, 0);
		SegSet16(es, 0);
		SegSet16(fs, 0);
		SegSet16(gs, 0);
		SegSet16(ss, 0);

		// Enable interrupts
		CPU_SetFlags(FLAG_IF, FMASK_ALL);
		cpu.cr0 = 0xffffffff;

		// Initialise
		CPU_SET_CRX(0, 0);

		cpu.code.big      = false;
		cpu.stack.mask    = 0xffff;
		cpu.stack.notmask = 0xffff0000;
		cpu.stack.big     = false;
		cpu.trap_skip     = false;

		cpu.idt.SetBase(0);
		cpu.idt.SetLimit(1023);

		for (Bitu i = 0; i < 7; i++) {
			cpu.drx[i] = 0;
			cpu.trx[i] = 0;
		}

		if (CPU_ArchitectureType >= ArchitectureType::Pentium) {
			cpu.drx[6] = 0xffff0ff0;
		} else {
			cpu.drx[6] = 0xffff1ff0;
		}

		cpu.drx[7] = 0x00000400;

		// Init the CPU cores
		CPU_Core_Normal_Init();
		CPU_Core_Simple_Init();
		CPU_Core_Full_Init();
#if C_DYNAMIC_X86
		CPU_Core_Dyn_X86_Init();
#elif C_DYNREC
		CPU_Core_Dynrec_Init();
#endif
		MAPPER_AddHandler(cpu_decrease_cycles,
		                  SDL_SCANCODE_F11,
		                  PRIMARY_MOD,
		                  "cycledown",
		                  "Dec Cycles");

		MAPPER_AddHandler(cpu_increase_cycles,
		                  SDL_SCANCODE_F12,
		                  PRIMARY_MOD,
		                  "cycleup",
		                  "Inc Cycles");

		Configure(sec);

		// Set up the first CPU core
		CPU_JMP(false, 0, 0, 0);
	}

	~Cpu() = default;

	void ConfigureCyclesModern(SectionProp* secprop)
	{
		modern_cycles_config = {};

		auto clamp_and_sync_cycles =
		        [](const int cycles, const std::string& setting_name) {
			if (cycles < CpuCyclesMin || cycles > CpuCyclesMax) {
				const auto new_cycles = clamp(cycles,
				                              CpuCyclesMin,
				                              CpuCyclesMax);
				LOG_WARNING(
				        "CPU: Invalid '%s' setting: '%d'; "
				        "must be between %d and %d, using '%d'",
				        setting_name.c_str(),
				        cycles,
				        CpuCyclesMin,
				        CpuCyclesMax,
				        new_cycles);

				set_section_property_value("cpu",
				                           setting_name,
				                           format_str("%d", new_cycles));
				return new_cycles;
			} else {
				return cycles;
			}
		};

		// Real mode
		const std::string cpu_cycles_pref = secprop->GetString("cpu_cycles");

		if (cpu_cycles_pref == "max") {
			modern_cycles_config.real_mode = {};

		} else if (const auto maybe_int = parse_int(cpu_cycles_pref)) {
			const auto new_cycles = *maybe_int;
			modern_cycles_config.real_mode =
			        clamp_and_sync_cycles(new_cycles, "cpu_cycles");

		} else {
			modern_cycles_config.real_mode = CpuCyclesRealModeDefault;

			LOG_WARNING("CPU: Invalid 'cpu_cycles' setting: '%s', using '%d'",
			            cpu_cycles_pref.c_str(),
			            *modern_cycles_config.real_mode);

			set_section_property_value(
			        "cpu",
			        "cpu_cycles",
			        format_str("%d", *modern_cycles_config.real_mode));
		}

		auto handle_cpu_cycles_protected_auto = [&] {
			// 'cpu_cycles' controls both real and protected mode
			modern_cycles_config.protected_mode_auto = true;
		};

		// Protected mode
		const std::string cpu_cycles_protected_pref = secprop->GetString(
		        "cpu_cycles_protected");

		if (cpu_cycles_protected_pref == "auto") {
			handle_cpu_cycles_protected_auto();

		} else if (cpu_cycles_protected_pref == "max") {
			modern_cycles_config.protected_mode = {};

		} else if (const auto maybe_int = parse_int(cpu_cycles_protected_pref)) {
			if (cpu_cycles_pref == "max") {
				LOG_WARNING(
				        "CPU: Invalid 'cpu_cycles_protected' setting: '%s'; "
				        "fixed values are not allowed if 'cpu_cycles' is "
				        "'max', using 'auto'",
				        cpu_cycles_protected_pref.c_str());

				set_section_property_value("cpu",
				                           "cpu_cycles_protected",
				                           "auto");

				handle_cpu_cycles_protected_auto();

			} else {
				// Different fixed cycles values for real and
				// protected mode
				const auto new_cycles = *maybe_int;

				modern_cycles_config.protected_mode = clamp_and_sync_cycles(
				        new_cycles, "cpu_cycles_protected");
			}
		} else {
			if (cpu_cycles_pref == "max") {
				LOG_WARNING(
				        "CPU: Invalid 'cpu_cycles_protected' setting: '%s', "
				        "using 'auto'",
				        cpu_cycles_protected_pref.c_str());

				set_section_property_value("cpu",
				                           "cpu_cycles_protected",
				                           "auto");

				handle_cpu_cycles_protected_auto();

			} else {
				modern_cycles_config.protected_mode = CpuCyclesProtectedModeDefault;

				// fixed cycles
				LOG_WARNING(
				        "CPU: Invalid 'cpu_cycles_protected' setting: '%s', "
				        "using '%d'",
				        cpu_cycles_protected_pref.c_str(),
				        *modern_cycles_config.protected_mode);

				set_section_property_value(
				        "cpu",
				        "cpu_cycles_protected",
				        format_str("%d", *modern_cycles_config.protected_mode));
			}
		}

		// Throttling
		modern_cycles_config.throttle = secprop->GetBool("cpu_throttle");
	}

	// The legacy 'cycles' config setting accepts all the following value
	// permutations as valid. There are probably more valid permutations as
	// the ordering is somewhat freeform.
	//
	// Error handling is very minimal (almost non-existent), which
	// makes it unclear from the user's point of view whether a new 'cycles'
	// setting was accepted, rejected, or accepted but clamped to valid
	// limits, etc. To confuse matters even more, many weird
	// looking-permutations are accepted as valid because of the "relaxed"
	// parsing logic.
	//
	// All these issues make the 'cycles' setting very non-intuitive to use,
	// hard to document, and hard to troubleshoot. Unfortunately, we might
	// need keep it around in perpetuity as a fallback "legacy mode" option
	// for all the existing configs out there from the last 20 years out
	// there.
	//
	// So these are all valid settings:
	//
	//   12000
	//   fixed 12000
	//
	//   max
	//   max limit 50000
	//   max 90%
	//   max 90% limit 50000
	//
	//   auto limit 50000       (implicit "3000" for real mode & "max 100%")
	//   auto 90%               (implicit "3000" for real mode)
	//   auto 90% limit 50000   (implicit "3000" for real mode]
	//
	//   auto max                   (implicit "3000" for real mode)
	//   auto max limit 50000       (implicit "3000" for real mode)
	//   auto max 90%               (implicit "3000" for real mode)
	//   auto max 90% limit 50000   (implicit "3000" for real mode)
	//
	//   auto 12000                 (implicit "max 100%"
	//   auto 12000 limit 50000     (implicit "max 100%")
	//   auto 12000 90%
	//   auto 12000 90% limit 50000
	//
	//   auto 12000 max
	//   auto 12000 max limit 50000
	//   auto 12000 max 90%
	//   auto 12000 max 90% limit 50000
	//
	void ConfigureCyclesLegacy(SectionProp* secprop)
	{
		// Sets the value if the string in within the min and max values
		auto set_if_in_range = [](const std::string& str,
		                          int& value,
		                          const int min_value = 1,
		                          const int max_value = 0) {
			std::istringstream stream(str);
			int v = 0;
			stream >> v;

			const bool within_min = (v >= min_value);
			const bool within_max = (!max_value || v <= max_value);

			if (within_min && within_max) {
				value = v;
			}
		};

		PropMultiVal* p = secprop->GetMultiVal("cycles");

		const std::string type = p->GetSection()->GetString("type");
		std::string str;
		CommandLine cmd("", p->GetSection()->GetString("parameters"));

		constexpr auto MinPercent = 0;
		constexpr auto MaxPercent = 100;

		bool displayed_dynamic_core_warning = false;

		auto display_dynamic_core_warning_once = [&] {
			if (!displayed_dynamic_core_warning) {
				displayed_dynamic_core_warning =
				        maybe_display_switch_to_dynamic_core_warning(
				                CPU_CycleMax);
			}
		};

		if (type == "max") {
			CPU_CycleMax        = 0;
			CPU_CyclePercUsed   = 100;
			CPU_CycleAutoAdjust = true;
			CPU_CycleLimit      = -1;

			for (unsigned int cmdnum = 1; cmdnum <= cmd.GetCount();
			     ++cmdnum) {
				if (cmd.FindCommand(cmdnum, str)) {
					if (str.back() == '%') {
						str.pop_back();
						set_if_in_range(str,
										CPU_CyclePercUsed,
										MinPercent,
										MaxPercent);

					} else if (str == "limit") {
						++cmdnum;
						if (cmd.FindCommand(cmdnum, str)) {
							set_if_in_range(str, CPU_CycleLimit);
						}
					}
				}
			}
		} else {
			if (type == "auto") {
				auto_determine_mode.auto_cycles = true;

				CPU_CycleMax      = CpuCyclesRealModeDefault;
				old_cycle_max     = CpuCyclesRealModeDefault;
				CPU_CyclePercUsed = 100;

				for (unsigned int cmdnum = 0;
				     cmdnum <= cmd.GetCount();
				     ++cmdnum) {
					if (cmd.FindCommand(cmdnum, str)) {
						if (str.back() == '%') {
							str.pop_back();
							set_if_in_range(str,
							                CPU_CyclePercUsed,
							                MinPercent,
							                MaxPercent);

						} else if (str == "limit") {
							++cmdnum;
							if (cmd.FindCommand(cmdnum, str)) {
								set_if_in_range(str, CPU_CycleLimit);
							}

						} else {
							set_if_in_range(str, CPU_CycleMax);
							set_if_in_range(str, old_cycle_max);
						}
					}
				}
			} else if (type == "fixed") {
				if (cmd.FindCommand(1, str)) {
					set_if_in_range(str, CPU_CycleMax);
					display_dynamic_core_warning_once();
				}
			} else {
				set_if_in_range(type, CPU_CycleMax);
				display_dynamic_core_warning_once();
			}

			CPU_CycleAutoAdjust = false;
		}

		if (CPU_CycleMax <= 0) {
			CPU_CycleMax = CpuCyclesRealModeDefault;
		}
	}

	void ConfigureCpuCore(const std::string& cpu_core)
	{
		cpudecoder = &CPU_Core_Normal_Run;

		if (cpu_core == "normal") {
			cpudecoder = &CPU_Core_Normal_Run;

		} else if (cpu_core == "simple") {
			cpudecoder = &CPU_Core_Simple_Run;

		} else if (cpu_core == "full") {
			cpudecoder = &CPU_Core_Full_Run;

		} else if (cpu_core == "auto") {
			cpudecoder = &CPU_Core_Normal_Run;

#if C_DYNAMIC_X86
			auto_determine_mode.auto_core = true;

		} else if (cpu_core == "dynamic") {
			cpudecoder = &CPU_Core_Dyn_X86_Run;
			CPU_Core_Dyn_X86_SetFPUMode(true);

		} else if (cpu_core == "dynamic_nodhfpu") {
			cpudecoder = &CPU_Core_Dyn_X86_Run;
			CPU_Core_Dyn_X86_SetFPUMode(false);
#elif C_DYNREC
			auto_determine_mode.auto_core = true;

		} else if (cpu_core == "dynamic") {
			cpudecoder = &CPU_Core_Dynrec_Run;
#else
#endif
		}

#if C_DYNAMIC_X86
		CPU_Core_Dyn_X86_Cache_Init((cpu_core == "dynamic") ||
		                            (cpu_core == "dynamic_nodhfpu"));
#elif C_DYNREC
		CPU_Core_Dynrec_Cache_Init(cpu_core == "dynamic");
#endif
	}

	void ConfigureCpuType(const std::string& cpu_core, const std::string& cpu_type)
	{
		if (cpu_type == "auto") {
			CPU_ArchitectureType = ArchitectureType::Mixed;

		} else if (cpu_type == "386_fast") {
			CPU_ArchitectureType = ArchitectureType::Intel386Fast;

		} else if (cpu_type == "386_prefetch") {
			CPU_ArchitectureType = ArchitectureType::Intel386Fast;

			if (cpu_core == "auto") {
				cpudecoder = &CPU_Core_Prefetch_Run;

				CPU_PrefetchQueueSize         = 16;
				auto_determine_mode.auto_core = false;

			} else {
				if (cpu_core != "normal") {
					LOG_WARNING(
					        "CPU: 'core = 386_prefetch' requires the 'normal' "
					        "core, using 'core = normal'");

					set_section_property_value("cpu",
					                           "core",
					                           "normal");
				}

				cpudecoder = &CPU_Core_Prefetch_Run;

				CPU_PrefetchQueueSize = 16;
			}

		} else if (cpu_type == "386") {
			CPU_ArchitectureType = ArchitectureType::Intel386Slow;

		} else if (cpu_type == "486") {
			CPU_ArchitectureType = ArchitectureType::Intel486OldSlow;

		} else if (cpu_type == "486_prefetch") {
			CPU_ArchitectureType = ArchitectureType::Intel486NewSlow;

			if (cpu_core == "auto") {
				cpudecoder = &CPU_Core_Prefetch_Run;

				CPU_PrefetchQueueSize         = 32;
				auto_determine_mode.auto_core = false;

			} else {
				if (cpu_core != "normal") {
					LOG_WARNING(
					        "CPU: 'core = 486_prefetch' requires the 'normal' "
					        "core, using 'core = normal'");

					set_section_property_value("cpu",
					                           "core",
					                           "normal");
				}

				cpudecoder = &CPU_Core_Prefetch_Run;

				CPU_PrefetchQueueSize = 32;
			}

		} else if (cpu_type == "pentium") {
			CPU_ArchitectureType = ArchitectureType::Pentium;

		} else if (cpu_type == "pentium_mmx") {
			CPU_ArchitectureType = ArchitectureType::PentiumMmx;

		} else {
			CPU_ArchitectureType = ArchitectureType::Mixed;
		}

		if (CPU_ArchitectureType >= ArchitectureType::Intel486NewSlow) {
			cpu_extflags_toggle = FLAG_ID | FLAG_AC;

		} else if (CPU_ArchitectureType >= ArchitectureType::Intel486OldSlow) {
			cpu_extflags_toggle = FLAG_AC;

		} else {
			cpu_extflags_toggle = 0;
		}
	}

	bool Configure(Section* sec)
	{
		// TODO needed?
		// CPU_CycleLeft = 0;
		CPU_Cycles          = 0;
		auto_determine_mode = {};

		auto secprop = static_cast<SectionProp*>(sec);

		const std::string cpu_core = secprop->GetString("core");
		const std::string cpu_type = secprop->GetString("cputype");

		ConfigureCpuCore(cpu_core);
		ConfigureCpuType(cpu_core, cpu_type);

		auto cycles_pref = secprop->GetString("cycles");
		trim(cycles_pref);

		if (!cycles_pref.empty()) {
			// "Legacy cycles mode" is enabled by setting 'cycles'
			// to a non-blank value (the default value is a single
			// space character).
			legacy_cycles_mode = true;

			// Clear new CPU settings in "legacy cycles mode" to
			// avoid confusion.
			//
			// The rationale behind this is that setting 'cycles' is
			// our legacy backwards compatible mode; if an existing
			// game config sets 'cycles' explicitly, that will
			// override the new settings.
			//
			set_section_property_value("cpu", "cpu_cycles", "");
			set_section_property_value("cpu", "cpu_cycles_protected", "");
			set_section_property_value("cpu", "cpu_throttle", "false");
		}

		if (legacy_cycles_mode) {
			ConfigureCyclesLegacy(secprop);
		} else {
			ConfigureCyclesModern(secprop);
			set_modern_cycles_config(CpuMode::Real);
		}

		cpu_cycle_up   = secprop->GetInt("cycleup");
		cpu_cycle_down = secprop->GetInt("cycledown");

		should_hlt_on_idle = secprop->GetBool("cpu_idle");

		TITLEBAR_NotifyCyclesChanged();

		return true;
	}
};

// Initialise static members
bool Cpu::initialised = false;

static std::unique_ptr<Cpu> cpu_instance = {};

void CPU_Init()
{
	auto section = get_section("cpu");

	cpu_instance = std::make_unique<Cpu>(section);
}

void CPU_Destroy()
{
#if C_DYNAMIC_X86
	CPU_Core_Dyn_X86_Cache_Close();
#elif C_DYNREC
	CPU_Core_Dynrec_Cache_Close();
#endif

	cpu_instance.reset();
}

static void notify_cpu_setting_updated([[maybe_unused]] SectionProp& section,
                                       [[maybe_unused]] const std::string& prop_name)
{
	CPU_Destroy();
	CPU_Init();
}

void init_cpu_config_settings(SectionProp& secprop)
{
	using enum Property::Changeable::Value;

	auto pstring = secprop.AddString("core", WhenIdle, "auto");
	pstring->SetValues({
		"auto",
#if C_DYNAMIC_X86 || C_DYNREC
		"dynamic",
#endif
		"normal", "simple"
	});

	pstring->SetHelp(
	        "Type of CPU emulation core to use ('auto' by default). Possible values:\n"
	        "\n"
	        "  auto:     'normal' core for real mode programs, 'dynamic' core for protected\n"
	        "            mode programs (default). Most programs will run correctly with this\n"
	        "            setting.\n"
	        "\n"
	        "  normal:   The DOS program is interpreted instruction by instruction. This\n"
	        "            yields the most accurate timings, but puts 3-5 times more load on\n"
	        "            the host CPU compared to the 'dynamic' core. Therefore, it's\n"
	        "            generally only recommended for real mode programs that don't need\n"
	        "            a fast emulated CPU or are timing-sensitive. The 'normal' core is\n"
	        "            also necessary for programs that self-modify their code.\n"
	        "\n"
	        "  simple:   The 'normal' core optimised for old real mode programs; it might\n"
	        "            give you slightly better compatibility with older games. Auto-\n"
	        "            switches to the 'normal' core in protected mode.\n"
	        "\n"
	        "  dynamic:  The instructions of the DOS program are translated to host CPU\n"
	        "            instructions in blocks and are then executed directly. This puts\n"
	        "            3-5 times less load on the host CPU compared to the 'normal' core,\n"
	        "            but the timings might be less accurate. The 'dynamic' core is a\n"
	        "            necessity for demanding DOS programs (e.g., 3D SVGA games).\n"
	        "            Programs that self-modify their code might misbehave or crash on\n"
	        "            the 'dynamic' core; use the 'normal' core for such programs.");

	pstring = secprop.AddString("cputype", Always, "auto");
	pstring->SetValues(
	        {"auto", "386", "386_fast", "386_prefetch", "486", "pentium", "pentium_mmx"});

	pstring->SetHelp(
	        "CPU type to emulate ('auto' by default). You should only change this if the\n"
	        "program doesn't run correctly on 'auto'. Possible values:\n"
	        "\n"
	        "  auto:          The fastest and most compatible setting (default).\n"
	        "                 Technically, this is '386_fast' plus 486 CPUID, 486 CR\n"
	        "                 register behaviour, and extra 486 instructions.\n"
	        "\n"
	        "  386:           386 CPUID and 386 specific page access level calculation.\n"
	        "\n"
	        "  386_fast:      Same as '386' but with loose page privilege checks which is\n"
	        "                 much faster.\n"
	        "\n"
	        "  386_prefetch:  Same as '386_fast' plus accurate CPU prefetch queue emulation.\n"
	        "                 Requires 'core = normal'. This setting is necessary for\n"
	        "                 programs that self-modify their code or employ anti-debugging\n"
	        "                 tricks. Games that require '386_prefetch' include Contra, FIFA\n"
	        "                 International Soccer (1994), Terminator 1, and X-Men: Madness\n"
	        "                 in The Murderworld.\n"
	        "\n"
	        "  486:           486 CPUID, 486+ specific page access level calculation, 486 CR\n"
	        "                 register behaviour, and extra 486 instructions.\n"
	        "\n"
	        "  pentium:       Same as '486' but with Pentium CPUID, Pentium CR register\n"
	        "                 behaviour, and RDTSC instruction support. Recommended for\n"
	        "                 Windows 3.x games (e.g., Betrayal in Antara).\n"
	        "\n"
	        "  pentium_mmx:   Same as 'pentium' plus MMX instruction set support. Very few\n"
	        "                 games use MMX instructions; it's mostly only useful for\n"
	        "                 demoscene productions.");

	pstring->SetDeprecatedWithAlternateValue("386_slow", "386");
	pstring->SetDeprecatedWithAlternateValue("486_slow", "486");
	pstring->SetDeprecatedWithAlternateValue("486_prefetch", "486");
	pstring->SetDeprecatedWithAlternateValue("pentium_slow", "pentium");

	// Legacy `cycles` setting
	auto pmulti_remain = secprop.AddMultiValRemain("cycles",
	                                               DeprecatedButAllowed,
	                                               " ");
	pmulti_remain->SetHelp(
	        "The [color=light-green]'cycles'[reset] setting is deprecated but still accepted;\n"
	        "please use [color=light-green]'cpu_cycles'[reset], "
	        "[color=light-green]'cpu_cycles_protected'[reset] and "
	        "[color=light-green]'cpu_throttle'[reset] instead.");

	pstring = pmulti_remain->GetSection()->AddString("type", Always, "auto");
	pmulti_remain->SetValue(" ");
	pstring->SetValues({"auto", "fixed", "max", "%u"});

	pmulti_remain->GetSection()->AddString("parameters", Always, "");

	// Revised CPU cycles related settings
	const auto cpu_cycles_default = format_str("%d", CpuCyclesRealModeDefault);

	pstring = secprop.AddString("cpu_cycles", Always, cpu_cycles_default.c_str());
	pstring->SetHelp(format_str(
	        "Speed of the emulated CPU ('%d' by default). If 'cpu_cycles_protected' is on\n"
	        "'auto', this sets the cycles for both real and protected mode programs.\n"
	        "Possible values:\n"
	        "\n"
	        "  <number>:  Emulate a fixed number of cycles per millisecond (roughly\n"
	        "             equivalent to MIPS). Valid range is from %d to %d.\n"
	        "\n"
	        "  max:       Emulate as many cycles as your host CPU can handle on a single\n"
	        "             core. The number of cycles per millisecond can vary; this might\n"
	        "             cause issues in some DOS programs.\n"
	        "Notes:\n"
	        "  - Setting the CPU speed to 'max' or to high fixed values may result in sound\n"
	        "    drop-outs and general lagginess.\n"
	        "\n"
	        "  - Set the lowest fixed cycles value that runs the game at an acceptable speed\n"
	        "    for the best results.\n"
	        "\n"
	        "  - Ballpark cycles values for common CPUs. DOSBox does not do cycle-accurate\n"
	        "    CPU emulation, so treat these as starting points, then fine-tune per game.\n"
	        "\n"
	        "      8088 (4.77 MHz)     300\n"
	        "      286-8               700\n"
	        "      286-12             1500\n"
	        "      386SX-20           3000\n"
	        "      386DX-33           6000\n"
	        "      386DX-40           8000\n"
	        "      486DX-33          12000\n"
	        "      486DX/2-66        25000\n"
	        "      Pentium 90        50000\n"
	        "      Pentium MMX-166  100000\n"
	        "      Pentium II 300   200000",
	        CpuCyclesRealModeDefault,
	        CpuCyclesMin,
	        CpuCyclesMax));

	const auto cpu_cycles_protected_default =
	        format_str("%d", CpuCyclesProtectedModeDefault);

	pstring = secprop.AddString("cpu_cycles_protected",
	                             Always,
	                             cpu_cycles_protected_default.c_str());
	pstring->SetHelp(format_str(
	        "Speed of the emulated CPU for protected mode programs only ('%d' by\n"
	        "default). Possible values:\n"
	        "\n"
	        "  auto:      Use the `cpu_cycles' setting.\n"
	        "\n"
	        "  <number>:  Emulate a fixed number of cycles per millisecond (roughly\n"
	        "             equivalent to MIPS). Valid range is from %d to %d.\n"
	        "\n"
	        "  max:       Emulate as many cycles as your host CPU can handle on a single\n"
	        "             core. The number of cycles per millisecond can vary; this might\n"
	        "             cause issues in some DOS programs.\n"
	        "\n"
	        "Note: See 'cpu_cycles' setting for further info.",
	        CpuCyclesProtectedModeDefault,
	        CpuCyclesMin,
	        CpuCyclesMax));

	auto pbool = secprop.AddBool("cpu_throttle", Always, CpuThrottleDefault);
	pbool->SetHelp(
	        format_str("Throttle down the number of emulated CPU cycles dynamically if your host CPU\n"
	                   "cannot keep up (%s by default). Only affects fixed cycles settings. When\n"
	                   "enabled, the number of cycles per millisecond can vary; this might cause issues\n"
	                   "in some DOS programs.",
	                   (CpuThrottleDefault ? "'on'" : "'off'")));

	auto pint = secprop.AddInt("cycleup", Always, DefaultCpuCycleUp);
	pint->SetMinMax(CpuCycleStepMin, CpuCycleStepMax);
	pint->SetHelp(
	        format_str("Number of cycles to add with the 'Inc Cycles' hotkey (%d by default).\n"
	                   "Values lower than 100 are treated as a percentage increase.",
	                   DefaultCpuCycleUp));

	pint = secprop.AddInt("cycledown", Always, DefaultCpuCycleDown);
	pint->SetMinMax(CpuCycleStepMin, CpuCycleStepMax);
	pint->SetHelp(
	        format_str("Number of cycles to subtract with the 'Dec Cycles' hotkey (%d by default).\n"
	                   "Values lower than 100 are treated as a percentage decrease.",
	                   DefaultCpuCycleDown));

	pbool = secprop.AddBool("cpu_idle", Always, true);
	pbool->SetHelp(
	        "Reduce the CPU usage in the DOS shell and in some applications when DOSBox is\n"
	        "idle ('on' by default). This is done by emulating the HLT CPU instruction, so\n"
	        "it might interfere with other power management tools such as DOSidle and FDAPM\n"
	        "when enabled.");
}

bool CPU_ShouldHltOnIdle()
{
	// We should only execute the power-saving HLT if configured AND if the
	// interrupts are enabled
	return should_hlt_on_idle && (reg_flags & FLAG_IF);
}

void CPU_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	auto section = conf->AddSection("cpu");

	section->AddUpdateHandler(notify_cpu_setting_updated);

	init_cpu_config_settings(*section);
}
