// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#if C_DEBUG

#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <list>
#include <sstream>
#include <string>
#include <vector>

#include "debug.h"
#include "cross.h" //snprintf
#include "cpu.h"
#include "video.h"
#include "pic.h"
#include "mapper.h"
#include "cpu.h"
#include "callback.h"
#include "inout.h"
#include "mixer.h"
#include "timer.h"
#include "paging.h"
#include "string_utils.h"
#include "support.h"
#include "shell.h"
#include "programs.h"
#include "debug_inc.h"
#include "../cpu/lazyflags.h"
#include "keyboard.h"
#include "setup.h"
#include "std_filesystem.h"

SDL_Window *GFX_GetSDLWindow(void);

int old_cursor_state;

// Forwards
static void DrawCode(void);
static void DEBUG_RaiseTimerIrq(void);
static void SaveMemory(uint16_t seg, uint32_t ofs1, uint32_t num);
static void SaveMemoryBin(uint16_t seg, uint32_t ofs1, uint32_t num);
static void LogMCBS(void);
static void LogGDT(void);
static void LogLDT(void);
static void LogIDT(void);
static void LogPages(char* selname);
static void LogCPUInfo(void);
static void OutputVecTable(char* filename);
static void DrawVariables(void);

char* AnalyzeInstruction(char* inst, bool saveSelector);
uint32_t GetHexValue(char* str, char*& hex);

#if 0
class DebugPageHandler final : public PageHandler {
public:
	Bitu readb(PhysPt /*addr*/) {
	}
	Bitu readw(PhysPt /*addr*/) {
	}
	Bitu readd(PhysPt /*addr*/) {
	}
	void writeb(PhysPt /*addr*/,Bitu /*val*/) {
	}
	void writew(PhysPt /*addr*/,Bitu /*val*/) {
	}
	void writed(PhysPt /*addr*/,Bitu /*val*/) {
	}
};
#endif


class DEBUG;

DEBUG*	pDebugcom	= nullptr;
bool	exitLoop	= false;


// Heavy Debugging Vars for logging
#if C_HEAVY_DEBUG
static std::ofstream 	cpuLogFile;
static bool		cpuLog			= false;
static int		cpuLogCounter	= 0;
static int		cpuLogType		= 1;	// log detail
static bool zeroProtect = false;
bool	logHeavy	= false;
#endif

static struct  {
	uint32_t eax = 0;
	uint32_t ebx = 0;
	uint32_t ecx = 0;
	uint32_t edx = 0;
	uint32_t esi = 0;
	uint32_t edi = 0;
	uint32_t ebp = 0;
	uint32_t esp = 0;
	uint32_t eip = 0;
} oldregs = {};

static char curSelectorName[3] = { 0,0,0 };

static Segment oldsegs[6] = {};

static auto oldflags  = cpu_regs.flags;
static auto oldcpucpl = cpu.cpl;

DBGBlock dbg = {};
Bitu cycle_count = 0;
static bool debugging = false;

static void SetColor(Bitu test) {
	if (test) {
		if (has_colors()) { wattrset(dbg.win_reg,COLOR_PAIR(PAIR_BYELLOW_BLACK));}
	} else {
		if (has_colors()) { wattrset(dbg.win_reg,0);}
	}
}

#define MAXCMDLEN 254
struct SCodeViewData {
	int cursorPos          = 0;
	uint16_t firstInstSize = 0;
	uint16_t useCS         = 0;
	uint32_t useEIPlast    = 0;
	uint32_t useEIPmid     = 0;
	uint32_t useEIP        = 0;
	uint16_t cursorSeg     = 0;
	uint32_t cursorOfs     = 0;

	bool ovrMode = false;

	char inputStr[MAXCMDLEN + 1]     = {};
	char suspInputStr[MAXCMDLEN + 1] = {};

	int inputPos = 0;
} codeViewData = {};

static uint16_t dataSeg = 0;
static uint32_t dataOfs = 0;

static bool showExtend    = true;
static bool showPrintable = true;

static void ClearInputLine(void) {
	codeViewData.inputStr[0] = 0;
	codeViewData.inputPos = 0;
}

// History stuff
#define MAX_HIST_BUFFER 50
static std::list<std::string> histBuff = {};
static auto histBuffPos = histBuff.end();

/***********/
/* Helpers */
/***********/

uint32_t PhysMakeProt(uint16_t selector, uint32_t offset)
{
	Descriptor desc;
	if (cpu.gdt.GetDescriptor(selector,desc)) return desc.GetBase()+offset;
	return 0;
}

uint32_t GetAddress(uint16_t seg, uint32_t offset)
{
	if (seg==SegValue(cs)) return SegPhys(cs)+offset;
	if (cpu.pmode && !(reg_flags & FLAG_VM)) {
		Descriptor desc;
		if (cpu.gdt.GetDescriptor(seg,desc)) return PhysMakeProt(seg,offset);
	}
	return (seg<<4)+offset;
}

static char empty_sel[] = { ' ',' ',0 };

bool GetDescriptorInfo(char* selname, char* out1, char* out2)
{
	Bitu sel;
	Descriptor desc;

	if (strstr(selname,"cs") || strstr(selname,"CS")) sel = SegValue(cs);
	else if (strstr(selname,"ds") || strstr(selname,"DS")) sel = SegValue(ds);
	else if (strstr(selname,"es") || strstr(selname,"ES")) sel = SegValue(es);
	else if (strstr(selname,"fs") || strstr(selname,"FS")) sel = SegValue(fs);
	else if (strstr(selname,"gs") || strstr(selname,"GS")) sel = SegValue(gs);
	else if (strstr(selname,"ss") || strstr(selname,"SS")) sel = SegValue(ss);
	else {
		sel = GetHexValue(selname,selname);
		if (*selname==0) selname=empty_sel;
	}
	if (cpu.gdt.GetDescriptor(sel,desc)) {
		switch (desc.Type()) {
			case DESC_TASK_GATE:
				sprintf(out1,"%s: s:%08X type:%02X p",selname,desc.GetSelector(),desc.saved.gate.type);
				sprintf(out2,"    TaskGate   dpl : %01X %1X",desc.saved.gate.dpl,desc.saved.gate.p);
				return true;
			case DESC_LDT:
			case DESC_286_TSS_A:
			case DESC_286_TSS_B:
			case DESC_386_TSS_A:
			case DESC_386_TSS_B:
				sprintf(out1,"%s: b:%08X type:%02X pag",selname,desc.GetBase(),desc.saved.seg.type);
				sprintf(out2,"    l:%08X dpl : %01X %1X%1X%1X",desc.GetLimit(),desc.saved.seg.dpl,desc.saved.seg.p,desc.saved.seg.avl,desc.saved.seg.g);
				return true;
			case DESC_286_CALL_GATE:
			case DESC_386_CALL_GATE:
				sprintf(out1,"%s: s:%08X type:%02X p params: %02X",selname,desc.GetSelector(),desc.saved.gate.type,desc.saved.gate.paramcount);
				sprintf(out2,"    o:%08X dpl : %01X %1X",desc.GetOffset(),desc.saved.gate.dpl,desc.saved.gate.p);
				return true;
			case DESC_286_INT_GATE:
			case DESC_286_TRAP_GATE:
			case DESC_386_INT_GATE:
			case DESC_386_TRAP_GATE:
				sprintf(out1,"%s: s:%08X type:%02X p",selname,desc.GetSelector(),desc.saved.gate.type);
				sprintf(out2,"    o:%08X dpl : %01X %1X",desc.GetOffset(),desc.saved.gate.dpl,desc.saved.gate.p);
				return true;
		}
		sprintf(out1,"%s: b:%08X type:%02X parbg",selname,desc.GetBase(),desc.saved.seg.type);
		sprintf(out2,"    l:%08X dpl : %01X %1X%1X%1X%1X%1X",desc.GetLimit(),desc.saved.seg.dpl,desc.saved.seg.p,desc.saved.seg.avl,desc.saved.seg.r,desc.saved.seg.big,desc.saved.seg.g);
		return true;
	} else {
		strcpy(out1,"                                     ");
		strcpy(out2,"                                     ");
	}
	return false;
}

/********************/
/* DebugVar   stuff */
/********************/

class CDebugVar
{
public:
	CDebugVar(const char *vname, PhysPt address) : adr(address)
	{
		safe_strcpy(name, vname);
	}

	char*  GetName (void)                 { return name; }
	PhysPt GetAdr  (void)                 { return adr; }
	void   SetValue(bool has, uint16_t val) { hasvalue = has; value=val; }
	uint16_t GetValue(void)                 { return value; }
	bool   HasValue(void)                 { return hasvalue; }

private:
	const PhysPt adr = 0;
	char name[16]    = {};
	bool hasvalue    = false;
	uint16_t value   = 0;

public:
	static void       InsertVariable(char* name, PhysPt adr);
	static CDebugVar* FindVar       (PhysPt adr);
	static void       DeleteAll     ();
	static bool       SaveVars      (char* name);
	static bool       LoadVars      (char* name);
};

static std::vector<CDebugVar *> varList = {};

/********************/
/* Breakpoint stuff */
/********************/

bool skipFirstInstruction = false;

enum EBreakpoint {
	BKPNT_UNKNOWN,
	BKPNT_PHYSICAL,
	BKPNT_INTERRUPT,
	BKPNT_MEMORY,
	BKPNT_MEMORY_READ,
	BKPNT_MEMORY_PROT,
	BKPNT_MEMORY_LINEAR
};

#define BPINT_ALL 0x100

class CBreakpoint
{
public:

	CBreakpoint(void);
	void					SetAddress		(uint16_t seg, uint32_t off)	{ location = GetAddress(seg,off); type = BKPNT_PHYSICAL; segment = seg; offset = off; }
	void					SetAddress		(PhysPt adr)				{ location = adr; type = BKPNT_PHYSICAL; }
	void					SetInt			(uint8_t _intNr, uint16_t ah, uint16_t al)	{ intNr = _intNr, ahValue = ah; alValue = al; type = BKPNT_INTERRUPT; }
	void					SetOnce			(bool _once)				{ once = _once; }
	void					SetType			(EBreakpoint _type)			{ type = _type; }
	void					SetValue		(uint8_t value)				{ ahValue = value; }
	void					SetOther		(uint8_t other)				{ alValue = other; }

	bool					IsActive		(void)						{ return active; }
	void					Activate		(bool _active);

	EBreakpoint GetType() const noexcept { return type; }
	bool GetOnce() const noexcept { return once; }
	PhysPt GetLocation() const noexcept { return location; }
	uint16_t GetSegment() const noexcept { return segment; }
	uint32_t GetOffset() const noexcept { return offset; }
	uint8_t GetIntNr() const noexcept { return intNr; }
	uint16_t GetValue() const noexcept { return ahValue; }
	uint16_t GetOther() const noexcept { return alValue; }
#if C_HEAVY_DEBUG
	void FlagMemoryAsRead()
	{
		memory_was_read = true;
	}

	void FlagMemoryAsUnread()
	{
		memory_was_read = false;
	}

	bool WasMemoryRead() const
	{
		return memory_was_read;
	}
#endif

	// statics
	static CBreakpoint*		AddBreakpoint		(uint16_t seg, uint32_t off, bool once);
	static CBreakpoint*		AddIntBreakpoint	(uint8_t intNum, uint16_t ah, uint16_t al, bool once);
	static CBreakpoint*		AddMemBreakpoint	(uint16_t seg, uint32_t off);
	static void				DeactivateBreakpoints();
	static void				ActivateBreakpoints	();
	static void				ActivateBreakpointsExceptAt(PhysPt adr);
	static bool				CheckBreakpoint		(PhysPt adr);
	static bool				CheckBreakpoint		(Bitu seg, Bitu off);
	static bool				CheckIntBreakpoint	(PhysPt adr, uint8_t intNr, uint16_t ahValue, uint16_t alValue);
	static CBreakpoint*		FindPhysBreakpoint	(uint16_t seg, uint32_t off, bool once);
	static CBreakpoint*		FindOtherActiveBreakpoint(PhysPt adr, CBreakpoint* skip);
	static bool				IsBreakpoint		(uint16_t seg, uint32_t off);
	static bool				DeleteBreakpoint	(uint16_t seg, uint32_t off);
	static bool				DeleteByIndex		(uint16_t index);
	static void				DeleteAll			(void);
	static void				ShowList			(void);


private:
	EBreakpoint type = {};
	// Physical
	PhysPt location  = 0;
	uint8_t oldData  = 0;
	uint16_t segment = 0;
	uint32_t offset  = 0;
	// Int
	uint8_t intNr    = 0;
	uint16_t ahValue = 0;
	uint16_t alValue = 0;
	// Shared
	bool active = 0;
	bool once   = 0;
#if C_HEAVY_DEBUG
	bool memory_was_read = false;

	friend bool DEBUG_HeavyIsBreakpoint(void);
#	endif
};

CBreakpoint::CBreakpoint(void):
type(BKPNT_UNKNOWN),
location(0),oldData(0xCC),
segment(0),offset(0),intNr(0),ahValue(0),alValue(0),
active(false),once(false){ }

void CBreakpoint::Activate(bool _active)
{
#if !C_HEAVY_DEBUG
	if (GetType() == BKPNT_PHYSICAL) {
		if (_active) {
			// Set 0xCC and save old value
			uint8_t data = mem_readb(location);
			if (data != 0xCC) {
				oldData = data;
				mem_writeb(location,0xCC);
			} else if (!active) {
				// Another activate breakpoint is already here.
				// Find it, and copy its oldData value
				CBreakpoint *bp = FindOtherActiveBreakpoint(location, this);

				if (!bp || bp->oldData == 0xCC) {
					// This might also happen if there is a real 0xCC instruction here
					DEBUG_ShowMsg("DEBUG: Internal error while activating breakpoint.\n");
					oldData = 0xCC;
				} else
					oldData = bp->oldData;
			}
		} else {
			if (mem_readb(location) == 0xCC) {
				if (oldData == 0xCC)
					DEBUG_ShowMsg("DEBUG: Internal error while deactivating breakpoint.\n");

				// Check if we are the last active breakpoint at this location
				bool otherActive = (FindOtherActiveBreakpoint(location, this) != nullptr);

				// If so, remove 0xCC and set old value
				if (!otherActive)
					mem_writeb(location, oldData);
			}
		}
	}
#endif
	active = _active;
}

// Statics
static std::list<CBreakpoint *> BPoints = {};

#if C_HEAVY_DEBUG
template <typename T>
void DEBUG_UpdateMemoryReadBreakpoints(const PhysPt addr)
{
	static_assert(std::is_unsigned_v<T>);
	static_assert(std::is_integral_v<T>);

	for (CBreakpoint* bp : BPoints) {
		if (bp->GetType() == BKPNT_MEMORY_READ) {
			const PhysPt location_begin = bp->GetLocation();
			const PhysPt location_end = location_begin + sizeof(T);
			if ((addr >= location_begin) && (addr < location_end)) {
				DEBUG_ShowMsg("bpmr hit: %04X:%04X, cs:ip = %04X:%04X",
				              bp->GetSegment(),
				              bp->GetOffset(),
				              SegValue(cs),
				              reg_eip);
				bp->FlagMemoryAsRead();
			}
		}
	}
}
// Explicit instantiations
template void DEBUG_UpdateMemoryReadBreakpoints<uint8_t>(const PhysPt addr);
template void DEBUG_UpdateMemoryReadBreakpoints<uint16_t>(const PhysPt addr);
template void DEBUG_UpdateMemoryReadBreakpoints<uint32_t>(const PhysPt addr);
template void DEBUG_UpdateMemoryReadBreakpoints<uint64_t>(const PhysPt addr);
#endif

CBreakpoint* CBreakpoint::AddBreakpoint(uint16_t seg, uint32_t off, bool once)
{
	auto bp = new CBreakpoint();
	bp->SetAddress		(seg,off);
	bp->SetOnce			(once);
	BPoints.push_front	(bp);
	return bp;
}

CBreakpoint* CBreakpoint::AddIntBreakpoint(uint8_t intNum, uint16_t ah, uint16_t al, bool once)
{
	auto bp = new CBreakpoint();
	bp->SetInt			(intNum,ah,al);
	bp->SetOnce			(once);
	BPoints.push_front	(bp);
	return bp;
}

CBreakpoint* CBreakpoint::AddMemBreakpoint(uint16_t seg, uint32_t off)
{
	auto bp = new CBreakpoint();
	bp->SetAddress		(seg,off);
	bp->SetOnce			(false);
	bp->SetType			(BKPNT_MEMORY);
	BPoints.push_front	(bp);
	return bp;
}

void CBreakpoint::ActivateBreakpoints()
{
	// activate all breakpoints
	std::list<CBreakpoint*>::iterator i;
	for (auto &bp : BPoints)
		bp->Activate(true);
}

void CBreakpoint::DeactivateBreakpoints()
{
	// deactivate all breakpoints
	for (auto &bp : BPoints)
		bp->Activate(false);
}

void CBreakpoint::ActivateBreakpointsExceptAt(PhysPt adr)
{
	// activate all breakpoints, except those at adr
	std::list<CBreakpoint*>::iterator i;
	for (auto &bp : BPoints) {
		// Do not activate breakpoints at adr
		if (bp->GetType() == BKPNT_PHYSICAL && bp->GetLocation() == adr)
			continue;
		bp->Activate(true);
	}
}

// Checks if breakpoint is valid and should stop execution
bool CBreakpoint::CheckBreakpoint(Bitu seg, Bitu off)
{
	// Quick exit if there are no breakpoints
	if (BPoints.empty()) return false;

	// Search matching breakpoint
	for (auto i = BPoints.begin(); i != BPoints.end(); ++i) {
		auto bp = (*i);

		if ((bp->GetType() == BKPNT_PHYSICAL) && bp->IsActive() &&
		    (bp->GetLocation() == GetAddress(seg, off))) {
			// Found
			if (bp->GetOnce()) {
				// delete it, if it should only be used once
				BPoints.erase(i);
				bp->Activate(false);
				delete bp;
			} else {
				// Also look for once-only breakpoints at this address
				bp = FindPhysBreakpoint(seg, off, true);
				if (bp) {
					BPoints.remove(bp);
					bp->Activate(false);
					delete bp;
				}
			}
			return true;
		}
#if C_HEAVY_DEBUG
		// Memory breakpoint support
		else if (bp->IsActive()) {
			if ((bp->GetType()==BKPNT_MEMORY) || (bp->GetType()==BKPNT_MEMORY_PROT) || (bp->GetType()==BKPNT_MEMORY_LINEAR)) {
				// Watch Protected Mode Memoryonly in pmode
				if (bp->GetType()==BKPNT_MEMORY_PROT) {
					// Check if pmode is active
					if (!cpu.pmode) return false;
					// Check if descriptor is valid
					Descriptor desc;
					if (!cpu.gdt.GetDescriptor(bp->GetSegment(),desc)) return false;
					if (desc.GetLimit()==0) return false;
				}

				Bitu address; 
				if (bp->GetType()==BKPNT_MEMORY_LINEAR) address = bp->GetOffset();
				else address = GetAddress(bp->GetSegment(),bp->GetOffset());
				uint8_t value=0;
				if (mem_readb_checked(address,&value)) return false;
				if (bp->GetValue() != value) {
					// Yup, memory value changed
					DEBUG_ShowMsg("DEBUG: Memory breakpoint %s: %04X:%04X - %02X -> %02X\n",(bp->GetType()==BKPNT_MEMORY_PROT)?"(Prot)":"",bp->GetSegment(),bp->GetOffset(),bp->GetValue(),value);
					bp->SetValue(value);
					return true;
				}
			} else if (bp->GetType() == BKPNT_MEMORY_READ) {
				if (bp->WasMemoryRead()) {
					// Yup, memory value was read
					DEBUG_ShowMsg("DEBUG: Memory read breakpoint: %04X:%04X\n",
					              bp->GetSegment(),
					              bp->GetOffset());
					bp->FlagMemoryAsUnread();
					return true;
				}
			}
		}
#endif
	}
	return false;
}

bool CBreakpoint::CheckIntBreakpoint([[maybe_unused]] PhysPt adr, uint8_t intNr, uint16_t ahValue, uint16_t alValue)
// Checks if interrupt breakpoint is valid and should stop execution
{
	if (BPoints.empty()) return false;

	// Search matching breakpoint
	for (auto i = BPoints.begin(); i != BPoints.end(); ++i) {
		auto bp = (*i);
		if ((bp->GetType() == BKPNT_INTERRUPT) && bp->IsActive() &&
		    (bp->GetIntNr() == intNr)) {
			if (((bp->GetValue() == BPINT_ALL) ||
			     (bp->GetValue() == ahValue)) &&
			    ((bp->GetOther() == BPINT_ALL) ||
			     (bp->GetOther() == alValue))) {
				// Ignore it once ?
				// Found
				if (bp->GetOnce()) {
					// delete it, if it should only be used once
					BPoints.erase(i);
					bp->Activate(false);
					delete bp;
				}
				return true;
			}
		}
	}
	return false;
}

void CBreakpoint::DeleteAll()
{
	for (auto &bp : BPoints) {
		bp->Activate(false);
		delete bp;
	}
	BPoints.clear();
}

bool CBreakpoint::DeleteByIndex(uint16_t index)
{
	// Request is past the end
	if (index >= BPoints.size())
		return false;

	auto it = BPoints.begin();
	std::advance(it, index);
	auto bp = *it;

	BPoints.erase(it);
	bp->Activate(false);
	delete bp;
	return true;
}

CBreakpoint* CBreakpoint::FindPhysBreakpoint(uint16_t seg, uint32_t off, bool once)
{
	if (BPoints.empty()) return nullptr;
#if !C_HEAVY_DEBUG
	PhysPt adr = GetAddress(seg, off);
#endif
	// Search for matching breakpoint
	for (auto &bp : BPoints) {
#	if C_HEAVY_DEBUG
		// Heavy debugging breakpoints are triggered by matching seg:off
		bool atLocation = bp->GetSegment() == seg && bp->GetOffset() == off;
#else
		// Normal debugging breakpoints are triggered at an address
		bool atLocation = bp->GetLocation() == adr;
#endif

		if (bp->GetType() == BKPNT_PHYSICAL && atLocation && bp->GetOnce() == once)
			return bp;
	}

	return nullptr;
}

CBreakpoint* CBreakpoint::FindOtherActiveBreakpoint(PhysPt adr, CBreakpoint* skip)
{
	for (auto &bp : BPoints)
		if (bp != skip && bp->GetType() == BKPNT_PHYSICAL && bp->GetLocation() == adr && bp->IsActive())
			return bp;
	return nullptr;
}

// is there a permanent breakpoint at address ?
bool CBreakpoint::IsBreakpoint(uint16_t seg, uint32_t off)
{
	return FindPhysBreakpoint(seg, off, false) != nullptr;
}

bool CBreakpoint::DeleteBreakpoint(uint16_t seg, uint32_t off)
{
	CBreakpoint* bp = FindPhysBreakpoint(seg, off, false);
	if (bp) {
		BPoints.remove(bp);
		delete bp;
		return true;
	}

	return false;
}


void CBreakpoint::ShowList(void)
{
	// iterate list
	int nr = 0;
	for (auto &bp : BPoints) {
		if (bp->GetType()==BKPNT_PHYSICAL) {
			DEBUG_ShowMsg("%02X. BP %04X:%04X\n",nr,bp->GetSegment(),bp->GetOffset());
		} else if (bp->GetType()==BKPNT_INTERRUPT) {
			if (bp->GetValue()==BPINT_ALL) DEBUG_ShowMsg("%02X. BPINT %02X\n",nr,bp->GetIntNr());
			else if (bp->GetOther()==BPINT_ALL) DEBUG_ShowMsg("%02X. BPINT %02X AH=%02X\n",nr,bp->GetIntNr(),bp->GetValue());
			else DEBUG_ShowMsg("%02X. BPINT %02X AH=%02X AL=%02X\n",nr,bp->GetIntNr(),bp->GetValue(),bp->GetOther());
		} else if (bp->GetType()==BKPNT_MEMORY) {
			DEBUG_ShowMsg("%02X. BPMEM %04X:%04X (%02X)\n",nr,bp->GetSegment(),bp->GetOffset(),bp->GetValue());
		} else if (bp->GetType() == BKPNT_MEMORY_READ) {
			DEBUG_ShowMsg("%02X. BPMR %04X:%04X\n",
			              nr,
			              bp->GetSegment(),
			              bp->GetOffset());
		} else if (bp->GetType() == BKPNT_MEMORY_PROT) {
			DEBUG_ShowMsg("%02X. BPPM %04X:%08X (%02X)\n",nr,bp->GetSegment(),bp->GetOffset(),bp->GetValue());
		} else if (bp->GetType() == BKPNT_MEMORY_LINEAR) {
			DEBUG_ShowMsg("%02X. BPLM %08X (%02X)\n",nr,bp->GetOffset(),bp->GetValue());
		}
		nr++;
	}
}

bool DEBUG_Breakpoint(void)
{
	/* First get the physical address and check for a set Breakpoint */
	if (!CBreakpoint::CheckBreakpoint(SegValue(cs),reg_eip)) return false;
	// Found. Breakpoint is valid
	// PhysPt where=GetAddress(SegValue(cs),reg_eip); -- "where" is unused
	CBreakpoint::DeactivateBreakpoints();	// Deactivate all breakpoints
	return true;
}

bool DEBUG_IntBreakpoint(uint8_t intNum)
{
	/* First get the physical address and check for a set Breakpoint */
	PhysPt where=GetAddress(SegValue(cs),reg_eip);
	if (!CBreakpoint::CheckIntBreakpoint(where,intNum,reg_ah,reg_al)) return false;
	// Found. Breakpoint is valid
	CBreakpoint::DeactivateBreakpoints();	// Deactivate all breakpoints
	return true;
}

static bool StepOver()
{
	exitLoop = false;
	PhysPt start=GetAddress(SegValue(cs),reg_eip);
	char dline[200];Bitu size;
	size=DasmI386(dline, start, reg_eip, cpu.code.big);

	if (strstr(dline,"call") || strstr(dline,"int") || strstr(dline,"loop") || strstr(dline,"rep")) {
		// Don't add a temporary breakpoint if there's already one here
		if (!CBreakpoint::FindPhysBreakpoint(SegValue(cs), reg_eip+size, true))
			CBreakpoint::AddBreakpoint(SegValue(cs),reg_eip+size, true);
		debugging=false;
		DrawCode();
		return true;
	} 
	return false;
}

bool DEBUG_ExitLoop(void)
{
#if C_HEAVY_DEBUG
	DrawVariables();
#endif

	if (exitLoop) {
		exitLoop = false;
		return true;
	}
	return false;
}

/********************/
/*   Draw windows   */
/********************/

static void DrawData(void) {

	uint8_t ch;
	uint32_t add = dataOfs;
	uint32_t address;
	/* Data win */	
	for (int y=0; y<8; y++) {
		// Address
		if (add<0x10000) mvwprintw (dbg.win_data,y,0,"%04X:%04X     ",dataSeg,add);
		else mvwprintw (dbg.win_data,y,0,"%04X:%08X ",dataSeg,add);
		for (int x=0; x<16; x++) {
			address = GetAddress(dataSeg,add);
			if (mem_readb_checked(address,&ch)) ch=0;
			mvwprintw (dbg.win_data,y,14+3*x,"%02X",ch);
			if (showPrintable) {
				if (ch<32 || !isprint(*reinterpret_cast<unsigned char*>(&ch))) ch='.';
				mvwaddch (dbg.win_data,y,63+x,ch);
			} else {
#if PDCURSES
				mvwaddrawch (dbg.win_data,y,63+x,ch);
#else
				if (ch<32) ch='.';
				mvwaddch (dbg.win_data,y,63+x,ch);
#endif
			}
			add++;
		}
	}
	wrefresh(dbg.win_data);
}

static void DrawRegisters(void) {
	/* Main Registers */
	SetColor(reg_eax!=oldregs.eax);oldregs.eax=reg_eax;mvwprintw (dbg.win_reg,0,4,"%08X",reg_eax);
	SetColor(reg_ebx!=oldregs.ebx);oldregs.ebx=reg_ebx;mvwprintw (dbg.win_reg,1,4,"%08X",reg_ebx);
	SetColor(reg_ecx!=oldregs.ecx);oldregs.ecx=reg_ecx;mvwprintw (dbg.win_reg,2,4,"%08X",reg_ecx);
	SetColor(reg_edx!=oldregs.edx);oldregs.edx=reg_edx;mvwprintw (dbg.win_reg,3,4,"%08X",reg_edx);

	SetColor(reg_esi!=oldregs.esi);oldregs.esi=reg_esi;mvwprintw (dbg.win_reg,0,18,"%08X",reg_esi);
	SetColor(reg_edi!=oldregs.edi);oldregs.edi=reg_edi;mvwprintw (dbg.win_reg,1,18,"%08X",reg_edi);
	SetColor(reg_ebp!=oldregs.ebp);oldregs.ebp=reg_ebp;mvwprintw (dbg.win_reg,2,18,"%08X",reg_ebp);
	SetColor(reg_esp!=oldregs.esp);oldregs.esp=reg_esp;mvwprintw (dbg.win_reg,3,18,"%08X",reg_esp);
	SetColor(reg_eip!=oldregs.eip);oldregs.eip=reg_eip;mvwprintw (dbg.win_reg,1,42,"%08X",reg_eip);

	SetColor(SegValue(ds)!=oldsegs[ds].val);oldsegs[ds].val=SegValue(ds);mvwprintw (dbg.win_reg,0,31,"%04X",SegValue(ds));
	SetColor(SegValue(es)!=oldsegs[es].val);oldsegs[es].val=SegValue(es);mvwprintw (dbg.win_reg,0,41,"%04X",SegValue(es));
	SetColor(SegValue(fs)!=oldsegs[fs].val);oldsegs[fs].val=SegValue(fs);mvwprintw (dbg.win_reg,0,51,"%04X",SegValue(fs));
	SetColor(SegValue(gs)!=oldsegs[gs].val);oldsegs[gs].val=SegValue(gs);mvwprintw (dbg.win_reg,0,61,"%04X",SegValue(gs));
	SetColor(SegValue(ss)!=oldsegs[ss].val);oldsegs[ss].val=SegValue(ss);mvwprintw (dbg.win_reg,0,71,"%04X",SegValue(ss));
	SetColor(SegValue(cs)!=oldsegs[cs].val);oldsegs[cs].val=SegValue(cs);mvwprintw (dbg.win_reg,1,31,"%04X",SegValue(cs));

	/*Individual flags*/
	Bitu changed_flags = reg_flags ^ oldflags;
	oldflags = reg_flags;

	SetColor(changed_flags&FLAG_CF);
	mvwprintw (dbg.win_reg,1,53,"%01X",GETFLAG(CF) ? 1:0);
	SetColor(changed_flags&FLAG_ZF);
	mvwprintw (dbg.win_reg,1,56,"%01X",GETFLAG(ZF) ? 1:0);
	SetColor(changed_flags&FLAG_SF);
	mvwprintw (dbg.win_reg,1,59,"%01X",GETFLAG(SF) ? 1:0);
	SetColor(changed_flags&FLAG_OF);
	mvwprintw (dbg.win_reg,1,62,"%01X",GETFLAG(OF) ? 1:0);
	SetColor(changed_flags&FLAG_AF);
	mvwprintw (dbg.win_reg,1,65,"%01X",GETFLAG(AF) ? 1:0);
	SetColor(changed_flags&FLAG_PF);
	mvwprintw (dbg.win_reg,1,68,"%01X",GETFLAG(PF) ? 1:0);


	SetColor(changed_flags&FLAG_DF);
	mvwprintw (dbg.win_reg,1,71,"%01X",GETFLAG(DF) ? 1:0);
	SetColor(changed_flags&FLAG_IF);
	mvwprintw (dbg.win_reg,1,74,"%01X",GETFLAG(IF) ? 1:0);
	SetColor(changed_flags&FLAG_TF);
	mvwprintw (dbg.win_reg,1,77,"%01X",GETFLAG(TF) ? 1:0);

	SetColor(changed_flags&FLAG_IOPL);
	mvwprintw (dbg.win_reg,2,72,"%01X",GETFLAG(IOPL)>>12);


	SetColor(cpu.cpl ^ oldcpucpl);
	mvwprintw (dbg.win_reg,2,78,"%01" PRIXPTR ,cpu.cpl);
	oldcpucpl=cpu.cpl;

	if (cpu.pmode) {
		if (reg_flags & FLAG_VM) mvwprintw(dbg.win_reg,0,76,"VM86");
		else if (cpu.code.big) mvwprintw(dbg.win_reg,0,76,"Pr32");
		else mvwprintw(dbg.win_reg,0,76,"Pr16");
	} else
		mvwprintw(dbg.win_reg,0,76,"Real");

	// Selector info, if available
	if ((cpu.pmode) && curSelectorName[0]) {
		char out1[200], out2[200];
		GetDescriptorInfo(curSelectorName,out1,out2);
		mvwprintw(dbg.win_reg,2,28,out1);
		mvwprintw(dbg.win_reg,3,28,out2);
	}

	wattrset(dbg.win_reg,0);
	mvwprintw(dbg.win_reg,3,60,"%" PRIuPTR "       ",cycle_count);
	wrefresh(dbg.win_reg);
}

static void DrawCode(void) {
	bool saveSel;
	uint32_t disEIP = codeViewData.useEIP;
	PhysPt start  = GetAddress(codeViewData.useCS,codeViewData.useEIP);
	char dline[200];Bitu size;Bitu c;
	static char line20[21] = "                    ";

	for (int i=0;i<10;i++) {
		saveSel = false;
		if (has_colors()) {
			if ((codeViewData.useCS==SegValue(cs)) && (disEIP == reg_eip)) {
				wattrset(dbg.win_code,COLOR_PAIR(PAIR_GREEN_BLACK));
				if (codeViewData.cursorPos==-1) {
					codeViewData.cursorPos = i; // Set Cursor
				}
				if (i == codeViewData.cursorPos) {
					codeViewData.cursorSeg = SegValue(cs);
					codeViewData.cursorOfs = disEIP;
				}
				saveSel = (i == codeViewData.cursorPos);
			} else if (i == codeViewData.cursorPos) {
				wattrset(dbg.win_code,COLOR_PAIR(PAIR_BLACK_GREY));
				codeViewData.cursorSeg = codeViewData.useCS;
				codeViewData.cursorOfs = disEIP;
				saveSel = true;
			} else if (CBreakpoint::IsBreakpoint(codeViewData.useCS, disEIP)) {
				wattrset(dbg.win_code,COLOR_PAIR(PAIR_GREY_RED));
			} else {
				wattrset(dbg.win_code,0);
			}
		}


		Bitu drawsize=size=DasmI386(dline, start, disEIP, cpu.code.big);
		bool toolarge = false;
		mvwprintw(dbg.win_code,i,0,"%04X:%04X  ",codeViewData.useCS,disEIP);

		if (drawsize>10) { toolarge = true; drawsize = 9; }
		for (c=0;c<drawsize;c++) {
			uint8_t value;
			if (mem_readb_checked(start+c,&value)) value=0;
			wprintw(dbg.win_code,"%02X",value);
		}
		if (toolarge) { waddstr(dbg.win_code,".."); drawsize++; }
		// Spacepad up to 20 characters
		if(drawsize && (drawsize < 11)) {
			line20[20 - drawsize*2] = 0;
			waddstr(dbg.win_code,line20);
			line20[20 - drawsize*2] = ' ';
		} else waddstr(dbg.win_code,line20);

		char empty_res[] = { 0 };
		char* res = empty_res;
		if (showExtend) res = AnalyzeInstruction(dline, saveSel);
		// Spacepad it up to 28 characters
		size_t dline_len = safe_strlen(dline);
		if (dline_len < 28) memset(dline + dline_len, ' ',28 - dline_len);
		dline[28] = 0;
		waddstr(dbg.win_code,dline);
		// Spacepad it up to 20 characters
		size_t res_len = strlen(res);
		if(res_len && (res_len < 21)) {
			waddstr(dbg.win_code,res);
			line20[20-res_len] = 0;
			waddstr(dbg.win_code,line20);
			line20[20-res_len] = ' ';
		} else 	waddstr(dbg.win_code,line20);

		start+=size;
		disEIP+=size;

		if (i==0) codeViewData.firstInstSize = size;
		if (i==4) codeViewData.useEIPmid	 = disEIP;
	}

	codeViewData.useEIPlast = disEIP;

	wattrset(dbg.win_code,0);
	if (!debugging) {
		if (has_colors()) wattrset(dbg.win_code,COLOR_PAIR(PAIR_GREEN_BLACK));
		mvwprintw(dbg.win_code,10,0,"%s","(Running)");
		wclrtoeol(dbg.win_code);
	} else {
		//TODO long lines
		char* dispPtr = codeViewData.inputStr;
		char* curPtr = &codeViewData.inputStr[codeViewData.inputPos];
		mvwprintw(dbg.win_code,10,0,"%c-> %s%c",
			(codeViewData.ovrMode?'O':'I'),dispPtr,(*curPtr?' ':'_'));
		wclrtoeol(dbg.win_code); // not correct in pdcurses if full line
		mvwchgat(dbg.win_code,10,0,3,0,(PAIR_BLACK_GREY),nullptr);
		if (*curPtr) {
			mvwchgat(dbg.win_code,10,(curPtr-dispPtr+4),1,0,(PAIR_BLACK_GREY),nullptr);
 		}
	}

	wattrset(dbg.win_code,0);
	wrefresh(dbg.win_code);
}

static void SetCodeWinStart()
{
	if ((SegValue(cs)==codeViewData.useCS) && (reg_eip>=codeViewData.useEIP) && (reg_eip<=codeViewData.useEIPlast)) {
		// in valid window - scroll ?
		if (reg_eip>=codeViewData.useEIPmid) codeViewData.useEIP += codeViewData.firstInstSize;

	} else {
		// totally out of range.
		codeViewData.useCS	= SegValue(cs);
		codeViewData.useEIP	= reg_eip;
	}
	codeViewData.cursorPos = -1;	// Recalc Cursor position
}

/********************/
/*    User input    */
/********************/

uint32_t GetHexValue(char* str, char*& hex)
{
	uint32_t	value = 0;
	uint32_t regval = 0;
	hex = str;
	while (*hex == ' ') hex++;
	if (strncmp(hex,"EAX",3) == 0) { hex+=3; regval = reg_eax; } else
	if (strncmp(hex,"EBX",3) == 0) { hex+=3; regval = reg_ebx; } else
	if (strncmp(hex,"ECX",3) == 0) { hex+=3; regval = reg_ecx; } else
	if (strncmp(hex,"EDX",3) == 0) { hex+=3; regval = reg_edx; } else
	if (strncmp(hex,"ESI",3) == 0) { hex+=3; regval = reg_esi; } else
	if (strncmp(hex,"EDI",3) == 0) { hex+=3; regval = reg_edi; } else
	if (strncmp(hex,"EBP",3) == 0) { hex+=3; regval = reg_ebp; } else
	if (strncmp(hex,"ESP",3) == 0) { hex+=3; regval = reg_esp; } else
	if (strncmp(hex,"EIP",3) == 0) { hex+=3; regval = reg_eip; } else
	if (strncmp(hex,"AX",2) == 0)  { hex+=2; regval = reg_ax; } else
	if (strncmp(hex,"BX",2) == 0)  { hex+=2; regval = reg_bx; } else
	if (strncmp(hex,"CX",2) == 0)  { hex+=2; regval = reg_cx; } else
	if (strncmp(hex,"DX",2) == 0)  { hex+=2; regval = reg_dx; } else
	if (strncmp(hex,"SI",2) == 0)  { hex+=2; regval = reg_si; } else
	if (strncmp(hex,"DI",2) == 0)  { hex+=2; regval = reg_di; } else
	if (strncmp(hex,"BP",2) == 0)  { hex+=2; regval = reg_bp; } else
	if (strncmp(hex,"SP",2) == 0)  { hex+=2; regval = reg_sp; } else
	if (strncmp(hex,"IP",2) == 0)  { hex+=2; regval = reg_ip; } else
	if (strncmp(hex,"CS",2) == 0)  { hex+=2; regval = SegValue(cs); } else
	if (strncmp(hex,"DS",2) == 0)  { hex+=2; regval = SegValue(ds); } else
	if (strncmp(hex,"ES",2) == 0)  { hex+=2; regval = SegValue(es); } else
	if (strncmp(hex,"FS",2) == 0)  { hex+=2; regval = SegValue(fs); } else
	if (strncmp(hex,"GS",2) == 0)  { hex+=2; regval = SegValue(gs); } else
	if (strncmp(hex,"SS",2) == 0)  { hex+=2; regval = SegValue(ss); }

	while (*hex) {
		if      ((*hex >= '0') && (*hex <= '9')) value = (value<<4) + *hex - '0';
		else if ((*hex >= 'A') && (*hex <= 'F')) value = (value<<4) + *hex - 'A' + 10;
		else {
			if (*hex == '+') {hex++;return regval + value + GetHexValue(hex,hex); } else
			if (*hex == '-') {hex++;return regval + value - GetHexValue(hex,hex); }
			else break; // No valid char
		}
		hex++;
	}
	return regval + value;
}

bool ChangeRegister(char* str)
{
	char* hex = str;
	while (*hex==' ') hex++;
	if (strncmp(hex,"EAX",3) == 0) { hex+=3; reg_eax = GetHexValue(hex,hex); } else
	if (strncmp(hex,"EBX",3) == 0) { hex+=3; reg_ebx = GetHexValue(hex,hex); } else
	if (strncmp(hex,"ECX",3) == 0) { hex+=3; reg_ecx = GetHexValue(hex,hex); } else
	if (strncmp(hex,"EDX",3) == 0) { hex+=3; reg_edx = GetHexValue(hex,hex); } else
	if (strncmp(hex,"ESI",3) == 0) { hex+=3; reg_esi = GetHexValue(hex,hex); } else
	if (strncmp(hex,"EDI",3) == 0) { hex+=3; reg_edi = GetHexValue(hex,hex); } else
	if (strncmp(hex,"EBP",3) == 0) { hex+=3; reg_ebp = GetHexValue(hex,hex); } else
	if (strncmp(hex,"ESP",3) == 0) { hex+=3; reg_esp = GetHexValue(hex,hex); } else
	if (strncmp(hex,"EIP",3) == 0) { hex+=3; reg_eip = GetHexValue(hex,hex); } else
	if (strncmp(hex,"AX",2) == 0)  { hex+=2; reg_ax = (uint16_t)GetHexValue(hex,hex); } else
	if (strncmp(hex,"BX",2) == 0)  { hex+=2; reg_bx = (uint16_t)GetHexValue(hex,hex); } else
	if (strncmp(hex,"CX",2) == 0)  { hex+=2; reg_cx = (uint16_t)GetHexValue(hex,hex); } else
	if (strncmp(hex,"DX",2) == 0)  { hex+=2; reg_dx = (uint16_t)GetHexValue(hex,hex); } else
	if (strncmp(hex,"SI",2) == 0)  { hex+=2; reg_si = (uint16_t)GetHexValue(hex,hex); } else
	if (strncmp(hex,"DI",2) == 0)  { hex+=2; reg_di = (uint16_t)GetHexValue(hex,hex); } else
	if (strncmp(hex,"BP",2) == 0)  { hex+=2; reg_bp = (uint16_t)GetHexValue(hex,hex); } else
	if (strncmp(hex,"SP",2) == 0)  { hex+=2; reg_sp = (uint16_t)GetHexValue(hex,hex); } else
	if (strncmp(hex,"IP",2) == 0)  { hex+=2; reg_ip = (uint16_t)GetHexValue(hex,hex); } else
	if (strncmp(hex,"CS",2) == 0)  { hex+=2; SegSet16(cs,(uint16_t)GetHexValue(hex,hex)); } else
	if (strncmp(hex,"DS",2) == 0)  { hex+=2; SegSet16(ds,(uint16_t)GetHexValue(hex,hex)); } else
	if (strncmp(hex,"ES",2) == 0)  { hex+=2; SegSet16(es,(uint16_t)GetHexValue(hex,hex)); } else
	if (strncmp(hex,"FS",2) == 0)  { hex+=2; SegSet16(fs,(uint16_t)GetHexValue(hex,hex)); } else
	if (strncmp(hex,"GS",2) == 0)  { hex+=2; SegSet16(gs,(uint16_t)GetHexValue(hex,hex)); } else
	if (strncmp(hex,"SS",2) == 0)  { hex+=2; SegSet16(ss,(uint16_t)GetHexValue(hex,hex)); } else
	if (strncmp(hex,"AF",2) == 0)  { hex+=2; SETFLAGBIT(AF,GetHexValue(hex,hex)); } else
	if (strncmp(hex,"CF",2) == 0)  { hex+=2; SETFLAGBIT(CF,GetHexValue(hex,hex)); } else
	if (strncmp(hex,"DF",2) == 0)  { hex+=2; SETFLAGBIT(DF,GetHexValue(hex,hex)); } else
	if (strncmp(hex,"IF",2) == 0)  { hex+=2; SETFLAGBIT(IF,GetHexValue(hex,hex)); } else
	if (strncmp(hex,"OF",2) == 0)  { hex+=2; SETFLAGBIT(OF,GetHexValue(hex,hex)); } else
	if (strncmp(hex,"ZF",2) == 0)  { hex+=2; SETFLAGBIT(ZF,GetHexValue(hex,hex)); } else
	if (strncmp(hex,"PF",2) == 0)  { hex+=2; SETFLAGBIT(PF,GetHexValue(hex,hex)); } else
	if (strncmp(hex,"SF",2) == 0)  { hex+=2; SETFLAGBIT(SF,GetHexValue(hex,hex)); } else
	{ return false; }
	return true;
}

bool ParseCommand(char* str) {
	char* found = str;
	for(char* idx = found;*idx != 0; idx++)
		*idx = ascii_to_upper(*idx);

	found = trim(found);
	std::string s_found(found);
	std::istringstream stream(s_found);
	std::string command;
	stream >> command;
	std::string::size_type next = s_found.find_first_not_of(' ', command.size());
	if (next == std::string::npos) {
		next = command.size();
	}
	(s_found.erase)(0,next);
	found = const_cast<char*>(s_found.c_str());

	if (command == "MEMDUMP") { // Dump memory to file
		uint16_t seg = (uint16_t)GetHexValue(found,found); found++;
		uint32_t ofs = GetHexValue(found,found); found++;
		uint32_t num = GetHexValue(found,found); found++;
		SaveMemory(seg,ofs,num);
		return true;
	}

	if (command == "MEMDUMPBIN") { // Dump memory to file binary
		uint16_t seg = (uint16_t)GetHexValue(found,found); found++;
		uint32_t ofs = GetHexValue(found,found); found++;
		uint32_t num = GetHexValue(found,found); found++;
		SaveMemoryBin(seg,ofs,num);
		return true;
	}

	if (command == "IV") { // Insert variable
		uint16_t seg = (uint16_t)GetHexValue(found,found); found++;
		uint32_t ofs = (uint16_t)GetHexValue(found,found); found++;
		char name[16];
		for (int i=0; i<16; i++) {
			if (found[i] && (found[i]!=' ')) name[i] = found[i];
			else { name[i] = 0; break; }
		}
		name[15] = 0;

		if(!name[0]) return false;
		DEBUG_ShowMsg("DEBUG: Created debug var %s at %04X:%04X\n",name,seg,ofs);
		CDebugVar::InsertVariable(name,GetAddress(seg,ofs));
		return true;
	}

	if (command == "SV") { // Save variables
		char name[13];
		for (int i=0; i<12; i++) {
			if (found[i] && (found[i]!=' ')) name[i] = found[i];
			else { name[i] = 0; break; }
		}
		name[12] = 0;
		if(!name[0]) return false;
		DEBUG_ShowMsg("DEBUG: Variable list save (%s) : %s.\n",name,(CDebugVar::SaveVars(name)?"ok":"failure"));
		return true;
	}

	if (command == "LV") { // load variables
		char name[13];
		for (int i=0; i<12; i++) {
			if (found[i] && (found[i]!=' ')) name[i] = found[i];
			else { name[i] = 0; break; }
		}
		name[12] = 0;
		if(!name[0]) return false;
		DEBUG_ShowMsg("DEBUG: Variable list load (%s) : %s.\n",name,(CDebugVar::LoadVars(name)?"ok":"failure"));
		return true;
	}

	if (command == "ADDLOG") {
		if(found && *found)	DEBUG_ShowMsg("NOTICE: %s\n",found);
		return true;
	}

	if (command == "SR") { // Set register value
		DEBUG_ShowMsg("DEBUG: Set Register %s.\n",(ChangeRegister(found)?"success":"failure"));
		return true;
	}

	if (command == "SM") { // Set memory with following values
		uint16_t seg = (uint16_t)GetHexValue(found,found); found++;
		uint32_t ofs = GetHexValue(found,found); found++;
		uint16_t count = 0;
		while (*found) {
			while (*found==' ') found++;
			if (*found) {
				uint8_t value = (uint8_t)GetHexValue(found,found);
				if(*found) found++;
				mem_writeb_checked(GetAddress(seg,ofs+count),value);
				count++;
			}
		}
		DEBUG_ShowMsg("DEBUG: Memory changed.\n");
		return true;
	}

	if (command == "BP") { // Add new breakpoint
		uint16_t seg = (uint16_t)GetHexValue(found,found);found++; // skip ":"
		uint32_t ofs = GetHexValue(found,found);
		CBreakpoint::AddBreakpoint(seg,ofs,false);
		DEBUG_ShowMsg("DEBUG: Set breakpoint at %04X:%04X\n",seg,ofs);
		return true;
	}

#if C_HEAVY_DEBUG

	if (command == "BPM") { // Add new breakpoint
		uint16_t seg = (uint16_t)GetHexValue(found,found);found++; // skip ":"
		uint32_t ofs = GetHexValue(found,found);
		CBreakpoint::AddMemBreakpoint(seg,ofs);
		DEBUG_ShowMsg("DEBUG: Set memory breakpoint at %04X:%04X\n",seg,ofs);
		return true;
	}

	if (command == "BPMR") { // Add new breakpoint
		uint16_t seg = (uint16_t)GetHexValue(found, found);
		found++; // skip ":"
		uint32_t ofs    = GetHexValue(found, found);
		CBreakpoint* bp = CBreakpoint::AddMemBreakpoint(seg, ofs);
		bp->SetType(BKPNT_MEMORY_READ);
		bp->FlagMemoryAsUnread();
		DEBUG_ShowMsg("DEBUG: Set memory read breakpoint at %04X:%04X\n",
		              seg,
		              ofs);
		return true;
	}

	if (command == "BPPM") { // Add new breakpoint
		uint16_t seg = (uint16_t)GetHexValue(found,found);found++; // skip ":"
		uint32_t ofs = GetHexValue(found,found);
		CBreakpoint* bp = CBreakpoint::AddMemBreakpoint(seg,ofs);
		if (bp)	{
			bp->SetType(BKPNT_MEMORY_PROT);
			DEBUG_ShowMsg("DEBUG: Set prot-mode memory breakpoint at %04X:%08X\n",seg,ofs);
		}
		return true;
	}

	if (command == "BPLM") { // Add new breakpoint
		uint32_t ofs = GetHexValue(found,found);
		CBreakpoint* bp = CBreakpoint::AddMemBreakpoint(0,ofs);
		if (bp) bp->SetType(BKPNT_MEMORY_LINEAR);
		DEBUG_ShowMsg("DEBUG: Set linear memory breakpoint at %08X\n",ofs);
		return true;
	}

#endif

	if (command == "BPINT") { // Add Interrupt Breakpoint
		uint8_t intNr	= (uint8_t)GetHexValue(found,found);
		bool all = !(*found);
		uint8_t valAH = (uint8_t)GetHexValue(found,found);
		if ((valAH==0x00) && (*found=='*' || all)) {
			CBreakpoint::AddIntBreakpoint(intNr,BPINT_ALL,BPINT_ALL,false);
			DEBUG_ShowMsg("DEBUG: Set interrupt breakpoint at INT %02X\n",intNr);
		} else {
			all = !(*found);
			uint8_t valAL = (uint8_t)GetHexValue(found,found);
			if ((valAL==0x00) && (*found=='*' || all)) {
				CBreakpoint::AddIntBreakpoint(intNr,valAH,BPINT_ALL,false);
				DEBUG_ShowMsg("DEBUG: Set interrupt breakpoint at INT %02X AH=%02X\n",intNr,valAH);
			} else {
				CBreakpoint::AddIntBreakpoint(intNr,valAH,valAL,false);
				DEBUG_ShowMsg("DEBUG: Set interrupt breakpoint at INT %02X AH=%02X AL=%02X\n",intNr,valAH,valAL);
			}
		}
		return true;
	}

	if (command == "BPLIST") {
		DEBUG_ShowMsg("Breakpoint list:\n");
		DEBUG_ShowMsg("-------------------------------------------------------------------------\n");
		CBreakpoint::ShowList();
		return true;
	}

	if (command == "BPDEL") { // Delete Breakpoints
		uint8_t bpNr	= (uint8_t)GetHexValue(found,found);
		if ((bpNr==0x00) && (*found=='*')) { // Delete all
			CBreakpoint::DeleteAll();
			DEBUG_ShowMsg("DEBUG: Breakpoints deleted.\n");
		} else {
			// delete single breakpoint
			DEBUG_ShowMsg("DEBUG: Breakpoint deletion %s.\n",(CBreakpoint::DeleteByIndex(bpNr)?"success":"failure"));
		}
		return true;
	}

	if (command == "C") { // Set code overview
		uint16_t codeSeg = (uint16_t)GetHexValue(found,found); found++;
		uint32_t codeOfs = GetHexValue(found,found);
		DEBUG_ShowMsg("DEBUG: Set code overview to %04X:%04X\n",codeSeg,codeOfs);
		codeViewData.useCS	= codeSeg;
		codeViewData.useEIP = codeOfs;
		codeViewData.cursorPos = 0;
		return true;
	}

	if (command == "D") { // Set data overview
		dataSeg = (uint16_t)GetHexValue(found,found); found++;
		dataOfs = GetHexValue(found,found);
		DEBUG_ShowMsg("DEBUG: Set data overview to %04X:%04X\n",dataSeg,dataOfs);
		return true;
	}

#if C_HEAVY_DEBUG

	if (command == "LOG") { // Create Cpu normal log file
		cpuLogType = 1;
		command = "logcode";
	}

	if (command == "LOGS") { // Create Cpu short log file
		cpuLogType = 0;
		command = "logcode";
	}

	if (command == "LOGL") { // Create Cpu long log file
		cpuLogType = 2;
		command = "logcode";
	}

	if (command == "LOGC") { // Create Cpu coverage log file
		cpuLogType = 3;
		command = "logcode";
	}

	if (command == "logcode") { //Shared code between all logs
		DEBUG_ShowMsg("DEBUG: Starting log\n");
		const std_fs::path log_cpu_txt = "LOGCPU.TXT";
		cpuLogFile.open(log_cpu_txt.string());
		if (!cpuLogFile.is_open()) {
			DEBUG_ShowMsg("DEBUG: Logfile couldn't be created.\n");
			return false;
		}
		DEBUG_ShowMsg("DEBUG: Logfile '%s' created.\n",
		              std_fs::absolute(log_cpu_txt).string().c_str());
		//Initialize log object
		cpuLogFile << std::hex << std::noshowbase << std::setfill('0')
		           << std::uppercase;
		cpuLog = true;
		cpuLogCounter = GetHexValue(found,found);

		debugging = false;
		CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip);
		DOSBOX_SetNormalLoop();	
		return true;
	}

#endif

	if (command == "INTT") { //trace int.
		uint8_t intNr = (uint8_t)GetHexValue(found,found);
		DEBUG_ShowMsg("DEBUG: Tracing INT %02X\n",intNr);
		CPU_HW_Interrupt(intNr);
		SetCodeWinStart();
		return true;
	}

	if (command == "INT") { // start int.
		uint8_t intNr = (uint8_t)GetHexValue(found,found);
		DEBUG_ShowMsg("DEBUG: Starting INT %02X\n",intNr);
		CBreakpoint::AddBreakpoint(SegValue(cs),reg_eip, true);
		CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip-1);
		debugging = false;
		DrawCode();
		DOSBOX_SetNormalLoop();
		CPU_HW_Interrupt(intNr);
		return true;
	}

	if (command == "SELINFO") {
		while (found[0] == ' ') found++;
		char out1[200],out2[200];
		GetDescriptorInfo(found,out1,out2);
		DEBUG_ShowMsg("SelectorInfo %s:\n%s\n%s\n",found,out1,out2);
		return true;
	}

	if (command == "DOS") {
		stream >> command;
		if (command == "MCBS") LogMCBS();
		return true;
	}

	if (command == "GDT") {LogGDT(); return true;}

	if (command == "LDT") {LogLDT(); return true;}

	if (command == "IDT") {LogIDT(); return true;}

	if (command == "PAGING") {LogPages(found); return true;}

	if (command == "CPU") {LogCPUInfo(); return true;}

	if (command == "INTVEC") {
		if (found[0] != 0) {
			OutputVecTable(found);
			return true;
		}
	}

	if (command == "INTHAND") {
		if (found[0] != 0) {
			uint8_t intNr = (uint8_t)GetHexValue(found,found);
			DEBUG_ShowMsg("DEBUG: Set code overview to interrupt handler %X\n",intNr);
			codeViewData.useCS	= mem_readw(intNr*4+2);
			codeViewData.useEIP = mem_readw(intNr*4);
			codeViewData.cursorPos = 0;
			return true;
		}
	}

	if(command == "EXTEND") { //Toggle additional data.
		showExtend = !showExtend;
		return true;
	}

	if(command == "TIMERIRQ") { //Start a timer irq
		DEBUG_RaiseTimerIrq(); 
		DEBUG_ShowMsg("Debug: Timer Int started.\n");
		return true;
	}


#if C_HEAVY_DEBUG
	if (command == "HEAVYLOG") { // Create Cpu log file
		logHeavy = !logHeavy;
		DEBUG_ShowMsg("DEBUG: Heavy cpu logging %s.\n",logHeavy?"on":"off");
		return true;
	}

	if (command == "ZEROPROTECT") { //toggle zero protection
		zeroProtect = !zeroProtect;
		DEBUG_ShowMsg("DEBUG: Zero code execution protection %s.\n",zeroProtect?"on":"off");
		return true;
	}

#endif
	if (command == "HELP" || command == "?") {
		DEBUG_ShowMsg("Debugger commands (enter all values in hex or as register):\n");
		DEBUG_ShowMsg("Commands------------------------------------------------\n");
		DEBUG_ShowMsg("BP     [segment]:[offset] - Set breakpoint.\n");
		DEBUG_ShowMsg("BPINT  [intNr] *          - Set interrupt breakpoint.\n");
		DEBUG_ShowMsg("BPINT  [intNr] [ah] *     - Set interrupt breakpoint with ah.\n");
		DEBUG_ShowMsg("BPINT  [intNr] [ah] [al]  - Set interrupt breakpoint with ah and al.\n");
#if C_HEAVY_DEBUG
		DEBUG_ShowMsg("BPM    [segment]:[offset] - Set memory breakpoint (memory change).\n");
		DEBUG_ShowMsg("BPMR   [segment]:[offset] - Set memory breakpoint (memory read).\n");
		DEBUG_ShowMsg("BPPM   [selector]:[offset]- Set pmode-memory breakpoint (memory change).\n");
		DEBUG_ShowMsg("BPLM   [linear address]   - Set linear memory breakpoint (memory change).\n");
#endif
		DEBUG_ShowMsg("BPLIST                    - List breakpoints.\n");
		DEBUG_ShowMsg("BPDEL  [bpNr] / *         - Delete breakpoint nr / all.\n");
		DEBUG_ShowMsg("C / D  [segment]:[offset] - Set code / data view address.\n");
		DEBUG_ShowMsg("DOS MCBS                  - Show Memory Control Block chain.\n");
		DEBUG_ShowMsg("INT [nr] / INTT [nr]      - Execute / Trace into interrupt.\n");
#if C_HEAVY_DEBUG
		DEBUG_ShowMsg("LOG [num]                 - Write cpu log file.\n");
		DEBUG_ShowMsg("LOGS/LOGL/LOGC [num]      - Write short/long/cs:ip-only cpu log file.\n");
		DEBUG_ShowMsg("HEAVYLOG                  - Enable/Disable automatic cpu log when DOSBox exits.\n");
		DEBUG_ShowMsg("ZEROPROTECT               - Enable/Disable zero code execution detection.\n");
#endif
		DEBUG_ShowMsg("SR [reg] [value]          - Set register value.\n");
		DEBUG_ShowMsg("SM [seg]:[off] [val] [.]..- Set memory with following values.\n");

		DEBUG_ShowMsg("IV [seg]:[off] [name]     - Create var name for memory address.\n");
		DEBUG_ShowMsg("SV [filename]             - Save var list in file.\n");
		DEBUG_ShowMsg("LV [filename]             - Load var list from file.\n");

		DEBUG_ShowMsg("ADDLOG [message]          - Add message to the log file.\n");

		DEBUG_ShowMsg("MEMDUMP [seg]:[off] [len] - Write memory to file memdump.txt.\n");
		DEBUG_ShowMsg("MEMDUMPBIN [s]:[o] [len]  - Write memory to file memdump.bin.\n");
		DEBUG_ShowMsg("SELINFO [segName]         - Show selector info.\n");

		DEBUG_ShowMsg("INTVEC [filename]         - Writes interrupt vector table to file.\n");
		DEBUG_ShowMsg("INTHAND [intNum]          - Set code view to interrupt handler.\n");

		DEBUG_ShowMsg("CPU                       - Display CPU status information.\n");
		DEBUG_ShowMsg("GDT                       - Lists descriptors of the GDT.\n");
		DEBUG_ShowMsg("LDT                       - Lists descriptors of the LDT.\n");
		DEBUG_ShowMsg("IDT                       - Lists descriptors of the IDT.\n");
		DEBUG_ShowMsg("PAGING [page]             - Display content of page table.\n");
		DEBUG_ShowMsg("EXTEND                    - Toggle additional info.\n");
		DEBUG_ShowMsg("TIMERIRQ                  - Run the system timer.\n");

		DEBUG_ShowMsg("HELP                      - Help\n");
		DEBUG_ShowMsg("Keys------------------------------------------------\n");
		DEBUG_ShowMsg("F3/F6                     - Previous command in history.\n");
		DEBUG_ShowMsg("F4/F7                     - Next command in history.\n");
		DEBUG_ShowMsg("F5                        - Run.\n");
		DEBUG_ShowMsg("F8                        - Toggle printable characters.\n");
		DEBUG_ShowMsg("F9                        - Set/Remove breakpoint.\n");
		DEBUG_ShowMsg("F10/F11                   - Step over / trace into instruction.\n");
		DEBUG_ShowMsg("ALT + D/E/S/X/B           - Set data view to DS:SI/ES:DI/SS:SP/DS:DX/ES:BX.\n");
		DEBUG_ShowMsg("Escape                    - Clear input line.");
		DEBUG_ShowMsg("Up/Down                   - Move code view cursor.\n");
		DEBUG_ShowMsg("Page Up/Down              - Scroll data view.\n");
		DEBUG_ShowMsg("Home/End                  - Scroll log messages.\n");

		return true;
	}
	return false;
}

char* AnalyzeInstruction(char* inst, bool saveSelector) {
	static char result[256];

	char instu[256];
	char prefix[3];
	uint16_t seg;

	strcpy(instu,inst);
	upcase(instu);

	result[0] = 0;
	char* pos = strchr(instu,'[');
	if (pos) {
		// Segment prefix ?
		if (*(pos-1)==':') {
			char* segpos = pos-3;
			prefix[0] = tolower(*segpos);
			prefix[1] = tolower(*(segpos+1));
			prefix[2] = 0;
			seg = (uint16_t)GetHexValue(segpos,segpos);
		} else {
			if (strstr(pos,"SP") || strstr(pos,"BP")) {
				seg = SegValue(ss);
				strcpy(prefix,"ss");
			} else {
				seg = SegValue(ds);
				strcpy(prefix,"ds");
			}
		}

		pos++;
		uint32_t adr = GetHexValue(pos,pos);
		while (*pos!=']') {
			if (*pos=='+') {
				pos++;
				adr += GetHexValue(pos,pos);
			} else if (*pos=='-') {
				pos++;
				adr -= GetHexValue(pos,pos);
			} else
				pos++;
		}
		uint32_t address = GetAddress(seg,adr);
		if (!(get_tlb_readhandler(address)->flags & PFLAG_INIT)) {
			static char outmask[] = "%s:[%04X]=%02X";

			if (cpu.pmode) outmask[6] = '8';
				switch (DasmLastOperandSize()) {
			        case 8: {
				        uint8_t val = mem_readb<MemOpMode::SkipBreakpoints>(
				                address);
				        outmask[12] = '2';
				        sprintf(result, outmask, prefix, adr, val);
			        } break;
			        case 16: {
				        uint16_t val = mem_readw<MemOpMode::SkipBreakpoints>(
				                address);
				        outmask[12] = '4';
				        sprintf(result, outmask, prefix, adr, val);
			        } break;
			        case 32: {
				        uint32_t val = mem_readd<MemOpMode::SkipBreakpoints>(
				                address);
				        outmask[12] = '8';
				        sprintf(result, outmask, prefix, adr, val);
			        } break;
			        }
		} else {
			sprintf(result,"[illegal]");
		}
		// Variable found ?
		CDebugVar* var = CDebugVar::FindVar(address);
		if (var) {
			// Replace occurrence
			char* pos1 = strchr(inst,'[');
			char* pos2 = strchr(inst,']');
			if (pos1 && pos2) {
				char temp[256];
				strcpy(temp,pos2);				// save end
				pos1++; *pos1 = 0;				// cut after '['
				strcat(inst,var->GetName());	// add var name
				strcat(inst,temp);				// add end
			}
		}
		// show descriptor info, if available
		if ((cpu.pmode) && saveSelector) {
			strcpy(curSelectorName,prefix);
		}
	}
	// If it is a callback add additional info
	pos = strstr(inst,"callback");
	if (pos) {
		pos += 9;
		Bitu nr = GetHexValue(pos,pos);
		const char* descr = CALLBACK_GetDescription(nr);
		if (descr) {
			strcat(inst,"  ("); strcat(inst,descr); strcat(inst,")");
		}
	}
	// Must be a jump
	if (instu[0] == 'J')
	{
		bool jmp = false;
		switch (instu[1]) {
		case 'A' :	{	jmp = (get_CF()?false:true) && (get_ZF()?false:true); // JA
					}	break;
		case 'B' :	{	if (instu[2] == 'E') {
							jmp = (get_CF()?true:false) || (get_ZF()?true:false); // JBE
						} else {
							jmp = get_CF()?true:false; // JB
						}
					}	break;
		case 'C' :	{	if (instu[2] == 'X') {
							jmp = reg_cx == 0; // JCXZ
						} else {
							jmp = get_CF()?true:false; // JC
						}
					}	break;
		case 'E' :	{	jmp = get_ZF()?true:false; // JE
					}	break;
		case 'G' :	{	if (instu[2] == 'E') {
							jmp = (get_SF()?true:false)==(get_OF()?true:false); // JGE
						} else {
							jmp = (get_ZF()?false:true) && ((get_SF()?true:false)==(get_OF()?true:false)); // JG
						}
					}	break;
		case 'L' :	{	if (instu[2] == 'E') {
							jmp = (get_ZF()?true:false) || ((get_SF()?true:false)!=(get_OF()?true:false)); // JLE
						} else {
							jmp = (get_SF()?true:false)!=(get_OF()?true:false); // JL
						}
					}	break;
		case 'M' :	{	jmp = true; // JMP
					}	break;
		case 'N' :	{	switch (instu[2]) {
						case 'B' :	
						case 'C' :	{	jmp = get_CF()?false:true;	// JNB / JNC
									}	break;
						case 'E' :	{	jmp = get_ZF()?false:true;	// JNE
									}	break;
						case 'O' :	{	jmp = get_OF()?false:true;	// JNO
									}	break;
						case 'P' :	{	jmp = get_PF()?false:true;	// JNP
									}	break;
						case 'S' :	{	jmp = get_SF()?false:true;	// JNS
									}	break;
						case 'Z' :	{	jmp = get_ZF()?false:true;	// JNZ
									}	break;
						}
					}	break;
		case 'O' :	{	jmp = get_OF()?true:false; // JO
					}	break;
		case 'P' :	{	if (instu[2] == 'O') {
							jmp = get_PF()?false:true; // JPO
						} else {
							jmp = get_SF()?true:false; // JP / JPE
						}
					}	break;
		case 'S' :	{	jmp = get_SF()?true:false; // JS
					}	break;
		case 'Z' :	{	jmp = get_ZF()?true:false; // JZ
					}	break;
		}
		if (jmp) {
			pos = strchr(instu,'$');
			if (pos) {
				pos = strchr(instu,'+');
				if (pos) {
					strcpy(result,"(down)");
				} else {
					strcpy(result,"(up)");
				}
			}
		} else {
			sprintf(result,"(no jmp)");
		}
	}
	return result;
}


int32_t DEBUG_Run(int32_t amount,bool quickexit) {
	skipFirstInstruction = true;
	CPU_Cycles = amount;
	int32_t ret = (*cpudecoder)();
	if (quickexit) SetCodeWinStart();
	else {
		// ensure all breakpoints are activated
		CBreakpoint::ActivateBreakpoints();

		const auto graphics_window = GFX_GetSDLWindow();
		SDL_RaiseWindow(graphics_window);
		SDL_SetWindowInputFocus(graphics_window);

		DOSBOX_SetNormalLoop();
	}
	return ret;
}



uint32_t DEBUG_CheckKeys(void) {
	Bits ret=0;
	bool numberrun = false;
	bool skipDraw = false;
	int key=getch();

	if (key >='1' && key <='5' && safe_strlen(codeViewData.inputStr) == 0) {
		const int32_t v[] ={5,500,1000,5000,10000};

		ret = DEBUG_Run(v[key - '1'],true);

		/* Setup variables so we end up at the proper ret processing */
		numberrun = true;

		// Don't redraw the screen if it's going to get redrawn immediately
		// afterwards, to avoid resetting oldregs.
		if (ret == static_cast<Bits>(debugCallback))
			skipDraw = true;
		key = -1;
	}

	if (key>0 || numberrun) {
#if defined(WIN32) && PDCURSES
		switch (key) {
		case PADENTER:	key=0x0A;	break;
		case PADSLASH:	key='/';	break;
		case PADSTAR:	key='*';	break;
		case PADMINUS:	key='-';	break;
		case PADPLUS:	key='+';	break;
		case PADSTOP:	key=KEY_DC;		break;
		case PAD0:		key=KEY_IC;		break;
		case KEY_A1:	key=KEY_HOME;	break;
		case KEY_A2:	key=KEY_UP;		break;
		case KEY_A3:	key=KEY_PPAGE;	break;
		case KEY_B1:	key=KEY_LEFT;	break;
		case KEY_B3:	key=KEY_RIGHT;	break;
		case KEY_C1:	key=KEY_END;	break;
		case KEY_C2:	key=KEY_DOWN;	break;
		case KEY_C3:	key=KEY_NPAGE;	break;
		case ALT_D:
			if (ungetch('D') != ERR) key=27;
			break;
		case ALT_E:
			if (ungetch('E') != ERR) key=27;
			break;
		case ALT_X:
			if (ungetch('X') != ERR) key=27;
			break;
		case ALT_B:
			if (ungetch('B') != ERR) key=27;
			break;
		case ALT_S:
			if (ungetch('S') != ERR) key=27;
			break;
		}
#endif
		switch (ascii_to_upper(key)) {
		case 27:	// escape (a bit slow): Clears line. and processes alt commands.
			key=getch();
			if(key < 0) { //Purely escape Clear line
				ClearInputLine();
				break;
			}

			switch(ascii_to_upper(key)) {
			case 'D' : // ALT - D: DS:SI
				dataSeg = SegValue(ds);
				if (cpu.pmode && !(reg_flags & FLAG_VM)) dataOfs = reg_esi;
				else dataOfs = reg_si;
				break;
			case 'E' : //ALT - E: es:di
				dataSeg = SegValue(es);
				if (cpu.pmode && !(reg_flags & FLAG_VM)) dataOfs = reg_edi;
				else dataOfs = reg_di;
				break;
			case 'X': //ALT - X: ds:dx
				dataSeg = SegValue(ds);
				if (cpu.pmode && !(reg_flags & FLAG_VM)) dataOfs = reg_edx;
				else dataOfs = reg_dx;
				break;
			case 'B' : //ALT -B: es:bx
				dataSeg = SegValue(es);
				if (cpu.pmode && !(reg_flags & FLAG_VM)) dataOfs = reg_ebx;
				else dataOfs = reg_bx;
				break;
			case 'S': //ALT - S: ss:sp
				dataSeg = SegValue(ss);
				if (cpu.pmode && !(reg_flags & FLAG_VM)) dataOfs = reg_esp;
				else dataOfs = reg_sp;
				break;
			default:
				break;
			}
			break;
		case KEY_PPAGE :	dataOfs -= 16;	break;
		case KEY_NPAGE :	dataOfs += 16;	break;

		case KEY_DOWN:	// down 
				if (codeViewData.cursorPos<9) codeViewData.cursorPos++;
				else codeViewData.useEIP += codeViewData.firstInstSize;
				break;
		case KEY_UP:	// up 
				if (codeViewData.cursorPos>0) codeViewData.cursorPos--;
				else {
					Bitu bytes = 0;
					char dline[200];
					Bitu size = 0;
					uint32_t newEIP = codeViewData.useEIP - 1;
					if(codeViewData.useEIP) {
						for (; bytes < 10; bytes++) {
							PhysPt start = GetAddress(codeViewData.useCS,newEIP);
							size = DasmI386(dline, start, newEIP, cpu.code.big);
							if(codeViewData.useEIP == newEIP+size) break;
							newEIP--;
						}
						if (bytes>=10) newEIP = codeViewData.useEIP - 1;
					}
					codeViewData.useEIP = newEIP;
				}
				break;
		case KEY_HOME:	// Home: scroll log page up
				DEBUG_RefreshPage(-1);
				break;
		case KEY_END:	// End: scroll log page down
				DEBUG_RefreshPage(1);
				break;
		case KEY_IC:	// Insert: toggle insert/overwrite
				codeViewData.ovrMode = !codeViewData.ovrMode;
				break;
		case KEY_LEFT:	// move to the left in command line
				if (codeViewData.inputPos > 0) codeViewData.inputPos--;
 				break;
		case KEY_RIGHT:	// move to the right in command line
				if (codeViewData.inputStr[codeViewData.inputPos]) codeViewData.inputPos++;
				break;
		case KEY_F(6):	// previous command (f1-f4 generate rubbish at my place)
		case KEY_F(3):	// previous command
				if (histBuffPos == histBuff.begin()) break;
				if (histBuffPos == histBuff.end()) {
					// copy inputStr to suspInputStr so we can restore it
					safe_strcpy(codeViewData.suspInputStr,
					            codeViewData.inputStr);
				}
				safe_strcpy(codeViewData.inputStr,
				            (--histBuffPos)->c_str());
				codeViewData.inputPos = safe_strlen(codeViewData.inputStr);
				break;
		case KEY_F(7):	// next command (f1-f4 generate rubbish at my place)
		case KEY_F(4):	// next command
				if (histBuffPos == histBuff.end()) break;
				if (++histBuffPos != histBuff.end()) {
					safe_strcpy(codeViewData.inputStr,
					            histBuffPos->c_str());
				} else {
					// copy suspInputStr back into inputStr
					safe_strcpy(codeViewData.inputStr,
					            codeViewData.suspInputStr);
				}
				codeViewData.inputPos = safe_strlen(codeViewData.inputStr);
				break;
		case KEY_F(5):	// Run Program
				debugging=false;
				DrawCode(); // update code window to show "running" status

				ret = DEBUG_Run(1,false);
				skipDraw = true; // don't update screen after this instruction
				break;
		case KEY_F(8):	// Toggle printable characters
				showPrintable = !showPrintable;
				break;
		case KEY_F(9):	// Set/Remove Breakpoint
				if (CBreakpoint::IsBreakpoint(codeViewData.cursorSeg, codeViewData.cursorOfs)) {
					if (CBreakpoint::DeleteBreakpoint(codeViewData.cursorSeg, codeViewData.cursorOfs))
						DEBUG_ShowMsg("DEBUG: Breakpoint deletion success.\n");
					else
						DEBUG_ShowMsg("DEBUG: Failed to delete breakpoint.\n");
				}
				else {
					CBreakpoint::AddBreakpoint(codeViewData.cursorSeg, codeViewData.cursorOfs, false);
					DEBUG_ShowMsg("DEBUG: Set breakpoint at %04X:%04X\n",codeViewData.cursorSeg,codeViewData.cursorOfs);
				}
				break;
		case KEY_F(10):	// Step over inst
				if (StepOver()) {
					ret = DEBUG_Run(1,false);
					skipDraw = true;
					break;
				}
				// If we aren't stepping over something, do a normal step.
				[[fallthrough]];
		case KEY_F(11):	// trace into
				exitLoop = false;
				ret = DEBUG_Run(1,true);
				break;
		case 0x0A: //Parse typed Command
				codeViewData.inputStr[MAXCMDLEN] = '\0';
				if(ParseCommand(codeViewData.inputStr)) {
					char* cmd = ltrim(codeViewData.inputStr);
					if (histBuff.empty() || *--histBuff.end()!=cmd)
						histBuff.push_back(cmd);
					if (histBuff.size() > MAX_HIST_BUFFER) histBuff.pop_front();
					histBuffPos = histBuff.end();
					ClearInputLine();
				} else {
					codeViewData.inputPos = safe_strlen(codeViewData.inputStr);
				}
				break;
		case KEY_BACKSPACE: //backspace (linux)
		case 0x7f:	// backspace in some terminal emulators (linux)
		case 0x08:	// delete 
				if (codeViewData.inputPos == 0) break;
				codeViewData.inputPos--;
				[[fallthrough]];
		case KEY_DC: // delete character
				if ((codeViewData.inputPos<0) || (codeViewData.inputPos>=MAXCMDLEN)) break;
				if (codeViewData.inputStr[codeViewData.inputPos] != 0) {
						codeViewData.inputStr[MAXCMDLEN] = '\0';
						for(char* p=&codeViewData.inputStr[codeViewData.inputPos];(*p=*(p+1));p++) {}
				}
 				break;
		default:
				if ((key>=32) && (key<127)) {
					if ((codeViewData.inputPos<0) || (codeViewData.inputPos>=MAXCMDLEN)) break;
					codeViewData.inputStr[MAXCMDLEN] = '\0';
					if (codeViewData.inputStr[codeViewData.inputPos] == 0) {
							codeViewData.inputStr[codeViewData.inputPos++] = char(key);
							codeViewData.inputStr[codeViewData.inputPos] = '\0';
					} else if (!codeViewData.ovrMode) {
						int len = (int) safe_strlen(codeViewData.inputStr);
						if (len < MAXCMDLEN) {
							for(len++;len>codeViewData.inputPos;len--)
								codeViewData.inputStr[len]=codeViewData.inputStr[len-1];
							codeViewData.inputStr[codeViewData.inputPos++] = char(key);
						}
					} else {
						codeViewData.inputStr[codeViewData.inputPos++] = char(key);
					}
				} else if (key==killchar()) {
					ClearInputLine();
				}
				break;
		}
		if (ret<0) return ret;
		if (ret>0) {
			if (ret >= CB_MAX) {
				ret = 0;
			} else {
				ret = (*Callback_Handlers[ret])();
			}
			if (ret) {
				exitLoop=true;
				CPU_Cycles=CPU_CycleLeft=0;
				return ret;
			}
		}
		ret=0;
		if (!skipDraw)
			DEBUG_DrawScreen();
	}
	return ret;
}

Bitu DEBUG_Loop(void) {
//TODO Disable sound
	DOSBOX_PollAndHandleEvents();
	// Interrupt started ? - then skip it
	uint16_t oldCS	= SegValue(cs);
	uint32_t oldEIP	= reg_eip;
	PIC_runIRQs();
	Delay(1);
	if ((oldCS!=SegValue(cs)) || (oldEIP!=reg_eip)) {
		CBreakpoint::AddBreakpoint(oldCS,oldEIP,true);
		CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip);
		debugging=false;
		DOSBOX_SetNormalLoop();
		return 0;
	}
	return DEBUG_CheckKeys();
}

#include <queue>
extern SDL_Window *pdc_window;
extern std::queue<SDL_Event> pdc_event_queue;

void DEBUG_Enable(bool pressed)
{
	if (!pressed)
		return;

	// Maybe construct the debugger's UI
	static bool was_ui_started = false;
	if (!was_ui_started) {
		DBGUI_StartUp();
		was_ui_started = (pdc_window != nullptr);
	}

	// The debugger is run in release mode so cannot use asserts
	if (!was_ui_started) {
		LOG_ERR("DEBUG: Failed to start up the debug window");
		return;
	}

	// Defocus the graphical UI and bring the debugger UI into focus
	GFX_LosingFocus();
	pdc_event_queue = {};
	SDL_RaiseWindow(pdc_window);
	SDL_SetWindowInputFocus(pdc_window);
	SetCodeWinStart();
	DEBUG_DrawScreen();

	// Maybe show help for the first time in the debugger
	static bool was_help_shown = false;
	if (!was_help_shown) {
		DEBUG_ShowMsg("***| TYPE HELP (+ENTER) TO GET AN OVERVIEW OF ALL COMMANDS |***\n");
		was_help_shown = true;
	}

	// Start the debugging loops
	debugging = true;
	DOSBOX_SetLoop(&DEBUG_Loop);

	KEYBOARD_ClrBuffer();
}

void DEBUG_DrawScreen(void) {
	DrawData();
	DrawCode();
	DrawRegisters();
	DrawVariables();
}

static void DEBUG_RaiseTimerIrq(void) {
	PIC_ActivateIRQ(0);
}

// Display the content of the MCB chain starting with the MCB at the specified segment.
static void LogMCBChain(uint16_t mcb_segment) {
	DOS_MCB mcb(mcb_segment);
	char filename[9]; // 8 characters plus a terminating NUL
	const char *psp_seg_note;
	uint16_t DOS_dataOfs = static_cast<uint16_t>(dataOfs); //Realmode addressing only
	PhysPt dataAddr = PhysicalMake(dataSeg,DOS_dataOfs);// location being viewed in the "Data Overview"

	// loop forever, breaking out of the loop once we've processed the last MCB
	while (true) {
		// verify that the type field is valid
		if (mcb.GetType()!=0x4d && mcb.GetType()!=0x5a) {
			LOG(LOG_MISC,LOG_ERROR)("MCB chain broken at %04X:0000!",mcb_segment);
			return;
		}

		mcb.GetFileName(filename);

		// some PSP segment values have special meanings
		switch (mcb.GetPSPSeg()) {
			case MCB_FREE:
				psp_seg_note = "(free)";
				break;
			case MCB_DOS:
				psp_seg_note = "(DOS)";
				break;
			default:
				psp_seg_note = "";
		}

		LOG(LOG_MISC,LOG_ERROR)("   %04X  %12u     %04X %-7s  %s",mcb_segment,mcb.GetSize() << 4,mcb.GetPSPSeg(), psp_seg_note, filename);

		// print a message if dataAddr is within this MCB's memory range
		PhysPt mcbStartAddr = PhysicalMake(mcb_segment+1,0);
		PhysPt mcbEndAddr = PhysicalMake(mcb_segment+1+mcb.GetSize(),0);
		if (dataAddr >= mcbStartAddr && dataAddr < mcbEndAddr) {
			LOG(LOG_MISC,LOG_ERROR)("   (data addr %04hX:%04X is %u bytes past this MCB)",dataSeg,DOS_dataOfs,dataAddr - mcbStartAddr);
		}

		// if we've just processed the last MCB in the chain, break out of the loop
		if (mcb.GetType()==0x5a) {
			break;
		}
		// else, move to the next MCB in the chain
		mcb_segment+=mcb.GetSize()+1;
		mcb.SetPt(mcb_segment);
	}
}

// Display the content of all Memory Control Blocks.
static void LogMCBS(void) {
	LOG(LOG_MISC,LOG_ERROR)("MCB Seg  Size (bytes)  PSP Seg (notes)  Filename");
	LOG(LOG_MISC,LOG_ERROR)("Conventional memory:");
	LogMCBChain(dos.firstMCB);

	LOG(LOG_MISC,LOG_ERROR)("Upper memory:");
	LogMCBChain(dos_infoblock.GetStartOfUMBChain());
}

static void LogGDT(void) {
	char out1[512];
	Descriptor desc;
	Bitu length = cpu.gdt.GetLimit();
	PhysPt address = cpu.gdt.GetBase();
	PhysPt max	   = address + length;
	Bitu i = 0;
	LOG(LOG_MISC,LOG_ERROR)("GDT Base:%08X Limit:%08" sBitfs(X),address,length);
	while (address<max) {
		desc.Load(address);
		sprintf(out1,"%04" sBitfs(X) ": b:%08X type: %02X parbg",(i<<3),desc.GetBase(),desc.saved.seg.type);
		LOG(LOG_MISC,LOG_ERROR)("%s",out1);
		sprintf(out1,"      l:%08X dpl : %01X  %1X%1X%1X%1X%1X",desc.GetLimit(),desc.saved.seg.dpl,desc.saved.seg.p,desc.saved.seg.avl,desc.saved.seg.r,desc.saved.seg.big,desc.saved.seg.g);
		LOG(LOG_MISC,LOG_ERROR)("%s",out1);
		address+=8; i++;
	}
}

static void LogLDT(void) {
	char out1[512];
	Descriptor desc;
	Bitu ldtSelector = cpu.gdt.SLDT();
	if (!cpu.gdt.GetDescriptor(ldtSelector,desc)) return;
	Bitu length = desc.GetLimit();
	PhysPt address = desc.GetBase();
	PhysPt max	   = address + length;
	Bitu i = 0;
	LOG(LOG_MISC,LOG_ERROR)("LDT Base:%08X Limit:%08" sBitfs(X),address,length);
	while (address<max) {
		desc.Load(address);
		sprintf(out1,"%04" sBitfs(X) ": b:%08X type: %02X parbg",(i<<3)|4,desc.GetBase(),desc.saved.seg.type);
		LOG(LOG_MISC,LOG_ERROR)("%s",out1);
		sprintf(out1,"      l:%08X dpl : %01X  %1X%1X%1X%1X%1X",desc.GetLimit(),desc.saved.seg.dpl,desc.saved.seg.p,desc.saved.seg.avl,desc.saved.seg.r,desc.saved.seg.big,desc.saved.seg.g);
		LOG(LOG_MISC,LOG_ERROR)("%s",out1);
		address+=8; i++;
	}
}

static void LogIDT(void) {
	char out1[512];
	Descriptor desc;
	uint32_t address = 0;
	while (address<256*8) {
		if (cpu.idt.GetDescriptor(address,desc)) {
			sprintf(out1,"%04X: sel:%04X off:%02X",address/8,desc.GetSelector(),desc.GetOffset());
			LOG(LOG_MISC,LOG_ERROR)("%s",out1);
		}
		address+=8;
	}
}

void LogPages(char* selname) {
	char out1[512];
	if (paging.enabled) {
		Bitu sel = GetHexValue(selname,selname);
		if ((sel==0x00) && ((*selname==0) || (*selname=='*'))) {
			for (int i=0; i<0xfffff; i++) {
				Bitu table_addr=(paging.base.page<<12)+(i >> 10)*4;
				X86PageEntry table;
				table.set(phys_readd(table_addr));
				if (table.p) {
					X86PageEntry entry;
					Bitu entry_addr = (table.base << 12) +
					                  (i & 0x3ff) * 4;
					entry.set(phys_readd(entry_addr));
					if (entry.p) {
						sprintf(out1,
						        "page %05Xxxx -> %04Xxxx  flags [uw] %x:%x::%x:%x [d=%x|a=%x]",
						        i,
						        entry.base,
						        entry.us,
						        table.us,
						        entry.wr,
						        table.wr,
						        entry.d,
						        entry.a);
						LOG(LOG_MISC, LOG_ERROR)
						("%s", out1);
					}
				}
			}
		} else {
			Bitu table_addr=(paging.base.page<<12)+(sel >> 10)*4;
			X86PageEntry table;
			table.set(phys_readd(table_addr));
			if (table.p) {
				X86PageEntry entry;
				Bitu entry_addr = (table.base << 12) +
				                  (sel & 0x3ff) * 4;
				entry.set(phys_readd(entry_addr));
				sprintf(out1,
				        "page %05" sBitfs(X) "xxx -> %04Xxxx  flags [puw] %x:%x::%x:%x::%x:%x",
				        sel,
				        entry.base,
				        entry.p,
				        table.p,
				        entry.us,
				        table.us,
				        entry.wr,
				        table.wr);
				LOG(LOG_MISC, LOG_ERROR)("%s", out1);
			} else {
				sprintf(out1,
				        "pagetable %03" sBitfs(X) " not present, flags [puw] %x::%x::%x",
				        (sel >> 10),
				        table.p,
				        table.us,
				        table.wr);
				LOG(LOG_MISC, LOG_ERROR)("%s", out1);
			}
		}
	}
}

static void LogCPUInfo(void) {
	char out1[512];
	sprintf(out1,"cr0:%08" sBitfs(X) " cr2:%08u cr3:%08u  cpl=%" sBitfs(x),cpu.cr0,paging.cr2,paging.cr3,cpu.cpl);
	LOG(LOG_MISC,LOG_ERROR)("%s",out1);
	sprintf(out1, "eflags:%08x [vm=%x iopl=%x nt=%x]", reg_flags,
	        GETFLAG(VM) >> 17, GETFLAG(IOPL) >> 12, GETFLAG(NT) >> 14);
	LOG(LOG_MISC,LOG_ERROR)("%s",out1);
	sprintf(out1,"GDT base=%08X limit=%08" sBitfs(X),cpu.gdt.GetBase(),cpu.gdt.GetLimit());
	LOG(LOG_MISC,LOG_ERROR)("%s",out1);
	sprintf(out1,"IDT base=%08X limit=%08" sBitfs(X),cpu.idt.GetBase(),cpu.idt.GetLimit());
	LOG(LOG_MISC,LOG_ERROR)("%s",out1);

	Bitu sel=CPU_STR();
	Descriptor desc;
	if (cpu.gdt.GetDescriptor(sel,desc)) {
		sprintf(out1,"TR selector=%04" sBitfs(X) ", base=%08X limit=%08X*%X",sel,desc.GetBase(),desc.GetLimit(),desc.saved.seg.g?0x4000:1);
		LOG(LOG_MISC,LOG_ERROR)("%s",out1);
	}
	sel=CPU_SLDT();
	if (cpu.gdt.GetDescriptor(sel,desc)) {
		sprintf(out1,"LDT selector=%04" sBitfs(X) ", base=%08X limit=%08X*%X",sel,desc.GetBase(),desc.GetLimit(),desc.saved.seg.g?0x4000:1);
		LOG(LOG_MISC,LOG_ERROR)("%s",out1);
	}
}

#if C_HEAVY_DEBUG
static void LogInstruction(uint16_t segValue, uint32_t eipValue, std::ofstream &out)
{
	static char empty[23] = { 32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,0 };

	using std::setw;
	if (cpuLogType == 3) { //Log only cs:ip.
		out << setw(4) << SegValue(cs) << ":" << setw(8) << reg_eip << std::endl;
		return;
	}

	PhysPt start = GetAddress(segValue,eipValue);
	char dline[200];Bitu size;
	size = DasmI386(dline, start, reg_eip, cpu.code.big);
	char* res = empty;
	if (showExtend && (cpuLogType > 0) ) {
		res = AnalyzeInstruction(dline,false);
		if (!res || !(*res)) res = empty;
		Bitu reslen = strlen(res);
		if (reslen < 22) memset(res + reslen, ' ',22 - reslen);
		res[22] = 0;
	}
	Bitu len = safe_strlen(dline);
	if (len < 30) memset(dline + len,' ',30 - len);
	dline[30] = 0;

	// Get register values

	if(cpuLogType == 0) {
		out << setw(4) << SegValue(cs) << ":" << setw(4) << reg_eip << "  " << dline;
	} else if (cpuLogType == 1) {
		out << setw(4) << SegValue(cs) << ":" << setw(8) << reg_eip << "  " << dline << "  " << res;
	} else if (cpuLogType == 2) {
		char ibytes[200]="";	char tmpc[200];
		for (Bitu i=0; i<size; i++) {
			uint8_t value;
			if (mem_readb_checked(start+i,&value)) sprintf(tmpc,"%s","?? ");
			else sprintf(tmpc,"%02X ",value);
			strcat(ibytes,tmpc);
		}
		len = safe_strlen(ibytes);
		if (len<21) { for (Bitu i=0; i<21-len; i++) ibytes[len + i] =' '; ibytes[21]=0;} //NOTE THE BRACKETS
		out << setw(4) << SegValue(cs) << ":" << setw(8) << reg_eip << "  " << dline << "  " << res << "  " << ibytes;
	}

	out << " EAX:" << setw(8) << reg_eax << " EBX:" << setw(8) << reg_ebx
	    << " ECX:" << setw(8) << reg_ecx << " EDX:" << setw(8) << reg_edx
	    << " ESI:" << setw(8) << reg_esi << " EDI:" << setw(8) << reg_edi
	    << " EBP:" << setw(8) << reg_ebp << " ESP:" << setw(8) << reg_esp
	    << " DS:"  << setw(4) << SegValue(ds)<< " ES:"  << setw(4) << SegValue(es);

	if(cpuLogType == 0) {
		out << " SS:"  << setw(4) << SegValue(ss) << " C"  << (get_CF()>0)  << " Z"   << (get_ZF()>0)
		    << " S" << (get_SF()>0) << " O"  << (get_OF()>0) << " I"  << GETFLAGBOOL(IF);
	} else {
		out << " FS:"  << setw(4) << SegValue(fs) << " GS:"  << setw(4) << SegValue(gs)
		    << " SS:"  << setw(4) << SegValue(ss)
		    << " CF:"  << (get_CF()>0)  << " ZF:"   << (get_ZF()>0)  << " SF:"  << (get_SF()>0)
		    << " OF:"  << (get_OF()>0)  << " AF:"   << (get_AF()>0)  << " PF:"  << (get_PF()>0)
		    << " IF:"  << GETFLAGBOOL(IF);
	}
	if(cpuLogType == 2) {
		out << " TF:" << GETFLAGBOOL(TF) << " VM:" << GETFLAGBOOL(VM) <<" FLG:" << setw(8) << reg_flags
		    << " CR0:" << setw(8) << cpu.cr0;
	}
	out << std::endl;
}
#endif

// DEBUG.COM stuff

class DEBUG final : public Program {
public:
	DEBUG() : active(false) { pDebugcom = this; }

	~DEBUG() override { pDebugcom = nullptr; }

	bool IsActive() const { return active; }

	void Run() override
	{
		if(cmd->FindExist("/NOMOUSE",false)) {
	        	real_writed(0,0x33<<2,0);
			return;
		}

		uint16_t commandNr = 1;
		if (!cmd->FindCommand(commandNr++,temp_line)) return;
		// Get filename
		char filename[128];
		safe_strcpy(filename, temp_line.c_str());
		// Setup commandline
		char args[256+1];
		args[0]	= 0;
		bool found = cmd->FindCommand(commandNr++,temp_line);
		while (found) {
			if (safe_strlen(args)+temp_line.length()+1>256) break;
			strcat(args,temp_line.c_str());
			found = cmd->FindCommand(commandNr++,temp_line);
			if (found) strcat(args," ");
		}
		// Start new shell and execute prog
		active = true;
		// Save cpu state....
		uint16_t oldcs	= SegValue(cs);
		uint32_t oldeip	= reg_eip;
		uint16_t oldss	= SegValue(ss);
		uint32_t oldesp	= reg_esp;

		// Start shell
		DOS_Shell shell;
		if (!shell.ExecuteProgram(filename, args))
			WriteOut(MSG_Get("PROGRAM_EXECUTABLE_MISSING"), filename);

		// set old reg values
		SegSet16(ss,oldss);
		reg_esp = oldesp;
		SegSet16(cs,oldcs);
		reg_eip = oldeip;
	}

private:
	bool	active;
};

void DEBUG_CheckExecuteBreakpoint(uint16_t seg, uint32_t off)
{
	if (pDebugcom && pDebugcom->IsActive()) {
		CBreakpoint::AddBreakpoint(seg,off,true);
		CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip);
		pDebugcom = nullptr;
	}
}

Bitu DEBUG_EnableDebugger()
{
	exitLoop = true;
	DEBUG_Enable(true);
	CPU_Cycles=CPU_CycleLeft=0;
	return 0;
}

void DEBUG_ShutDown(Section * /*sec*/) {
	CBreakpoint::DeleteAll();
	CDebugVar::DeleteAll();
	curs_set(old_cursor_state);

	if (pdc_window)
		endwin();
}

Bitu debugCallback;

void DEBUG_Init(Section* sec) {

//	MSG_Add("DEBUG_CONFIGFILE_HELP","Debugger related options.\n");
	DEBUG_DrawScreen();
	/* Add some keyhandlers */
	MAPPER_AddHandler(DEBUG_Enable, SDL_SCANCODE_PAUSE, MMOD2, "debugger",
	                  "Debugger");
	/* Reset code overview and input line */
	codeViewData = {};
	/* setup debug.com */
	PROGRAMS_MakeFile("DEBUG.COM",ProgramCreate<DEBUG>);
	PROGRAMS_MakeFile("DBXDEBUG.COM",ProgramCreate<DEBUG>);
	/* Setup callback */
	debugCallback=CALLBACK_Allocate();
	CALLBACK_Setup(debugCallback,DEBUG_EnableDebugger,CB_RETF,"debugger");
	/* shutdown function */
	sec->AddDestroyFunction(&DEBUG_ShutDown);
}

// DEBUGGING VAR STUFF

void CDebugVar::InsertVariable(char *name, PhysPt adr)
{
	varList.push_back(new CDebugVar(name,adr));
}

void CDebugVar::DeleteAll()
{
	std::vector<CDebugVar*>::iterator i;
	CDebugVar* bp;
	for(i=varList.begin(); i != varList.end(); i++) {
		bp = static_cast<CDebugVar*>(*i);
		delete bp;
	}
	(varList.clear)();
}

CDebugVar *CDebugVar::FindVar(PhysPt pt)
{
	if (varList.empty()) return nullptr;

	std::vector<CDebugVar*>::size_type s = varList.size();
	CDebugVar* bp;
	for(std::vector<CDebugVar*>::size_type i = 0; i != s; i++) {
		bp = static_cast<CDebugVar*>(varList[i]);
		if (bp->GetAdr() == pt) return bp;
	}
	return nullptr;
}

bool CDebugVar::SaveVars(char *name)
{
	if (varList.size() > 65535) return false;
	const std_fs::path vars_file = name;
	FILE* f = fopen(vars_file.string().c_str(), "wb+");
	if (!f) {
		DEBUG_ShowMsg("DEBUG: Output of vars failed.\n");
		return false;
	}
	DEBUG_ShowMsg("DEBUG: vars file '%s' created.\n",
	              std_fs::absolute(vars_file).string().c_str());


	// write number of vars
	uint16_t num = (uint16_t)varList.size();
	fwrite(&num,1,sizeof(num),f);

	std::vector<CDebugVar*>::iterator i;
	CDebugVar* bp;
	for(i=varList.begin(); i != varList.end(); i++) {
		bp = static_cast<CDebugVar*>(*i);
		// name
		fwrite(bp->GetName(),1,16,f);
		// adr
		PhysPt adr = bp->GetAdr();
		fwrite(&adr,1,sizeof(adr),f);
	}
	fclose(f);
	return true;
}

bool CDebugVar::LoadVars(char *name)
{
	const std_fs::path vars_file = name;
	FILE* f = fopen(vars_file.string().c_str(), "rb");
	if (!f) {
		DEBUG_ShowMsg("DEBUG: Load of vars from %s failed.\n",
		              name);
		return false;
	}
	DEBUG_ShowMsg("DEBUG: vars file '%s' loaded.\n",
	              std_fs::absolute(vars_file).string().c_str());
	// read number of vars
	uint16_t num;
	if (fread(&num,sizeof(num),1,f) != 1) {
		fclose(f);
		return false;
	}
	for (uint16_t i=0; i<num; i++) {
		char name[16];
		// name
		if (fread(name,16,1,f) != 1) break;
		// adr
		PhysPt adr;
		if (fread(&adr,sizeof(adr),1,f) != 1) break;
		// insert
		InsertVariable(name,adr);
	}
	fclose(f);
	return true;
}

static void SaveMemory(uint16_t seg, uint32_t ofs1, uint32_t num) {
	const std_fs::path memdump_txt = "MEMDUMP.TXT";
	FILE* f = fopen(memdump_txt.string().c_str(),"wt");
	if (!f) {
		DEBUG_ShowMsg("DEBUG: Memory dump failed.\n");
		return;
	}
	DEBUG_ShowMsg("DEBUG: Memory dump file '%s' created.\n",
	              std_fs::absolute(memdump_txt).string().c_str());

	char buffer[128];
	char temp[16];

	while (num>16) {
		sprintf(buffer,"%04X:%04X   ",seg,ofs1);
		for (uint16_t x=0; x<16; x++) {
			uint8_t value;
			if (mem_readb_checked(GetAddress(seg,ofs1+x),&value)) sprintf(temp,"%s","?? ");
			else sprintf(temp,"%02X ",value);
			strcat(buffer,temp);
		}
		ofs1+=16;
		num-=16;

		fprintf(f,"%s\n",buffer);
	}
	if (num>0) {
		sprintf(buffer,"%04X:%04X   ",seg,ofs1);
		for (uint16_t x=0; x<num; x++) {
			uint8_t value;
			if (mem_readb_checked(GetAddress(seg,ofs1+x),&value)) sprintf(temp,"%s","?? ");
			else sprintf(temp,"%02X ",value);
			strcat(buffer,temp);
		}
		fprintf(f,"%s\n",buffer);
	}
	fclose(f);
	DEBUG_ShowMsg("DEBUG: Memory dump success.\n");
}

static void SaveMemoryBin(uint16_t seg, uint32_t ofs1, uint32_t num) {
	const std_fs::path memdump_bin = "MEMDUMP.BIN";
	FILE* f = fopen(memdump_bin.string().c_str(), "wb");
	if (!f) {
		DEBUG_ShowMsg("DEBUG: Memory binary dump failed.\n");
		return;
	}
	DEBUG_ShowMsg("DEBUG: Memory binary dump file '%s' created.\n",
	              std_fs::absolute(memdump_bin).string().c_str());

	for (Bitu x = 0; x < num;x++) {
		uint8_t val;
		if (mem_readb_checked(GetAddress(seg,ofs1+x),&val)) val=0;
		fwrite(&val,1,1,f);
	}

	fclose(f);
	DEBUG_ShowMsg("DEBUG: Memory dump binary success.\n");
}

static void OutputVecTable(char* filename) {
	const std_fs::path vec_table_file = filename;
	FILE* f = fopen(vec_table_file.string().c_str(), "wt");
	if (!f)
	{
		DEBUG_ShowMsg("DEBUG: Output of interrupt vector table failed.\n");
		return;
	}
	DEBUG_ShowMsg("DEBUG: Interrupt vector table file '%s' created.\n",
	              std_fs::absolute(vec_table_file).string().c_str());
	
	for (int i=0; i<256; i++)
		fprintf(f,"INT %02X:  %04X:%04X\n", i, mem_readw(i*4+2), mem_readw(i*4));

	fclose(f);
	DEBUG_ShowMsg("DEBUG: Interrupt vector table written to %s.\n",
	              vec_table_file.string().c_str());
}

#define DEBUG_VAR_BUF_LEN 16
static void DrawVariables()
{
	if (varList.empty())
		return;

	char buffer[DEBUG_VAR_BUF_LEN] = {};
	bool windowchanges = false;

	for (size_t i = 0; i != varList.size(); ++i) {
		if (i == 4*3) {
			/* too many variables */
			break;
		}

		auto dv = varList[i];
		uint16_t value;
		bool varchanges = false;
		bool has_no_value = mem_readw_checked(dv->GetAdr(),&value);
		if (has_no_value) {
			snprintf(buffer,DEBUG_VAR_BUF_LEN, "%s", "??????");
			dv->SetValue(false,0);
			varchanges = true;
		} else {
			if ( dv->HasValue() && dv->GetValue() == value) {
				; //It already had a value and it didn't change (most likely case)
			} else {
				dv->SetValue(true,value);
				snprintf(buffer,DEBUG_VAR_BUF_LEN, "0x%04x", value);
				varchanges = true;
			}
		}

		if (varchanges) {
			int y = i / 3;
			int x = (i % 3) * 26;
			mvwprintw(dbg.win_var, y, x, dv->GetName());
			mvwprintw(dbg.win_var, y,  (x + DEBUG_VAR_BUF_LEN + 1) , buffer);
			windowchanges = true; //Something has changed in this window
		}
	}

	if (windowchanges) wrefresh(dbg.win_var);
}
#undef DEBUG_VAR_BUF_LEN
// HEAVY DEBUGGING STUFF

#if C_HEAVY_DEBUG

const uint32_t LOGCPUMAX = 20000;

static uint32_t logCount = 0;

struct TLogInst {
	uint16_t s_cs;
	uint32_t eip;
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint32_t ebp;
	uint32_t esp;
	uint16_t s_ds;
	uint16_t s_es;
	uint16_t s_fs;
	uint16_t s_gs;
	uint16_t s_ss;
	bool c;
	bool z;
	bool s;
	bool o;
	bool a;
	bool p;
	bool i;
	char dline[31];
	char res[23];
};

TLogInst logInst[LOGCPUMAX];

void DEBUG_HeavyLogInstruction()
{
	static char empty[23] = { 32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,0 };

	PhysPt start = GetAddress(SegValue(cs),reg_eip);
	char dline[200];
	DasmI386(dline, start, reg_eip, cpu.code.big);
	char* res = empty;
	if (showExtend) {
		res = AnalyzeInstruction(dline,false);
		if (!res || !(*res)) res = empty;
		Bitu reslen = strlen(res);
		if (reslen < 22) memset(res + reslen, ' ',22 - reslen);
		res[22] = 0;
	}

	Bitu len = safe_strlen(dline);
	if (len < 30) memset(dline + len,' ',30 - len);
	dline[30] = 0;

	TLogInst & inst = logInst[logCount];
	strcpy(inst.dline,dline);
	inst.s_cs = SegValue(cs);
	inst.eip  = reg_eip;
	strcpy(inst.res,res);
	inst.eax  = reg_eax;
	inst.ebx  = reg_ebx;
	inst.ecx  = reg_ecx;
	inst.edx  = reg_edx;
	inst.esi  = reg_esi;
	inst.edi  = reg_edi;
	inst.ebp  = reg_ebp;
	inst.esp  = reg_esp;
	inst.s_ds = SegValue(ds);
	inst.s_es = SegValue(es);
	inst.s_fs = SegValue(fs);
	inst.s_gs = SegValue(gs);
	inst.s_ss = SegValue(ss);
	inst.c    = get_CF()>0;
	inst.z    = get_ZF()>0;
	inst.s    = get_SF()>0;
	inst.o    = get_OF()>0;
	inst.a    = get_AF()>0;
	inst.p    = get_PF()>0;
	inst.i    = GETFLAGBOOL(IF);

	if (++logCount >= LOGCPUMAX) logCount = 0;
}

void DEBUG_HeavyWriteLogInstruction()
{
	if (!logHeavy) return;
	logHeavy = false;

	DEBUG_ShowMsg("DEBUG: Creating cpu log LOGCPU_INT_CD.TXT\n");

	std::ofstream out("LOGCPU_INT_CD.TXT");
	if (!out.is_open()) {
		DEBUG_ShowMsg("DEBUG: Failed.\n");
		return;
	}
	out << std::hex << std::noshowbase << std::setfill('0') << std::uppercase;
	uint32_t startLog = logCount;
	do {
		// Write Instructions
		TLogInst & inst = logInst[startLog];
		using std::setw;
		out << setw(4) << inst.s_cs << ":" << setw(8) << inst.eip << "  "
		    << inst.dline << "  " << inst.res << " EAX:" << setw(8)<< inst.eax
		    << " EBX:" << setw(8) << inst.ebx << " ECX:" << setw(8) << inst.ecx
		    << " EDX:" << setw(8) << inst.edx << " ESI:" << setw(8) << inst.esi
		    << " EDI:" << setw(8) << inst.edi << " EBP:" << setw(8) << inst.ebp
		    << " ESP:" << setw(8) << inst.esp << " DS:"  << setw(4) << inst.s_ds
		    << " ES:"  << setw(4) << inst.s_es<< " FS:"  << setw(4) << inst.s_fs
		    << " GS:"  << setw(4) << inst.s_gs<< " SS:"  << setw(4) << inst.s_ss
		    << " CF:"  << inst.c  << " ZF:"   << inst.z  << " SF:"  << inst.s
		    << " OF:"  << inst.o  << " AF:"   << inst.a  << " PF:"  << inst.p
		    << " IF:"  << inst.i  << std::endl;

/*		fprintf(f,"%04X:%08X   %s  %s  EAX:%08X EBX:%08X ECX:%08X EDX:%08X ESI:%08X EDI:%08X EBP:%08X ESP:%08X DS:%04X ES:%04X FS:%04X GS:%04X SS:%04X CF:%01X ZF:%01X SF:%01X OF:%01X AF:%01X PF:%01X IF:%01X\n",
			logInst[startLog].s_cs,logInst[startLog].eip,logInst[startLog].dline,logInst[startLog].res,logInst[startLog].eax,logInst[startLog].ebx,logInst[startLog].ecx,logInst[startLog].edx,logInst[startLog].esi,logInst[startLog].edi,logInst[startLog].ebp,logInst[startLog].esp,
		        logInst[startLog].s_ds,logInst[startLog].s_es,logInst[startLog].s_fs,logInst[startLog].s_gs,logInst[startLog].s_ss,
		        logInst[startLog].c,logInst[startLog].z,logInst[startLog].s,logInst[startLog].o,logInst[startLog].a,logInst[startLog].p,logInst[startLog].i);*/
		if (++startLog >= LOGCPUMAX) startLog = 0;
	} while (startLog != logCount);

	out.close();
	DEBUG_ShowMsg("DEBUG: Done.\n");
}

bool DEBUG_HeavyIsBreakpoint(void) {
	static Bitu zero_count = 0;
	if (cpuLog) {
		if (cpuLogCounter>0) {
			LogInstruction(SegValue(cs),reg_eip,cpuLogFile);
			cpuLogCounter--;
		}
		if (cpuLogCounter<=0) {
			cpuLogFile.flush();
			cpuLogFile.close();
			DEBUG_ShowMsg("DEBUG: cpu log LOGCPU.TXT created\n");
			cpuLog = false;
			DEBUG_EnableDebugger();
			return true;
		}
	}
	// LogInstruction
	if (logHeavy) DEBUG_HeavyLogInstruction();
	if (zeroProtect) {
		uint32_t value = 0;
		if (!mem_readd_checked(SegPhys(cs)+reg_eip,&value)) {
			if (value == 0) zero_count++;
			else zero_count = 0;
		}
		if (zero_count == 10) {
			E_Exit("running zeroed code");
		}
	}

	if (skipFirstInstruction) {
		skipFirstInstruction = false;
		return false;
	}
	if (BPoints.size() && CBreakpoint::CheckBreakpoint(SegValue(cs), reg_eip))
		return true;

	return false;
}

#endif // HEAVY DEBUG

#endif // DEBUG
