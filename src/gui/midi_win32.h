/*
 *  Copyright (C) 2002-2003  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <windows.h>
#include <mmsystem.h>

static HMIDIOUT m_out;
static MIDIHDR m_hdr;
static HANDLE m_event;


static void MIDI_PlaySysex(Bit8u * sysex,Bitu len) {

	if (WaitForSingleObject (m_event, 2000) == WAIT_TIMEOUT) {
		LOG(LOG_MISC|LOG_ERROR,"Can't send midi message");
		return;
	}		
	midiOutUnprepareHeader (m_out, &m_hdr, sizeof (m_hdr));

	m_hdr.lpData = (char *) sysex;
	m_hdr.dwBufferLength = len ;
	m_hdr.dwBytesRecorded = len ;
	m_hdr.dwUser = 0;

	MMRESULT result = midiOutPrepareHeader (m_out, &m_hdr, sizeof (m_hdr));
	if (result != MMSYSERR_NOERROR) return;
	ResetEvent (m_event);
	result = midiOutLongMsg (m_out,&m_hdr,sizeof(m_hdr));
	if (result != MMSYSERR_NOERROR) {
		SetEvent (m_event);
		return;
	}
}

static void MIDI_PlayMsg(Bit32u msg) {
	MMRESULT ret=midiOutShortMsg(m_out, msg);	
}



static bool MIDI_StartUp(void) {
	m_event = CreateEvent (NULL, true, true, NULL);
	MMRESULT res = midiOutOpen(&m_out, MIDI_MAPPER, (DWORD)m_event, 0, CALLBACK_EVENT);
	if (res != MMSYSERR_NOERROR) return false;
	return true;
}



