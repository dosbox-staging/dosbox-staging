// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "printer_glue.h"

#include "dosbox_config.h"

#if C_PRINTER

#include <cstdlib>
#include <memory>
#include <regex>
#include <string>

#include "config/config.h"
#include "config/setup.h"
#include "gui/mapper.h"
#include "hardware/lpt.h"
#include "hardware/port.h"
#include "misc/logging.h"
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
        {"a3",      29.7 / 2.54, 42.0 / 2.54},
        {"a4",      21.0 / 2.54, 29.7 / 2.54},
        {"a5",      14.8 / 2.54, 21.0 / 2.54},
        {"letter",  8.5,         11.0},
        {"legal",   8.5,         14.0},
        {"tabloid", 11.0,        17.0},
        {"fanfold", 14.875,      11.0},
};

// Parse a printer_page_size string. Accepts case-insensitive preset
// names (A3, A4, A5, Letter, Legal, Tabloid, Fanfold) or explicit
// dimensions in the form "<W>x<H>cm" or "<W>x<H>inch" / "<W>x<H>in".
// Returns true on success, with width_in / height_in set to inches.
bool parse_page_size(const std::string& input, double& width_in,
                     double& height_in)
{
	std::string lower = input;
	for (auto& c : lower) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}

	for (const auto& preset : PageSizePresets) {
		if (lower == preset.name) {
			width_in  = preset.width_in;
			height_in = preset.height_in;
			return true;
		}
	}

	// Explicit dimensions: "<num>x<num>cm" or "<num>x<num>inch|in".
	static const std::regex re(
	        R"(^\s*([0-9]+(?:\.[0-9]+)?)\s*x\s*([0-9]+(?:\.[0-9]+)?)\s*(cm|inch|in)\s*$)",
	        std::regex::optimize);
	std::smatch m;
	if (!std::regex_match(lower, m, re)) {
		return false;
	}
	const double w    = std::stod(m[1].str());
	const double h    = std::stod(m[2].str());
	const auto& unit  = m[3].str();
	const double mult = (unit == "cm") ? (1.0 / 2.54) : 1.0;

	width_in  = w * mult;
	height_in = h * mult;

	// Sanity check — reject zero, negative, or absurd values.
	if (width_in <= 0.0 || height_in <= 0.0 || width_in > 50.0 ||
	    height_in > 50.0) {
		return false;
	}
	return true;
}

PrinterModelKind parse_printer_model(const std::string& s)
{
	if (s == "epson_dot_matrix") {
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

// Forward declaration: the change-handler reuses Init/Destroy.
void PRINTER_Init();
void PRINTER_Destroy();

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
	s.AddUpdateHandler(
	        [](SectionProp& /*sec*/, const std::string& prop_name) {
		        LOG_MSG("PRINTER: Configuration changed (%s), "
		                "re-initialising",
		                prop_name.c_str());
		        PRINTER_Destroy();
		        PRINTER_Init();
	        });

	auto pstring = s.AddString("printer_model", WhenIdle, "none");
	pstring->SetHelp(
	        "Which kind of virtual printer to emulate.\n"
	        "\n"
	        "  none:             Printer disabled. (default)\n"
	        "  epson_dot_matrix: Emulate an Epson dot-matrix printer\n"
	        "                    (ESC/P and ESC/P 2 dialects). Best fit\n"
	        "                    for the Epson LQ-870 driver in DOS apps.\n"
	        "                    Writes one PNG per page.\n"
	        "  postscript:       Passthrough PostScript printer. The DOS\n"
	        "                    app sends raw PostScript; we save it\n"
	        "                    verbatim to a .ps file. Use this when\n"
	        "                    your DOS app targets a PostScript printer\n"
	        "                    (Apple LaserWriter, generic PostScript).");
	pstring->SetValues({"none", "epson_dot_matrix", "postscript"});

	pstring = s.AddString("printer_port", WhenIdle, "lpt1");
	pstring->SetHelp(
	        "Which parallel port the virtual printer attaches to. Most\n"
	        "DOS apps target LPT1. Use 'lpt2' or 'lpt3' if you also need\n"
	        "an LPT DAC (Disney / Covox / Stereo-on-1) on LPT1.");
	pstring->SetValues({"lpt1", "lpt2", "lpt3"});

	auto pint = s.AddInt("printer_dpi", WhenIdle, 360);
	pint->SetHelp(
	        "Only applies when printer_model = epson_dot_matrix. Ignored\n"
	        "in PostScript mode (the PostScript file defines its own\n"
	        "resolution).\n"
	        "\n"
	        "Output resolution in dots per inch (default 360). Higher\n"
	        "values produce sharper output and larger PNG files.");

	pstring = s.AddString("printer_page_size", WhenIdle, "A4");
	pstring->SetHelp(
	        "Only applies when printer_model = epson_dot_matrix. Ignored\n"
	        "in PostScript mode (the PostScript file defines its own\n"
	        "page size).\n"
	        "\n"
	        "Output page size. Accepts a named preset or explicit\n"
	        "dimensions. Presets:\n"
	        "\n"
	        "  A3       (29.7x42 cm) -- wide-carriage Epson only.\n"
	        "  A4       (21x29.7 cm) -- the European default.\n"
	        "  A5       (14.8x21 cm).\n"
	        "  Letter   (8.5x11 inch) -- US default.\n"
	        "  Legal    (8.5x14 inch) -- long US.\n"
	        "  Tabloid  (11x17 inch) -- wide US, ~A3.\n"
	        "  Fanfold  (14.875x11 inch) -- 'green-bar' tractor-feed\n"
	        "           continuous paper, the classic DOS-era format.\n"
	        "\n"
	        "Or specify exact dimensions, e.g. '21x29.7cm',\n"
	        "'8.5x11inch', '9.5x11inch' (narrow tractor-feed).");

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

	const auto dpi      = static_cast<uint16_t>(section.GetInt("printer_dpi"));
	const auto size_str = section.GetString("printer_page_size");
	const auto out_dir  = section.GetString("printer_output_dir");
	const auto timeout  = section.GetInt("printer_timeout");

	double page_w_in = 0.0;
	double page_h_in = 0.0;
	if (!parse_page_size(size_str, page_w_in, page_h_in)) {
		LOG_WARNING(
		        "PRINTER: Invalid printer_page_size '%s'; falling back to A4."
		        " Use a preset (A3, A4, A5, Letter, Legal, Tabloid, Fanfold)"
		        " or an explicit '<W>x<H>cm' / '<W>x<H>inch' value.",
		        size_str.c_str());
		parse_page_size("A4", page_w_in, page_h_in);
	}

	PRINTER_Configure(model, dpi, page_w_in, page_h_in, out_dir, timeout);

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

	const char* model_name = (model == PrinterModelKind::EpsonDotMatrix)
	                                 ? "Epson dot-matrix"
	                                 : "PostScript passthrough";

	if (model == PrinterModelKind::EpsonDotMatrix) {
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

#else // !C_PRINTER

void PRINTER_AddConfigSection(const ConfigPtr&) {}
void PRINTER_Init() {}
void PRINTER_Destroy() {}

#endif
