// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/intel8042.h"

#include "config/config.h"
#include "dosbox.h"
#include "dosbox_config.h"
#include "hardware/memory.h"
#include "hardware/pic.h"
#include "hardware/port.h"
#include "hardware/vmware.h"
#include "utils/bit_view.h"
#include "utils/bitops.h"
#include "utils/checks.h"

CHECK_NARROWING();

// Emulates the Intel 8042 keyboard/mouse controller.

// Reference:
// - https://wiki.osdev.org/%228042%22_PS/2_Controller
// - https://stanislavs.org/helppc/8042.html
// - https://homepages.cwi.nl/~aeb/linux/kbd/scancodes.html
// - http://www-ug.eecg.toronto.edu/msl/nios_devices/datasheets/PS2%20Keyboard%20Protocol.htm
// - https://k.lse.epita.fr/data/8042.pdf (SMSC KBD43W13 whitepaper)
// - https://tvsat.com.pl/PDF/W/W83C42P_win.pdf (Winbond W83C42 whitepaper)
// - http://www.os2museum.com/wp/ibm-pcat-8042-keyboard-controller-commands/
// - http://www.os2museum.com/wp/ibm-ps2-model-50-keyboard-controller/

static constexpr uint8_t IrqNumKbdPcjr  = 6;
static constexpr uint8_t IrqNumKbdIbmPc = 1;
static constexpr uint8_t IrqNumMouse    = 12;

constexpr uint8_t        FirmwareRevision  = 0x00;
static const std::string FirmwareCopyright = DOSBOX_COPYRIGHT;

static constexpr uint8_t BufferSize = 64; // in bytes
// delay appropriate for 20-30 kHz serial clock and 11 bits/byte
static constexpr double PortDelayMs = 0.300;

// Port operation width to be possibly taken over by the VMware interface
static const auto WidthVmWare = io_width_t::dword;

enum class Command : uint8_t { // PS/2 mouse/keyboard controller commands
	None = 0x00,

	// Note: some obsolete commands (or even usually available ones)
	// might have a different meaning on certain old machines! The
	// following known ones are completely skipped from implementation:
	//
	// Compaq BIOS:
	//     0xa3: enable system speed control
	//     0xa4: toggle speed
	//     0xa5: special read of P2
	// ISA/EISA systems with AMI BIOS:
	//     0xa2: set lines P22 and P23 low
	//     0xa3: set lines P22 and P23 high
	//           Commands 0xa2 and 0xa3 are used for speed
	//           switching. They return a garbage byte.
	//     0xa4: set clock line low
	//     0xa5: set clock line high
	//     0xa6: read clock state, 0 = low, 1 = high
	//     0xa7: 'write cache bad' (unclear what it does)
	//     0xa8: 'write cache good' (unclear what it does)
	//     0xc8: blocks bits 2 and 3 of port P2 for writing
	//           using command 0xd1
	//     0xc9  unblock the bits blocked by command 0xc8
	// MCA:
	//     0xa5: loads the password, in scancode format,
	//	         terminated by NUL, via port 0x60
	//     0xa6: check password, enable access if success
	//           Password functionality not implemented on purpose,
	//           as it puts the controller in a state some software
	//           (possibly expecting some incompatible vendor-specific
	//           extension here) might be unable to recover from.
	//           Besides, we are reporting no password is installed.
	// MCA, controller type 1 only
	//     0xc1: input port low nibble (bits 0-3) polling
	//     0xc2: input port high nibble (bits 0-3) polling
	//           Continuous copy of bits 0-3 or 4-7 of the input
	//           port to bits 4-7 of port 0x64, until the next
	//           command. Dangerous - rare extension, puts the
	//           controller in a state some software (possibly
	//           expecting some incompatible vendor-specific
	//           extension here) might be unable to recover from.
	// Various:
	//     0xb0-0xbd: in general manipulate keyboard controller lines,
	//                different meanings for different manufacturers

	// Below is the list of controller commands recognized
	// by this emulator; not all of them are implemented, though

	// Controller memory read/write, most don't have enum values:
	// 0x00-0x1f: aliases for 0x20-0x3f - obsolete, AMI BIOS
	// 0x20-0x3f: memory read           - obsolete except for 0x20
	// 0x40-0x5f: aliases for 0x60-0x7f - obsolete, AMI BIOS
	// 0x60-0x7f: memory write          - obsolete except for 0x60
	// Note: at least on some systems the aliased memory read/write uses
	// value from byte index 0x02 as offset, adding it to low 5 bits of
	// a command!

	ReadByteConfig      = 0x20, // usually available
	WriteByteConfig     = 0x60, // usually available
	ReadFwCopyright     = 0xa0, // obsolete, some controllers only
	ReadFwRevision      = 0xa1, // obsolete, some controllers only
	PasswordCheck       = 0xa4, // obsolete, MCA, some other controllers
	DisablePortAux      = 0xa7, // usually available
	EnablePortAux       = 0xa8, // usually available
	TestPortAux         = 0xa9, // usually available
	TestController      = 0xaa, // usually available
	TestPortKbd         = 0xab, // usually available
	DiagnosticDump      = 0xac, // obsolete, some controllers only
	DisablePortKbd      = 0xad, // usually available
	EnablePortKbd       = 0xae, // usually available
	ReadKbdVersion      = 0xaf, // obsolete, some controllers only
	ReadInputPort       = 0xc0, // usually available
	ReadControllerMode  = 0xca, // obsolete, AMI BIOS, VIA
	WriteControllerMode = 0xcb, // obsolete, AMI BIOS
	ReadOutputPort      = 0xd0, // usually available
	WriteOutputPort     = 0xd1, // usually available
	SimulateInputKbd    = 0xd2, // usually available
	SimulateInputAux    = 0xd3, // usually available
	DisableA20          = 0xdd, // obsolete, HP Vectra
	EnableA20           = 0xdf, // obsolete, HP Vectra
	ReadTestInputs      = 0xe0, // usually available
	WriteAux            = 0xd4, // usually available
	// 0xf0-0xff: pulsing lines, 0xf0 usually available, remaing obsolete
};

// Byte 0x00 of the controller memory - configuration byte

static union {
	uint8_t data = 0b0000'0111;

	// bit 0: 1 = byte from keyboard triggers IRQ
	bit_view<0, 1> is_irq_active_kbd;
	// bit 1: 1 = byte from aux (mouse) triggers IRQ
	bit_view<1, 1> is_irq_active_aux;
	// bit 2: 1 = controller self test passed, 0 = cold boot
	bit_view<2, 1> passed_self_test;
	// bit 3: reserved, should be 0
	bit_view<3, 1> reserved_bit_3;

	// bit 4: 1 = keyboard port disabled
	bit_view<4, 1> is_disabled_kbd;
	// bit 5: 1 = aux (mouse) port disabled
	bit_view<5, 1> is_disabled_aux;
	// bit 6: 1 = keyboard input should be translated for XT comaptibility
	bit_view<6, 1> uses_kbd_translation;
	// bit 7: reserved, should be 0
	bit_view<7, 1> reserved_bit_7;

} config_byte;

static constexpr void sanitize_config_byte()
{
	config_byte.passed_self_test = true;

	config_byte.reserved_bit_3 = 0;
	config_byte.reserved_bit_7 = 0;
}

static auto& passed_self_test     = config_byte.passed_self_test;
static auto& is_irq_active_kbd    = config_byte.is_irq_active_kbd;
static auto& is_irq_active_aux    = config_byte.is_irq_active_aux;
static auto& is_disabled_kbd      = config_byte.is_disabled_kbd;
static auto& is_disabled_aux      = config_byte.is_disabled_aux;
static auto& uses_kbd_translation = config_byte.uses_kbd_translation;

static bool is_diagnostic_dump = false;

// Byte returned from port 0x60

static uint8_t data_byte = 0;

// Byte returned from port 0x64

static union {
	uint8_t data = 0b0001'1100;

	// bit 0: 1 = new byte is waiting in 0x60
	bit_view<0, 1> is_data_new;
	// bit 1: input buffer status, 0 = guest can write to 0x60 or 0x64
	// bit 2: 1 = POST has already passed since power on
	// bit 3: 0 = last write was to 0x60, 1 = to 0x64
	bit_view<3, 1> was_last_write_cmd;

	// bit 4: 0 = keyboard locked (by external switch)
	// bit 5: 1 = data byte in 0x60 is from AUX
	bit_view<5, 1> is_data_from_aux;
	// bit 6: 1 = timeout error during data transmission
	bit_view<6, 1> is_transmit_timeout;
	// bit 7: 1 = parity error

} status_byte;

static auto& is_data_new      = status_byte.is_data_new;
static auto& is_data_from_aux = status_byte.is_data_from_aux;
static bool is_data_from_kbd  = false; // not present in status byte

static auto& is_transmit_timeout = status_byte.is_transmit_timeout;

// Controller internal buffer

static struct {
	uint8_t data = 0;

	bool is_from_aux = false;
	bool is_from_kbd = false;
	bool skip_delay  = false;

} buffer[BufferSize];

static size_t buffer_start_idx = 0;
static size_t buffer_num_used  = 0;

static size_t waiting_bytes_from_aux = 0;
static size_t waiting_bytes_from_kbd = 0;

// true = delay timer is in progress
static bool delay_running = false;
// true = delay timer expired, event can be sent immediately
static bool delay_expired = true;

// Executing command, do not notify devices about readiness for accepting frame
static bool should_skip_device_notify = false;

// Command currently being executed, waiting for parameter
static Command current_command = Command::None;

// ***************************************************************************
// Helper routines to log various warnings
// ***************************************************************************

static void warn_buffer_full()
{
	static constexpr uint32_t threshold_ms = 15 * 1000; // 15 seconds

	static bool already_warned = false;
	static uint32_t last_timestamp = 0;

	if (!already_warned || (PIC_Ticks - last_timestamp > threshold_ms)) {
		LOG_WARNING("I8042: Internal buffer overflow");
		last_timestamp = PIC_Ticks;
		already_warned = true;
	}
}

static void warn_controller_mode()
{
	static bool already_warned = false;
	if (!already_warned) {
		LOG_WARNING("I8042: Switching controller to AT mode not emulated");
		already_warned = true;
	}
}

static void warn_internal_ram_access()
{
	static bool already_warned = false;
	if (!already_warned) {
		LOG_WARNING("I8042: Accessing internal RAM (other than byte 0x00) gives vendor-specific results");
		already_warned = true;
	}
}

static void warn_line_pulse()
{
	static bool already_warned = false;
	if (!already_warned) {
		LOG_WARNING("I8042: Pulsing line other than RESET not emulated");
		already_warned = true;
	}
}

static void warn_read_test_inputs()
{
	static bool already_warned = false;
	if (!already_warned) {
		LOG_WARNING("I8042: Reading test inputs not implemented");
		already_warned = true;
	}
}

static void warn_vendor_lines()
{
	static bool already_warned = false;
	if (!already_warned) {
		LOG_WARNING("I8042: No vendor-specific commands to manipulate controller lines are emulated");
		already_warned = true;
	}
}

static void warn_unknown_command(const Command command)
{
	static bool already_warned[UINT8_MAX + 1];
	const auto code = static_cast<uint8_t>(command);
	if (!already_warned[code]) {
		LOG_WARNING("I8042: Unknown command 0x%02x", code);
		already_warned[code] = true;
	}
}

// ***************************************************************************
// XT translation for keyboard input
// ***************************************************************************

static uint8_t get_translated(const uint8_t byte)
{
	// A drain bamaged keyboard input translation

	// Intended to make scancode set 2 compatible with software knowing
	// only scancode set 1. Translates every byte coming from the keyboard,
	// scancodes and command responses alike!

	// Values from 86Box source code, can also be found in many other places

	// clang-format off
	static constexpr uint8_t translation_table[] = {
		0xff, 0x43, 0x41, 0x3f, 0x3d, 0x3b, 0x3c, 0x58,
		0x64, 0x44, 0x42, 0x40, 0x3e, 0x0f, 0x29, 0x59,
		0x65, 0x38, 0x2a, 0x70, 0x1d, 0x10, 0x02, 0x5a,
		0x66, 0x71, 0x2c, 0x1f, 0x1e, 0x11, 0x03, 0x5b,
		0x67, 0x2e, 0x2d, 0x20, 0x12, 0x05, 0x04, 0x5c,
		0x68, 0x39, 0x2f, 0x21, 0x14, 0x13, 0x06, 0x5d,
		0x69, 0x31, 0x30, 0x23, 0x22, 0x15, 0x07, 0x5e,
		0x6a, 0x72, 0x32, 0x24, 0x16, 0x08, 0x09, 0x5f,
		0x6b, 0x33, 0x25, 0x17, 0x18, 0x0b, 0x0a, 0x60,
		0x6c, 0x34, 0x35, 0x26, 0x27, 0x19, 0x0c, 0x61,
		0x6d, 0x73, 0x28, 0x74, 0x1a, 0x0d, 0x62, 0x6e,
		0x3a, 0x36, 0x1c, 0x1b, 0x75, 0x2b, 0x63, 0x76,
		0x55, 0x56, 0x77, 0x78, 0x79, 0x7a, 0x0e, 0x7b,
		0x7c, 0x4f, 0x7d, 0x4b, 0x47, 0x7e, 0x7f, 0x6f,
		0x52, 0x53, 0x50, 0x4c, 0x4d, 0x48, 0x01, 0x45,
		0x57, 0x4e, 0x51, 0x4a, 0x37, 0x49, 0x46, 0x54,
		0x80, 0x81, 0x82, 0x41, 0x54, 0x85, 0x86, 0x87,
		0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
		0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
		0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
		0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
		0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
		0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
		0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
		0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
		0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
		0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
		0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
		0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
		0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
		0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
		0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
	};
	// clang-format on

	return translation_table[byte];
}

// ***************************************************************************
// Controller buffer support
// ***************************************************************************

static uint8_t get_irq_mouse()
{
	return IrqNumMouse;
}

static uint8_t get_irq_keyboard()
{
	if (is_machine_pcjr()) {
		return IrqNumKbdPcjr;
	} else {
		return IrqNumKbdIbmPc;
	}
}

static void activate_irqs_if_needed()
{
	if (is_data_from_aux && is_irq_active_aux) {
		PIC_ActivateIRQ(get_irq_mouse());
	}
	if (is_data_from_kbd && is_irq_active_kbd) {
		PIC_ActivateIRQ(get_irq_keyboard());
	}
}

static void flush_buffer()
{
	is_data_new      = false;
	is_data_from_aux = false;
	is_data_from_kbd = false;

	buffer_start_idx = 0;
	buffer_num_used  = 0;

	const bool should_notify_aux = !should_skip_device_notify &&
	                               !I8042_IsReadyForAuxFrame();
	const bool should_notify_kbd = !should_skip_device_notify &&
	                               !I8042_IsReadyForKbdFrame();

	waiting_bytes_from_aux = 0;
	waiting_bytes_from_kbd = 0;

	if (should_notify_aux && I8042_IsReadyForAuxFrame()) {
		MOUSEPS2_NotifyReadyForFrame();
	}

	if (should_notify_kbd && I8042_IsReadyForKbdFrame()) {
		KEYBOARD_NotifyReadyForFrame();
	}
}

static void enforce_buffer_space(const size_t num_bytes = 1)
{
	assert(num_bytes <= BufferSize);

	if (BufferSize < buffer_num_used + num_bytes) {
		warn_buffer_full();
		flush_buffer();
	}
}

static void maybe_transfer_buffer(); // forward declaration

static void delay_handler(uint32_t /*val*/)
{
	delay_running = false;
	delay_expired = true;

	maybe_transfer_buffer();
}

static void restart_delay_timer(const double time_ms = PortDelayMs)
{
	if (delay_running) {
		PIC_RemoveEvents(delay_handler);
	}
	PIC_AddEvent(delay_handler, time_ms);
	delay_running = true;
	delay_expired = false;
}

static void maybe_transfer_buffer()
{
	if (is_data_new || !buffer_num_used) {
		// There is already some data waiting to be picked up,
		// or there is nothing waiting in the buffer
		return;
	}

	// If not set to skip the delay, do not send byte until timer expires
	const auto idx = buffer_start_idx;
	if (!delay_expired && !buffer[idx].skip_delay) {
		return;
	}

	// Mark byte as consummed
	buffer_start_idx = (buffer_start_idx + 1) % BufferSize;
	--buffer_num_used;

	// Transfer one byte of data from buffer to output port
	data_byte        = buffer[idx].data;
	is_data_from_aux = buffer[idx].is_from_aux;
	is_data_from_kbd = buffer[idx].is_from_kbd;
	is_data_new      = true;
	restart_delay_timer();
	activate_irqs_if_needed();
}

static void buffer_add(const uint8_t byte,
                       const bool is_from_aux = false,
                       const bool is_from_kbd = false,
                       const bool skip_delay  = false)
{
	if ((is_from_aux && is_disabled_aux) || (is_from_kbd && is_disabled_kbd)) {
		// Byte came from a device which is currently disabled
		return;
	}

	if (buffer_num_used >= BufferSize) {
		warn_buffer_full();
		flush_buffer();
		return;
	}

	const size_t idx = (buffer_start_idx + buffer_num_used++) % BufferSize;

	if (is_from_kbd && uses_kbd_translation) {
		buffer[idx].data = get_translated(byte);
	} else {
		buffer[idx].data = byte;
	}
	buffer[idx].is_from_aux = is_from_aux;
	buffer[idx].is_from_kbd = is_from_kbd;
	buffer[idx].skip_delay  = skip_delay || (!is_from_aux && !is_from_kbd);

	if (is_from_aux) {
		++waiting_bytes_from_aux;
	}
	if (is_from_kbd) {
		++waiting_bytes_from_kbd;
	}

	maybe_transfer_buffer();
}

static void buffer_add_aux(const uint8_t byte, const bool skip_delay = false)
{
	const bool is_from_aux = true;
	const bool is_from_kbd = false;

	buffer_add(byte, is_from_aux, is_from_kbd, skip_delay);
}

static void buffer_add_kbd(const uint8_t byte)
{
	const bool is_from_aux = false;
	const bool is_from_kbd = true;

	buffer_add(byte, is_from_aux, is_from_kbd);
}

// ***************************************************************************
// Command handlers
// ***************************************************************************

static uint8_t get_input_port() // aka port P1
{
	static union {
		uint8_t data = 0b1010'0000;

		// bit 0: keyboard data in, ISA - unused
		// bit 1: mouse data in, ISA - unused
		// bit 2: ISA, EISA, PS/2 - unused
		//        MCA - 0 = keyboard has power, 1 = no power
		//        might be configured for clock switching
		// bit 3: ISA, EISA, PS/2 - unused
		//        might be configured for clock switching

		// bit 4: 0 = 512 KB, 1 = 256 KB
		// bit 5: 0 = manufacturer jumper, infinite diagnostics loop
		// bit 6: 0 = CGA, 1 = MDA
		bit_view<6, 1> lacks_cga;
		// bit 7: 0 = keyboard locked, 1 = not locked

	} port;

	port.lacks_cga = !is_machine_cga_or_better();

	return port.data;
}

static uint8_t get_output_port() // aka port P2
{
	static union {
		uint8_t data = 0b0000'0001;

		// bit 0: 0 = CPU reset, 1 = normal
		// bit 1: 0 = A20 disabled, 1 = enabled
		bit_view<1, 1> is_a20_enabled;
		// bit 2: mouse data out, ISA - unused
		// bit 3: mouse clock, ISA - unused

		// bit 4: 0 = IRQ1 (keyboard) not active, 1 = active
		bit_view<4, 1> is_irq_active_kbd;
		// bit 5: 0 = IRQ12 (mouse) not active, 1 = active
		bit_view<5, 1> is_irq_active_aux;
		// bit 6: keyboard clock
		// bit 7: keyboard data out

	} port;

	port.is_a20_enabled    = MEM_A20_Enabled();
	port.is_irq_active_kbd = is_irq_active_kbd;
	port.is_irq_active_aux = is_irq_active_aux;

	return port.data;
}

static bool is_cmd_mem_read(const Command command)
{
	const auto code = static_cast<uint8_t>(command);
	return (code >= 0x20) && (code <= 0x3f);
}

static bool is_cmd_mem_write(const Command command)
{
	const auto code = static_cast<uint8_t>(command);
	return (code >= 0x60) && (code <= 0x7f);
}

static bool is_cmd_pulse_line(const Command command)
{
	const auto code = static_cast<uint8_t>(command);
	return (code >= 0xf0);
}

static bool is_cmd_vendor_lines(const Command command)
{
	const auto code = static_cast<uint8_t>(command);
	return (code >= 0xb0) && (code <= 0xbd);
}

static void execute_command(const Command command)
{
	// LOG_INFO("I8042: Command 0x%02x", static_cast<int>(command));

	auto diag_dump_byte = [](const uint8_t byte) {
		// Based on communication logs collected from real chip
		// by Vogons forum user 'migry' - reference:
		// - https://www.vogons.org/viewtopic.php?p=1054200
		// - https://www.vogons.org/download/file.php?id=133167

		const size_t nibble_hi = (byte & 0b1111'0000) >> 4;
		const size_t nibble_lo = (byte & 0b0000'1111);

		// clang-format off
		static constexpr uint8_t translation_table[] = {
			0x0b, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
			0x09, 0x0a, 0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21
		};
		// clang-format on

		// Diagnostic dumps send 3 bytes for each byte from memory:
		// - high nibble in hex ASCII, translated using codeset 1 table
		// - low nibble, similarly
		// - 0x39 (space in codeset 1)
		buffer_add(translation_table[nibble_hi]);
		buffer_add(translation_table[nibble_lo]);
		buffer_add(0x39);
	};

	switch (command) {
	//
	// Commands requiring a parameter
	//
	case Command::WriteByteConfig:  // 0x60
	case Command::WriteOutputPort:  // 0xd1
	case Command::SimulateInputKbd: // 0xd2
	case Command::SimulateInputAux: // 0xd3
	case Command::WriteAux:         // 0xd4
		current_command = command;
		break;
	case Command::WriteControllerMode: // 0xcb
		warn_controller_mode();
		current_command = command;
		break;

	//
	// No-parameter commands
	//
	case Command::ReadByteConfig: // 0x20
		// Reads the keyboard controller configuration byte
		flush_buffer();
		buffer_add(config_byte.data);
		break;
	case Command::ReadFwCopyright: // 0xa0
		// Reads the keyboard controller firmware
		// copyright string, terminated by NUL
		flush_buffer();
		for (auto byte : FirmwareCopyright) {
			buffer_add(static_cast<uint8_t>(byte));
		}
		buffer_add(0);
		break;
	case Command::ReadFwRevision: // 0xa1
		// Reads the keyboard controller firmware
		// revision, always one byte
		flush_buffer();
		buffer_add(FirmwareRevision);
		break;
	case Command::PasswordCheck: // 0xa4
		// Check if password installed
		// 0xf1: not installed, or no hardware support
		// 0xfa: password installed
		flush_buffer();
		buffer_add(0xf1);
		break;
	case Command::DisablePortAux: // 0xa7
		// Disable aux (mouse) port
		is_disabled_aux = true;
		break;
	case Command::EnablePortAux: // 0xa8
		// Enable aux (mouse) port
		is_disabled_aux = false;
		break;
	case Command::TestPortAux: // 0xa9
		// Port test. Possible results:
		// 0x01: clock line stuck low
		// 0x02: clock line stuck high
		// 0x03: data line stuck low
		// 0x04: data line stuck high
		// Disables the aux (mouse) port
		is_disabled_aux = true;
		flush_buffer();
		buffer_add(0x00);
		break;
	case Command::TestController: // 0xaa
		// Controller test. Possible results:
		// 0x55: passed; 0xfc: failed
		// Disables aux (mouse) and keyboard ports, enables translation,
		// enables A20 line, marks self-test as passed.
		MEM_A20_Enable(true);
		is_disabled_aux      = true;
		is_disabled_kbd      = true;
		uses_kbd_translation = true;
		passed_self_test     = true;
		flush_buffer();
		buffer_add(0x55);
		break;
	case Command::TestPortKbd: // 0xab
		// Port test. Possible results:
		// (as with aux port test)
		// Disables the keyboard port
		is_disabled_kbd = true;
		flush_buffer();
		buffer_add(0x00); // as with TestPortAux
		break;
	case Command::DiagnosticDump: // 0xac
		// Dump the whole controller internal RAM (16 bytes),
		// output port, input port, test input, and status byte
		warn_internal_ram_access();
		static_assert(BufferSize >= 20 * 3,
		              "Buffer has to hold 3 bytes for each byte of dump");
		flush_buffer();
		is_diagnostic_dump = true;
		diag_dump_byte(config_byte.data);
		for (uint8_t idx = 1; idx <= 16; idx++) {
			diag_dump_byte(0);
		}
		diag_dump_byte(get_input_port());
		diag_dump_byte(get_output_port());
		warn_read_test_inputs();
		diag_dump_byte(0); // test input - TODO: not emulated for now
		diag_dump_byte(status_byte.data);
		break;
	case Command::DisablePortKbd: // 0xad
		// Disable keyboard port; any keyboard command
		// reenables the port
		is_disabled_kbd = true;
		break;
	case Command::EnablePortKbd: // 0xae
		// Enable the keyboard port
		is_disabled_kbd = false;
		break;
	case Command::ReadKbdVersion: // 0xaf
		// Reads the keyboard version
		// TODO: not found any meaningful description,
		// so the code follows 86Box behaviour
		flush_buffer();
		buffer_add(0);
		break;
	case Command::ReadInputPort: // 0xc0
		// Reads the controller input port (P1)
		flush_buffer();
		buffer_add(get_input_port());
		break;
	case Command::ReadControllerMode: // 0xca
		// Reads keyboard controller mode
		// 0x00: ISA (AT)
		// 0x01: PS/2 (MCA)
		flush_buffer();
		buffer_add(0x01);
		break;
	case Command::ReadOutputPort: // 0xd0
		// Reads the controller output port (P2)
		flush_buffer();
		buffer_add(get_output_port());
		break;
	case Command::DisableA20: // 0xdd
		// Disable A20 line
		MEM_A20_Enable(false);
		// Note: extension might seem dangerous, but probably
		// it is better to have it implemented - it is said that
		// some versions of HIMEM.SYS wrongly identify machine
		// as HP Vectra in tries to use it, leading to crashes:
		// https://www.win.tue.nl/~aeb/linux/kbd/A20.html
		break;
	case Command::EnableA20: // 0xdf
		// Enable A20 line
		MEM_A20_Enable(true);
		break;
	case Command::ReadTestInputs: // 0xe0
		// Read test bits:
		// bit 0: keyboard clock in
		// bit 1: (AT) keyboard data in, or (PS/2) mouse clock in
		// Not fully implemented, follows DOSBox-X behaviour.
		warn_read_test_inputs();
		flush_buffer();
		buffer_add(0x00);
		break;
	//
	// Unknown or mostly unsupported commands
	//
	default:
		// Some more MCA controller memory locations are known:
		// - 0x13 - nonzero when a password is enabled
		// - 0x14 - nonzero when the password was matched
		// - 0x16-0x17 - two make codes to be discarded during password
		//               matching
		// For now these are not emulated. If you want to support them,
		// do not forget to update DiagnosticDump command.

		if (is_cmd_mem_read(command)) { // 0x20-0x3f
			// Read internal RAM - dummy, unimplemented
			warn_internal_ram_access();
			buffer_add(0x00);
		} else if (is_cmd_mem_write(command)) { // 0x60-0x7f
			// Write internal RAM - dummy, unimplemented
			warn_internal_ram_access();
			// requires a parameter
			current_command = command;
		} else if (is_cmd_vendor_lines(command)) { // 0xb0-0xbd
			warn_vendor_lines();
		} else if (is_cmd_pulse_line(command)) { // 0xf0-0xff
			// requires a parameter
			current_command = command;
		} else {
			warn_unknown_command(command);
		}
		break;
	}
}

static void execute_command(const Command command, const uint8_t param)
{
	// LOG_INFO("I8042: Command 0x%02x, parameter 0x%02x",
	//          static_cast<int>(command), param);

	using namespace bit::literals;
	switch (command) {
	case Command::WriteByteConfig: // 0x60
		// Writes the keyboard controller configuration byte
		config_byte.data = param;
		// TODO: how this should really work? should the
		// firmware allow everything? writing bits 4,5 and 6
		// should be safe in real implementation, how about here?
		sanitize_config_byte();
		break;
	case Command::WriteControllerMode: // 0xcb
		// Changes controller mode to PS/2 or AT
		// TODO: not implemented for now
		// ReadControllerMode will always claim PS/2
		break;
	case Command::WriteOutputPort: // 0xd1
		// Writes the controller output port (P2)
		// TODO: how should writing other bit behave,
		// should the firmware allow changing them at all?
		MEM_A20_Enable(bit::is(param, b1));
		if (!bit::is(param, b0)) {
			LOG_WARNING("I8042: Clearing P2 bit 0 locks a real PC");
			DOSBOX_Restart();
		}
		break;
	case Command::SimulateInputKbd: // 0xd2
		// Acts as if the byte was received from keyboard
		flush_buffer();
		buffer_add_kbd(param);
		break;
	case Command::SimulateInputAux: // 0xd3
		// Acts as if the byte was received from aux (mouse)
		flush_buffer();
		buffer_add_aux(param);
		break;
	case Command::WriteAux: // 0xd4
		// Sends a byte to the mouse.
		// To prevent excessive inter-module communication,
		// aux (mouse) part is implemented completely within
		// the mouse module
		restart_delay_timer(PortDelayMs * 2); // 'round trip' delay
		is_transmit_timeout = !MOUSEPS2_PortWrite(param);
		break;
	default:
		if (is_cmd_mem_write(command)) { // 0x60-0x7f
			// Internall controller memory write,
			// not implemented for most bytes
		} else if (is_cmd_pulse_line(command)) { // 0xf0-0xff
			// Pulse controller lines for 6ms,
			// bits 0-3 counts, 0 = pulse relevant line
			const auto lines = param & 0b0000'1111;
			const auto code  = static_cast<uint8_t>(command);
			if ((code == 0xf0 && param != 0b1111 && param != 0b1110) ||
			    (code != 0xf0 && param != 0b1111)) {
				warn_line_pulse();
			}
			if (code == 0xf0 && !(lines & 0b0001)) {
				// System reset via keyboard controller
				DOSBOX_Restart();
			}
		} else {
			// If we are here, than either this function
			// was wrongly called or it is incomplete
			assert(false);
		}
		break;
	}
}

// ***************************************************************************
// I/O port handlers
// ***************************************************************************

static uint32_t read_data_port(io_port_t, io_width_t width)
{
	// Port 0x60 read handler

	if (width == WidthVmWare && VMWARE_I8042_ReadTakeover()) {
		return VMWARE_I8042_ReadDataPort();
	}

	if (!is_data_new) {
		// Byte already read - just return the previous one
		return data_byte;
	}

	if (is_diagnostic_dump && !buffer_num_used) {
		// Diagnostic dump finished
		is_diagnostic_dump = false;
		if (I8042_IsReadyForAuxFrame()) {
			MOUSEPS2_NotifyReadyForFrame();
		}
		if (I8042_IsReadyForKbdFrame()) {
			KEYBOARD_NotifyReadyForFrame();
		}
	}

	if (is_data_from_aux) {
		assert(waiting_bytes_from_aux);
		--waiting_bytes_from_aux;
		if (I8042_IsReadyForAuxFrame()) {
			MOUSEPS2_NotifyReadyForFrame();
		}
	}

	if (is_data_from_kbd) {
		assert(waiting_bytes_from_kbd);
		--waiting_bytes_from_kbd;
		if (I8042_IsReadyForKbdFrame()) {
			KEYBOARD_NotifyReadyForFrame();
		}
	}

	const auto ret_val = data_byte;

	is_data_new      = false; // mark byte as already read
	is_data_from_aux = false;
	is_data_from_kbd = false;

	// Enforce the simulated data transfer delay, as some software
	// (Tyrian 2000 setup) reads the port without waiting for the
	// interrupt.
	restart_delay_timer(PortDelayMs);

	return ret_val;
}

static uint32_t read_status_register(io_port_t, io_width_t width)
{
	// Port 0x64 read handler

	if (width == WidthVmWare && VMWARE_I8042_ReadTakeover()) {
		return VMWARE_I8042_ReadStatusRegister();
	}

	return status_byte.data;
}

static void write_data_port(io_port_t, io_val_t value, io_width_t)
{
	// Port 0x60 write handler

	const auto byte = check_cast<uint8_t>(value);
	status_byte.was_last_write_cmd = false;

	if (current_command != Command::None) {
		// A controller command is waiting for a parameter
		const auto command = current_command;
		current_command    = Command::None;

		const bool should_notify_aux = !I8042_IsReadyForAuxFrame();
		const bool should_notify_kbd = !I8042_IsReadyForKbdFrame();

		should_skip_device_notify = true;
		flush_buffer();
		execute_command(command, byte);
		should_skip_device_notify = false;

		if (should_notify_aux && I8042_IsReadyForAuxFrame()) {
			MOUSEPS2_NotifyReadyForFrame();
		}
		if (should_notify_kbd && I8042_IsReadyForKbdFrame()) {
			KEYBOARD_NotifyReadyForFrame();
		}
	} else {
		// Send this byte to the keyboard
		is_transmit_timeout = false;
		is_disabled_kbd     = false; // port auto-enable

		flush_buffer();
		restart_delay_timer(PortDelayMs * 2); // 'round trip' delay
		KEYBOARD_PortWrite(byte);
	}
}

static void write_command_port(io_port_t, io_val_t value, io_width_t width)
{
	// Port 0x64 write handler

	if (width == WidthVmWare && VMWARE_I8042_WriteCommandPort(value)) {
		return;
	}

	const auto byte = static_cast<uint8_t>(value);
	should_skip_device_notify = true;

	const bool should_notify_aux = !I8042_IsReadyForAuxFrame();
	const bool should_notify_kbd = !I8042_IsReadyForKbdFrame();

	if (is_diagnostic_dump) {
		is_diagnostic_dump = false;
		flush_buffer();
	}

	status_byte.was_last_write_cmd = true;

	current_command = Command::None;
	if ((byte <= 0x1f) || (byte >= 0x40 && byte <= 0x5f)) {
		// AMI BIOS systems command aliases
		execute_command(static_cast<Command>(byte + 0x20));
	} else {
		execute_command(static_cast<Command>(byte));
	}

	should_skip_device_notify = false;

	if (should_notify_aux && I8042_IsReadyForAuxFrame()) {
		MOUSEPS2_NotifyReadyForFrame();
	}
	if (should_notify_kbd && I8042_IsReadyForKbdFrame()) {
		KEYBOARD_NotifyReadyForFrame();
	}
}

// ***************************************************************************
// External entry points
// ***************************************************************************

void I8042_AddAuxByte(const uint8_t byte)
{
	if (is_disabled_aux) {
		return; // aux (mouse) port is disabled
	}

	is_transmit_timeout = false;

	enforce_buffer_space();
	buffer_add_aux(byte);
}

void I8042_AddAuxFrame(const std::vector<uint8_t>& bytes)
{
	assert(bytes.size() < UINT8_MAX);

	if (bytes.empty() || is_disabled_aux) {
		return; // empty frame or aux (mouse) port is disabled
	}

	is_transmit_timeout = false;

	// Cheat a little to improve input latency - skip delay timer between
	// subsequent bytes of mouse data frame; this seems to be compatible
	// with all the PS/2 mouse drivers tested so far.

	bool skip_delay = false;
	enforce_buffer_space(bytes.size());
	for (const auto& byte : bytes) {
		buffer_add_aux(byte, skip_delay);
		skip_delay = true;
	}
}

void I8042_AddKbdByte(const uint8_t byte)
{
	if (is_disabled_kbd) {
		return; // keyboard port is disabled
	}

	is_transmit_timeout = false;

	enforce_buffer_space();
	buffer_add_kbd(byte);
}

void I8042_AddKbdFrame(const std::vector<uint8_t>& bytes)
{
	assert(bytes.size() < UINT8_MAX);

	if (bytes.empty() || is_disabled_kbd) {
		return; // empty frame or keyboard port is disabled
	}

	is_transmit_timeout = false;

	enforce_buffer_space(bytes.size());
	for (const auto& byte : bytes) {
		buffer_add_kbd(byte);
	}
}

bool I8042_IsReadyForAuxFrame()
{
	return !waiting_bytes_from_aux && !is_disabled_aux && !is_diagnostic_dump;
}

bool I8042_IsReadyForKbdFrame()
{
	return !waiting_bytes_from_kbd && !is_disabled_kbd && !is_diagnostic_dump;
}

void I8042_TriggerAuxInterrupt()
{
	PIC_ActivateIRQ(get_irq_mouse());
}

// ***************************************************************************
// Initialization
// ***************************************************************************

void I8042_Init()
{
	assert(BufferSize >= FirmwareCopyright.size() + 16);

	IO_RegisterReadHandler(port_num_i8042_data,
	                       read_data_port,
	                       io_width_t::dword);
	IO_RegisterReadHandler(port_num_i8042_status,
	                       read_status_register,
	                       io_width_t::dword);
	IO_RegisterWriteHandler(port_num_i8042_data,
	                        write_data_port,
	                        io_width_t::byte);
	IO_RegisterWriteHandler(port_num_i8042_command,
	                        write_command_port,
	                        io_width_t::dword);

	// Initialize hardware
	flush_buffer();
}
