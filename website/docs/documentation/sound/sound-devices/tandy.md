# Tandy 3 Voice

The **Tandy 3 Voice** audio was introduced with the Tandy 1000 IBM PC clone
released in 1984, which was as a clone of the short-lived IBM PCjr.

Life was simple in the 80s---most games only supported the PC speaker, but
some of them had enhanced sound on the Tandy 1000 IBM PC compatible.

The enhanced sound hardware of the Tandy lineup was an integral, non-removable
part of the computer, and was not available as an add-on card for non-Tandy
machines. Therefore, most Tandy games simply check if they're running on a
Tandy, and if so, make use of its integrated sound and graphics capabilities.

To play Tandy games with Tandy sound, you need to enable the emulation of a
generic Tandy computer as follows:

``` ini
[dosbox]
machine = tandy
```

Later Tandy 1000 models added support for a digital audio as well.

``` ini
[sblaster]
sbtype = none
```


!!! note

    Tandy 3 Voice sound is sometimes referred to as **Tandy 1000** or
    **IBM PCjr** in the game's sound setup program.

    Tandy digital sound is sometimes called **Tandy with DAC** or
    **Tandy 1000 SL, TL, HL series** or something similar.




The original Tandy 1000 series of computers that first came out in 1984 upgraded the internal PC speaker to three channels plus white noise. Later models also allowed for external volume control and support for headphones. The Tandy 1000 audio improves the default PC speaker when implemented in software.

In 1989 Tandy released the 1000 SL series of computers that updated their 3-channel audio chip to incorporate an onboard 8-bit, mono DAC/ADC for digital sampling and playback. Several games, including a few Sierra titles such as Space Quest III, support this improvement. They experimented with it for a brief period before the Creative Sound Blaster went retail and drew Sierra’s attention elsewhere.



One of the evolutionary steps IBM took was embedding sound capabilities into the PCjr. The PCjr contained the TI-SN76496 3-voice sound chip, which could also produce a fourth noise channel. Most Tandy 1000 models have this chip as well, as they were clones of the PCjr.

You can hear examples of this chip at That Oldskool Beat, and can run programs that support it under Tand-Em, the Tandy 1000 emulator.




Unlike the PCjr., it has register level compatibility with CGA, whereas the PCjr. has only BIOS level compatibility.  Also, while the PCjr. and the Tandy 1000 support the PC Speaker sound generation, the PCjr. only has a piezo tweeter, while the 1000s have the biggest PC Speaker cone I have ever seen in a compatible.  This means that games that tweak the speaker, like Access Software's Realsound games, will sound far superior on the Tandy than the PCjr.


Many games use these graphics and sound capabilities to give great improvements over competing systems.  There are several games that support Tandy but not EGA graphics and games that support Tandy sound but not sound cards or midi devices. 

PCjr. specific software (almost all of which was released by IBM) may not run on the Tandy or support Tandy graphics and sound.  King's Quest and Touchdown Football (PCjr. versions) will not work in anything other than a Tandy 1000 with 128KB of RAM.  Fortunately there are Tandy versions available of these games.  The near mythical M.U.L.E. port IBM PC was alleged to take advantage of PCjr features but really only supports CGA and PC Speaker sound (but it runs on the jr.).  Cartridge software, even when dumped, will not work because it expects to find itself in the D000 and E000 segments. 

Games automatically detected the Tandy 1000 by first searching the BIOS ROM for the string TANDY. It will be found in any Tandy 1000 because it is shown on the BIOS copyright screen upon bootup. 


some games combine tandy and pc speaker

 Budokan, The Cycles
 silpheed



!!! warning

    Due to resource conflicts, DOSBox cannot emulate the Tandy DAC and Sound Blaster digital
    audio at the same time. If you want to emulate the Tandy DAC in addition
    to the Tandy synthesiser, you need to disable Sound Blaster emulation by
    setting `sbtype` to `none` as shown above.

The Sound Blaster 1.0-2.0 all use DMA1 and do not play nicely with the PSSJ sound chip in the TL/SL/RL, which also uses DMA1.  Lockups and freezes are commonplace when digital sound is played.  The SX does not have a PSSJ chip, so the issue is not present.  


http://nerdlypleasures.blogspot.com/2022/04/list-of-pcjr-and-tandy-exclusive.html
