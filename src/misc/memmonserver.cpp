/*
 *  Memory monitor server -- a small telnet-accessible debug console.
 *
 *  See include/memmonserver.h for what this does. It is a line oriented
 *  command interpreter, not a raw byte stream: a client connects, gets a
 *  banner and a prompt, types a command, gets a reply and another prompt.
 *  Telnet option negotiation bytes are stripped out of the input but
 *  otherwise no character-mode negotiation is attempted -- an ordinary
 *  line-mode telnet client (or plain `nc host port`) works unmodified,
 *  relying on the client's own local line editing.
 *
 *  Memory is read and written straight through the same bounds-checked
 *  accessors the core uses (mem_read*_checked / mem_write*_checked), and
 *  CPU registers are read/set directly via regs.h. This runs on its own
 *  thread and touches emulator state without marshalling onto the
 *  emulation thread, same as the existing GDB stub (src/debug/gdbserver.cpp)
 *  already does -- this tool is meant to be used the same way: interactively,
 *  typically while the emulated CPU is paused in the debugger.
 */

#if defined(WIN32)
/* before dosbox.h, which may drag in the older winsock.h through windows.h */
# include <winsock2.h>
# include <ws2tcpip.h>
#endif

#include "config/config.h"

#include "dosbox.h"
#include "logging.h"
#include "config/setup.h"
#include "control.h"
#include "cpu/registers.h"
#include "cpu/paging.h"
#include "memmonserver.h"

#include <string>
#include <vector>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <thread>

#if defined(WIN32)
typedef SOCKET mm_socket_t;
# define MM_INVALID_SOCKET  INVALID_SOCKET
# define mm_closesocket(s)  closesocket(s)
# define mm_sockerr()       WSAGetLastError()
# define MM_EWOULDBLOCK     WSAEWOULDBLOCK
typedef int mm_socklen_t;
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <arpa/inet.h>
# include <unistd.h>
# include <fcntl.h>
# include <errno.h>
typedef int mm_socket_t;
# define MM_INVALID_SOCKET  (-1)
# define mm_closesocket(s)  close(s)
# define mm_sockerr()       errno
# define MM_EWOULDBLOCK     EWOULDBLOCK
typedef socklen_t mm_socklen_t;
#endif

/* telnet protocol bytes we filter out of the input (RFC 854) */
#define TEL_SE      240
#define TEL_SB      250
#define TEL_WILL    251
#define TEL_WONT    252
#define TEL_DO      253
#define TEL_DONT    254
#define TEL_IAC     255

#define MM_POLL_MS          20      /* select() timeout */
#define MM_MAX_LINE         4096    /* longest command line accepted */
#define MM_MAX_PENDING      (1u<<20)/* per client send backlog before we give up */
#define MM_DEFAULT_DUMP_LEN 128
#define MM_MAX_DUMP_LEN     (64u*1024u)
#define MM_MAX_FILL_LEN     (16u*1024u*1024u)
#define MM_MAX_FIND_LEN     (16u*1024u*1024u)
#define MM_MAX_FIND_HITS    64

static int          mm_port = 2161;
static std::string  mm_bindaddress = "127.0.0.1";
static volatile bool mm_running = false;

static std::thread  mm_thread;
static volatile bool mm_thread_quit = false;

/* ------------------------------------------------------------------------- */
/* small parsing helpers -- everything numeric is hex, "0x" prefix optional  */
/* ------------------------------------------------------------------------- */

static std::vector<std::string> mm_split(const std::string &line) {
    std::vector<std::string> out;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

static bool mm_parse_hex(const std::string &s, uint64_t &value) {
    if (s.empty()) return false;
    const char *p = s.c_str();
    if (s.size() > 1 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    if (!*p) return false;
    char *end = NULL;
    unsigned long long v = strtoull(p, &end, 16);
    if (end == p || *end != '\0') return false;
    value = (uint64_t)v;
    return true;
}

/* "1234" -> that value as a flat/physical address.
 * "40:100" -> real-mode style segment:offset, linear = seg*16+off. */
static bool mm_parse_address(const std::string &s, uint32_t &addr) {
    size_t colon = s.find(':');
    if (colon == std::string::npos) {
        uint64_t v;
        if (!mm_parse_hex(s, v)) return false;
        addr = (uint32_t)v;
        return true;
    }
    uint64_t seg, off;
    if (!mm_parse_hex(s.substr(0, colon), seg)) return false;
    if (!mm_parse_hex(s.substr(colon + 1), off)) return false;
    addr = (uint32_t)((seg * 16) + off);
    return true;
}

/* ------------------------------------------------------------------------- */
/* register access, by name -- mirrors DEBUG_GetRegister/SetRegister         */
/* (src/debug/debug.cpp), reimplemented here so this tool does not depend   */
/* on the C_DEBUG-only debugger build.                                      */
/* ------------------------------------------------------------------------- */

static const char *mm_reg_names[] = {
    "eax","ebx","ecx","edx","esi","edi","esp","ebp","eip","eflags",
    "cs","ss","ds","es","fs","gs",
};

static bool mm_get_register(const std::string &name, uint32_t &value) {
    if (name == "eax") { value = reg_eax; return true; }
    if (name == "ebx") { value = reg_ebx; return true; }
    if (name == "ecx") { value = reg_ecx; return true; }
    if (name == "edx") { value = reg_edx; return true; }
    if (name == "esi") { value = reg_esi; return true; }
    if (name == "edi") { value = reg_edi; return true; }
    if (name == "esp") { value = reg_esp; return true; }
    if (name == "ebp") { value = reg_ebp; return true; }
    if (name == "eip") { value = reg_eip; return true; }
    if (name == "eflags") { value = reg_flags; return true; }
    if (name == "cs") { value = SegValue(cs); return true; }
    if (name == "ss") { value = SegValue(ss); return true; }
    if (name == "ds") { value = SegValue(ds); return true; }
    if (name == "es") { value = SegValue(es); return true; }
    if (name == "fs") { value = SegValue(fs); return true; }
    if (name == "gs") { value = SegValue(gs); return true; }
    return false;
}

static bool mm_set_register(const std::string &name, uint32_t value) {
    if (name == "eax") { reg_eax = value; return true; }
    if (name == "ebx") { reg_ebx = value; return true; }
    if (name == "ecx") { reg_ecx = value; return true; }
    if (name == "edx") { reg_edx = value; return true; }
    if (name == "esi") { reg_esi = value; return true; }
    if (name == "edi") { reg_edi = value; return true; }
    if (name == "esp") { reg_esp = value; return true; }
    if (name == "ebp") { reg_ebp = value; return true; }
    if (name == "eip") { reg_eip = value; return true; }
    if (name == "eflags") { reg_flags = value; return true; }
    if (name == "cs") { SegSet16(cs, (uint16_t)value); return true; }
    if (name == "ss") { SegSet16(ss, (uint16_t)value); return true; }
    if (name == "ds") { SegSet16(ds, (uint16_t)value); return true; }
    if (name == "es") { SegSet16(es, (uint16_t)value); return true; }
    if (name == "fs") { SegSet16(fs, (uint16_t)value); return true; }
    if (name == "gs") { SegSet16(gs, (uint16_t)value); return true; }
    return false;
}

static std::string mm_flags_decode(uint32_t fl) {
    char buf[160];
    snprintf(buf, sizeof(buf),
        "CF=%u PF=%u AF=%u ZF=%u SF=%u TF=%u IF=%u DF=%u OF=%u",
        (fl>>0)&1,(fl>>2)&1,(fl>>4)&1,(fl>>6)&1,(fl>>7)&1,
        (fl>>8)&1,(fl>>9)&1,(fl>>10)&1,(fl>>11)&1);
    return std::string(buf);
}

/* ------------------------------------------------------------------------- */
/* memory access -- thin wrappers with the sane boolean convention           */
/*                                                                           */
/* NTS: mem_read*_checked()/mem_write*_checked() (include/paging.h) return   */
/* true on FAULT and false on SUCCESS (see PageHandler::readb_checked et al. */
/* in src/cpu/paging.cpp, and how src/debug/debug.cpp's DEBUG_ReadMemory     */
/* uses them) -- the opposite of what the name suggests. Route every access  */
/* through these wrappers, which flip the sense back to true=ok, so the rest */
/* of this file can read naturally.                                         */
/* ------------------------------------------------------------------------- */

static inline bool mm_read_byte(const PhysPt addr, uint8_t *val)    { return !mem_readb_checked(addr, val); }
static inline bool mm_read_word(const PhysPt addr, uint16_t *val)   { return !mem_readw_checked(addr, val); }
static inline bool mm_read_dword(const PhysPt addr, uint32_t *val)  { return !mem_readd_checked(addr, val); }
static inline bool mm_write_byte(const PhysPt addr, const uint8_t val)   { return !mem_writeb_checked(addr, val); }
static inline bool mm_write_word(const PhysPt addr, const uint16_t val)  { return !mem_writew_checked(addr, val); }
static inline bool mm_write_dword(const PhysPt addr, const uint32_t val) { return !mem_writed_checked(addr, val); }

/* ------------------------------------------------------------------------- */
/* command implementations                                                   */
/* ------------------------------------------------------------------------- */

static const char *MM_HELP =
    "Commands (all numbers are hex; address is either FLATADDR or SEG:OFF):\r\n"
    "  d [addr] [len]          hex+ascii dump (default: continues, len=0x80)\r\n"
    "  rb/rw/rd addr           read a byte/word/dword\r\n"
    "  wb/ww/wd addr v [v...]  write byte(s)/word(s)/dword(s) sequentially\r\n"
    "  fill addr len val       fill len bytes with val\r\n"
    "  find addr len b0 [b1..] search for a byte pattern in [addr,addr+len)\r\n"
    "  regs                    show CPU registers\r\n"
    "  set reg value           set a register (eax..edi,esp,ebp,eip,eflags,\r\n"
    "                          cs,ss,ds,es,fs,gs)\r\n"
    "  info                    show total emulated memory size\r\n"
    "  help                    this text\r\n"
    "  quit                    close the connection\r\n";

struct MMClient {
    mm_socket_t fd;
    std::string pending;       /* bytes not yet written to the socket */
    std::string linebuf;       /* bytes accumulated since the last newline */
    int  telnet_state;         /* TS_* below */
    bool closing;               /* send pending output, then drop */

    uint32_t last_addr;
    uint32_t last_len;

    MMClient(mm_socket_t s) :
        fd(s), telnet_state(0), closing(false),
        last_addr(0), last_len(MM_DEFAULT_DUMP_LEN) {}
};

enum { TS_DATA=0, TS_IAC, TS_OPTION, TS_SB, TS_SB_IAC };

static void mm_reply(MMClient &cl, const std::string &s) {
    cl.pending += s;
}

static void mm_prompt(MMClient &cl) {
    cl.pending += "mem> ";
}

static void mm_cmd_dump(MMClient &cl, const std::vector<std::string> &args, std::string &out) {
    uint32_t addr = cl.last_addr;
    uint32_t len  = cl.last_len;

    if (args.size() >= 2) {
        uint32_t a;
        if (!mm_parse_address(args[1], a)) { out += "bad address\r\n"; return; }
        addr = a;
    }
    if (args.size() >= 3) {
        uint64_t l;
        if (!mm_parse_hex(args[2], l)) { out += "bad length\r\n"; return; }
        if (l == 0) l = 1;
        if (l > MM_MAX_DUMP_LEN) l = MM_MAX_DUMP_LEN;
        len = (uint32_t)l;
    }

    char line[128];
    uint32_t done = 0;
    while (done < len) {
        uint32_t row = addr + done;
        char hex[16*3+2]; hex[0] = '\0';
        char asc[17]; asc[0] = '\0';
        unsigned ai = 0;

        for (unsigned i = 0; i < 16 && done < len; i++, done++) {
            uint8_t v = 0;
            bool ok = mm_read_byte(row + i, &v);
            char piece[5];
            if (ok) snprintf(piece, sizeof(piece), "%02x%s", v, (i==7)?"-":" ");
            else    snprintf(piece, sizeof(piece), "??%s", (i==7)?"-":" ");
            strcat(hex, piece);
            asc[ai++] = (ok && v >= 0x20 && v < 0x7F) ? (char)v : '.';
        }
        asc[ai] = '\0';
        snprintf(line, sizeof(line), "%08x  %-50s %s\r\n", row, hex, asc);
        out += line;
    }

    cl.last_addr = addr + len;
    cl.last_len = len;
}

static void mm_cmd_read(MMClient &cl, const std::string &cmd,
                         const std::vector<std::string> &args, std::string &out) {
    (void)cl;
    if (args.size() < 2) { out += "usage: " + cmd + " addr\r\n"; return; }
    uint32_t addr;
    if (!mm_parse_address(args[1], addr)) { out += "bad address\r\n"; return; }

    char buf[64];
    if (cmd == "rb") {
        uint8_t v;
        if (!mm_read_byte(addr, &v)) { out += "** read fault **\r\n"; return; }
        snprintf(buf, sizeof(buf), "%08x: %02x\r\n", addr, v);
    } else if (cmd == "rw") {
        uint16_t v;
        if (!mm_read_word(addr, &v)) { out += "** read fault **\r\n"; return; }
        snprintf(buf, sizeof(buf), "%08x: %04x\r\n", addr, v);
    } else { /* rd */
        uint32_t v;
        if (!mm_read_dword(addr, &v)) { out += "** read fault **\r\n"; return; }
        snprintf(buf, sizeof(buf), "%08x: %08x\r\n", addr, v);
    }
    out += buf;
}

static void mm_cmd_write(MMClient &cl, const std::string &cmd,
                          const std::vector<std::string> &args, std::string &out) {
    (void)cl;
    if (args.size() < 3) { out += "usage: " + cmd + " addr value [value...]\r\n"; return; }
    uint32_t addr;
    if (!mm_parse_address(args[1], addr)) { out += "bad address\r\n"; return; }

    unsigned step = (cmd == "wb") ? 1 : (cmd == "ww") ? 2 : 4;
    uint64_t limit = (step == 1) ? 0xFFull : (step == 2) ? 0xFFFFull : 0xFFFFFFFFull;

    int written = 0;
    for (size_t i = 2; i < args.size(); i++) {
        uint64_t v;
        if (!mm_parse_hex(args[i], v) || v > limit) {
            out += "bad value '" + args[i] + "'\r\n";
            break;
        }
        bool ok;
        if (step == 1) ok = mm_write_byte(addr, (uint8_t)v);
        else if (step == 2) ok = mm_write_word(addr, (uint16_t)v);
        else ok = mm_write_dword(addr, (uint32_t)v);

        if (!ok) {
            char buf[64];
            snprintf(buf, sizeof(buf), "** write fault at %08x **\r\n", addr);
            out += buf;
            break;
        }
        addr += step;
        written++;
    }

    char buf[48];
    snprintf(buf, sizeof(buf), "wrote %d value(s)\r\n", written);
    out += buf;
}

static void mm_cmd_fill(const std::vector<std::string> &args, std::string &out) {
    if (args.size() < 4) { out += "usage: fill addr len val\r\n"; return; }
    uint32_t addr;
    uint64_t len, val;
    if (!mm_parse_address(args[1], addr)) { out += "bad address\r\n"; return; }
    if (!mm_parse_hex(args[2], len)) { out += "bad length\r\n"; return; }
    if (!mm_parse_hex(args[3], val) || val > 0xFF) { out += "bad value (must be a byte)\r\n"; return; }
    if (len == 0) { out += "nothing to do\r\n"; return; }
    if (len > MM_MAX_FILL_LEN) len = MM_MAX_FILL_LEN;

    uint32_t i;
    for (i = 0; i < (uint32_t)len; i++) {
        if (!mm_write_byte(addr + i, (uint8_t)val)) break;
    }
    char buf[64];
    if (i < (uint32_t)len)
        snprintf(buf, sizeof(buf), "** write fault after %u byte(s) **\r\n", i);
    else
        snprintf(buf, sizeof(buf), "filled %u byte(s)\r\n", i);
    out += buf;
}

static void mm_cmd_find(const std::vector<std::string> &args, std::string &out) {
    if (args.size() < 4) { out += "usage: find addr len b0 [b1...]\r\n"; return; }
    uint32_t addr;
    uint64_t len;
    if (!mm_parse_address(args[1], addr)) { out += "bad address\r\n"; return; }
    if (!mm_parse_hex(args[2], len)) { out += "bad length\r\n"; return; }
    if (len > MM_MAX_FIND_LEN) len = MM_MAX_FIND_LEN;

    std::vector<uint8_t> pattern;
    for (size_t i = 3; i < args.size(); i++) {
        uint64_t v;
        if (!mm_parse_hex(args[i], v) || v > 0xFF) { out += "bad pattern byte '" + args[i] + "'\r\n"; return; }
        pattern.push_back((uint8_t)v);
    }
    if (pattern.empty()) { out += "empty pattern\r\n"; return; }

    int hits = 0;
    uint64_t scanned = 0;
    while (scanned + pattern.size() <= len) {
        bool match = true;
        for (size_t i = 0; i < pattern.size() && match; i++) {
            uint8_t v;
            if (!mm_read_byte((uint32_t)(addr + scanned + i), &v) || v != pattern[i])
                match = false;
        }
        if (match) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%08x\r\n", (uint32_t)(addr + scanned));
            out += buf;
            hits++;
            if (hits >= MM_MAX_FIND_HITS) { out += "(stopping, too many matches)\r\n"; break; }
        }
        scanned++;
    }
    if (hits == 0) out += "no match\r\n";
}

static void mm_cmd_regs(std::string &out) {
    char buf[512];
    snprintf(buf, sizeof(buf),
        "eax=%08x ebx=%08x ecx=%08x edx=%08x\r\n"
        "esi=%08x edi=%08x esp=%08x ebp=%08x\r\n"
        "eip=%08x eflags=%08x  (%s)\r\n"
        "cs=%04x ss=%04x ds=%04x es=%04x fs=%04x gs=%04x\r\n"
        "cs:eip -> linear %08x\r\n",
        reg_eax, reg_ebx, reg_ecx, reg_edx,
        reg_esi, reg_edi, reg_esp, reg_ebp,
        reg_eip, (uint32_t)reg_flags, mm_flags_decode((uint32_t)reg_flags).c_str(),
        SegValue(cs), SegValue(ss), SegValue(ds), SegValue(es), SegValue(fs), SegValue(gs),
        SegPhys(cs) + reg_eip);
    out += buf;
}

static void mm_cmd_set(const std::vector<std::string> &args, std::string &out) {
    if (args.size() < 3) {
        out += "usage: set reg value   (regs: ";
        size_t n = sizeof(mm_reg_names)/sizeof(mm_reg_names[0]);
        for (size_t i = 0; i < n; i++) {
            out += mm_reg_names[i];
            if (i + 1 < n) out += ",";
        }
        out += ")\r\n";
        return;
    }
    std::string name = args[1];
    for (size_t i = 0; i < name.size(); i++) name[i] = (char)tolower((unsigned char)name[i]);

    uint64_t v;
    if (!mm_parse_hex(args[2], v)) { out += "bad value\r\n"; return; }
    if (!mm_set_register(name, (uint32_t)v)) { out += "unknown register '" + args[1] + "'\r\n"; return; }

    uint32_t check = 0;
    mm_get_register(name, check);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s = %08x\r\n", name.c_str(), check);
    out += buf;
}

static void mm_cmd_info(std::string &out) {
    char buf[128];
    Bitu pages = MEM_TotalPages();
    snprintf(buf, sizeof(buf), "total emulated memory: %u KB (%u pages of 4 KB)\r\n",
             (unsigned)(pages * 4), (unsigned)pages);
    out += buf;
}

/* Execute one already-trimmed command line. Returns false if the client
 * asked to close the connection. */
static bool mm_execute(MMClient &cl, const std::string &line, std::string &out) {
    std::vector<std::string> args = mm_split(line);
    if (args.empty()) return true;

    std::string cmd = args[0];
    for (size_t i = 0; i < cmd.size(); i++) cmd[i] = (char)tolower((unsigned char)cmd[i]);

    if (cmd == "quit" || cmd == "exit" || cmd == "bye") {
        out += "bye\r\n";
        return false;
    } else if (cmd == "help" || cmd == "?") {
        out += MM_HELP;
    } else if (cmd == "d" || cmd == "dump") {
        mm_cmd_dump(cl, args, out);
    } else if (cmd == "rb" || cmd == "rw" || cmd == "rd") {
        mm_cmd_read(cl, cmd, args, out);
    } else if (cmd == "wb" || cmd == "ww" || cmd == "wd") {
        mm_cmd_write(cl, cmd, args, out);
    } else if (cmd == "fill") {
        mm_cmd_fill(args, out);
    } else if (cmd == "find") {
        mm_cmd_find(args, out);
    } else if (cmd == "regs") {
        mm_cmd_regs(out);
    } else if (cmd == "set") {
        mm_cmd_set(args, out);
    } else if (cmd == "info") {
        mm_cmd_info(out);
    } else {
        out += "unknown command '" + cmd + "' (try 'help')\r\n";
    }
    return true;
}

/* Strip telnet negotiation, accumulate a line, and execute it on '\n'. */
static void mm_feed(MMClient &cl, const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len && !cl.closing; i++) {
        uint8_t c = buf[i];
        bool data_byte = false;

        switch (cl.telnet_state) {
        case TS_DATA:
            if (c == TEL_IAC) cl.telnet_state = TS_IAC;
            else data_byte = true;
            break;
        case TS_IAC:
            if (c == TEL_IAC) { cl.telnet_state = TS_DATA; data_byte = true; }  /* doubled IAC = literal 0xFF */
            else if (c == TEL_WILL || c == TEL_WONT || c == TEL_DO || c == TEL_DONT) cl.telnet_state = TS_OPTION;
            else if (c == TEL_SB) cl.telnet_state = TS_SB;
            else cl.telnet_state = TS_DATA;   /* a two byte command, nothing to do */
            break;
        case TS_OPTION:
            cl.telnet_state = TS_DATA;
            break;
        case TS_SB:
            if (c == TEL_IAC) cl.telnet_state = TS_SB_IAC;
            break;
        case TS_SB_IAC:
            cl.telnet_state = (c == TEL_SE) ? TS_DATA : TS_SB;
            break;
        }

        if (!data_byte) continue;

        /* plain data byte */
        if (c == '\r') continue;                 /* swallow, '\n' ends the line */
        if (c == 0x08 || c == 0x7F) {             /* backspace / DEL, for raw non-telnet clients */
            if (!cl.linebuf.empty()) cl.linebuf.erase(cl.linebuf.size() - 1);
            continue;
        }
        if (c == '\n') {
            std::string out;
            bool keep_open = mm_execute(cl, cl.linebuf, out);
            cl.linebuf.clear();
            mm_reply(cl, out);
            if (!keep_open) cl.closing = true;
            else mm_prompt(cl);
            continue;
        }
        if (cl.linebuf.size() < MM_MAX_LINE) cl.linebuf += (char)c;
    }
}

/* ------------------------------------------------------------------------- */
/* network thread -- structurally the same accept/select/flush loop as      */
/* src/misc/ttyserver.cpp, just with a different per-byte handler.          */
/* ------------------------------------------------------------------------- */

static void mm_set_nonblocking(mm_socket_t s) {
#if defined(WIN32)
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    if (flags >= 0) fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}

static mm_socket_t mm_listen(void) {
    struct sockaddr_in addr;
    mm_socket_t s;
    int opt = 1;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == MM_INVALID_SOCKET) {
        LOG_MSG("Memory monitor: cannot create socket (error %d)", mm_sockerr());
        return MM_INVALID_SOCKET;
    }

    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)mm_port);
    if (mm_bindaddress.empty() || mm_bindaddress == "any")
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    else
        addr.sin_addr.s_addr = inet_addr(mm_bindaddress.c_str());

    if (addr.sin_addr.s_addr == INADDR_NONE) {
        LOG_MSG("Memory monitor: bad bindaddress '%s'", mm_bindaddress.c_str());
        mm_closesocket(s);
        return MM_INVALID_SOCKET;
    }

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        LOG_MSG("Memory monitor: cannot bind %s:%d (error %d)",
                mm_bindaddress.c_str(), mm_port, mm_sockerr());
        mm_closesocket(s);
        return MM_INVALID_SOCKET;
    }

    if (listen(s, 4) != 0) {
        LOG_MSG("Memory monitor: cannot listen (error %d)", mm_sockerr());
        mm_closesocket(s);
        return MM_INVALID_SOCKET;
    }

    mm_set_nonblocking(s);
    LOG_MSG("Memory monitor: listening on %s:%d", mm_bindaddress.c_str(), mm_port);
    return s;
}

static const char *MM_BANNER =
    "DOSBox-X memory monitor. Type 'help' for commands, 'quit' to disconnect.\r\n";

/* Returns false when the client should be dropped. */
static bool mm_flush(MMClient &cl) {
    while (!cl.pending.empty()) {
        int n = (int)send(cl.fd, cl.pending.data(), (int)cl.pending.size(), 0);
        if (n > 0) { cl.pending.erase(0, (size_t)n); continue; }
        if (n == 0) return false;
        if (mm_sockerr() == MM_EWOULDBLOCK) break;
        return false;
    }
    if (cl.closing && cl.pending.empty()) return false;
    return cl.pending.size() < MM_MAX_PENDING;
}

static void mm_thread_func(void) {
    mm_socket_t listen_fd = mm_listen();
    std::vector<MMClient> clients;

    /* NTS: on failure just return; shutdown still joins this thread. */
    if (listen_fd == MM_INVALID_SOCKET) return;

    while (!mm_thread_quit) {
        fd_set rfds;
        struct timeval tv;
        mm_socket_t maxfd = listen_fd;

        FD_ZERO(&rfds);
        FD_SET(listen_fd, &rfds);
        for (size_t i = 0; i < clients.size(); i++) {
            FD_SET(clients[i].fd, &rfds);
            if (clients[i].fd > maxfd) maxfd = clients[i].fd;
        }

        tv.tv_sec = 0;
        tv.tv_usec = MM_POLL_MS * 1000;
        if (select((int)maxfd + 1, &rfds, NULL, NULL, &tv) < 0) {
            if (mm_sockerr() == MM_EWOULDBLOCK) continue;
#if !defined(WIN32)
            if (mm_sockerr() == EINTR) continue;
#endif
            break;
        }

        if (FD_ISSET(listen_fd, &rfds)) {
            struct sockaddr_in peer;
            mm_socklen_t peerlen = sizeof(peer);
            mm_socket_t c = accept(listen_fd, (struct sockaddr*)&peer, &peerlen);
            if (c != MM_INVALID_SOCKET) {
                int one = 1;
                mm_set_nonblocking(c);
                setsockopt(c, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));
                clients.push_back(MMClient(c));
                mm_reply(clients.back(), MM_BANNER);
                mm_prompt(clients.back());
                LOG_MSG("Memory monitor: client connected from %s", inet_ntoa(peer.sin_addr));
            }
        }

        for (size_t i = 0; i < clients.size(); ) {
            bool drop = false;

            if (FD_ISSET(clients[i].fd, &rfds)) {
                uint8_t buf[512];
                int n = (int)recv(clients[i].fd, (char*)buf, sizeof(buf), 0);
                if (n > 0) mm_feed(clients[i], buf, (size_t)n);
                else if (n == 0) drop = true;
                else if (mm_sockerr() != MM_EWOULDBLOCK) drop = true;
            }

            if (!drop && !mm_flush(clients[i])) drop = true;

            if (drop) {
                LOG_MSG("Memory monitor: client disconnected");
                mm_closesocket(clients[i].fd);
                clients.erase(clients.begin() + (ptrdiff_t)i);
            } else i++;
        }
    }

    for (size_t i = 0; i < clients.size(); i++) mm_closesocket(clients[i].fd);
    mm_closesocket(listen_fd);
}

/* ------------------------------------------------------------------------- */
/* setup / teardown                                                          */
/* ------------------------------------------------------------------------- */

static void MEMMON_Destroy(void) {
    if (!mm_running) return;

    mm_running = false;
    mm_thread_quit = true;
    if (mm_thread.joinable()) mm_thread.join();
}


void MEMMON_AddConfigSection(const ConfigPtr& conf) {
	assert(conf);

    using enum Property::Changeable::Value;

	auto section = conf->AddSection("memmonserver");

	auto pbool = section->AddBool("memmonserver",
	                            Property::Changeable::OnlyAtStart,
	                            false);
	pbool->SetOptionHelp(
	        "Enable the memory monitor server. It is a small line-command console, reachable with an\n"
	        "ordinary telnet client on the TCP port below, for inspecting and modifying the emulated\n"
	        "machine's memory and CPU registers (hex dump, byte/word/dword read and write, fill,\n"
	        "search, and register display/edit). Type 'help' after connecting for the command list.\n"
	        "There is no authentication, so only open this up on a network you trust.");

	auto pint = section->AddInt("port", Property::Changeable::OnlyAtStart, 2161);
	pint->SetOptionHelp("TCP port the memory monitor server listens on.");

	auto pstring = section->AddString("bindaddress",
	                                 Property::Changeable::OnlyAtStart,
	                                 "127.0.0.1");
	pstring->SetOptionHelp(
	        "Address the memory monitor server binds to. The default only accepts connections from\n"
	        "this machine; use 'any' to accept connections from the network.");
}

void MEMMON_Init(void) {
	assert(get_section("memmonserver") != NULL);
	SectionProp* section = static_cast<SectionProp*>(get_section("memmonserver"));
    if (section == NULL) return;

    if (!section->GetBool("memmonserver")) return;

    LOG(LOG_MISC, LOG_NORMAL)("Initializing memory monitor (telnet) server");

    mm_port = section->GetInt("port");
    mm_bindaddress = section->GetString("bindaddress");

    if (mm_port <= 0 || mm_port > 65535) {
        LOG_MSG("Memory monitor: port %d out of range, not starting", mm_port);
        return;
    }

#if defined(WIN32)
    {
        /* refcounted; other components (ttyserver, serial passthrough, ...)
         * may have started it already */
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
            LOG_MSG("Memory monitor: WSAStartup failed, not starting");
            return;
        }
    }
#endif

    mm_thread_quit = false;
    mm_running = true;
    mm_thread = std::thread(mm_thread_func);
}
