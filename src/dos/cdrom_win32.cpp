/*
 *  Copyright (C) 2024-2024  The DOSBox Team
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

#include "cdrom.h"
#include "checks.h"
#include "string_utils.h"
#include "support.h"

#if defined(WIN32)

#include <ntddcdrm.h>
#include <windows.h>

CHECK_NARROWING();

bool CDROM_Interface_Win32::IsOpen() const
{
	return cdrom_handle != INVALID_HANDLE_VALUE;
}

CDROM_Interface_Win32::~CDROM_Interface_Win32()
{
	if (IsOpen()) {
		CloseHandle(cdrom_handle);
	}
	cdrom_handle = INVALID_HANDLE_VALUE;
}

bool CDROM_Interface_Win32::Open(const char drive_letter)
{
	const std::string device_path = std::string("\\\\.\\") + drive_letter + ':';

	LPCSTR file_name     = device_path.c_str();
	DWORD desired_access = GENERIC_READ;
	DWORD share_mode     = FILE_SHARE_READ | FILE_SHARE_WRITE;
	LPSECURITY_ATTRIBUTES securiy_attributes = NULL;
	DWORD creation_disposition               = OPEN_EXISTING;
	DWORD flags                              = 0;
	HANDLE template_file                     = NULL;

	HANDLE device = CreateFileA(file_name,
	                            desired_access,
	                            share_mode,
	                            securiy_attributes,
	                            creation_disposition,
	                            flags,
	                            template_file);
	if (device == INVALID_HANDLE_VALUE) {
		return false;
	}

	// Test to make sure this device is a CDROM drive
	CDROM_TOC toc = {};

	DWORD control_code       = IOCTL_CDROM_READ_TOC;
	LPVOID input_buffer      = NULL;
	DWORD input_buffer_size  = 0;
	LPVOID output_buffer     = &toc;
	DWORD output_buffer_size = sizeof(toc);
	LPDWORD bytes_returned   = NULL;
	LPOVERLAPPED overlapped  = NULL;

	if (!DeviceIoControl(device,
	                     control_code,
	                     input_buffer,
	                     input_buffer_size,
	                     output_buffer,
	                     output_buffer_size,
	                     bytes_returned,
	                     overlapped)) {
		CloseHandle(device);
		return false;
	}

	if (IsOpen()) {
		CloseHandle(cdrom_handle);
	}
	cdrom_handle = device;
	return true;
}

bool CDROM_Interface_Win32::SetDevice(const char* path)
{
	/* Must be a root path. Ex: D:\ */
	if (strlen(path) > 3) {
		return false;
	}
	const char drive_letter = get_drive_letter_from_path(path);
	if (!drive_letter) {
		return false;
	}
	if (!Open(drive_letter)) {
		return false;
	}
	InitAudio();
	return true;
}

bool CDROM_Interface_Win32::GetUPC(unsigned char& attr, std::string& upc)
{
	if (!IsOpen()) {
		return false;
	}

	CDROM_SUB_Q_DATA_FORMAT format = {};
	format.Format                  = IOCTL_CDROM_MEDIA_CATALOG;
	SUB_Q_CHANNEL_DATA data        = {};

	DWORD control_code       = IOCTL_CDROM_READ_Q_CHANNEL;
	LPVOID input_buffer      = &format;
	DWORD input_buffer_size  = sizeof(format);
	LPVOID output_buffer     = &data;
	DWORD output_buffer_size = sizeof(data);
	LPDWORD bytes_returned   = NULL;
	LPOVERLAPPED overlapped  = NULL;

	if (!DeviceIoControl(cdrom_handle,
	                     control_code,
	                     input_buffer,
	                     input_buffer_size,
	                     output_buffer,
	                     output_buffer_size,
	                     bytes_returned,
	                     overlapped)) {
		return false;
	}
	if (!data.MediaCatalog.Mcval) {
		return false;
	}
	attr = 0;
	upc  = safe_tostring((const char*)data.MediaCatalog.MediaCatalog,
                            sizeof(data.MediaCatalog.MediaCatalog));
	return true;
}

bool CDROM_Interface_Win32::GetAudioTracks(uint8_t& stTrack, uint8_t& end, TMSF& leadOut)
{
	if (!IsOpen()) {
		return false;
	}

	CDROM_TOC toc = {};

	DWORD control_code       = IOCTL_CDROM_READ_TOC;
	LPVOID input_buffer      = NULL;
	DWORD input_buffer_size  = 0;
	LPVOID output_buffer     = &toc;
	DWORD output_buffer_size = sizeof(toc);
	LPDWORD bytes_returned   = NULL;
	LPOVERLAPPED overlapped  = NULL;

	if (!DeviceIoControl(cdrom_handle,
	                     control_code,
	                     input_buffer,
	                     input_buffer_size,
	                     output_buffer,
	                     output_buffer_size,
	                     bytes_returned,
	                     overlapped)) {
		return false;
	}
	if (toc.LastTrack >= MAXIMUM_NUMBER_TRACKS) {
		return false;
	}
	stTrack     = toc.FirstTrack;
	end         = toc.LastTrack;
	leadOut.min = toc.TrackData[toc.LastTrack].Address[1];
	leadOut.sec = toc.TrackData[toc.LastTrack].Address[2];
	leadOut.fr  = toc.TrackData[toc.LastTrack].Address[3];
	return true;
}

bool CDROM_Interface_Win32::GetAudioTrackInfo(uint8_t track, TMSF& start,
                                              unsigned char& attr)
{
	if (!IsOpen()) {
		return false;
	}
	uint8_t index = track - 1;
	if (index >= MAXIMUM_NUMBER_TRACKS) {
		return false;
	}

	CDROM_TOC toc = {};

	DWORD control_code       = IOCTL_CDROM_READ_TOC;
	LPVOID input_buffer      = NULL;
	DWORD input_buffer_size  = 0;
	LPVOID output_buffer     = &toc;
	DWORD output_buffer_size = sizeof(toc);
	LPDWORD bytes_returned   = NULL;
	LPOVERLAPPED overlapped  = NULL;

	if (!DeviceIoControl(cdrom_handle,
	                     control_code,
	                     input_buffer,
	                     input_buffer_size,
	                     output_buffer,
	                     output_buffer_size,
	                     bytes_returned,
	                     overlapped)) {
		return false;
	}
	start.min = toc.TrackData[index].Address[1];
	start.sec = toc.TrackData[index].Address[2];
	start.fr  = toc.TrackData[index].Address[3];
	attr = (toc.TrackData[index].Control << 4) | toc.TrackData[index].Adr;
	return true;
}

bool CDROM_Interface_Win32::GetAudioSub(unsigned char& attr, unsigned char& track,
                                        unsigned char& index, TMSF& relPos,
                                        TMSF& absPos)
{
	if (!IsOpen()) {
		return false;
	}

	CDROM_SUB_Q_DATA_FORMAT format = {};
	format.Format                  = IOCTL_CDROM_CURRENT_POSITION;
	SUB_Q_CHANNEL_DATA data        = {};

	DWORD control_code       = IOCTL_CDROM_READ_Q_CHANNEL;
	LPVOID input_buffer      = &format;
	DWORD input_buffer_size  = sizeof(format);
	LPVOID output_buffer     = &data;
	DWORD output_buffer_size = sizeof(data);
	LPDWORD bytes_returned   = NULL;
	LPOVERLAPPED overlapped  = NULL;

	if (!DeviceIoControl(cdrom_handle,
	                     control_code,
	                     input_buffer,
	                     input_buffer_size,
	                     output_buffer,
	                     output_buffer_size,
	                     bytes_returned,
	                     overlapped)) {
		return false;
	}

	attr  = (data.CurrentPosition.Control << 4) | data.CurrentPosition.ADR;
	track = data.CurrentPosition.TrackNumber;
	index = data.CurrentPosition.IndexNumber;
	relPos.min = data.CurrentPosition.TrackRelativeAddress[1];
	relPos.sec = data.CurrentPosition.TrackRelativeAddress[2];
	relPos.fr  = data.CurrentPosition.TrackRelativeAddress[3];
	absPos.min = data.CurrentPosition.AbsoluteAddress[1];
	absPos.sec = data.CurrentPosition.AbsoluteAddress[2];
	absPos.fr  = data.CurrentPosition.AbsoluteAddress[3];
	return true;
}

bool CDROM_Interface_Win32::GetMediaTrayStatus(bool& mediaPresent,
                                               bool& mediaChanged, bool& trayOpen)
{
	mediaPresent = true;
	mediaChanged = false;
	trayOpen     = false;
	return true;
}

// TODO: Find a test case and implement these.
// LaserLock copy protection needs this but that needs more work to implment.
// LaserLock currently does not work with CDROM_Interface_Image or
// CDROM_Interface_Ioctl either which does implement these. I could not find any
// other game that uses this.
bool CDROM_Interface_Win32::ReadSectors(PhysPt buffer, const bool raw,
                                        const uint32_t sector, const uint16_t num)
{
	return false;
}

bool CDROM_Interface_Win32::ReadSectorsHost(void* buffer, bool raw,
                                            unsigned long sector, unsigned long num)
{
	return false;
}

bool CDROM_Interface_Win32::LoadUnloadMedia(bool unload)
{
	if (!IsOpen()) {
		return false;
	}
	DWORD control_code       = unload ? IOCTL_STORAGE_EJECT_MEDIA
	                                  : IOCTL_DISK_LOAD_MEDIA;
	LPVOID input_buffer      = NULL;
	DWORD input_buffer_size  = 0;
	LPVOID output_buffer     = NULL;
	DWORD output_buffer_size = 0;
	LPDWORD bytes_returned   = NULL;
	LPOVERLAPPED overlapped  = NULL;
	return DeviceIoControl(cdrom_handle,
	                       control_code,
	                       input_buffer,
	                       input_buffer_size,
	                       output_buffer,
	                       output_buffer_size,
	                       bytes_returned,
	                       overlapped);
}

std::vector<int16_t> CDROM_Interface_Win32::ReadAudio(const uint32_t sector,
                                                      const uint32_t frames_requested)
{
	constexpr uint32_t MaximumFramesPerCall = 55;
	const uint32_t num_frames = std::min(frames_requested, MaximumFramesPerCall);

	std::vector<int16_t> audio_frames(num_frames * SAMPLES_PER_REDBOOK_FRAME);

	RAW_READ_INFO read_info      = {};
	read_info.DiskOffset.LowPart = sector * BYTES_PER_COOKED_REDBOOK_FRAME;
	read_info.SectorCount        = num_frames;
	read_info.TrackMode          = CDDA;

	DWORD control_code       = IOCTL_CDROM_RAW_READ;
	LPVOID input_buffer      = &read_info;
	DWORD input_buffer_size  = sizeof(read_info);
	LPVOID output_buffer     = audio_frames.data();
	DWORD output_buffer_size = audio_frames.size() * sizeof(int16_t);
	LPDWORD bytes_returned   = NULL;
	LPOVERLAPPED overlapped  = NULL;

	DeviceIoControl(cdrom_handle,
	                control_code,
	                input_buffer,
	                input_buffer_size,
	                output_buffer,
	                output_buffer_size,
	                bytes_returned,
	                overlapped);

	return audio_frames;
}

#endif // WIN32
