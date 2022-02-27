/*
 *  Copyright (C) 2022 Jon Dennis
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

//
// This file contains the reelmagic driver and device emulation code...
//
// This is where all the interaction with the "DOS world" occurs and is
// the implementation of the provided "RMDOS_API.md" documentation...
//




#include "reelmagic.h"
#include "dos_system.h"
#include "dos_inc.h"
#include "regs.h"
#include "programs.h"
#include "callback.h"
#include "mixer.h"
#include "setup.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <exception>
#include <string>
#include <stack>




//note: Reported ReelMagic driver version 2.21 seems to be the most common...
static const Bit8u  REELMAGIC_DRIVER_VERSION_MAJOR  = 2;
static const Bit8u  REELMAGIC_DRIVER_VERSION_MINOR  = 21;
static const Bit16u REELMAGIC_BASE_IO_PORT          = 0x9800; //note: the real deal usually sits at 260h... practically unused for now; XXX should this be configurable!?
static const Bit8u  REELMAGIC_IRQ                   = 11;     //practically unused for now; XXX should this be configurable!?
static const char   REELMAGIC_FMPDRV_EXE_LOCATION[] = "Z:\\"; //the trailing \ is super important!

static Bitu   _dosboxCallbackNumber      = 0;
static Bit8u  _installedInterruptNumber  = 0; //0 means not currently installed
static bool   _unloadAllowed             = true;


//enable full API logging only when heavy debugging is on...
#if C_HEAVY_DEBUG
  static bool _a204debug = true;
  static bool _a206debug = true;
  static inline bool IsDebugLogMessageFiltered(const Bit8u command, const Bit16u subfunc) {
    if (command != 0x0A) return false;
    if ((subfunc == 0x204) && (!_a204debug)) return true;
    if ((subfunc == 0x206) && (!_a206debug)) return true;
    return false;
  }
  #define APILOG LOG
  #define APILOG_DCFILT(COMMAND, SUBFUNC, ...)        \
    if (!IsDebugLogMessageFiltered(COMMAND, SUBFUNC)) \
      APILOG(LOG_REELMAGIC, LOG_NORMAL)(__VA_ARGS__)
#else
  static inline void APILOG_DONOTHING_FUNC(...) {}
  #define APILOG(A,B) APILOG_DONOTHING_FUNC
  static inline void APILOG_DCFILT(...) {}
#endif

//
// driver -> user callback function stuff...
//
namespace {
struct UserCallbackCall {
  Bit16u command;
  Bit16u handle;
  Bit16u param1;
  Bit16u param2;
  bool invokeNext; //set to true if the next queued callback shall be auto-invoked when this one returns
  //typedef (void* postcbcb_t) (void*);
  //postcbcb_t postCallbackCallback; //set to non-null to invoke function after callback returns
  //void *     postCallbackCallbackUserPtr;
  UserCallbackCall() : command(0), handle(0), param1(0), param2(0), invokeNext(false) {}
  UserCallbackCall(const Bit16u command_, const Bit16u handle_ = 0, const Bit16u param1_ = 0, const Bit16u param2_ = 0, const bool invokeNext_ = false) :
    command(command_), handle(handle_), param1(param1_), param2(param2_), invokeNext(invokeNext_) {}
};
struct UserCallbackPreservedState {
  struct Segments segs;
  struct CPU_Regs regs;
  UserCallbackPreservedState() : segs(Segs), regs(cpu_regs) {}
};
};
static std::stack<UserCallbackCall>           _userCallbackStack;
static std::stack<UserCallbackPreservedState> _preservedUserCallbackStates;
static RealPt _userCallbackReturnIp        = 0; //place to point the return address to after the user callback returns back to us
static RealPt _userCallbackReturnDetectIp  = 0; //used to detect if we are returning from the user registered FMPDRV.EXE callback
static RealPt _userCallbackFarPtr          = 0; // 0 = no callback registered
static Bitu   _userCallbackType            = 0; //or rather calling convention


namespace {struct RMException : ::std::exception { //XXX currently duplicating this in realmagic_*.cpp files to avoid header pollution... TDB if this is a good idea...
  std::string _msg;
  RMException(const char *fmt = "General ReelMagic Exception", ...) {
    va_list vl;
    va_start(vl, fmt); _msg.resize(vsnprintf(&_msg[0], 0, fmt, vl) + 1);   va_end(vl);
    va_start(vl, fmt); vsnprintf(&_msg[0], _msg.size(), fmt, vl);          va_end(vl);
    LOG(LOG_REELMAGIC, LOG_ERROR)("%s", _msg.c_str());
  }
  virtual ~RMException() throw() {}
  virtual const char* what() const throw() {return _msg.c_str();}
};};


//
// the file I/O implementations of the "ReelMagic Media Player" begins here...
//
namespace {
  class ReelMagic_MediaPlayerDOSFile : public ReelMagic_MediaPlayerFile {
    //this class gives the same "look and feel" to reelmagic programs...
    //as far as I can tell, "FMPDRV.EXE" also opens requested files into the current PSP...
    const std::string _fileName;
    const Bit16u _pspEntry;
    static Bit16u OpenDosFileEntry(const std::string& filename) {
      Bit16u rv;
      std::string dosfilepath = &filename[4]; //skip over the "DOS:" prefixed by the constructor
      const size_t last_slash = dosfilepath.find_last_of('/');
      if (last_slash != std::string::npos) dosfilepath = dosfilepath.substr(0, last_slash);
      if (!DOS_OpenFile(dosfilepath.c_str(), OPEN_READ, &rv))
        throw RMException("DOS File: Open for read failed: %s", filename.c_str());
      return rv;
    }
    static std::string strcpyFromDos(const Bit16u seg, const Bit16u ptr, const bool firstByteIsLen) {
      PhysPt dosptr = PhysMake(seg, ptr);
      std::string rv; rv.resize(firstByteIsLen ? ((size_t)mem_readb(dosptr++)) : 256);
      for (char *ptr = &rv[0]; ptr <= &rv[rv.size()-1]; ++ptr) {
        (*ptr) = mem_readb(dosptr++);
        if ((*ptr) == '\0') {
          rv.resize(ptr - &rv[0]);
          break;
        }
      }
      return rv;
    }
  protected:
    const char *GetFileName() const {return _fileName.c_str();}
    Bit32u GetFileSize() const {
      Bit32u currentPos = 0;
      if (!DOS_SeekFile(_pspEntry, &currentPos, DOS_SEEK_CUR)) throw RMException("DOS File: Seek failed: Get current position");
      Bit32u result = 0;
      if (!DOS_SeekFile(_pspEntry, &result, DOS_SEEK_END)) throw RMException("DOS File: Seek failed: Get current position");
      if (!DOS_SeekFile(_pspEntry, &currentPos, DOS_SEEK_SET)) throw RMException("DOS File: Seek failed: Reset current position");
      return result;
    }
    Bit32u Read(Bit8u *data, Bit32u amount) {
      Bit32u bytesRead = 0;
      Bit16u transactionAmount;
      while (amount > 0) {
        transactionAmount = (amount > 0xFFFF) ? 0xFFFF : (Bit16u)amount;
        if (!DOS_ReadFile(_pspEntry, data, &transactionAmount)) throw RMException("DOS File: Read failed");
        if (transactionAmount == 0) break;
        data += transactionAmount;
        bytesRead += transactionAmount;
        amount -= transactionAmount;
      }
      return bytesRead;
    }
    void Seek(Bit32u pos, Bit32u type) {
      if (!DOS_SeekFile(_pspEntry, &pos, type)) throw RMException("DOS File: Seek failed.");
    }
  public:
    ReelMagic_MediaPlayerDOSFile(const char * const dosFilepath) :
      _fileName(std::string("DOS:")+dosFilepath),
      _pspEntry(OpenDosFileEntry(dosFilepath)) {}
    ReelMagic_MediaPlayerDOSFile(const Bit16u filenameStrSeg, const Bit16u filenameStrPtr, const bool firstByteIsLen = false) :
      _fileName(std::string("DOS:") + strcpyFromDos(filenameStrSeg, filenameStrPtr, firstByteIsLen)),
      _pspEntry(OpenDosFileEntry(_fileName)) {}
    virtual ~ReelMagic_MediaPlayerDOSFile() { DOS_CloseFile(_pspEntry); }
  };

  class ReelMagic_MediaPlayerHostFile : public ReelMagic_MediaPlayerFile {
    //this class is really only useful for debugging shit...
    FILE * const _fp;
    const std::string _fileName;
    const Bit32u _fileSize;
    static Bit32u GetFileSize(FILE * const fp) {
      if (fseek(fp, 0, SEEK_END) == -1) throw RMException("Host File: fseek() failed: %s", strerror(errno));
      const long tell_result = ftell(fp);
      if (tell_result == -1) throw RMException("Host File: ftell() failed: %s", strerror(errno));
      if (fseek(fp, 0, SEEK_SET) == -1) throw RMException("Host File: fseek() failed: %s", strerror(errno));
      return (Bit32u)tell_result;
    }
  protected:
    const char *GetFileName() const {return _fileName.c_str();}
    Bit32u GetFileSize() const { return _fileSize; }
    Bit32u Read(Bit8u *data, Bit32u amount) {
      const size_t fread_result = fread(data, 1, amount, _fp);
      if ((fread_result == 0) && ferror(_fp))
        throw RMException("Host File: fread() failed: %s", strerror(errno));
      return (Bit32u)fread_result;
    }
    void Seek(Bit32u pos, Bit32u type) {
      if (fseek(_fp, (long)pos, (type == DOS_SEEK_SET) ? SEEK_SET : SEEK_CUR) == -1)
        throw RMException("Host File: fseek() failed: %s", strerror(errno));
    }
  public:
    ReelMagic_MediaPlayerHostFile(const char * const hostFilepath) :
      _fp(fopen(hostFilepath, "rb")), //not using fopen_wrap() as this class is really intended for debug...
      _fileName(std::string("HOST:") + hostFilepath),
      _fileSize(GetFileSize(_fp)) { 
      if (_fp == NULL) throw RMException("Host File: fopen(\"%s\")failed: %s", hostFilepath, strerror(errno));
    }
    virtual ~ReelMagic_MediaPlayerHostFile() { fclose(_fp); }
  };
};







//
// the implementation of "FMPDRV.EXE" begins here...
//
static Bit8u findFreeInt(void) {
  //"FMPDRV.EXE" installs itself into a free IVT slot starting at 0x80...
  for (Bit8u intNum = 0x80; intNum; ++intNum) {
    if (RealGetVec(intNum) == 0) return intNum;
  }
  return 0x00; //failed
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
static Bitu FMPDRV_EXE_INTHandler(void);
static bool FMPDRV_EXE_InstallINTHandler(void) {
  if (_installedInterruptNumber != 0) return true; //already installed
  _installedInterruptNumber = findFreeInt();
  if (_installedInterruptNumber == 0) {
    LOG(LOG_REELMAGIC, LOG_ERROR)("Unable to install INT handler due to no free IVT slots!");
    return false; //hard to beleive this could actually happen... but need to account for...
  }

  //This is the contents of the "FMPDRV.EXE" INT handler which will be put in the ROM region:
  const Bit8u isr_impl[] = { //Note: this is derrived from "case CB_IRET:" in "../cpu/callback.cpp"
    0xEB, 0x1A,  // JMP over the check strings like a champ...
    9, //9 bytes for "FMPDriver" check string
    'F', 'M', 'P', 'D', 'r', 'i', 'v', 'e', 'r', '\0',
    13, //13 bytes for "ReelMagic(TM)" check string
    'R','e','e','l','M','a','g','i','c','(','T','M',')','\0',
    0xFE, 0x38,  //GRP 4 + Extra Callback Instruction
    (Bit8u)(_dosboxCallbackNumber), (Bit8u)(_dosboxCallbackNumber >> 8),
    0xCF,        //IRET

    //extra "unreachable" callback instruction used to signal end of FMPDRV.EXE registered callback 
    //when invoking the "user callback" from this driver
    0xFE, 0x38,  //GRP 4 + Extra Callback Instruction
    (Bit8u)(_dosboxCallbackNumber), (Bit8u)(_dosboxCallbackNumber >> 8)
  };
  //Note: checking against double CB_SIZE. This is because we allocate two callbacks to make this fit
  //      within the "callback ROM" region. See comment in ReelMagic_Init() function below
  if (sizeof(isr_impl) > (CB_SIZE * 2)) E_Exit("CB_SIZE too small to fit ReelMagic driver IVT code. This means that DOSBox was not compiled correctly!");

  CALLBACK_Setup(_dosboxCallbackNumber, &FMPDRV_EXE_INTHandler, CB_IRET, "ReelMagic"); //must happen BEFORE we copy to ROM region!
  for (Bitu i = 0; i < sizeof(isr_impl); ++i) // XXX is there an existing function for this?
    phys_writeb(CALLBACK_PhysPointer(_dosboxCallbackNumber) + i, isr_impl[i]);

  _userCallbackReturnDetectIp = CALLBACK_RealPointer(_dosboxCallbackNumber) + sizeof(isr_impl);
  _userCallbackReturnIp = _userCallbackReturnDetectIp - 4;
  
  RealSetVec(_installedInterruptNumber, CALLBACK_RealPointer(_dosboxCallbackNumber));
  LOG(LOG_REELMAGIC, LOG_NORMAL)("Successfully installed FMPDRV.EXE at INT %02hhXh", _installedInterruptNumber);
  ReelMagic_SetVideoMixerEnabled(true);
  return true; //success
}

static void FMPDRV_EXE_UninstallINTHandler(void) {
  if (_installedInterruptNumber == 0) return; //already uninstalled...
  if (!_unloadAllowed) return;
  LOG(LOG_REELMAGIC, LOG_NORMAL)("Uninstalling FMPDRV.EXE from INT %02hhXh", _installedInterruptNumber);
  ReelMagic_SetVideoMixerEnabled(false);
  RealSetVec(_installedInterruptNumber, 0);
  _installedInterruptNumber = 0;
  _userCallbackFarPtr = 0;
}



//
// functions to serialize the player state into the required API format
//
static Bit16u GetFileStateValue(const ReelMagic_MediaPlayer& player) {
  Bit16u value = 0;
  if (player.HasVideo()) value |= 2;
  if (player.HasAudio()) value |= 1;
  return value;
}

static Bit16u GetPlayStateValue(const ReelMagic_MediaPlayer& player) {
  //XXX status code 1 = paused
  //XXX status code 2 = stopped (e.g. never started with function 3)
  Bit16u value = player.IsPlaying() ? 0x4 : 0x1;
  if ((_userCallbackType == 0x2000) && player.IsPlaying()) value |= 0x10; //XXX hack for RTZ!!!
  return value;
}

static Bit16u GetPlayerSurfaceZOrderValue(const ReelMagic_PlayerConfiguration& cfg) {
  if (!cfg.VideoOutputVisible) return 1;
  if (cfg.UnderVga) return 4;
  return 2;
}

//
// This is to invoke the user program driver callback if registered...
//
static void EnqueueTopUserCallbackOnCPUResume() {
  if (_userCallbackStack.empty()) E_Exit("FMPDRV.EXE Asking to enqueue a callback with nothing on the top of the callback stack!");
  if (_userCallbackFarPtr == 0) E_Exit("FMPDRV.EXE Asking to enqueue a callback with no user callback pointer set!");
  UserCallbackCall& ucc = _userCallbackStack.top();

  //snapshot the current state...
  _preservedUserCallbackStates.push(UserCallbackPreservedState());

  //prepare the function call...
  switch (_userCallbackType) { //AFAIK, _userCallbackType dictates the calling convention... this is the value that was passed in when registering the callback function to us
  case 0x2000: //RTZ-style; shit is passed on the stack...
    reg_ax = reg_bx = reg_cx = reg_dx = 0; //clear the GP regs for good measure...
    mem_writew(PhysMake(SegValue(ss), reg_sp-=2), ucc.param2);
    mem_writew(PhysMake(SegValue(ss), reg_sp-=2), ucc.param1);
    mem_writew(PhysMake(SegValue(ss), reg_sp-=2), ucc.handle);
    mem_writew(PhysMake(SegValue(ss), reg_sp-=2), ucc.command);
    break;
  default:
    LOG(LOG_REELMAGIC, LOG_WARN)("Unknown user callback type %04Xh. Defaulting to 0000. This is probably gonna screw something up!", (unsigned)_userCallbackType);
    //fall through
  case 0x0000: //The Horde style... shit is passed in registers...
    reg_bx = ((ucc.command << 8) & 0xFF00) | (ucc.handle & 0xFF);
    reg_ax = ucc.param1;
    reg_dx = ucc.param2;
    reg_cx = 0; //???
    break;
  }

  //push the far-call return address...
  mem_writew(PhysMake(SegValue(ss), reg_sp-=2), RealSeg(_userCallbackReturnIp)); //return address to invoke CleanupFromUserCallback()
  mem_writew(PhysMake(SegValue(ss), reg_sp-=2), RealOff(_userCallbackReturnIp)); //return address to invoke CleanupFromUserCallback()

  //then we blast off into the wild blue...
  SegSet16(cs, RealSeg(_userCallbackFarPtr));
  reg_ip = RealOff(_userCallbackFarPtr);

  APILOG(LOG_REELMAGIC, LOG_NORMAL)("Post-Invoking registered user-callback on CPU resume. cmd=%04Xh handle=%04Xh p1=%04Xh p2=%04Xh", (unsigned)ucc.command, (unsigned)ucc.handle, (unsigned)ucc.param1, (unsigned)ucc.param2);
}

static void InvokePlayerStateChangeCallbackOnCPUResumeIfRegistered(const bool isPausing, ReelMagic_MediaPlayer& player) {
  if (_userCallbackFarPtr == 0) return; //no callback registered...

  const unsigned cbstackStartSize = _userCallbackStack.size();
  const ReelMagic_PlayerAttributes& attrs = player.GetAttrs();

  if ((_userCallbackType == 0x2000) && (!isPausing)) {
    //hack to make RTZ work for now...
    _userCallbackStack.push(UserCallbackCall(5, attrs.Handles.Master, 0, 0, _userCallbackStack.size() != cbstackStartSize));
  }

  if (isPausing) {
    //we are being invoked from a pause command
    if (attrs.Handles.Demux) //is this the correct "last" handle !?
      _userCallbackStack.push(UserCallbackCall(7, attrs.Handles.Demux, GetPlayStateValue(player), 0, _userCallbackStack.size() != cbstackStartSize));
    if (attrs.Handles.Video) //is this the correct "middle" handle !?
      _userCallbackStack.push(UserCallbackCall(7, attrs.Handles.Video, GetPlayStateValue(player), 0, _userCallbackStack.size() != cbstackStartSize));
    if (attrs.Handles.Audio) //on the real deal, highest handle always calls back first! I'm assuming its audio!
      _userCallbackStack.push(UserCallbackCall(7, attrs.Handles.Audio, GetPlayStateValue(player), 0, _userCallbackStack.size() != cbstackStartSize));
  } 
  else {
    //we are being invoked from a close command
    if (player.IsPlaying() && attrs.Handles.Demux) //is this the correct "last" handle !?
      _userCallbackStack.push(UserCallbackCall(7, attrs.Handles.Demux, GetPlayStateValue(player), 0, _userCallbackStack.size() != cbstackStartSize)); //4 = state of player; since we only get called on pause, this will always be 4
    if (attrs.Handles.Audio)
      _userCallbackStack.push(UserCallbackCall(7, attrs.Handles.Audio, GetPlayStateValue(player), 0, _userCallbackStack.size() != cbstackStartSize)); //4 = state of player; since we only get called on pause, this will always be 4
    if (attrs.Handles.Video)
      _userCallbackStack.push(UserCallbackCall(7, attrs.Handles.Video, GetPlayStateValue(player), 0, _userCallbackStack.size() != cbstackStartSize)); //4 = state of player; since we only get called on pause, this will always be 4
  }

  if (_userCallbackStack.size() != cbstackStartSize)
    EnqueueTopUserCallbackOnCPUResume();
}

static void CleanupFromUserCallback(void) {
  if (_preservedUserCallbackStates.empty()) E_Exit("FMPDRV.EXE Asking to cleanup with nothing on preservation stack");
  if (_userCallbackStack.empty()) E_Exit("FMPDRV.EXE Asking to cleanup with nothing on user callback stack");
  APILOG(LOG_REELMAGIC, LOG_NORMAL)("Returning from driver_callback()");
  
  //restore the previous state of things...
  const UserCallbackCall& ucc = _userCallbackStack.top();
  const bool invokeNext = ucc.invokeNext;
  _userCallbackStack.pop();

  const UserCallbackPreservedState& s = _preservedUserCallbackStates.top();
  Segs = s.segs;
  cpu_regs = s.regs;
  _preservedUserCallbackStates.pop();

  if (invokeNext) {
    APILOG(LOG_REELMAGIC, LOG_NORMAL)("Invoking Next Chained Callback...");
    EnqueueTopUserCallbackOnCPUResume();
  }
}


static Bit32u FMPDRV_EXE_driver_call(const Bit8u command, const Bit8u media_handle, const Bit16u subfunc, const Bit16u param1, const Bit16u param2) {
  ReelMagic_MediaPlayer *player;
  ReelMagic_PlayerConfiguration *cfg;
  Bit32u rv;
  switch(command) {
  //
  // Open Media Handle (File)
  //
  case 0x01:
    if (media_handle != 0) LOG(LOG_REELMAGIC, LOG_WARN)("Non-zero media handle on open command");
    if (((subfunc & 0xEFFF) != 1) && (subfunc != 2)) LOG(LOG_REELMAGIC, LOG_WARN)("subfunc not 1 or 2 on open command");
    //if subfunc (or rather flags) has the 0x1000 bit set, then the first byte of the caller's
    //pointer is the file path string length
    rv = ReelMagic_NewPlayer(new ReelMagic_MediaPlayerDOSFile(param2, param1, (subfunc & 0x1000) != 0));

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
    player = &ReelMagic_HandleToMediaPlayer(media_handle); // will throw on bad handle
    if (subfunc & 0xFFF0) LOG(LOG_REELMAGIC, LOG_WARN)("Ignoring upper 12-bits for play command subfunc: %04X", (unsigned)subfunc);
    switch (subfunc & 0x000F) {
    case 0x0000: //start playing -- Stop on completion
      LOG(LOG_REELMAGIC, LOG_NORMAL)("Start playing handle #%u; stop on completion", (unsigned)media_handle);
      player->Play(ReelMagic_MediaPlayer::MPPM_STOPONCOMPLETE);
      return 0; //not sure if this means success or not... nobody seems to actually check this...
    case 0x0001: //start playing -- Pause on completion
      LOG(LOG_REELMAGIC, LOG_NORMAL)("Start playing handle #%u; pause on completion", (unsigned)media_handle);
      player->Play(ReelMagic_MediaPlayer::MPPM_PAUSEONCOMPLETE);
      return 0; //not sure if this means success or not... nobody seems to actually check this...
    case 0x0004: //start playing in loop
      LOG(LOG_REELMAGIC, LOG_NORMAL)("Start playing/looping handle #%u", (unsigned)media_handle);
      player->Play(ReelMagic_MediaPlayer::MPPM_LOOP);
      return 0; //not sure if this means success or not... nobody seems to actually check this...
    default:
      LOG(LOG_REELMAGIC, LOG_ERROR)("Got unknown play player command. Gonna start playing anyway and hope for the best. handle=%u command=%04Xh", (unsigned)media_handle, (unsigned)subfunc);
      player->Play();
      return 0; //not sure if this means success or not... nobody seems to actually check this...
    }
    return 0;

  //
  // Pause Media Handle
  //
  case 0x04:
    player = &ReelMagic_HandleToMediaPlayer(media_handle); // will throw on bad handle
    if (!player->IsPlaying()) return 0; //nothing to do
    InvokePlayerStateChangeCallbackOnCPUResumeIfRegistered(true, *player);
    player->Pause();
    return 0; //returning zero, nobody seems to actually check this anyways...

  //
  // Unknown 5
  //
  case 0x05:
    player = &ReelMagic_HandleToMediaPlayer(media_handle); // will throw on bad handle
    LOG(LOG_REELMAGIC, LOG_WARN)("Ignoring unknown function 5. handle=%u subfunc=%04Xh", (unsigned)media_handle, (unsigned)subfunc);
    return 0;

  //
  // Seek to Byte Offset
  //
  case 0x06:
    player = &ReelMagic_HandleToMediaPlayer(media_handle); // will throw on bad handle
    switch (subfunc) {
    case 0x201: //not sure exactly what this means, but Crime Patrol is always setting this value
      player->SeekToByteOffset((param2 << 16) | param1);
      LOG(LOG_REELMAGIC, LOG_NORMAL)("Seeking player handle #%u to file offset %04X%04Xh", (unsigned)media_handle, (unsigned)param2, (unsigned)param1);
      return 0;
    default:
      LOG(LOG_REELMAGIC, LOG_ERROR)("Got unknown seek subfunc. handle=%u subfunc=%04Xh", (unsigned)media_handle, (unsigned)subfunc);
      return 0;
    }
    return 0;


  //
  // Unknown 7
  //
  case 0x07:
    player = &ReelMagic_HandleToMediaPlayer(media_handle); // will throw on bad handle
    LOG(LOG_REELMAGIC, LOG_WARN)("Ignoring unknown function 7. handle=%u subfunc=%04Xh", (unsigned)media_handle, (unsigned)subfunc);
    return 0;

  //
  // Set Parameter
  //
  case 0x09:
    if (media_handle == 0) { // global
      cfg = &ReelMagic_GlobalDefaultPlayerConfig();
    }
    else { // per player...
      player = &ReelMagic_HandleToMediaPlayer(media_handle); // will throw on bad handle
      cfg = &player->Config();
    }
    switch (subfunc) {
    case 0x0208: //user data
      rv = cfg->UserData;
      cfg->UserData = (param2 << 16) | param1;
      LOG(LOG_REELMAGIC, LOG_NORMAL)("Setting %s #%u User Data to %08X", (media_handle ? "Player" : "Global"), (unsigned)media_handle, (unsigned)cfg->UserData);
      break;
    case 0x0210: //magical decode key
      rv = cfg->MagicDecodeKey;
      cfg->MagicDecodeKey = (param2 << 16) | param1;
      LOG(LOG_REELMAGIC, LOG_NORMAL)("Setting %s #%u Magical Decode Key to %08X", (media_handle ? "Player" : "Global"), (unsigned)media_handle, (unsigned)cfg->MagicDecodeKey);
      break;
    case 0x040D: //VGA alpha palette index
      rv = cfg->VgaAlphaIndex;
      cfg->VgaAlphaIndex = param1;
      LOG(LOG_REELMAGIC, LOG_NORMAL)("Setting %s #%u VGA Alpha Palette Index to %02Xh", (media_handle ? "Player" : "Global"), (unsigned)media_handle, (unsigned)cfg->VgaAlphaIndex);
      break;
    case 0x040E:
      rv = GetPlayerSurfaceZOrderValue(*cfg);
      cfg->VideoOutputVisible = (param1 & 1) == 0;
      cfg->UnderVga = (param1 & 4) != 0;
      LOG(LOG_REELMAGIC, LOG_NORMAL)("Setting %s #%u Surface Z-Order To: %s %s VGA", (media_handle ? "Player" : "Global"), (unsigned)media_handle, cfg->VideoOutputVisible ? "Visible" : "Hidden", cfg->UnderVga ? "Under" : "Over");
      break;
    case 0x1409:
      rv = 0;
      cfg->DisplaySize.Width  = param1;
      cfg->DisplaySize.Height = param2;
      LOG(LOG_REELMAGIC, LOG_NORMAL)("Setting %s #%u Display Size To: %ux%u", (media_handle ? "Player" : "Global"), (unsigned)media_handle, (unsigned)param1, (unsigned)param2);
      break;
    case 0x2408:
      rv = 0;
      cfg->DisplayPosition.X = param1;
      cfg->DisplayPosition.Y = param2;
      LOG(LOG_REELMAGIC, LOG_NORMAL)("Setting %s #%u Display Position To: %ux%u", (media_handle ? "Player" : "Global"), (unsigned)media_handle, (unsigned)param1, (unsigned)param2);
      break;
    default:
      LOG(LOG_REELMAGIC, LOG_WARN)("FMPDRV.EXE Unimplemented 09h: handle=%u subfunc=%04hXh param1=%hu", (unsigned)media_handle, (unsigned short)subfunc, (unsigned short)param1);
      return 0;
    }
    if (media_handle != 0) player->NotifyConfigChange();

    return rv;

  //
  // Get Parameter or Status
  //
  case 0x0A:
    if (media_handle == 0) { // global
      cfg = &ReelMagic_GlobalDefaultPlayerConfig();
    }
    else { // per player...
      player = &ReelMagic_HandleToMediaPlayer(media_handle); // will throw on bad handle
      cfg = &player->Config();
      switch (subfunc) {
      case 0x202: //get file state (bitmap of what streams are available...)
        return GetFileStateValue(*player);
      case 0x204: //get play state
        return GetPlayStateValue(*player);
      case 0x206: //get bytes decoded
        return player->GetBytesDecoded();
      case 0x208: //user data
        //return cfg->UserData;
        return 0; //XXX WARNING: Not yet returning this as I fear the consequences will be dire unless DMA streaming is properly implemented!
      case 0x403:
        //XXX WARNING: FMPTEST.EXE thinks the display width is 720 instead of 640!
        return (player->GetAttrs().PictureSize.Height << 16) | player->GetAttrs().PictureSize.Width;
      }
    }
    switch (subfunc) {
    case 0x108: //memory available?
      return 0x00000032; //XXX FMPTEST wants at least 0x32... WHY !?
    case 0x0210: //magical key
      return cfg->MagicDecodeKey;
    case 0x040D: //VGA alpha palette index
      return cfg->VgaAlphaIndex;
    case 0x040E: //surface z-order
      return GetPlayerSurfaceZOrderValue(*cfg);
    }
    LOG(LOG_REELMAGIC, LOG_ERROR)("Got unknown status query. Likely things are gonna fuck up here. handle=%u query_type=%04Xh", (unsigned)media_handle, (unsigned)subfunc);
    return 0;


  //
  // Set The Driver -> User Application Callback Function
  //
  case 0x0b:
    LOG(LOG_REELMAGIC, LOG_WARN)("Registering driver_callback() as [%04X:%04X]", (unsigned)param2, (unsigned)param1);
    _userCallbackFarPtr = RealMake(param2, param1);
    _userCallbackType = subfunc;
    return 0;

  //
  // Unload FMPDRV.EXE
  //
  case 0x0d:
    LOG(LOG_REELMAGIC, LOG_NORMAL)("Request to unload FMPDRV.EXE via INT handler.");
    FMPDRV_EXE_UninstallINTHandler();
    return 0;

  //
  // Reset
  //
  case 0x0e:
    LOG(LOG_REELMAGIC, LOG_NORMAL)("Reset");
    ReelMagic_ResetPlayers();
    ReelMagic_ResetVideoMixer();
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
  E_Exit("Unknown command %02hhXh caught in ReelMagic driver", command);
  throw RMException("Unknown API command %02hhXh caught", command);
}

static Bitu FMPDRV_EXE_INTHandler(void) {
  if (RealMake(SegValue(cs), reg_ip) == _userCallbackReturnDetectIp) {
    //if we get here, this is not a driver call, but rather we are cleaning up
    //and restoring state from us invoking the user callback...
    CleanupFromUserCallback();
    return CBRET_NONE;
  }

  //define what the registers mean up front...
  const Bit8u command                       = reg_bh;
  ReelMagic_MediaPlayer_Handle media_handle = reg_bl;
  const Bit16u subfunc                      = reg_cx;
  const Bit16u param1                       = reg_ax; //filename_ptr for command 0x1 & hardcoded to 1 for command 9
  const Bit16u param2                       = reg_dx; //filename_seg for command 0x1
  try {
   //clear all regs by default on return...
   reg_ax = 0; reg_bx = 0; reg_cx = 0; reg_dx = 0;

   const Bit32u driver_call_rv = FMPDRV_EXE_driver_call(command, media_handle, subfunc, param1, param2);
   reg_ax = (Bit16u)(driver_call_rv & 0xFFFF); //low
   reg_dx = (Bit16u)(driver_call_rv >> 16);    //high
   APILOG_DCFILT(command, subfunc, "driver_call(%02Xh,%02Xh,%Xh,%Xh,%Xh)=%Xh", (unsigned)command, (unsigned)media_handle, (unsigned)subfunc, (unsigned)param1, (unsigned)param2, (unsigned)driver_call_rv);
  }
  catch (std::exception& ex) {
    LOG(LOG_REELMAGIC, LOG_WARN)("Zeroing out INT return registers due to exception in driver_call(%02Xh,%02Xh,%Xh,%Xh,%Xh)", (unsigned)command, (unsigned)media_handle, (unsigned)subfunc, (unsigned)param1, (unsigned)param2);
    reg_ax = 0; reg_bx = 0; reg_cx = 0; reg_dx = 0;
  }
  return CBRET_NONE;
}

struct FMPDRV_EXE : Program {
  static void ProgramStart(Program * * make) { (*make) = new FMPDRV_EXE; }
  static void WriteProgram() { PROGRAMS_MakeFile("FMPDRV.EXE", &FMPDRV_EXE::ProgramStart); }
  void Run() {
    WriteOut("Full Motion Player Driver %hhu.%hhu -- DOSBox\r\n", REELMAGIC_DRIVER_VERSION_MAJOR, REELMAGIC_DRIVER_VERSION_MINOR);
    std::string ignore;
    if (cmd->FindStringBegin("/u", ignore)) {
      //unload driver
      if (_installedInterruptNumber == 0) {
        WriteOut("Driver is not installed\r\n");
        return;
      }
      if (!_unloadAllowed) {
        WriteOut("Unload not allowed.\r\n");
        return;
      }
      FMPDRV_EXE_UninstallINTHandler();
      WriteOut("Successfully removed driver.\r\n");
    }
    else {
      //load driver
      if (_installedInterruptNumber != 0) {
        WriteOut("Driver is already installed at INT %02hhXh\r\n", _installedInterruptNumber);
        return;
      }
      if (!FMPDRV_EXE_InstallINTHandler()) {
        WriteOut("Failed to install ReelMagic driver: No free INTs!\r\n");
        return;
      }
      WriteOut("Successfully installed at INT %02hhXh\r\n", _installedInterruptNumber);
    }
  }
};



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
static Bit16u GetMixerVolume(const char * const channelName, const bool right) {
  MixerChannel * const chan = MIXER_FindChannel(channelName);
  if (chan == NULL) return 0;
  return (Bit16u)(chan->volmain[right ? 1 : 0] * 100.0f);
}
static void SetMixerVolume(const char * const channelName, const Bit16u val, const bool right) {
  MixerChannel * const chan = MIXER_FindChannel(channelName);
  if (chan == NULL) return;
  chan->volmain[right ? 1 : 0] = ((float)val) / 100.0f;
  chan->UpdateVolume();
}
static bool RMDEV_SYS_int2fHandler() {
  if ((reg_ax & 0xFF00) != 0x9800) return false;
  APILOG(LOG_REELMAGIC, LOG_NORMAL)("RMDEV.SYS ax = 0x%04X bx = 0x%04X cx = 0x%04X dx = 0x%04X", (unsigned)reg_ax, (unsigned)reg_bx, (unsigned)reg_cx, (unsigned)reg_dx);
  switch (reg_ax) {
  case 0x9800:
    switch (reg_bx) {
    case 0x0000: //query driver magic number
      reg_ax = 0x524D; //"RM" magic number
      return true;
    case 0x0001: //query driver version
      //AH is major and AL is minor
      reg_ax = (REELMAGIC_DRIVER_VERSION_MAJOR << 8) | REELMAGIC_DRIVER_VERSION_MINOR;
      return true;
    case 0x0002: //query port i/o base address -- stock "FMPDRV.EXE" only
      reg_ax = REELMAGIC_BASE_IO_PORT;
      LOG(LOG_REELMAGIC, LOG_WARN)("RMDEV.SYS Telling whoever an invalid base port I/O address of %04Xh... This is unlikely to end well...", (unsigned)reg_ax);
      return true;
    case 0x0003: //UNKNOWN!!! REAL DEAL COMES BACK WITH 5...
      reg_ax = 5;
      return true;

    case 0x0004: // query if MPEG audio channel is enabled ?
      reg_ax = 0x0001; //yes ?
      return true;
    case 0x0006: // query ReelMagic board IRQ
      reg_ax = REELMAGIC_IRQ;
      LOG(LOG_REELMAGIC, LOG_WARN)("RMDEV.SYS Telling whoever an invalid IRQ of %u... This is unlikely to end well", (unsigned)reg_ax);
      return true;
    case 0x0007: // query if PCM and CD audio channel is enabled ?
      reg_ax = 0x0001; //yes ?
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

    case 0x0010: //query MAIN left volume
      reg_ax = 100; //can't touch this
      return true;
    case 0x0011: //query MAIN right volume
      reg_ax = 100; //can't touch this
      return true;
    case 0x0012: //query MPEG left volume
      reg_ax = GetMixerVolume("REELMAGC", false);
      return true;
    case 0x0013: //query MPEG right volume
      reg_ax = GetMixerVolume("REELMAGC", true);
      return true;
    case 0x0014: //query SYNT left volume
      reg_ax = GetMixerVolume("FM", false);
      return true;
    case 0x0015: //query SYNT right volume
      reg_ax = GetMixerVolume("FM", true);
      return true;
    case 0x0016: //query PCM left volume
      reg_ax = GetMixerVolume("SB", false);
      return true;
    case 0x0017: //query PCM right volume
      reg_ax = GetMixerVolume("SB", true);
      return true;
    case 0x001C: //query CD left volume
      reg_ax = GetMixerVolume("CDAUDIO", false);
      return true;
    case 0x001D: //query CD right volume
      reg_ax = GetMixerVolume("CDAUDIO", true);
      return true;
    }
    break;
  case 0x9801:
    switch (reg_bx) {
    case 0x0010: //set MAIN left volume
      LOG(LOG_REELMAGIC, LOG_ERROR)("RMDEV.SYS: Can't update MAIN Left Volume");
      return true;
    case 0x0011: //set MAIN right volume
      LOG(LOG_REELMAGIC, LOG_ERROR)("RMDEV.SYS: Can't update MAIN Right Volume");
      return true;
    case 0x0012: //set MPEG left volume
      SetMixerVolume("REELMAGC", reg_dx, false);
      return true;
    case 0x0013: //set MPEG right volume
      SetMixerVolume("REELMAGC", reg_dx, true);
      return true;
    case 0x0014: //set SYNT left volume
      SetMixerVolume("FM", reg_dx, false);
      return true;
    case 0x0015: //set SYNT right volume
      SetMixerVolume("FM", reg_dx, true);
      return true;
    case 0x0016: //set PCM left volume
      SetMixerVolume("SB", reg_dx, false);
      return true;
    case 0x0017: //set PCM right volume
      SetMixerVolume("SB", reg_dx, true);
      return true;
    case 0x001C: //set CD left volume
      SetMixerVolume("CDAUDIO", reg_dx, false);
      return true;
    case 0x001D: //set CD right volume
      SetMixerVolume("CDAUDIO", reg_dx, true);
      return true;

    }
    break;
  case 0x9803: //output a '\' terminated path string to "FMPDRV.EXE" to DX:BX
    //Note: observing the behavior of "FMPLOAD.COM", a "mov dx,ds" occurs right
    //      before the "INT 2fh" call... therfore, I am assuming the segment to
    //      output the string to is indeed DX and not DS...
    reg_ax = 0;
    MEM_BlockWrite(PhysMake(reg_dx, reg_bx), REELMAGIC_FMPDRV_EXE_LOCATION, sizeof(REELMAGIC_FMPDRV_EXE_LOCATION));
    return true;
  case 0x981E: //stock "FMPDRV.EXE" and "RTZ" does this... it might mean reset, but probably not
    //Note: if this handler is commented out (ax=981E), then we get a lot of unhandled 10h from RTZ
    ReelMagic_DeleteAllPlayers();
    reg_ax = 0;
    return true;
  case 0x98FF: //this always seems to be invoked when an "FMPLOAD /u" happens... so some kind of cleanup i guess?
    ReelMagic_DeleteAllPlayers();
    reg_ax = 0x0; //clearing AX for good measure, although it does not appear anything is checked after this called
    return true;
  }
  LOG(LOG_REELMAGIC, LOG_WARN)("RMDEV.SYS Caught a likely unhandled ReelMagic destined INT 2F!! ax = 0x%04X bx = 0x%04X cx = 0x%04X dx = 0x%04X", (unsigned)reg_ax, (unsigned)reg_bx, (unsigned)reg_cx, (unsigned)reg_dx);
  return false;
}




// #include "inout.h"
// static IO_ReadHandleObject   _readHandler;
// static IO_WriteHandleObject  _writeHandler;
// static Bitu read_rm(Bitu port, Bitu /*iolen*/) {
//   LOG(LOG_REELMAGIC, LOG_WARN)("Caught port I/O read @ addr %04X", (unsigned)port);
//   switch (port & 0xff) {
//   case 0x02:
//     return 'R'; //no idea what this does... choosing a value from the magic number... might as well be random
//   case 0x03:
//     return 'M'; //no idea what this does... choosing a value from the magic number... might as well be random
//   }
//   return 0;
// }
// static void write_rm(Bitu port, Bitu val, Bitu /*iolen*/) {
//   LOG(LOG_REELMAGIC, LOG_WARN)("Caught port I/O write @ addr %04X", (unsigned)port);
// }
void ReelMagic_Init(Section* sec) {
  Section_prop * section=static_cast<Section_prop *>(sec);
  if (!section->Get_bool("enabled")) return;
  //Video Mixer Initialization...
  // nothing to do ...

  //Player Initialization...
  ReelMagic_InitPlayer(sec);

  //Video Mixer Initialization...
  ReelMagic_InitVideoMixer(sec);
  
  //Driver/Hardware Initialization...
  _dosboxCallbackNumber = CALLBACK_Allocate(); //dosbox will exit with error if this fails so no need to check result...
  if (CALLBACK_Allocate() != _dosboxCallbackNumber + 1) {
    //this is so damn hacky! basically the code that the IVT points to for
    //this driver needs more than 32-bytes of code to fit the check strings
    //
    //therefore, we are allocating two adjacent callbacks... seems kinda
    //wasteful... need to explore a better way of doing this...
    E_Exit("Failed to allocate adjacent \"burner\" callback");
  }
  FMPDRV_EXE::WriteProgram();
  DOS_AddMultiplexHandler(&RMDEV_SYS_int2fHandler);
  LOG(LOG_REELMAGIC, LOG_NORMAL)("\"RMDEV.SYS\" and \"Z:\\FMPDRV.EXE\" successfully installed");

  //_readHandler.Install(REELMAGIC_BASE_IO_PORT,   &read_rm, IO_MB,  0x3);
  //_writeHandler.Install(REELMAGIC_BASE_IO_PORT,  &write_rm, IO_MB, 0x3);

  if (section->Get_bool("alwaysresident")) {
    _unloadAllowed = false;
    FMPDRV_EXE_InstallINTHandler();
  }
  #if C_HEAVY_DEBUG
    _a204debug = section->Get_bool("a204debug");
    _a206debug = section->Get_bool("a206debug");
  #endif
}
