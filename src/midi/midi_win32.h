/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#ifndef DOSBOX_MIDI_WIN32_H
#define DOSBOX_MIDI_WIN32_H

#include "midi_device.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// clang-format off
// 'windows.h' must be included first, otherwise we'll get compilation errors
#include <windows.h>
#include <mmsystem.h>
// clang-format on

#include <string>
#include <sstream>

#include "programs.h"

class MidiDeviceWin32 final : public MidiDevice {
private:
	HMIDIOUT m_out = nullptr;
	MIDIHDR m_hdr = {};
	HANDLE m_event = nullptr;
	bool isOpen;
public:
	MidiDeviceWin32() : MidiDevice(), isOpen(false) {}
	
	MidiDeviceWin32(const MidiDeviceWin32&) = delete;
	MidiDeviceWin32& operator=(const MidiDeviceWin32&) = delete;

	std::string GetName() const override
	{
		return "win32";
	}

	MidiDeviceType GetType() const override
	{
		return MidiDeviceType::External;
	}

	bool Open(const char *conf) override
	{
		if (isOpen) return false;
		m_event = CreateEvent (nullptr, true, true, nullptr);
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
				LOG_MSG("MIDI:WIN32: Selected output device %s",mididev.szPname);
				res = midiOutOpen(&m_out, nummer, (DWORD_PTR)m_event, 0, CALLBACK_EVENT);
			}
		} else {
			res = midiOutOpen(&m_out, MIDI_MAPPER, (DWORD_PTR)m_event, 0, CALLBACK_EVENT);
		}
		if (res != MMSYSERR_NOERROR) return false;
		isOpen=true;
		return true;
	}

	void Close() override
	{
		if (!isOpen) {
			return;
		}

		Reset();

		isOpen = false;
		midiOutClose(m_out);
		CloseHandle(m_event);
	}

	void SendMidiMessage(const MidiMessage& data) override
	{
		const auto status  = data[0];
		const auto data1   = data[1];
		const auto data2   = data[2];
		const uint32_t msg = status + (data1 << 8) + (data2 << 16);

		midiOutShortMsg(m_out, msg);
	}

	void SendSysExMessage(uint8_t *sysex, size_t len) override
	{
		if (WaitForSingleObject (m_event, 2000) == WAIT_TIMEOUT) {
			LOG_WARNING("MIDI:WIN32: Can't send midi message");
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

	MIDI_RC ListAll(Program *caller) override
	{
		unsigned int total = midiOutGetNumDevs();
		for(unsigned int i = 0;i < total;i++) {
			MIDIOUTCAPS mididev;
			midiOutGetDevCaps(i, &mididev, sizeof(MIDIOUTCAPS));
			caller->WriteOut("  %2d - \"%s\"\n", i, mididev.szPname);
		}
		return MIDI_RC::OK;
	}
};

MidiDeviceWin32 Midi_win32;

#endif
