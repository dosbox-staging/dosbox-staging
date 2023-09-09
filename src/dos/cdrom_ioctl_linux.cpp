/*
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

#include <assert.h>
#include <string.h>
#include "cdrom.h"
#include "support.h"

#if defined(LINUX)
#include <fcntl.h>
#include <limits.h>
#include <linux/cdrom.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

constexpr auto MixerChannelNamePrefix = "CDAUDIO_";
// ioctl cannot be read more than 75 redbook frames at a time (one second of audio)
constexpr int InputBufferMaxRedbookFrames = 25;
constexpr size_t PcmSamplesPerRedbookFrame = BYTES_PER_RAW_REDBOOK_FRAME / REDBOOK_BPS;
constexpr size_t InputBufferMaxSamples = InputBufferMaxRedbookFrames *
                                         PcmSamplesPerRedbookFrame;

CDROM_Interface_Ioctl::~CDROM_Interface_Ioctl()
{
	if (mixer_channel) {
		MIXER_DeregisterChannel(mixer_channel);
	}
	if (IsOpen()) {
		close(cdrom_fd);
	}
}

bool CDROM_Interface_Ioctl::IsOpen() const
{
	return cdrom_fd != -1;
}

bool CDROM_Interface_Ioctl::GetUPC(unsigned char &attr, char *upc)
{
	if (!IsOpen()) {
		return false;
	}

	struct cdrom_mcn cdrom_mcn;
	int ret = ioctl(cdrom_fd, CDROM_GET_MCN, &cdrom_mcn);

	if (ret > 0) {
		attr = 0;
		safe_strncpy(upc, (char *)cdrom_mcn.medium_catalog_number, 14);
	}

	return (ret > 0);
}

bool CDROM_Interface_Ioctl::GetAudioTracks(uint8_t& stTrack, uint8_t& end, TMSF& leadOut)
{
	if (!IsOpen()) {
#ifdef DEBUG_IOCTL
		LOG_WARNING("CDROM_IOCTL: GetAudioTracks: cdrom_fd not open");
#endif
		return false;
	}

	cdrom_tochdr toc = {};
	if (ioctl(cdrom_fd, CDROMREADTOCHDR, &toc) != 0) {
#ifdef DEBUG_IOCTL
		LOG_WARNING("CDROM_IOCTL: GetAudioTracks: ioctl CDROMREADTOCHDR failed");
#endif
		return false;
	}

	cdrom_tocentry entry = {};
	entry.cdte_track     = CDROM_LEADOUT;
	entry.cdte_format    = CDROM_MSF;
	if (ioctl(cdrom_fd, CDROMREADTOCENTRY, &entry) != 0) {
#ifdef DEBUG_IOCTL
		LOG_WARNING("CDROM_IOCTL: GetAudioTracks: ioctl CDROMREADTOCENTRY failed");
#endif
		return false;
	}

	stTrack     = toc.cdth_trk0;
	end         = toc.cdth_trk1;
	leadOut.min = entry.cdte_addr.msf.minute;
	leadOut.sec = entry.cdte_addr.msf.second;
	leadOut.fr  = entry.cdte_addr.msf.frame;

#ifdef DEBUG_IOCTL
	LOG_INFO("CDROM_IOCTL: GetAudioTracks => start track is %2d, last playable track is %2d, "
	         "and lead-out MSF is %02d:%02d:%02d",
	         stTrack,
	         end,
	         leadOut.min,
	         leadOut.sec,
	         leadOut.fr);
#endif

	return true;
}

bool CDROM_Interface_Ioctl::GetAudioTrackInfo(uint8_t track, TMSF& start,
                                              unsigned char& attr)
{
	if (!IsOpen()) {
#ifdef DEBUG_IOCTL
		LOG_WARNING("CDROM_IOCTL: GetAudioTrackInfo: cdrom_fd not open");
#endif
		return false;
	}

	cdrom_tocentry entry = {};
	entry.cdte_track     = track;
	entry.cdte_format    = CDROM_MSF;
	if (ioctl(cdrom_fd, CDROMREADTOCENTRY, &entry) != 0) {
#ifdef DEBUG_IOCTL
		LOG_WARNING("CDROM_IOCTL: GetAudioTrackInfo: ioctl CDROMREADTOCENTRY failed");
#endif
		return false;
	}

	start.min = entry.cdte_addr.msf.minute;
	start.sec = entry.cdte_addr.msf.second;
	start.fr  = entry.cdte_addr.msf.frame;
	attr      = (entry.cdte_ctrl << 4) | entry.cdte_adr;

#ifdef DEBUG_IOCTL
	LOG_INFO("CDROM_IOCTL: GetAudioTrackInfo for track %u => "
	         "MSF %02d:%02d:%02d, which is sector %d",
	         track,
	         start.min,
	         start.sec,
	         start.fr,
	         msf_to_frames(start));
#endif

	return true;
}

bool CDROM_Interface_Ioctl::GetAudioSub(unsigned char& attr, unsigned char& track,
                                        unsigned char& index, TMSF& relPos,
                                        TMSF& absPos)
{
	if (!IsOpen()) {
#ifdef DEBUG_IOCTL
		LOG_WARNING("CDROM_IOCTL: GetAudioSub: cdrom_fd not open");
#endif
		return false;
	}

	cdrom_subchnl sub = {};
	sub.cdsc_format   = CDROM_MSF;
	if (ioctl(cdrom_fd, CDROMSUBCHNL, &sub) != 0) {
#ifdef DEBUG_IOCTL
		LOG_WARNING("CDROM_IOCTL: GetAudioSub: ioctl CDROMSUBCHNL failed");
#endif
		return false;
	}

	attr       = (sub.cdsc_ctrl << 4) | sub.cdsc_adr;
	track      = sub.cdsc_trk;
	index      = sub.cdsc_ind;
	relPos.min = sub.cdsc_reladdr.msf.minute;
	relPos.sec = sub.cdsc_reladdr.msf.second;
	relPos.fr  = sub.cdsc_reladdr.msf.frame;
	absPos.min = sub.cdsc_absaddr.msf.minute;
	absPos.sec = sub.cdsc_absaddr.msf.second;
	absPos.fr  = sub.cdsc_absaddr.msf.frame;

#ifdef DEBUG_IOCTL
	LOG_INFO("CDROM_IOCTL: GetAudioSub => position at %02d:%02d:%02d (on sector %u) "
	         "within track %u at %02d:%02d:%02d (at its sector %u)",
	         absPos.min,
	         absPos.sec,
	         absPos.fr,
	         msf_to_frames(absPos),
	         track,
	         relPos.min,
	         relPos.sec,
	         relPos.fr,
	         msf_to_frames(relPos));
#endif

	return true;
}

bool CDROM_Interface_Ioctl::GetAudioStatus(bool& playing, bool& pause)
{
	playing = is_playing;
	pause   = is_paused;

#ifdef DEBUG_IOCTL
	LOG_INFO("CDROM_IOCTL: GetAudioStatus => %s and %s",
	         playing ? "is playing" : "stopped",
	         pause ? "paused" : "not paused");
#endif

	return true;
}

// Called from CMscdex::GetDeviceStatus - dos_mscdex.cpp line 821
// Function doesn't check return value or initialize variables so just set some
// defaults and never fail.
bool CDROM_Interface_Ioctl::GetMediaTrayStatus(bool& mediaPresent,
                                               bool& mediaChanged, bool& trayOpen)
{
	mediaPresent = false;
	mediaChanged = false;
	trayOpen     = false;

	if (!IsOpen()) {
#ifdef DEBUG_IOCTL
		LOG_WARNING("CDROM_IOCTL: GetMediaTrayStatus: cdrom_fd not open");
#endif
		return true;
	}

	switch (ioctl(cdrom_fd, CDROM_DRIVE_STATUS, CDSL_CURRENT)) {
	case CDS_TRAY_OPEN: trayOpen = true; break;
	case CDS_DISC_OK: mediaPresent = true; break;
	}

	auto ret     = ioctl(cdrom_fd, CDROM_MEDIA_CHANGED, CDSL_CURRENT);
	mediaChanged = (ret > 0) && (ret & 1);

#ifdef DEBUG_IOCTL
	LOG_INFO("CDROM_IOCTL: GetMediaTrayStatus => media is %s, %s, and the tray is %s",
	         mediaPresent ? "present" : "not present",
	         mediaChanged ? "was changed" : "hasn't been changed",
	         trayOpen ? "open" : "closed");
#endif

	return true;
}

bool CDROM_Interface_Ioctl::ReadSectors(PhysPt buffer, const bool raw,
                                        const uint32_t sector, const uint16_t num)
{
	if (!IsOpen()) {
		return false;
	}

	const auto buflen = raw ? num * (unsigned int)CD_FRAMESIZE_RAW
	                        : num * (unsigned int)CD_FRAMESIZE;
	assert(buflen);
	std::vector<uint8_t> buf(buflen, 0);
	int ret = 0;

	if (raw) {
		struct cdrom_read cdrom_read;
		cdrom_read.cdread_lba = (int)sector;
		cdrom_read.cdread_bufaddr = reinterpret_cast<char *>(buf.data());
		cdrom_read.cdread_buflen = (int)buflen;

		ret = ioctl(cdrom_fd, CDROMREADRAW, &cdrom_read);
	} else {
		ret = lseek(cdrom_fd, (off_t)(sector * (unsigned long)CD_FRAMESIZE),
		            SEEK_SET);
		if (ret >= 0)
			ret = read(cdrom_fd, buf.data(), buflen);
		if ((Bitu)ret != buflen)
			ret = -1;
	}

	MEM_BlockWrite(buffer, buf.data(), buflen);

	return (ret > 0);
}

// Opens the given device name, replacing any currently opened device
bool CDROM_Interface_Ioctl::Open(const char* device_name)
{
	int fd = open(device_name, O_RDONLY | O_NONBLOCK);
	if (fd == -1) {
		return false;
	}
	if (IsOpen()) {
		close(cdrom_fd);
	}
	cdrom_fd = fd;
	return true;
}

bool CDROM_Interface_Ioctl::SetDevice(const char* path, const int cd_number)
{
	assert(path != nullptr);
	assert(*path);

	int num = SDL_CDNumDrives();
	if ((cd_number >= 0) && (cd_number < num)) {
		auto cd_name = SDL_CDName(cd_number);
		if (cd_name && Open(cd_name)) {
			InitAudio(cd_number);
			return true;
		}
	}

	for (auto i = 0; i < num; ++i) {
		auto cd_name = SDL_CDName(i);
		if (cd_name && strcmp(cd_name, path) == 0 && Open(cd_name)) {
			InitAudio(i);
			return true;
		}
	}

	LOG_WARNING("CDROM: SetDevice failed to find device for path '%s' and device number %d",
	            path,
	            cd_number);

	return false;
}

bool CDROM_Interface_Ioctl::ReadSectorsHost([[maybe_unused]] void *buffer,
                                            [[maybe_unused]] bool raw,
                                            [[maybe_unused]] unsigned long sector,
                                            [[maybe_unused]] unsigned long num)
{
	return false; /*TODO*/
}

void CDROM_Interface_Ioctl::InitAudio(const int device_number)
{
	if (mixer_channel) {
		return;
	}

	std::string name = std::string(MixerChannelNamePrefix) +
	                   std::to_string(device_number);

	// Input buffer is used as a C-style buffer passed to an ioctl.
	// Its size must always be 1 second of data.
	// Stored in int16_t samples.
	input_buffer.resize(InputBufferMaxSamples);

	auto callback = std::bind(&CDROM_Interface_Ioctl::CdAudioCallback,
	                          this,
	                          std::placeholders::_1);
	mixer_channel = MIXER_AddChannel(callback,
	                                 REDBOOK_PCM_FRAMES_PER_SECOND,
	                                 name.c_str(),
	                                 {ChannelFeature::Stereo,
	                                  ChannelFeature::DigitalAudio});
}

void CDROM_Interface_Ioctl::CdAudioCallback(const uint16_t requested_frames)
{
	if (input_buffer_position == 0) {
		if (sectors_remaining < 1) {
			StopAudio();
			return;
		}
		cdrom_read_audio cd = {};
		cd.addr.lba         = current_sector;
		cd.addr_format      = CDROM_LBA;
		cd.buf = reinterpret_cast<uint8_t*>(input_buffer.data());

		// Maximum this ioctl can read in a single call is 1 second (75
		// redbook frames)
		cd.nframes = std::min(sectors_remaining, InputBufferMaxRedbookFrames);
		input_buffer_samples = cd.nframes * PcmSamplesPerRedbookFrame;
		current_sector += cd.nframes;
		sectors_remaining -= cd.nframes;

		if (ioctl(cdrom_fd, CDROMREADAUDIO, &cd) != 0) {
			// Write out silence if something goes wrong
			std::fill(input_buffer.begin(), input_buffer.end(), 0);

#ifdef DEBUG_IOCTL
			LOG_WARNING("CDROM_IOCTL: CdAudioCallback: CDROMREADAUDIO ioctl failed");
#endif
		}
	}

	size_t end = input_buffer_position + (requested_frames * REDBOOK_CHANNELS);
	end = std::min(end, input_buffer_samples);

	output_buffer.clear();
	while (input_buffer_position < end) {
		const int16_t left = host_to_le16(
		        input_buffer[input_buffer_position++]);
		const int16_t right = host_to_le16(
		        input_buffer[input_buffer_position++]);
		output_buffer.emplace_back(left, right);
	}

	mixer_channel->AddSamples_sfloat(output_buffer.size(), &output_buffer[0][0]);

	if (input_buffer_position >= input_buffer_samples) {
		input_buffer_position = 0;
	}
}

bool CDROM_Interface_Ioctl::PlayAudioSector(const uint32_t start, uint32_t len)
{
	if (!mixer_channel) {
		return false;
	}
	input_buffer_position = 0;
	current_sector        = start;
	sectors_remaining     = len;
	is_playing            = true;
	is_paused             = false;
	mixer_channel->Enable(true);
#ifdef DEBUG_IOCTL
	LOG_INFO("CDROM_IOCTL: PlayAudioSector: start: %u len: %u", start, len);
#endif
	return true;
}

bool CDROM_Interface_Ioctl::PauseAudio(bool resume)
{
	if (mixer_channel) {
		mixer_channel->Enable(resume);
	}
	is_paused = !resume;
#ifdef DEBUG_IOCTL
	LOG_INFO("CDROM: PauseAudio => audio is now %s",
	         resume ? "unpaused" : "paused");
#endif
	return true;
}

bool CDROM_Interface_Ioctl::StopAudio()
{
	if (mixer_channel) {
		mixer_channel->Enable(false);
	}
	is_playing            = false;
	is_paused             = false;
	current_sector        = 0;
	sectors_remaining     = 0;
	input_buffer_position = 0;
#ifdef DEBUG_IOCTL
	LOG_INFO("CDROM_IOCTL: StopAudio => stopped playback and halted the mixer");
#endif
	return true;
}

// Taken from CDROM_Interface_Image
void CDROM_Interface_Ioctl::ChannelControl(TCtrl ctrl)
{
	if (!mixer_channel) {
		return;
	}

	constexpr float MaxVolume = 255.0f;
	// Adjust the volume of our mixer channel as defined by the application
	mixer_channel->SetAppVolume(
	        {ctrl.vol[0] / MaxVolume, ctrl.vol[1] / MaxVolume});

	// Map the audio channels in our mixer channel as defined by the
	// application
	const auto left_mapped  = static_cast<LineIndex>(ctrl.out[0]);
	const auto right_mapped = static_cast<LineIndex>(ctrl.out[1]);
	mixer_channel->SetChannelMap({left_mapped, right_mapped});
#ifdef DEBUG_IOCTL
	LOG_INFO("CDROM_IOCTL: ChannelControl => volumes %d/255 and %d/255, "
	         "and left-right map %d, %d",
	         ctrl.vol[0],
	         ctrl.vol[1],
	         ctrl.out[0],
	         ctrl.out[1]);
#endif
}

bool CDROM_Interface_Ioctl::LoadUnloadMedia(bool unload)
{
	if (!IsOpen()) {
		return false;
	}
	if (unload) {
		return ioctl(cdrom_fd, CDROMEJECT) == 0;
	} else {
		return ioctl(cdrom_fd, CDROMCLOSETRAY) == 0;
	}
}

#endif
