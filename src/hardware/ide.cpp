// SPDX-FileCopyrightText:  2021-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2012-2021 Jonathan Campbell
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ide.h"

#include <algorithm>
#include <cmath>
#include <cassert>

#include "audio/mixer.h"
#include "config/config.h"
#include "config/setup.h"
#include "cpu/callback.h"
#include "cpu/cpu.h"
#include "dos/cdrom.h"
#include "hardware/pic.h"
#include "hardware/port.h"
#include "hardware/timer.h"
#include "ints/bios_disk.h"
#include "utils/string_utils.h"

extern int bootdrive;
extern bool bootguest, bootvm, use_quick_reboot;

static void ide_altio_w(io_port_t port, io_val_t val, io_width_t width);
static uint32_t ide_altio_r(io_port_t port, io_width_t width);
static void ide_baseio_w(io_port_t port, io_val_t val, io_width_t width);
static uint32_t ide_baseio_r(io_port_t port, io_width_t width);
bool GetMSCDEXDrive(uint8_t drive_letter, CDROM_Interface **_cdrom);

enum IDEDeviceType { IDE_TYPE_NONE, IDE_TYPE_HDD = 1, IDE_TYPE_CDROM };

enum IDEDeviceState {
	IDE_DEV_READY = 0,
	IDE_DEV_SELECT_WAIT,
	IDE_DEV_CONFUSED,
	IDE_DEV_BUSY,
	IDE_DEV_DATA_READ,
	IDE_DEV_DATA_WRITE,
	IDE_DEV_ATAPI_PACKET_COMMAND,
	IDE_DEV_ATAPI_BUSY
};

enum {
	IDE_STATUS_BUSY = 0x80,
	IDE_STATUS_DRIVE_READY = 0x40,
	IDE_STATUS_DRIVE_SEEK_COMPLETE = 0x10,
	IDE_STATUS_DRQ = 0x08,
	IDE_STATUS_ERROR = 0x01
};

class IDEController;

#if 0 // unused
static inline bool drivehead_is_lba48(uint8_t val) {
    return (val&0xE0) == 0x40;
}
#endif

static inline bool drivehead_is_lba(uint8_t val)
{
	return (val & 0xE0) == 0xE0;
}

#if 0 // unused
static inline bool drivehead_is_chs(uint8_t val) {
    return (val&0xE0) == 0xA0;
}
#endif

static const char *get_controller_name(int index)
{
	switch (index) {
	case 0: return "primary";
	case 1: return "secondary";
	case 2: return "tertiary";
	case 3: return "quaternary";
	default: return "unknown-controller_name";
	}
}

static const char *get_cable_slot_name(const bool is_second_slot)
{
	return is_second_slot ? "second" : "first";
}

class IDEDevice {
public:
	IDEController *controller = nullptr;
	uint16_t feature = 0;
	uint16_t count = 0;
	uint16_t lba[3] = {}; /* feature = BASE+1  count = BASE+2   lba[3] = BASE+3,+4,+5 */
	uint8_t command = 0;
	uint8_t drivehead = 0;
	uint8_t status = 0x00; /* command/status = BASE+7  drivehead = BASE+6 */
	enum IDEDeviceType type = IDE_TYPE_NONE;
	bool faked_command = false; /* if set, DOSBox is sending commands to itself */
	bool allow_writing = true;
	bool motor_on = true;
	bool asleep = false;
	IDEDeviceState state = IDE_DEV_READY;
	/* feature: 0x1F1 (Word 00h in ATA specs)
	     count: 0x1F2 (Word 01h in ATA specs)
	    lba[3]: 0x1F3 (Word 02h) 0x1F4 (Word 03h) and 0x1F5 (Word 04h)
	 drivehead: 0x1F6 (copy of last value written)
	   command: 0x1F7 (Word 05h)
	    status: 0x1F7 (value read back to IDE controller, including busy and drive ready bits as well as
	error status)

	In C/H/S modes lba[3] becomes lba[0]=sector lba[1]=cylinder-low lba[2]=cylinder-high and
	the code must read the 4-bit head number from drivehead[bits 3:0].

	"drivehead" in this struct is always maintained as a device copy of the controller's
	drivehead value. it is only updated on write, and not returned on read.

	"allow_writing" if set allows the DOS program/OS to write the registers. It is
	clear during command execution, obviously, so the state of the device is not confused
	while executing the command.

	Registers are 16-bit where applicable so future revisions of this code
	can support LBA48 commands */
public:
	/* tweakable parameters */
	double ide_select_delay = 0.5;  /* 500us. time between writing 0x1F6 and drive readiness */
	double ide_spinup_delay = 3000; /* 3 seconds. time it takes to spin the hard disk motor up to speed */
	double ide_spindown_delay = 1000; /* 1 second. time it takes for hard disk motor to spin down */
	double ide_identify_command_delay = 0.01; /* 10us */
public:
	IDEDevice(IDEController *c, const IDEDeviceType device_type) : controller(c), type(device_type) {}
	IDEDevice(const IDEDevice &other) = delete;            // prevent copying
	IDEDevice &operator=(const IDEDevice &other) = delete; // prevent assignment
	virtual ~IDEDevice() = default;

	virtual void host_reset_begin();    /* IDE controller -> upon setting bit 2 of alt (0x3F6) */
	virtual void host_reset_complete(); /* IDE controller -> upon setting bit 2 of alt (0x3F6) */
	virtual void select(uint8_t ndh, bool switched_to);
	virtual void deselect();
	virtual void abort_error();
	virtual void abort_normal();
	virtual void interface_wakeup();
	virtual void writecommand(uint8_t cmd);
	virtual uint32_t data_read(io_width_t width);          /* read from 1F0h data port from IDE device */
	virtual void data_write(uint32_t v, io_width_t width); /* write to 1F0h data port to IDE device */
	virtual bool command_interruption_ok(uint8_t cmd);
	virtual void abort_silent();
};

class IDEATADevice : public IDEDevice {
public:
	IDEATADevice(IDEController *c, uint8_t disk_index);
	IDEATADevice(const IDEATADevice &other) = delete;            // prevent copying
	IDEATADevice &operator=(const IDEATADevice &other) = delete; // prevent assignment
	~IDEATADevice() override = default;

	void writecommand(uint8_t cmd) override;

public:
	std::string id_serial = "8086";
	std::string id_firmware_rev = "8086";
	std::string id_model = "DOSBox IDE disk";
	uint8_t bios_disk_index;

	std::shared_ptr<imageDisk> getBIOSdisk();

	void update_from_biosdisk();
	/* read from 1F0h data port from IDE device */
	uint32_t data_read(io_width_t width) override;
	/* write to 1F0h data port to IDE device */
	void data_write(uint32_t v, io_width_t width) override;
	virtual void generate_identify_device();
	virtual void prepare_read(uint32_t offset, uint32_t size);
	virtual void prepare_write(uint32_t offset, uint32_t size);
	virtual void io_completion();
	virtual bool increment_current_address(uint32_t count = 1);

public:
	uint8_t sector[512 * 128] = {};
	uint32_t sector_i = 0;
	uint32_t sector_total = 0;

	uint32_t multiple_sector_max = sizeof(sector) / 512;
	uint32_t multiple_sector_count = 1;

	uint32_t heads = 0;
	uint32_t sects = 0;
	uint32_t cyls = 0;

	uint32_t headshr = 0;
	uint32_t progress_count = 0;

	uint32_t phys_heads = 0;
	uint32_t phys_sects = 0;
	uint32_t phys_cyls = 0;

	bool geo_translate = false;
};

enum {
	LOAD_NO_DISC = 0,
	LOAD_INSERT_CD,    /* user is "inserting" the CD */
	LOAD_IDLE,         /* disc is stationary, not spinning */
	LOAD_DISC_LOADING, /* disc is "spinning up" */
	LOAD_DISC_READIED, /* disc just "became ready" */
	LOAD_READY
};

class IDEATAPICDROMDevice : public IDEDevice {
public:
	IDEATAPICDROMDevice(IDEController *c, uint8_t requested_drive_index);
	IDEATAPICDROMDevice(const IDEATAPICDROMDevice &other) = delete;            // prevent copying
	IDEATAPICDROMDevice &operator=(const IDEATAPICDROMDevice &other) = delete; // prevent assignment
	~IDEATAPICDROMDevice() override = default;

	void writecommand(uint8_t cmd) override;

public:
	std::string id_serial = "123456789";
	std::string id_firmware_rev = "0.83-X";
	std::string id_model = "DOSBox-X Virtual CD-ROM";
	uint8_t drive_index = 0;

	CDROM_Interface *getMSCDEXDrive();
	void update_from_cdrom();
	/* read from 1F0h data port from IDE device */
	uint32_t data_read(io_width_t width) override;
	/* write to 1F0h data port to IDE device */
	void data_write(uint32_t v, io_width_t width) override;
	virtual void generate_identify_device();
	virtual void generate_mmc_inquiry();
	virtual void prepare_read(uint32_t offset, uint32_t size);
	virtual void prepare_write(uint32_t offset, uint32_t size);
	virtual void set_sense(uint8_t SK, uint8_t ASC = 0, uint8_t ASCQ = 0,
	                       uint32_t len = 0);
	virtual bool common_spinup_response(bool trigger, bool wait);
	virtual void on_mode_select_io_complete();
	virtual void atapi_io_completion();
	virtual void io_completion();
	virtual void atapi_cmd_completion();
	virtual void on_atapi_busy_time();
	virtual void read_subchannel();
	virtual void play_audio_msf();
	virtual void pause_resume();
	virtual void play_audio10();
	virtual void mode_sense();
	virtual void read_toc();

public:
	/* if set, PACKET data transfer is to be read by host */
	bool atapi_to_host = false;

	/* drive takes 1 second to spin up from idle */
	double spinup_time = 1000;

	/* drive spins down automatically after 10 seconds */
	double spindown_timeout = 10000;

	/* a quick user that can switch CDs in 4 seconds */
	double cd_insertion_time = 4000;

	/* host maximum byte count during PACKET transfer */
	uint32_t host_maximum_byte_count = 0;

	/* INQUIRY strings */
	std::string id_mmc_vendor_id = "DOSBox-X";
	std::string id_mmc_product_id = "Virtual CD-ROM";
	std::string id_mmc_product_rev = "0.83-X";
	uint32_t LBA = 0;
	uint32_t TransferLength = 0;
	int loading_mode = LOAD_IDLE;
	bool has_changed = false;

public:
	uint8_t sense[256] = {};
	uint32_t sense_length = 0;
	uint8_t atapi_cmd[12] = {};
	uint8_t atapi_cmd_i = 0;
	uint8_t atapi_cmd_total = 0;
	uint8_t sector[512 * 128] = {};
	uint32_t sector_i = 0;
	uint32_t sector_total = 0;
};

class IDEController {
public:
	int IRQ = -1;
	bool int13fakeio = false; /* on certain INT 13h calls, force IDE state as if BIOS had carried them out */
	bool int13fakev86io = false; /* on certain INT 13h calls in virtual 8086 mode, trigger fake CPU I/O traps */
	bool enable_pio32 = false; /* enable 32-bit PIO (if disabled, attempts at 32-bit PIO are handled as if
	                              two 16-bit I/O) */
	bool ignore_pio32 = false; /* if 32-bit PIO enabled, but ignored, writes do nothing, reads return
	                              0xFFFFFFFF */
	bool register_pnp = false;
	uint16_t alt_io = 0;
	uint16_t base_io = 0;
	uint8_t interface_index = 0;
	IO_ReadHandleObject ReadHandler[8] = {};
	IO_ReadHandleObject ReadHandlerAlt[2] = {};
	IO_WriteHandleObject WriteHandler[8] = {};
	IO_WriteHandleObject WriteHandlerAlt[2] = {};

public:
	IDEDevice *device[2] = {nullptr, nullptr}; /* IDE devices (master, slave) */
	uint32_t select = 0;       /* selected device (0 or 1) */
	uint32_t status = 0;       /* status register */
	uint32_t drivehead = 0; /* which is selected, status register (0x1F7) but ONLY if no device exists at
	                           selection, drive/head register (0x1F6) */
	bool interrupt_enable = true; /* bit 1 of alt (0x3F6) */
	bool host_reset = false;      /* bit 2 of alt */
	bool irq_pending = false;
	/* defaults for CD-ROM emulation */
	double spinup_time = 0.0;
	double spindown_timeout = 0.0;
	double cd_insertion_time = 0.0;

public:
	IDEController(const uint8_t index,
	              const uint8_t irq,
	              const uint16_t port,
	              const uint16_t alt_port);
	IDEController(const IDEController &other) = delete; // prevent copying
	IDEController &operator=(const IDEController &other) = delete; // prevent assignment
	~IDEController();

	void install_io_ports();
	void uninstall_io_ports();
	void raise_irq();
	void lower_irq();
};

static std::array<IDEController *, MAX_IDE_CONTROLLERS> idecontroller{{
        nullptr,
        nullptr,
        nullptr,
        nullptr,
}};

static void IDE_DelayedCommand(uint32_t idx /*which IDE controller*/);
static IDEController *GetIDEController(uint32_t idx);

static void IDE_ATAPI_SpinDown(uint32_t idx /*which IDE controller*/)
{
	IDEController *ctrl = GetIDEController(idx);
	if (ctrl == nullptr)
		return;

	for (uint32_t i = 0; i < 2; i++) {
		IDEDevice *dev = ctrl->device[i];
		if (dev == nullptr)
			continue;

		if (dev->type == IDE_TYPE_HDD) {
			// no-op
		} else if (dev->type == IDE_TYPE_CDROM) {
			auto atapi = (IDEATAPICDROMDevice *)dev;

			if (atapi->loading_mode == LOAD_DISC_READIED || atapi->loading_mode == LOAD_READY) {
				atapi->loading_mode = LOAD_IDLE;
				LOG_MSG("IDE: ATAPI CD-ROM spinning down");
			}
		} else {
			LOG_WARNING("IDE: Unknown ATAPI spinup callback");
		}
	}
}

static void IDE_ATAPI_SpinUpComplete(uint32_t idx /*which IDE controller*/);

static void IDE_ATAPI_CDInsertion(uint32_t idx /*which IDE controller*/)
{
	IDEController *ctrl = GetIDEController(idx);
	if (ctrl == nullptr)
		return;

	for (uint32_t i = 0; i < 2; i++) {
		IDEDevice *dev = ctrl->device[i];
		if (dev == nullptr)
			continue;

		if (dev->type == IDE_TYPE_HDD) {
			// no-op
		} else if (dev->type == IDE_TYPE_CDROM) {
			auto atapi = (IDEATAPICDROMDevice *)dev;

			if (atapi->loading_mode == LOAD_INSERT_CD) {
				atapi->loading_mode = LOAD_DISC_LOADING;
				LOG_MSG("IDE: ATAPI CD-ROM loading inserted CD");
				PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown, idx);
				PIC_RemoveSpecificEvents(IDE_ATAPI_CDInsertion, idx);
				PIC_AddEvent(IDE_ATAPI_SpinUpComplete, atapi->spinup_time /*ms*/, idx);
			}
		} else {
			LOG_WARNING("IDE: Unknown ATAPI spinup callback");
		}
	}
}

static void IDE_ATAPI_SpinUpComplete(uint32_t idx /*which IDE controller*/)
{
	IDEController *ctrl = GetIDEController(idx);
	if (ctrl == nullptr)
		return;

	for (uint32_t i = 0; i < 2; i++) {
		IDEDevice *dev = ctrl->device[i];
		if (dev == nullptr)
			continue;

		if (dev->type == IDE_TYPE_HDD) {
			// no-op
		} else if (dev->type == IDE_TYPE_CDROM) {
			auto atapi = (IDEATAPICDROMDevice *)dev;

			if (atapi->loading_mode == LOAD_DISC_LOADING) {
				atapi->loading_mode = LOAD_DISC_READIED;
				LOG_MSG("IDE: ATAPI CD-ROM spinup complete");
				PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown, idx);
				PIC_RemoveSpecificEvents(IDE_ATAPI_CDInsertion, idx);
				PIC_AddEvent(IDE_ATAPI_SpinDown, atapi->spindown_timeout /*ms*/, idx);
			}
		} else {
			LOG_WARNING("IDE: Unknown ATAPI spinup callback");
		}
	}
}

/* returns "true" if command should proceed as normal, "false" if sense data was set and command should not
 * proceed. this function helps to enforce virtual "spin up" and "ready" delays. */
bool IDEATAPICDROMDevice::common_spinup_response(bool trigger, bool wait)
{
	if (loading_mode == LOAD_IDLE) {
		if (trigger) {
			LOG_MSG("IDE: ATAPI CD-ROM triggered to spin up from idle");
			loading_mode = LOAD_DISC_LOADING;
			PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown, controller->interface_index);
			PIC_RemoveSpecificEvents(IDE_ATAPI_CDInsertion, controller->interface_index);
			PIC_AddEvent(IDE_ATAPI_SpinUpComplete, spinup_time /*ms*/, controller->interface_index);
		}
	} else if (loading_mode == LOAD_READY) {
		if (trigger) {
			PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown, controller->interface_index);
			PIC_RemoveSpecificEvents(IDE_ATAPI_CDInsertion, controller->interface_index);
			PIC_AddEvent(IDE_ATAPI_SpinDown, spindown_timeout /*ms*/, controller->interface_index);
		}
	}

	switch (loading_mode) {
	case LOAD_NO_DISC:
	case LOAD_INSERT_CD:
		set_sense(/*SK=*/0x02, /*ASC=*/0x3A); /* Medium Not Present */
		return false;
	case LOAD_DISC_LOADING:
		if (has_changed && !wait /*if command will block until LOADING complete*/) {
			set_sense(/*SK=*/0x02, /*ASC=*/0x04, /*ASCQ=*/0x01); /* Medium is becoming available */
			return false;
		}
		break;
	case LOAD_DISC_READIED:
		loading_mode = LOAD_READY;
		if (has_changed) {
			if (trigger)
				has_changed = false;
			set_sense(/*SK=*/0x02, /*ASC=*/0x28, /*ASCQ=*/0x00); /* Medium is ready (has changed) */
			return false;
		}
		break;
	case LOAD_IDLE:
	case LOAD_READY: break;
	default: abort();
	}

	return true;
}

void IDEATAPICDROMDevice::read_subchannel()
{
	//  uint8_t Format = atapi_cmd[2] & 0xF;
	//  uint8_t Track = atapi_cmd[6];
	uint8_t paramList = atapi_cmd[3];
	uint8_t attr, track, index;
	bool SUBQ = !!(atapi_cmd[2] & 0x40);
	bool TIME = !!(atapi_cmd[1] & 2);
	uint8_t *write;
	uint8_t astat;
	bool playing, pause;
	TMSF rel, abs;

	CDROM_Interface *cdrom = getMSCDEXDrive();
	if (cdrom == nullptr) {
		LOG_WARNING("IDE: WARNING: ATAPI READ TOC unable to get CDROM drive");
		prepare_read(0, 8);
		return;
	}

	if (paramList == 0 || paramList > 3) {
		LOG_WARNING("IDE: ATAPI READ SUBCHANNEL unknown param list");
		prepare_read(0, 8);
		return;
	} else if (paramList == 2) {
		LOG_WARNING("IDE: ATAPI READ SUBCHANNEL Media Catalog Number not supported");
		prepare_read(0, 8);
		return;
	} else if (paramList == 3) {
		LOG_WARNING("IDE: ATAPI READ SUBCHANNEL ISRC not supported");
		prepare_read(0, 8);
		return;
	}

	/* get current subchannel position */
	if (!cdrom->GetAudioSub(attr, track, index, rel, abs)) {
		LOG_WARNING("IDE: ATAPI READ SUBCHANNEL unable to read current pos");
		prepare_read(0, 8);
		return;
	}

	if (!cdrom->GetAudioStatus(playing, pause))
		playing = pause = false;

	if (playing)
		astat = pause ? 0x12 : 0x11;
	else
		astat = 0x13;

	std::fill_n(sector, 8, 0);
	write = sector;
	*write++ = 0x00;
	*write++ = astat; /* AUDIO STATUS */
	*write++ = 0x00;  /* SUBCHANNEL DATA LENGTH */
	*write++ = 0x00;

	if (SUBQ) {
		*write++ = 0x01;               /* subchannel data format code */
		*write++ = (attr >> 4) | 0x10; /* ADR/CONTROL */
		*write++ = track;
		*write++ = index;
		if (TIME) {
			*write++ = 0x00;
			*write++ = abs.min;
			*write++ = abs.sec;
			*write++ = abs.fr;
			*write++ = 0x00;
			*write++ = rel.min;
			*write++ = rel.sec;
			*write++ = rel.fr;
		} else {
			uint32_t sec;

			sec = (abs.min * 60u * 75u) + (abs.sec * 75u) + abs.fr - 150u;
			*write++ = (uint8_t)(sec >> 24u);
			*write++ = (uint8_t)(sec >> 16u);
			*write++ = (uint8_t)(sec >> 8u);
			*write++ = (uint8_t)(sec >> 0u);

			sec = (rel.min * 60u * 75u) + (rel.sec * 75u) + rel.fr - 150u;
			*write++ = (uint8_t)(sec >> 24u);
			*write++ = (uint8_t)(sec >> 16u);
			*write++ = (uint8_t)(sec >> 8u);
			*write++ = (uint8_t)(sec >> 0u);
		}
	}

	{
		uint32_t x = (uint32_t)(write - sector) - 4;
		sector[2] = check_cast<uint8_t>(x >> 8);
		sector[3] = check_cast<uint8_t>(x);
	}

	prepare_read(0, std::min((uint32_t)(write - sector), host_maximum_byte_count));
#if 0
    for (size_t i=0;i < sector_total;i++) LOG_MSG("IDE: Subchannel %02x ",sector[i]);
#endif
}

void IDEATAPICDROMDevice::mode_sense()
{
	uint8_t PAGE = atapi_cmd[2] & 0x3F;
	//  uint8_t SUBPAGE = atapi_cmd[3];
	uint8_t *write;
	uint32_t x;

	write = sector;

	/* Mode Parameter List MMC-3 Table 340 */
	/* - Mode parameter header */
	/* - Page(s) */

	/* Mode Parameter Header (response for 10-byte MODE SENSE) SPC-2 Table 148 */
	*write++ = 0x00; /* MODE DATA LENGTH                     (MSB) */
	*write++ = 0x00; /*                                      (LSB) */
	*write++ = 0x00; /* MEDIUM TYPE */
	*write++ = 0x00; /* DEVICE-SPECIFIC PARAMETER */
	*write++ = 0x00; /* Reserved */
	*write++ = 0x00; /* Reserved */
	*write++ = 0x00; /* BLOCK DESCRIPTOR LENGTH              (MSB) */
	*write++ = 0x00; /*                                      (LSB) */
	/* NTS: MMC-3 Table 342 says that BLOCK DESCRIPTOR LENGTH is zero, where it would be 8 for legacy units */

	/* Mode Page Format MMC-3 Table 341 */
	*write++ = PAGE; /* PS|reserved|Page Code */
	*write++ = 0x00; /* Page Length (n - 1) ... Length in bytes of the mode parameters that follow */
	switch (PAGE) {
	case 0x01:               /* Read error recovery MMC-3 Section 6.3.4 table 344 */
		*write++ = 0x00; /* +2 Error recovery Parameter  AWRE|ARRE|TB|RC|Reserved|PER|DTE|DCR */
		*write++ = 3;    /* +3 Read Retry Count */
		*write++ = 0x00; /* +4 Reserved */
		*write++ = 0x00; /* +5 Reserved */
		*write++ = 0x00; /* +6 Reserved */
		*write++ = 0x00; /* +7 Reserved */
		*write++ = 0x00; /* +8 Write Retry Count (this is not yet CD burner) */
		*write++ = 0x00; /* +9 Reserved */
		*write++ = 0x00; /* +10 Recovery Time Limit (should be zero)         (MSB) */
		*write++ = 0x00; /* +11                                              (LSB) */
		break;
	case 0x0E:               /* CD-ROM audio control MMC-3 Section 6.3.7 table 354 */
		                 /* also MMC-1 Section 5.2.3.1 table 97 */
		*write++ = 0x04; /* +2 Reserved|IMMED=1|SOTC=0|Reserved */
		*write++ = 0x00; /* +3 Reserved */
		*write++ = 0x00; /* +4 Reserved */
		*write++ = 0x00; /* +5 Reserved */
		*write++ = 0x00; /* +6 Obsolete (75) */
		*write++ = 75;   /* +7 Obsolete (75) */
		*write++ = 0x01; /* +8 output port 0 selection (0001b = channel 0) */
		*write++ = 0xFF; /* +9 output port 0 volume (0xFF = 0dB atten.) */
		*write++ = 0x02; /* +10 output port 1 selection (0010b = channel 1) */
		*write++ = 0xFF; /* +11 output port 1 volume (0xFF = 0dB atten.) */
		*write++ = 0x00; /* +12 output port 2 selection (none) */
		*write++ = 0x00; /* +13 output port 2 volume (0x00 = mute) */
		*write++ = 0x00; /* +14 output port 3 selection (none) */
		*write++ = 0x00; /* +15 output port 3 volume (0x00 = mute) */
		break;
	case 0x2A: /* CD-ROM mechanical status MMC-3 Section 6.3.11 table 361 */
		   /*    MSB            |             |             |             |              |    |    |    LSB    */
		*write++ = 0x07; /* +2 Reserved       |Reserved     |DVD-RAM read |DVD-R read   |DVD-ROM read
		                    |   Method 2    | CD-RW read   | CD-R read */
		*write++ = 0x00; /* +3 Reserved       |Reserved     |DVD-RAM write|DVD-R write  |   Reserved
		                    |  Test Write   | CD-RW write  | CD-R write */
		*write++ = 0x71; /* +4 Buffer Underrun|Multisession |Mode 2 form 2|Mode 2 form 1|Digital Port
		                    2|Digital Port 1 |  Composite   | Audio play */
		*write++ = 0xFF; /* +5 Read code bar  |UPC          |ISRC         |C2 Pointers  |R-W deintcorr
		                    | R-W supported |CDDA accurate |CDDA support */
		*write++ = 0x2F; /* +6 Loading mechanism type                     |Reserved     |Eject
		                    |Prevent Jumper |Lock state    |Lock */
		                 /*      0 (0x00) = Caddy
		                  *      1 (0x20) = Tray
		                  *      2 (0x40) = Popup
		                  *      3 (0x60) = Reserved
		                  *      4 (0x80) = Changer with indivually changeable discs
		                  *      5 (0xA0) = Changer using a magazine mechanism
		                  *      6 (0xC0) = Reserved
		                  *      6 (0xE0) = Reserved */
		*write++ = 0x03; /* +7 Reserved       |Reserved     |R-W in leadin|Side chg cap |S/W slot sel
		                    |Changer disc pr|Sep. ch. mute |Sep. volume levels */

		x = 176 * 8; /* +8 maximum speed supported in kB: 8X  (obsolete in MMC-3) */
		*write++ = check_cast<uint8_t>(x >> 8);
		*write++ = check_cast<uint8_t>(x & 0xff);

		x = 256; /* +10 Number of volume levels supported */
		*write++ = check_cast<uint8_t>(x >> 8);
		*write++ = check_cast<uint8_t>(x & 0xff);

		x = 6 * 256; /* +12 buffer size supported by drive in kB */
		*write++ = check_cast<uint8_t>(x >> 8);
		*write++ = check_cast<uint8_t>(x & 0xff);

		x = 176 * 8; /* +14 current read speed selected in kB: 8X  (obsolete in MMC-3) */
		*write++ = check_cast<uint8_t>(x >> 8);
		*write++ = check_cast<uint8_t>(x & 0xff);

		*write++ = 0;    /* +16 Reserved */
		*write++ = 0x00; /* +17 Reserved | Reserved | Length | Length | LSBF | RCK | BCK | Reserved */

		x = 0; /* +18 maximum write speed supported in kB: 0  (obsolete in MMC-3) */
		*write++ = check_cast<uint8_t>(x >> 8);
		*write++ = check_cast<uint8_t>(x & 0xff);

		assert(x == 0); /* +20 current write speed in kB: 0  (obsolete in MMC-3) */
		*write++ = check_cast<uint8_t>(x >> 8);
		*write++ = check_cast<uint8_t>(x & 0xff);
		break;
	default:
		std::fill_n(write, 6, 0);
		write += 6;
		LOG_WARNING("IDE: MODE SENSE on page 0x%02x not supported", PAGE);
		break;
	}

	/* mode param header, data length */
	x = (uint32_t)(write - sector) - 2;
	sector[0] = (uint8_t)(x >> 8u);
	sector[1] = (uint8_t)x;
	/* page length */
	sector[8 + 1] = check_cast<uint8_t>((uint32_t)(write - sector) - 2 - 8);

	prepare_read(0, std::min((uint32_t)(write - sector), host_maximum_byte_count));
#if 0
    for (size_t i=0;i < sector_total;i++) printf("IDE: Sense %02x ",sector[i]);
#endif
}

void IDEATAPICDROMDevice::pause_resume()
{
	bool Resume = !!(atapi_cmd[8] & 1);

	CDROM_Interface *cdrom = getMSCDEXDrive();
	if (cdrom == nullptr) {
		LOG_WARNING("IDE: ATAPI READ TOC unable to get CDROM drive");
		sector_total = 0;
		return;
	}

	cdrom->PauseAudio(Resume);
}

void IDEATAPICDROMDevice::play_audio_msf()
{
	uint32_t start_lba = 0;
	uint32_t end_lba = 0;

	CDROM_Interface *cdrom = getMSCDEXDrive();
	if (cdrom == nullptr) {
		LOG_WARNING("IDE: ATAPI READ TOC unable to get CDROM drive");
		sector_total = 0;
		return;
	}

	if (atapi_cmd[3] == 0xFF && atapi_cmd[4] == 0xFF && atapi_cmd[5] == 0xFF)
		start_lba = 0xFFFFFFFF;
	else {
		start_lba = (atapi_cmd[3] * 60u * 75u) + (atapi_cmd[4] * 75u) + atapi_cmd[5];

		if (start_lba >= 150u)
			start_lba -= 150u; /* LBA sector 0 == M:S:F sector 0:2:0 */
		else
			start_lba = 0;
	}

	if (atapi_cmd[6] == 0xFF && atapi_cmd[7] == 0xFF && atapi_cmd[8] == 0xFF)
		end_lba = 0xFFFFFFFF;
	else {
		end_lba = (atapi_cmd[6] * 60u * 75u) + (atapi_cmd[7] * 75u) + atapi_cmd[8];

		if (end_lba >= 150u)
			end_lba -= 150u; /* LBA sector 0 == M:S:F sector 0:2:0 */
		else
			end_lba = 0;
	}

	if (start_lba == end_lba) {
		/* The play length field specifies the number of contiguous logical blocks that shall
		 * be played. A play length of zero indicates that no audio operation shall occur.
		 * This condition is not an error. */
		/* TBD: How do we interpret that? Does that mean audio playback stops? Or does it
		 * mean we do nothing to the state of audio playback? */
		sector_total = 0;
		return;
	}

	/* LBA 0xFFFFFFFF means start playing wherever the optics of the CD sit */
	if (start_lba != 0xFFFFFFFF)
		cdrom->PlayAudioSector(start_lba, end_lba - start_lba);
	else
		cdrom->PauseAudio(true);

	sector_total = 0;
}

void IDEATAPICDROMDevice::play_audio10()
{
	uint16_t play_length;
	uint32_t start_lba;

	CDROM_Interface *cdrom = getMSCDEXDrive();
	if (cdrom == nullptr) {
		LOG_WARNING("IDE: ATAPI READ TOC unable to get CDROM drive");
		sector_total = 0;
		return;
	}

	start_lba = ((uint32_t)atapi_cmd[2] << 24) + ((uint32_t)atapi_cmd[3] << 16) +
	            ((uint32_t)atapi_cmd[4] << 8) + ((uint32_t)atapi_cmd[5] << 0);

	play_length = check_cast<uint16_t>(((uint16_t)atapi_cmd[7] << 8) + ((uint16_t)atapi_cmd[8] << 0));

	if (play_length == 0) {
		/* The play length field specifies the number of contiguous logical blocks that shall
		 * be played. A play length of zero indicates that no audio operation shall occur.
		 * This condition is not an error. */
		/* TBD: How do we interpret that? Does that mean audio playback stops? Or does it
		 * mean we do nothing to the state of audio playback? */
		sector_total = 0;
		return;
	}

	/* LBA 0xFFFFFFFF means start playing wherever the optics of the CD sit */
	if (start_lba != 0xFFFFFFFF)
		cdrom->PlayAudioSector(start_lba, play_length);
	else
		cdrom->PauseAudio(true);

	sector_total = 0;
}

#if 0 /* TODO move to library */
static uint8_t dec2bcd(uint8_t c) {
    return ((c / 10) << 4) + (c % 10);
}
#endif

void IDEATAPICDROMDevice::read_toc()
{
	/* NTS: The SCSI MMC standards say we're allowed to indicate the return data
	 *      is longer than it's allocation length. But here's the thing: some MS-DOS
	 *      CD-ROM drivers will ask for the TOC but only provide enough room for one
	 *      entry (OAKCDROM.SYS) and if we signal more data than it's buffer, it will
	 *      reject our response and render the CD-ROM drive inaccessible. So to make
	 *      this emulation work, we have to cut our response short to the driver's
	 *      allocation length */
	uint32_t AllocationLength = ((uint32_t)atapi_cmd[7] << 8) + atapi_cmd[8];
	uint8_t Format = atapi_cmd[2] & 0xF;
	uint8_t Track = atapi_cmd[6];
	bool TIME = !!(atapi_cmd[1] & 2);
	uint8_t *write;
	uint8_t first, last, track;
	TMSF leadOut;

	CDROM_Interface *cdrom = getMSCDEXDrive();
	if (cdrom == nullptr) {
		LOG_WARNING("IDE: ATAPI READ TOC unable to get CDROM drive");
		prepare_read(0, 8);
		return;
	}

	std::fill_n(sector, 8, 0);

	if (!cdrom->GetAudioTracks(first, last, leadOut)) {
		LOG_WARNING("IDE: ATAPI READ TOC failed to get track info");
		prepare_read(0, 8);
		return;
	}

	/* start 2 bytes out. we'll fill in the data length later */
	write = sector + 2;

	if (Format == 1) { /* Read multisession info */
		uint8_t attr;
		TMSF start;

		*write++ = (uint8_t)1; /* @+2 first complete session */
		*write++ = (uint8_t)1; /* @+3 last complete session */

		if (!cdrom->GetAudioTrackInfo(first, start, attr)) {
			LOG_WARNING("IDE: ATAPI READ TOC unable to read track %u information", first);
			attr = 0x41; /* ADR=1 CONTROL=4 */
			start.min = 0;
			start.sec = 0;
			start.fr = 0;
		}

		LOG_MSG("IDE: ATAPI playing Track %u (attr=0x%02x %02u:%02u:%02u)", first, attr, start.min,
		        start.sec, start.fr);

		*write++ = 0x00;               /* entry+0 RESERVED */
		*write++ = (attr >> 4) | 0x10; /* entry+1 ADR=1 CONTROL=4 (DATA) */
		*write++ = first;              /* entry+2 TRACK */
		*write++ = 0x00;               /* entry+3 RESERVED */

		/* then, start address of first track in session */
		if (TIME) {
			*write++ = 0x00;
			*write++ = start.min;
			*write++ = start.sec;
			*write++ = start.fr;
		} else {
			uint32_t sec = (start.min * 60u * 75u) + (start.sec * 75u) + start.fr - 150u;
			*write++ = (uint8_t)(sec >> 24u);
			*write++ = (uint8_t)(sec >> 16u);
			*write++ = (uint8_t)(sec >> 8u);
			*write++ = (uint8_t)(sec >> 0u);
		}
	} else if (Format == 0) { /* Read table of contents */
		*write++ = first; /* @+2 */
		*write++ = last;  /* @+3 */

		for (track = first; track <= last; track++) {
			uint8_t attr;
			TMSF start;

			if (!cdrom->GetAudioTrackInfo(track, start, attr)) {
				LOG_WARNING("IDE: ATAPI READ TOC unable to read track %u information", track);
				attr = 0x41; /* ADR=1 CONTROL=4 */
				start.min = 0;
				start.sec = 0;
				start.fr = 0;
			}

			if (track < Track)
				continue;
			if ((write + 8) > (sector + AllocationLength))
				break;

			LOG_MSG("IDE: ATAPI playing Track %u (attr=0x%02x %02u:%02u:%02u)", first, attr,
			        start.min, start.sec, start.fr);

			*write++ = 0x00;               /* entry+0 RESERVED */
			*write++ = (attr >> 4) | 0x10; /* entry+1 ADR=1 CONTROL=4 (DATA) */
			*write++ = track;              /* entry+2 TRACK */
			*write++ = 0x00;               /* entry+3 RESERVED */
			if (TIME) {
				*write++ = 0x00;
				*write++ = start.min;
				*write++ = start.sec;
				*write++ = start.fr;
			} else {
				uint32_t sec = (start.min * 60u * 75u) + (start.sec * 75u) + start.fr - 150u;
				*write++ = (uint8_t)(sec >> 24u);
				*write++ = (uint8_t)(sec >> 16u);
				*write++ = (uint8_t)(sec >> 8u);
				*write++ = (uint8_t)(sec >> 0u);
			}
		}

		if ((write + 8) <= (sector + AllocationLength)) {
			*write++ = 0x00;
			*write++ = 0x14;
			*write++ = 0xAA; /*TRACK*/
			*write++ = 0x00;
			if (TIME) {
				*write++ = 0x00;
				*write++ = leadOut.min;
				*write++ = leadOut.sec;
				*write++ = leadOut.fr;
			} else {
				uint32_t sec = (leadOut.min * 60u * 75u) + (leadOut.sec * 75u) + leadOut.fr - 150u;
				*write++ = (uint8_t)(sec >> 24u);
				*write++ = (uint8_t)(sec >> 16u);
				*write++ = (uint8_t)(sec >> 8u);
				*write++ = (uint8_t)(sec >> 0u);
			}
		}
	} else {
		LOG_WARNING("IDE: ATAPI READ TOC Format=%u not supported", Format);
		prepare_read(0, 8);
		return;
	}

	/* update the TOC data length field */
	{
		uint32_t x = (uint32_t)(write - sector) - 2;
		sector[0] = check_cast<uint8_t>(x >> 8);
		sector[1] = x & 0xFF;
	}

	prepare_read(0, std::min(std::min((uint32_t)(write - sector), host_maximum_byte_count), AllocationLength));
}

/* when the ATAPI command has been accepted, and the timeout has passed */
void IDEATAPICDROMDevice::on_atapi_busy_time()
{
	/* if the drive is spinning up, then the command waits */
	if (loading_mode == LOAD_DISC_LOADING) {
		switch (atapi_cmd[0]) {
		case 0x00:                                                  /* TEST UNIT READY */
		case 0x03: /* REQUEST SENSE */ allow_writing = true; break; /* do not delay */
		default: PIC_AddEvent(IDE_DelayedCommand, 100 /*ms*/, controller->interface_index); return;
		}
	} else if (loading_mode == LOAD_DISC_READIED) {
		switch (atapi_cmd[0]) {
		case 0x00:                                                  /* TEST UNIT READY */
		case 0x03: /* REQUEST SENSE */ allow_writing = true; break; /* do not delay */
		default:
			if (!common_spinup_response(/*spin up*/ true, /*wait*/ false)) {
				count = 0x03;
				state = IDE_DEV_READY;
				feature = check_cast<uint16_t>(((sense[2] & 0xF) << 4) |
				                               ((sense[2] & 0xF) ? 0x04 /*abort*/ : 0x00));
				status = IDE_STATUS_DRIVE_READY |
				         ((sense[2] & 0xF) ? IDE_STATUS_ERROR : IDE_STATUS_DRIVE_SEEK_COMPLETE);
				controller->raise_irq();
				allow_writing = true;
				return;
			}
			break;
		}
	}

	switch (atapi_cmd[0]) {
	case 0x03: /* REQUEST SENSE */
		prepare_read(0, std::min(sense_length, host_maximum_byte_count));
		memcpy(sector, sense, sense_length);

		feature = 0x00;
		state = IDE_DEV_DATA_READ;
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRQ | IDE_STATUS_DRIVE_SEEK_COMPLETE;

		/* ATAPI protocol also says we write back into LBA 23:8 what
		 * we're going to transfer in the block */
		lba[2] = check_cast<uint16_t>(sector_total >> 8);
		lba[1] = check_cast<uint16_t>(sector_total);

		controller->raise_irq();
		allow_writing = true;
		break;
	case 0x1E: /* PREVENT ALLOW MEDIUM REMOVAL */
		count = 0x03;
		feature = 0x00;
		sector_total = 0x00;
		state = IDE_DEV_DATA_READ;
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;

		/* Don't care. Do nothing. */

		/* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
		lba[2] = check_cast<uint16_t>(sector_total >> 8);
		lba[1] = check_cast<uint16_t>(sector_total);

		controller->raise_irq();
		allow_writing = true;
		break;
	case 0x25: /* READ CAPACITY */ {
		const uint32_t secsize = 2048;
		uint8_t first, last;
		TMSF leadOut;

		CDROM_Interface *cdrom = getMSCDEXDrive();

		if (!cdrom->GetAudioTracks(first, last, leadOut))
			LOG_WARNING("IDE: ATAPI READ TOC failed to get track info");

		uint32_t sec = (leadOut.min * 60u * 75u) + (leadOut.sec * 75u) + leadOut.fr - 150u;

		prepare_read(0, std::min((uint32_t)8, host_maximum_byte_count));
		sector[0] = sec >> 24u;
		sector[1] = check_cast<uint8_t>(sec >> 16u);
		sector[2] = check_cast<uint8_t>(sec >> 8u);
		sector[3] = sec & 0xFF;
		sector[4] = secsize >> 24u;
		sector[5] = secsize >> 16u;
		sector[6] = secsize >> 8u;
		sector[7] = secsize & 0xFF;
		//          LOG_MSG("sec=%lu secsize=%lu",sec,secsize);

		feature = 0x00;
		state = IDE_DEV_DATA_READ;
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRQ | IDE_STATUS_DRIVE_SEEK_COMPLETE;

		/* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
		lba[2] = check_cast<uint16_t>(sector_total >> 8);
		lba[1] = check_cast<uint16_t>(sector_total);

		controller->raise_irq();
		allow_writing = true;
	} break;
	case 0x2B: /* SEEK */
		count = 0x03;
		feature = 0x00;
		sector_total = 0x00;
		state = IDE_DEV_DATA_READ;
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;

		/* Don't care. Do nothing. */

		/* Except... Windows 95's CD player expects the SEEK command to interrupt CD audio playback.
		 * In fact it depends on it to the exclusion of commands explicitly standardized to... you
		 * know... stop or pause playback. Oh Microsoft, you twits... */
		{
			CDROM_Interface *cdrom = getMSCDEXDrive();
			if (cdrom) {
				bool playing, pause;

				if (!cdrom->GetAudioStatus(playing, pause))
					playing = true;

				if (playing) {
					LOG_MSG("IDE: ATAPI: Interrupting CD audio playback due to SEEK");
					cdrom->StopAudio();
				}
			}
		}

		/* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
		lba[2] = check_cast<uint16_t>(sector_total >> 8);
		lba[1] = check_cast<uint16_t>(sector_total);

		controller->raise_irq();
		allow_writing = true;
		break;
	case 0x12: /* INQUIRY */
		/* NTS: the state of atapi_to_host doesn't seem to matter. */
		generate_mmc_inquiry();
		prepare_read(0, std::min((uint32_t)36, host_maximum_byte_count));

		feature = 0x00;
		state = IDE_DEV_DATA_READ;
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRQ | IDE_STATUS_DRIVE_SEEK_COMPLETE;

		/* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
		lba[2] = check_cast<uint16_t>(sector_total >> 8);
		lba[1] = check_cast<uint16_t>(sector_total);

		controller->raise_irq();
		allow_writing = true;
		break;
	case 0x28: /* READ(10) */
	case 0xA8: /* READ(12) */
		if (TransferLength == 0) {
			/* this is legal. the SCSI MMC standards say so.
			   and apparently, MSCDEX.EXE issues READ(10) commands with transfer length == 0
			   to test the drive, so we have to emulate this */
			feature = 0x00;
			count = 0x03;     /* no more transfer */
			sector_total = 0; /*nothing to transfer */
			state = IDE_DEV_READY;
			status = IDE_STATUS_DRIVE_READY;
		} else {
			/* OK, try to read */
			CDROM_Interface *cdrom = getMSCDEXDrive();
			bool res = (cdrom != nullptr ? cdrom->ReadSectorsHost(/*buffer*/ sector, false, LBA, TransferLength)
			                          : false);
			if (res) {
				prepare_read(0, std::min((TransferLength * 2048), host_maximum_byte_count));
				feature = 0x00;
				state = IDE_DEV_DATA_READ;
				status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRQ | IDE_STATUS_DRIVE_SEEK_COMPLETE;
			} else {
				feature = 0xF4;   /* abort sense=0xF */
				count = 0x03;     /* no more transfer */
				sector_total = 0; /*nothing to transfer */
				state = IDE_DEV_READY;
				status = IDE_STATUS_DRIVE_READY | IDE_STATUS_ERROR;
				LOG_WARNING("IDE: ATAPI: Failed to read %lu sectors at %lu",
				        (unsigned long)TransferLength, (unsigned long)LBA);
				/* TBD: write sense data */
			}
		}

		/* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
		lba[2] = check_cast<uint16_t>(sector_total >> 8);
		lba[1] = check_cast<uint16_t>(sector_total);

		controller->raise_irq();
		allow_writing = true;
		break;
	case 0x42: /* READ SUB-CHANNEL */
		read_subchannel();

		feature = 0x00;
		state = IDE_DEV_DATA_READ;
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRQ | IDE_STATUS_DRIVE_SEEK_COMPLETE;

		/* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
		lba[2] = check_cast<uint16_t>(sector_total >> 8);
		lba[1] = check_cast<uint16_t>(sector_total);

		controller->raise_irq();
		allow_writing = true;
		break;
	case 0x43: /* READ TOC */
		read_toc();

		feature = 0x00;
		state = IDE_DEV_DATA_READ;
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRQ | IDE_STATUS_DRIVE_SEEK_COMPLETE;

		/* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
		lba[2] = check_cast<uint16_t>(sector_total >> 8);
		lba[1] = check_cast<uint16_t>(sector_total);

		controller->raise_irq();
		allow_writing = true;
		break;
	case 0x45: /* PLAY AUDIO(10) */
		play_audio10();

		count = 0x03;
		feature = 0x00;
		sector_total = 0x00;
		state = IDE_DEV_DATA_READ;
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;

		/* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
		lba[2] = check_cast<uint16_t>(sector_total >> 8);
		lba[1] = check_cast<uint16_t>(sector_total);

		controller->raise_irq();
		allow_writing = true;
		break;
	case 0x47: /* PLAY AUDIO MSF */
		play_audio_msf();

		count = 0x03;
		feature = 0x00;
		sector_total = 0x00;
		state = IDE_DEV_DATA_READ;
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;

		/* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
		lba[2] = check_cast<uint16_t>(sector_total >> 8);
		lba[1] = check_cast<uint16_t>(sector_total);

		controller->raise_irq();
		allow_writing = true;
		break;
	case 0x4B: /* PAUSE/RESUME */
		pause_resume();

		count = 0x03;
		feature = 0x00;
		sector_total = 0x00;
		state = IDE_DEV_DATA_READ;
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;

		/* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
		lba[2] = check_cast<uint16_t>(sector_total >> 8);
		lba[1] = check_cast<uint16_t>(sector_total);

		controller->raise_irq();
		allow_writing = true;
		break;
	case 0x55: /* MODE SELECT(10) */
		/* we need the data written first, will act in I/O completion routine */
		{
			uint32_t x;

			x = (uint32_t)lba[1] + ((uint32_t)lba[2] << 8u);

			/* Windows 95 likes to set 0xFFFF here for whatever reason.
			 * Negotiate it down to a maximum of 512 for sanity's sake */
			if (x > 512)
				x = 512;
			lba[2] = check_cast<uint16_t>(x >> 8u);
			lba[1] = check_cast<uint16_t>(x);

			//              LOG_MSG("MODE SELECT expecting %u bytes",x);
			prepare_write(0, (x + 1u) & (~1u));
		}

		feature = 0x00;
		state = IDE_DEV_DATA_WRITE;
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRQ | IDE_STATUS_DRIVE_SEEK_COMPLETE;
		controller->raise_irq();
		allow_writing = true;
		break;
	case 0x5A: /* MODE SENSE(10) */
		mode_sense();

		feature = 0x00;
		state = IDE_DEV_DATA_READ;
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRQ | IDE_STATUS_DRIVE_SEEK_COMPLETE;

		/* ATAPI protocol also says we write back into LBA 23:8 what we're going to transfer in the block */
		lba[2] = check_cast<uint16_t>(sector_total >> 8);
		lba[1] = check_cast<uint16_t>(sector_total);

		controller->raise_irq();
		allow_writing = true;
		break;
	default:
		LOG_WARNING("IDE: Unknown ATAPI command after busy wait. Why?");
		abort_error();
		controller->raise_irq();
		allow_writing = true;
		break;
	}
}

void IDEATAPICDROMDevice::set_sense(uint8_t SK, uint8_t ASC, uint8_t ASCQ, uint32_t len)
{
	if (len < 18)
		len = 18;
	memset(sense, 0, len);
	sense_length = len;

	sense[0] = 0x70;                          /* RESPONSE CODE */
	sense[2] = SK & 0xF;                      /* SENSE KEY */
	sense[7] = check_cast<uint8_t>(len - 18); /* additional sense length */
	sense[12] = ASC;
	sense[13] = ASCQ;
}

IDEATAPICDROMDevice::IDEATAPICDROMDevice(IDEController *c, uint8_t requested_drive_index)
        : IDEDevice(c, IDE_TYPE_CDROM),
          drive_index(requested_drive_index)
{
	IDEATAPICDROMDevice::set_sense(/*SK=*/0);

	/* TBD: Spinup/down times should be dosbox.conf configurable, if the DOSBox gamers
	 *        care more about loading times than emulation accuracy. */
	if (c->cd_insertion_time > 0)
		cd_insertion_time = c->cd_insertion_time;

	if (c->spinup_time > 0)
		spinup_time = c->spinup_time;

	if (c->spindown_timeout > 0)
		spindown_timeout = c->spindown_timeout;
}

void IDEATAPICDROMDevice::on_mode_select_io_complete()
{
	uint32_t AllocationLength = ((uint32_t)atapi_cmd[7] << 8) + atapi_cmd[8];
	uint8_t *scan, *fence;
	size_t i;

	/* the first 8 bytes are a mode parameter header.
	 * It's supposed to provide length, density, etc. or whatever the hell
	 * it means. Windows 95 seems to send all zeros there, so ignore it.
	 *
	 * we care about the bytes following it, which contain page_0 mode
	 * pages */

	scan = sector + 8;
	fence = sector + std::min(sector_total, AllocationLength);

	while ((scan + 2) < fence) {
		uint8_t PAGE = *scan++;
		auto LEN = (uint32_t)(*scan++);

		if ((scan + LEN) > fence) {
			LOG_WARNING("IDE: ATAPI MODE SELECT warning, page_0 length extends %u bytes past buffer",
			        (uint32_t)(scan + LEN - fence));
			break;
		}

		LOG_MSG("IDE: ATAPI MODE SELECT, PAGE 0x%02x len=%u", PAGE, LEN);
		LOG_MSG("  ");
		for (i = 0; i < LEN; i++)
			LOG_MSG("%02x ", scan[i]);
		LOG_MSG(" ");

		scan += LEN;
	}
}

void IDEATAPICDROMDevice::atapi_io_completion()
{
	/* for most ATAPI PACKET commands, the transfer is done and we need to clear
	   all indication of a possible data transfer */

	if (count == 0x00) { /* the command was expecting data. now it can act on it */
		switch (atapi_cmd[0]) {
		case 0x55: /* MODE SELECT(10) */ on_mode_select_io_complete(); break;
		}
	}

	count = 0x03; /* no more data (command/data=1, input/output=1) */
	status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
	state = IDE_DEV_READY;
	allow_writing = true;

	/* Apparently: real IDE ATAPI controllers fire another IRQ after the transfer.
	   And there are MS-DOS CD-ROM drivers that assume that. */
	controller->raise_irq();
}

void IDEATAPICDROMDevice::io_completion()
{
	/* lower DRQ */
	status &= ~IDE_STATUS_DRQ;

	/* depending on the command, either continue it or finish up */
	switch (command) {
	case 0xA0: /*ATAPI PACKET*/ atapi_io_completion(); break;
	default: /* most commands: signal drive ready, return to ready state */
		/* NTS: Some MS-DOS CD-ROM drivers will loop endlessly if we never set "drive seek complete"
		        because they like to hit the device with DEVICE RESET (08h) whether or not it's
		    a hard disk or CD-ROM drive */
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
		state = IDE_DEV_READY;
		allow_writing = true;
		break;
	}
}

bool IDEATADevice::increment_current_address(uint32_t n)
{
	if (n == 0)
		return false;

	if (drivehead_is_lba(drivehead)) {
		/* 28-bit LBA:
		 *    drivehead: 27:24
		 *    lba[2]:    23:16
		 *    lba[1]:    15:8
		 *    lba[0]:    7:0 */
		do {
			if (((++lba[0]) & 0xFF) == 0x00) {
				lba[0] = 0x00;
				if (((++lba[1]) & 0xFF) == 0x00) {
					lba[1] = 0x00;
					if (((++lba[2]) & 0xFF) == 0x00) {
						lba[2] = 0x00;
						if (((++drivehead) & 0xF) == 0) {
							drivehead -= 0x10;
							return false;
						}
					}
				}
			}
		} while ((--n) != 0);
	} else {
		/* C/H/S increment with rollover */
		do {
			/* increment sector */
			if (((++lba[0]) & 0xFF) == ((sects + 1) & 0xFF)) {
				lba[0] = 1;
				/* increment head */
				if (((++drivehead) & 0xF) == (heads & 0xF)) {
					drivehead &= 0xF0;
					if (heads == 16)
						drivehead -= 0x10;
					/* increment cylinder */
					if (((++lba[1]) & 0xFF) == 0x00) {
						if (((++lba[2]) & 0xFF) == 0x00) {
							return false;
						}
					}
				}
			}
		} while ((--n) != 0);
	}

	return true;
}

void IDEATADevice::io_completion()
{
	/* lower DRQ */
	status &= ~IDE_STATUS_DRQ;

	/* depending on the command, either continue it or finish up */
	switch (command) {
	case 0x20: /* READ SECTOR */
		/* OK, decrement count, increment address */
		/* NTS: Remember that count == 0 means the host wanted to transfer 256 sectors */
		progress_count++;
		if ((count & 0xFF) == 1) {
			/* end of the transfer */
			count = 0;
			status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
			state = IDE_DEV_READY;
			allow_writing = true;
			return;
		} else if ((count & 0xFF) == 0)
			count = 255;
		else
			count--;

		if (!increment_current_address()) {
			LOG_WARNING("IDE: READ advance error");
			abort_error();
			return;
		}

		/* cause another delay, another sector read */
		state = IDE_DEV_BUSY;
		status = IDE_STATUS_BUSY;
		PIC_AddEvent(IDE_DelayedCommand, 0.00001 /*ms*/, controller->interface_index);
		break;
	case 0xC5: /* WRITE MULTIPLE */
	case 0x30: /* WRITE SECTOR */
		/* this is where the drive has accepted the sector, lowers DRQ, and begins executing the command */
		state = IDE_DEV_BUSY;
		status = IDE_STATUS_BUSY;
		PIC_AddEvent(IDE_DelayedCommand, ((progress_count == 0 && !faked_command) ? 0.1 : 0.00001) /*ms*/,
		             controller->interface_index);
		break;
	case 0xC4: /* READ MULTIPLE */
		/* OK, decrement count, increment address */
		/* NTS: Remember that count == 0 means the host wanted to transfer 256 sectors */
		for (uint32_t cc = 0; cc < multiple_sector_count; cc++) {
			progress_count++;
			if ((count & 0xFF) == 1) {
				/* end of the transfer */
				count = 0;
				status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
				state = IDE_DEV_READY;
				allow_writing = true;
				return;
			} else if ((count & 0xFF) == 0)
				count = 255;
			else
				count--;

			if (!increment_current_address()) {
				LOG_WARNING("IDE: READ advance error");
				abort_error();
				return;
			}
		}

		/* cause another delay, another sector read */
		state = IDE_DEV_BUSY;
		status = IDE_STATUS_BUSY;
		PIC_AddEvent(IDE_DelayedCommand, 0.00001 /*ms*/, controller->interface_index);
		break;
	default: /* most commands: signal drive ready, return to ready state */
		/* NTS: Some MS-DOS CD-ROM drivers will loop endlessly if we never set "drive seek complete"
		        because they like to hit the device with DEVICE RESET (08h) whether or not it's
		    a hard disk or CD-ROM drive */
		count = 0;
		drivehead &= 0xF0;
		lba[0] = 0;
		lba[1] = lba[2] = 0;
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
		state = IDE_DEV_READY;
		allow_writing = true;
		break;
	}
}

uint32_t IDEATAPICDROMDevice::data_read(io_width_t width)
{
	uint32_t w = ~0u;

	if (state != IDE_DEV_DATA_READ)
		return 0xFFFFUL;

	if (!(status & IDE_STATUS_DRQ)) {
		LOG_MSG("IDE: Data read when DRQ=0");
		return 0xFFFFUL;
	}

	if (sector_i >= sector_total)
		return 0xFFFFUL;

	if (width == io_width_t::dword) {
		w = host_readd(sector + sector_i);
		sector_i += 4;
	} else if (width == io_width_t::word) {
		w = host_readw(sector + sector_i);
		sector_i += 2;
	}
	/* NTS: Some MS-DOS CD-ROM drivers like OAKCDROM.SYS use byte-wide I/O for the initial identification */
	else if (width == io_width_t::byte) {
		w = sector[sector_i++];
	}

	if (sector_i >= sector_total)
		io_completion();

	return w;
}

/* TBD: Your code should also be paying attention to the "transfer length" field
         in many of the commands here. Right now it doesn't matter. */
void IDEATAPICDROMDevice::atapi_cmd_completion()
{
#if 0
    LOG_MSG("IDE: ATAPI command %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x to_host=%u",
        atapi_cmd[ 0],atapi_cmd[ 1],atapi_cmd[ 2],atapi_cmd[ 3],atapi_cmd[ 4],atapi_cmd[ 5],
        atapi_cmd[ 6],atapi_cmd[ 7],atapi_cmd[ 8],atapi_cmd[ 9],atapi_cmd[10],atapi_cmd[11],
        atapi_to_host);
#endif

	switch (atapi_cmd[0]) {
	case 0x00: /* TEST UNIT READY */
		if (common_spinup_response(/*spin up*/ false, /*wait*/ false))
			set_sense(0); /* <- nothing wrong */

		count = 0x03;
		state = IDE_DEV_READY;
		feature = check_cast<uint16_t>(((sense[2] & 0xF) << 4) |
		                               ((sense[2] & 0xF) ? 0x04 /*abort*/ : 0x00));
		status = IDE_STATUS_DRIVE_READY |
		         ((sense[2] & 0xF) ? IDE_STATUS_ERROR : IDE_STATUS_DRIVE_SEEK_COMPLETE);
		controller->raise_irq();
		allow_writing = true;
		break;
	case 0x03: /* REQUEST SENSE */
		count = 0x02;
		state = IDE_DEV_ATAPI_BUSY;
		status = IDE_STATUS_BUSY;
		PIC_AddEvent(IDE_DelayedCommand, (faked_command ? 0.000001 : 1) /*ms*/, controller->interface_index);
		break;
	case 0x1E: /* PREVENT ALLOW MEDIUM REMOVAL */
		count = 0x02;
		state = IDE_DEV_ATAPI_BUSY;
		status = IDE_STATUS_BUSY;
		PIC_AddEvent(IDE_DelayedCommand, (faked_command ? 0.000001 : 1) /*ms*/, controller->interface_index);
		break;
	case 0x25: /* READ CAPACITY */
		count = 0x02;
		state = IDE_DEV_ATAPI_BUSY;
		status = IDE_STATUS_BUSY;
		PIC_AddEvent(IDE_DelayedCommand, (faked_command ? 0.000001 : 1) /*ms*/, controller->interface_index);
		break;
	case 0x2B: /* SEEK */
		if (common_spinup_response(/*spin up*/ true, /*wait*/ true)) {
			set_sense(0); /* <- nothing wrong */
			count = 0x02;
			state = IDE_DEV_ATAPI_BUSY;
			status = IDE_STATUS_BUSY;
			PIC_AddEvent(IDE_DelayedCommand, (faked_command ? 0.000001 : 1) /*ms*/,
			             controller->interface_index);
		} else {
			count = 0x03;
			state = IDE_DEV_READY;
			feature = check_cast<uint16_t>(((sense[2] & 0xF) << 4) |
			                               ((sense[2] & 0xF) ? 0x04 /*abort*/ : 0x00));
			status = IDE_STATUS_DRIVE_READY |
			         ((sense[2] & 0xF) ? IDE_STATUS_ERROR : IDE_STATUS_DRIVE_SEEK_COMPLETE);
			controller->raise_irq();
			allow_writing = true;
		}
		break;
	case 0x12: /* INQUIRY */
		count = 0x02;
		state = IDE_DEV_ATAPI_BUSY;
		status = IDE_STATUS_BUSY;
		PIC_AddEvent(IDE_DelayedCommand, (faked_command ? 0.000001 : 1) /*ms*/, controller->interface_index);
		break;
	case 0xA8: /* READ(12) */
		if (common_spinup_response(/*spin up*/ true, /*wait*/ true)) {
			set_sense(0); /* <- nothing wrong */

			/* TBD: MSCDEX.EXE appears to test the drive by issuing READ(10) with transfer
			   length == 0. This is all well and good but our response seems to cause a temporary
			   2-3 second pause for each attempt. Why? */
			LBA = ((uint32_t)atapi_cmd[2] << 24UL) | ((uint32_t)atapi_cmd[3] << 16UL) |
			      ((uint32_t)atapi_cmd[4] << 8UL) | ((uint32_t)atapi_cmd[5] << 0UL);
			TransferLength = ((uint32_t)atapi_cmd[6] << 24UL) | ((uint32_t)atapi_cmd[7] << 16UL) |
			                 ((uint32_t)atapi_cmd[8] << 8UL) | ((uint32_t)atapi_cmd[9]);

			/* TBD: We actually should NOT be capping the transfer length, but instead should
			   be breaking the larger transfer into smaller DRQ block transfers like
			   most IDE ATAPI drives do. Writing the test IDE code taught me that if you
			   go to most drives and request a transfer length of 0xFFFE the drive will
			   happily set itself up to transfer that many sectors in one IDE command! */
			/* NTS: In case you're wondering, it's legal to issue READ(10) with transfer length ==
			   0. MSCDEX.EXE does it when starting up, for example */
			if ((TransferLength * 2048) > sizeof(sector))
				TransferLength = sizeof(sector) / 2048;

			count = 0x02;
			state = IDE_DEV_ATAPI_BUSY;
			status = IDE_STATUS_BUSY;
			/* TBD: Emulate CD-ROM spin-up delay, and seek delay */
			PIC_AddEvent(IDE_DelayedCommand, (faked_command ? 0.000001 : 3) /*ms*/,
			             controller->interface_index);
		} else {
			count = 0x03;
			state = IDE_DEV_READY;
			feature = check_cast<uint16_t>(((sense[2] & 0xF) << 4) |
			                               ((sense[2] & 0xF) ? 0x04 /*abort*/ : 0x00));
			status = IDE_STATUS_DRIVE_READY |
			         ((sense[2] & 0xF) ? IDE_STATUS_ERROR : IDE_STATUS_DRIVE_SEEK_COMPLETE);
			controller->raise_irq();
			allow_writing = true;
		}
		break;
	case 0x28: /* READ(10) */
		if (common_spinup_response(/*spin up*/ true, /*wait*/ true)) {
			set_sense(0); /* <- nothing wrong */

			/* TBD: MSCDEX.EXE appears to test the drive by issuing READ(10) with transfer
			   length == 0. This is all well and good but our response seems to cause a temporary
			   2-3 second pause for each attempt. Why? */
			LBA = ((uint32_t)atapi_cmd[2] << 24UL) | ((uint32_t)atapi_cmd[3] << 16UL) |
			      ((uint32_t)atapi_cmd[4] << 8UL) | ((uint32_t)atapi_cmd[5] << 0UL);
			TransferLength = ((uint32_t)atapi_cmd[7] << 8) | ((uint32_t)atapi_cmd[8]);

			/* TBD: We actually should NOT be capping the transfer length, but instead should
			   be breaking the larger transfer into smaller DRQ block transfers like
			   most IDE ATAPI drives do. Writing the test IDE code taught me that if you
			   go to most drives and request a transfer length of 0xFFFE the drive will
			   happily set itself up to transfer that many sectors in one IDE command! */
			/* NTS: In case you're wondering, it's legal to issue READ(10) with transfer length ==
			   0. MSCDEX.EXE does it when starting up, for example */
			if ((TransferLength * 2048) > sizeof(sector))
				TransferLength = sizeof(sector) / 2048;

			count = 0x02;
			state = IDE_DEV_ATAPI_BUSY;
			status = IDE_STATUS_BUSY;
			/* TBD: Emulate CD-ROM spin-up delay, and seek delay */
			PIC_AddEvent(IDE_DelayedCommand, (faked_command ? 0.000001 : 3) /*ms*/,
			             controller->interface_index);
		} else {
			count = 0x03;
			state = IDE_DEV_READY;
			feature = check_cast<uint16_t>(((sense[2] & 0xF) << 4) |
			                               ((sense[2] & 0xF) ? 0x04 /*abort*/ : 0x00));
			status = IDE_STATUS_DRIVE_READY |
			         ((sense[2] & 0xF) ? IDE_STATUS_ERROR : IDE_STATUS_DRIVE_SEEK_COMPLETE);
			controller->raise_irq();
			allow_writing = true;
		}
		break;
	case 0x42: /* READ SUB-CHANNEL */
		if (common_spinup_response(/*spin up*/ true, /*wait*/ true)) {
			set_sense(0); /* <- nothing wrong */

			count = 0x02;
			state = IDE_DEV_ATAPI_BUSY;
			status = IDE_STATUS_BUSY;
			PIC_AddEvent(IDE_DelayedCommand, (faked_command ? 0.000001 : 1) /*ms*/,
			             controller->interface_index);
		} else {
			count = 0x03;
			state = IDE_DEV_READY;
			feature = check_cast<uint16_t>(((sense[2] & 0xF) << 4) |
			                               ((sense[2] & 0xF) ? 0x04 /*abort*/ : 0x00));
			status = IDE_STATUS_DRIVE_READY |
			         ((sense[2] & 0xF) ? IDE_STATUS_ERROR : IDE_STATUS_DRIVE_SEEK_COMPLETE);
			controller->raise_irq();
			allow_writing = true;
		}
		break;
	case 0x43: /* READ TOC */
		if (common_spinup_response(/*spin up*/ true, /*wait*/ true)) {
			set_sense(0); /* <- nothing wrong */

			count = 0x02;
			state = IDE_DEV_ATAPI_BUSY;
			status = IDE_STATUS_BUSY;
			PIC_AddEvent(IDE_DelayedCommand, (faked_command ? 0.000001 : 1) /*ms*/,
			             controller->interface_index);
		} else {
			count = 0x03;
			state = IDE_DEV_READY;
			feature = check_cast<uint16_t>(((sense[2] & 0xF) << 4) |
			                               ((sense[2] & 0xF) ? 0x04 /*abort*/ : 0x00));
			status = IDE_STATUS_DRIVE_READY |
			         ((sense[2] & 0xF) ? IDE_STATUS_ERROR : IDE_STATUS_DRIVE_SEEK_COMPLETE);
			controller->raise_irq();
			allow_writing = true;
		}
		break;
	case 0x45: /* PLAY AUDIO (1) */
	case 0x47: /* PLAY AUDIO MSF */
	case 0x4B: /* PAUSE/RESUME */
		if (common_spinup_response(/*spin up*/ true, /*wait*/ true)) {
			set_sense(0); /* <- nothing wrong */

			count = 0x02;
			state = IDE_DEV_ATAPI_BUSY;
			status = IDE_STATUS_BUSY;
			PIC_AddEvent(IDE_DelayedCommand, (faked_command ? 0.000001 : 1) /*ms*/,
			             controller->interface_index);
		} else {
			count = 0x03;
			state = IDE_DEV_READY;
			feature = check_cast<uint16_t>(((sense[2] & 0xF) << 4) |
			                               ((sense[2] & 0xF) ? 0x04 /*abort*/ : 0x00));
			status = IDE_STATUS_DRIVE_READY |
			         ((sense[2] & 0xF) ? IDE_STATUS_ERROR : IDE_STATUS_DRIVE_SEEK_COMPLETE);
			controller->raise_irq();
			allow_writing = true;
		}
		break;
	case 0x55:            /* MODE SELECT(10) */
		count = 0x00; /* we will be accepting data */
		state = IDE_DEV_ATAPI_BUSY;
		status = IDE_STATUS_BUSY;
		PIC_AddEvent(IDE_DelayedCommand, (faked_command ? 0.000001 : 1) /*ms*/, controller->interface_index);
		break;
	case 0x5A: /* MODE SENSE(10) */
		count = 0x02;
		state = IDE_DEV_ATAPI_BUSY;
		status = IDE_STATUS_BUSY;
		PIC_AddEvent(IDE_DelayedCommand, (faked_command ? 0.000001 : 1) /*ms*/, controller->interface_index);
		break;
	default:
		/* we don't know the command, immediately return an error */
		LOG_WARNING("IDE: Unknown ATAPI command %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
		        atapi_cmd[0], atapi_cmd[1], atapi_cmd[2], atapi_cmd[3], atapi_cmd[4], atapi_cmd[5],
		        atapi_cmd[6], atapi_cmd[7], atapi_cmd[8], atapi_cmd[9], atapi_cmd[10], atapi_cmd[11]);

		abort_error();
		count = 0x03; /* no more data (command/data=1, input/output=1) */
		feature = 0xF4;
		controller->raise_irq();
		allow_writing = true;
		break;
	}
}

void IDEATAPICDROMDevice::data_write(uint32_t v, io_width_t width)
{
	if (state == IDE_DEV_ATAPI_PACKET_COMMAND) {
		if (atapi_cmd_i < atapi_cmd_total)
			atapi_cmd[atapi_cmd_i++] = check_cast<uint8_t>(v & 0xFF);
		if ((width == io_width_t::word || width == io_width_t::dword) && atapi_cmd_i < atapi_cmd_total)
			atapi_cmd[atapi_cmd_i++] = check_cast<uint8_t>(v >> 8) & 0xFF;
		if (width == io_width_t::dword && atapi_cmd_i < atapi_cmd_total) {
			atapi_cmd[atapi_cmd_i++] = check_cast<uint8_t>(v >> 16) & 0xFF;
			atapi_cmd[atapi_cmd_i++] = check_cast<uint8_t>(v >> 24) & 0xFF;
		}

		if (atapi_cmd_i >= atapi_cmd_total)
			atapi_cmd_completion();
	} else {
		if (state != IDE_DEV_DATA_WRITE) {
			LOG_WARNING("IDE: ATAPI data write when device not in data_write state");
			return;
		}
		if (!(status & IDE_STATUS_DRQ)) {
			LOG_WARNING("IDE: ATAPI data write with drq=0");
			return;
		}
		if ((sector_i + static_cast<uint8_t>(width)) > sector_total) {
			LOG_WARNING("IDE: ATAPI sector already full %lu / %lu", (unsigned long)sector_i,
			        (unsigned long)sector_total);
			return;
		}

		if (width == io_width_t::dword) {
			host_writed(sector + sector_i, v);
			sector_i += 4;
		} else if (width == io_width_t::word) {
			host_writew(sector + sector_i, check_cast<uint16_t>(v));
			sector_i += 2;
		} else if (width == io_width_t::byte) {
			sector[sector_i++] = check_cast<uint8_t>(v);
		}

		if (sector_i >= sector_total)
			io_completion();
	}
}

uint32_t IDEATADevice::data_read(io_width_t width)
{
	uint32_t w = ~0u;

	if (state != IDE_DEV_DATA_READ)
		return 0xFFFFUL;

	if (!(status & IDE_STATUS_DRQ)) {
		LOG_MSG("IDE: Data read when DRQ=0");
		return 0xFFFFUL;
	}

	if ((sector_i + static_cast<uint8_t>(width)) > sector_total) {
		LOG_WARNING("IDE: ATA: sector already read %lu / %lu", (unsigned long)sector_i,
		        (unsigned long)sector_total);
		return 0xFFFFUL;
	}

	if (width == io_width_t::dword) {
		w = host_readd(sector + sector_i);
		sector_i += 4;
	} else if (width == io_width_t::word) {
		w = host_readw(sector + sector_i);
		sector_i += 2;
	}
	/* NTS: Some MS-DOS CD-ROM drivers like OAKCDROM.SYS use byte-wide I/O for the initial identification */
	else if (width == io_width_t::byte) {
		w = sector[sector_i++];
	}

	if (sector_i >= sector_total)
		io_completion();

	return w;
}

void IDEATADevice::data_write(uint32_t v, io_width_t width)
{
	if (state != IDE_DEV_DATA_WRITE) {
		LOG_WARNING("IDE: ATA: data write when device not in data_write state");
		return;
	}
	if (!(status & IDE_STATUS_DRQ)) {
		LOG_WARNING("IDE: ATA: data write with drq=0");
		return;
	}
	if ((sector_i + static_cast<uint8_t>(width)) > sector_total) {
		LOG_WARNING("IDE: ATA: sector already full %lu / %lu", (unsigned long)sector_i,
		        (unsigned long)sector_total);
		return;
	}

	if (width == io_width_t::dword) {
		host_writed(sector + sector_i, v);
		sector_i += 4;
	} else if (width == io_width_t::word) {
		host_writew(sector + sector_i, check_cast<uint16_t>(v));
		sector_i += 2;
	} else if (width == io_width_t::byte) {
		sector[sector_i++] = check_cast<uint8_t>(v);
	}

	if (sector_i >= sector_total)
		io_completion();
}

void IDEATAPICDROMDevice::prepare_read(uint32_t offset, uint32_t size)
{
	/* I/O must be WORD ALIGNED */
	assert((offset & 1) == 0);
	//  assert((size&1) == 0);

	sector_i = offset;
	sector_total = size;
	assert(sector_i <= sector_total);
	assert(sector_total <= sizeof(sector));
}

void IDEATAPICDROMDevice::prepare_write(uint32_t offset, uint32_t size)
{
	/* I/O must be WORD ALIGNED */
	assert((offset & 1) == 0);
	//  assert((size&1) == 0);

	sector_i = offset;
	sector_total = size;
	assert(sector_i <= sector_total);
	assert(sector_total <= sizeof(sector));
}

void IDEATADevice::prepare_write(uint32_t offset, uint32_t size)
{
	/* I/O must be WORD ALIGNED */
	assert((offset & 1) == 0);
	//  assert((size&1) == 0);

	sector_i = offset;
	sector_total = size;
	assert(sector_i <= sector_total);
	assert(sector_total <= sizeof(sector));
}

void IDEATADevice::prepare_read(uint32_t offset, uint32_t size)
{
	/* I/O must be WORD ALIGNED */
	assert((offset & 1) == 0);
	//  assert((size&1) == 0);

	sector_i = offset;
	sector_total = size;
	assert(sector_i <= sector_total);
	assert(sector_total <= sizeof(sector));
}

void IDEATAPICDROMDevice::generate_mmc_inquiry()
{
	uint32_t i;

	/* IN RESPONSE TO ATAPI COMMAND 0x12: INQUIRY */
	std::fill_n(sector, 36, 0);
	sector[0] = (0 << 5) | 5; /* Peripheral qualifier=0   device type=5 (CDROM) */
	sector[1] = 0x80;         /* RMB=1 removable media */
	sector[3] = 0x21;
	sector[4] = 36 - 5; /* additional length */

	for (i = 0; i < 8 && i < id_mmc_vendor_id.length(); i++)
		sector[i + 8] = (uint8_t)id_mmc_vendor_id[i];
	for (; i < 8; i++)
		sector[i + 8] = ' ';

	for (i = 0; i < 16 && i < id_mmc_product_id.length(); i++)
		sector[i + 16] = (uint8_t)id_mmc_product_id[i];
	for (; i < 16; i++)
		sector[i + 16] = ' ';

	for (i = 0; i < 4 && i < id_mmc_product_rev.length(); i++)
		sector[i + 32] = (uint8_t)id_mmc_product_rev[i];
	for (; i < 4; i++)
		sector[i + 32] = ' ';
}

void IDEATAPICDROMDevice::generate_identify_device()
{
	uint8_t csum;
	uint32_t i;

	/* IN RESPONSE TO IDENTIFY DEVICE (0xA1)
	   GENERATE 512-BYTE REPLY */
	std::fill_n(sector, 512, 0);

	host_writew(sector + (0 * 2), 0x85C0U); /* ATAPI device, command set #5 (what the fuck does that
	                                           mean?), removable, */

	for (i = 0; i < 20 && i < id_serial.length(); i++)
		sector[(i ^ 1) + (10 * 2)] = (uint8_t)id_serial[i];
	for (; i < 20; i++)
		sector[(i ^ 1) + (10 * 2)] = ' ';

	for (i = 0; i < 8 && i < id_firmware_rev.length(); i++)
		sector[(i ^ 1) + (23 * 2)] = (uint8_t)id_firmware_rev[i];
	for (; i < 8; i++)
		sector[(i ^ 1) + (23 * 2)] = ' ';

	for (i = 0; i < 40 && i < id_model.length(); i++)
		sector[(i ^ 1) + (27 * 2)] = (uint8_t)id_model[i];
	for (; i < 40; i++)
		sector[(i ^ 1) + (27 * 2)] = ' ';

	host_writew(sector + (49 * 2), 0x0800UL |         /*IORDY supported*/
	                                       0x0200UL | /*must be one*/
	                                       0);
	host_writew(sector + (50 * 2), 0x4000UL);
	host_writew(sector + (51 * 2), 0x00F0UL);
	host_writew(sector + (52 * 2), 0x00F0UL);
	host_writew(sector + (53 * 2), 0x0006UL);
	host_writew(sector + (64 * 2), /* PIO modes supported */
	            0x0003UL);
	host_writew(sector + (67 * 2), /* PIO cycle time */
	            0x0078UL);
	host_writew(sector + (68 * 2), /* PIO cycle time */
	            0x0078UL);
	host_writew(sector + (80 * 2), 0x007E); /* major version number. Here we say we support ATA-1 through ATA-8 */
	host_writew(sector + (81 * 2), 0x0022); /* minor version */
	host_writew(sector + (82 * 2), 0x4008); /* command set: NOP, DEVICE RESET[XXXXX], POWER MANAGEMENT */
	host_writew(sector + (83 * 2), 0x0000); /* command set: LBA48[XXXX] */
	host_writew(sector + (85 * 2), 0x4208); /* commands in 82 enabled */
	host_writew(sector + (86 * 2), 0x0000); /* commands in 83 enabled */

	/* ATA-8 integrity checksum */
	sector[510] = 0xA5;
	csum = 0;
	for (i = 0; i < 511; i++)
		csum += sector[i];
	sector[511] = 0 - csum;
}

void IDEATADevice::generate_identify_device()
{
	//  imageDisk *disk = getBIOSdisk();
	uint8_t csum;
	uint32_t i;

	/* IN RESPONSE TO IDENTIFY DEVICE (0xEC)
	   GENERATE 512-BYTE REPLY */
	std::fill_n(sector, 512, 0);

	/* total disk capacity in sectors */
	uint64_t total = sects;
	total *= cyls;
	total *= heads;

	uint64_t ptotal = phys_sects;
	ptotal *= phys_cyls;
	ptotal *= phys_heads;

	host_writew(sector + (0 * 2), 0x0040); /* bit 6: 1=fixed disk */
	host_writew(sector + (1 * 2), check_cast<uint16_t>(phys_cyls));
	host_writew(sector + (3 * 2), check_cast<uint16_t>(phys_heads));
	host_writew(sector + (4 * 2), check_cast<uint16_t>(phys_sects * 512)); /* unformatted bytes per track */
	host_writew(sector + (5 * 2), 512); /* unformatted bytes per sector */
	host_writew(sector + (6 * 2), check_cast<uint16_t>(phys_sects));

	for (i = 0; i < 20 && i < id_serial.length(); i++)
		sector[(i ^ 1) + (10 * 2)] = (uint8_t)id_serial[i];
	for (; i < 20; i++)
		sector[(i ^ 1) + (10 * 2)] = ' ';

	host_writew(sector + (20 * 2), 1); /* ATA-1: single-ported single sector buffer */
	host_writew(sector + (21 * 2), 4); /* ATA-1: ECC bytes on read/write long */

	for (i = 0; i < 8 && i < id_firmware_rev.length(); i++)
		sector[(i ^ 1) + (23 * 2)] = (uint8_t)id_firmware_rev[i];
	for (; i < 8; i++)
		sector[(i ^ 1) + (23 * 2)] = ' ';

	for (i = 0; i < 40 && i < id_model.length(); i++)
		sector[(i ^ 1) + (27 * 2)] = (uint8_t)id_model[i];
	for (; i < 40; i++)
		sector[(i ^ 1) + (27 * 2)] = ' ';

	if (multiple_sector_max != 0)
		host_writew(sector + (47 * 2),
		            check_cast<uint16_t>(0x80 | multiple_sector_max)); /* <- READ/WRITE MULTIPLE MAX SECTORS */

	host_writew(sector + (48 * 2), 0x0000); /* :0  0=we do not support doubleword (32-bit) PIO */
	host_writew(sector + (49 * 2), 0x0A00); /* :13 0=Standby timer values managed by device */
	                                        /* :11 1=IORDY supported */
	                                        /* :10 0=IORDY not disabled */
	                                        /* :9  1=LBA supported */
	                                        /* :8  0=DMA not supported */
	host_writew(sector + (50 * 2), 0x4000); /* TBD: ??? */
	host_writew(sector + (51 * 2), 0x00F0); /* PIO data transfer cycle timing mode */
	host_writew(sector + (52 * 2), 0x00F0); /* DMA data transfer cycle timing mode */
	host_writew(sector + (53 * 2), 0x0007); /* :2  1=the fields in word 88 are valid */
	                                        /* :1  1=the fields in word (70:64) are valid */
	                                        /* :0  1= ??? */
	host_writew(sector + (54 * 2), check_cast<uint16_t>(cyls));  /* current cylinders */
	host_writew(sector + (55 * 2), check_cast<uint16_t>(heads)); /* current heads */
	host_writew(sector + (56 * 2), check_cast<uint16_t>(sects)); /* current sectors per track */
	host_writed(sector + (57 * 2), check_cast<uint16_t>(total)); /* current capacity in sectors */

	if (multiple_sector_count != 0)
		host_writew(sector + (59 * 2),
		            check_cast<uint16_t>(0x0100 | multiple_sector_count)); /* :8  multiple sector
		                                                                      setting is valid */
	/* 7:0 current setting for number of log. sectors per DRQ of READ/WRITE MULTIPLE */

	host_writed(sector + (60 * 2), check_cast<uint16_t>(ptotal)); /* total user addressable sectors (LBA) */
	host_writew(sector + (62 * 2), 0x0000);                       /* TBD: ??? */
	host_writew(sector + (63 * 2), 0x0000); /* :10 0=Multiword DMA mode 2 not selected */
	                                        /* TBD: Basically, we don't do DMA. Fill out this comment */
	host_writew(sector + (64 * 2), 0x0003); /* 7:0 PIO modes supported (TBD: ???) */
	host_writew(sector + (65 * 2), 0x0000); /* TBD: ??? */
	host_writew(sector + (66 * 2), 0x0000); /* TBD: ??? */
	host_writew(sector + (67 * 2), 0x0078); /* TBD: ??? */
	host_writew(sector + (68 * 2), 0x0078); /* TBD: ??? */
	host_writew(sector + (80 * 2), 0x007E); /* major version number. Here we say we support ATA-1 through ATA-8 */
	host_writew(sector + (81 * 2), 0x0022); /* minor version */
	host_writew(sector + (82 * 2), 0x4208); /* command set: NOP, DEVICE RESET[XXXXX], POWER MANAGEMENT */
	host_writew(sector + (83 * 2), 0x4000); /* command set: LBA48[XXXX] */
	host_writew(sector + (84 * 2), 0x4000); /* TBD: ??? */
	host_writew(sector + (85 * 2), 0x4208); /* commands in 82 enabled */
	host_writew(sector + (86 * 2), 0x4000); /* commands in 83 enabled */
	host_writew(sector + (87 * 2), 0x4000); /* TBD: ??? */
	host_writew(sector + (88 * 2), 0x0000); /* TBD: ??? */
	host_writew(sector + (93 * 3), 0x0000); /* TBD: ??? */

	/* ATA-8 integrity checksum */
	sector[510] = 0xA5;
	csum = 0;
	for (i = 0; i < 511; i++)
		csum += sector[i];
	sector[511] = 0 - csum;
}

IDEATADevice::IDEATADevice(IDEController *c, uint8_t disk_index)
        : IDEDevice(c, IDE_TYPE_HDD),
          bios_disk_index(disk_index)
{}

std::shared_ptr<imageDisk> IDEATADevice::getBIOSdisk()
{
	if (bios_disk_index >= (2 + MAX_HDD_IMAGES))
		return nullptr;
	return imageDiskList[bios_disk_index];
}

CDROM_Interface *IDEATAPICDROMDevice::getMSCDEXDrive()
{
	CDROM_Interface *cdrom = nullptr;

	if (!GetMSCDEXDrive(drive_index, &cdrom))
		return nullptr;

	return cdrom;
}

void IDEATAPICDROMDevice::update_from_cdrom()
{
	CDROM_Interface *cdrom = getMSCDEXDrive();
	if (cdrom == nullptr) {
		LOG_WARNING("IDE: IDE update from CD-ROM failed, disk not available");
		return;
	}
}

void IDEATADevice::update_from_biosdisk()
{
	const auto dsk = getBIOSdisk();
	if (dsk == nullptr) {
		LOG_WARNING("IDE: IDE update from BIOS disk failed, disk not available");
		return;
	}

	headshr = 0;
	geo_translate = false;
	cyls = dsk->cylinders;
	heads = dsk->heads;
	sects = dsk->sectors;

	/* One additional correction: The disk image is probably using BIOS-style geometry
	   translation (such as C/H/S 1024/64/63) which is impossible given that the IDE
	   standard only allows up to 16 heads. So we have to translate the geometry. */
	while (heads > 16 && (heads & 1) == 0) {
		cyls <<= 1U;
		heads >>= 1U;
		headshr++;
	}

	/* If we can't divide the heads down, then pick a LBA-like mapping that is good enough.
	 * Note that if what we pick does not evenly map to the INT 13h geometry, and the partition
	 * contained within is not an LBA type FAT16/FAT32 partition, then Windows 95's IDE driver
	 * will ignore this device and fall back to using INT 13h. For user convenience we will
	 * print a warning to reminder the user of exactly that. */
	if (heads > 16) {
		geo_translate = true;

		uint64_t tmp = heads;
		tmp *= cyls;
		tmp *= sects;

		sects = 63;
		heads = 16;
		cyls = static_cast<uint32_t>((tmp + ((63 * 16) - 1)) / (63 * 16));
		LOG_WARNING("IDE: Unable to reduce heads to 16 and below");
		LOG_MSG("    If at all possible, please consider using INT 13h geometry with a head");
		LOG_MSG("    count that is easier to map to the BIOS, like 240 heads or 128 heads/track.");
		LOG_MSG("    Some OSes, such as Windows 95, will not enable their 32-bit IDE driver if");
		LOG_MSG("    a clean mapping does not exist between IDE and BIOS geometry.");
		LOG_MSG("    Mapping BIOS DISK C/H/S %u/%u/%u as IDE %u/%u/%u (non-straightforward mapping)",
		        dsk->cylinders, dsk->heads, dsk->sectors, cyls, heads, sects);
	} else {
		LOG_MSG("IDE: Mapping BIOS DISK C/H/S %u/%u/%u as IDE %u/%u/%u", dsk->cylinders, dsk->heads,
		        dsk->sectors, cyls, heads, sects);
	}

	phys_heads = heads;
	phys_sects = sects;
	phys_cyls = cyls;
}

// Get an existing IDE controller, or create a new one if it doesn't exist
IDEController *get_or_create_controller(const int8_t i)
{
	// Hold the controllers for the lifetime of the program
	static std::vector<std::unique_ptr<IDEController>> ide_controllers = {};

	// Note that all checks are asserts because calls to this should be
	// programmatically managed (and not come from user data).

	// Is the requested controller out of bounds?
	assert(i >= 0 && i < MAX_IDE_CONTROLLERS);
	const auto index = static_cast<uint8_t>(i);

	// Does the requested controller already exist?
	if (idecontroller.at(index)) {
		return idecontroller[index];
	}

	// Create a new controller
	assert(idecontroller.at(index) == nullptr); // consistency check
	assert(index == ide_controllers.size());    // index should be the next
	                                            // available slot

	ide_controllers.emplace_back(std::make_unique<IDEController>(index, 0, 0, 0));
	assert(idecontroller.at(index) != nullptr); // consistency check
	return idecontroller[index];
}

void IDE_Get_Next_Cable_Slot(int8_t &index, bool &slave)
{
	index = -1;
	slave = false;
	for (int8_t i = 0; i < MAX_IDE_CONTROLLERS; ++i) {
		const auto c = get_or_create_controller(i);
		assert(c);

		// If both devices are populated, then the controller is already used (so we can't use it)
		if (c->device[0] && c->device[1]) {
			continue;
		}
		if (!c->device[0]) {
			slave = false;
			index = i;
			break;
		}
		if (!c->device[1]) {
			slave = true;
			index = i;
			break;
		}
	}
}

/* drive_index = drive letter 0...A to 25...Z */
void IDE_ATAPI_MediaChangeNotify(uint8_t requested_drive_index)
{
	for (uint32_t ide = 0; ide < MAX_IDE_CONTROLLERS; ide++) {
		IDEController *c = idecontroller[ide];
		if (c == nullptr)
			continue;
		for (uint32_t ms = 0; ms < 2; ms++) {
			IDEDevice *dev = c->device[ms];
			if (dev == nullptr)
				continue;
			if (dev->type == IDE_TYPE_CDROM) {
				auto atapi = (IDEATAPICDROMDevice *)dev;
				if (requested_drive_index == atapi->drive_index) {
					LOG_MSG("IDE: ATAPI acknowledge media change for drive %c",
					        requested_drive_index + 'A');
					atapi->has_changed = true;
					atapi->loading_mode = LOAD_INSERT_CD;
					PIC_RemoveSpecificEvents(IDE_ATAPI_SpinDown, c->interface_index);
					PIC_RemoveSpecificEvents(IDE_ATAPI_SpinUpComplete, c->interface_index);
					PIC_RemoveSpecificEvents(IDE_ATAPI_CDInsertion, c->interface_index);
					PIC_AddEvent(IDE_ATAPI_CDInsertion, atapi->cd_insertion_time /*ms*/,
					             c->interface_index);
				}
			}
		}
	}
}

/* drive_index = drive letter 0...A to 25...Z */
void IDE_CDROM_Attach(int8_t index, bool slave, const int8_t requested_drive_index)
{
	if (index < 0 || index >= MAX_IDE_CONTROLLERS)
		return;

	// Check if the requested drive index is valid
	assert(requested_drive_index >= 0 && requested_drive_index < DOS_DRIVES);
	const auto drive_index = static_cast<uint8_t>(requested_drive_index);

	auto c = get_or_create_controller(index);
	if (c == nullptr)
		return;

	if (c->device[slave ? 1 : 0] != nullptr) {
		LOG_WARNING("IDE: %s controller slot %s is already taken", get_controller_name(index), get_cable_slot_name(slave));
		return;
	}

	if (!GetMSCDEXDrive(drive_index, nullptr)) {
		LOG_WARNING("IDE: Asked to attach CD-ROM that does not exist");
		return;
	}

	auto dev = new IDEATAPICDROMDevice(c, drive_index);
	dev->update_from_cdrom();
	c->device[slave ? 1 : 0] = (IDEDevice *)dev;

    LOG_MSG("Attached ATAPI CD-ROM on %s IDE controller's %s cable slot",  get_controller_name(index), get_cable_slot_name(slave));
}

/* drive_index = drive letter 0...A to 25...Z */
void IDE_CDROM_Detach(const int8_t requested_drive_index)
{
	// Check if the requested drive index is valid
	assert(requested_drive_index >= 0 && requested_drive_index < DOS_DRIVES);
	const auto drive_index = static_cast<uint8_t>(requested_drive_index);

	for (uint8_t index = 0; index < MAX_IDE_CONTROLLERS; index++) {
		IDEController *c = idecontroller[index];
		if (c)
			for (int slave = 0; slave < 2; slave++) {
				auto dev = dynamic_cast<IDEATAPICDROMDevice *>(c->device[slave]);
				if (dev && dev->drive_index == drive_index) {
					delete dev;
					c->device[slave] = nullptr;
				}
			}
	}
}

void IDE_CDROM_Detach_Ret(int8_t &indexret,bool &slaveret,int8_t drive_index) {
    indexret = -1;
    for (uint8_t index = 0; index < MAX_IDE_CONTROLLERS; index++) {
        IDEController *c = idecontroller[index];
        if (c)
        for (int slave = 0; slave < 2; slave++) {
            auto dev = dynamic_cast<IDEATAPICDROMDevice*>(c->device[slave]);
            if (dev && dev->drive_index == drive_index) {
                delete dev;
                c->device[slave] = nullptr;
                slaveret = slave;
                indexret = check_cast<int8_t>(index);
            }
        }
    }
}

void IDE_CDROM_DetachAll()
{
	for (uint8_t index = 0; index < MAX_IDE_CONTROLLERS; index++) {
		IDEController *c = idecontroller[index];
		if (c)
			for (int slave = 0; slave < 2; slave++) {
				auto dev = dynamic_cast<IDEATAPICDROMDevice *>(c->device[slave]);
				if (dev) {
					delete dev;
					c->device[slave] = nullptr;
				}
			}
	}
}

/* bios_disk_index = index into BIOS INT 13h disk array: imageDisk *imageDiskList[MAX_DISK_IMAGES]; */
void IDE_Hard_Disk_Attach(int8_t index,
                          bool slave,
                          uint8_t bios_disk_index /*not INT13h, the index into DOSBox's BIOS drive emulation*/)
{
	IDEController *c;
	IDEATADevice *dev;

	if (index < 0 || index >= MAX_IDE_CONTROLLERS)
		return;
	c = idecontroller[static_cast<uint8_t>(index)];
	if (c == nullptr)
		return;

	if (c->device[slave ? 1 : 0] != nullptr) {
		LOG_WARNING("IDE: Controller %u %s already taken", index, slave ? "slave" : "master");
		return;
	}

	if (!imageDiskList.at(bios_disk_index)) {
		LOG_WARNING("IDE: Asked to attach bios disk that does not exist");
		return;
	}

	dev = new IDEATADevice(c, bios_disk_index);
	dev->update_from_biosdisk();
	c->device[slave ? 1 : 0] = (IDEDevice *)dev;
}

/* bios_disk_index = index into BIOS INT 13h disk array: imageDisk *imageDiskList[MAX_DISK_IMAGES]; */
void IDE_Hard_Disk_Detach(uint8_t bios_disk_index)
{
	for (uint8_t index = 0; index < MAX_IDE_CONTROLLERS; index++) {
		IDEController *c = idecontroller[index];
		if (c)
			for (int slave = 0; slave < 2; slave++) {
				auto dev = dynamic_cast<IDEATADevice *>(c->device[slave]);
				if (dev && dev->bios_disk_index == bios_disk_index) {
					delete dev;
					c->device[slave] = nullptr;
				}
			}
	}
}

std::string GetIDEPosition(uint8_t bios_disk_index)
{
	for (uint8_t index = 0; index < MAX_IDE_CONTROLLERS; index++) {
		IDEController *c = GetIDEController(index);
		if (c)
			for (int slave = 0; slave < 2; slave++) {
				auto dev = dynamic_cast<IDEATADevice *>(c->device[slave]);
				if (dev && dev->bios_disk_index == bios_disk_index) {
					return std::to_string(index + 1) + (slave ? 's' : 'm');
				}
			}
	}
	return "";
}

std::string GetIDEInfo()
{
	std::string info = {};
	for (uint8_t index = 0; index < MAX_IDE_CONTROLLERS; index++) {
		IDEController *c = GetIDEController(index);
		if (c)
			for (int slave = 0; slave < 2; slave++) {
				info += "IDE position " + std::to_string(index + 1) + (slave ? 's' : 'm') + ": ";
				if (dynamic_cast<IDEATADevice *>(c->device[slave]))
					info += "disk image";
				else if (dynamic_cast<IDEATAPICDROMDevice *>(c->device[slave]))
					info += "CD image";
				else
					info += "none";
			}
	}
	return info;
}

static IDEController *GetIDEController(uint32_t idx)
{
	if (idx >= MAX_IDE_CONTROLLERS)
		return nullptr;
	return idecontroller[idx];
}

static IDEDevice *GetIDESelectedDevice(IDEController *ide)
{
	if (ide == nullptr)
		return nullptr;
	return ide->device[ide->select];
}

static bool IDE_CPU_Is_Vm86()
{
	return (cpu.pmode && ((GETFLAG_IOPL < cpu.cpl) || GETFLAG(VM)));
}

static uint32_t IDE_SelfIO_In(IDEController * /* ide */, io_port_t port, io_width_t width)
{
	return ide_baseio_r(port, width);
}

static void IDE_SelfIO_Out(IDEController * /* ide */, io_port_t port, io_val_t val, io_width_t width)
{
	ide_baseio_w(port, val, width);
}

/* INT 13h extensions */
void IDE_EmuINT13DiskReadByBIOS_LBA(uint8_t disk, uint64_t lba)
{
	IDEController *ide;
	IDEDevice *dev;
	uint8_t idx, ms;

	if (disk < 0x80)
		return;
	if (lba >= (1ULL << 28ULL))
		return; /* this code does not support LBA48 */

	for (idx = 0; idx < MAX_IDE_CONTROLLERS; idx++) {
		ide = GetIDEController(idx);
		if (ide == nullptr)
			continue;
		if (!ide->int13fakeio && !ide->int13fakev86io)
			continue;

		/* TBD: Print a warning message if the IDE controller is busy (debug/warning message) */

		/* TBD: Force IDE state to readiness, abort command, etc. */

		/* for master/slave device... */
		for (ms = 0; ms < 2; ms++) {
			dev = ide->device[ms];
			if (dev == nullptr)
				continue;

			/* TBD: Print a warning message if the IDE device is busy or in the middle of a command */

			/* TBD: Forcibly device-reset the IDE device */

			/* Issue I/O to ourself to select drive */
			dev->faked_command = true;
			IDE_SelfIO_In(ide, ide->base_io + 7u, io_width_t::byte);
			IDE_SelfIO_Out(ide, ide->base_io + 6u, static_cast<uint16_t>(ms << 4u), io_width_t::byte);
			dev->faked_command = false;

			if (dev->type == IDE_TYPE_HDD) {
				auto ata = (IDEATADevice *)dev;
				//              static bool int13_fix_wrap_warned = false;
				bool vm86 = IDE_CPU_Is_Vm86();

				if ((ata->bios_disk_index - 2) == (disk - 0x80)) {
					//                  imageDisk *dsk = ata->getBIOSdisk();

					if (ide->int13fakev86io && vm86) {
						dev->faked_command = true;

						/* we MUST clear interrupts.
						 * leaving them enabled causes Win95 (or DOSBox?) to
						 * recursively pagefault and DOSBox to crash. In any case it
						 * seems Win95's IDE driver assumes the BIOS INT 13h code will
						 * do this since it's customary for the BIOS to do it at some
						 * point, usually just before reading the sector data. */
						CPU_CLI();

						/* We're in virtual 8086 mode and we're asked to fake I/O as if
						 * executing a BIOS subroutine. Some OS's like Windows 95 rely on
						 * executing INT 13h in virtual 8086 mode: on startup, the ESDI
						 * driver traps IDE ports and then executes INT 13h to watch what
						 * I/O ports it uses. It then uses that information to decide
						 * what IDE hard disk and controller corresponds to what DOS
						 * drive. So to get 32-bit disk access to work in Windows 95,
						 * we have to put on a good show to convince Windows 95 we're
						 * a legitimate BIOS INT 13h call doing it's job. */
						IDE_SelfIO_In(ide, ide->base_io + 7u,
						              io_width_t::byte); /* dum de dum reading status */
						IDE_SelfIO_Out(ide, ide->base_io + 6u,
						               static_cast<uint16_t>(ms << 4u) + 0xE0u +
						                       check_cast<uint32_t>(lba >> 24u),
						               io_width_t::byte); /* drive and head */
						IDE_SelfIO_In(ide, ide->base_io + 7u,
						              io_width_t::byte); /* dum de dum reading status */
						IDE_SelfIO_Out(ide, ide->base_io + 2u, 0x01,
						               io_width_t::byte); /* sector count */
						IDE_SelfIO_Out(ide, ide->base_io + 3u, lba & 0xFF,
						               io_width_t::byte); /* sector number */
						IDE_SelfIO_Out(ide, ide->base_io + 4u, (lba >> 8u) & 0xFF,
						               io_width_t::byte); /* cylinder lo */
						IDE_SelfIO_Out(ide, ide->base_io + 5u, (lba >> 16u) & 0xFF,
						               io_width_t::byte); /* cylinder hi */
						IDE_SelfIO_Out(ide, ide->base_io + 6u,
						               static_cast<uint16_t>(ms << 4u) + 0xE0 +
						                       check_cast<uint32_t>(lba >> 24u),
						               io_width_t::byte); /* drive and head */
						IDE_SelfIO_In(ide, ide->base_io + 7u,
						              io_width_t::byte); /* dum de dum reading status */
						IDE_SelfIO_Out(ide, ide->base_io + 7u, 0x20,
						               io_width_t::byte); /* issue READ */

						do {
							/* TBD: Timeout needed */
							uint32_t i = IDE_SelfIO_In(ide, ide->alt_io,
							                           io_width_t::byte);
							if ((i & 0x80) == 0)
								break;
						} while (1);
						IDE_SelfIO_In(ide, ide->base_io + 7u, io_width_t::byte);

						/* for brevity assume it worked. we're here to bullshit
						 * Windows 95 after all */
						for (uint32_t i = 0; i < 256; i++)
							IDE_SelfIO_In(ide, ide->base_io + 0u,
							              io_width_t::word); /* 16-bit IDE data read */

						/* one more */
						IDE_SelfIO_In(ide, ide->base_io + 7u,
						              io_width_t::byte); /* dum de dum reading status */

						/* assume IRQ happened and clear it */
						if (ide->IRQ >= 8)
							IDE_SelfIO_Out(ide, 0xA0, 0x60u + (uint32_t)ide->IRQ - 8u,
							               io_width_t::byte); /* specific EOI */
						else
							IDE_SelfIO_Out(ide, 0x20, 0x60u + (uint32_t)ide->IRQ,
							               io_width_t::byte); /* specific EOI */

						ata->abort_normal();
						dev->faked_command = false;
					} else {
						/* hack IDE state as if a BIOS executing IDE disk routines.
						 * This is required if we want IDE emulation to work with
						 * Windows 3.11 Windows for Workgroups 32-bit disk access
						 * (WDCTRL), because the driver "tests" the controller by
						 * issuing INT 13h calls then reading back IDE registers to
						 * see if they match the C/H/S it requested */
						dev->feature = 0x00; /* clear error (WDCTRL test phase 5/C/13) */
						dev->count = 0x00; /* clear sector count (WDCTRL test phase 6/D/14) */
						dev->lba[0] = lba & 0xFF; /* leave sector number the same
						                             (WDCTRL test phase 7/E/15) */
						dev->lba[1] = (lba >> 8u) & 0xFF;  /* leave cylinder the same
						                                      (WDCTRL test phase  8/F/16)  */
						dev->lba[2] = (lba >> 16u) & 0xFF; /* ...ditto */
						dev->drivehead = check_cast<uint8_t>(
						        0xE0u | static_cast<uint16_t>(ms << 4u) |
						        (lba >> 24u)); /* drive head and master/slave (WDCTRL
						                          test phase 9/10/17) */
						ide->drivehead = dev->drivehead;
						dev->status = IDE_STATUS_DRIVE_READY |
						              IDE_STATUS_DRIVE_SEEK_COMPLETE; /* status (WDCTRL
						                                                 test phase A/11/18) */
						dev->allow_writing = true;
						static bool vm86_warned = false;

						if (vm86 && !vm86_warned) {
							LOG_WARNING("IDE: INT 13h extensions read from virtual 8086 mode.");
							LOG_WARNING("     If using Windows 95 OSR2, please set int13fakev86io=true for proper 32-bit disk access");
							vm86_warned = true;
						}
					}

					/* break out, we're done */
					idx = MAX_IDE_CONTROLLERS;
					break;
				}
			}
		}
	}
}

/* this is called after INT 13h AH=0x02 READ DISK to change IDE state to simulate the BIOS in action.
 * this is needed for old "32-bit disk drivers" like WDCTRL in Windows 3.11 Windows for Workgroups,
 * which issues INT 13h to read-test and then reads IDE registers to see if they match expectations */
void IDE_EmuINT13DiskReadByBIOS(uint8_t disk, uint32_t cyl, uint32_t head, unsigned sect)
{
	IDEController *ide;
	IDEDevice *dev;
	uint8_t idx, ms;

	if (disk < 0x80)
		return;

	for (idx = 0; idx < MAX_IDE_CONTROLLERS; idx++) {
		ide = GetIDEController(idx);
		if (ide == nullptr)
			continue;
		if (!ide->int13fakeio && !ide->int13fakev86io)
			continue;

		/* TBD: Print a warning message if the IDE controller is busy (debug/warning message) */

		/* TBD: Force IDE state to readiness, abort command, etc. */

		/* for master/slave device... */
		for (ms = 0; ms < 2; ms++) {
			dev = ide->device[ms];
			if (dev == nullptr)
				continue;

			/* TBD: Print a warning message if the IDE device is busy or in the middle of a command */

			/* TBD: Forcibly device-reset the IDE device */

			/* Issue I/O to ourself to select drive */
			dev->faked_command = true;
			IDE_SelfIO_In(ide, ide->base_io + 7u, io_width_t::byte);
			IDE_SelfIO_Out(ide, ide->base_io + 6u, static_cast<uint16_t>(ms << 4u), io_width_t::byte);
			dev->faked_command = false;

			if (dev->type == IDE_TYPE_HDD) {
				auto ata  = (IDEATADevice *)dev;
				bool vm86 = IDE_CPU_Is_Vm86();

				if ((ata->bios_disk_index - 2) == (disk - 0x80)) {
					const auto dsk = ata->getBIOSdisk();

					/* print warning if INT 13h is being called after the OS changed
					 * logical geometry */
					if (ata->sects != ata->phys_sects || ata->heads != ata->phys_heads ||
					    ata->cyls != ata->phys_cyls)
						LOG_WARNING("IDE: INT 13h I/O issued on drive attached to IDE emulation with changed logical geometry!");

					/* HACK: src/ints/bios_disk.cpp implementation doesn't correctly
					 *       wrap sector numbers across tracks. it fullfills the read
					 *       by counting sectors and reading from C,H,S+i which means
					 *       that if the OS assumes the ability to read across track
					 *       boundaries (as Windows 95 does) we will get invalid
					 *       sector numbers, which in turn fouls up our emulation.
					 *
					 *       Windows 95 OSR2 for example, will happily ask for 63
					 *       sectors starting at C/H/S 30/9/42 without regard for
					 *       track boundaries. */
					if (dsk && sect > dsk->sectors) {
#if 0 /* this warning is pointless */
                        static bool int13_fix_wrap_warned = false;
                        if (!int13_fix_wrap_warned) {
                            LOG_WARNING("IDE: INT 13h implementation warning: we were given over-large sector number.");
                            LOG_WARNING("     This is normally the fault of DOSBox INT 13h emulation that counts sectors");
                            LOG_WARNING("     Without consideration of track boundaries");
                            int13_fix_wrap_warned = true;
                        }
#endif

						do {
							sect -= dsk->sectors;
							if ((++head) >= dsk->heads) {
								head -= dsk->heads;
								cyl++;
							}
						} while (sect > dsk->sectors);
					}

					/* translate BIOS INT 13h geometry to IDE geometry */
					if (ata->headshr != 0 || ata->geo_translate) {
						unsigned long lba;

						if (dsk == nullptr)
							return;
						lba = (head * dsk->sectors) +
						      (cyl * dsk->sectors * dsk->heads) + sect - 1;
						sect = (lba % ata->sects) + 1;
						head = (lba / ata->sects) % ata->heads;
						cyl = check_cast<uint32_t>(lba / ata->sects / ata->heads);
					}

					if (ide->int13fakev86io && vm86) {
						dev->faked_command = true;

						/* we MUST clear interrupts.
						 * leaving them enabled causes Win95 (or DOSBox?) to
						 * recursively pagefault and DOSBox to crash. In any case it
						 * seems Win95's IDE driver assumes the BIOS INT 13h code will
						 * do this since it's customary for the BIOS to do it at some
						 * point, usually just before reading the sector data. */
						CPU_CLI();

						/* We're in virtual 8086 mode and we're asked to fake I/O as if
						 * executing a BIOS subroutine. Some OS's like Windows 95 rely on
						 * executing INT 13h in virtual 8086 mode: on startup, the ESDI
						 * driver traps IDE ports and then executes INT 13h to watch what
						 * I/O ports it uses. It then uses that information to decide
						 * what IDE hard disk and controller corresponds to what DOS
						 * drive. So to get 32-bit disk access to work in Windows 95,
						 * we have to put on a good show to convince Windows 95 we're
						 * a legitimate BIOS INT 13h call doing it's job. */
						IDE_SelfIO_In(ide, ide->base_io + 7u,
						              io_width_t::byte); /* dum de dum reading status */
						IDE_SelfIO_Out(ide, ide->base_io + 6u,
						               static_cast<uint16_t>(ms << 4u) + 0xA0u + head,
						               io_width_t::byte); /* drive and head */
						IDE_SelfIO_In(ide, ide->base_io + 7u,
						              io_width_t::byte); /* dum de dum reading status */
						IDE_SelfIO_Out(ide, ide->base_io + 2u, 0x01,
						               io_width_t::byte); /* sector count */
						IDE_SelfIO_Out(ide, ide->base_io + 3u, sect,
						               io_width_t::byte); /* sector number */
						IDE_SelfIO_Out(ide, ide->base_io + 4u, cyl & 0xFF,
						               io_width_t::byte); /* cylinder lo */
						IDE_SelfIO_Out(ide, ide->base_io + 5u, (cyl >> 8u) & 0xFF,
						               io_width_t::byte); /* cylinder hi */
						IDE_SelfIO_Out(ide, ide->base_io + 6u,
						               static_cast<uint16_t>(ms << 4u) + 0xA0u + head,
						               io_width_t::byte); /* drive and head */
						IDE_SelfIO_In(ide, ide->base_io + 7u,
						              io_width_t::byte); /* dum de dum reading status */
						IDE_SelfIO_Out(ide, ide->base_io + 7u, 0x20u,
						               io_width_t::byte); /* issue READ */

						do {
							/* TBD: Timeout needed */
							uint32_t i = IDE_SelfIO_In(ide, ide->alt_io,
							                           io_width_t::byte);
							if ((i & 0x80) == 0)
								break;
						} while (1);
						IDE_SelfIO_In(ide, ide->base_io + 7u, io_width_t::byte);

						/* for brevity assume it worked. we're here to bullshit
						 * Windows 95 after all */
						for (uint32_t i = 0; i < 256; i++)
							IDE_SelfIO_In(ide, ide->base_io + 0, io_width_t::word); /* 16-bit IDE data read */

						/* one more */
						IDE_SelfIO_In(ide, ide->base_io + 7u,
						              io_width_t::byte); /* dum de dum reading status */

						/* assume IRQ happened and clear it */
						if (ide->IRQ >= 8)
							IDE_SelfIO_Out(ide, 0xA0, 0x60u + (uint32_t)ide->IRQ - 8u,
							               io_width_t::byte); /* specific EOI */
						else
							IDE_SelfIO_Out(ide, 0x20, 0x60u + (uint32_t)ide->IRQ,
							               io_width_t::byte); /* specific EOI */

						ata->abort_normal();
						dev->faked_command = false;
					} else {
						/* hack IDE state as if a BIOS executing IDE disk routines.
						 * This is required if we want IDE emulation to work with
						 * Windows 3.11 Windows for Workgroups 32-bit disk access
						 * (WDCTRL), because the driver "tests" the controller by
						 * issuing INT 13h calls then reading back IDE registers to
						 * see if they match the C/H/S it requested */
						dev->feature = 0x00; /* clear error (WDCTRL test phase 5/C/13) */
						dev->count = 0x00; /* clear sector count (WDCTRL test phase 6/D/14) */
						dev->lba[0] = check_cast<uint16_t>(sect); /* leave sector number
						                                             the same (WDCTRL
						                                             test phase 7/E/15) */
						dev->lba[1] = check_cast<uint16_t>(
						        cyl); /* leave cylinder the same (WDCTRL test phase 8/F/16) */
						dev->lba[2] = check_cast<uint16_t>(cyl >> 8u); /* ...ditto */
						dev->drivehead = check_cast<uint8_t>(
						        0xA0u | static_cast<uint16_t>(ms << 4u) |
						        head); /* drive head and master/slave (WDCTRL test phase 9/10/17) */
						ide->drivehead = dev->drivehead;
						dev->status = IDE_STATUS_DRIVE_READY |
						              IDE_STATUS_DRIVE_SEEK_COMPLETE; /* status (WDCTRL
						                                                 test phase A/11/18) */
						dev->allow_writing = true;
						static bool vm86_warned = false;

						if (vm86 && !vm86_warned) {
							LOG_WARNING("IDE: INT 13h read from virtual 8086 mode.");
							LOG_WARNING("     If using Windows 95, please set int13fakev86io=true for proper 32-bit disk access");
							vm86_warned = true;
						}
					}

					/* break out, we're done */
					idx = MAX_IDE_CONTROLLERS;
					break;
				}
			}
		}
	}
}

/* this is called by src/ints/bios_disk.cpp whenever INT 13h AH=0x00 is called on a hard disk.
 * this gives us a chance to update IDE state as if the BIOS had gone through with a full disk reset as requested. */
void IDE_ResetDiskByBIOS(uint8_t disk)
{
	IDEController *ide;
	IDEDevice *dev;
	uint8_t idx, ms;

	if (disk < 0x80)
		return;

	for (idx = 0; idx < MAX_IDE_CONTROLLERS; idx++) {
		ide = GetIDEController(idx);
		if (ide == nullptr)
			continue;
		if (!ide->int13fakeio && !ide->int13fakev86io)
			continue;

		/* TBD: Print a warning message if the IDE controller is busy (debug/warning message) */

		/* TBD: Force IDE state to readiness, abort command, etc. */

		/* for master/slave device... */
		for (ms = 0; ms < 2; ms++) {
			dev = ide->device[ms];
			if (dev == nullptr)
				continue;

			/* TBD: Print a warning message if the IDE device is busy or in the middle of a command */

			/* TBD: Forcibly device-reset the IDE device */

			/* Issue I/O to ourself to select drive */
			IDE_SelfIO_In(ide, ide->base_io + 7u, io_width_t::byte);
			IDE_SelfIO_Out(ide, ide->base_io + 6u, static_cast<uint16_t>(ms << 4u), io_width_t::byte);

			/* TBD: Forcibly device-reset the IDE device */

			if (dev->type == IDE_TYPE_HDD) {
				auto ata = (IDEATADevice *)dev;

				if ((ata->bios_disk_index - 2) == (disk - 0x80)) {
					LOG_MSG("IDE: %d%c reset by BIOS disk 0x%02x", (uint32_t)(idx + 1),
					        ms ? 's' : 'm', (uint32_t)disk);

					if (ide->int13fakev86io && IDE_CPU_Is_Vm86()) {
						/* issue the DEVICE RESET command */
						IDE_SelfIO_In(ide, ide->base_io + 7u, io_width_t::byte);
						IDE_SelfIO_Out(ide, ide->base_io + 7u, 0x08, io_width_t::byte);

						IDE_SelfIO_In(ide, ide->base_io + 7u, io_width_t::byte);

						/* assume IRQ happened and clear it */
						if (ide->IRQ >= 8)
							IDE_SelfIO_Out(ide, 0xA0, 0x60u + (uint32_t)ide->IRQ - 8u,
							               io_width_t::byte); /* specific EOI */
						else
							IDE_SelfIO_Out(ide, 0x20, 0x60u + (uint32_t)ide->IRQ,
							               io_width_t::byte); /* specific EOI */
					} else {
						/* Windows 3.1 WDCTRL needs this, or else, it will read the
						 * status register and see something other than
						 * DRIVE_READY|SEEK_COMPLETE */
						dev->writecommand(0x08);

						/* and then immediately clear the IRQ */
						ide->lower_irq();
					}
				}
			}
		}
	}
}

static void IDE_DelayedCommand(uint32_t idx /*which IDE controller*/)
{
	IDEDevice *dev = GetIDESelectedDevice(GetIDEController(idx));
	if (dev == nullptr)
		return;

	if (dev->type == IDE_TYPE_HDD) {
		auto ata = (IDEATADevice *)dev;
		uint32_t sectorn = 0; /* TBD: expand to uint64_t when adding LBA48 emulation */
		uint32_t sectcount;
		std::shared_ptr<imageDisk> disk = nullptr;
		//      int i;

		switch (dev->command) {
		case 0x30: /* WRITE SECTOR */
			disk = ata->getBIOSdisk();
			if (disk == nullptr) {
				LOG_WARNING("IDE: ATA READ fail, bios disk N/A");
				ata->abort_error();
				dev->controller->raise_irq();
				return;
			}

			// Sector count is unused, but retained as a comment
			// sectcount = ata->count & 0xFF;
			// if (sectcount == 0) sectcount = 256;
			if (drivehead_is_lba(ata->drivehead)) {
				/* LBA */
				sectorn = ((ata->drivehead & 0xFu) << 24u) | (uint32_t)ata->lba[0] |
				          ((uint32_t)ata->lba[1] << 8u) | ((uint32_t)ata->lba[2] << 16u);
			} else {
				/* C/H/S */
				if (ata->lba[0] == 0) {
					LOG_WARNING("IDE: ATA sector 0 does not exist");
					ata->abort_error();
					dev->controller->raise_irq();
					return;
				} else if ((ata->drivehead & 0xFu) >= ata->heads ||
				           (uint32_t)ata->lba[0] > ata->sects ||
				           (uint32_t)(ata->lba[1] | (ata->lba[2] << 8u)) >= ata->cyls) {
					LOG_WARNING("IDE: C/H/S %u/%u/%u out of bounds %u/%u/%u",
					        (uint32_t)(ata->lba[1] | (ata->lba[2] << 8u)),
					        (ata->drivehead & 0xFu), (uint32_t)(ata->lba[0]), ata->cyls,
					        ata->heads, ata->sects);
					ata->abort_error();
					dev->controller->raise_irq();
					return;
				}

				sectorn = ((ata->drivehead & 0xF) * ata->sects) +
				          (((uint32_t)ata->lba[1] | ((uint32_t)ata->lba[2] << 8u)) *
				           ata->sects * ata->heads) +
				          ((uint32_t)ata->lba[0] - 1u);
			}

			if (disk->Write_AbsoluteSector(sectorn, ata->sector) != 0) {
				LOG_WARNING("IDE: Failed to write sector");
				ata->abort_error();
				dev->controller->raise_irq();
				return;
			}

			/* NTS: the way this command works is that the drive writes ONE sector, then fires the IRQ
			        and lets the host read it, then reads another sector, fires the IRQ, etc. One
			    IRQ signal per sector. We emulate that here by adding another event to trigger this
			    call unless the sector count has just dwindled to zero, then we let it stop. */
			if ((ata->count & 0xFF) == 1) {
				/* end of the transfer */
				ata->count = 0;
				ata->status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
				dev->controller->raise_irq();
				ata->state = IDE_DEV_READY;
				ata->allow_writing = true;
				return;
			} else if ((ata->count & 0xFF) == 0)
				ata->count = 255;
			else
				ata->count--;
			ata->progress_count++;

			if (!ata->increment_current_address()) {
				LOG_WARNING("IDE: READ advance error");
				ata->abort_error();
				return;
			}

			/* begin another sector */
			dev->state = IDE_DEV_DATA_WRITE;
			dev->status = IDE_STATUS_DRQ | IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
			ata->prepare_write(0, 512);
			dev->controller->raise_irq();
			break;

		case 0x20: /* READ SECTOR */
			disk = ata->getBIOSdisk();
			if (disk == nullptr) {
				LOG_MSG("IDE: ATA READ fail, bios disk N/A");
				ata->abort_error();
				dev->controller->raise_irq();
				return;
			}
			// Sector count is unused, but retained as a comment
			// sectcount = ata->count & 0xFF;
			// if (sectcount == 0) sectcount = 256;
			if (drivehead_is_lba(ata->drivehead)) {
				/* LBA */
				sectorn = (((uint32_t)ata->drivehead & 0xFu) << 24u) | (uint32_t)ata->lba[0] |
				          ((uint32_t)ata->lba[1] << 8u) | ((uint32_t)ata->lba[2] << 16u);
			} else {
				/* C/H/S */
				if (ata->lba[0] == 0) {
					LOG_MSG("IDE: C/H/S access mode and sector==0");
					ata->abort_error();
					dev->controller->raise_irq();
					return;
				} else if ((uint32_t)(ata->drivehead & 0xF) >= ata->heads ||
				           (uint32_t)ata->lba[0] > ata->sects ||
				           (ata->lba[1] | ((uint32_t)ata->lba[2] << 8u)) >= ata->cyls) {
					LOG_WARNING("IDE: C/H/S %u/%u/%u out of bounds %u/%u/%u",
					        (ata->lba[1] | ((uint32_t)ata->lba[2] << 8u)),
					        (uint32_t)(ata->drivehead & 0xF), (uint32_t)ata->lba[0],
					        ata->cyls, ata->heads, ata->sects);
					ata->abort_error();
					dev->controller->raise_irq();
					return;
				}

				sectorn = ((ata->drivehead & 0xFu) * ata->sects) +
				          ((ata->lba[1] | ((uint32_t)ata->lba[2] << 8u)) * ata->sects * ata->heads) +
				          ((uint32_t)ata->lba[0] - 1u);
			}

			if (disk->Read_AbsoluteSector(sectorn, ata->sector) != 0) {
				LOG_WARNING("IDE: ATA read failed");
				ata->abort_error();
				dev->controller->raise_irq();
				return;
			}

			/* NTS: the way this command works is that the drive reads ONE sector, then fires the IRQ
			        and lets the host read it, then reads another sector, fires the IRQ, etc. One
			    IRQ signal per sector. We emulate that here by adding another event to trigger this
			    call unless the sector count has just dwindled to zero, then we let it stop. */
			/* NTS: The sector advance + count decrement is done in the I/O completion function */
			dev->state = IDE_DEV_DATA_READ;
			dev->status = IDE_STATUS_DRQ | IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
			ata->prepare_read(0, 512);
			dev->controller->raise_irq();
			break;

		case 0x40: /* READ SECTOR VERIFY WITH RETRY */
		case 0x41: /* READ SECTOR VERIFY WITHOUT RETRY */
			disk = ata->getBIOSdisk();
			if (disk == nullptr) {
				LOG_WARNING("IDE: ATA READ fail, bios disk N/A");
				ata->abort_error();
				dev->controller->raise_irq();
				return;
			}
			// Sector count is unused, but retained as a comment
			// sectcount = ata->count & 0xFF;
			// if (sectcount == 0) sectcount = 256;
			if (drivehead_is_lba(ata->drivehead)) {
				/* LBA */
				sectorn = (((uint32_t)ata->drivehead & 0xFu) << 24u) | (uint32_t)ata->lba[0] |
				          ((uint32_t)ata->lba[1] << 8u) | ((uint32_t)ata->lba[2] << 16u);
			} else {
				/* C/H/S */
				if (ata->lba[0] == 0) {
					LOG_WARNING("IDE: C/H/S access mode and sector==0");
					ata->abort_error();
					dev->controller->raise_irq();
					return;
				} else if ((uint32_t)(ata->drivehead & 0xF) >= ata->heads ||
				           (uint32_t)ata->lba[0] > ata->sects ||
				           (ata->lba[1] | ((uint32_t)ata->lba[2] << 8u)) >= ata->cyls) {
					LOG_WARNING("IDE: C/H/S %u/%u/%u out of bounds %u/%u/%u",
					        ata->lba[1] | ((uint32_t)ata->lba[2] << 8u), ata->drivehead & 0xFu,
					        (uint32_t)ata->lba[0], ata->cyls, ata->heads, ata->sects);
					ata->abort_error();
					dev->controller->raise_irq();
					return;
				}

				sectorn = (((uint32_t)ata->drivehead & 0xFu) * ata->sects) +
				          (((uint32_t)ata->lba[1] | ((uint32_t)ata->lba[2] << 8u)) *
				           ata->sects * ata->heads) +
				          ((uint32_t)ata->lba[0] - 1u);
			}

			if (disk->Read_AbsoluteSector(sectorn, ata->sector) != 0) {
				LOG_WARNING("IDE: ATA read failed");
				ata->abort_error();
				dev->controller->raise_irq();
				return;
			}

			if ((ata->count & 0xFF) == 1) {
				/* end of the transfer */
				ata->count = 0;
				ata->status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
				dev->controller->raise_irq();
				ata->state = IDE_DEV_READY;
				ata->allow_writing = true;
				return;
			} else if ((ata->count & 0xFF) == 0)
				ata->count = 255;
			else
				ata->count--;
			ata->progress_count++;

			if (!ata->increment_current_address()) {
				LOG_WARNING("IDE: READ advance error");
				ata->abort_error();
				return;
			}

			ata->state = IDE_DEV_BUSY;
			ata->status = IDE_STATUS_BUSY;
			PIC_AddEvent(IDE_DelayedCommand, 0.00001 /*ms*/, dev->controller->interface_index);
			break;

		case 0xC4: /* READ MULTIPLE */
			disk = ata->getBIOSdisk();
			if (disk == nullptr) {
				LOG_WARNING("IDE: ATA READ fail, bios disk N/A");
				ata->abort_error();
				dev->controller->raise_irq();
				return;
			}

			sectcount = ata->count & 0xFF;
			if (sectcount == 0)
				sectcount = 256;
			if (drivehead_is_lba(ata->drivehead)) {
				/* LBA */
				sectorn = (((uint32_t)ata->drivehead & 0xFu) << 24u) | (uint32_t)ata->lba[0] |
				          ((uint32_t)ata->lba[1] << 8u) | ((uint32_t)ata->lba[2] << 16u);
			} else {
				/* C/H/S */
				if (ata->lba[0] == 0) {
					LOG_WARNING("IDE: C/H/S access mode and sector==0");
					ata->abort_error();
					dev->controller->raise_irq();
					return;
				} else if ((uint32_t)(ata->drivehead & 0xF) >= ata->heads ||
				           (uint32_t)ata->lba[0] > ata->sects ||
				           (ata->lba[1] | ((uint32_t)ata->lba[2] << 8u)) >= ata->cyls) {
					LOG_WARNING("IDE: C/H/S %u/%u/%u out of bounds %u/%u/%u",
					        (ata->lba[1] | ((uint32_t)ata->lba[2] << 8u)),
					        (uint32_t)(ata->drivehead & 0xF), (uint32_t)ata->lba[0],
					        ata->cyls, ata->heads, ata->sects);
					ata->abort_error();
					dev->controller->raise_irq();
					return;
				}

				sectorn = ((ata->drivehead & 0xF) * ata->sects) +
				          (((uint32_t)ata->lba[1] | ((uint32_t)ata->lba[2] << 8u)) *
				           ata->sects * ata->heads) +
				          ((uint32_t)ata->lba[0] - 1);
			}

			if ((512 * ata->multiple_sector_count) > sizeof(ata->sector))
				E_Exit("SECTOR OVERFLOW");

			for (uint32_t cc = 0; cc < std::min(ata->multiple_sector_count, sectcount); cc++) {
				/* it would be great if the disk object had a "read
				 * multiple sectors" member function */
				if (disk->Read_AbsoluteSector(sectorn + cc, ata->sector + (cc * 512)) != 0) {
					LOG_WARNING("IDE: ATA read failed");
					ata->abort_error();
					dev->controller->raise_irq();
					return;
				}
			}

			/* NTS: the way this command works is that the drive reads ONE sector, then fires the IRQ
			        and lets the host read it, then reads another sector, fires the IRQ, etc. One
			    IRQ signal per sector. We emulate that here by adding another event to trigger this
			    call unless the sector count has just dwindled to zero, then we let it stop. */
			/* NTS: The sector advance + count decrement is done in the I/O completion function */
			dev->state = IDE_DEV_DATA_READ;
			dev->status = IDE_STATUS_DRQ | IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
			ata->prepare_read(0, 512 * std::min(ata->multiple_sector_count, sectcount));
			dev->controller->raise_irq();
			break;

		case 0xC5: /* WRITE MULTIPLE */
			disk = ata->getBIOSdisk();
			if (disk == nullptr) {
				LOG_WARNING("IDE: ATA READ fail, bios disk N/A");
				ata->abort_error();
				dev->controller->raise_irq();
				return;
			}

			sectcount = ata->count & 0xFF;
			if (sectcount == 0)
				sectcount = 256;
			if (drivehead_is_lba(ata->drivehead)) {
				/* LBA */
				sectorn = (((uint32_t)ata->drivehead & 0xF) << 24) | (uint32_t)ata->lba[0] |
				          ((uint32_t)ata->lba[1] << 8) | ((uint32_t)ata->lba[2] << 16);
			} else {
				/* C/H/S */
				if (ata->lba[0] == 0) {
					LOG_WARNING("IDE: ATA sector 0 does not exist");
					ata->abort_error();
					dev->controller->raise_irq();
					return;
				} else if ((uint32_t)(ata->drivehead & 0xF) >= ata->heads ||
				           (uint32_t)ata->lba[0] > ata->sects ||
				           (ata->lba[1] | ((uint32_t)ata->lba[2] << 8)) >= ata->cyls) {
					LOG_WARNING("IDE: C/H/S %u/%u/%u out of bounds %u/%u/%u",
					        (ata->lba[1] | ((uint32_t)ata->lba[2] << 8)),
					        (uint32_t)(ata->drivehead & 0xF), (uint32_t)ata->lba[0],
					        ata->cyls, ata->heads, ata->sects);
					ata->abort_error();
					dev->controller->raise_irq();
					return;
				}

				sectorn = ((uint32_t)(ata->drivehead & 0xF) * ata->sects) +
				          (((uint32_t)ata->lba[1] | ((uint32_t)ata->lba[2] << 8)) *
				           ata->sects * ata->heads) +
				          ((uint32_t)ata->lba[0] - 1);
			}

			for (uint32_t cc = 0; cc < std::min(ata->multiple_sector_count, sectcount); cc++) {
				/* it would be great if the disk object had a "write
				 * multiple sectors" member function */
				if (disk->Write_AbsoluteSector(sectorn + cc, ata->sector + (cc * 512)) != 0) {
					LOG_WARNING("IDE: Failed to write sector");
					ata->abort_error();
					dev->controller->raise_irq();
					return;
				}
			}

			for (uint32_t cc = 0; cc < std::min(ata->multiple_sector_count, sectcount); cc++) {
				if ((ata->count & 0xFF) == 1) {
					/* end of the transfer */
					ata->count = 0;
					ata->status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
					dev->controller->raise_irq();
					ata->state = IDE_DEV_READY;
					ata->allow_writing = true;
					return;
				} else if ((ata->count & 0xFF) == 0)
					ata->count = 255;
				else
					ata->count--;
				ata->progress_count++;

				if (!ata->increment_current_address()) {
					LOG_WARNING("IDE: READ advance error");
					ata->abort_error();
					return;
				}
			}

			/* begin another sector */
			sectcount = ata->count & 0xFF;
			if (sectcount == 0)
				sectcount = 256;
			dev->state = IDE_DEV_DATA_WRITE;
			dev->status = IDE_STATUS_DRQ | IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
			ata->prepare_write(0, 512 * std::min(ata->multiple_sector_count, sectcount));
			dev->controller->raise_irq();
			break;

		case 0xEC: /*IDENTIFY DEVICE (CONTINUED) */
			dev->state = IDE_DEV_DATA_READ;
			dev->status = IDE_STATUS_DRQ | IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
			ata->generate_identify_device();
			ata->prepare_read(0, 512);
			dev->count = 0x01;
			dev->lba[0] = 0x00;
			dev->feature = 0x00;
			dev->lba[1] = 0x00;
			dev->lba[2] = 0x00;
			dev->controller->raise_irq();
			break;
		default:
			LOG_WARNING("IDE: Unknown delayed IDE/ATA command");
			dev->abort_error();
			dev->controller->raise_irq();
			break;
		}
	} else if (dev->type == IDE_TYPE_CDROM) {
		auto atapi = (IDEATAPICDROMDevice *)dev;

		if (dev->state == IDE_DEV_ATAPI_BUSY) {
			switch (dev->command) {
			case 0xA0: /*ATAPI PACKET*/ atapi->on_atapi_busy_time(); break;
			default:
				LOG_WARNING("IDE: Unknown delayed IDE/ATAPI busy wait command");
				dev->abort_error();
				dev->controller->raise_irq();
				break;
			}
		} else {
			switch (dev->command) {
			case 0xA0: /*ATAPI PACKET*/
				dev->state = IDE_DEV_ATAPI_PACKET_COMMAND;
				dev->status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE |
				              IDE_STATUS_DRQ;
				dev->count = 0x01;           /* input/output == 0, command/data == 1 */
				atapi->atapi_cmd_total = 12; /* NTS: do NOT raise IRQ */
				atapi->atapi_cmd_i = 0;
				break;
			case 0xA1: /*IDENTIFY PACKET DEVICE (CONTINUED) */
				dev->state = IDE_DEV_DATA_READ;
				dev->status = IDE_STATUS_DRQ | IDE_STATUS_DRIVE_READY |
				              IDE_STATUS_DRIVE_SEEK_COMPLETE;
				atapi->generate_identify_device();
				atapi->prepare_read(0, 512);
				dev->controller->raise_irq();
				break;
			default:
				LOG_WARNING("IDE: Unknown delayed IDE/ATAPI command");
				dev->abort_error();
				dev->controller->raise_irq();
				break;
			}
		}
	} else {
		LOG_WARNING("IDE: delayed command");
		dev->abort_error();
		dev->controller->raise_irq();
	}
}

void IDEController::raise_irq()
{
	irq_pending = true;
	if (IRQ >= 0 && interrupt_enable)
		PIC_ActivateIRQ(check_cast<uint8_t>(IRQ));
}

void IDEController::lower_irq()
{
	irq_pending = false;
	if (IRQ >= 0)
		PIC_DeActivateIRQ(check_cast<uint8_t>(IRQ));
}

IDEController *match_ide_controller(io_port_t port)
{
	for (uint32_t i = 0; i < MAX_IDE_CONTROLLERS; i++) {
		IDEController *ide = idecontroller[i];
		if (ide == nullptr)
			continue;
		if (ide->base_io != 0U && ide->base_io == (port & 0xFFF8U))
			return ide;
		if (ide->alt_io != 0U && ide->alt_io == (port & 0xFFFEU))
			return ide;
	}

	return nullptr;
}

uint32_t IDEDevice::data_read(io_width_t)
{
	return 0xAAAAU;
}

void IDEDevice::data_write(io_val_t, io_width_t)
{}

/* IDE controller -> upon writing bit 2 of alt (0x3F6) */
void IDEDevice::host_reset_complete()
{
	status = 0x00;
	asleep = false;
	allow_writing = true;
	state = IDE_DEV_READY;
}

void IDEDevice::host_reset_begin()
{
	status = 0xFF;
	asleep = false;
	allow_writing = true;
	state = IDE_DEV_BUSY;
}

void IDEDevice::abort_silent()
{
	assert(controller != nullptr);

	/* a command was written while another is in progress */
	state = IDE_DEV_READY;
	allow_writing = true;
	command = 0x00;
	status = IDE_STATUS_ERROR | IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
}

void IDEDevice::abort_error()
{
	assert(controller != nullptr);
	LOG_WARNING("IDE: abort dh=0x%02x with error on 0x%03x", drivehead, controller->base_io);

	/* a command was written while another is in progress */
	state = IDE_DEV_READY;
	allow_writing = true;
	command = 0x00;
	status = IDE_STATUS_ERROR | IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
}

void IDEDevice::abort_normal()
{
	/* a command was written while another is in progress */
	state = IDE_DEV_READY;
	allow_writing = true;
	command = 0x00;
	status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
}

void IDEDevice::interface_wakeup()
{
	if (asleep) {
		asleep = false;
	}
}

bool IDEDevice::command_interruption_ok(uint8_t cmd)
{
	/* apparently this is OK, if the Linux kernel is doing it:
	 * writing the same command byte as the one in progress, OR, issuing
	 * Device Reset while another command is waiting for data read/write */
	if (cmd == command)
		return true;
	if (state != IDE_DEV_READY && state != IDE_DEV_BUSY && cmd == 0x08) {
		LOG_WARNING("IDE: Device reset while another (%02x) is in progress (state=%u). Aborting current command to begin another",
		        command, state);
		abort_silent();
		return true;
	}

	if (state != IDE_DEV_READY) {
		LOG_WARNING("IDE: Command %02x written while another (%02x) is in progress (state=%u). Aborting current command",
		        cmd, command, state);
		abort_error();
		return false;
	}

	return true;
}

void IDEDevice::writecommand(uint8_t cmd)
{
	if (!command_interruption_ok(cmd))
		return;

	/* if the drive is asleep, then writing a command wakes it up */
	interface_wakeup();

	/* drive is ready to accept command */
	LOG_WARNING("IDE: IDE command %02X", cmd);
	abort_error();
}

void IDEATAPICDROMDevice::writecommand(uint8_t cmd)
{
	if (!command_interruption_ok(cmd))
		return;

	/* if the drive is asleep, then writing a command wakes it up */
	interface_wakeup();

	/* drive is ready to accept command */
	allow_writing = false;
	command = cmd;
	switch (cmd) {
	case 0x08: /* DEVICE RESET */
		status = 0x00;
		drivehead &= 0x10;
		controller->drivehead = drivehead;
		count = 0x01;
		lba[0] = 0x01;
		feature = 0x01;
		lba[1] = 0x14; /* <- magic ATAPI identification */
		lba[2] = 0xEB;
		/* NTS: Testing suggests that ATAPI devices do NOT trigger an IRQ on receipt of this command */
		allow_writing = true;
		break;
	case 0xEC: /* IDENTIFY DEVICE */
		/* "devices that implement the PACKET command set shall post command aborted and place PACKET
		   command feature set in the appropriate fields". We have to do this. Unlike OAKCDROM.SYS
		   Windows 95 appears to autodetect IDE devices by what they do when they're sent command 0xEC
		   out of the blue---Microsoft didn't write their IDE drivers to use command 0x08 DEVICE
		   RESET. */
	case 0x20: /* READ SECTOR */
		abort_normal();
		status = IDE_STATUS_ERROR | IDE_STATUS_DRIVE_READY;
		drivehead &= 0x30;
		controller->drivehead = drivehead;
		count = 0x01;
		lba[0] = 0x01;
		feature = 0x04; /* abort */
		lba[1] = 0x14;  /* <- magic ATAPI identification */
		lba[2] = 0xEB;
		controller->raise_irq();
		allow_writing = true;
		break;
	case 0xA0: /* ATAPI PACKET */
		if (feature & 1) {
			/* this code does not support DMA packet commands */
			LOG_MSG("IDE: Attempted DMA transfer");
			abort_error();
			count = 0x03; /* no more data (command/data=1, input/output=1) */
			feature = 0xF4;
			controller->raise_irq();
		} else {
			state = IDE_DEV_BUSY;
			status = IDE_STATUS_BUSY;
			atapi_to_host = (feature >> 2) & 1; /* 0=to device 1=to host */
			host_maximum_byte_count = ((uint32_t)lba[2] << 8) +
			                          (uint32_t)lba[1]; /* LBA field bits 23:8 are byte count */
			if (host_maximum_byte_count == 0)
				host_maximum_byte_count = 0x10000UL;
			PIC_AddEvent(IDE_DelayedCommand, (faked_command ? 0.000001 : 0.25) /*ms*/,
			             controller->interface_index);
		}
		break;
	case 0xA1: /* IDENTIFY PACKET DEVICE */
		state = IDE_DEV_BUSY;
		status = IDE_STATUS_BUSY;
		PIC_AddEvent(IDE_DelayedCommand, (faked_command ? 0.000001 : ide_identify_command_delay),
		             controller->interface_index);
		break;
	default:
		LOG_WARNING("IDE: IDE/ATAPI command %02X", cmd);
		abort_error();
		allow_writing = true;
		count = 0x03; /* no more data (command/data=1, input/output=1) */
		feature = 0xF4;
		controller->raise_irq();
		break;
	}
}

void IDEATADevice::writecommand(uint8_t cmd)
{
	if (!command_interruption_ok(cmd))
		return;

	if (!faked_command) {
		if (drivehead_is_lba(drivehead)) {
			// unused
			// uint64_t n =
			// ((uint32_t)(drivehead&0xF)<<24)+((uint32_t)lba[2]<<16)+((uint32_t)lba[1]<<8)+(uint32_t)lba[0];
		}

		LOG(LOG_SB, LOG_NORMAL)("IDE: ATA command %02x", cmd);
	}

	/* if the drive is asleep, then writing a command wakes it up */
	interface_wakeup();

	/* TBD: OAKCDROM.SYS is sending the hard disk command 0xA0 (ATAPI packet) for some reason. Why? */

	/* drive is ready to accept command */
	allow_writing = false;
	command = cmd;
	switch (cmd) {
	case 0x00: /* NOP */
		feature = 0x04;
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_ERROR;
		controller->raise_irq();
		allow_writing = true;
		break;
	case 0x08: /* DEVICE RESET */
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
		drivehead &= 0x10;
		controller->drivehead = drivehead;
		count = 0x01;
		lba[0] = 0x01;
		feature = 0x00;
		lba[1] = lba[2] = 0;
		/* NTS: Testing suggests that ATA hard drives DO fire an IRQ at this stage.
		        In fact, Windows 95 won't detect hard drives that don't fire an IRQ in desponse */
		controller->raise_irq();
		allow_writing = true;
		break;
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17: /* RECALIBRATE (1xh) */
	case 0x18:
	case 0x19:
	case 0x1A:
	case 0x1B:
	case 0x1C:
	case 0x1D:
	case 0x1E:
	case 0x1F:
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
		/* "if the command is executed in CHS mode, then ... sector number register shall be 1.
		 *  if executed in LAB mode, then ... sector number register shall be 0" */
		if (drivehead_is_lba(drivehead))
			lba[0] = 0x00;
		else
			lba[0] = 0x01;
		drivehead &= 0x10;
		controller->drivehead = drivehead;
		lba[1] = lba[2] = 0;
		feature = 0x00;
		controller->raise_irq();
		allow_writing = true;
		break;
	case 0x30: /* WRITE SECTOR */
		/* the drive does NOT signal an interrupt. it sets DRQ and waits for a sector
		 * to be transferred to it before executing the command */
		progress_count = 0;
		state = IDE_DEV_DATA_WRITE;
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRQ;
		prepare_write(0, 512);
		break;
	case 0x20: /* READ SECTOR */
	case 0x40: /* READ SECTOR VERIFY WITH RETRY */
	case 0x41: /* READ SECTOR VERIFY WITHOUT RETRY */
	case 0xC4: /* READ MULTIPLE */
		progress_count = 0;
		state = IDE_DEV_BUSY;
		status = IDE_STATUS_BUSY;
		PIC_AddEvent(IDE_DelayedCommand, (faked_command ? 0.000001 : 0.1) /*ms*/,
		             controller->interface_index);
		break;
	case 0x91: /* INITIALIZE DEVICE PARAMETERS */
		if ((uint32_t)count != sects || (uint32_t)((drivehead & 0xF) + 1) != heads) {
			if (count == 0) {
				LOG_WARNING("IDE: OS attempted to change geometry to invalid H/S %u/%u",
				        count, (drivehead & 0xF) + 1);
				abort_error();
				allow_writing = true;
				return;
			} else {
				uint32_t ncyls;

				ncyls = (phys_cyls * phys_heads * phys_sects);
				ncyls += (count * ((uint32_t)(drivehead & 0xF) + 1u)) - 1u;
				ncyls /= count * ((uint32_t)(drivehead & 0xF) + 1u);

				/* the OS is changing logical disk geometry, so update our head/sector count
				 * (needed for Windows ME) */
				LOG_WARNING("IDE: OS is changing logical geometry from C/H/S %u/%u/%u to logical H/S %u/%u/%u",
				        (int)cyls, (int)heads, (int)sects, (int)ncyls, (drivehead & 0xF) + 1,
				        (int)count);
				LOG_WARNING("     Compatibility issues may occur if the OS tries to use INT 13 at the same time!");

				cyls = ncyls;
				sects = count;
				heads = (drivehead & 0xFu) + 1u;
			}
		}

		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
		allow_writing = true;
		break;
	case 0xC5: /* WRITE MULTIPLE */
		/* the drive does NOT signal an interrupt. it sets DRQ and waits for a sector
		 * to be transferred to it before executing the command */
		progress_count = 0;
		state = IDE_DEV_DATA_WRITE;
		status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRQ;
		prepare_write(0UL, 512UL * std::min(multiple_sector_count, (count == 0 ? 256u : count)));
		break;
	case 0xC6: /* SET MULTIPLE MODE */
		/* only sector counts 1, 2, 4, 8, 16, 32, 64, and 128 are legal by standard.
		 * NTS: There's a bug in VirtualBox that makes 0 legal too! */
		if (count != 0 && count <= multiple_sector_max && std::has_single_bit(count)) {
			multiple_sector_count = count;
			status = IDE_STATUS_DRIVE_READY | IDE_STATUS_DRIVE_SEEK_COMPLETE;
		} else {
			feature = 0x04; /* abort error */
			abort_error();
		}
		controller->raise_irq();
		allow_writing = true;
		break;
	case 0xA0: /*ATAPI PACKET*/
		   /* We're not an ATAPI packet device!
		    * Windows 95 seems to issue this at startup to hard drives. Duh. */
		   /* fall through */
	case 0xA1: /* IDENTIFY PACKET DEVICE */
		/* We are not an ATAPI packet device.
		 * Most MS-DOS drivers and Windows 95 like to issue both IDENTIFY ATA and IDENTIFY ATAPI
		 * commands. I also gather from some contributers on the github comments that people think our
		 * "Unknown IDE/ATA command" error message is part of some other error in the emulation.
		 * Rather than put up with that, we'll just silently abort the command with an error. */
		abort_normal();
		status = IDE_STATUS_ERROR | IDE_STATUS_DRIVE_READY;
		drivehead &= 0x30;
		controller->drivehead = drivehead;
		count = 0x01;
		lba[0] = 0x01;
		feature = 0x04; /* abort */
		lba[1] = 0x00;
		lba[2] = 0x00;
		controller->raise_irq();
		allow_writing = true;
		break;
	case 0xEC: /* IDENTIFY DEVICE */
		state = IDE_DEV_BUSY;
		status = IDE_STATUS_BUSY;
		PIC_AddEvent(IDE_DelayedCommand, (faked_command ? 0.000001 : ide_identify_command_delay),
		             controller->interface_index);
		break;
	default:
		LOG_WARNING("IDE: IDE/ATA command %02X", cmd);
		abort_error();
		allow_writing = true;
		controller->raise_irq();
		break;
	}
}

void IDEDevice::deselect()
{}

/* the hard disk or CD-ROM class override of this member is responsible for checking
   the head value and clamping within range if C/H/S mode is selected */
void IDEDevice::select(uint8_t ndh, bool switched_to)
{
	(void)switched_to; // UNUSED
	(void)ndh;         // UNUSED
	/* NTS: I thought there was some delay between selecting a drive and sending a command.
	    Apparently I was wrong. */
	if (allow_writing)
		drivehead = ndh;
	//  status = (!asleep)?(IDE_STATUS_DRIVE_READY|IDE_STATUS_DRIVE_SEEK_COMPLETE):0;
	//  allow_writing = !asleep;
	//  state = IDE_DEV_READY;
}

IDEController::IDEController(const uint8_t index,
                             const uint8_t irq,
                             const uint16_t port,
                             const uint16_t alt_port)
{
	constexpr int CONFIGS = 4;
	constexpr std::array<uint8_t, CONFIGS> irqs{{
	        14, /* primary */
	        15, /* secondary */
	        11, /* tertiary */
	        10, /* quaternary */
	}};
	constexpr std::array<uint16_t, CONFIGS> base_ios{{
	        0x1F0, /* primary */
	        0x170, /* secondary */
	        0x1E8, /* tertiary */
	        0x168, /* quaternary */
	}};
	constexpr std::array<uint16_t, CONFIGS> alt_ios{{
	        0x3F6, /* primary */
	        0x376, /* secondary */
	        0x3EE, /* tertiary */
	        0x36E, /* quaternary */
	}};

	assert(index < CONFIGS);
	interface_index = index;

	IRQ = contains(irqs, irq) ? irq : irqs[index];

	base_io = contains(base_ios, port) ? port : base_ios[index];

	alt_io = contains(alt_ios, alt_port) ? alt_port : alt_ios[index];

	LOG_MSG("IDE: Created %s controller IRQ %d, base I/O port %03xh, alternate I/O port %03xh",
	        get_controller_name(index), IRQ, base_io, alt_io);

	install_io_ports();
	PIC_SetIRQMask((uint32_t)IRQ, false);

	idecontroller[index] = this;
}

void IDEController::install_io_ports()
{
	if (base_io != 0) {
		for (io_port_t i = 0; i < 8; i++) {
			WriteHandler[i].Install(base_io + i, ide_baseio_w, io_width_t::dword);
			ReadHandler[i].Install(base_io + i, ide_baseio_r, io_width_t::dword);
		}
	}

	if (alt_io != 0) {
		WriteHandlerAlt[0].Install(alt_io, ide_altio_w, io_width_t::dword);
		ReadHandlerAlt[0].Install(alt_io, ide_altio_r, io_width_t::dword);

		WriteHandlerAlt[1].Install(alt_io + 1u, ide_altio_w, io_width_t::dword);
		ReadHandlerAlt[1].Install(alt_io + 1u, ide_altio_r, io_width_t::dword);
	}
}

void IDEController::uninstall_io_ports()
{
	// Uninstall the eight sets of base I/O ports
	assert(base_io != 0);
	for (auto & h : WriteHandler)
		h.Uninstall();
	for (auto & h : ReadHandler)
		h.Uninstall();

	// Uninstall the two sets of alternate I/O ports
	assert(alt_io != 0);
	for (auto & h : WriteHandlerAlt)
		h.Uninstall();
	for (auto & h : ReadHandlerAlt)
		h.Uninstall();
}

IDEController::~IDEController()
{
	lower_irq();
	uninstall_io_ports();

	for (auto &d : device) {
		delete d;
		d = nullptr;
	}
	idecontroller[interface_index] = nullptr;
}

static void ide_altio_w(io_port_t port, io_val_t val, io_width_t width)
{
	IDEController *ide = match_ide_controller(port);
	if (ide == nullptr) {
		LOG_WARNING("IDE: port read from I/O port not registered to IDE, yet callback triggered");
		return;
	}

	if (!ide->enable_pio32 && width == io_width_t::dword) {
		ide_altio_w(port, val & 0xFFFF, io_width_t::word);
		ide_altio_w(port + 2u, val >> 16u, io_width_t::word);
		return;
	} else if (ide->ignore_pio32 && width == io_width_t::dword)
		return;

	port &= 1;

	if (port == 0) { /*3F6*/
		ide->interrupt_enable = (val & 2u) ? 0 : 1;
		if (ide->interrupt_enable) {
			if (ide->irq_pending)
				ide->raise_irq();
		} else {
			if (ide->IRQ >= 0)
				PIC_DeActivateIRQ(check_cast<uint8_t>(ide->IRQ));
		}

		if ((val & 4) && !ide->host_reset) {
			if (ide->device[0])
				ide->device[0]->host_reset_begin();
			if (ide->device[1])
				ide->device[1]->host_reset_begin();
			ide->host_reset = 1;
		} else if (!(val & 4) && ide->host_reset) {
			if (ide->device[0])
				ide->device[0]->host_reset_complete();
			if (ide->device[1])
				ide->device[1]->host_reset_complete();
			ide->host_reset = 0;
		}
	}
}

static uint32_t ide_altio_r(io_port_t port, io_width_t width)
{
	IDEController *ide = match_ide_controller(port);
	IDEDevice *dev;

	if (ide == nullptr) {
		LOG_WARNING("IDE: port read from I/O port not registered to IDE, yet callback triggered");
		return UINT32_MAX;
	}

	if (!ide->enable_pio32 && width == io_width_t::dword)
		return ide_altio_r(port, io_width_t::word) + (ide_altio_r(port + 2u, io_width_t::word) << 16u);
	else if (ide->ignore_pio32 && width == io_width_t::dword)
		return UINT32_MAX;

	dev = ide->device[ide->select];

	port &= 1;

	if (port == 0) /*3F6(R) status, does NOT clear interrupt*/
		return (dev != nullptr) ? dev->status : ide->status;
	else /*3F7(R) Drive Address Register*/
		return 0x80u | (ide->select == 0 ? 0u : 1u) | (ide->select == 1 ? 0u : 2u) |
		       ((dev != nullptr) ? (((dev->drivehead & 0xFu) ^ 0xFu) << 2u) : 0x3Cu);

	return UINT32_MAX;
}

static uint32_t ide_baseio_r(io_port_t port, io_width_t width)
{
	IDEController *ide = match_ide_controller(port);
	IDEDevice *dev;
	uint32_t ret = UINT32_MAX;

	if (ide == nullptr) {
		LOG_WARNING("IDE: port read from I/O port not registered to IDE, yet callback triggered");
		return UINT32_MAX;
	}

	if (!ide->enable_pio32 && width == io_width_t::dword)
		return ide_baseio_r(port, io_width_t::word) + (ide_baseio_r(port + 2, io_width_t::word) << 16);
	else if (ide->ignore_pio32 && width == io_width_t::dword)
		return UINT32_MAX;

	dev = ide->device[ide->select];

	port &= 7;

	switch (port) {
	case 0: /* 1F0 */ ret = (dev != nullptr) ? dev->data_read(width) : 0xFFFFFFFFUL; break;
	case 1: /* 1F1 */ ret = (dev != nullptr) ? dev->feature : 0x00; break;
	case 2: /* 1F2 */ ret = (dev != nullptr) ? dev->count : 0x00; break;
	case 3: /* 1F3 */ ret = (dev != nullptr) ? dev->lba[0] : 0x00; break;
	case 4: /* 1F4 */ ret = (dev != nullptr) ? dev->lba[1] : 0x00; break;
	case 5: /* 1F5 */ ret = (dev != nullptr) ? dev->lba[2] : 0x00; break;
	case 6: /* 1F6 */ ret = ide->drivehead; break;
	case 7: /* 1F7 */
		/* if an IDE device exists at selection return it's status, else return our status */
		if (dev && dev->status & IDE_STATUS_BUSY) {
			// no-op
		} else if (dev == nullptr && ide->status & IDE_STATUS_BUSY) {
			// no-op
		} else {
			ide->lower_irq();
		}

		ret = (dev != nullptr) ? dev->status : ide->status;
		break;
	}

	return ret;
}

static void ide_baseio_w(io_port_t port, io_val_t val, io_width_t width)
{
	IDEController *ide = match_ide_controller(port);
	IDEDevice *dev;

	if (ide == nullptr) {
		LOG_WARNING("IDE: port read from I/O port not registered to IDE, yet callback triggered");
		return;
	}

	if (!ide->enable_pio32 && width == io_width_t::dword) {
		ide_baseio_w(port, val & 0xFFFF, io_width_t::word);
		ide_baseio_w(port + 2, val >> 16, io_width_t::word);
		return;
	} else if (ide->ignore_pio32 && width == io_width_t::dword)
		return;

	dev = ide->device[ide->select];

	port &= 7;

	/* ignore I/O writes if the controller is busy */
	if (dev) {
		if (dev->status & IDE_STATUS_BUSY) {
			if (port == 6 && ((val >> 4) & 1) == ide->select) {
				/* some MS-DOS drivers like ATAPICD.SYS are just very pedantic about writing
				 * to port +6 to ensure the right drive is selected */
				return;
			} else {
				LOG_WARNING("IDE: W-%03X %02X BUSY DROP [DEV]", port + ide->base_io, (int)val);
				return;
			}
		}
	} else if (ide->status & IDE_STATUS_BUSY) {
		if (port == 6 && ((val >> 4) & 1) == ide->select) {
			/* some MS-DOS drivers like ATAPICD.SYS are just very pedantic about writing to port
			 * +6 to ensure the right drive is selected */
			return;
		} else {
			LOG_WARNING("IDE: W-%03X %02X BUSY DROP [IDE]", port + ide->base_io, (int)val);
			return;
		}
	}

#if 0
    if (ide == idecontroller[1])
        LOG_MSG("IDE: baseio write port %u val %02x",(uint32_t)port,(uint32_t)val);
#endif

	if (port >= 1 && port <= 5 && dev && !dev->allow_writing) {
		LOG_WARNING("IDE: Write to port %u val %02x when device not ready to accept writing",
		        (uint32_t)port, val);
	}

	switch (port) {
	case 0: /* 1F0 */
		if (dev)
			dev->data_write(val, width); /* <- TBD: what about 32-bit PIO modes? */
		break;
	case 1:                                /* 1F1 */
		if (dev && dev->allow_writing) /* TBD: LBA48 16-bit wide register */
			dev->feature = check_cast<uint16_t>(val);
		break;
	case 2:                                /* 1F2 */
		if (dev && dev->allow_writing) /* TBD: LBA48 16-bit wide register */
			dev->count = check_cast<uint16_t>(val);
		break;
	case 3:                                /* 1F3 */
		if (dev && dev->allow_writing) /* TBD: LBA48 16-bit wide register */
			dev->lba[0] = check_cast<uint16_t>(val);
		break;
	case 4:                                /* 1F4 */
		if (dev && dev->allow_writing) /* TBD: LBA48 16-bit wide register */
			dev->lba[1] = check_cast<uint16_t>(val);
		break;
	case 5:                                /* 1F5 */
		if (dev && dev->allow_writing) /* TBD: LBA48 16-bit wide register */
			dev->lba[2] = check_cast<uint16_t>(val);
		break;
	case 6: /* 1F6 */
		if (((val >> 4) & 1) != ide->select) {
			ide->lower_irq();
			/* update select pointer if bit 4 changes.
			   also emulate IDE busy state when changing drives */
			if (dev)
				dev->deselect();
			ide->select = (val >> 4) & 1;
			dev = ide->device[ide->select];
			if (dev)
				dev->select(check_cast<uint8_t>(val), 1);
			else
				ide->status = 0; /* NTS: if there is no drive there you're supposed to not
				                    have anything set */
		} else if (dev) {
			dev->select(check_cast<uint8_t>(val), 0);
		} else {
			ide->status = 0; /* NTS: if there is no drive there you're supposed to not have anything set */
		}

		ide->drivehead = check_cast<uint8_t>(val);
		break;
	case 7: /* 1F7 */
		if (dev)
			dev->writecommand(check_cast<uint8_t>(val));
		break;
	}
}
