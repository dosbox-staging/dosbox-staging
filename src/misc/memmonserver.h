/*
 *  Memory monitor server: a telnet-accessible console for inspecting and
 *  manipulating the emulated machine's memory and CPU registers.
 *
 *  Unlike the GDB stub, this is a small human-friendly line command
 *  interpreter (hex dump, byte/word/dword read & write, fill, search,
 *  register display and modification) aimed at being driven interactively
 *  with a telnet client, or scripted by a simple external tool.
 *
 *  Configured through the [memmonserver] section of the configuration file.
 */

#ifndef DOSBOX_MEMMONSERVER_H
#define DOSBOX_MEMMONSERVER_H

/* Set up the [memmonserver] section handlers. Called once from the init sequence. */
void MEMMON_Init(void);
void MEMMON_Destroy(void);
void MEMMON_AddConfigSection(const ConfigPtr& conf);
#endif
