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

#include <AudioUnit/AudioUnit.h>

class MidiHandler_coreaudio : public MidiHandler {
private:
	AudioUnit m_musicDevice;
	AudioUnit m_outputUnit;
public:
	MidiHandler_coreaudio() : m_musicDevice(0), m_outputUnit(0) {}
	char * GetName(void) { return "coreaudio"; }
	bool Open(const char * conf) {
		int err;
		AudioUnitConnection auconnect;
		ComponentDescription compdesc;
		Component compid;
	
		if (m_outputUnit)
			return false;
		
		// Open the Music Device
		compdesc.componentType = kAudioUnitComponentType;
		compdesc.componentSubType = kAudioUnitSubType_MusicDevice;
		compdesc.componentManufacturer = kAudioUnitID_DLSSynth;
		compdesc.componentFlags = 0;
		compdesc.componentFlagsMask = 0;
		compid = FindNextComponent(NULL, &compdesc);
		m_musicDevice = (AudioUnit) OpenComponent(compid);
	
		// open the output unit
		m_outputUnit = (AudioUnit) OpenDefaultComponent(kAudioUnitComponentType, kAudioUnitSubType_Output);
	
		// connect the units
		auconnect.sourceAudioUnit = m_musicDevice;
		auconnect.sourceOutputNumber = 0;
		auconnect.destInputNumber = 0;
		err =
			AudioUnitSetProperty(m_outputUnit, kAudioUnitProperty_MakeConnection, kAudioUnitScope_Input, 0,
													 (void *)&auconnect, sizeof(AudioUnitConnection));
	
		// initialize the units
		AudioUnitInitialize(m_musicDevice);
		AudioUnitInitialize(m_outputUnit);
	
		// start the output
		AudioOutputUnitStart(m_outputUnit);
	
		return true;
	}
	
	void Close(void) {
		if (m_outputUnit) {
			AudioOutputUnitStop(m_outputUnit);
			CloseComponent(m_outputUnit);
			m_outputUnit = 0;
		}
		if (m_musicDevice) {
			CloseComponent(m_musicDevice);
			m_musicDevice = 0;
		}
	}
	
	void PlayMsg(Bit32u msg) {
		MusicDeviceMIDIEvent(m_musicDevice,
				(msg & 0x000000FF),
				(msg & 0x0000FF00) >> 8,
				(msg & 0x00FF0000) >> 16,
				0);
	}
	
	void PlaySysex(Bit8u * sysex, Bitu len) {
		MusicDeviceSysEx(m_musicDevice, sysex, len);
	}
};

MidiHandler_coreaudio Midi_coreaudio;
