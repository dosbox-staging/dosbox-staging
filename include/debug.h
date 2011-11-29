/*
 *  Copyright (C) 2002-2011  The DOSBox Team
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

#ifndef DOSBOX_DEBUG_H
#define DOSBOX_DEBUG_H

#if C_DEBUG || C_GDBSERVER

#include <fstream>

extern Bit16u  DEBUG_dataSeg;
extern Bit32u  DEBUG_dataOfs;
extern bool    DEBUG_showExtend;
extern char DEBUG_curSelectorName[3];
extern bool DEBUG_exitLoop;

void DEBUG_SetupConsole(void);
void DEBUG_DrawScreen(void);
bool DEBUG_Breakpoint(void);
bool DEBUG_IntBreakpoint(Bit8u intNum);
void DEBUG_Enable(bool pressed);
void DEBUG_CheckExecuteBreakpoint(Bit16u seg, Bit32u off);
bool DEBUG_ExitLoop(void);
void DEBUG_RefreshPage(char scroll);
Bitu DEBUG_EnableDebugger(void);
Bit32u DEBUG_GetHexValue(char* str, char*& hex);
Bit32u DEBUG_GetAddress(Bit16u seg, Bit32u offset);
char* DEBUG_AnalyzeInstruction(char* inst, bool saveSelector);
bool DEBUG_GetDescriptorInfo(char* selname, char* out1, char* out2);

void DEBUG_LogMCBChain(Bit16u mcb_segment);
void DEBUG_LogMCBS(void);
void DEBUG_LogGDT(void);
void DEBUG_LogLDT(void);
void DEBUG_LogIDT(void);
void DEBUG_LogPages(char* selname);
void DEBUG_LogCPUInfo(void);
void DEBUG_LogInstruction(Bit16u segValue, Bit32u eipValue, std::ofstream& out);

extern bool    DEBUG_showExtend;

extern Bitu DEBUG_cycle_count;
extern Bitu DEBUG_debugCallback;

#if C_HEAVY_DEBUG || C_GDBSERVER
extern std::ofstream DEBUG_cpuLogFile;
extern bool DEBUG_cpuLog;
extern int  DEBUG_cpuLogCounter;
extern int  DEBUG_cpuLogType;	// log detail
extern bool DEBUG_zeroProtect;
extern bool DEBUG_logHeavy;

bool DEBUG_HeavyIsBreakpoint(void);
void DEBUG_HeavyLogInstruction(void);
void DEBUG_HeavyWriteLogInstruction(void);
#endif

#ifdef C_GDBSERVER
void DEBUG_GdbMemReadHook(Bit32u address, int width);
void DEBUG_GdbMemWriteHook(Bit32u address, int width, Bit32u value);
void DEBUG_IrqBreakpoint(Bit8u intNum);
#endif

/********************/
/* DebugVar   stuff */
/********************/

#include <list>
#include "paging.h"

class CDebugVar
{
public:
	CDebugVar(char* _name, PhysPt _adr);
	
	char*	GetName(void) { return name; };
	PhysPt	GetAdr (void) { return adr;  };

private:
	PhysPt  adr;
	char	name[16];

public: 
	static void			InsertVariable	(char* name, PhysPt adr);
	static CDebugVar*	FindVar			(PhysPt adr);
	static void			DeleteAll		();
	static bool			SaveVars		(char* name);
	static bool			LoadVars		(char* name);

	static std::list<CDebugVar*>	varList;
};

#endif
#endif
