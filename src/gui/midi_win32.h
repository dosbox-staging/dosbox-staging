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

class MidiHandler_win32: public MidiHandler {
private:
	HMIDIOUT m_out;
	MIDIHDR m_hdr;
	HANDLE m_event;
	bool isOpen;
public:
	MidiHandler_win32() : isOpen(false),MidiHandler() {};
	char * GetName(void) { return "win32";};
	bool Open(const char * conf) {
		if (isOpen) return false;
		m_event = CreateEvent (NULL, true, true, NULL);
		MMRESULT res = midiOutOpen(&m_out, MIDI_MAPPER, (DWORD)m_event, 0, CALLBACK_EVENT);
		if (res != MMSYSERR_NOERROR) return false;
		isOpen=true;
		return true;
	};
	void Close(void) {
		if (!isOpen) return;
		isOpen=false;
		midiOutClose(m_out);
		CloseHandle (m_event);
	};
	void PlayMsg(Bit32u msg) {
		midiOutShortMsg(m_out, msg);
	};
	void PlaySysex(Bit8u * sysex,Bitu len) {
		if (WaitForSingleObject (m_event, 2000) == WAIT_TIMEOUT) {
			LOG(LOG_MISC,LOG_ERROR)("Can't send midi message");
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
};

MidiHandler_win32 Midi_win32; 


