// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2022-2022 Jon Dennis
// SPDX-License-Identifier: GPL-2.0-or-later

//
// This file contains the reelmagic driver and device emulation code...
//
// This is where all the interaction with the "DOS world" occurs and is the
// implementation of the provided driver API notes.
//

#include "hardware/video/reelmagic/reelmagic.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stack>
#include <string>

#include "audio/channel_names.h"
#include "audio/mixer.h"
#include "config/setup.h"
#include "cpu/callback.h"
#include "cpu/registers.h"
#include "dos/dos.h"
#include "dos/dos_system.h"
#include "dos/programs.h"
#include "dos/programs/more_output.h"
#include "gui/mapper.h"
#include "utils/math_utils.h"

// note: Reported ReelMagic driver version 2.21 seems to be the most common...
static const uint8_t REELMAGIC_DRIVER_VERSION_MAJOR = 2;
static const uint8_t REELMAGIC_DRIVER_VERSION_MINOR = 21;

// note: the real deal usually sits at 260h... practically unused for now; XXX should this be
// configurable!?
[[maybe_unused]] static const uint16_t REELMAGIC_BASE_IO_PORT = 0x9800;

// practically unused for now; XXX should this be configurable!?
static const uint8_t REELMAGIC_IRQ = 11;

// the trailing \ is super important!
static const char REELMAGIC_FMPDRV_EXE_LOCATION[] = "Z:\\";

static callback_number_t _dosboxCallbackNumber = 0;
static uint8_t _installedInterruptNumber       = 0; // 0 means not currently installed
static bool _unloadAllowed                     = true;

// enable full API logging only when heavy debugging is on...
#if C_HEAVY_DEBUGGER
static bool _a204debug = true;
static bool _a206debug = true;
static inline bool IsDebugLogMessageFiltered(const uint8_t command, const uint16_t subfunc)
{
	if (command != 0x0A)
		return false;
	if ((subfunc == 0x204) && (!_a204debug))
		return true;
	if ((subfunc == 0x206) && (!_a206debug))
		return true;
	return false;
}
#	define APILOG LOG
#	define APILOG_DCFILT(COMMAND, SUBFUNC, ...) \
		if (!IsDebugLogMessageFiltered(COMMAND, SUBFUNC)) \
		APILOG(LOG_REELMAGIC, LOG_NORMAL)(__VA_ARGS__)
#else
static inline void APILOG_DONOTHING_FUNC(...) {}
#	define APILOG(A, B) APILOG_DONOTHING_FUNC
static inline void APILOG_DCFILT(...) {}
#endif

//
// driver -> user callback function stuff...
//
namespace {
struct UserCallbackCall {
	uint16_t command;
	uint16_t handle;
	uint16_t param1;
	uint16_t param2;

	// set to true if the next queued callback shall be auto-invoked when this one returns
	bool invokeNext;

	// typedef (void* postcbcb_t) (void*);

	// set to non-null to invoke function after callback returns
	// postcbcb_t postCallbackCallback;

	// void *     postCallbackCallbackUserPtr;

	UserCallbackCall() : command(0), handle(0), param1(0), param2(0), invokeNext(false) {}

	UserCallbackCall(const uint16_t command_, const uint16_t handle_ = 0, const uint16_t param1_ = 0,
	                 const uint16_t param2_ = 0, const bool invokeNext_ = false)
	        : command(command_),
	          handle(handle_),
	          param1(param1_),
	          param2(param2_),
	          invokeNext(invokeNext_)
	{}
};
struct UserCallbackPreservedState {
	struct Segments segs;
	struct CPU_Regs regs;
	UserCallbackPreservedState() : segs(Segs), regs(cpu_regs) {}
};
} // namespace
static std::stack<UserCallbackCall> _userCallbackStack;
static std::stack<UserCallbackPreservedState> _preservedUserCallbackStates;

// place to point the return address to after the user callback returns back to us
static RealPt _userCallbackReturnIp = 0;

// used to detect if we are returning from the user registered FMPDRV.EXE callback
static RealPt _userCallbackReturnDetectIp = 0;

// 0 = no callback registered
static RealPt _userCallbackFarPtr = 0;

// or rather calling convention
static Bitu _userCallbackType = 0;

// XXX currently duplicating this in realmagic_*.cpp files to avoid header pollution... TDB if this
// is a good idea...
namespace {
struct RMException : ::std::exception {
	std::string _msg = {};
	RMException(const char* fmt = "General ReelMagic Exception", ...)
	{
		va_list vl;
		va_start(vl, fmt);
		_msg.resize(vsnprintf(&_msg[0], 0, fmt, vl) + 1);
		va_end(vl);
		va_start(vl, fmt);
		vsnprintf(&_msg[0], _msg.size(), fmt, vl);
		va_end(vl);
		LOG(LOG_REELMAGIC, LOG_ERROR)("%s", _msg.c_str());
	}
	~RMException() noexcept override = default;
	const char* what() const noexcept override
	{
		return _msg.c_str();
	}
};
} // namespace

//
// the file I/O implementations of the "ReelMagic Media Player" begins here...
//
namespace {
class ReelMagic_MediaPlayerDOSFile : public ReelMagic_MediaPlayerFile {
	// this class gives the same "look and feel" to reelmagic programs...
	// as far as I can tell, "FMPDRV.EXE" also opens requested files into the current PSP...
	const std::string _fileName = {};
	const uint16_t _pspEntry    = 0;

	static uint16_t OpenDosFileEntry(const std::string& filename)
	{
		uint16_t rv;
		std::string dosfilepath = &filename[4]; // skip over the "DOS:" prefixed by the
		                                        // constructor
		const size_t last_slash = dosfilepath.find_last_of('/');
		if (last_slash != std::string::npos)
			dosfilepath = dosfilepath.substr(0, last_slash);
		if (!DOS_OpenFile(dosfilepath.c_str(), OPEN_READ, &rv))
			throw RMException("DOS File: Open for read failed: %s", filename.c_str());
		return rv;
	}

	static std::string strcpyFromDos(const uint16_t seg, const uint16_t ptr, const bool firstByteIsLen)
	{
		PhysPt dosptr = PhysicalMake(seg, ptr);
		std::string rv;
		rv.resize(firstByteIsLen ? ((size_t)mem_readb(dosptr++)) : 256);
		for (char* rv_ptr = &rv[0]; rv_ptr <= &rv[rv.size() - 1]; ++rv_ptr) {
			(*rv_ptr) = mem_readb(dosptr++);
			if ((*rv_ptr) == '\0') {
				rv.resize(rv_ptr - &rv[0]);
				break;
			}
		}
		return rv;
	}

protected:
	const char* GetFileName() const override
	{
		return _fileName.c_str();
	}

	uint32_t GetFileSize() const override
	{
		uint32_t currentPos = 0;
		if (!DOS_SeekFile(_pspEntry, &currentPos, DOS_SEEK_CUR))
			throw RMException("DOS File: Seek failed: Get current position");
		uint32_t result = 0;
		if (!DOS_SeekFile(_pspEntry, &result, DOS_SEEK_END))
			throw RMException("DOS File: Seek failed: Get current position");
		if (!DOS_SeekFile(_pspEntry, &currentPos, DOS_SEEK_SET))
			throw RMException("DOS File: Seek failed: Reset current position");
		return result;
	}

	uint32_t Read(uint8_t* data, uint32_t amount) override
	{
		uint32_t bytesRead = 0;
		uint16_t transactionAmount;
		while (amount > 0) {
			transactionAmount = (amount > 0xFFFF) ? 0xFFFF : (uint16_t)amount;
			if (!DOS_ReadFile(_pspEntry, data, &transactionAmount))
				throw RMException("DOS File: Read failed");
			if (transactionAmount == 0)
				break;
			data += transactionAmount;
			bytesRead += transactionAmount;
			amount -= transactionAmount;
		}
		return bytesRead;
	}

	void Seek(uint32_t pos, uint32_t type) override
	{
		if (!DOS_SeekFile(_pspEntry, &pos, type))
			throw RMException("DOS File: Seek failed.");
	}

public:
	ReelMagic_MediaPlayerDOSFile(const char* const dosFilepath)
	        : _fileName(std::string("DOS:") + dosFilepath),
	          _pspEntry(OpenDosFileEntry(dosFilepath))
	{}
	ReelMagic_MediaPlayerDOSFile(const uint16_t filenameStrSeg, const uint16_t filenameStrPtr,
	                             const bool firstByteIsLen = false)
	        : _fileName(std::string("DOS:") +
	                    strcpyFromDos(filenameStrSeg, filenameStrPtr, firstByteIsLen)),
	          _pspEntry(OpenDosFileEntry(_fileName))
	{}
	~ReelMagic_MediaPlayerDOSFile() override
	{
		DOS_CloseFile(_pspEntry);
	}
};

class ReelMagic_MediaPlayerHostFile : public ReelMagic_MediaPlayerFile {
	// this class is really only useful for debugging shit...
	FILE* _fp             = nullptr;
	std::string _fileName = {};
	uint32_t _fileSize    = 0;

	static uint32_t GetFileSize(FILE* const fp)
	{
		assert(fp); // we always expect a non-void pointer
		if (fseek(fp, 0, SEEK_END) == -1)
			throw RMException("Host File: fseek() failed: %s", strerror(errno));
		const long tell_result = ftell(fp);
		if (tell_result == -1)
			throw RMException("Host File: ftell() failed: %s", strerror(errno));
		if (fseek(fp, 0, SEEK_SET) == -1)
			throw RMException("Host File: fseek() failed: %s", strerror(errno));
		return (uint32_t)tell_result;
	}

protected:
	const char* GetFileName() const override
	{
		return _fileName.c_str();
	}

	uint32_t GetFileSize() const override
	{
		return _fileSize;
	}

	uint32_t Read(uint8_t* data, uint32_t amount) override
	{
		const size_t fread_result = fread(data, 1, amount, _fp);
		if ((fread_result == 0) && ferror(_fp))
			throw RMException("Host File: fread() failed: %s", strerror(errno));
		return (uint32_t)fread_result;
	}

	void Seek(uint32_t pos, uint32_t type) override
	{
		if (fseek(_fp, (long)pos, (type == DOS_SEEK_SET) ? SEEK_SET : SEEK_CUR) == -1)
			throw RMException("Host File: fseek() failed: %s", strerror(errno));
	}

public:
	ReelMagic_MediaPlayerHostFile(ReelMagic_MediaPlayerHostFile&)                  = delete;
	ReelMagic_MediaPlayerHostFile& operator=(const ReelMagic_MediaPlayerHostFile&) = delete;

	ReelMagic_MediaPlayerHostFile(const char* const hostFilepath)
	{
		assert(hostFilepath);
		_fileName = std::string("HOST:") + hostFilepath;

		_fp = fopen(hostFilepath, "rb");
		if (!_fp) {
			throw RMException("Host File: fopen(\"%s\")failed: %s",
			                  hostFilepath,
			                  strerror(errno));
		}

		// Only get the size if we've got a valid file pointer
		_fileSize = GetFileSize(_fp);
	}
	~ReelMagic_MediaPlayerHostFile() override
	{
		fclose(_fp);
	}
};
} // namespace

//
// the implementation of "FMPDRV.EXE" begins here...
//
static uint8_t findFreeInt(void)
{
	//"FMPDRV.EXE" installs itself into a free IVT slot starting at 0x80...
	for (uint8_t intNum = 0x80; intNum; ++intNum) {
		if (RealGetVec(intNum) == 0)
			return intNum;
	}
	return 0x00; // failed
}

/*
  ok so this is some shit...
  detection of the reelmagic "FMPDRV.EXE" driver TSR presence works as follows:
    for (int_num = 0x80; int_num < 0x100; ++int_num) {
      ivt_func_t ivt_callback_ptr = cpu_global_ivt[int_num];
      if (ivt_callback_ptr == NULL) continue;
      const char * str = ivt_callback_ptr; //yup you read that correctly,
                                           //we are casting a function pointer to a string...
      if (strcmp(&str[3], "FMPDriver") == 0) {
        return int_num; //we have found the FMPDriver at INT int_num
    }
*/
static Bitu FMPDRV_INTHandler(void);
static bool FMPDRV_InstallINTHandler()
{
	if (_installedInterruptNumber != 0)
		return true; // already installed
	_installedInterruptNumber = findFreeInt();
	if (_installedInterruptNumber == 0) {
		LOG(LOG_REELMAGIC, LOG_ERROR)
		("Unable to install INT handler due to no free IVT slots!");
		return false; // hard to beleive this could actually happen... but need to account
		              // for...
	}


	// Taking the upper 8 bits if the callback number is always zero because
	// the maximum callback number is only 128. So we just confirm that here.
	static_assert(sizeof(_dosboxCallbackNumber) < sizeof(uint16_t));
	constexpr uint8_t upper_8_bits_of_callback = 0;

	// This is the contents of the "FMPDRV.EXE" INT handler which will be put in
	// the ROM region (Note: this is derrived from "case CB_IRET:" in
	// "src/cpu/callback.cpp"):
	const uint8_t isr_impl[] = {0xEB,
	                            0x1A, // JMP over the check strings like a champ...
	                            9,    // 9 bytes for "FMPDriver" check string
	                            'F',
	                            'M',
	                            'P',
	                            'D',
	                            'r',
	                            'i',
	                            'v',
	                            'e',
	                            'r',
	                            '\0',
	                            13, // 13 bytes for "ReelMagic(TM)" check string
	                            'R',
	                            'e',
	                            'e',
	                            'l',
	                            'M',
	                            'a',
	                            'g',
	                            'i',
	                            'c',
	                            '(',
	                            'T',
	                            'M',
	                            ')',
	                            '\0',
	                            0xFE,
	                            0x38, // GRP 4 + Extra Callback Instruction
	                            _dosboxCallbackNumber,
	                            upper_8_bits_of_callback,
	                            0xCF, // IRET

	                            // Extra "unreachable" callback instruction used
	                            // to signal end of FMPDRV.EXE registered callback
	                            // when invoking the "user callback" from this
	                            // driver.
	                            0xFE,
	                            0x38, // GRP 4 + Extra Callback Instruction
	                            _dosboxCallbackNumber,
	                            upper_8_bits_of_callback};
	// Note: checking against double CB_SIZE. This is because we allocate
	// two callbacks to make this fit within the "callback ROM" region. See
	// comment in REELMAGIC_Init() function below
	if (sizeof(isr_impl) > (CB_SIZE * 2)) {
		E_Exit("CB_SIZE too small to fit ReelMagic driver IVT code. This means that DOSBox was not compiled correctly!");
	}

	CALLBACK_Setup(_dosboxCallbackNumber,
	               &FMPDRV_INTHandler,
	               CB_IRET,
	               "ReelMagic"); // must happen BEFORE we copy to ROM region!

	// XXX is there an existing function for this?
	for (PhysPt i = 0; i < static_cast<PhysPt>(sizeof(isr_impl)); ++i)
		phys_writeb(CALLBACK_PhysPointer(_dosboxCallbackNumber) + i, isr_impl[i]);

	_userCallbackReturnDetectIp = CALLBACK_RealPointer(_dosboxCallbackNumber) + sizeof(isr_impl);
	_userCallbackReturnIp = _userCallbackReturnDetectIp - 4;

	RealSetVec(_installedInterruptNumber, CALLBACK_RealPointer(_dosboxCallbackNumber));
	LOG(LOG_REELMAGIC, LOG_NORMAL)
	("Successfully installed FMPDRV.EXE at INT %xh", _installedInterruptNumber);
	ReelMagic_SetVideoMixerEnabled(true);
	return true; // success
}

static void FMPDRV_UninstallINTHandler()
{
	if (_installedInterruptNumber == 0)
		return; // already uninstalled...
	if (!_unloadAllowed)
		return;
	LOG(LOG_REELMAGIC, LOG_NORMAL)
	("Uninstalling FMPDRV.EXE from INT %xh", _installedInterruptNumber);
	ReelMagic_SetVideoMixerEnabled(false);
	RealSetVec(_installedInterruptNumber, 0);
	_installedInterruptNumber = 0;
	_userCallbackFarPtr       = 0;
}

//
// functions to serialize the player state into the required API format
//
static uint16_t GetFileStateValue(const ReelMagic_MediaPlayer& player)
{
	uint16_t value = 0;
	if (player.HasVideo())
		value |= 2;
	if (player.HasAudio())
		value |= 1;
	return value;
}

static uint16_t GetPlayStateValue(const ReelMagic_MediaPlayer& player)
{
	// XXX status code 1 = paused
	// XXX status code 2 = stopped (e.g. never started with function 3)
	uint16_t value = player.IsPlaying() ? 0x4 : 0x1;
	if ((_userCallbackType == 0x2000) && player.IsPlaying())
		value |= 0x10; // XXX hack for RTZ!!!
	return value;
}

static uint16_t GetPlayerSurfaceZOrderValue(const ReelMagic_PlayerConfiguration& cfg)
{
	if (!cfg.VideoOutputVisible)
		return 1;
	if (cfg.UnderVga)
		return 4;
	return 2;
}

//
// This is to invoke the user program driver callback if registered...
//
static void EnqueueTopUserCallbackOnCPUResume()
{
	if (_userCallbackStack.empty())
		E_Exit("FMPDRV.EXE Asking to enqueue a callback with nothing on the top of the callback stack!");
	if (_userCallbackFarPtr == 0)
		E_Exit("FMPDRV.EXE Asking to enqueue a callback with no user callback pointer set!");
	UserCallbackCall& ucc = _userCallbackStack.top();

	// snapshot the current state...
	_preservedUserCallbackStates.emplace(UserCallbackPreservedState());

	// prepare the function call...
	switch (_userCallbackType) { // AFAIK, _userCallbackType dictates the calling convention...
		                     // this is the value that was passed in when registering the
		                     // callback function to us

	case 0x2000:                                   // RTZ-style; shit is passed on the stack...
		reg_ax = reg_bx = reg_cx = reg_dx = 0; // clear the GP regs for good measure...
		mem_writew(PhysicalMake(SegValue(ss), reg_sp -= 2), ucc.param2);
		mem_writew(PhysicalMake(SegValue(ss), reg_sp -= 2), ucc.param1);
		mem_writew(PhysicalMake(SegValue(ss), reg_sp -= 2), ucc.handle);
		mem_writew(PhysicalMake(SegValue(ss), reg_sp -= 2), ucc.command);
		break;

	default:
		LOG(LOG_REELMAGIC, LOG_WARN)
		("Unknown user callback type %04Xh. Defaulting to 0000. This is probably gonna screw something up!",
		 (unsigned)_userCallbackType);
		[[fallthrough]];

	case 0x0000: // The Horde style... shit is passed in registers...
		reg_bx = ((ucc.command << 8) & 0xFF00) | (ucc.handle & 0xFF);
		reg_ax = ucc.param1;
		reg_dx = ucc.param2;
		reg_cx = 0; //???
		break;
	}

	// push the far-call return address...

	// return address to invoke CleanupFromUserCallback()
	mem_writew(PhysicalMake(SegValue(ss), reg_sp -= 2), RealSegment(_userCallbackReturnIp));

	// return address to invoke CleanupFromUserCallback()
	mem_writew(PhysicalMake(SegValue(ss), reg_sp -= 2), RealOffset(_userCallbackReturnIp));

	// then we blast off into the wild blue...
	SegSet16(cs, RealSegment(_userCallbackFarPtr));
	reg_ip = RealOffset(_userCallbackFarPtr);

	APILOG(LOG_REELMAGIC, LOG_NORMAL)
	("Post-Invoking registered user-callback on CPU resume. cmd=%04Xh handle=%04Xh p1=%04Xh p2=%04Xh",
	 (unsigned)ucc.command,
	 (unsigned)ucc.handle,
	 (unsigned)ucc.param1,
	 (unsigned)ucc.param2);
}

static void InvokePlayerStateChangeCallbackOnCPUResumeIfRegistered(const bool isPausing,
                                                                   ReelMagic_MediaPlayer& player)
{
	if (_userCallbackFarPtr == 0)
		return; // no callback registered...

	const auto cbstackStartSize             = _userCallbackStack.size();
	const ReelMagic_PlayerAttributes& attrs = player.GetAttrs();

	if ((_userCallbackType == 0x2000) && (!isPausing)) {
		// hack to make RTZ work for now...
		_userCallbackStack.push(UserCallbackCall(5,
		                                         attrs.Handles.Base,
		                                         0,
		                                         0,
		                                         _userCallbackStack.size() !=
		                                                 cbstackStartSize));
	}

	if (isPausing) {
		// we are being invoked from a pause command

		// is this the correct "last" handle !?
		if (attrs.Handles.Demux)
			_userCallbackStack.push(
			        UserCallbackCall(7,
			                         attrs.Handles.Demux,
			                         GetPlayStateValue(player),
			                         0,
			                         _userCallbackStack.size() != cbstackStartSize));

		// is this the correct "middle" handle !?
		if (attrs.Handles.Video)
			_userCallbackStack.push(
			        UserCallbackCall(7,
			                         attrs.Handles.Video,
			                         GetPlayStateValue(player),
			                         0,
			                         _userCallbackStack.size() != cbstackStartSize));

		// on the real deal, highest handle always calls back first! I'm assuming its audio!
		if (attrs.Handles.Audio)
			_userCallbackStack.push(
			        UserCallbackCall(7,
			                         attrs.Handles.Audio,
			                         GetPlayStateValue(player),
			                         0,
			                         _userCallbackStack.size() != cbstackStartSize));
	} else {
		// we are being invoked from a close command

		// is this the correct "last" handle !?
		if (player.IsPlaying() && attrs.Handles.Demux)
			_userCallbackStack.push(
			        UserCallbackCall(7,
			                         attrs.Handles.Demux,
			                         GetPlayStateValue(player),
			                         0,
			                         _userCallbackStack.size() != cbstackStartSize));
		// 4 = state of player; since we only get called on pause, this will always be 4

		if (attrs.Handles.Audio)
			_userCallbackStack.push(
			        UserCallbackCall(7,
			                         attrs.Handles.Audio,
			                         GetPlayStateValue(player),
			                         0,
			                         _userCallbackStack.size() != cbstackStartSize));
		// 4 = state of player; since we only get called on pause, this will always be 4

		if (attrs.Handles.Video)
			_userCallbackStack.push(
			        UserCallbackCall(7,
			                         attrs.Handles.Video,
			                         GetPlayStateValue(player),
			                         0,
			                         _userCallbackStack.size() != cbstackStartSize));
		// 4 = state of player; since we only get called on pause, this will always be 4
	}

	if (_userCallbackStack.size() != cbstackStartSize)
		EnqueueTopUserCallbackOnCPUResume();
}

static void CleanupFromUserCallback(void)
{
	if (_preservedUserCallbackStates.empty())
		E_Exit("FMPDRV.EXE Asking to cleanup with nothing on preservation stack");
	if (_userCallbackStack.empty())
		E_Exit("FMPDRV.EXE Asking to cleanup with nothing on user callback stack");
	APILOG(LOG_REELMAGIC, LOG_NORMAL)("Returning from driver_callback()");

	// restore the previous state of things...
	const UserCallbackCall& ucc = _userCallbackStack.top();
	const bool invokeNext       = ucc.invokeNext;
	_userCallbackStack.pop();

	const UserCallbackPreservedState& s = _preservedUserCallbackStates.top();

	Segs     = s.segs;
	cpu_regs = s.regs;
	_preservedUserCallbackStates.pop();

	if (invokeNext) {
		APILOG(LOG_REELMAGIC, LOG_NORMAL)("Invoking Next Chained Callback...");
		EnqueueTopUserCallbackOnCPUResume();
	}
}

static uint32_t FMPDRV_driver_call(const uint8_t command, const uint8_t media_handle,
                                   const uint16_t subfunc, const uint16_t param1, const uint16_t param2)
{
	ReelMagic_MediaPlayer* player = nullptr;
	ReelMagic_PlayerConfiguration* cfg = nullptr;
	uint32_t rv;
	switch (command) {
	//
	// Open Media Handle (File)
	//
	case 0x01:
		if (media_handle != 0)
			LOG(LOG_REELMAGIC, LOG_WARN)("Non-zero media handle on open command");
		if (((subfunc & 0xEFFF) != 1) && (subfunc != 2))
			LOG(LOG_REELMAGIC, LOG_WARN)("subfunc not 1 or 2 on open command");
		// if subfunc (or rather flags) has the 0x1000 bit set, then the first byte of the
		// caller's pointer is the file path string length
		rv = ReelMagic_NewPlayer(
		        new ReelMagic_MediaPlayerDOSFile(param2, param1, (subfunc & 0x1000) != 0));

		return rv;

	//
	// Close Media Handle
	//
	case 0x02:
		player = &ReelMagic_HandleToMediaPlayer(media_handle); // will throw on bad handle
		InvokePlayerStateChangeCallbackOnCPUResumeIfRegistered(false, *player);
		ReelMagic_DeletePlayer(media_handle);
		LOG(LOG_REELMAGIC, LOG_NORMAL)("Closed media player handle=%u", media_handle);
		return 0;

	//
	// Play Media Handle
	//
	case 0x03:
		// will throw on bad handle
		player = &ReelMagic_HandleToMediaPlayer(media_handle);
		if (subfunc & 0xFFF0)
			LOG(LOG_REELMAGIC, LOG_WARN)
		("Ignoring upper 12-bits for play command subfunc: %04X", (unsigned)subfunc);
		switch (subfunc & 0x000F) {
		case 0x0000: // start playing -- Stop on completion
			LOG(LOG_REELMAGIC, LOG_NORMAL)
			("Start playing handle #%u; stop on completion", (unsigned)media_handle);
			player->Play(ReelMagic_MediaPlayer::MPPM_STOPONCOMPLETE);
			return 0; // not sure if this means success or not... nobody seems to
			          // actually check this...

		case 0x0001: // start playing -- Pause on completion
			LOG(LOG_REELMAGIC, LOG_NORMAL)
			("Start playing handle #%u; pause on completion", (unsigned)media_handle);
			player->Play(ReelMagic_MediaPlayer::MPPM_PAUSEONCOMPLETE);
			return 0; // not sure if this means success or not... nobody seems to
			          // actually check this...

		case 0x0004: // start playing in loop
			LOG(LOG_REELMAGIC, LOG_NORMAL)
			("Start playing/looping handle #%u", (unsigned)media_handle);
			player->Play(ReelMagic_MediaPlayer::MPPM_LOOP);
			return 0; // not sure if this means success or not... nobody seems to
			          // actually check this...

		default:
			LOG(LOG_REELMAGIC, LOG_ERROR)
			("Got unknown play player command. Gonna start playing anyway and hope for the best. handle=%u command=%04Xh",
			 (unsigned)media_handle,
			 (unsigned)subfunc);
			player->Play();
			// not sure if this means success or not... nobody seems to actually check
			// this...
			return 0;
		}
		return 0;

	//
	// Pause Media Handle
	//
	case 0x04:
		player = &ReelMagic_HandleToMediaPlayer(media_handle); // will throw on bad handle
		if (!player->IsPlaying())
			return 0; // nothing to do
		InvokePlayerStateChangeCallbackOnCPUResumeIfRegistered(true, *player);
		player->Pause();
		return 0; // returning zero, nobody seems to actually check this anyways...

	//
	// Unknown 5
	//
	case 0x05:
		// player = &ReelMagic_HandleToMediaPlayer(media_handle); // currently unused - will
		// throw on bad handle
		LOG(LOG_REELMAGIC, LOG_WARN)
		("Ignoring unknown function 5. handle=%u subfunc=%04Xh",
		 (unsigned)media_handle,
		 (unsigned)subfunc);
		return 0;

	//
	// Seek to Byte Offset
	//
	case 0x06:
		player = &ReelMagic_HandleToMediaPlayer(media_handle); // will throw on bad handle
		switch (subfunc) {
		case 0x201:
			// not sure exactly what this means, but Crime Patrol is always setting this
			// value
			player->SeekToByteOffset(static_cast<uint32_t>((param2 << 16) | param1));
			LOG(LOG_REELMAGIC, LOG_NORMAL)
			("Seeking player handle #%u to file offset %04X%04Xh",
			 (unsigned)media_handle,
			 (unsigned)param2,
			 (unsigned)param1);
			return 0;
		default:
			LOG(LOG_REELMAGIC, LOG_ERROR)
			("Got unknown seek subfunc. handle=%u subfunc=%04Xh",
			 (unsigned)media_handle,
			 (unsigned)subfunc);
			return 0;
		}
		return 0;

	//
	// Unknown 7
	//
	case 0x07:
		// player = &ReelMagic_HandleToMediaPlayer(media_handle); // currently unused - will
		// throw on bad handle
		LOG(LOG_REELMAGIC, LOG_WARN)
		("Ignoring unknown function 7. handle=%u subfunc=%04Xh",
		 (unsigned)media_handle,
		 (unsigned)subfunc);
		return 0;

	//
	// Set Parameter
	//
	case 0x09:
		if (media_handle == 0) { // global
			cfg = &ReelMagic_GlobalDefaultPlayerConfig();
		} else { // per player...
			 // will throw on bad handle
			player = &ReelMagic_HandleToMediaPlayer(media_handle);
			cfg    = &player->Config();
		}
		switch (subfunc) {
		case 0x0208: // user data
			rv = cfg->UserData;

			cfg->UserData = static_cast<uint32_t>((param2 << 16) | param1);
			LOG(LOG_REELMAGIC, LOG_NORMAL)
			("Setting %s #%u User Data to %08X",
			 (media_handle ? "Player" : "Global"),
			 (unsigned)media_handle,
			 cfg->UserData);
			break;
		case 0x0210: // magical decode key
			rv = cfg->MagicDecodeKey;

			cfg->MagicDecodeKey = static_cast<uint32_t>((param2 << 16) | param1);
			LOG(LOG_REELMAGIC, LOG_NORMAL)
			("Setting %s #%u Magical Decode Key to %08X",
			 (media_handle ? "Player" : "Global"),
			 (unsigned)media_handle,
			 cfg->MagicDecodeKey);
			break;
		case 0x040D: // VGA alpha palette index
			rv = cfg->VgaAlphaIndex;
			assert(param1 <= UINT8_MAX);
			cfg->VgaAlphaIndex = static_cast<uint8_t>(param1);
			LOG(LOG_REELMAGIC, LOG_NORMAL)
			("Setting %s #%u VGA Alpha Palette Index to %02Xh",
			 (media_handle ? "Player" : "Global"),
			 (unsigned)media_handle,
			 (unsigned)cfg->VgaAlphaIndex);
			break;
		case 0x040E:
			rv = GetPlayerSurfaceZOrderValue(*cfg);

			cfg->VideoOutputVisible = (param1 & 1) == 0;
			cfg->UnderVga           = (param1 & 4) != 0;
			LOG(LOG_REELMAGIC, LOG_NORMAL)
			("Setting %s #%u Surface Z-Order To: %s %s VGA",
			 (media_handle ? "Player" : "Global"),
			 (unsigned)media_handle,
			 cfg->VideoOutputVisible ? "Visible" : "Hidden",
			 cfg->UnderVga ? "Under" : "Over");
			break;
		case 0x1409:
			rv = 0;

			cfg->DisplaySize.Width  = param1;
			cfg->DisplaySize.Height = param2;
			LOG(LOG_REELMAGIC, LOG_NORMAL)
			("Setting %s #%u Display Size To: %ux%u",
			 (media_handle ? "Player" : "Global"),
			 (unsigned)media_handle,
			 (unsigned)param1,
			 (unsigned)param2);
			break;
		case 0x2408:
			rv = 0;

			cfg->DisplayPosition.X = param1;
			cfg->DisplayPosition.Y = param2;
			LOG(LOG_REELMAGIC, LOG_NORMAL)
			("Setting %s #%u Display Position To: %ux%u",
			 (media_handle ? "Player" : "Global"),
			 (unsigned)media_handle,
			 (unsigned)param1,
			 (unsigned)param2);
			break;
		default:
			LOG(LOG_REELMAGIC, LOG_WARN)
			("FMPDRV.EXE Unimplemented 09h: handle=%u subfunc=%04hXh param1=%hu",
			 (unsigned)media_handle,
			 (unsigned short)subfunc,
			 (unsigned short)param1);
			return 0;
		}
		if (player != nullptr)
			player->NotifyConfigChange();

		return rv;

	//
	// Get Parameter or Status
	//
	case 0x0A:
		if (media_handle == 0) { // global
			cfg = &ReelMagic_GlobalDefaultPlayerConfig();
		} else { // per player...
			 // will throw on bad handle
			player = &ReelMagic_HandleToMediaPlayer(media_handle);
			cfg    = &player->Config();
			switch (subfunc) {
			case 0x202: // get file state (bitmap of what streams are available...)
				return GetFileStateValue(*player);
			case 0x204: // get play state
				return GetPlayStateValue(*player);
			case 0x206: // get bytes decoded
				return static_cast<uint32_t>(player->GetBytesDecoded());
			case 0x208: // user data
				// return cfg->UserData;

				// XXX WARNING: Not yet returning this as I fear the consequences
				// will be dire unless DMA streaming is properly implemented!
				return 0;
			case 0x403:
				// XXX WARNING: FMPTEST.EXE thinks the display width is 720 instead
				// of 640!
				return static_cast<uint32_t>((player->GetAttrs().PictureSize.Height << 16) |
				                             player->GetAttrs().PictureSize.Width);
			}
		}
		switch (subfunc) {
		case 0x108:                // memory available?
			return 0x00000032; // XXX FMPTEST wants at least 0x32... WHY !?
		case 0x0210:               // magical key
			return cfg->MagicDecodeKey;
		case 0x040D: // VGA alpha palette index
			return cfg->VgaAlphaIndex;
		case 0x040E: // surface z-order
			return GetPlayerSurfaceZOrderValue(*cfg);
		}
		LOG(LOG_REELMAGIC, LOG_ERROR)
		("Got unknown status query. Likely things are gonna fuck up here. handle=%u query_type=%04Xh",
		 (unsigned)media_handle,
		 (unsigned)subfunc);
		return 0;

	//
	// Set The Driver -> User Application Callback Function
	//
	case 0x0b:
		LOG(LOG_REELMAGIC, LOG_WARN)
		("Registering driver_callback() as [%04X:%04X]", (unsigned)param2, (unsigned)param1);
		_userCallbackFarPtr = RealMake(param2, param1);
		_userCallbackType   = subfunc;
		return 0;

	//
	// Unload FMPDRV.EXE
	//
	case 0x0d:
		LOG(LOG_REELMAGIC, LOG_NORMAL)("Request to unload FMPDRV.EXE via INT handler.");
		FMPDRV_UninstallINTHandler();
		return 0;

	//
	// Reset
	//
	case 0x0e:
		LOG(LOG_REELMAGIC, LOG_NORMAL)("Reset");
		ReelMagic_ClearPlayers();
		ReelMagic_ClearVideoMixer();
		_userCallbackFarPtr = 0;
		_userCallbackType   = 0;
		return 0;

	//
	// Unknown 0x10
	//
	case 0x10: // unsure what this is -- RTZ only if we dont respond to the INT 2F 981Eh call...
		LOG(LOG_REELMAGIC, LOG_WARN)("FMPDRV.EXE Unsure 10h");
		return 0;
	}
	E_Exit("Unknown command %xh caught in ReelMagic driver", command);
	// throw RMException("Unknown API command %xh caught", command); (unreachable)
}

static Bitu FMPDRV_INTHandler()
{
	if (RealMake(SegValue(cs), reg_ip) == _userCallbackReturnDetectIp) {
		// if we get here, this is not a driver call, but rather we are cleaning up
		// and restoring state from us invoking the user callback...
		CleanupFromUserCallback();
		return CBRET_NONE;
	}

	// define what the registers mean up front...
	const uint8_t command           = reg_bh;
	reelmagic_handle_t media_handle = reg_bl;
	const uint16_t subfunc          = reg_cx;

	// filename_ptr for command 0x1 & hardcoded to 1 for command 9
	const uint16_t param1 = reg_ax;

	// filename_seg for command 0x1
	const uint16_t param2 = reg_dx;
	try {
		// clear all regs by default on return...
		reg_ax = 0;
		reg_bx = 0;
		reg_cx = 0;
		reg_dx = 0;

		const uint32_t driver_call_rv =
		        FMPDRV_driver_call(command, media_handle, subfunc, param1, param2);
		reg_ax = (uint16_t)(driver_call_rv & 0xFFFF); // low
		reg_dx = (uint16_t)(driver_call_rv >> 16);    // high
		APILOG_DCFILT(command,
		              subfunc,
		              "driver_call(%02Xh,%02Xh,%Xh,%Xh,%Xh)=%Xh",
		              (unsigned)command,
		              (unsigned)media_handle,
		              (unsigned)subfunc,
		              (unsigned)param1,
		              (unsigned)param2,
		              (unsigned)driver_call_rv);
	} catch ([[maybe_unused]] std::exception& ex) {
		LOG(LOG_REELMAGIC, LOG_WARN)
		("Zeroing out INT return registers due to exception in driver_call(%02Xh,%02Xh,%Xh,%Xh,%Xh)",
		 (unsigned)command,
		 (unsigned)media_handle,
		 (unsigned)subfunc,
		 (unsigned)param1,
		 (unsigned)param2);
		reg_ax = 0;
		reg_bx = 0;
		reg_cx = 0;
		reg_dx = 0;
	}
	return CBRET_NONE;
}

class FMPDRV final : public Program {
public:
	FMPDRV()
	{
		help_detail = {HELP_Filter::All,
		               HELP_Category::Dosbox,
		               HELP_CmdType::Program,
		               "FMPDRV"};
	}
	void Run() override
	{
		if (HelpRequested()) {
			MoreOutputStrings output(*this);
			output.AddString(MSG_Get("PROGRAM_FMPDRV_HELP_LONG"));
			output.Display();
			return;
		}
		WriteOut(MSG_Get("PROGRAM_FMPDRV_TITLE"),
		         REELMAGIC_DRIVER_VERSION_MAJOR,
		         REELMAGIC_DRIVER_VERSION_MINOR);

		if (cmd->FindExist("/u")) {
			// unload driver
			if (_installedInterruptNumber == 0) {
				WriteOut(MSG_Get("PROGRAM_FMPDRV_UNLOAD_FAILED_NOT_LOADED"));
				return;
			}
			if (!_unloadAllowed) {
				WriteOut(MSG_Get("PROGRAM_FMPDRV_UNLOAD_FAILED_BLOCKED"));
				return;
			}
			FMPDRV_UninstallINTHandler();
			WriteOut(MSG_Get("PROGRAM_FMPDRV_UNLOADED"));
		} else {
			// load driver
			if (_installedInterruptNumber != 0) {
				WriteOut(MSG_Get("PROGRAM_FMPDRV_LOAD_FAILED_ALREADY_LOADED"),
				         _installedInterruptNumber);
				return;
			}
			if (!FMPDRV_InstallINTHandler()) {
				WriteOut(MSG_Get("PROGRAM_FMPDRV_LOAD_FAILED_INT_CONFLICT"));
				return;
			}
			WriteOut(MSG_Get("PROGRAM_FMPDRV_LOADED"), _installedInterruptNumber);
		}
	}

	static void AddMessages()
	{
		// Only do this once
		static bool messages_were_added = false;
		if (messages_were_added)
			return;

		MSG_Add("PROGRAM_FMPDRV_HELP_LONG",
		        "Load or unload the built-in ReelMagic Full Motion Player driver.\n"
		        "\n"
		        "Usage:\n"
		        "  [color=light-green]fmpdrv[reset]     (load the driver)\n"
		        "  [color=light-green]fmpdrv[reset] /u  (unload the driver)\n"
		        "\n"
		        "Notes:\n"
		        "  The \"reelmagic = on\" configuration setting loads the\n"
		        "  driver on startup and prevents it from being unloaded.\n");

		MSG_Add("PROGRAM_FMPDRV_TITLE",
		        "ReelMagic Full Motion Player Driver (built-in) %hhu.%hhu\n");

		MSG_Add("PROGRAM_FMPDRV_LOADED",
		        "[reset][color=brown]Loaded at interrupt %xh[reset]\n");

		MSG_Add("PROGRAM_FMPDRV_LOAD_FAILED_ALREADY_LOADED",
		        "[reset][color=brown]Already loaded at interrupt %xh[reset]\n");

		MSG_Add("PROGRAM_FMPDRV_LOAD_FAILED_INT_CONFLICT",
		        "[reset][color=light-red]Not loaded: No free interrupts![reset]\n");

		MSG_Add("PROGRAM_FMPDRV_UNLOADED", "[reset][color=brown]Driver unloaded[reset]\n");

		MSG_Add("PROGRAM_FMPDRV_UNLOAD_FAILED_NOT_LOADED",
		        "[reset][color=brown]Driver was not loaded[reset]\n");

		MSG_Add("PROGRAM_FMPDRV_UNLOAD_FAILED_BLOCKED",
		        "[reset][color=brown]Driver not unloaded: configured to stay resident[reset]\n");

		messages_were_added = true;
	}
};

void REELMAGIC_MaybeCreateFmpdrvExecutable()
{
	// Always register the driver's text messages, even if the card is
	// disabled. We cannot rely on the driver to register them because we
	// only create the driver if the user enables ReelMagic support.
	FMPDRV::AddMessages();

	static bool was_driver_created = false;
	const bool is_card_initialized = _dosboxCallbackNumber != 0;

	if (is_card_initialized && !was_driver_created) {
		PROGRAMS_MakeFile("FMPDRV.EXE", ProgramCreate<FMPDRV>);

		// Once we've created the driver, there's no going back: we
		// don't have a PROGRAMS_DestroyFile(..) to remove files from
		// the virtual Z:
		was_driver_created = true;
	}
}

//
// the implementation of "RMDEV.SYS" begins here...
//
/*
  AFAIK, the responsibility of the "RMDEV.SYS" file is to point
  applications to where they can find the ReelMagic driver (RMDRV.EXE)
  and configuration...

  It also is the sound mixer control API to ReelMagic

  This thing pretty much sits in the DOS multiplexer (INT 2Fh) and responds
  only to AH = 98h things...
*/
static uint16_t GetMixerVolume(const char* const channelName, const bool right)
{
	const auto chan = MIXER_FindChannel(channelName);
	if (!chan) {
		return 0;
	}

	const auto vol_gain       = chan->GetAppVolume();
	const auto vol_percentage = gain_to_percentage(vol_gain[right ? 1 : 0]);
	return check_cast<uint16_t>(iroundf(vol_percentage));
}

static void SetMixerVolume(const char* const channelName, const uint16_t percentage, const bool right)
{
	auto chan = MIXER_FindChannel(channelName);
	if (!chan) {
		return;
	}

	AudioFrame vol_gain     = chan->GetAppVolume();
	vol_gain[right ? 1 : 0] = percentage_to_gain(percentage);
	chan->SetAppVolume({vol_gain.left, vol_gain.right});
}

static bool RMDEV_SYS_int2fHandler()
{
	if ((reg_ax & 0xFF00) != 0x9800)
		return false;
	APILOG(LOG_REELMAGIC, LOG_NORMAL)
	("RMDEV.SYS ax = 0x%04X bx = 0x%04X cx = 0x%04X dx = 0x%04X",
	 (unsigned)reg_ax,
	 (unsigned)reg_bx,
	 (unsigned)reg_cx,
	 (unsigned)reg_dx);
	switch (reg_ax) {
	case 0x9800:
		switch (reg_bx) {
		case 0x0000:             // query driver magic number
			reg_ax = 0x524D; //"RM" magic number
			return true;
		case 0x0001: // query driver version
			// AH is major and AL is minor
			reg_ax = (REELMAGIC_DRIVER_VERSION_MAJOR << 8) | REELMAGIC_DRIVER_VERSION_MINOR;
			return true;
		case 0x0002: // query port i/o base address -- stock "FMPDRV.EXE" only
			assert(reg_ax == REELMAGIC_BASE_IO_PORT); // reg_ax already is assigned 0x9800
			LOG(LOG_REELMAGIC, LOG_WARN)
			("RMDEV.SYS Telling whoever an invalid base port I/O address of %04Xh... This is unlikely to end well...",
			 (unsigned)reg_ax);
			return true;
		case 0x0003: // UNKNOWN!!! REAL DEAL COMES BACK WITH 5...
			reg_ax = 5;
			return true;

		case 0x0007:             // query if PCM and CD audio channel is enabled ?
		case 0x0004:             // query if MPEG audio channel is enabled ?
			reg_ax = 0x0001; // yes ?
			return true;
		case 0x0006: // query ReelMagic board IRQ
			reg_ax = REELMAGIC_IRQ;
			LOG(LOG_REELMAGIC, LOG_WARN)
			("RMDEV.SYS Telling whoever an invalid IRQ of %u... This is unlikely to end well",
			 (unsigned)reg_ax);
			return true;

		case 0x0008: // query sound card port
			reg_ax = 0x220;
			return true;
		case 0x0009: // query sound card IRQ
			reg_ax = 7;
			return true;
		case 0x000A: // query sound card DMA channel
			reg_ax = 1;
			return true;

		case 0x0010:          // query MAIN left volume
		case 0x0011:          // query MAIN right volume
			reg_ax = 100; // can't touch this
			return true;
		case 0x0012: // query MPEG left volume
			reg_ax = GetMixerVolume(ChannelName::ReelMagic, false);
			return true;
		case 0x0013: // query MPEG right volume
			reg_ax = GetMixerVolume(ChannelName::ReelMagic, true);
			return true;
		case 0x0014: // query SYNT left volume
			reg_ax = GetMixerVolume(ChannelName::Opl, false);
			return true;
		case 0x0015: // query SYNT right volume
			reg_ax = GetMixerVolume(ChannelName::Opl, true);
			return true;
		case 0x0016: // query PCM left volume
			reg_ax = GetMixerVolume(ChannelName::SoundBlasterDac, false);
			return true;
		case 0x0017: // query PCM right volume
			reg_ax = GetMixerVolume(ChannelName::SoundBlasterDac, true);
			return true;
		case 0x001C: // query CD left volume
			reg_ax = GetMixerVolume(ChannelName::CdAudio, false);
			return true;
		case 0x001D: // query CD right volume
			reg_ax = GetMixerVolume(ChannelName::CdAudio, true);
			return true;
		}
		break;
	case 0x9801:
		switch (reg_bx) {
		case 0x0010: // set MAIN left volume
			LOG(LOG_REELMAGIC, LOG_ERROR)("RMDEV.SYS: Can't update MAIN Left Volume");
			return true;
		case 0x0011: // set MAIN right volume
			LOG(LOG_REELMAGIC, LOG_ERROR)("RMDEV.SYS: Can't update MAIN Right Volume");
			return true;
		case 0x0012: // set MPEG left volume
			SetMixerVolume(ChannelName::ReelMagic, reg_dx, false);
			return true;
		case 0x0013: // set MPEG right volume
			SetMixerVolume(ChannelName::ReelMagic, reg_dx, true);
			return true;
		case 0x0014: // set SYNT left volume
			SetMixerVolume(ChannelName::Opl, reg_dx, false);
			return true;
		case 0x0015: // set SYNT right volume
			SetMixerVolume(ChannelName::Opl, reg_dx, true);
			return true;
		case 0x0016: // set PCM left volume
			SetMixerVolume(ChannelName::SoundBlasterDac, reg_dx, false);
			return true;
		case 0x0017: // set PCM right volume
			SetMixerVolume(ChannelName::SoundBlasterDac, reg_dx, true);
			return true;
		case 0x001C: // set CD left volume
			SetMixerVolume(ChannelName::CdAudio, reg_dx, false);
			return true;
		case 0x001D: // set CD right volume
			SetMixerVolume(ChannelName::CdAudio, reg_dx, true);
			return true;
		}
		break;
	case 0x9803:
		// output a '\' terminated path string to "FMPDRV.EXE" to DX:BX
		// Note: observing the behavior of "FMPLOAD.COM", a "mov dx,ds" occurs right
		//       before the "INT 2fh" call... therfore, I am assuming the segment to
		//       output the string to is indeed DX and not DS...
		reg_ax = 0;
		MEM_BlockWrite(PhysicalMake(reg_dx, reg_bx),
		               REELMAGIC_FMPDRV_EXE_LOCATION,
		               sizeof(REELMAGIC_FMPDRV_EXE_LOCATION));
		return true;
	case 0x981E:
		// stock "FMPDRV.EXE" and "RTZ" does this... it might mean reset, but probably not
		// Note: if this handler is commented out (ax=981E), then we get a lot of unhandled
		// 10h from RTZ
		ReelMagic_DeleteAllPlayers();
		reg_ax = 0;
		return true;
	case 0x98FF:
		// this always seems to be invoked when an "FMPLOAD /u" happens... so some kind of
		// cleanup i guess?
		ReelMagic_DeleteAllPlayers();

		// clearing AX for good measure, although it does not appear anything is checked
		// after this called
		reg_ax = 0x0;
		return true;
	}
	LOG(LOG_REELMAGIC, LOG_WARN)
	("RMDEV.SYS Caught a likely unhandled ReelMagic destined INT 2F!! ax = 0x%04X bx = 0x%04X cx = 0x%04X dx = 0x%04X",
	 (unsigned)reg_ax,
	 (unsigned)reg_bx,
	 (unsigned)reg_cx,
	 (unsigned)reg_dx);
	return false;
}

void REELMAGIC_Init()
{
	const auto section = get_section("reelmagic");

	// Does the user want ReelMagic emulation?
	const auto reelmagic_choice = section->GetString("reelmagic");

	const auto wants_card_only = (reelmagic_choice == "cardonly");

	const auto reelmagic_choice_has_bool = parse_bool_setting(reelmagic_choice);

	const auto wants_card_and_driver = (reelmagic_choice_has_bool &&
	                                    *reelmagic_choice_has_bool == true);

	if (!wants_card_only && !wants_card_and_driver) {
		if (!reelmagic_choice_has_bool) {
			LOG_WARNING("REELMAGIC: Invalid 'reelmagic' value: '%s', shutting down.",
			            reelmagic_choice.c_str());
		}
		return;
	}

	ReelMagic_InitPlayer(section);
	ReelMagic_InitVideoMixer(section);

	// Driver/Hardware Initialization...
	if (_dosboxCallbackNumber == 0) {
		_dosboxCallbackNumber = CALLBACK_Allocate();
		[[maybe_unused]] const auto second_callback = CALLBACK_Allocate();
		assert(second_callback == _dosboxCallbackNumber + 1);
		// this is so damn hacky! basically the code that the IVT points
		// to for this driver needs more than 32-bytes of code to fit
		// the check strings therefore, we are allocating two adjacent
		// callbacks... seems kinda wasteful... need to explore a better
		// way of doing this...
	}
	DOS_AddMultiplexHandler(&RMDEV_SYS_int2fHandler);
	LOG(LOG_REELMAGIC, LOG_NORMAL)("\"RMDEV.SYS\" successfully installed");

	//_readHandler.Install(REELMAGIC_BASE_IO_PORT,   &read_rm, IO_MB,  0x3);
	//_writeHandler.Install(REELMAGIC_BASE_IO_PORT,  &write_rm, IO_MB, 0x3);

	REELMAGIC_MaybeCreateFmpdrvExecutable();

	if (wants_card_and_driver) {
		_unloadAllowed = false;
		FMPDRV_InstallINTHandler();
	}

	// Assess the state and inform the user
	const bool card_initialized   = _dosboxCallbackNumber != 0;
	const bool driver_initialized = _installedInterruptNumber != 0;

	if (card_initialized && driver_initialized) {
		LOG_MSG("REELMAGIC: Initialised ReelMagic MPEG playback card and driver");
	} else if (card_initialized) {
		LOG_MSG("REELMAGIC: Initialised ReelMagic MPEG playback card");
	} else {
		// Should be impossible to initialize the driver without the card
		assert(driver_initialized == false);
		LOG_WARNING("REELMAGIC: Failed initializing ReelMagic MPEG playback card and/or driver");
	}

#if C_HEAVY_DEBUGGER
	_a204debug = true;
	_a206debug = true;
#endif
}

void REELMAGIC_Destroy()
{
	// Assess the state prior to destruction
	bool card_is_shutdown   = (_dosboxCallbackNumber == 0);
	bool driver_is_shutdown = (_installedInterruptNumber == 0);

	// Already shutdown, no work to do.
	if (card_is_shutdown && driver_is_shutdown) {
		return;
	}

	if (!card_is_shutdown && !driver_is_shutdown) {
		LOG_MSG("REELMAGIC: Shutting down ReelMagic MPEG playback card and driver");
	} else {
		// Ensure the only valid alternate state is a running card but
		// not driver
		assert(!card_is_shutdown && driver_is_shutdown);
		LOG_MSG("REELMAGIC: Shutting down ReelMagic MPEG playback card");
	}

	// unload the software driver
	_unloadAllowed = true;
	FMPDRV_UninstallINTHandler();

	// un-register the interrupt handlers
	DOS_DeleteMultiplexHandler(&RMDEV_SYS_int2fHandler);

	// stop mixing VGA and MPEG signals; use pass-through mode
	ReelMagic_SetVideoMixerEnabled(false);

	// un-register the audio channel
	ReelMagic_EnableAudioChannel(false);

	// un-register the callbacks. The presence of a non-zero callback number
	// indicates the card is currently active
	if (_dosboxCallbackNumber != 0) {
		CALLBACK_DeAllocate(_dosboxCallbackNumber + 1);
		CALLBACK_DeAllocate(_dosboxCallbackNumber);
		_dosboxCallbackNumber = 0;
	}

	// Re-assess the driver's state after destruction
	driver_is_shutdown = _installedInterruptNumber == 0;
	if (!driver_is_shutdown) {
		LOG_WARNING("REELMAGIC: Failed unloading ReelMagic MPEG playback driver");
	}
}

static void notify_reelmagic_setting_updated([[maybe_unused]] SectionProp& section,
                                             [[maybe_unused]] const std::string& prop_name)
{
	REELMAGIC_Destroy();
	REELMAGIC_Init();
}

static void init_reelmagic_config_settings(SectionProp& section)
{
	using enum Property::Changeable::Value;

	auto pstring = section.AddString("reelmagic", WhenIdle, "off");
	pstring->SetHelp(
	        "ReelMagic (aka REALmagic) MPEG playback support ('off' by default).\n"
	        "Possible values:\n"
	        "\n"
	        "  off:       Disable support (default).\n"
	        "  cardonly:  Initialize the card without loading the FMPDRV.EXE driver.\n"
	        "  on:        Initialize the card and load the FMPDRV.EXE on startup.");

	pstring = section.AddString("reelmagic_key", WhenIdle, "auto");
	pstring->SetHelp(
	        "Set the 32-bit magic key used to decode the game's videos ('auto' by default).\n"
	        "Possible values:\n"
	        "\n"
	        "  auto:      Use the built-in routines to determine the key (default).\n"
	        "  common:    Use the most commonly found key, which is 0x40044041.\n"
	        "  thehorde:  Use The Horde's key, which is 0xC39D7088.\n"
	        "  <custom>:  Set a custom key in hex format (e.g., 0x12345678).");

	auto pint = section.AddInt("reelmagic_fcode", WhenIdle, 0);
	pint->SetHelp(
	        "Override the frame rate code used during video playback (0 by default).\n"
	        "Possible values:\n"
	        "\n"
	        "  0:       No override: attempt automatic rate discovery (default).\n"
	        "\n"
	        "  1 to 7:  Override the frame rate to one the following (use 1 through 7):\n"
	        "           1=23.976, 2=24, 3=25, 4=29.97, 5=30, 6=50, or 7=59.94 FPS.");
}

void REELMAGIC_AddConfigSection([[maybe_unused]] const ConfigPtr& conf)
{
	assert(conf);

	auto section = conf->AddSection("reelmagic");
	section->AddUpdateHandler(notify_reelmagic_setting_updated);

	init_reelmagic_config_settings(*section);
}
