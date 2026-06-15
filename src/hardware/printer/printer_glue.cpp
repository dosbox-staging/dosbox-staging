// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "printer_glue.h"

#include "dosbox_config.h"

#include <memory>
#include <string>

#include "config/config.h"
#include "config/setup.h"
#include "gui/mapper.h"
#include "hardware/parallelport/lpt.h"
#include "hardware/port.h"
#include "misc/support.h"
#include "printer_if.h"
#include "utils/checks.h"

CHECK_NARROWING();

namespace {

struct PrinterState {
	std::unique_ptr<IO_WriteHandleObject> data_write;
	std::unique_ptr<IO_ReadHandleObject> data_read;
	std::unique_ptr<IO_ReadHandleObject> status_read;
	std::unique_ptr<IO_WriteHandleObject> control_write;
	std::unique_ptr<IO_ReadHandleObject> control_read;

	// Lifetimes for strings passed by raw pointer into printer.cpp's
	// PRINTER_Configure (which stores the pointers verbatim).
	std::string docpath;
	std::string output_format;

	bool active = false;
};

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

void PRINTER_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);
	auto section = conf->AddSection("printer");
	auto& s      = *static_cast<SectionProp*>(section);

	using enum Property::Changeable::Value;

	auto pbool = s.AddBool("printer", WhenIdle, false);
	pbool->SetHelp(
	        "Enable the virtual ESC/P2 dot-matrix printer ('off' by default).\n"
	        "Pages are written to files in the 'docpath' directory in the\n"
	        "format selected by 'printoutput'. The emulated printer responds\n"
	        "to Epson ESC/P2 escape sequences and produces text and graphics\n"
	        "output. Requires TTF font files (roman.ttf, sansserif.ttf,\n"
	        "courier.ttf, script.ttf, ocra.ttf) in the staging config dir.");

	auto pstring = s.AddString("printer_port", WhenIdle, "lpt1");
	pstring->SetHelp(
	        "Which parallel port the virtual printer attaches to ('lpt1' by\n"
	        "default). Most DOS apps target LPT1. Use 'lpt2' or 'lpt3' if\n"
	        "you also need an LPT DAC (Disney / Covox / Stereo-on-1) on LPT1.");
	pstring->SetValues({"lpt1", "lpt2", "lpt3"});

	auto pint = s.AddInt("dpi", WhenIdle, 360);
	pint->SetHelp("Output resolution in dots per inch (default 360).");

	pint = s.AddInt("width", WhenIdle, 85);
	pint->SetHelp("Paper width in tenths of an inch (default 85 = 8.5\").");

	pint = s.AddInt("height", WhenIdle, 110);
	pint->SetHelp("Paper height in tenths of an inch (default 110 = 11\").");

	pstring = s.AddString("printoutput", WhenIdle, "png");
	pstring->SetHelp(
	        "Output file format ('png' by default). Possible values:\n"
	        "\n"
	        "  png:  One PNG file per page.\n"
	        "  ps:   PostScript Level 2; multipage docs go in one .ps file\n"
	        "        when 'multipage = on'.");
	pstring->SetValues({"png", "ps"});

	pstring = s.AddString("docpath", WhenIdle, ".");
	pstring->SetHelp(
	        "Directory where output pages are written (default: current\n"
	        "working directory).");

	pbool = s.AddBool("multipage", WhenIdle, false);
	pbool->SetHelp(
	        "Combine all pages from a single print job into one PostScript\n"
	        "file ('off' by default). Press Ctrl+F2 to close the document.");

	pint = s.AddInt("timeout", WhenIdle, 500);
	pint->SetHelp(
	        "Inactivity timeout in milliseconds after which an unfinished\n"
	        "page is auto-ejected (default 500ms). Set to 0 to disable.");
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

	if (!section.GetBool("printer")) {
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

	state.docpath       = section.GetString("docpath");
	state.output_format = section.GetString("printoutput");

	const auto dpi       = static_cast<uint16_t>(section.GetInt("dpi"));
	const auto width     = static_cast<uint16_t>(section.GetInt("width"));
	const auto height    = static_cast<uint16_t>(section.GetInt("height"));
	const auto timeout = section.GetInt("timeout");

	PRINTER_Configure(dpi,
	                  width,
	                  height,
	                  state.docpath.c_str(),
	                  state.output_format.c_str(),
	                  timeout);

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

	LOG_MSG("PRINTER: Initialised virtual ESC/P2 printer on %s (%03xh)",
	        port_name.c_str(),
	        lpt_port);
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
