
# Overview

This document describes behavior observed with version 2.21 of the ReelMagic driver.

## Major Components

*  RMDEV.SYS          - Tells programs information about the ReelMagic driver and configuration.
*  MPGDEV.SYS         - Used in place of RMDEV.SYS on newer models.
*  FMPDRV.EXE         - This is the TSR driver for the ReelMagic hardware.
*  The Physical H/W   - FMPDRV.EXE (and possibly RMDEV.SYS) talk to this via port I/O and also probably DMA




# RMDEV.SYS
When loaded, this DOS device driver file responds to some INT 2F / DOS Multiplex calls. Its main
purpose is to tell applications where they can find the driver as well as some other things about
the hardware. It also handles the audio mixer interfaces.

Note: The ReelMagic DOSBox code `driver.cpp` emulates this file's functionality and
therefore is not required for using the DOSBox ReelMagic emulator. Like many emulated functionalities
within DOSBox, there is no actual "RMDEV.SYS" file. Its functionality is permanently resident when
ReelMagic support is enabled.


## Function AX=9800h
The AX=9800h function has several subfunctions it responds to:

### Subfunction BX=0000h - Query Magic Number
Means for applications to discover if the ReelMagic driver and hardware is installed.
Replies with "RM" string by setting AX=524Dh

### Subfunction BX=0001h - Query Driver Version
Means for applications to query the installed ReelMagic driver version.
Replies with AH=major and AL=minor.
Since version 2.21 is the target here, I reply with AH=02h AL=15h

### Subfunction BX=0002h - Query I/O Base Address
Means for applications to query which I/O base address the ReelMagic card is at. From my limited research,
the port I/O size is 4 bytes. Currently replying with a totally incorrect value of AX=9800h. This way if
anything reads/writes to the port, the DOSBox debugger will be verbose about it. The default stock config
of a ReelMagic card sits at port 0x260 from what I can tell.

Note: As "FMPDRV.EXE" is fully emulated, this value is ignored so it does not really matter
      what it replies with.

### Subfunction BX=0003h - Unknown
Not 100% sure, real deal with Maxima card returns ax=5 so emulator currently does the same.

### Subfunction BX=0004h - Query if MPEG/ReelMagic Audio Channel is Enabled (I think)
This impacts the enabled/disabled UI state of the MPEG slider in the "DOXMIX.EXE" utility. This
is either a query to say that we have MPEG audio channel for the mixer, or it's replying with an
IRQ or location or something... AX=1 for yes, AX = 0 for no

### Subfunction BX=0006h - Query IRQ
Means for applications to query the IRQ for the ReelMagic card; returned in ax. The default stock config
of a ReelMagic card sits at IRQ 11.

Note: As "FMPDRV.EXE" is fully emulated, this value is ignored so it does not really matter
      what it replies with.

### Subfunction BX=0007h - Query if PCM and CD Audio Channels are Enabled (I think)
This impacts the enabled/disabled UI state of the PCM and CD Audio sliders in the "DOXMIX.EXE"
utility. This is either a query to say that we have these channels for the mixer, or it's
replying with an IRQ or location or something... AX=1 for yes, AX = 0 for no

### Subfunction BX=0008h - Query PCM Sound Card Port
Returns the port address in AX of the PCM sound card (sound blaster compatible) 

### Subfunction BX=0009h - Query PCM Sound Card IRQ
Returns the IRQ in AX of the PCM sound card (sound blaster compatible) 

### Subfunction BX=000Ah - Query PCM Sound Card DMA Channel
Returns the DMA channel in AX of the PCM sound card (sound blaster compatible) 

### Subfunction BX=0010h - Query Main Audio Left Channel Volume
Reply with the channel volume value in AX. 0 = off and 100 = max.

### Subfunction BX=0011h - Query Main Audio Right Channel Volume
Reply with the channel volume value in AX. 0 = off and 100 = max.

### Subfunction BX=0012h - Query MPEG Audio Left Channel Volume
Reply with the channel volume value in AX. 0 = off and 100 = max.

### Subfunction BX=0013h - Query MPEG Audio Right Channel Volume
Reply with the channel volume value in AX. 0 = off and 100 = max.

### Subfunction BX=0014h - Query SYNTH Audio Left Channel Volume
Reply with the channel volume value in AX. 0 = off and 100 = max.

### Subfunction BX=0015h - Query SYNTH Audio Right Channel Volume
Reply with the channel volume value in AX. 0 = off and 100 = max.

### Subfunction BX=0016h - Query PCM Audio Left Channel Volume
Reply with the channel volume value in AX. 0 = off and 100 = max.

### Subfunction BX=0017h - Query PCM Audio Right Channel Volume
Reply with the channel volume value in AX. 0 = off and 100 = max.

### Subfunction BX=001Ch - Query PCM Audio Left Channel Volume
Reply with the channel volume value in AX. 0 = off and 100 = max.

### Subfunction BX=001Dh - Query PCM Audio Right Channel Volume
Reply with the channel volume value in AX. 0 = off and 100 = max.

### Subfunction BX=001Eh - Unknown
Called from Lord of the Rings. Currently known what this does and not modifying AX
register on return! (meaning it stays at 9800)


## Function AX=9801h
The AX=9801h function has several subfunctions it responds to... As far as I can tell,
these are just setters for the mixer.

### Subfunction BX=0010h - Set Main Audio Left Channel Volume
New volume level is in DX. Value will be between 0 (off) and 100 (max).

### Subfunction BX=0011h - Set Main Audio Right Channel Volume
New volume level is in DX. Value will be between 0 (off) and 100 (max).

### Subfunction BX=0012h - Set MPEG Audio Left Channel Volume
New volume level is in DX. Value will be between 0 (off) and 100 (max).

### Subfunction BX=0013h - Set MPEG Audio Right Channel Volume
New volume level is in DX. Value will be between 0 (off) and 100 (max).

### Subfunction BX=0014h - Set SYNTH Audio Left Channel Volume
New volume level is in DX. Value will be between 0 (off) and 100 (max).

### Subfunction BX=0015h - Set SYNTH Audio Right Channel Volume
New volume level is in DX. Value will be between 0 (off) and 100 (max).

### Subfunction BX=0016h - Set PCM Audio Left Channel Volume
New volume level is in DX. Value will be between 0 (off) and 100 (max).

### Subfunction BX=0017h - Set PCM Audio Right Channel Volume
New volume level is in DX. Value will be between 0 (off) and 100 (max).

### Subfunction BX=001Ch - Set PCM Audio Left Channel Volume
New volume level is in DX. Value will be between 0 (off) and 100 (max).

### Subfunction BX=001Dh - Set PCM Audio Right Channel Volume
New volume level is in DX. Value will be between 0 (off) and 100 (max).



## Function AX=9803h - Query Path to Driver EXE
Means for applications to query the path where they can find the driver ("FMPDRV.EXE") executable. The
path must be fully-qualified and end with a '\' character. For example, if "FMPDRV.EXE" is installed in
"Z:\FMPDRV.EXE", then this function must respond with "Z:\".

The output is to be written as a null-terminated string to caller-provided memory at address DX:BX,
and set AX=0 on success.


## Function AX=981Eh - Unknown
I'm not quite sure what this does. It is possibly a call to reset the card, but I'm just speculating. If I
do NOT return with an AX=0 from this call, then the Return to Zork game spams a bunch of `driver_call(10h,...)`
calls to FMPDRV.EXE and things don't seem to quite function as expected.

On the real setup (Maxima), a non-zero value is returned and Return to Zork
spams these 10h calls on game startup:
```
>> RMDEV.SYS ax=981Eh bx=0005h cx=53F2h dx=1AEEh
<< RMDEV.SYS ax=0001h bx=0005h cx=53F2h dx=1AEEh
>> driver_call(10h, 01h, 0000h, 0000h, 0000h)
<< driver_call() = 00000000h bx=0000h cx=0000h
>> driver_call(10h, 01h, 0000h, 0000h, 0000h)
<< driver_call() = 00000000h bx=0000h cx=0000h
```

## Function AX=98FFh - Probably Driver Unload
This appears to be a reset / clean call that is invoked when a "FMPLOAD.COM /u" is called. Currently
returning AX=0



 








# FMPDRV.EXE

This is the main driver for the ReelMagic MPEG video decoders. When invoked, it installs a software
interrupt handler as a TSR which is responsible for handling all requests to the MPEG decoders. If invoked
with the "/u" command-line option, it unloads the TSR and the interrupt handler it previously installed.

This file must always be named "FMPDRV.EXE" but can exist in any path. The path to this EXE file is
provided by an INT 2F function AX=9803h call to RMDEV.SYS as documented above.


Note: The ReelMagic DOSBox code `driver.cpp` emulates this file's functionality. Since an actual
"FMPDRV.EXE" file must exist somewhere for things to work smoothly, a functional "Z:\FMPDRV.EXE" file is
provided when ReelMagic support is enabled.

## Loading

The Return to Zork game provides an "FMPLOAD.COM" executable, which is responsible for finding and
executing "FMPDRV.EXE" before the game start, and unloading it after the game ends. Since the "FMPDRV.EXE"
string is hardcoded, a constraint is created to use this exact filename for our driver emulator. The
"FMPLOAD.COM" executable will call the correct "Z:\FMPDRV.EXE" file as it makes a call to the "RMDEV.SYS"
Function AX=9803h - Query Path to Driver EXE.


Alternatively, the "Z:\FMPDRV.EXE" file can be invoked in DOSBox's `autoexec` config section as
subsequent calls to either the emulated "FMPDRV.EXE" or a real "FMPDRV.EXE" will be a no-op because the
drivers perform a check to make sure they are not already loaded.

## Detection of FMPDRV.EXE From User Applications

User applicaions detect the ReelMagic "FMPDRV.EXE" driver TSR presence
and interrupt number to use by doing something like this:
```
for (int_num = 0x80; int_num < 0x100; ++int_num) {
  ivt_func_t ivt_callback_ptr = cpu_global_ivt[int_num];
  if (ivt_callback_ptr == NULL) continue;
  const char * str = ivt_callback_ptr; //yup you read that correctly,
                                       //we are casting a function pointer to a string...
  if (strcmp(&str[3], "FMPDriver") == 0)
    return int_num; //we have found the FMPDriver at INT int_num
}
```

Or possibly something like this: (looking at you "FMPTEST.EXE")
```
for (int_num = 0x80; int_num < 0x100; ++int_num) {
  ivt_func_t ivt_callback_ptr = cpu_global_ivt[int_num];
  if (ivt_callback_ptr == NULL) continue;
  const char * str = ivt_callback_ptr; //yup you read that correctly,
                                       //we are casting a function pointer to a string...
  size_t strsize = (unsigned char)&str[2];
  if (strncmp(&str[3], "FMPDriver", strsize) == 0)
    return int_num; //we have found the FMPDriver at INT int_num
}
```

Crime Patrol checks for TWO strings. Something like this:
```
for (int_num = 0x80; int_num < 0x100; ++int_num) {
  ivt_func_t ivt_callback_ptr = cpu_global_ivt[int_num];
  if (ivt_callback_ptr == NULL) continue;
  const char * str = ivt_callback_ptr; //yup you read that correctly,
                                       //we are casting a function pointer to a string...
  str += 2; //skip over JMP instruction
  size_t strsize = (unsigned char)&str[0];
  str += 1; //skip over size field
  if (strncmp(str, "FMPDriver", strsize) == 0) {
    str += strsize + 1; //+1 = '\0' terminator
    strsize = (unsigned char)&str[0];
    str += 1; //skip over size field
    if (strncmp(str, "ReelMagic(TM)", strsize) == 0)
      return int_num; //we have found the FMPDriver at INT int_num
  }
}
```

## The "driver_call()" INT 80h+ API

This is the main API call entry point to the driver from user applications such as SPLAYER.EXE or Return
to Zork, which is used to control the ReelMagic MPEG video decoders. This is normally found at INT 80h,
but it appears that it can also be placed in a higher IVT slot if 80h is already occupied (!=0) by
something else. See above section "Detection of FMPDRV.EXE From User Applications" for the logic used in
user applications to determine which INT number this API is found on. For this project, I usually refer
to this API as the `driver_call()` function.

The prototype for this `driver_call()` function I use is:
```
uint32_t driver_call(uint8_t command, uint8_t media_handle, uint16_t subfunc, uint16_t param1, uint16_t param2);
```

The parameters are passed in as registers:
```
  BH = command
  BL = media_handle
  CX = subfunc
  AX = param1
  DX = param2
```

The 32-bit return value is stored in AX (low 16-bits) and DX (high 16-bits)

The `command` parameter is used to specify which command/function is used and the `subfunc` parameter is
essentially a sub-function for some of these calls. The `media_handle` parameter specifies which handle
the call is specifically for, or 00h if N/A or global parameter. A media handle is essentially a reference
to a "hardware" MPEG video player/decoder resource.

### Command/Function 01h - Open Media Handle
This command/function is used to open/allocate a media handle (MPEG decoder / player)  and returns the
new media handle if successful.

A few `subfunc` values have been observed:
* 0001h -- Open File (Seen by SPLAYER.EXE)
* 0002h -- Open Stream (Seen by Return to Zork)
* 0101h -- Open File (Seen by Dragon's Lair)
* 1001h -- Open File (Seen by FMPTEST.EXE)

When opening a file, `param1` (offset) and `param2` (segment) specify a far pointer to a string containing
a DOS filepath. If the `subfunc` has the 1000h bit set, then the first byte the far pointer is pointing to
is the string length. Otherwise, the string must be NULL terminated.

If the `subfunc` has the 0100h bit set, then the file is not initially scanned. The open file command returns
immediately and successfully regardless of whether or not the file even exists.

When opening a stream, the `param1` and `param2` values are passed directly to the callback command/function
04h. 

### Command/Function 02h - Close Media Handle
Closes the given media handle and frees all resources associated with it. Always returns zero, but as far
as I can tell, no one checks the return value.

### Command/Function 03h - Play Media Handle
This is the call to "play" the given media handle, I have only observed the following sub-functions.

A few `subfunc` values have been observed:
* 0000h -- Stop on finish. After play completes, screen is black, and a call to Ah/204h will yield 2.
* 0001h -- Pause on finish. After play completes, screen is last decoded picture, and a call to Ah/204h will yield 1.
* 0004h -- Play in loop. Calls to Ah/206h will restart from 0 every time the play loops.

I have only seen this command return zero.


### Command/Function 04h - Stop or Pause Media Handle
This command tells the player to stop or pause a video. I see Return to Zork call this on a specific media
handle usually before it closes the handle. Also, Lord of the Rings calls down on this when the user hits
the ESC key or spacebar. Currently returns 0.

### Command/Function 05h - Unknown
Unknown what this does. Crime Patrol calls this often.

### Command/Function 06h - Seek Play Position
This is seen in Crime Patrol and is used as means so set the byte position of the MPEG decoder. This
functionality is needed because the game has a large single MPEG file containing all video assets. 

Only `subfunc` value of 201h has been observed. `param1` is the low 16-bits of the file byte position.
`param2` is the high 16-bits of the file byte position.


### Command/Function 07h - Unknown
Unknown what this does. Crime Patrol calls this often.

### Command/Function 09h - Set Parameter
This is a "set parameter" function. For most of these I am just ignoring them as I do not know exactly what
all these do and am returning zero. This can be called on a specific `media_handle` or called globally by
using a zero `media_handle`.

#### Subfunction 0109h - Unknown
Called from "SPLAYER.EXE" with a zero media handle and return value is not checked. I don't think I have
seen Return to Zork call this subfunction. Returning zero and ignoring for now.

#### Subfunction 0208h - Set User Data
This sets arbitrary user data to associate with a media handle. Both `param1` and `param2` are stored
with the given media handle, and can be retrived with command/function 0xA subfunction 0208h.

The return value does not appear to be checked. Returning zero and ignoring for now.


#### Subfunction 0210h - Set Magic Key

Setting this has an impact on if the card can play "magical" MPEG assets. See
the MPEG decoder notes for more information on this.

* Defaults to: `param1=4041h` `param2=4004h`
* Return to Zork sets this to: `param1=4041h` `param2=4004h`
* The Horde sets this to: `param1=7088h` `param2=C39Dh`

The return value does not appear to be checked. Returning zero and ignoring for now.

#### Subfunction 0302h - Unknown
Return to Zork calls this with a `param1` value of 1 from the open stream callback.

The return value does not appear to be checked. Returning zero and ignoring for now.

#### Subfunction 0303h - Streaming Mode: Set Current Buffer Pointer Offset

AFAIK, this is only for "streaming mode". This sets the current requested buffer pointer offset.

See callback command/function 02h below.

#### Subfunction 0304h - Streaming Mode: Set Current Buffer Size

AFAIK, this is only for "streaming mode". This sets the current requested buffer size.

See callback command/function 02h below.

#### Subfunction 0307h - Streaming Mode: Set Current Buffer Pointer Segment

AFAIK, this is only for "streaming mode". This sets the current requested buffer pointer segment.

See callback command/function 02h below.

#### Subfunction 0408h - Unknown
Called from Return to Zork with a zero media handle and return value does not appear to be checked.
Returning zero and ignoring for now.

#### Subfunction 0409h - Likely Set Resolution or Display Size.
Looks like `param1` is the width and `param2` is the height. Called from Return to Zork with a zero
media handle, 0x0 values, and return value does not appear to be checked. Called from Lord of the
Rings with a zero media handle and 320x200 value. Returning zero and ignoring for now.

Likely be related to subfunction 1409h below.

#### Subfunction 040Ch - Unknown
Called from Return to Zork with a zero media handle and return value does not appear to be checked.
Returning zero and ignoring for now.

#### Subfunction 040Dh - Set VGA Alpha Palette Index
This sets the VGA palette index to use for the alpha/transparent color for when the MPEG surface
z-order is behind the VGA feed.

#### Subfunction 040Eh - Set Surface Z-Order
Can be set globally to default the Z-order and all new media handles, or set per media handle.
`param1` sets the value. Known values are:

* 1 = MPEG video surface is invisible
* 2 = MPEG video surface is in front of the VGA surface.
* 4 = MPEG video surface is behind the VGA surface.

#### Subfunction 1409h - Set Display Size
Called from "FMPTEST.EXE" to set the display dimensions of the output video window. "param1" is width and
"param2" is height. Returning zero.

Likely be related to subfunction 0409h above.

#### Subfunction 2408h - Set Display Position
Called from "FMPTEST.EXE" to set the display position of the output video window. "param1" is width and
"param2" is height. Returning zero.


### Command/Function 0Ah - Get Parameter / Status
This is a "get parameter" or "get status" or both function. Unlike the set functions, these get functions
can't be as easily ignored and must reply back to the user applications with valid values otherwise things
get screwed up real good real fast...


#### Subfunction 0108h - Query Something About Resource Availability ???
Not quite sure about this one, but "FMPTEST.EXE" wants a value of DX=0 and AX >=32h. If I don't give it
what it wants, then the program gives me a "Not enough memory" error. This is only seems to be called
with a zero (global) media handle.


#### Subfunction 0202h - Query Stream Types
This returns the detected stream types. A bitmap is returned which indicates the stream types detected in
the given stream/file. If this returns zero, then the file/stream is deemed invalid. The 0x1 bit is used to
indicate that the MPEG file/stream contains audio. The 0x2 bit is used to indicate that the file/stream
contains video.


#### Subfunction 0204h - Query Play State
This returns the current state of play as a bitmap. Currently, the following bits are known:

* 0x01 - Stream is paused
* 0x02 - Stream is stopped
* 0x04 - Stream is playing.
* 0x10 - Unknown.

#### Subfunction 0206h - Query Bytes Decoded
This returns the 32-bit count of bytes that has been decoded. DX has the upper 16-bits and AX has the
lower 16-bits.


#### Subfunction 0208h - Get User Data
This gets the arbitrary user data associated with a media handle. The `param1` (low) and `param2` (high)
that were previously stored using command/funcion 0x9 subfunction 0208h on the given media handle is
returned. The current implementation returns zero.

#### Subfunction 0403h - Likely Get Decoded Picture Dimensions
This is likely a call to get the decoded picture dimensions. The picture height is in the
upper 16-bits (DX) and the picture width is returned in the lower 16-bits.

If it's not a getter for the decoded picture dimensions, then it's probably a getter for current
display size. (which would need to default to decoded picture dimensions)

#### Subfunction 040Dh - Get VGA Alpha Palette Index
This gets the VGA palette index to use for the alpha/transparent color for when the MPEG surface
z-order is behind the VGA feed.


### Command/Function 0Bh - Register User Callback Function
This registers a callback function for the API user. It would appear that the registered callback function
is to be called on certain driver/device events. See the "The User Callback Function" subsection below
for more information.

AFAIK, the media handle parameter is not used, The `subfunc` parameter specifies the calling convention.
A `subfunc` of 0 is calling convention "A" and a `subfunc` of 2000h is calling convention "B". The callback
function far pointer is specified in the `param1` and `param2` parameters: `param2` is the segment and
`param1` is the offset of the callback function pointer. Zero can be given to both `param2` and `param1` to
disable. Zero is always returned; it does not appear the return value is checked.

### Command/Function 0Dh - Unload FMPDRV.EXE
This is called by "FMPDRV.EXE" when the user passes a "/u" onto the command line. Invoking this function
cleans up any open media handles and removes the driver's INT handler. The "RMDEV.SYS" INT 2F functions
still remain resident and functional. Zero is always returned.

### Command/Function 0Eh - Reset
This is the very first call that "SPLAYER.EXE" and Return to Zork do at application startup. Likely, this is
a reset function as the return value does not appear to be checked.

### Command/Function 10h - Unknown
Unknown what this does. See RMDEV.SYS function AX=981Eh.





## The User Callback "driver_callback()" Function
A "user callback function" can be registered via the `driver_call(0Bh, ...)` API. This function must
be called back from the driver to the user application on certain events. There are multiple calling-
conventions for this callback. The calling convention is set by the argument passed when registering
the callback function.

WARNING: These callbacks can be invoked from an interrupt coming from the ReelMagic board. This is
usually IRQ 11.

There are four known parameters that are passed to the callback function:

* Command -- Specifies what type of callback this is. Always present.
* Handle  -- The ReelMagic media handle. Always present.
* Param1  -- Optional depending on command.
* Param2  -- Optional depending on command.


### Calling Convention "A" (Register with `subfunc=0000h`)

This calling convention passes the parameters in registers. The paramter/register mapping is assigned
as follows:

* BH=Command
* BL=Handle
* AX=param1
* DX=param2
* CX=????

The far return address is on the stack so a RETF must be used to return.

Not sure if the driver is expecting a return, but most applications zero the AX register on return.


### Calling Convention "B" (Register with `subfunc=2000h`)

This calling convention passes the parameters on the stack immediately above the far return address.

The stack looks as such:
```
  -------------------------
  |     16-bit Param2     |
  -------------------------
  |     16-bit Param1     |
  -------------------------
  |     16-bit Handle     |
  -------------------------
  |     16-bit Command    |
  -------------------------
  |   16-bit RA Segment   |
  -------------------------
  |   16-bit RA Offset    | <-- SP is Here When Routine is Invoked
  -------------------------

```

The far return address is on the stack so a RETF must be used to return.

Not sure if the driver is expecting a return, but most applications zero the AX register on return.


### Command/Function 01h - Streaming Media Event Unknown, Likely: "Fill Buffers"
Return to Zork usually reads from an MPEG file into one of its buffer when it gets this callback.

### Command/Function 02h - Streaming Media Event Unknown, Likely: "Report Buffers"
This is called when the driver requests stream data. The requested stream position will be provided
in `param1` (low) and `param2` (high).

It is expecting something like:
```
  driver_call(9, handle, 307, buffer_ptr_segment, 0)
  driver_call(9, handle, 303, buffer_ptr_offset, 0)
  driver_call(9, handle, 304, buffer_size, 0)
```

### Command/Function 03h - Streaming Media Event Unknown
No idea what this does. I am not (yet) calling it.

### Command/Function 04h - Streaming Media Event Opening
This is called when a new media handle is being opened in stream mode. (`driver_call(01h, 00h, 0002h, ...)`)

### Command/Function 05h - Streaming Media Event Closing
This callback is expected to happen once the driver has been asked to close a streaming media handle.

### Command/Function 07h - Decoder Stopped
This is called when a decoder is stopped in either stream or file mode.

### Command/Function 09h - Unknown
No idea what this does. I am not (yet) calling it.

### Command/Function 0Ah - Unknown
No idea what this does. I am not (yet) calling it.

### Command/Function 0Bh - Unknown
No idea what this does. I am not (yet) calling it.



# Physical Hardware

For now, no port I/O calls have been implemented, but I left commented out some of the remaining port I/O
handler code I was tinkering with when debugging. For now, I am taking a similiar approach to what DOSBox did
with MSCDEX, and the implementation of an emulated driver resides in DOSBox.


# Known Issues and Limitations

As this API was generated from what I could get from the DOSBox debugger, Ghidra, and custom tools I have
written for this purpose. There are going to be bugs and issues primarily because this is based on how I
observed things interacting, and not on a known spec.

The custom ReelMagic tools I have written for this can be found here:
https://github.com/jrdennisoss/rmtools



# Example API Usage from SPLAYER.EXE

```
if (detect_driver_intnum() == 0) die; //stores the intnum in a global somewhere that driver_call() uses
driver_call(0xe,0,0,0,0);
driver_call(9,0,0x109,1,0);
mpeg_file_handle = driver_call(1,0,1,argv[1],argv_segment);
if (!mpeg_file_handle) die;
if (!driver_call(10,mpeg_file_handle,0x202,0,0)) {
  driver_call(2,mpeg_file_handle,0,0,0);
  die;
}
INT 10h AX = 0f00h -- Get videomode (store result of AL)
INT 10h AX = 0012h -- Set videomode to 12h; i think this is M_EGA 640x480 (see mode lists in "ints/int10_modes.cpp")
driver_call(3,mpeg_file_handle,1,0,0);
while ((driver_call(10,mpeg_file_handle,0x204,0,0) & 3) == 0);
driver_call(2,mpeg_file_handle,0,0,0);
INT 10h AX = 00??h -- Restore video mode from first INT10h call; AL is set to the stored result
```
