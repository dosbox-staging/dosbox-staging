// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "printer_glue.h"

#include "dosbox_config.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "config/config.h"
#include "config/setup.h"
#include "gui/mapper.h"
#include "hardware/parallelport/lpt.h"
#include "hardware/port.h"
#include "misc/logging.h"
#include "misc/notifications.h"
#include "misc/support.h"
#include "printer_if.h"
#include "utils/checks.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

namespace {

struct PrinterState {
	std::unique_ptr<IO_WriteHandleObject> data_write;
	std::unique_ptr<IO_ReadHandleObject> data_read;
	std::unique_ptr<IO_ReadHandleObject> status_read;
	std::unique_ptr<IO_WriteHandleObject> control_write;
	std::unique_ptr<IO_ReadHandleObject> control_read;

	bool active = false;
};

// Recognised page-size presets in inches. The "common DOS-era sizes
// users actually saw" list — A3/Tabloid/Fanfold are wide-carriage
// (LQ-1170, FX-100, FX-1050, etc.); the others fit any model.
struct PageSizePreset {
	std::string_view name;
	double width_in;
	double height_in;
};

constexpr PageSizePreset PageSizePresets[] = {
        {     "a3", 29.7 / 2.54, 42.0 / 2.54},
        {     "a4", 21.0 / 2.54, 29.7 / 2.54},
        {     "a5", 14.8 / 2.54, 21.0 / 2.54},
        { "letter",         8.5,        11.0},
        {  "legal",         8.5,        14.0},
        {"tabloid",        11.0,        17.0},
        {"fanfold",      14.875,        11.0},
};

// Parse a printer_page_size string. Accepts case-insensitive preset
// names (a3, a4, a5, letter, legal, tabloid, fanfold) or explicit
// dimensions in the form "<W>x<H>cm" or "<W>x<H>inch" / "<W>x<H>in".
// An optional trailing " landscape" / " portrait" word swaps the
// dimensions to satisfy the requested orientation. With no suffix
// the dimensions are used as given.
// Returns true on success, with width_in / height_in set to inches.
bool parse_page_size(const std::string& input, double& width_in, double& height_in)
{
	std::string lower = input;
	lowcase(lower);

	auto tokens = split(lower);
	if (tokens.empty()) {
		return false;
	}

	enum class Orient { AsIs, Landscape, Portrait };
	auto orient = Orient::AsIs;

	if (tokens.back() == "landscape") {
		orient = Orient::Landscape;
		tokens.pop_back();

	} else if (tokens.back() == "portrait") {
		orient = Orient::Portrait;
		tokens.pop_back();
	}

	if (tokens.empty()) {
		return false;
	}

	// Re-join the (possibly multi-token, whitespace-tolerant) size part
	// into a single string for preset / dimension matching.
	std::string size_str;
	for (const auto& t : tokens) {
		size_str += t;
	}

	bool parsed = false;
	for (const auto& preset : PageSizePresets) {
		if (size_str == preset.name) {
			width_in  = preset.width_in;
			height_in = preset.height_in;
			parsed    = true;
			break;
		}
	}

	if (!parsed) {
		// Explicit dimensions: "<W>x<H><unit>" where unit is cm, inch
		// or in. Strip the unit from the end first.
		double mult = 0.0;
		if (size_str.ends_with("cm")) {
			size_str.resize(size_str.size() - 2);
			mult = 1.0 / 2.54;

		} else if (size_str.ends_with("inch")) {
			size_str.resize(size_str.size() - 4);
			mult = 1.0;

		} else if (size_str.ends_with("in")) {
			size_str.resize(size_str.size() - 2);
			mult = 1.0;

		} else {
			return false;
		}

		// Split on 'x' to get width and height.
		const auto parts = split(size_str, "x");
		if (parts.size() != 2) {
			return false;
		}
		const auto w = parse_float(parts[0]);
		const auto h = parse_float(parts[1]);
		if (!w || !h) {
			return false;
		}
		width_in  = *w * mult;
		height_in = *h * mult;
	}

	// 1 inch lower bound (anything smaller is nonsense in practice);
	// 50 inch upper bound (well past any real paper).
	constexpr double MinDimIn = 1.0;
	constexpr double MaxDimIn = 50.0;

	if (width_in < MinDimIn || height_in < MinDimIn ||
	    width_in > MaxDimIn || height_in > MaxDimIn) {
		return false;
	}

	if (orient == Orient::Landscape && width_in < height_in) {
		std::swap(width_in, height_in);

	} else if (orient == Orient::Portrait && height_in < width_in) {
		std::swap(width_in, height_in);
	}
	return true;
}

PrinterModelKind parse_printer_model(const std::string& s)
{
	if (s == "epson-24pin") {
		return PrinterModelKind::EpsonDotMatrix;
	}
	if (s == "postscript") {
		return PrinterModelKind::PostScript;
	}
	return PrinterModelKind::None;
}

PrinterState state{};

io_port_t lpt_port_for_name(const std::string& name)
{
	if (name == "lpt2") {
		return Lpt2Port;
	}
	if (name == "lpt3") {
		return Lpt3Port;
	}
	return Lpt1Port;
}

void install_io_handlers(const io_port_t lpt_port)
{
	const auto data_port    = lpt_port;
	const auto status_port  = static_cast<io_port_t>(lpt_port + 1u);
	const auto control_port = static_cast<io_port_t>(lpt_port + 2u);

	state.data_write    = std::make_unique<IO_WriteHandleObject>();
	state.data_read     = std::make_unique<IO_ReadHandleObject>();
	state.status_read   = std::make_unique<IO_ReadHandleObject>();
	state.control_write = std::make_unique<IO_WriteHandleObject>();
	state.control_read  = std::make_unique<IO_ReadHandleObject>();

	state.data_write->Install(
	        data_port,
	        [](io_port_t /*port*/, io_val_t val, io_width_t /*width*/) {
		        PRINTER_WriteData(0, val, 1);
	        },
	        io_width_t::byte);

	state.data_read->Install(
	        data_port,
	        [](io_port_t /*port*/, io_width_t /*width*/) -> io_val_t {
		        return static_cast<io_val_t>(PRINTER_ReadData(0, 1));
	        },
	        io_width_t::byte);

	state.status_read->Install(
	        status_port,
	        [](io_port_t /*port*/, io_width_t /*width*/) -> io_val_t {
		        return static_cast<io_val_t>(PRINTER_ReadStatus(0, 1));
	        },
	        io_width_t::byte);

	state.control_write->Install(
	        control_port,
	        [](io_port_t /*port*/, io_val_t val, io_width_t /*width*/) {
		        PRINTER_WriteControl(0, val, 1);
	        },
	        io_width_t::byte);

	state.control_read->Install(
	        control_port,
	        [](io_port_t /*port*/, io_width_t /*width*/) -> io_val_t {
		        return static_cast<io_val_t>(PRINTER_ReadControl(0, 1));
	        },
	        io_width_t::byte);
}

void uninstall_io_handlers()
{
	state.data_write.reset();
	state.data_read.reset();
	state.status_read.reset();
	state.control_write.reset();
	state.control_read.reset();
}

void formfeed_handler(bool pressed)
{
	PRINTER_FormFeed(pressed);
}

bool mapper_handler_registered = false;

} // namespace

// PRINTER_Init and PRINTER_Destroy are declared in printer_glue.h
// (included at the top of this file) — the change-handler reuses them.

void PRINTER_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);
	auto section = conf->AddSection("printer");
	auto& s      = *static_cast<SectionProp*>(section);

	using enum Property::Changeable::Value;

	// Any printer-section property change at the DOS prompt
	// re-initialises the whole printer subsystem. The user is
	// expected not to reconfigure mid-print (same contract as
	// turning off a real printer mid-job: the in-flight page
	// is lost).
	s.AddUpdateHandler([](SectionProp& /*sec*/, const std::string& prop_name) {
		LOG_MSG("PRINTER: Configuration changed (%s), "
		        "re-initialising",
		        prop_name.c_str());
		PRINTER_Destroy();
		PRINTER_Init();
	});

	auto pstring = s.AddString("printer_model", WhenIdle, "none");
	pstring->SetHelp(
	        "Type of virtual printer to emulate ('none' by default).\n"
	        "\n"
	        "  none:         Printer disabled. (default)\n"
	        "\n"
	        "  epson-24pin:  24-pin Epson LQ-series dot-matrix printer (ESC/P and ESC/P 2\n"
	        "                dialects). Best fit for the Epson LQ-870 / LQ-850 / LQ-550\n"
	        "                drivers. Writes one PNG per page.\n"
	        "\n"
	        "  postscript:   Saves the raw PostScript data sent by the program to a file. Use\n"
	        "                this when your programs targets a PostScript printer (e.g.,\n"
	        "                Apple LaserWriter, or generic PostScript).");
	pstring->SetValues({"none", "epson-24pin", "postscript"});

	pstring = s.AddString("printer_port", WhenIdle, "lpt1");
	pstring->SetHelp(
	        "Parallel port the virtual printer is attached to ('lpt1' by default).\n"
	        "Most DOS programs target LPT1. Use 'lpt2' or 'lpt3' if you also need an LPT DAC\n"
	        "(see 'lpt_dac')."
	        "an LPT DAC (Disney / Covox / Stereo-on-1) on LPT1.");
	pstring->SetValues({"lpt1", "lpt2", "lpt3"});

	auto pint = s.AddInt("printer_dpi", WhenIdle, 360);
	pint->SetHelp(
	        "Output resolution of the virtual printer in dots per inch (360 by default).\n"
	        "Valid range is 60-1200. Higher values produce sharper output and larger PNG\n"
	        "files.\n"
	        "\n"
	        "Note:  Only applies to the 'epson-24pin' printer model to set the size of the\n"
	        "       PNG output image.");

	pstring = s.AddString("printer_page_size", WhenIdle, "a4");
	pstring->SetHelp(
	        "Output page size of the virtual printer ('a4' by default). Accepts a named\n"
	        "preset or explicit dimensions with an optional orientation suffix. Append\n"
	        "'landscape' or 'portrait' to specify the orientation (e.g. 'a4 landscape'),\n"
	        "defaults to 'portrait' if unspecified. Presets:\n"
	        "\n"
	        "  a3:       Wide-carriage Epson only (29.7x42 cm)\n"
	        "  a4:       The European default (21x29.7 cm)\n"
	        "  a5:       Half an A4 page (14.8x21 cm)\n"
	        "\n"
	        "  letter:   US default (8.5x11 inch)\n"
	        "  legal:    Long US (8.5x14 inch)\n"
	        "  tabloid:  Wide US (11x17 inch)\n"
	        "\n"
	        "  fanfold:  Tractor-feed continuous paper, the classic DOS-era format\n"
	        "            (14.875x11 inch)\n"
	        "\n"
	        "  WxHcm:    Exact dimensions in centimeters (e.g., 29.7x42cm)\n"
	        "\n"
	        "  WxHin:    Exact dimensions in inches (e.g., 8.5x14inch)\n"
	        "  WxHinch:\n"
	        "\n"
	        "Note:  Only applies to the 'epson-24pin' printer model to set the size of the\n"
	        "       PNG output image.");

	pstring = s.AddString("printer_output_dir", WhenIdle, ".");
	pstring->SetHelp(
	        "Directory where output files are written (PNGs in dot-matrix\n"
	        "mode, .ps files in PostScript mode). Default is the current\n"
	        "working directory. The directory is auto-created if it\n"
	        "doesn't exist.");

	pint = s.AddInt("printer_timeout", WhenIdle, 500);
	pint->SetHelp(
	        "Inactivity timeout in milliseconds after which an unfinished\n"
	        "page is auto-ejected. Default 500ms. Set to 0 to disable\n"
	        "auto-eject; you can also eject manually via the 'ejectpage'\n"
	        "mapper action (default Ctrl+F2, fully remappable).");
}

void PRINTER_Init()
{
	if (state.active) {
		return;
	}

	const auto section_base = get_section("printer");
	if (!section_base) {
		return;
	}
	auto& section = *section_base;

	const auto model_str = section.GetString("printer_model");
	const auto model     = parse_printer_model(model_str);
	if (model == PrinterModelKind::None) {
		return;
	}

	const auto port_name = section.GetString("printer_port");
	const auto lpt_port  = lpt_port_for_name(port_name);

	if (IO_HasWriteHandler(lpt_port)) {
		LOG_WARNING(
		        "PRINTER: Port %s (%03xh) is already in use by another device;"
		        " printer disabled. Set 'printer_port' to a free LPT port.",
		        port_name.c_str(),
		        lpt_port);
		return;
	}

	constexpr int MinDpi     = 60;
	constexpr int MaxDpi     = 1200;
	constexpr int DefaultDpi = 360;

	auto dpi = section.GetInt("printer_dpi");
	if (dpi < MinDpi || dpi > MaxDpi) {
		const auto bad = std::to_string(dpi);
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "PRINTER",
		                      "PROGRAM_CONFIG_INVALID_SETTING",
		                      "printer_dpi",
		                      bad.c_str(),
		                      "360");
		dpi = DefaultDpi;
	}
	const auto dpi_u16  = static_cast<uint16_t>(dpi);
	const auto size_str = section.GetString("printer_page_size");
	const auto out_dir  = section.GetString("printer_output_dir");
	const auto timeout  = section.GetInt("printer_timeout");

	double page_w_in = 0.0;
	double page_h_in = 0.0;
	if (!parse_page_size(size_str, page_w_in, page_h_in)) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "PRINTER",
		                      "PROGRAM_CONFIG_INVALID_SETTING",
		                      "printer_page_size",
		                      size_str.c_str(),
		                      "a4");
		parse_page_size("a4", page_w_in, page_h_in);
	}

	PRINTER_Configure(model, dpi_u16, page_w_in, page_h_in, out_dir, timeout);

	install_io_handlers(lpt_port);

	if (!mapper_handler_registered) {
		MAPPER_AddHandler(formfeed_handler,
		                  SDL_SCANCODE_F2,
		                  PRIMARY_MOD,
		                  "ejectpage",
		                  "Eject page");
		mapper_handler_registered = true;
	}

	state.active = true;

	const char* model_name = "";
	switch (model) {
	case PrinterModelKind::EpsonDotMatrix:
		model_name = "Epson dot-matrix (24-pin)";
		break;

	case PrinterModelKind::PostScript:
		model_name = "PostScript passthrough";
		break;

	case PrinterModelKind::None: break;
	}

	const bool is_dot_matrix = (model == PrinterModelKind::EpsonDotMatrix);
	if (is_dot_matrix) {
		LOG_MSG("PRINTER: Initialised %s on %s (%03xh), %dx%d page at %d dpi, output dir %s",
		        model_name,
		        port_name.c_str(),
		        lpt_port,
		        static_cast<int>(page_w_in * 100 + 0.5),
		        static_cast<int>(page_h_in * 100 + 0.5),
		        dpi,
		        out_dir.c_str());
	} else {
		LOG_MSG("PRINTER: Initialised %s on %s (%03xh), output dir %s",
		        model_name,
		        port_name.c_str(),
		        lpt_port,
		        out_dir.c_str());
	}
}

void PRINTER_Destroy()
{
	if (!state.active) {
		return;
	}

	PRINTER_Reset();

	uninstall_io_handlers();
	state.active = false;

	// Note: staging's mapper API has no MAPPER_RemoveHandler counterpart,
	// so the Ctrl+F2 binding stays registered; subsequent Init calls
	// reuse it.
}
