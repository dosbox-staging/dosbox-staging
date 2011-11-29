/*
 *  Copyright (C) 2002-2011  The DOSBox Team
 *  Copyright (C) 2011       Alexandre Becoulet
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

#include "dosbox.h"
#if defined(C_GDBSERVER)

#include <string.h>
#include <list>
#include <ctype.h>
#include <fstream>
#include <iomanip>
#include <string>
#include <sstream>
#include <map>
#include <algorithm>
#include <vector>

using namespace std;

#ifdef WIN32
//#include <Winsock2.h>
#include "poll.h"
char * strtok_r (char *s, const char *delim, char **save_ptr) {
  char *token;

  if (s == NULL)
    s = *save_ptr;

  /* Scan leading delimiters.  */
  s += strspn (s, delim);
  if (*s == '\0')
    {
      *save_ptr = s;
      return NULL;
    }

  /* Find the end of the token.  */
  token = s;
  s = strpbrk (token, delim);
  if (s == NULL)
    /* This token finishes the string.  */
    *save_ptr = token + strlen (token);
  else
    {
      /* Terminate the token and make *SAVE_PTR point past it.  */
      *s = '\0';
      *save_ptr = s + 1;
    }
  return token;
}

#else
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#endif
#include <assert.h>
#include <stdarg.h>

#include "debug.h"
#include "cpu.h"
#include "video.h"
#include "pic.h"
#include "cpu.h"
#include "callback.h"
#include "paging.h"
#include "setup.h"

#define GDB_TCP_PORT 1234

// remote protocol debug display, must not call DEBUG_ShowMsg in the end
#define GDB_REMOTE_LOG(...) fprintf(stderr, __VA_ARGS__)

enum GdbState {
  GdbNotConnected,
  GdbRunning,
  GdbStoped,
  GdbStep,         // set during step by step
  GdbMonitor,      // set during monitor command processing
};

static GdbState gdb_state = GdbNotConnected;
static bool gdb_remote_debug = false;
static int gdb_socket, gdb_asocket;

static Bit32u step_eip;
static Bit16u step_cs;
GdbState step_next_state;

// breakpoints/watchpoints maps
typedef map<Bit32u /* start addr */, size_t /* len */> bp_map_t;
static bp_map_t break_points;
static bp_map_t read_watch_points;
static bp_map_t write_watch_points;

// interrupts breakpoints bitmap
vector<bool> int_bp(256);

Bitu DEBUG_debugCallback;
Bitu DEBUG_cycle_count;
static Bitu cycle_bp;

extern bool DEBUG_logHeavy;

static inline Bit32u
swapByte32(Bit32u x)
{
  return (((x >> 24) & 0x000000ff) |
	  ((x >> 8 ) & 0x0000ff00) |
	  ((x << 8 ) & 0x00ff0000) |
	  ((x << 24) & 0xff000000));
}

static inline Bit16u
swapByte16(Bit16u x)
{
  return (x >> 8) | (x << 8);
}

/****************************/
/* Gdb cpu registers access */
/****************************/

enum GdbEipMode {
  GdbFlatEip,
  GdbRealEip,
};

static GdbEipMode gdb_eip_mode = GdbFlatEip;

static void gdbSetEip(Bit32u gdb_eip)
{
  /* make gdb client see flat eip value */
  switch (gdb_eip_mode) {
  case GdbFlatEip:
    reg_eip = gdb_eip - SegPhys(cs);
    break;
  case GdbRealEip:
    reg_eip = gdb_eip;
    break;
  }
}

static Bit32u gdbGetEip(void)
{
  switch (gdb_eip_mode) {
  case GdbFlatEip:
    return reg_eip + SegPhys(cs);
  case GdbRealEip:
    return reg_eip;
  }
}

#define GDB_REGS_COUNT 16

Bit32u DEBUG_GdbGetRegister(int reg)
{
  switch (reg) {
  case 0:
    return swapByte32(reg_eax);
  case 1:
    return swapByte32(reg_ecx);
  case 2:
    return swapByte32(reg_edx);
  case 3:
    return swapByte32(reg_ebx);
  case 4:
    return swapByte32(reg_esp);
  case 5:
    return swapByte32(reg_ebp);
  case 6:
    return swapByte32(reg_esi);
  case 7:
    return swapByte32(reg_edi);
  case 8:
    return swapByte32(gdbGetEip());
  case 9:
    return swapByte32(reg_flags);
  case 10:
    return swapByte32(SegValue(cs));
  case 11:
    return swapByte32(SegValue(ss));
  case 12:
    return swapByte32(SegValue(ds));
  case 13:
    return swapByte32(SegValue(es));
  case 14:
    return swapByte32(SegValue(fs));
  case 15:
    return swapByte32(SegValue(gs));
  default:
    return 0;
  }
}

int DEBUG_GdbGetRegisterSize(int reg)
{
  switch (reg) {
  case 0: /* gp regs */
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
  case 7:
  case 8:
  case 9:
    return 32;

  case 10: /* segments */
  case 11:
  case 12:
  case 13:
  case 14:
  case 15:
    return 32;

  default:
    return 0;
  }

}

void DEBUG_GdbSetRegister(int reg, Bit32u value)
{
  switch (reg) {
  case 0:
    reg_eax = swapByte32(value);
    break;
  case 1:
    reg_ecx = swapByte32(value);
    break;
  case 2:
    reg_edx = swapByte32(value);
    break;
  case 3:
    reg_ebx = swapByte32(value);
    break;
  case 4:
    reg_esp = swapByte32(value);
    break;
  case 5:
    reg_ebp = swapByte32(value);
    break;
  case 6:
    reg_esi = swapByte32(value);
    break;
  case 7:
    reg_edi = swapByte32(value);
    break;
  case 8:
    gdbSetEip(swapByte32(value));
    break;
  case 9:
    reg_flags = swapByte32(value);
    break;
  case 10:
    SegSet16(cs, swapByte32(value));
    break;
  case 11:
    SegSet16(ss, swapByte32(value));
    break;
  case 12:
    SegSet16(ds, swapByte32(value));
    break;
  case 13:
    SegSet16(es, swapByte32(value));
    break;
  case 14:
    SegSet16(fs, swapByte32(value));
    break;
  case 15:
    SegSet16(gs, swapByte32(value));
    break;
  }
}

/***********************/
/* Gdb remote protocol */
/***********************/

int DEBUG_GdbWritePacket(const char *data_)
{
  unsigned int i, len = strlen(data_);
  char ack, end[4];
  uint8_t chksum = 0;
  char data[len];

  memcpy(data, data_, len + 1);

  unsigned char repeat = 0;
  unsigned int cmplen = len;
  char *cmp = data;
  char *last = 0;

#if 1     // gdb RLE data compression

  for (i = 0; ; ) {
    if (i < cmplen && last && (*last == cmp[i]) && (repeat + 29 < 126)) {
      repeat++;
    } else {
      if (repeat > 3) {
	while (repeat == '#' - 29 || repeat == '$' - 29 ||
	       repeat == '+' - 29 || repeat == '-' - 29) {
	  repeat--;
	  last++;
	}
	last[1] = '*';
	last[2] = 29 + repeat;
	memmove(last + 3, cmp + i, cmplen - i + 1);
	cmp = last + 3;
	cmplen -= i;
	i = 0;
	last = 0;
	repeat = 0;
	continue;
      } else {
	last = cmp + i;
	repeat = 0;
      }

      if (i == cmplen)
	break;
    }
    i++;
  }

  cmp[cmplen] = 0;
  len = cmp - data + cmplen;

#endif

  for (i = 0; i < len; i++)
    chksum += data[i];

  sprintf(end, "#%02x", chksum);

  do {
    if (gdb_remote_debug)
      GDB_REMOTE_LOG("GDB: sending packet data '%s'\n", data);

    send(gdb_asocket, "$", 1, MSG_DONTWAIT | MSG_NOSIGNAL);
    send(gdb_asocket, data, len, MSG_DONTWAIT | MSG_NOSIGNAL);
    send(gdb_asocket, end, 3, MSG_DONTWAIT | MSG_NOSIGNAL);

    if (read(gdb_asocket, &ack, 1) < 1) {
      close(gdb_asocket);
      gdb_state = GdbNotConnected;
      return -1;
    }
  } while (ack != '+');

  return 0;
}

char * DEBUG_GdbReadPacket(char *buffer, size_t size)
{
  int res = read(gdb_asocket, buffer, size);

  if (res <= 0) {
    close(gdb_asocket);
    gdb_state = GdbNotConnected;
    return 0;
  }

  uint8_t sum = 0, chksum = 0;
  char *data = 0;
  char *end = 0;
  int i;

  // find data in packet
  for (i = 0; i < res; i++) {
    switch (buffer[i]) {
    case '$':
      sum = 0;
      data = buffer + i + 1;
      break;

    case '#':
      chksum = sum;
      end = buffer + i;
      *end = 0;
      goto end;

    default:
      sum += buffer[i];
    }
  }
 end:

  // malformed packet
  if (!end || data >= end) {
    if (gdb_remote_debug)
      GDB_REMOTE_LOG("GDB: malformed packet %i bytes\n", res);

    return 0;
  }

  if (gdb_remote_debug)
    GDB_REMOTE_LOG("GDB: packet with checksum %02x: %s\n", chksum, data);

  // verify checksum
  end[3] = 0;
  if (chksum != strtoul(end + 1, 0, 16)) {
    send(gdb_asocket, "-", 1, MSG_DONTWAIT | MSG_NOSIGNAL);

    if (gdb_remote_debug)
      GDB_REMOTE_LOG("GDB: bad packet checksum\n");

    return 0;
  }

  send(gdb_asocket, "+", 1, MSG_DONTWAIT | MSG_NOSIGNAL);

  return data;
}

static bool DEBUG_GdbPointSet(bp_map_t &b, Bit32u addr, size_t len, bool set_)
{
  if (set_) {
    pair<bp_map_t::iterator, bool> ret = b.insert(bp_map_t::value_type(addr, len));

    if (!ret.second)	// adjust existing point
      ret.first->second = max(ret.first->second, len);
  } else {
    b.erase(addr);
  }
}

void DEBUG_GdbProcessMonitorPacket(char *data)
{
  const char *delim = " \t,";
  char *tokens[255], *save;
  unsigned int i = 0;

  for (char *t = strtok_r(data, delim, &save);
       t != NULL; t = strtok_r(NULL, delim, &save)) {
    tokens[i++] = t;
    if (i == 255)
      break;
  }

  if (i >= 2 && !strcmp(tokens[0], "remote_debug")) {
    gdb_remote_debug = atoi(tokens[1]);
    DEBUG_GdbWritePacket("OK");
    return;
  }

  if (i >= 1 && !strcmp(tokens[0], "write_log_instruction")) {
    DEBUG_HeavyWriteLogInstruction();
    DEBUG_GdbWritePacket("OK");
    return;
  }

  if (i >= 2 && !strcmp(tokens[0], "log_heavy")) {
    DEBUG_logHeavy = atoi(tokens[1]);
    DEBUG_GdbWritePacket("OK");
    return;
  }

  if (i >= 2 && !strcmp(tokens[0], "cycle_abs_bp")) {
    cycle_bp = atoi(tokens[1]);
    DEBUG_GdbWritePacket("OK");
    return;
  }

  if (i >= 2 && !strcmp(tokens[0], "cycle_bp")) {
    cycle_bp = DEBUG_cycle_count + atoi(tokens[1]);
    DEBUG_ShowMsg("GDB: Cycle break point set at %u.\n", cycle_bp);
    DEBUG_GdbWritePacket("OK");
    return;
  }

  if (i >= 2 && !strcmp(tokens[0], "flat_eip")) {
    gdb_eip_mode = atoi(tokens[1]) ? GdbFlatEip : GdbRealEip;
    DEBUG_GdbWritePacket("OK");
    return;
  }

  if (i >= 3 && !strcmp(tokens[0], "int_bp")) {
    unsigned int inum = strtoul(tokens[1], 0, 0);

    if (inum < 256) {
      int_bp[inum] = atoi(tokens[2]);
      DEBUG_GdbWritePacket("OK");
      return;
    }
  }

  if (i >= 1 && !strcmp(tokens[0], "log_gdt")) {
    DEBUG_LogGDT();
    DEBUG_GdbWritePacket("OK");
    return;
  }

  if (i >= 1 && !strcmp(tokens[0], "log_ldt")) {
    DEBUG_LogLDT();
    DEBUG_GdbWritePacket("OK");
    return;
  }

  if (i >= 1 && !strcmp(tokens[0], "log_idt")) {
    DEBUG_LogIDT();
    DEBUG_GdbWritePacket("OK");
    return;
  }

  if (i >= 1 && !strcmp(tokens[0], "log_cpuinfo")) {
    DEBUG_LogCPUInfo();
    DEBUG_GdbWritePacket("OK");
    return;
  }

  if (i >= 2 && !strcmp(tokens[0], "log_pages")) {
    DEBUG_LogPages(tokens[1]);
    DEBUG_GdbWritePacket("OK");
    return;
  }

  DEBUG_ShowMsg("Supported DOSBox Gdb monitor commands:");
  DEBUG_ShowMsg("  monitor cycle_bp [value]           - set relative cycle breakpoint.");
  DEBUG_ShowMsg("  monitor cycle_abs_bp [value]       - set absolute cycle breakpoint.");
  DEBUG_ShowMsg("  monitor flat_eip [ 0 | 1 ]         - enable/disable use of flat eip register value.");
  DEBUG_ShowMsg("  monitor int_bp [int_num] [ 0 | 1 ] - set breakpoint on cpu interrupt.");
  DEBUG_ShowMsg("  monitor log_gdt                    - Lists descriptors of the GDT.");
  DEBUG_ShowMsg("  monitor log_ldt                    - Lists descriptors of the LDT.");
  DEBUG_ShowMsg("  monitor log_idt                    - Lists descriptors of the IDT.");
  DEBUG_ShowMsg("  monitor log_cpuinfo                - Display CPU status information.");
  DEBUG_ShowMsg("  monitor log_pages [page]           - Display content of page table.");
  DEBUG_ShowMsg("  monitor remote_debug [ 0 | 1 ]     - enable/disable gdb remote protocol debug.");
  DEBUG_ShowMsg("  monitor set_log_heavy [ 0 | 1 ]    - enable/disable heavy CPU logging.");
  DEBUG_ShowMsg("  monitor write_log_instruction      - write instructions log to disk.");

  DEBUG_GdbWritePacket("");
}

void DEBUG_GdbProcessPackets()
{
  char buffer[1024];
  char *data = DEBUG_GdbReadPacket(buffer, 1024);

  if (!data)
    return;

  switch (data[0]) {
  case 'k':       // Kill
    DEBUG_GdbWritePacket("OK");
    exit(0);
    return;

  case 'D': // Detach
    DEBUG_GdbWritePacket("OK");
    close(gdb_asocket);
    gdb_state = GdbNotConnected;
    return;

  case 'q': // Query
    switch (data[1]) {
    case 'R': {
      unsigned int i;
      char byte[3] = { 0 };

      if (strncmp(data + 2, "cmd", 3))
	break;

      assert(data[5] == ',');

      data += 6;

      for (i = 0; data[i * 2]; i++) {
	memcpy(byte, data + i * 2, 2);
	data[i] = strtoul(byte, 0, 16);
      }

      data[i] = 0;

      if (gdb_remote_debug)
	GDB_REMOTE_LOG("GDB: monitor packet: '%s'\n", data);

      gdb_state = GdbMonitor;
      DEBUG_GdbProcessMonitorPacket(data);
      gdb_state = GdbStoped;
      return;
    }

    }
    DEBUG_GdbWritePacket("");
    return;

  case '?':       // Indicate the reason the target halted
    DEBUG_GdbWritePacket("S05"); // SIGTRAP
    return;

  case 'p': {       // read single register
    unsigned int reg = strtoul(data + 1, 0, 16);
    char fmt[32];
    size_t s = DEBUG_GdbGetRegisterSize(reg);

    if (s == 0)
      break;

    sprintf(fmt, "%%0%ux", s / 4);
    sprintf(buffer, fmt, DEBUG_GdbGetRegister(reg));
    DEBUG_GdbWritePacket(buffer);
    return;
  }

  case 'P': {     // write single register
    char *end;
    unsigned int reg = strtoul(data + 1, &end, 16);
    assert(*end == '=');
    uint32_t value = strtoul(end + 1, 0, 16);
    size_t s = DEBUG_GdbGetRegisterSize(reg);

    if (s == 0)
      break;

    DEBUG_GdbSetRegister(reg, value);
    DEBUG_GdbWritePacket("OK");
    return;
  }

  case 'g': {      // read multiple registers
    char *b = buffer;

    for (unsigned int i = 0; i < 1 /* FIXME */; i++) {
      char fmt[32];
      sprintf(fmt, "%%0%ux", DEBUG_GdbGetRegisterSize(i) / 4);
      b += sprintf(b, fmt, DEBUG_GdbGetRegister(i));
    }
    DEBUG_GdbWritePacket(buffer);
    return;
  }

  case 'G': {       // write multiple registers

    data++;
    for (unsigned int i = 0; i < GDB_REGS_COUNT; i++)	{
      size_t s = DEBUG_GdbGetRegisterSize(i) / 4;
      char word[s + 1];
      word[s] = 0;
      memcpy(word, data, s);
      if (strlen(word) != s)
	break;
      DEBUG_GdbSetRegister(i, strtoul(word, 0, 16));
      data += s;
    }

    DEBUG_GdbWritePacket("OK");
    return;
  }

  case 'm': {     // read memory
    char *end;
    uint32_t addr = strtoul(data + 1, &end, 16);
    assert(*end == ',');
    size_t len = strtoul(end + 1, 0, 16);

    char packet[len * 2 + 1];
    char *b = packet;
    bool err = false;

    if (len % 4 == 0 && addr % 4 == 0) { // use dword access
      for (unsigned int i = 0; i < len / 4; i++) {
	Bit32u value;
	err |= mem_readd_checked(addr + i * 4, &value);
	b += sprintf(b, "%08x", swapByte32(value));
      }

    } else if (len % 2 == 0 && addr % 2 == 0) { // use word access
      for (unsigned int i = 0; i < len / 2; i++) {
	Bit16u value;
	err |= mem_readw_checked(addr + i * 2, &value);
	b += sprintf(b, "%04x", swapByte16(value));
      }

    } else { // use byte access
      for (unsigned int i = 0; i < len ; i++) {
	Bit8u value;
	err |= mem_readb_checked(addr + i, &value);
	b += sprintf(b, "%02x", value);
      }
    }

    DEBUG_GdbWritePacket(err ? "E0d" : packet);
    return;
  }

  case 'M': {      // write memory
    char *end;
    uint32_t addr = strtoul(data + 1, &end, 16);
    assert(*end == ',');
    size_t len = strtoul(end + 1, &end, 16);
    assert(*end == ':');

    char hex[9] = { 0 };
    bool err = false;

    if (len % 4 == 0 && addr % 4 == 0) { // use dword access
      for (unsigned int i = 0; i < len / 4; i++)
	{
	  memcpy(hex, end + 1 + i * 8, 8);
	  err |= mem_writed_checked(addr + i, swapByte32(strtoul(hex, 0, 16)));
	}

    } else if (len % 2 == 0 && addr % 2 == 0) { // use word access
      for (unsigned int i = 0; i < len / 2; i++)
	{
	  memcpy(hex, end + 1 + i * 4, 4);
	  err |= mem_writew_checked(addr + i, swapByte16(strtoul(hex, 0, 16)));
	}

    } else { // use byte access
      for (unsigned int i = 0; i < len; i++)
	{
	  memcpy(hex, end + 1 + i * 2, 2);
	  err |= mem_writeb_checked(addr + i, strtoul(hex, 0, 16));
	}
    }

    DEBUG_GdbWritePacket(err ? "E0d" : "OK");
    return;
  }

  case 'c': {      // continue [optional resume addr in hex]
    if (data[1]) {
      gdbSetEip(strtoul(data + 1, 0, 16));
      step_next_state = GdbRunning;
    } else {
      // go through single step to avoid break-point on resume address
      gdb_state = GdbStep;
      step_next_state = GdbRunning;
      step_eip = gdbGetEip();
      step_cs = SegValue(cs);
    }
    return;
  }

  case 's': {      // continue single step [optional resume addr in hex]
    uint32_t pc;

    if (data[1])
      gdbSetEip(strtoul(data + 1, 0, 16));

    gdb_state = GdbStep;
    step_next_state = GdbStoped;
    step_eip = gdbGetEip();
    step_cs = SegValue(cs);
    return;
  }

  case 'z':       // set and clean break points
  case 'Z': {
    char *end;
    uint32_t addr = strtoul(data + 3, &end, 16);
    assert(*end == ',');
    size_t len = strtoul(end + 1, 0, 16);

    switch (data[1]) {
    case '0':
    case '1': // execution break point
      DEBUG_GdbPointSet(break_points, addr, len, data[0] == 'Z');
      break;

    case '2': // write watch point
      DEBUG_GdbPointSet(write_watch_points, addr, len, data[0] == 'Z');
      break;

    case '4': // access watch point
      DEBUG_GdbPointSet(write_watch_points, addr, len, data[0] == 'Z');
    case '3': // read watch point
      DEBUG_GdbPointSet(read_watch_points, addr, len, data[0] == 'Z');
      break;

    default:
      DEBUG_GdbWritePacket("");
      return;
    }

    DEBUG_GdbWritePacket("OK");
    return;
  }

  default:
    break;
  }

  // empty reply if not supported
  DEBUG_GdbWritePacket("");
}

static bool DEBUG_GdbCheckEvent(void)
{
  struct pollfd pf;
  char c;

  switch (gdb_state) {
  case GdbNotConnected:	// check for incoming connection
    pf.fd = gdb_socket;
    pf.events = POLLIN | POLLPRI;

    if (poll(&pf, 1, 0) > 0) {
      struct sockaddr_in addr;
      socklen_t addr_size = sizeof(addr);

      gdb_asocket = accept(gdb_socket, (struct sockaddr*)&addr, &addr_size);

      if (gdb_asocket >= 0) {
	gdb_state = GdbStoped;
	return true;
      }
    }
    break;

  case GdbStep:
  case GdbRunning:      // try to read CTRL-C from client
    pf.fd = gdb_asocket;
    pf.events = POLLIN | POLLPRI;

    if (poll(&pf, 1, 0) == 1 && read(gdb_asocket, &c, 1) == 1 && c == 3) {
      DEBUG_ShowMsg("GDB: %u: break requested.\n", DEBUG_cycle_count);

      gdb_state = GdbStoped;
      DEBUG_GdbWritePacket("S02"); // sigint
      return true;
    }
    break;
  }

  return false;
}

/**********************/
/* dosbox debug stuff */
/**********************/

void DEBUG_ShowMsg(char const* format,...)
{
  char buf[256];
  va_list ap;
  int i;

  va_start(ap, format);
  i = vsnprintf(buf, sizeof(buf) - 1, format, ap);
  buf[i] = 0;
  va_end(ap);

  // trim spaces at end
  for (; i > 0 && buf[i] <= 32; i--)
    buf[i] = 0;

  // display message on stderr
  fprintf(stderr, "%s\n", buf);

  // send message to gdb client
  if (gdb_state == GdbRunning || gdb_state == GdbStep || gdb_state == GdbMonitor) {
    if (buf[0]) {
      char _p[sizeof(buf) * 2 + 3], *p = _p;
      int i;

      *p++ = 'O';
      for (i = 0; buf[i]; i++)
	p += sprintf(p, "%02x", buf[i]);
      *p++ = '0';
      *p++ = 'a';
      *p++ = 0;

      DEBUG_GdbWritePacket(_p);
    }
  }

}

/* called when irq is raised */
void DEBUG_IrqBreakpoint(Bit8u intNum)
{
  if (gdb_state == GdbRunning && int_bp[intNum]) {
    DEBUG_ShowMsg("GDB: %u: processor hardware interrupt 0x%x.\n", DEBUG_cycle_count, intNum);

    gdb_state = GdbStoped;
    DEBUG_GdbWritePacket("S05"); // trap
    DEBUG_EnableDebugger();
  }
}

/* called when "int n" opcode is encountered */
bool DEBUG_IntBreakpoint(Bit8u intNum)
{
  if (gdb_state == GdbRunning && int_bp[intNum]) {
    DEBUG_ShowMsg("GDB: %u: processor software interrupt 0x%x.\n", DEBUG_cycle_count, intNum);

    gdb_state = GdbStoped;
    DEBUG_GdbWritePacket("S05"); // trap
    return true;
  }

  return false;
}

/* called when "int3" opcode is encountered */
bool DEBUG_Breakpoint(void)
{
  return DEBUG_IntBreakpoint(3);
}

static inline bool DEBUG_GdbPointCheck(const bp_map_t &b, Bit32u addr)
{
  bp_map_t::const_iterator i = b.lower_bound(addr);

  return (i != b.end() && i->first <= addr && i->first + i->second > addr);
}

/* called for each executed instruction */
bool DEBUG_HeavyIsBreakpoint(void)
{
  static Bitu last_event_check = 0;

  void DEBUG_HeavyLogInstruction(void);
  if (DEBUG_logHeavy)
    DEBUG_HeavyLogInstruction();

  /* handle single step end */
  if (gdb_state == GdbStep && (step_eip != gdbGetEip() || step_cs != SegValue(cs))) {
    gdb_state = step_next_state;

    if (gdb_state == GdbStoped) {
      DEBUG_GdbWritePacket("S05"); // trap
      return true;
    }
  }

  /* check execution breakpoints */
  if (gdb_state == GdbRunning) {

    if (DEBUG_GdbPointCheck(break_points, gdbGetEip())) {
      DEBUG_ShowMsg("GDB: %u: hit a breakpoint.\n", DEBUG_cycle_count);

      gdb_state = GdbStoped;
      DEBUG_GdbWritePacket("S05"); // trap
      return true;
    }

    if (cycle_bp && cycle_bp <= DEBUG_cycle_count) {
      DEBUG_ShowMsg("GDB: %u: hit a cycle breakpoint.\n", DEBUG_cycle_count);

      cycle_bp = 0;
      gdb_state = GdbStoped;
      DEBUG_GdbWritePacket("S05"); // trap
      return true;
    }
  }

  /* sometimes check for incoming gdb client connections or CTRL-C from client */
  if (last_event_check + 16384 < DEBUG_cycle_count) {
    last_event_check = DEBUG_cycle_count;
    if (DEBUG_GdbCheckEvent())
      return true;
  }

  return false;
}

void DEBUG_GdbMemReadHook(Bit32u address, int width)
{
  if ((gdb_state == GdbRunning || gdb_state == GdbStep) && DEBUG_GdbPointCheck(read_watch_points, address)) {
    DEBUG_ShowMsg("GDB: %u hit a memory read access watchpoint: address=0x%08x.\n", DEBUG_cycle_count, address);

    gdb_state = GdbStoped;
    DEBUG_EnableDebugger();
    DEBUG_GdbWritePacket("S05"); // trap
  }
}

void DEBUG_GdbMemWriteHook(Bit32u address, int width, Bit32u value)
{
  if ((gdb_state == GdbRunning || gdb_state == GdbStep) && DEBUG_GdbPointCheck(write_watch_points, address)) {
    DEBUG_ShowMsg("GDB: %u: hit a memory write access watchpoint: address=0x%08x, new_value=0x%x.\n", DEBUG_cycle_count, address, value);

    gdb_state = GdbStoped;
    DEBUG_EnableDebugger();
    DEBUG_GdbWritePacket("S05"); // trap
  }
}

Bitu DEBUG_Loop(void)
{ 
  GFX_Events();
  PIC_runIRQs();

  if (gdb_state == GdbStoped)
    DEBUG_GdbProcessPackets();

  if (gdb_state != GdbStoped) {
    DEBUG_exitLoop = false;
    DOSBOX_SetNormalLoop();
  }

  return 0;
}

bool DEBUG_ExitLoop(void)
{
  if (DEBUG_exitLoop) {
    DEBUG_exitLoop = false;
    return true;
  }
  return false;
}

Bitu DEBUG_EnableDebugger(void)
{
  DEBUG_exitLoop = true;
  DOSBOX_SetLoop(&DEBUG_Loop);
  return 0;
}

void DEBUG_ShutDown(Section *sec)
{
  close(gdb_asocket);
  close(gdb_socket);
}

void DEBUG_Init(Section* sec)
{
  /* Setup callback */
  DEBUG_debugCallback=CALLBACK_Allocate();
  CALLBACK_Setup(DEBUG_debugCallback,DEBUG_EnableDebugger,CB_RETF,"debugger");

  /* shutdown function */
  sec->AddDestroyFunction(&DEBUG_ShutDown);

  gdb_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (gdb_socket < 0) {
    LOG_MSG("GDB: Unable to create socket");
    return;
  }

  int tmp = 1;
#ifdef WIN32
  setsockopt(gdb_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)tmp, sizeof(tmp));
#else
  setsockopt(gdb_socket, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(tmp));
#endif

  struct sockaddr_in    addr;

  int i;
  for (int i = 0; ; i++) {
    if (i == 10) {
      LOG_MSG("GDB: Unable to bind socket");
      return;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_port = htons(GDB_TCP_PORT + i);
    addr.sin_family = AF_INET;

    if (bind(gdb_socket, (struct sockaddr*)&addr, sizeof (struct sockaddr_in)) >= 0)
      break;
  }

  if (listen(gdb_socket, 1) < 0) {
    LOG_MSG("GDB: Unable to listen");
    return;
  }

  LOG_MSG("GDB: listening on TCP port %i", GDB_TCP_PORT + i);
}

#endif

