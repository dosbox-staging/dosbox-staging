// SPDX-FileCopyrightText:  2002-2025 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

/* Local Debug Function */

#ifndef DOSBOX_DEBUGGER_INC_H
#define DOSBOX_DEBUGGER_INC_H

#include "hardware/memory.h"
#include <SDL3/SDL.h>
#include <queue>
#include <string>

enum DebugColorPairs {
	PAIR_BLACK_BLUE    = 1,
	PAIR_BYELLOW_BLACK = 2,
	PAIR_GREEN_BLACK   = 3,
	PAIR_BLACK_GREY    = 4,
	PAIR_GREY_RED      = 5,
};

void DBGUI_StartUp();
void DBGUI_Shutdown();
void DBGUI_NewFrame();
void DBGUI_Render();
bool DBGUI_IsInitialized();
void DBGUI_DrawOutputWindow();
float DBGUI_GetWindowY(int window_index);
float DBGUI_GetTotalHeight();
float DBGUI_GetWindowWidth();

// Window title styling helpers (cyan background, black text)
bool DBGUI_BeginWindowWithStyledTitle(const char* title, int flags);
void DBGUI_EndWindowWithStyledTitle();

// Input handling
int DBGUI_GetKey();
void DBGUI_UngetKey(int key);
bool DBGUI_HasKey();

// Key constants (matching ncurses/PDCurses)
constexpr int KEY_NONE            = -1;
constexpr int DBGUI_KEY_UP        = 0x103;
constexpr int DBGUI_KEY_DOWN      = 0x102;
constexpr int DBGUI_KEY_LEFT      = 0x104;
constexpr int DBGUI_KEY_RIGHT     = 0x105;
constexpr int DBGUI_KEY_PPAGE     = 0x153;
constexpr int DBGUI_KEY_NPAGE     = 0x152;
constexpr int DBGUI_KEY_HOME      = 0x106;
constexpr int DBGUI_KEY_END       = 0x166;
constexpr int DBGUI_KEY_BACKSPACE = 0x107;
constexpr int DBGUI_KEY_DC        = 0x14A; // Delete character
constexpr int DBGUI_KEY_IC        = 0x14B; // Insert character
constexpr int DBGUI_KEY_F(int n)
{
	return 0x108 + n - 1;
}

// GUI layout and styling constants
namespace DBGUI {

// Buffer sizes
constexpr size_t MaxLogBuffer      = 500;
constexpr size_t MsgBufferSize     = 512;
constexpr size_t LogNameBufferSize = 64;

// Window dimensions (in characters/rows)
// +2 to account for padding
constexpr int DefaultWindowCols = 82;

// ImGui style
constexpr float WindowRounding    = 0.0f;
constexpr float FrameRounding     = 0.0f;
constexpr float ScrollbarRounding = 0.0f;
constexpr float FontSize          = 16.0f;

// Layout spacing
constexpr float WindowSeparatorSpacing = 4.0f;
constexpr float AutoScrollThreshold    = 0.0f;

// Clear color (RGBA)
constexpr uint8_t ClearColorR = 0;
constexpr uint8_t ClearColorG = 0;
constexpr uint8_t ClearColorB = 0;
constexpr uint8_t ClearColorA = 255;

} // namespace DBGUI

struct DBGBlock {
	SDL_Window* win_main   = nullptr;
	SDL_Renderer* renderer = nullptr;
	uint32_t active_win    = 0;
	uint32_t input_y       = 0;
	uint32_t global_mask   = 0;
	/* Window height values in rows */
	int32_t rows_registers = 4;
	int32_t rows_data      = 8;
	int32_t rows_code      = 11;
	int32_t rows_variables = 4;
	int32_t rows_output    = 8;

	// Scrolling state
	float output_scroll_y = 0.0f;

	// Window dimensions (in characters)
	int32_t window_cols = DBGUI::DefaultWindowCols;

	// Computed window dimensions (in pixels, calculated from rows/cols)
	int window_width  = 0;
	int window_height = 0;
};

struct DASMLine {
	uint32_t pc = 0;
	char dasm[80] = {0};
	PhysPt ea = 0;
	uint16_t easeg = 0;
	uint32_t eaoff = 0;
};

extern DBGBlock dbg;

// Event queue for debugger input
struct DebuggerInputEvent {
	SDL_Event ev     = {};
	std::string text = {};
};

extern std::queue<DebuggerInputEvent> debugger_event_queue;

/* Local Debug Stuff */
Bitu DasmI386(char* buffer, PhysPt pc, Bitu cur_ip, bool bit32);
int DasmLastOperandSize();

#endif // DOSBOX_DEBUGGER_INC_H
