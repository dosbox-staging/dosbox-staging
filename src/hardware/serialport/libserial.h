// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_LIBSERIAL_H
#define DOSBOX_LIBSERIAL_H

#include <cstddef>

typedef struct _COMPORT *COMPORT;

bool SERIAL_open(const char* portname, COMPORT* port);
void SERIAL_close(COMPORT port);
void SERIAL_getErrorString(char* buffer, size_t length);

#define SERIAL_1STOP 1
#define SERIAL_2STOP 2
#define SERIAL_15STOP 0

// parity: n, o, e, m, s

bool SERIAL_setCommParameters(COMPORT port,
			int baudrate, char parity, int stopbits, int length);

void SERIAL_setDTR(COMPORT port, bool value);
void SERIAL_setRTS(COMPORT port, bool value);
void SERIAL_setBREAK(COMPORT port, bool value);

#define SERIAL_CTS 0x10
#define SERIAL_DSR 0x20
#define SERIAL_RI 0x40
#define SERIAL_CD 0x80

int SERIAL_getmodemstatus(COMPORT port);
bool SERIAL_setmodemcontrol(COMPORT port, int flags);

bool SERIAL_sendchar(COMPORT port, char data);

// 0-7 char data, higher=flags
#define SERIAL_BREAK_ERR 0x10
#define SERIAL_FRAMING_ERR 0x08
#define SERIAL_PARITY_ERR 0x04
#define SERIAL_OVERRUN_ERR 0x02

int SERIAL_getextchar(COMPORT port);

#endif
