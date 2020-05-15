/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <string>
#include <sstream>

class MidiHandler_win32: public MidiHandler {
private:
	HMIDIOUT m_out;
	MIDIHDR m_hdr;
	HANDLE m_event;
	bool isOpen;
public:
	MidiHandler_win32() : MidiHandler(),isOpen(false) {};
	const char * GetName(void) { return "win32";};
	bool Open(const char * conf) {
		if (isOpen) return false;
		m_event = CreateEvent (NULL, true, true, NULL);
		MMRESULT res = MMSYSERR_NOERROR;
		if(conf && *conf) {
			std::string strconf(conf);
			std::istringstream configmidi(strconf);
			unsigned int total = midiOutGetNumDevs();
			unsigned int nummer = total;
			configmidi >> nummer;
			if (configmidi.fail() && total) {
				lowcase(strconf);
				for(unsigned int i = 0; i< total;i++) {
					MIDIOUTCAPS mididev;
					midiOutGetDevCaps(i, &mididev, sizeof(MIDIOUTCAPS));
					std::string devname(mididev.szPname);
					lowcase(devname);
					if (devname.find(strconf) != std::string::npos) {
						nummer = i;
						break;
					}
				}
			}

			if (nummer < total) {
				MIDIOUTCAPS mididev;
				midiOutGetDevCaps(nummer, &mididev, sizeof(MIDIOUTCAPS));
				LOG_MSG("MIDI: win32 selected %s",mididev.szPname);
				res = midiOutOpen(&m_out, nummer, (DWORD_PTR)m_event, 0, CALLBACK_EVENT);
			}
		} else {
			res = midiOutOpen(&m_out, MIDI_MAPPER, (DWORD_PTR)m_event, 0, CALLBACK_EVENT);
		}
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
	void PlayMsg(Bit8u * msg) {
		midiOutShortMsg(m_out, *(Bit32u*)msg);
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
	void ListAll(Program* base) {
		unsigned int total = midiOutGetNumDevs();
		for(unsigned int i = 0;i < total;i++) {
			MIDIOUTCAPS mididev;
			midiOutGetDevCaps(i, &mididev, sizeof(MIDIOUTCAPS));
			base->WriteOut("%2d\t \"%s\"\n",i,mididev.szPname);
		}
	}
};

MidiHandler_win32 Midi_win32;


