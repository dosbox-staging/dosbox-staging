// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cassert>
#include <cstring>

#include "cdrom.h"
#include "util/string_utils.h"
#include "misc/support.h"

#if defined(LINUX)
#include <fcntl.h>
#include <limits.h>
#include <linux/cdrom.h>
#include <mntent.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

CDROM_Interface_Ioctl::~CDROM_Interface_Ioctl()
{
	if (IsOpen()) {
		close(cdrom_fd);
	}
	cdrom_fd = -1;
}

bool CDROM_Interface_Ioctl::IsOpen() const
{
	return cdrom_fd != -1;
}

bool CDROM_Interface_Ioctl::GetUPC(unsigned char &attr, std::string& upc)
{
	if (!IsOpen()) {
		return false;
	}

	struct cdrom_mcn cdrom_mcn = {};

	if (ioctl(cdrom_fd, CDROM_GET_MCN, &cdrom_mcn) == 0) {
		attr = 0;
		upc = safe_tostring((const char *)cdrom_mcn.medium_catalog_number, sizeof(cdrom_mcn.medium_catalog_number));
		return true;
	}

	return false;
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

bool CDROM_Interface_Ioctl::ReadSector(uint8_t* /*buffer*/, const bool /*raw*/,
                                       const uint32_t /*sector*/)
{
	return false; /*TODO*/
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

	// Test to make sure this device is a CDROM drive
	cdrom_tochdr toc = {};
	if (ioctl(fd, CDROMREADTOCHDR, &toc) != 0) {
		close(fd);
		return false;
	}

	if (IsOpen()) {
		close(cdrom_fd);
	}
	cdrom_fd = fd;
	return true;
}

bool CDROM_Interface_Ioctl::SetDevice(const char* path)
{
	assert(path != nullptr);
	assert(*path);

	// Search mounts file to get the device name (ex. /dev/sr0) from the mounted path name (ex. /mnt/cdrom)
	FILE *mounts = setmntent("/proc/mounts", "r");
	if (!mounts) {
		return false;
	}

	std::error_code err = {};
	const auto cannonical_path = std_fs::canonical(path, err);
	if (err) {
		return false;
	}

	while (mntent *entry = getmntent(mounts)) {
		// Don't try to open names that aren't a full path. Ex: "tmpfs" "sysfs"
		if (entry->mnt_fsname[0] == '/' && entry->mnt_dir == cannonical_path) {
			if (Open(entry->mnt_fsname)) {
				InitAudio();
				endmntent(mounts);
				return true;
			}
		}
	}

	endmntent(mounts);
	return false;
}

bool CDROM_Interface_Ioctl::ReadSectorsHost([[maybe_unused]] void *buffer,
                                            [[maybe_unused]] bool raw,
                                            [[maybe_unused]] unsigned long sector,
                                            [[maybe_unused]] unsigned long num)
{
	return false; /*TODO*/
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

std::vector<int16_t> CDROM_Interface_Ioctl::ReadAudio(const uint32_t sector, const uint32_t frames_requested)
{
	// Hard limit of 75 frames on the Linux kernel
	constexpr uint32_t MaximumFramesPerCall = 75;

	const uint32_t num_frames = std::min(frames_requested, MaximumFramesPerCall);

	std::vector<int16_t> audio_samples(num_frames * SAMPLES_PER_REDBOOK_FRAME);

	cdrom_read_audio cd = {};
	cd.addr.lba         = sector;
	cd.addr_format      = CDROM_LBA;
	cd.nframes          = num_frames;
	cd.buf              = reinterpret_cast<__u8 *>(audio_samples.data());

	if (ioctl(cdrom_fd, CDROMREADAUDIO, &cd) != 0) {
#ifdef DEBUG_IOCTL
		LOG_WARNING("CDROM_IOCTL: ReadAudio: CDROMREADAUDIO ioctl failed");
#endif
	}

	return audio_samples;
}

bool CDROM_Interface_Ioctl::HasDataTrack() const
{
	return true; /*TODO*/
}

#endif // Linux
