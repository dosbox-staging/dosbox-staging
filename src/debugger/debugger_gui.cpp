// SPDX-FileCopyrightText:  2002-2025 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#if C_DEBUGGER
#include "config/config.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iterator>
#include <list>
#include <string>

#include <SDL3/SDL.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "cpu/cpu.h"
#include "cpu/registers.h"
#include "debugger.h"
#include "debugger_inc.h"
#include "misc/cross.h"
#include "misc/support.h"
#include "utils/string_utils.h"

#include "IBM_VGA_8x16.h"

struct _LogGroup {
	const char* front = nullptr;
	bool enabled      = false;
};

static std::list<std::string> logBuff = {};

// Scroll state for Output window (lines from bottom, 0 = at bottom)
static int output_scroll_offset = 0;

static _LogGroup loggrp[LOG_MAX] = {
        {     "",  true},
        {nullptr, false}
};
static FILE* debuglog = nullptr;

// ImGui state
static bool imgui_initialized = false;
static std::deque<int> key_buffer;
static float display_scale = 1.0f;

// Event queue
std::queue<DebuggerInputEvent> debugger_event_queue;

bool DBGUI_IsInitialized()
{
	return imgui_initialized;
}

int DBGUI_GetKey()
{
	// First check key buffer
	if (!key_buffer.empty()) {
		int key = key_buffer.front();
		key_buffer.pop_front();
		return key;
	}

	// Then check event queue
	while (!debugger_event_queue.empty()) {
		DebuggerInputEvent event = debugger_event_queue.front();
		debugger_event_queue.pop();

		// Process ImGui events
		ImGui_ImplSDL3_ProcessEvent(&event.ev);

		// Convert SDL events to key codes
		if (event.ev.type == SDL_EVENT_KEY_DOWN) {
			SDL_Keycode key = event.ev.key.key;
			SDL_Keymod mod  = event.ev.key.mod;

			// Map SDL keys to our key constants
			switch (key) {
			case SDLK_UP: return DBGUI_KEY_UP;
			case SDLK_DOWN: return DBGUI_KEY_DOWN;
			case SDLK_LEFT: return DBGUI_KEY_LEFT;
			case SDLK_RIGHT: return DBGUI_KEY_RIGHT;
			case SDLK_PAGEUP: return DBGUI_KEY_PPAGE;
			case SDLK_PAGEDOWN: return DBGUI_KEY_NPAGE;
			case SDLK_HOME: return DBGUI_KEY_HOME;
			case SDLK_END: return DBGUI_KEY_END;
			case SDLK_BACKSPACE: return DBGUI_KEY_BACKSPACE;
			case SDLK_DELETE: return DBGUI_KEY_DC;
			case SDLK_INSERT: return DBGUI_KEY_IC;
			case SDLK_RETURN: return '\n';
			case SDLK_ESCAPE: return 27;
			case SDLK_TAB: return '\t';
			case SDLK_F1: return DBGUI_KEY_F(1);
			case SDLK_F2: return DBGUI_KEY_F(2);
			case SDLK_F3: return DBGUI_KEY_F(3);
			case SDLK_F4: return DBGUI_KEY_F(4);
			case SDLK_F5: return DBGUI_KEY_F(5);
			case SDLK_F6: return DBGUI_KEY_F(6);
			case SDLK_F7: return DBGUI_KEY_F(7);
			case SDLK_F8: return DBGUI_KEY_F(8);
			case SDLK_F9: return DBGUI_KEY_F(9);
			case SDLK_F10: return DBGUI_KEY_F(10);
			case SDLK_F11: return DBGUI_KEY_F(11);
			case SDLK_F12: return DBGUI_KEY_F(12);
			default:
				// Handle printable characters
				if (key >= SDLK_SPACE && key <= SDLK_Z) {
					char c = static_cast<char>(key);
					if (mod & SDL_KMOD_SHIFT) {
						c = toupper(c);
					}
					return c;
				}
				if (key >= SDLK_0 && key <= SDLK_9) {
					return static_cast<int>(key);
				}
				break;
			}
		} else if (event.ev.type == SDL_EVENT_TEXT_INPUT) {
			if (!event.text.empty()) {
				return static_cast<unsigned char>(event.text[0]);
			}
		}
	}

	return KEY_NONE;
}

void DBGUI_UngetKey(int key)
{
	key_buffer.push_front(key);
}

bool DBGUI_HasKey()
{
	return !key_buffer.empty() || !debugger_event_queue.empty();
}

void DEBUG_ShowMsg(const char* format, ...)
{
	if (!imgui_initialized) {
		return;
	}

	char buf[DBGUI::MsgBufferSize];
	va_list msg;
	va_start(msg, format);
	vsnprintf(buf, sizeof(buf), format, msg);
	va_end(msg);

	buf[sizeof(buf) - 1] = '\0';

	/* Add newline if not present */
	size_t len = safe_strlen(buf);
	if (len > 0 && buf[len - 1] != '\n' && len + 1 < sizeof(buf)) {
		strcat(buf, "\n");
	}

	if (debuglog) {
		fprintf(debuglog, "%s", buf);
		fflush(debuglog);
	}

	logBuff.emplace_back(buf);
	if (logBuff.size() > DBGUI::MaxLogBuffer) {
		logBuff.pop_front();
	}
	// Don't reset scroll offset - let user stay at their scroll position
}

void DEBUG_RefreshPage(int scroll)
{
	if (!imgui_initialized) {
		return;
	}

	if (scroll == -1) {
		// Home: scroll up (increase offset from bottom)
		output_scroll_offset++;
	} else if (scroll == 1) {
		// End: scroll down (decrease offset, min 0 = at bottom)
		if (output_scroll_offset > 0) {
			output_scroll_offset--;
		}
	}
}

void LOG::operator()(const char* format, ...)
{
	char buf[DBGUI::MsgBufferSize];
	va_list msg;
	va_start(msg, format);
	vsnprintf(buf, sizeof(buf), format, msg);
	va_end(msg);

	if (d_type >= LOG_MAX) {
		return;
	}
	if ((d_severity != LOG_ERROR) && (!loggrp[d_type].enabled)) {
		return;
	}
	DEBUG_ShowMsg("%10u: %s:%s\n",
	              static_cast<uint32_t>(cycle_count),
	              loggrp[d_type].front,
	              buf);
}

void LOG_Init()
{
	auto section = get_section("log");
	assert(section);

	std::string logfile = section->GetString("logfile");

	if (!logfile.empty() && (debuglog = fopen(logfile.c_str(), "wt+"))) {
		;
	} else {
		debuglog = nullptr;
	}

	char buf[DBGUI::LogNameBufferSize];

	// Skip LOG_ALL, it is always enabled
	for (Bitu i = LOG_ALL + 1; i < LOG_MAX; i++) {
		safe_strcpy(buf, loggrp[i].front);
		lowcase(buf);
		loggrp[i].enabled = section->GetBool(buf);
	}
}

void LOG_Destroy()
{
	if (debuglog) {
		fclose(debuglog);
	}
	debuglog = nullptr;
}

void LOG_StartUp()
{
	// Setup logging groups
	loggrp[LOG_ALL].front        = "ALL";
	loggrp[LOG_VGA].front        = "VGA";
	loggrp[LOG_VGAGFX].front     = "VGAGFX";
	loggrp[LOG_VGAMISC].front    = "VGAMISC";
	loggrp[LOG_INT10].front      = "INT10";
	loggrp[LOG_SB].front         = "SBLASTER";
	loggrp[LOG_DMACONTROL].front = "DMA_CONTROL";

	loggrp[LOG_FPU].front    = "FPU";
	loggrp[LOG_CPU].front    = "CPU";
	loggrp[LOG_PAGING].front = "PAGING";

	loggrp[LOG_FCB].front     = "FCB";
	loggrp[LOG_FILES].front   = "FILES";
	loggrp[LOG_IOCTL].front   = "IOCTL";
	loggrp[LOG_EXEC].front    = "EXEC";
	loggrp[LOG_DOSMISC].front = "DOSMISC";

	loggrp[LOG_PIT].front      = "PIT";
	loggrp[LOG_KEYBOARD].front = "KEYBOARD";
	loggrp[LOG_PIC].front      = "PIC";

	loggrp[LOG_MOUSE].front = "MOUSE";
	loggrp[LOG_BIOS].front  = "BIOS";
	loggrp[LOG_GUI].front   = "GUI";
	loggrp[LOG_MISC].front  = "MISC";

	loggrp[LOG_IO].front        = "IO";
	loggrp[LOG_PCI].front       = "PCI";
	loggrp[LOG_REELMAGIC].front = "REELMAGIC";

	// Register the log section
	auto sect = control->AddSection("log");

	PropString* pstring = sect->AddString("logfile",
	                                      Property::Changeable::Always,
	                                      "");

	pstring->SetHelp("Path of the log file.");

	char buf[DBGUI::LogNameBufferSize];
	for (Bitu i = LOG_ALL + 1; i < LOG_MAX; i++) {
		safe_strcpy(buf, loggrp[i].front);
		lowcase(buf);
		PropBool* pbool = sect->AddBool(buf, Property::Changeable::Always, true);
		pbool->SetHelp("Enable/disable logging of this type.");
	}
}

// Calculate window height for a given number of rows
static float CalcWindowHeight(int rows)
{
	float line_height      = ImGui::GetTextLineHeightWithSpacing();
	float title_bar_height = ImGui::GetFrameHeight();
	float padding          = ImGui::GetStyle().WindowPadding.y * 2;
	return (rows * line_height) + title_bar_height + padding;
}

// Calculate window width for a given number of columns
static float CalcWindowWidth(int cols)
{
	// Use a representative character to get monospace font width
	float char_width = ImGui::CalcTextSize("X").x;
	float padding    = ImGui::GetStyle().WindowPadding.x * 2;
	return (cols * char_width) + padding;
}

// Get the calculated Y positions for each window
float DBGUI_GetWindowY(int window_index)
{
	float y = 0;
	if (window_index > 0) {
		y += CalcWindowHeight(dbg.rows_registers); // After Registers
	}
	if (window_index > 1) {
		y += CalcWindowHeight(dbg.rows_data); // After Data
	}
	if (window_index > 2) {
		y += CalcWindowHeight(dbg.rows_code) +
		     DBGUI::WindowSeparatorSpacing; // After Code (with separator)
	}
	if (window_index > 3) {
		y += CalcWindowHeight(dbg.rows_variables); // After Variables
	}
	return y;
}

// Calculate total height needed for all windows
float DBGUI_GetTotalHeight()
{
	return DBGUI_GetWindowY(4) + CalcWindowHeight(dbg.rows_output);
}

// Calculate window width based on column count
float DBGUI_GetWindowWidth()
{
	return CalcWindowWidth(dbg.window_cols);
}

void DBGUI_StartUp(void)
{
	if (imgui_initialized) {
		return;
	}

	// Get display scale for high DPI support
	display_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
	if (display_scale <= 0.0f) {
		display_scale = 1.0f;
	}

	// Create debugger window with initial size (will be resized after ImGui
	// init). Use approximate pixel values before ImGui metrics are
	// available, scaled for high DPI displays.
	constexpr int InitialWindowWidth   = 800;
	constexpr int InitialWindowHeight  = 600;
	const SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE |
	                                     SDL_WINDOW_HIDDEN |
	                                     SDL_WINDOW_HIGH_PIXEL_DENSITY;

	dbg.win_main = SDL_CreateWindow(
	        "DOSBox Staging Debugger",
	        static_cast<int>(InitialWindowWidth * display_scale),
	        static_cast<int>(InitialWindowHeight * display_scale),
	        window_flags);

	if (!dbg.win_main) {
		LOG_ERR("DEBUG: Failed to create debugger window: %s",
		        SDL_GetError());
		return;
	}

	dbg.renderer = SDL_CreateRenderer(dbg.win_main, nullptr);
	if (!dbg.renderer) {
		LOG_ERR("DEBUG: Failed to create renderer: %s", SDL_GetError());
		SDL_DestroyWindow(dbg.win_main);
		dbg.win_main = nullptr;
		return;
	}

	// Setup ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.IniFilename = nullptr; // Disable saving/loading window layout

	// Setup ImGui style
	ImGui::StyleColorsDark();
	ImGuiStyle& style       = ImGui::GetStyle();
	style.WindowRounding    = DBGUI::WindowRounding;
	style.FrameRounding     = DBGUI::FrameRounding;
	style.ScrollbarRounding = DBGUI::ScrollbarRounding;
	style.FontSizeBase      = DBGUI::FontSize;

	// Scale style for high DPI displays
	style.ScaleAllSizes(display_scale);
	style.FontScaleDpi = display_scale;

	// Setup Platform/Renderer backends
	ImGui_ImplSDL3_InitForSDLRenderer(dbg.win_main, dbg.renderer);
	ImGui_ImplSDLRenderer3_Init(dbg.renderer);

	io.Fonts->AddFontFromMemoryCompressedTTF(IBM_VGA_8x16_compressed_data,
	                                         IBM_VGA_8x16_compressed_size);

	imgui_initialized = true;
	cycle_count       = 0;

	// Now that ImGui is initialized, resize the window to fit all child
	// windows. Need to do a dummy frame to get accurate font metrics.
	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	// Calculate window dimensions from character rows/columns
	dbg.window_width  = static_cast<int>(DBGUI_GetWindowWidth());
	dbg.window_height = static_cast<int>(DBGUI_GetTotalHeight());
	SDL_SetWindowSize(dbg.win_main, dbg.window_width, dbg.window_height);
	SDL_SetWindowPosition(dbg.win_main,
	                      SDL_WINDOWPOS_CENTERED,
	                      SDL_WINDOWPOS_CENTERED);
	SDL_ShowWindow(dbg.win_main);

	ImGui::EndFrame();
}

void DBGUI_Shutdown(void)
{
	if (!imgui_initialized) {
		return;
	}

	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

	if (dbg.renderer) {
		SDL_DestroyRenderer(dbg.renderer);
		dbg.renderer = nullptr;
	}

	if (dbg.win_main) {
		SDL_DestroyWindow(dbg.win_main);
		dbg.win_main = nullptr;
	}

	imgui_initialized = false;
}

void DBGUI_NewFrame(void)
{
	if (!imgui_initialized) {
		return;
	}

	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
}

void DBGUI_Render(void)
{
	if (!imgui_initialized) {
		return;
	}

	ImGui::Render();

	// Set render scale for high DPI displays
	ImGuiIO& io = ImGui::GetIO();
	SDL_SetRenderScale(dbg.renderer,
	                   io.DisplayFramebufferScale.x,
	                   io.DisplayFramebufferScale.y);

	SDL_SetRenderDrawColor(dbg.renderer,
	                       DBGUI::ClearColorR,
	                       DBGUI::ClearColorG,
	                       DBGUI::ClearColorB,
	                       DBGUI::ClearColorA);
	SDL_RenderClear(dbg.renderer);
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), dbg.renderer);
	SDL_RenderPresent(dbg.renderer);
}

// Title bar colors - cyan background with black text (classic DOS style)
static const ImVec4 TitleBgColor   = ImVec4(0.0f, 0.667f, 0.667f, 1.0f); // Cyan
static const ImVec4 TitleTextColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);     // Black

// Helper to begin a window with cyan title bar and black title text
static bool BeginWindowWithStyledTitle(const char* title, ImGuiWindowFlags flags)
{
	// Push title bar colors
	ImGui::PushStyleColor(ImGuiCol_TitleBg, TitleBgColor);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, TitleBgColor);
	ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, TitleBgColor);
	ImGui::PushStyleColor(ImGuiCol_Text, TitleTextColor);

	bool result = ImGui::Begin(title, nullptr, flags);

	// Restore text color for window content (keep title bar colors)
	ImGui::PopStyleColor(); // Pop text color

	return result;
}

// Helper to end a styled window
static void EndWindowWithStyledTitle()
{
	ImGui::End();
	ImGui::PopStyleColor(3); // Pop title bar colors
}

// Public versions for use from debugger.cpp
bool DBGUI_BeginWindowWithStyledTitle(const char* title, int flags)
{
	return BeginWindowWithStyledTitle(title, static_cast<ImGuiWindowFlags>(flags));
}

void DBGUI_EndWindowWithStyledTitle()
{
	EndWindowWithStyledTitle();
}

// Render functions for debugger windows - called from debugger.cpp
void DBGUI_DrawRegisterWindow(void)
{
	if (!imgui_initialized) {
		return;
	}

	// Calculate window dimensions based on character rows/columns
	float window_width  = DBGUI_GetWindowWidth();
	float window_height = CalcWindowHeight(dbg.rows_registers);

	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(window_width, window_height),
	                         ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Registers", nullptr, ImGuiWindowFlags_NoCollapse)) {
		// Row 1: EAX, ESI, DS, ES, FS, GS, SS
		ImGui::Text("EAX=%08X  ESI=%08X  DS=%04X  ES=%04X  FS=%04X  GS=%04X  SS=%04X",
		            reg_eax,
		            reg_esi,
		            SegValue(ds),
		            SegValue(es),
		            SegValue(fs),
		            SegValue(gs),
		            SegValue(ss));

		// Row 2: EBX, EDI, CS, EIP, Flags
		ImGui::Text("EBX=%08X  EDI=%08X  CS=%04X  EIP=%08X  C=%d Z=%d S=%d O=%d A=%d P=%d D=%d I=%d T=%d",
		            reg_ebx,
		            reg_edi,
		            SegValue(cs),
		            reg_eip,
		            GETFLAG(CF) ? 1 : 0,
		            GETFLAG(ZF) ? 1 : 0,
		            GETFLAG(SF) ? 1 : 0,
		            GETFLAG(OF) ? 1 : 0,
		            GETFLAG(AF) ? 1 : 0,
		            GETFLAG(PF) ? 1 : 0,
		            GETFLAG(DF) ? 1 : 0,
		            GETFLAG(IF) ? 1 : 0,
		            GETFLAG(TF) ? 1 : 0);

		// Row 3: ECX, EBP, IOPL, CPL
		ImGui::Text("ECX=%08X  EBP=%08X  IOPL=%d  CPL=%d",
		            reg_ecx,
		            reg_ebp,
		            (int)(GETFLAG(IOPL) >> 12),
		            (int)cpu.cpl);

		// Row 4: EDX, ESP, Mode, Cycles
		const char* mode_str = "Real";
		if (cpu.pmode) {
			if (GETFLAG(VM)) {
				mode_str = "VM86";
			} else if (cpu.code.big) {
				mode_str = "Pr32";
			} else {
				mode_str = "Pr16";
			}
		}
		ImGui::Text("EDX=%08X  ESP=%08X  %s  Cycles: %" PRIuPTR,
		            reg_edx,
		            reg_esp,
		            mode_str,
		            cycle_count);
	}
	ImGui::End();
}

void DBGUI_DrawOutputWindow(void)
{
	if (!imgui_initialized) {
		return;
	}

	// Calculate window dimensions based on character rows/columns
	float window_width  = DBGUI_GetWindowWidth();
	float window_height = CalcWindowHeight(dbg.rows_output);

	ImGui::SetNextWindowPos(ImVec2(0, DBGUI_GetWindowY(4)),
	                        ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(window_width, window_height),
	                         ImGuiCond_FirstUseEver);

	if (DBGUI_BeginWindowWithStyledTitle("-----(Output              Scroll: mousewheel,home/end)-----",
	                                ImGuiWindowFlags_NoCollapse |
	                                ImGuiWindowFlags_NoScrollbar |
	                                ImGuiWindowFlags_NoScrollWithMouse)) {
		// Handle mouse wheel scrolling when hovering over this window
		if (ImGui::IsWindowHovered()) {
			float wheel = ImGui::GetIO().MouseWheel;
			if (wheel > 0) {
				output_scroll_offset++; // Scroll up (older messages)
			} else if (wheel < 0 && output_scroll_offset > 0) {
				output_scroll_offset--; // Scroll down (newer messages)
			}
		}

		// Calculate how many lines we can display
		int visible_lines = dbg.rows_output - 1; // -1 for title bar
		int total_lines = static_cast<int>(logBuff.size());

		// Clamp scroll offset to valid range
		int max_offset = total_lines > visible_lines ? total_lines - visible_lines : 0;
		if (output_scroll_offset > max_offset) {
			output_scroll_offset = max_offset;
		}

		// Calculate start index (from the end, accounting for offset)
		int start_idx = total_lines - visible_lines - output_scroll_offset;
		if (start_idx < 0) {
			start_idx = 0;
		}

		// Display visible lines
		auto it = logBuff.begin();
		std::advance(it, start_idx);
		int lines_shown = 0;
		while (it != logBuff.end() && lines_shown < visible_lines) {
			ImGui::TextUnformatted(it->c_str());
			++it;
			++lines_shown;
		}
	}
	EndWindowWithStyledTitle();
}

#endif
