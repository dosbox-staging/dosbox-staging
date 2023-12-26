# Gravis UltraSound

The Gravis UltraSound was an advanced synthesizer released by an unlikely
manufacturer: Canadian joystick company Advanced Gravis. Its audio was far
ahead of any other consumer device of the time, supporting wave-table
synthesis, stereo sound, 14-channel playback at 44.1 KHz or a whopping 32
channels of playback at 19.2 KHz.

However, the Ultrasound eschewed any attempt at backwards-compatibility with
AdLib or Soundblaster cards. Programs had to be written to specifically take
advantage of its capabilities. Many DOS users kept a Sound Blaster in their PC
in addition to an Ultrasound, in case they needed to run a program that did
not support the more advanced card. (And in DOSBox, this can be imitated by
turning on both devices in your configuration file, which is recommended.)

One quirk of the Ultrasound is that, unlike most synthesizers, it did not come
with any voices pre-installed on the card. All voices had to be installed from
disk either at driver load time or by the application. Because of this, a set
of drivers and "patch files" is needed in order to use the Ultrasound in
DOSBox. Due to incompatibilities between the license of the patch files and
DOSBox's GPL license, these files cannot be distributed with DOSBox, so you
will need to download them from another website:

The Gravis Ultrasound is configured under the gus category. It has several
options, which are explained in the comments in the configuration file. Of
particular note is the ultradir option, which must be set to the path to the
patch files inside DOSBox. (Which is likely not the same as the path on your
real hard drive.)



On the plus side, when games did offer support for the Gravis, it provided a far greater audio capability than anything else on the home consumer market. It allowed for 14 channels at 44kHz playback or 32 channels at 19.2kHz.

Several games require the use of Gravis drivers, and as copyrighted software from a commercial company, these drivers are not included with the open-sourced DOSBox installation. You need to install the drivers within DOSBox session to a subdirectory and afterwards change ultradir= to point to this directory path in Windows, macOS or Linux.
