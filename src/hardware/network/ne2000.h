// SPDX-FileCopyrightText:  2021-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2008-2010  Ralf Grillenberger <h-a-l-9000@users.sourceforge.net>
// SPDX-FileCopyrightText:  2004-2008  Dean Beeler <canadacow@users.sourceforge.net>
// SPDX-FileCopyrightText:  2001-2003  Peter Grehan <grehan@iprg.nokia.com>
// SPDX-FileCopyrightText:  2001-2003  MandrakeSoft S.A.
// SPDX-License-Identifier: GPL-2.0-or-later

// An implementation of an ne2000 ISA ethernet adapter. This part uses
// a National Semiconductor DS-8390 ethernet MAC chip, with some h/w
// to provide a windowed memory region for the chip and a MAC address.

#ifndef DOSBOX_NE2000_H
#define DOSBOX_NE2000_H

#include "dosbox.h"

#include <string>

#include "config/setup.h"
#include "hardware/port.h"

#define bx_bool int
#define bx_param_c uint8_t


#  define BX_NE2K_SMF
#  define BX_NE2K_THIS_PTR 
#  define BX_NE2K_THIS	
//#define BX_INFO 
//LOG_MSG
//#define BX_DEBUG 
//LOG_MSG

#define  BX_NE2K_MEMSIZ    (32*1024)
#define  BX_NE2K_MEMSTART  (16*1024)
#define  BX_NE2K_MEMEND    (BX_NE2K_MEMSTART + BX_NE2K_MEMSIZ)

struct bx_ne2k_t {
	//
	// ne2k register state

	//
	// Page 0
	//
	//  Command Register - 00h read/write
	struct CR_t {
		bx_bool stop = 1;      // STP - Software Reset command
		bx_bool start = 0;     // START - start the NIC
		bx_bool tx_packet = 0; // TXP - initiate packet transmission
		uint8_t rdma_cmd = 4;  // RD0,RD1,RD2 - Remote DMA command
		uint8_t pgsel = 0;     // PS0,PS1 - Page select
	} CR = {};
	// Interrupt Status Register - 07h read/write
	struct ISR_t {
		bx_bool pkt_rx = 0; // PRX - packet received with no errors
		bx_bool pkt_tx = 0; // PTX - packet transmitted with no errors
		bx_bool rx_err = 0; // RXE - packet received with 1 or more errors
		bx_bool tx_err = 0; // TXE - packet tx'd       "  " "    "    "
		bx_bool overwrite = 0; // OVW - rx buffer resources exhausted
		bx_bool cnt_oflow = 0; // CNT - network tally counter MSB's set
		bx_bool rdma_done = 0; // RDC - remote DMA complete
		bx_bool reset = 1;     // RST - reset status
	} ISR = {};
	// Interrupt Mask Register - 0fh write
	struct IMR_t {
		bx_bool rx_inte = 0;    // PRXE - packet rx interrupt enable
		bx_bool tx_inte = 0;    // PTXE - packet tx interrput enable
		bx_bool rxerr_inte = 0; // RXEE - rx error interrupt enable
		bx_bool txerr_inte = 0; // TXEE - tx error interrupt enable
		bx_bool overw_inte = 0; // OVWE - overwrite warn int enable
		bx_bool cofl_inte = 0;  // CNTE - counter o'flow int enable
		bx_bool rdma_inte = 0;  // RDCE - remote DMA complete int enable
		bx_bool reserved = 0;   //  D7 - reserved
	} IMR = {};
	// Data Configuration Register - 0eh write
	struct DCR_t {
		bx_bool wdsize = 0;   // WTS - 8/16-bit select
		bx_bool endian = 0;   // BOS - byte-order select
		bx_bool longaddr = 1; // LAS - long-address select
		bx_bool loop = 0;     // LS  - loopback select
		bx_bool auto_rx = 0; // AR  - auto-remove rx packets with remote
		                     // DMA
		uint8_t fifo_size = 0; // FT0,FT1 - fifo threshold
	} DCR = {};
	// Transmit Configuration Register - 0dh write
	struct TCR_t {
		bx_bool crc_disable = 0; // CRC - inhibit tx CRC
		uint8_t loop_cntl = 0;   // LB0,LB1 - loopback control
		bx_bool ext_stoptx = 0; // ATD - allow tx disable by external mcast
		bx_bool coll_prio = 0; // OFST - backoff algorithm select
		uint8_t reserved = 0;  //  D5,D6,D7 - reserved
	} TCR = {};
	// Transmit Status Register - 04h read
	struct TSR_t {
		bx_bool tx_ok = 0;    // PTX - tx complete without error
		bx_bool reserved = 0; //  D1 - reserved
		bx_bool collided = 0; // COL - tx collided >= 1 times
		bx_bool aborted = 0; // ABT - aborted due to excessive collisions
		bx_bool no_carrier = 0; // CRS - carrier-sense lost
		bx_bool fifo_ur = 0;    // FU  - FIFO underrun
		bx_bool cd_hbeat = 0; // CDH - no tx cd-heartbeat from transceiver
		bx_bool ow_coll = 0; // OWC - out-of-window collision
	} TSR = {};
	// Receive Configuration Register - 0ch write
	struct RCR_t {
		bx_bool errors_ok = 0; // SEP - accept pkts with rx errors
		bx_bool runts_ok = 0;  // AR  - accept < 64-byte runts
		bx_bool broadcast = 0; // AB  - accept eth broadcast address
		bx_bool multicast = 0; // AM  - check mcast hash array
		bx_bool promisc = 0;   // PRO - accept all packets
		bx_bool monitor = 0;   // MON - check pkts, but don't rx
		uint8_t reserved = 0;  //  D6,D7 - reserved
	} RCR = {};
	// Receive Status Register - 0ch read
	struct RSR_t {
		bx_bool rx_ok = 0;      // PRX - rx complete without error
		bx_bool bad_crc = 0;    // CRC - Bad CRC detected
		bx_bool bad_falign = 0; // FAE - frame alignment error
		bx_bool fifo_or = 0;    // FO  - FIFO overrun
		bx_bool rx_missed = 0;  // MPA - missed packet error
		bx_bool rx_mbit = 0; // PHY - unicast or mcast/bcast address match
		bx_bool rx_disabled = 0; // DIS - set when in monitor mode
		bx_bool deferred = 0;    // DFR - collision active
	} RSR = {};

	uint16_t local_dma = 0;    // 01,02h read ; current local DMA addr
	uint8_t page_start = 0;    // 01h write ; page start register
	uint8_t page_stop = 0;     // 02h write ; page stop register
	uint8_t bound_ptr = 0;     // 03h read/write ; boundary pointer
	uint8_t tx_page_start = 0; // 04h write ; transmit page start register
	uint8_t num_coll = 0;      // 05h read  ; number-of-collisions register
	uint16_t tx_bytes = 0;   // 05,06h write ; transmit byte-count register
	uint8_t fifo = 0;        // 06h read  ; FIFO
	uint16_t remote_dma = 0; // 08,09h read ; current remote DMA addr
	uint16_t remote_start = 0; // 08,09h write ; remote start address register
	uint16_t remote_bytes = 0; // 0a,0bh write ; remote byte-count register
	uint8_t tallycnt_0 = 0; // 0dh read  ; tally counter 0 (frame align errors)
	uint8_t tallycnt_1 = 0; // 0eh read  ; tally counter 1 (CRC errors)
	uint8_t tallycnt_2 = 0; // 0fh read  ; tally counter 2 (missed pkt errors)

	//
	// Page 1
	//
	//   Command Register 00h (repeated)
	//
	uint8_t physaddr[6] = {}; // 01-06h read/write ; MAC address
	uint8_t curr_page = 0;    // 07h read/write ; current page register
	uint8_t mchash[8] = {};   // 08-0fh read/write ; multicast hash array

	//
	// Page 2  - diagnostic use only
	//
	//   Command Register 00h (repeated)
	//
	//   Page Start Register 01h read  (repeated)
	//   Page Stop Register  02h read  (repeated)
	//   Current Local DMA Address 01,02h write (repeated)
	//   Transmit Page start address 04h read (repeated)
	//   Receive Configuration Register 0ch read (repeated)
	//   Transmit Configuration Register 0dh read (repeated)
	//   Data Configuration Register 0eh read (repeated)
	//   Interrupt Mask Register 0fh read (repeated)
	//
	uint8_t rempkt_ptr = 0;   // 03h read/write ; remote next-packet pointer
	uint8_t localpkt_ptr = 0; // 05h read/write ; local next-packet pointer
	uint16_t address_cnt = 0; // 06,07h read/write ; address counter

	//
	// Page 3  - should never be modified.
	//

	// Novell ASIC state
	uint8_t macaddr[32] = {};         // ASIC ROM'd MAC address, even bytes
	uint8_t mem[BX_NE2K_MEMSIZ] = {}; // on-chip packet memory

	// ne2k internal state
	io_port_t base_address = 0;
	uint8_t base_irq = 0;
	int tx_timer_index = 0;
	int tx_timer_active = 0;
};

class bx_ne2k_c  {
public:
	bx_ne2k_c();
	virtual ~bx_ne2k_c() = default;

	virtual void init();
	virtual void reset(unsigned type);

public:
	bx_ne2k_t s = {};

  /* TODO: Setup SDL */
  //eth_pktmover_c *ethdev;

	BX_NE2K_SMF uint32_t read_cr();
	BX_NE2K_SMF void write_cr(io_val_t value);

	BX_NE2K_SMF uint32_t chipmem_read(io_port_t address, io_width_t io_len);
	BX_NE2K_SMF uint32_t asic_read(io_port_t offset, io_width_t io_len);
	BX_NE2K_SMF uint32_t page0_read(io_port_t offset, io_width_t io_len);
	BX_NE2K_SMF uint32_t page1_read(io_port_t offset, io_width_t io_len);
	BX_NE2K_SMF uint32_t page2_read(io_port_t offset, io_width_t io_len);
	BX_NE2K_SMF uint32_t page3_read(io_port_t offset, io_width_t io_len);

	BX_NE2K_SMF void chipmem_write(io_port_t address, io_val_t value, io_width_t io_len);
	BX_NE2K_SMF void asic_write(io_port_t address, io_val_t value, io_width_t io_len);
	BX_NE2K_SMF void page0_write(io_port_t address, io_val_t value, io_width_t io_len);
	BX_NE2K_SMF void page1_write(io_port_t address, io_val_t value, io_width_t io_len);
	BX_NE2K_SMF void page2_write(io_port_t address, io_val_t value, io_width_t io_len);
	BX_NE2K_SMF void page3_write(io_port_t address, io_val_t value, io_width_t io_len);

public:
  static void tx_timer_handler(void *);
  BX_NE2K_SMF void tx_timer(void);

  //static void rx_handler(void *arg, const void *buf, unsigned len);
  BX_NE2K_SMF unsigned mcast_index(const void *dst);
  BX_NE2K_SMF int rx_frame(const void *buf, unsigned bytes);

  static uint32_t read_handler(void *this_ptr, io_port_t address, io_width_t io_len);
  static void   write_handler(void *this_ptr, io_port_t address, io_val_t value, io_width_t io_len);
#if !BX_USE_NE2K_SMF
  uint32_t read(io_port_t address, io_width_t io_len);
  void   write(io_port_t address, io_val_t value, io_width_t io_len);
#endif
};

void NE2K_Init(SectionProp& section);
void NE2K_Destroy();

#endif // DOSBOX_NE2000_H
