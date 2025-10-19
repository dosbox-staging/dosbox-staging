// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PORT_H
#define DOSBOX_PORT_H

#include "dosbox.h"

#include <functional>

using io_port_t = uint16_t; // DOS only supports 16-bit port addresses
using io_val_t  = uint32_t; // Handling exists up to a dword (or less)

void IO_Init();
void IO_Destroy();

void IO_WriteB(io_port_t port, uint8_t val);
void IO_WriteW(io_port_t port, uint16_t val);
void IO_WriteD(io_port_t port, uint32_t val);

uint8_t IO_ReadB(io_port_t port);
uint16_t IO_ReadW(io_port_t port);
uint32_t IO_ReadD(io_port_t port);

// type-sized IO handler API
enum class io_width_t : uint8_t {
	byte = 1, // bytes
	word = 2, // bytes
	dword = 4, // bytes
};

// Sanity check the IO sizes
static_assert(static_cast<size_t>(io_width_t::byte) == sizeof(uint8_t), "io_width_t::byte must be 1 byte");
static_assert(static_cast<size_t>(io_width_t::word) == sizeof(uint16_t), "io_width_t::word must be 2 bytes");
static_assert(static_cast<size_t>(io_width_t::dword) == sizeof(uint32_t), "io_width_t::dword must be 4 bytes");

constexpr int io_widths = 3; // byte, word, and dword

using io_read_f = std::function<io_val_t(io_port_t port, io_width_t width)>;
using io_write_f = std::function<void(io_port_t port, io_val_t val, io_width_t width)>;

void IO_RegisterReadHandler(io_port_t port,
                            io_read_f handler,
                            io_width_t max_width,
                            io_port_t range = 1);

void IO_RegisterWriteHandler(io_port_t port,
                             io_write_f handler,
                             io_width_t max_width,
                             io_port_t range = 1);

void IO_FreeReadHandler(io_port_t port,
                        io_width_t max_width,
                        io_port_t range = 1);

void IO_FreeWriteHandler(io_port_t port,
                         io_width_t max_width,
                         io_port_t range = 1);

/* Classes to manage the IO objects created by the various devices.
 * The io objects will remove itself on destruction.*/
class IO_Base{
protected:
	bool installed = false;
	io_port_t m_port = 0u;
	io_width_t m_width = io_width_t::byte;
	io_port_t m_range = 0u;
};

class IO_ReadHandleObject: private IO_Base{
public:
	void Install(io_port_t port,
	             io_read_f handler,
	             io_width_t max_width,
	             io_port_t range = 1);

	void Uninstall();
	~IO_ReadHandleObject();
};
class IO_WriteHandleObject: private IO_Base{
public:
	void Install(io_port_t port,
	             io_write_f handler,
	             io_width_t max_width,
	             io_port_t range = 1);

	void Uninstall();
	~IO_WriteHandleObject();
};

static inline void IO_Write(io_port_t port, uint8_t val)
{
	IO_WriteB(port,val);
}

static inline uint8_t IO_Read(io_port_t port)
{
	// cast to be dropped after deprecating the Bitu IO handler API
	return IO_ReadB(port);
}

// Hardware I/O port numbers
//
// TODO fix casings and used namespaced constants; e.g.:
//
//   IoPort::i8042::Data
//   IoPort::i8042::Status
//   IoPort::i8042::Command
//
// Also move over VGAREG_* definitions from `int10.h`, e.g.:
//
//   IoPort::Vga::InputStatus
//   IoPort::Vga::VideoEnable
//   IoPort::Vga::SequencerAddress
//   IoPort::Vga::SequencerData
//
// This should make it easy to import only a subset with the `using` keyword,
// e.g.: `using namespace IoPort::Vga`.
//

namespace Port {

namespace AdLib {

constexpr io_port_t Command = 0x388u;

}
} // namespace Port

// Intel 8042 keyboard/mouse port microcontroller
constexpr io_port_t port_num_i8042_data    = 0x60u;
constexpr io_port_t port_num_i8042_status  = 0x64u; // read-only
constexpr io_port_t port_num_i8042_command = 0x64u; // write-only

// Intel 8255 microcontrollers
constexpr io_port_t port_num_i8255_1 = 0x61u;
constexpr io_port_t port_num_i8255_2 = 0x62u;

// PS/2 control port, mainly for fast A20
constexpr io_port_t port_num_fast_a20 = 0x92u;

// PCI bus registers
constexpr io_port_t port_num_pci_config_address = 0xcf8u;
constexpr io_port_t port_num_pci_config_data    = 0xcfcu;

// VirtualBox communication interface
// (can be moved, but two last bits have to be 0)
constexpr io_port_t port_num_virtualbox = 0x5654u;

// VMware communication interface
constexpr io_port_t port_num_vmware    = 0x5658u;
constexpr io_port_t port_num_vmware_hb = 0x5659u; // high bandwidth

#endif // DOSBOX_PORT_H
