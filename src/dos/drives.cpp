// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/drives.h"

#include <string_view>

#include "audio/disk_noise.h"
#include "cpu/callback.h"
#include "cpu/cpu.h"
#include "dos.h"
#include "hardware/ide.h"
#include "hardware/pic.h"
#include "ints/bios_disk.h"
#include "utils/string_utils.h"

extern char sfn[DOS_NAMELENGTH_ASCII];

struct DiskSettings {
	DiskSpeed hdd_disk_speed   = DiskSpeed::Maximum;
	DiskSpeed fdd_disk_speed   = DiskSpeed::Maximum;
	DiskSpeed cdrom_disk_speed = DiskSpeed::Maximum;
};

static struct DiskSettings disk_settings = {};

static std::optional<std::function<void()>> io_callback_floppy   = {};
static std::optional<std::function<void()>> io_callback_harddisk = {};
static std::optional<std::function<void()>> io_callback_cdrom    = {};

// TODO Right now label formatting seems to be a bit of mess, with various
// places in code setting/expecting different format, so simple GetLabel() on
// a drive object might not yield an expected result. Not sure how to sort it
// out, but it will require some attention to detail.
// Also: this function is too strict - it removes all punctuation when *some*
// punctuation is acceptable in drive labels (e.g. '_' or '-').
//
std::string To_Label(const char* name) {
	// Reformat the name per the DOS label specification:
	// - Upper-case, up to 11 ASCII characters
	// - Internal spaces allowed but no: tabs ? / \ | . , ; : + = [ ] < > " '
	std::string label(name);
	trim(label); // strip front-and-back white-space
	strip_punctuation(label); // strip all punctuation
	label.resize(11); // collapse remainder to (at-most) 11 chars
	upcase(label);
	return label;
}

void Set_Label(const char* const input, char* const output, bool cdrom)
{
	Bitu togo     = 8;
	Bitu vnamePos = 0;
	Bitu labelPos = 0;
	bool point    = false;

	//spacepadding the filenamepart to include spaces after the terminating zero is more closely to the specs. (not doing this now)
	// HELLO\0' '' '

	while (togo > 0) {
		if (input[vnamePos]==0) break;
		if (!point && (input[vnamePos]=='.')) {	togo=4; point=true; }

		//another mscdex quirk. Label is not always uppercase. (Daggerfall)
		output[labelPos] = (cdrom?input[vnamePos]:toupper(input[vnamePos]));

		labelPos++; vnamePos++;
		togo--;
		if ((togo==0) && !point) {
			if (input[vnamePos]=='.') vnamePos++;
			output[labelPos]='.'; labelPos++; point=true; togo=3;
		}
	};
	output[labelPos]=0;

	//Remove trailing dot. except when on cdrom and filename is exactly 8 (9 including the dot) letters. MSCDEX feature/bug (fifa96 cdrom detection)
	if((labelPos > 0) && (output[labelPos-1] == '.') && !(cdrom && labelPos ==9))
		output[labelPos-1] = 0;
}

constexpr bool is_special_character(const char c)
{
    constexpr auto special_characters = std::string_view("\"+=,;:<>[]|?*");
    return special_characters.find(c) != std::string_view::npos;
}

/* Generate 8.3 names from LFNs, with tilde usage (from ~1 to ~9999). */
std::string generate_8x3(const char *lfn, const unsigned int num, const bool start)
{
	unsigned int tilde_limit = 1000000;
	if (num >= tilde_limit)
		return "";
	static std::string result = "";
	std::string input = lfn;
	while (input.size() && (input[0] == '.' || input[0] == ' '))
		input.erase(input.begin());
	while (input.size() && (input.back() == '.' || input.back() == ' '))
		input.pop_back();
	size_t len = 0;
	auto found = input.rfind('.');
	unsigned int tilde_pos = 6 - (unsigned int)floor(log10(num));
	if (num == 1 || start) {
		result.clear();
		len = found != std::string::npos ? found : input.size();
		for (size_t i = 0; i < len; i++) {
			if (input[i] != ' ') {
				result += is_special_character(input[i])
				                  ? "_"
				                  : std::string(1, toupper(input[i]));
				if (result.size() >= tilde_pos)
					break;
			}
		}
	}
	result.erase(tilde_pos);
	result += '~' + std::to_string(num);
	if (found != std::string::npos) {
		input.erase(0, found + 1);
		size_t len_ext = 0;
		len = input.size();
		for (size_t i = 0; i < len; i++) {
			if (input[i] != ' ') {
				if (!len_ext)
					result += ".";
				result += is_special_character(input[i])
				                  ? "_"
				                  : std::string(1, toupper(input[i]));
				if (++len_ext >= 3)
					break;
			}
		}
	}
	return result;
}

bool filename_not_8x3(const char *n)
{
	unsigned int i;

	i = 0;
	while (*n != 0) {
		if (*n == '.')
			break;
		if ((*n & 0xFF) <= 32 || *n == 127 || is_special_character(*n))
			return true;
		i++;
		n++;
	}
	if (i > 8)
		return true;
	if (*n == 0)
		return false; /* made it past 8 or less normal chars and end of
		                 string: normal */

	/* skip dot */
	assert(*n == '.');
	n++;

	i = 0;
	while (*n != 0) {
		if (*n == '.')
			return true; /* another '.' means LFN */
		if ((*n & 0xFF) <= 32 || *n == 127 || is_special_character(*n))
			return true;
		i++;
		n++;
	}
	if (i > 3)
		return true;

	return false; /* it is 8.3 case */
}

/* Assuming an LFN call, if the name is not strict 8.3 uppercase, return true.
 * If the name is strict 8.3 uppercase like "FILENAME.TXT" there is no point
 * making an LFN because it is a waste of space */
bool filename_not_strict_8x3(const char *n)
{
	if (filename_not_8x3(n))
		return true;
	const auto len = strlen(n);
	for (unsigned int i = 0; i < len; i++)
		if (n[i] >= 'a' && n[i] <= 'z')
			return true;
	return false; /* it is strict 8.3 upper case */
}

DOS_Drive::DOS_Drive()
	: dirCache()
{
	curdir[0] = '\0';
	info[0] = '\0';
}

void DOS_Drive::SetDir(const char *path)
{
	safe_strcpy(curdir, path);
}

// Wrapper class that allows hooking into file I/O operations without modifying
// the various underlying immplementations
class MonitoredFile : public DOS_File {
public:
	MonitoredFile(std::unique_ptr<DOS_File> _real_file, const DiskType _disk_type)
	        : real_file(std::move(_real_file)),
	          disk_type(_disk_type)
	{
		// Mirror properties from the real file so DOS sees correct info
		flags = real_file->flags;
		time  = real_file->time;
		date  = real_file->date;
		attr  = real_file->attr;
		SetName(real_file->GetName());
	}

	bool Read(uint8_t* data, uint16_t* size) override
	{
		DiskNoises* disk_noises = DiskNoises::GetInstance();
		if (disk_noises) {
			disk_noises->SetLastIoPath(this->GetName(),
			                           DiskNoiseIoType::Read,
			                           disk_type);
		}
		DriveManager::PerformDiskIoDelay(*size, disk_type);
		return real_file->Read(data, size);
	}

	bool Write(const uint8_t* data, uint16_t* size) override
	{
		DiskNoises* disk_noises = DiskNoises::GetInstance();
		if (disk_noises) {
			disk_noises->SetLastIoPath(this->GetName(),
			                           DiskNoiseIoType::Write,
			                           disk_type);
		}
		DriveManager::PerformDiskIoDelay(*size, disk_type);
		return real_file->Write(data, size);
	}

	void Close() override
	{
		DriveManager::PerformDiskIoDelay(DriveManager::EstimatedFileCloseIoOverheadInBytes,
		                                 disk_type);
		real_file->Close();
	}

	bool Seek(uint32_t* pos, const uint32_t type) override
	{
		DriveManager::PerformDiskIoDelay(DriveManager::EstimatedSeekIoOverheadInBytes,
		                                 disk_type);
		return real_file->Seek(pos, type);
	}
	uint16_t GetInformation() override
	{
		return real_file->GetInformation();
	}
	bool IsOnReadOnlyMedium() const override
	{
		return real_file->IsOnReadOnlyMedium();
	}

private:
	std::unique_ptr<DOS_File> real_file = {};
	DiskType disk_type                  = {};
};

// Wrapper class that allows hooking into disk I/O operations similar to
// MonitoredFile
class MonitoredDrive : public DOS_Drive {
public:
	MonitoredDrive(const std::shared_ptr<DOS_Drive> _real_drive,
	               const DiskType _disk_type)
	        : real_drive(_real_drive),
	          disk_type(_disk_type)
	{
		// Copy the identity of the underlying drive so that
		// GetInfoString() and ListMounts() display the correct info.
		this->type = real_drive->GetType();
		safe_strcpy(this->info, real_drive->GetInfo());
		safe_strcpy(this->curdir, real_drive->curdir);
	}

	std::string MapDosToHostFilename(const char* dosname) override
	{
		return real_drive->MapDosToHostFilename(dosname);
	}

	FILE* GetHostFilePtr(const char* name, const char* mode) override
	{
		return real_drive->GetHostFilePtr(name, mode);
	}

	const char* GetBasedir() const override
	{
		return real_drive->GetBasedir();
	}

	std::unique_ptr<DOS_File> FileOpen(const char* name, const uint8_t flags) override
	{
		auto file = real_drive->FileOpen(name, flags);
		if (file) {
			DriveManager::PerformDiskIoDelay(DriveManager::EstimatedFileOpenIoOverheadInBytes,
			                                 disk_type);
			return std::make_unique<MonitoredFile>(std::move(file),
			                                       disk_type);
		}
		return nullptr;
	}

	std::unique_ptr<DOS_File> FileCreate(const char* name,
	                                     FatAttributeFlags attributes) override
	{
		auto file = real_drive->FileCreate(name, attributes);
		if (file) {
			DriveManager::PerformDiskIoDelay(DriveManager::EstimatedFileCreationIoOverheadInBytes,
			                                 disk_type);
			return std::make_unique<MonitoredFile>(std::move(file),
			                                       disk_type);
		}
		return nullptr;
	}

	bool FileUnlink(const char* name) override
	{
		return real_drive->FileUnlink(name);
	}
	bool RemoveDir(const char* dir) override
	{
		return real_drive->RemoveDir(dir);
	}
	bool MakeDir(const char* dir) override
	{
		return real_drive->MakeDir(dir);
	}
	bool TestDir(const char* dir) override
	{
		return real_drive->TestDir(dir);
	}
	bool FindFirst(const char* _dir, DOS_DTA& dta, bool fcb) override
	{
		return real_drive->FindFirst(_dir, dta, fcb);
	}
	bool FindNext(DOS_DTA& dta) override
	{
		return real_drive->FindNext(dta);
	}
	bool GetFileAttr(const char* name, FatAttributeFlags* attr) override
	{
		return real_drive->GetFileAttr(name, attr);
	}
	bool SetFileAttr(const char* name, FatAttributeFlags attr) override
	{
		return real_drive->SetFileAttr(name, attr);
	}
	bool Rename(const char* old, const char* newname) override
	{
		return real_drive->Rename(old, newname);
	}
	bool AllocationInfo(uint16_t* bs, uint8_t* sc, uint16_t* tc, uint16_t* fc) override
	{
		return real_drive->AllocationInfo(bs, sc, tc, fc);
	}
	bool FileExists(const char* name) override
	{
		return real_drive->FileExists(name);
	}
	uint8_t GetMediaByte() override
	{
		return real_drive->GetMediaByte();
	}
	bool IsRemote() override
	{
		return real_drive->IsRemote();
	}
	bool IsRemovable() override
	{
		return real_drive->IsRemovable();
	}
	Bits UnMount() override
	{
		return real_drive->UnMount();
	}
	void EmptyCache() override
	{
		real_drive->EmptyCache();
	}
	void Activate() override
	{
		real_drive->Activate();
	}
	const char* GetLabel() override
	{
		return real_drive->GetLabel();
	}
	bool IsReadOnly() const override
	{
		return real_drive->IsReadOnly();
	}

private:
	std::shared_ptr<DOS_Drive> real_drive;
	DiskType disk_type;
};

// static members variables
DriveManager::drive_infos_t DriveManager::drive_infos = {};

void DriveManager::RegisterFilesystemImage(const int drive,
                                           std::shared_ptr<DOS_Drive> image)
{
	auto& disks = drive_infos.at(drive).disks;
	disks.clear();
	disks.push_back(image);
}

void DriveManager::AppendFilesystemImages(const int drive,
                                          const filesystem_images_t& images)
{
	auto& disks = drive_infos.at(drive).disks;
	disks.insert(std::end(disks), std::begin(images), std::end(images));
}

void DriveManager::InitializeDrive(int drive) {
	auto& drive_info = drive_infos.at(drive);
	if (!drive_info.disks.empty()) {
		drive_info.current_disk = 0;

		const auto real_disk = drive_info.disks[drive_info.current_disk];

		if (real_disk) {
			// Determine disk type based on the Drive Number
			// (0/1=Floppy, 2+=HDD)
			const DiskType type = GetDiskTypeFromDriveNumber(drive);

			// Wrap the real disk in our monitor
			auto monitored_disk = std::make_shared<MonitoredDrive>(real_disk,
			                                                       type);

			// Store the wrapper in the global array
			Drives.at(drive) = monitored_disk;

			// Activate the real disk (via the wrapper's forwarding)
			if (drive_info.disks.size() > 1) {
				monitored_disk->Activate();
			}
		} else {
			Drives.at(drive) = nullptr;
		}
	}
}

void DriveManager::CycleDisks(int requested_drive, bool notify)
{
	const auto drive = check_cast<int8_t>(requested_drive);

	// short-hand reference
	auto& drive_info = drive_infos.at(drive);

	auto num_disks = check_cast<int>(drive_info.disks.size());
	if (num_disks > 1) {
		// cycle disk
		int current_disk = drive_info.current_disk;

		// dettach CDROM from controller, if attached
		const auto is_cdrom = Drives.at(drive) &&
		                      (Drives.at(drive)->GetType() ==
		                       DosDriveType::Iso);
		int8_t index = -1;
		bool slave = false;
		if (is_cdrom) {
			IDE_CDROM_Detach_Ret(index, slave, drive);
		}

		const auto old_disk     = drive_info.disks[current_disk];
		current_disk            = (current_disk + 1) % num_disks;
		const auto new_disk     = drive_info.disks[current_disk];
		drive_info.current_disk = current_disk;
		if (drive < MAX_DISK_IMAGES && imageDiskList.at(drive)) {
			if (new_disk && new_disk->GetType() == DosDriveType::Fat) {
				const auto fat_drive = std::dynamic_pointer_cast<fatDrive>(
				        new_disk);
				imageDiskList[drive] = fat_drive->loadedDisk;
			}
			if ((drive == 2 || drive == 3) && imageDiskList[drive] && imageDiskList[drive]->hardDrive) {
				updateDPT();
			}
		}

		// copy working directory, acquire system resources and finally switch to next drive
		if (new_disk && old_disk) {
			strcpy(new_disk->curdir, old_disk->curdir);
			new_disk->Activate();
		}
		Drives.at(drive) = new_disk;

		// Re-attach the new drive to the controller
		if (is_cdrom && index > -1) {
			IDE_CDROM_Attach(index, slave, drive);
		}

		if (notify)
			LOG_MSG("Drive %c: disk %d of %d now active",
			        'A' + drive,
			        current_disk + 1,
			        num_disks);
	}
}

void DriveManager::CycleAllDisks(void) {
	for (int idrive=0; idrive<DOS_DRIVES; idrive++) CycleDisks(idrive, true);
}

int DriveManager::UnmountDrive(int drive) {
	int result = 0;

	// dettach CDROM from controller, if attached
	const auto is_cdrom = Drives.at(drive) &&
	                      (Drives[drive]->GetType() == DosDriveType::Iso);
	if (is_cdrom) {
		IDE_CDROM_Detach(drive);
	}

	// short-hand reference
	auto& drive_info = drive_infos.at(drive);

	// unmanaged drive
	if (drive_info.disks.size() == 0) {
		result = Drives.at(drive)->UnMount();
	} else {
		// managed drive
		int current_disk = drive_info.current_disk;
		result           = drive_info.disks[current_disk]->UnMount();
	}
	// only clear on success
	if (result == 0) {
		drive_infos.at(drive) = {};
		Drives.at(drive)      = nullptr;
	}
	return result;
}

char *DriveManager::GetDrivePosition(int drive)
{
	static char swap_position[10];
	const auto& drive_info = drive_infos.at(drive);
	safe_sprintf(swap_position,
	             "%d / %d",
	             drive_info.current_disk + 1,
	             (int)drive_info.disks.size());
	return swap_position;
}

void DriveManager::SetDiskSpeed(const DiskSpeed disk_speed, const DiskType disk_type)
{
	switch (disk_type) {
	case DiskType::Floppy: disk_settings.fdd_disk_speed = disk_speed; break;
	case DiskType::HardDisk:
		disk_settings.hdd_disk_speed = disk_speed;
		break;
	case DiskType::CdRom:
		disk_settings.cdrom_disk_speed = disk_speed;
		break;
	default: assertm(false, "Invalid DiskType enum value");
	}
}

void DriveManager::RegisterIoCallback(std::function<void()> callback,
                                      const DiskType disk_type)
{
	switch (disk_type) {
	case DiskType::Floppy: io_callback_floppy = std::move(callback); break;
	case DiskType::HardDisk:
		io_callback_harddisk = std::move(callback);
		break;
	case DiskType::CdRom: io_callback_cdrom = std::move(callback); break;
	default: assertm(false, "Invalid DiskType enum value");
	}
}

// TODO: Unregister should work on particular callbacks instead of all of a
// given disk type
void DriveManager::UnregisterIoCallback(const DiskType disk_type)
{
	switch (disk_type) {
	case DiskType::Floppy: io_callback_floppy.reset(); break;
	case DiskType::HardDisk: io_callback_harddisk.reset(); break;
	case DiskType::CdRom: io_callback_cdrom.reset(); break;
	default: assertm(false, "Invalid DiskType enum value");
	}
}

// Call any registered callbacks in the supplied callback vector
void DriveManager::ExecuteRegisteredCallbacks(const DiskType disk_type)
{
	switch (disk_type) {
	case DiskType::Floppy:
		if (io_callback_floppy) {
			(*io_callback_floppy)();
		}
		break;
	case DiskType::HardDisk:
		if (io_callback_harddisk) {
			(*io_callback_harddisk)();
		}
		break;
	case DiskType::CdRom:
		if (io_callback_cdrom) {
			(*io_callback_cdrom)();
		}
		break;
	default: assertm(false, "Invalid DiskType enum value");
	}
}

DiskType DriveManager::GetDiskTypeFromDriveNumber(const int drive_number)
{
	// Drive A (0) and B (1) are floppy drives
	if (drive_number <= 1) {
		return DiskType::Floppy;
	}
	return DiskType::HardDisk;
}

bool is_valid_file_handle(const uint8_t real_handle)
{
	return (real_handle != 0xff && Files[real_handle]);
}

void DriveManager::ExecuteRegisteredCallbacksByHandle(const uint16_t reg_handle)
{
	const auto handle = RealHandle(reg_handle);

	if (is_valid_file_handle(handle)) {
		const auto drive = Files[handle]->GetDrive();
		if (drive >= DOS_DRIVES) {
			return;
		}
		ExecuteRegisteredCallbacks(GetDiskTypeFromDriveNumber(drive));
	}
}

// Add a delay as configured for the relevant disk type, and call any
// registered callbacks.
void DriveManager::PerformDiskIoDelay(const uint16_t data_transferred_bytes,
                                      const DiskType disk_type)
{
	ExecuteRegisteredCallbacks(disk_type);
	switch (disk_type) {
	case DiskType::Floppy:
		PerformFloppyIoDelay(data_transferred_bytes);
		break;
	case DiskType::HardDisk:
		PerformHardDiskIoDelay(data_transferred_bytes);
		break;
	case DiskType::CdRom:
		PerformCdRomIoDelay(data_transferred_bytes);
		break;
	default: assertm(false, "Invalid DiskType enum value");
	}
}

void DriveManager::PerformDiskIoDelayByHandle(const uint16_t data_transferred_bytes,
                                              const uint16_t reg_handle)
{
	const auto handle = RealHandle(reg_handle);
	if (handle != 0xff && Files[handle]) {
		const auto drive = Files[handle]->GetDrive();
		if (drive >= DOS_DRIVES) {
			return;
		}
		PerformDiskIoDelay(data_transferred_bytes,
		                   GetDiskTypeFromDriveNumber(drive));
	}
}

void DriveManager::PerformHardDiskIoDelay(const uint16_t data_transferred_bytes)
{
	constexpr auto HardDiskSpeedFastKbPerSec   = 15000;
	constexpr auto HardDiskSpeedMediumKbPerSec = 2500;
	constexpr auto HardDiskSpeedSlowKbPerSec   = 600;
	double scalar;

	switch (disk_settings.hdd_disk_speed) {
	case DiskSpeed::Maximum:
		// No delay
		return;
	case DiskSpeed::Fast:
		scalar = static_cast<double>(data_transferred_bytes) /
		         (HardDiskSpeedFastKbPerSec * BytesPerKilobyte);
		break;
	case DiskSpeed::Medium:
		scalar = static_cast<double>(data_transferred_bytes) /
		         (HardDiskSpeedMediumKbPerSec * BytesPerKilobyte);
		break;
	case DiskSpeed::Slow:
		scalar = static_cast<double>(data_transferred_bytes) /
		         (HardDiskSpeedSlowKbPerSec * BytesPerKilobyte);
		break;
	default: assertm(false, "Invalid DiskSpeed enum value");
	}

	double end_time_ms = PIC_FullIndex() + (scalar * MicrosInMillisecond);

	// MS-DOS will most likely enable interrupts in the course of
	// performing disk I/O
	CPU_STI();

	do {
		ExecuteRegisteredCallbacks(DiskType::HardDisk);
		if (CALLBACK_Idle()) {
			break;
		}
	} while (PIC_FullIndex() < end_time_ms);
}

void DriveManager::PerformFloppyIoDelay(const uint16_t data_transferred_bytes)
{
	constexpr auto FloppyDiskSpeedFastKbPerSec   = 120;
	constexpr auto FloppyDiskSpeedMediumKbPerSec = 60;
	constexpr auto FloppyDiskSpeedSlowKbPerSec   = 30;
	double scalar;

	switch (disk_settings.fdd_disk_speed) {
	case DiskSpeed::Maximum:
		// No delay
		return;
	case DiskSpeed::Fast:
		scalar = static_cast<double>(data_transferred_bytes) /
		         (FloppyDiskSpeedFastKbPerSec * BytesPerKilobyte);
		break;
	case DiskSpeed::Medium:
		scalar = static_cast<double>(data_transferred_bytes) /
		         (FloppyDiskSpeedMediumKbPerSec * BytesPerKilobyte);
		break;
	case DiskSpeed::Slow:
		scalar = static_cast<double>(data_transferred_bytes) /
		         (FloppyDiskSpeedSlowKbPerSec * BytesPerKilobyte);
		break;
	default: assertm(false, "Invalid DiskSpeed enum value");
	}

	double end_time_ms = PIC_FullIndex() + (scalar * MicrosInMillisecond);

	// MS-DOS will most likely enable interrupts in the course of
	// performing disk I/O
	CPU_STI();

	do {
		ExecuteRegisteredCallbacks(DiskType::Floppy);
		if (CALLBACK_Idle()) {
			break;
		}
	} while (PIC_FullIndex() < end_time_ms);
}

void DriveManager::PerformCdRomIoDelay(const uint16_t data_transferred_bytes)
{
	constexpr auto CdRomSpeedFastKbPerSec   = 1200;
	constexpr auto CdRomSpeedMediumKbPerSec = 300;
	constexpr auto CdRomSpeedSlowKbPerSec   = 150;
	double scalar;

	switch (disk_settings.cdrom_disk_speed) {
	case DiskSpeed::Maximum:
		// No delay
		return;
	case DiskSpeed::Fast:
		scalar = static_cast<double>(data_transferred_bytes) /
		         (CdRomSpeedFastKbPerSec * BytesPerKilobyte);
		break;
	case DiskSpeed::Medium:
		scalar = static_cast<double>(data_transferred_bytes) /
		         (CdRomSpeedMediumKbPerSec * BytesPerKilobyte);
		break;
	case DiskSpeed::Slow:
		scalar = static_cast<double>(data_transferred_bytes) /
		         (CdRomSpeedSlowKbPerSec * BytesPerKilobyte);
		break;
	default: assertm(false, "Invalid DiskSpeed enum value");
	}

	double end_time_ms = PIC_FullIndex() + (scalar * MicrosInMillisecond);

	// MS-DOS will most likely enable interrupts in the course of
	// performing disk I/O
	CPU_STI();

	do {
		ExecuteRegisteredCallbacks(DiskType::CdRom);
		if (CALLBACK_Idle()) {
			break;
		}
	} while (PIC_FullIndex() < end_time_ms);
}

void DriveManager::Init()
{
	// setup drive_infos structure
	for(int i = 0; i < DOS_DRIVES; i++) {
		drive_infos.at(i).current_disk = 0;
	}

	// MAPPER_AddHandler(&CycleDisk, SDL_SCANCODE_F3, MMOD1,
	//                   "cycledisk", "Cycle Disk");
	// MAPPER_AddHandler(&CycleDrive, SDL_SCANCODE_F3, MMOD2,
	//                   "cycledrive", "Cycle Drv");
}

void DRIVES_Init()
{
	DriveManager::Init();
}
