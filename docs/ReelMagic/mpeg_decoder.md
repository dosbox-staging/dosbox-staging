# Overview

The MPEG asset files that most ReelMagic games use are encoded in a non-standard
format. This causes major video artifact problems when playing these MPEG files
on something other than the ReelMagic decoder card, such as VLC Media Player. It
is theorized that this is some form of "clone protection" so that it would be
incredibly difficult to manufacture a competing product alternative to Sigma
Design's ReelMagic decoder card. This is unlikely a form of "copy protection"
because:
  1. These files can be copied and played on any PC with almost any model of
     ReelMagic decoder card.
  2. It is fairly trivial to restore the original MPEG file contents by analyzing
     these files and statically trying a handful of different picture `f_code`
     values, and visually inspecting the decode results.
  3. It is difficult to write an emulator which can play the original unmodified
     games for this platform.

By writing this document, my intent is to report all findings on this obsecure
format so that this amazing technology can be emulated on modern devices. This
way, people who have purchased games developed on this technology can still
enjoy said games without the original hardware. Also, this helps preserve the
history of PC computer gaming. Loosing something like this to time would be a
tragedy.

This file documents what I have learned about this format and how to play these
files on a standard MPEG-1 compliant decoder so that the ReelMagic card can be
emulated. The terminology I am using for these files are "Magical MPEG-1" files.

Note: A fundamental understanding of MPEG-1 or other like digital video compression
format(s) is highly recommended in order to fully understand this document.




# Differences Between a Standard MPEG-1 and "Magical" MPEG-1 File

A "magical" MPEG-1 file deviates from the standard by the following differences:

  * The frame rate code in the MPEG-1 sequence header has a value > 0x8. (reserved)
    This is how the file is identified as "magical" vs. MPEG-1 standard.
  * P-picture header forward `f_code` values are invalid and do not match the
    the value used at encode time.
  * B-picture header forward and backward `f_code` values are also invalid.


A standard MPEG-1 decoder/player can successfully play a "magical" MPEG-1 file
if it only plays the I-pictures in the stream. This is because the MPEG-1 'I'
picture headers do not contain an `f_code` value as they are standalone pictures
incapable of carrying motion compensation, or any other data type which is
dependent on another picture in the sequence. Decoding 'P' and 'B' pictures that
contain zero macroblocks utilizing motion vectors, also works just fine on a
standard MPEG-1 decoder/player because the `f_code` value is only used when
decompressing motion data. This is why the video appears to play just fine on
still scenes, but gets royally screwed up when things are moving around.



# Decoding a "Magical" MPEG-1 File

Unlike a standard MPEG-1 or other compressed format video file, decoding a
"magical" MPEG-1 file additionally requires a "magic key" in order to recover
the proper `f_code` values in each picture header.

The MPEG-1 sequence frame rate code needs to be recovered so that the decoder
knows the proper frame rate (for decode operations not using PTS/DTS values)
to play the video file at. The `f_code` values in each MPEG-1 'P' and 'B'
picture header must also be recovered prior to the picture's slices being
decoded/decompressed.


## Recovering the MPEG-1 Sequence Frame Rate Code

Recovering the frame rate code in the MPEG-1 sequence header is trivial. Simply
apply a bitwise AND of `0x7` to the frame rate code of a "magical" MPEG-1 file,
and that will yield the original frame rate code.

The formula is:
```
  MAGICAL_FRAME_RATE_CODE & 0x07 = MPEG_1_STANDARD_FRAME_RATE_CODE
```

For example, suppose a frame rate code with a value of `0xC` is decoded from a
"magical" MPEG-1 file. Performing a calculation using the above formula
`0xC & 0x7 = 0x4` yields a standard frame rate code value of `0x4` which
according to ISO/IEC 11172-2 is 30000/1001 (~29.97) fps.


## Recovering the MPEG-1 Picture `f_code` Values

Unfortunately, the exact details for recovering the `f_code` values in the
MPEG-1 picture headers are not completely (yet?) understood. However, it is
understood well enough in order to decode and play the "magical" MPEG-1 files
for all known ReelMagic games in existance as of June 2022.

The high-level function for recovering an `f_code` value in a "magical" MPEG-1
'P' or 'B' picture header is as such:

```
                          /---------\
           Magic Key ---> |         |
         Picture TSN ---> |   ???   | ---> Actual f_code Value
Encoded f_code Value ---> |         |
                          \---------/
```

The exact details of how the above function works are unknown, however, the
inputs and output are 100% confirmed. It appears this function is implemented
via custom firmware loaded into the PL-450 chip on the ReelMagic board.

Inputs are:

  * Magic Key -- 32-bit value provided by the game before any MPEG files are
    opened and decoded.
  * Picture TSN -- 10-bit temporal sequence number in the current picture
    header
  * Encoded `f_code` Value -- 3-bit forward/backward `f_code` value in the
    current picture header

The output is the actual `f_code` value(s) to be used in place of the input
encoded `f_code` value(s) in order to properly decode the picture's slices
without errors.

This function must be performed once per every 'P' picture (forward `f_code`)
and twice per every 'B' picture (forward and backward `f_code`)


### Find a Truthful `f_code` Approach


This approach requires random access to the complete "magical" MPEG-1 file
because a picture matching specific criteria must be found in order to be
able to start playing the file. This approach apperars to work well for
offline decode of a given file, but does not work well for an emulator as
there are a lot of games that feed the MPEG-1 data to the decoder in 4k-
sized chunks. This approach does not cover the case where an MPEG-1 file
is encoded with more than one unique `f_code` value.

The MPEG asset file is quickly scrubbed at media handle open time to find
a matchiing P or B picture containing a truthful `f_code` value. Then this
value is statically applied to the entire decoding of said MPEG asset file.

Known "magic key" values:

  * 0x40044041 -- A truthful `f_code` can be found in pictures with a
                  temporal sequence number (TSN) of 3 or 8.
  * 0xC39D7088 -- A truthful `f_code` can be found in pictures with a
                  temporal sequence number (TSN) of 4.


### Delta/Delta Approach

This approach enables `f_code` recovery on a per-picture basis with no
dependency on other pictures. It is virtually a drop-in replacement for
the recovery function mentioned above. The idea behind this approach is
that each "magic key" value generates a predictable repeating pattern of
delta values which can be summed to a picture's given `f_code` value to
yield the correct `f_code` value. The delta value choosen in the pattern
is based off the picture's TSN. The delta pattern itself is large as it
repeats only after 56 TSN values. However, the delta pattern can be
predicted on-the-fly by using a delta pattern of the delta pattern which
is a small set of only 8 numbers. This is why this is called the
delta/delta approach. Furthermore, since every odd number in this set
always appears to be 6, we can reduce the set to down to 4; just the
even numbers.

There are three phases to the delta/delta approach required to implement
the above mentioned high-level MPEG-1 picture `f_code` recovery function.
First, the magic key is converted to the "delta/delta even pattern":

```
               /---------\
               |         |
Magic Key ---> |   ???   | ---> Delta/Delta Even Pattern
               |         |
               \---------/
```

The relationship between the magic key and delta/delta even pattern is
not (yet?) understood. For now, the following lookup table is used to
retrieve the delta/delta even pattern given the magic key:

  * 0x40044041 -- [4, 3, 2, 3] 
  * 0xC39D7088 -- [1, 3, 3, 3] 


Next, the delta `f_code` value is computed using the delta/delta even
pattern in conjunction with the picture's TSN value:

```
                              /---------\
             Picture TSN ---> | Compute |
                              |  Delta  | ---> Delta f_code Value
Delta/Delta Even Pattern ---> | f_code  |
                              \---------/
```

An example Python implementation of this function:
```
def compute_delta_f_code(tsn, even_pattern):
    odd_increment = 6
    result = 2
    for i in range(tsn + 1):
        if i & 1 == 0:
            result += even_pattern[(i >> 1) & 3]
        else:
            result += odd_increment
    result %= 7
    return result
```

Finally, the delta `f_code` value is added to the `f_code` value encoded
in the picture header to yield the actual `f_code` value used to decode
the picture without errors:

```
                          /---------\
  Delta f_code value ---> |         |
                          |    +    | ---> Actual f_code value
Encoded f_code Value ---> |         |
                          \---------/
```

This addition must be performed considering that a legal `f_code` value
only has a range of 1-7. For example, `3+3 = 6`, `7+1 = 1`, and `6+3 = 2`

The formula for this is:

```
  (((ENCODED_F_CODE_VALUE - 1) + DELTA_F_CODE_VALUE) % 7) + 1
    = ACTUAL_F_CODE_VALUE
```


This algorithm can be optimized for realtime video decoding by pre-
generating the first 56 (0-55) TSN values for a given "magic key" and
storing these in a lookup table for use at picture decode time. For any
picture that is encounted with a TSN val > 55, the lookup TSN value
used is the picture's TSN value modulo 56.


# About the "Magic Key"

The "magic key" is a 32-bit value used to provision the ReelMagic
decoder card before a game's video asset files are played.

The following "magic key" values have been observed:

  * 0x40044041 -- Default value used by the ReelMagic card if none is provided.
    Although most ReelMagic games explicitly specify this value.
  * 0xC39D7088 -- Seen only so far in The Horde.


The "magic key" is provisioned into the card usually at each game's load time.
It is provided by the game to `FMPDRV.EXE` using a function 9/0208h call
`driver_call(9, X, 0208h, ...)`. See the driver API notes
for more information on this.





# Current Emulator Workaround

The current implementation of the emulator uses the  "Find a Truthful
`f_code` Approach" mentioned above. However, soon it will be updated
to use the "Delta/Delta" approach as this is needed to ensure 100%
compatibility accross all currently known ReelMagic games.


# Analyzing and Inspecting "Magical" MPEG-1 Files

A specific-purpose Windows GUI tool called "Voxam" was created to aid in
debugging these "magical" MPEG-1 files. It can be found here:
https://github.com/jrdennisoss/voxam

In addition to Voxam, there are a handful of programs I have included in
the `tools/` directory that can help with analyzing and debugging these files.
Just hit `make` in that directory to build everything. Pre-built Windows
exe files are also included in the DOSBox ReelMagic release.


# Appendix: Standard Frame Rate Code (ISO/IEC 13818-2 : 2000 (E)

Ref: "https://www.itu.int/rec/dologin_pub.asp?lang=s&id=T-REC-H.262-200002-S!!PDF-E&type=items", pp 39.

frame_rate_code (f_code): This is a four-bit integer used to define
frame_rate_value as shown in Table 6-4. frame_rate may be derived from
frame_rate_value, frame_rate_extension_n and frame_rate_extension_d as follows:
frame_rate = frame_rate_value * (frame_rate_extension_n + 1) ÷
(frame_rate_extension_d + 1).

When an entry for the frame rate exists directly in Table 6-4,
frame_rate_extension_n and frame_rate_extension_d shall be zero.
(frame_rate_extension_n + 1) and (frame_rate_extension_d + 1) shall not have a
common divisor greater than one.

Table 6-4 – frame_rate_value

| f_code (bits) | frames per second (FPS)   |
| --------------| ------------------------- |
| 0b0000        | Forbidden                 |
| 0b0001        | 24 000 ÷ 1001 (23.976...) |
| 0b0010        | 24                        |
| 0b0011        | 25                        |
| 0b0100        | 30 000 ÷ 1001 (29.97...)  |
| 0b0101        | 30                        |
| 0b0110        | 50                        |
| 0b0111        | 60 000 ÷ 1001 (59.94...)  |
| 0b1000        | 60                        |
| 0b1001        | Reserved                  |
| ...           | ...                       |
| 0b1111        | Reserved                  |


If progressive_sequence is '1' the period between two successive frames at the
output of the decoding process is the reciprocal of the frame_rate. See Figure
7-18.

If progressive_sequence is '0' the period between two successive fields at the
output of the decoding process is half of the reciprocal of the frame_rate. See
Figure 7-20.

The frame_rate signalled in the enhancement layer of temporal scalability is the
combined frame rate after the temporal re-multiplex operation if
picture_mux_enable in the sequence_scalable_extension() is set to '1'.
