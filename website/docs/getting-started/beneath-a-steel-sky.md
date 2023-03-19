# Beneath a Steel Sky

The next game we're going to set up is [Beneath a Steel
Sky](https://en.wikipedia.org/wiki/Beneath_a_Steel_Sky), a cyberpunk sci-fi
adventure game from 1994. It's one of the standout timeless classics of the
adventure genre, and the best of all, Revolution Software released the game as
freeware in 2003 (see their accompanying notes [here](bass-readme.txt)).


## Mounting a CD image

We're going to set up the liberated "talkie" CD-ROM version of the game that
has full voice-acting. You know the drill; create a new `Beneath a Steel Sky`
folder inside your `DOS Games` subfolder and the usual `drives/c` subfolder
within it. Then download the [ISO CD-ROM image](https://archive.org/details/Beneath_a_Steel_Sky_1995_Virgin) and put it into a `cd`
subfolder inside your game folder. The name of the `cd` subfolder has no
special significance; you could put the `.iso` image anywhere, but it's good to
get into the habit of organising your game files systematically (e.g., you
could create a `Manual` of `Extras` subfolder too and put the scanned manuals
and all other extra files there). It's a good idea to rename `Beneath a Steel Sky (1995)(Virgin).iso` to `bass.iso`; our game folder has full name of the game anyway.

Speaking of manuals, make sure to get the scan of the [Security Manual](https://archive.org/details/beneath-a-steel-sky-security-manual/) and the [comic book](https://ia802200.us.archive.org/13/items/beneath-a-steel-sky-comic-book/Beneath_a_Steel_Sky.pdf) that was included in the boxed version of the game.

For the visually inclined, this is the structure we'll end up with:

![](images/bass-tree.png){ .skip-lightbox style="width: 12.4rem; margin: 0.9rem calc(50% - 8.5rem);" }

We need to mount the CD image to be able to use it. Our C drive is the hard
drive, so we'll mount the CD-ROM image as the next letter D by convention.
This is equivalent to having a CD-ROM drive in our emulated computer assigned
to the drive letter D, and inserting a CD into it.

Mounting an image file, such as a floppy or CD-ROM image, is accomplished with
the `imgmount` command. It is pretty simple to use: the first argument is the
drive letter (`d`), the second the path to the CD-ROM image (`cd/bass.iso`),
and the third the type of the image file (`-t iso`).

So this is what we need to put into our config:


```ini
[autoexec]
imgmount d cd/bass.iso -t iso
```

!!! warning

    If you've kept the original name of the ISO, you'll need to enclose it in
    double quotes because it contains spaces:

        imgmount d "cd/Beneath a Steel Sky (1995)(Virgin).iso" -t iso


You can always run `imgmount /?` if you need a little reminder on how to use
it (although the full list of options can be a little overwhelming).

This is what you should see in the DOSBox window after startup if everything is
set up correctly:

<pre class="dos-prompt">
Local directory drives/c/ mounted as C drive
Z:\>imgmount d cd/bass.iso -t iso
MSCDEX installed.
ISO image cd/bass.iso mounted as D drive
Z:\>_
</pre>

MSCDEX is the name of the MS-DOS CD-ROM driver, and the next line just informs
us that our CD image is mounted as drive D. Time to get down to business then!


## Installing the game

Most games that come on CD images need to be installed to the hard drive
first. Usually there's an executable called `INSTALL.EXE` or `SETUP.EXE` in
the root directory of the CD (the extension could be `.COM` or `.BAT` as
well).

Switch to the D drive by executing `d:` then run the `dir` command to inspect
the contents of the CD:

<pre class="dos-prompt">
 Volume in drive D is BASS
 Directory of D:\

INSTALL  EXE                  28,846 06/15/1994  8:56a
README   TXT                   1,569 09/08/2005  1:34a
SKY      DNR                  40,796 07/07/1994  8:40a
SKY      DSK              72,429,382 07/07/1994  8:40a
SKY      EXE                 402,622 07/07/1994  7:21a
SKY      RST                  53,720 07/07/1994  7:19a
                6 file(s)            72,956,935 bytes
                0 dir(s)                      0 bytes free
</pre>

Okay, so we've got two executables with the `.EXE` extension, and one text
file. View `README.TXT` using the `more` command (run `more README.TXT`, then
keep pressing <kbd>Space</kbd> or <kbd>Enter</kbd> to go to the next page, or
<kbd>Ctrl</kbd>+<kbd>C</kbd> to quit). It turns out it just contains some
legal notice we don't really care about. `INSTALL.EXE` is what we're
after, so let's run that!

We're greeted by a pretty standard looking installer. Either press any key
or wait a few seconds to progress to the second screen where you need to
select where the game will be installed:

<figure markdown>
  ![](images/bass-setup1.png){ width=80% }
</figure>

You can navigate the interface by the cursor keys, <kbd>Esc</kbd>,
<kbd>Enter</kbd>, and the mouse. The default `C:\SKY` install location is
perfectly fine, so just accept it by pressing `Enter`.

That will take us to the setup screen where we can select the language of
the in-game text (the voice-acting is in English only), and the sound card
settings:

<figure markdown>
  ![](images/bass-setup2.png){ width=80% }
</figure>

English is fine, and the game has auto-detected the Sound Blaster correctly, so
just accept these defaults for now. And now, the counterintuitive part: to
finish the installation and save the settings, we need to press the *Exit
Install* button, which will take us to the (guess what?) *Exit Install*
dialog:

<figure markdown>
  ![](images/bass-setup3.png){ width=80% }
</figure>

Here you need to press the *Save Setup* button to finalise the settings and
exit the installer. The other two options are a little bit confusing: *Restore
Defaults* kind of makes sense; it will save the default settings and exit,
regardless of the setting on the previous screen. But what would *Quit Without
Saving* do? We have just installed the game, so will there be no settings
saved then if we press this? Probably it will just do the same thing as
*Restore Default*...

As you can see, this is not exactly a masterclass in user interface design,
but it does the job. Expect many DOS-era install and setup utilities to be
similarly slightly illogical---often it's not completely clear what to do, but
it's not too hard to figure it and out with some trial and error.

Anyway, after pressing *Save Setup* the installer will exit and print out the
following instructions:

<pre class="dos-prompt">

BENEATH A STEEL SKY has been installed to directory:

C:\SKY


To run the game type:

C:
CD \SKY
SKY

</pre>

Okey-dokey, let's do as the computer says!

It's the easiest to put the above commands into the `[autoexec]` section of
our config, then simply start DOSBox to run the game.

```ini
[autexec]
imgmount d cd/bass.iso -t iso
c:
cd \sky
sky
```

## Adjusting the emulated CPU speed

After starting the game, don't watch the intro just yet; press <kbd>Esc</kbd>
to jump straight to the starting scene. There's music playing, so far so good.
Move the cursor over the door on the right side of the screen, and when it
turns into a crosshair and the word "Door" appear next to it,
Click on the *Notice* on the right door to inspect it.

This is what Wikipedia say about the [Intel 486DX2/66](https://en.wikipedia.org/wiki/Intel_DX2#:~:text=The%20i486DX2%2D66,performance%20and%20longevity.):

> The i486DX2-66 was a very popular processor for video games enthusiasts in
> the early to mid-90s. Often coupled with 4 to 8 MB of RAM and a VLB video
> card, this CPU was capable of playing virtually every game title available
> for years after its release, right up to the end of the MS-DOS game era,
> making it a "sweet spot" in terms of CPU performance and longevity.

<div class="compact" markdown>

| Emulated CPU      |  MHz | Approx. cycles
|-------------------|-----:|--------------:
| 8088              | 4.77 |    300
| 286               |    8 |    700
| 286               |   12 |   1500
| 286               |   25 |   3000
| 386DX             |   33 |   6000
| 486DX             |   33 |  12000
| 486DX2            |   66 |  25000
| Intel Pentium     |   90 |  50000
| Intel Pentium MMX |  166 | 100000
| Intel Pentium II  |  300 | 200000

</div>

[list of CPU speed sensitive games](https://www.vogonswiki.com/index.php/List_of_CPU_speed_sensitive_games)



## Adjusting volume levels

```ini
[sblaster]
sbtype = sbpro1
```

`mixer fm 50`


```ini
[autoexec]
imgmount D "cd/bass.iso" -t iso
c:
cd sky
mixer fm 50
sky
exit
```

## Setting up Roland MT-32 sound

### Installing the MT-32 ROMs

You might have noticed the game offers a sound option called "Roland" in its
setup utility. What this refers to is the Roland MT-32 family of MIDI sound
modules. These were external devices you could connect to your PC that offered
far more realistic and higher-quality music than any Sound Blaster or AdLib
sound card could produce. They were the Cadillacs of DOS gaming audio for a
while, and they still sound pretty good even by today's standards.

DOSBox Staging can emulate all common variants of the MT-32 family, but it
requires ROM dumps of the original hardware devices to do so. So first we need
to download these ROMs [from
here](https://archive.org/details/mame-versioned-roland-mt-32-and-cm-32l-rom-files) as a ZIP package,
then copy the contents of the archive into our designated MT&#8209;32 ROM folder:

<div class="compact" markdown>

| <!-- --> | <!-- -->
|----------|----------
| **Windows**  | `C:\Users\<USERNAME>\AppData\Local\DOSBox\mt32-roms\`
| **macOS**    | `~/Library/Preferences/DOSBox/mt32-roms/`
| **Linux** ¹    | `$XDG_DATA_HOME/dosbox/mt32-roms/`<br>`$XDG_DATA_HOME/mt32-rom-data/`<br>`$XDG_DATA_DIRS/mt32-rom-data/`<br>`$XDG_CONF_HOME/mt32-roms/`<br>`~/.config/mt32-roms`

*¹ That's a priority order of the MT-32 ROM lookup locations; copy the files
into only one of them.*

If the above download link doesn't work, search for *"mt32 roms mame"* and
*"cm32l roms mame"* in your favourite search engine and you'll figure out the
rest...

After you've copied the ROM files into the appropriate location for your
platform, start up DOSBox again and run the `mixer /listmidi` command.
This will verify your MT-32 ROM files and print a **`y`** character below the
MT-32 ROM versions that could be successfully detected. You should get an
output similar to this:

<figure markdown>
  ![](images/mixer-listmidi.png)
</figure>

### Selecting the MT-32 version

As you might have guessed already, you can tell DOSBox to emulate an MT-32
model of a specific version; you can read about that [on our
wiki](https://github.com/dosbox-staging/dosbox-staging/wiki/MIDI). But in
practice, these are the two models will cover 99% of your gaming needs:

`cm32l`
: Your default should be the `cm32l` which emulates the Roland CM-32L. This is
  basically a 2nd generation MT-32 with 32 additional sound effects that many
  games make good use of. Certain studios, such as LucasArts, tended to favour
  the CM-32L, so their games sound a little better on this module.

`mt32_old`
: Some older games, most notably the early Sierra adventure catalog,
  absolutely _needs_ a 1st generation MT-32; they will refuse to work
  correctly on anything else. In those cases, use `mt32_old`.

To enable MT-32 emulation and specify the version of module you want to
emulate, insert the following into your config:

```ini
[midi]
mididevice = mt32

[mt32]
model = cm32l
```

How do you figure out which MT-32 version to use for a particular game? Well, you
can't do that easily without a lot of research and trial and error, but thanks
to the tireless work of certain prestigiuos individuals, you can simply refer to
[this
list](https://www.vogonswiki.com/index.php/List_of_MT-32-compatible_computer_games)
that tells you the required settings for most well-known games.

Let's consult the list and see what it says about **Beneath a Steel Sky**!

> Requires CM-series/LAPC-I for proper MT-32 output. Buffer overflows on MT-32 'old'. Combined MT-32/SB output only possible using ScummVM

Well, the list knows best, so we'll use the CM-32L for our game (as we've done
in the above config example).

(To appreciate the difference, you can try running the game with the
`mt32_old` model after you've had it successfully set up for the `cm32l`.
You'll find the sound effects in the opening scene sound a lot better on the
CM-32L.)


### Configuring the game for MT-32

So now DOSBox emulates the CM-32L, but we also need to set up the game for
"Roland sound". (They could've been a bit more precise and told us the
game works best with the CM-32L, couldn't they? It's not even mentioned in
the manual!)

Many games have a dedicated setup utility in the same directory where the main
game executable resides. This is usually called `SETUP.EXE`, `SETSOUND.EXE`,
`SOUND.EXE`, `SOUND.BAT`, or something similar. There is no standard, every
game is different. You'll need to poke around a bit; a good starting point is
to list all executables in the main game folder with the `dir *.exe`, `dir
*.com`, and `dir *.bat` commands and attempt running the most promising
looking ones. The manual might also offer some helpful pointers, and so can
the odd text file (`.TXT` extension) in the installation directory or the root
directory of the CD.

Certain games have a combined installer and setup utility usually called
`INSTALL.EXE` or `SETUP.EXE` which can be slightly disorienting for people
with modern sensibilities. You'll get used to it.

This particular game turns things up a notch and does *not* copy the
installer/setup utility into `C:\SKY` as one would rightly expect. To
reconfigure the game, you need to run `INSTALL.EXE` from the CD, so from the
D: drive (I've told you---setting up the game itself is part of the
adventure!)

So let's do that. As we've already installed the game onto the C: drive, we'll
need to press <kbd>Esc</kbd> instead of <kbd>Enter</kbd> in the first *Path
Selection Window*. Not exactly intuitive, but whatever. Now we're in the
*Setup Menu*  screen where we can change the language and configure the sound
options. Select *"Roland"* sound, then press the *Exit Install* and *Save
Setup* buttons to save your settings (don't even get me started...)

Okay, now the moment of truth: start the game up with the `sky` command after
exiting the installer. If nothing went sideways, we should hear the much
improved, glorious MT-32 soundtrack! Now we're cooking with gas!

So let's inspect our favourite door again in the opening scene---hey, where
did the voice-over go?! Yeah... you've probably glossed over this little
detail in the tip from the MT-32 wiki page:

> Combined MT-32/SB output **only possible** using ScummVM

So what this means for us ordinary mortals is that in the original game you can
either use MT-32 for music and sound effects, and you get *no* speech, or you
can use Sound Blaster for music, sound effects, *and* speech. MT-32 music and
sound effects *with* speech, that's a no go.

The game has just taught us an important life lesson: you can't have
everything---especially not in the world of older DOS games. You'll have to
pick what you value most: better music and only subtitles, or full
voice-acting with a slightly worse soundtrack. I'm opting for the latter, and
remember, we can always improve the Sound Blaster / AdLib music by adding
chorus and reverb. This will get it a little bit closer to the MT-32
soundtrack.

```ini
[mixer]
reverb = large
chorus = strong
```

## Aspect-ratio correction

```ini
[render]
aspect = off
viewport_resolution = 1280x800
```

!!! info "From squares to rectangles"

    For the vast majority of computer games from the 80s and 90s featuring 2D
    graphics, the art was created *once* for the leading platform, then it was
    reused for the various conversions for other platforms. It was just not
    economical to draw the graphics multiple times for different aspect ratios
    and resolutions, hence it was done extremely rarely.

    We explained earlier that CRT monitors in the DOS era had a 4:3 aspect
    ratio, so in 320&times;200 mode the pixels had to be 20% taller for the
    image to completely fill the screen. DOSBox does this aspect-ratio
    correction by default which results in games primarly developed for DOS
    PCs assuming 1:1.2 pixel aspect ratio to look correct (as the artist
    intended).

    But what about games where the leading platform was the Amiga or the Atari
    ST, and the game was developed by a European studio? The analog TV
    standard in Europe was PAL, therefore Amigas sold in Europe were PAL
    machines that had square pixels in the 320&times;256 screen mode most
    commonly used by games. So what these European studios most commonly did
    was to draw the art assuming square pixels, but using only a 320&times;200
    portion of the 320&times;256 total available area. On PAL Amigas the art
    appeared in the correct aspect ratio, but letterboxed; on NTSC Amigas and
    DOS PCs that had the 320&times;200 low-res screen mode, the art filled the
    whole screen but appeared slightly stretched. No one seemed to complain
    about this, and they saved a lot of money by not having to draw the art
    twice, so this became a common practice.

    However, now you have the option to enjoy these games in their correct
    aspect ratio *intended* by the original artist by simply turning aspect
    ratio correction off.

    So, the rules of thumb:

    **`aspect = true`**

    - For most games where DOS was the leading platform---this is the majority
      of DOS games.
    - For games where the leading platform was the Amiga or Atari ST, but the
      game was developed by a North American studio (or the actual work was
      *performed* by a European studio, but it was *commissioned* by North
      Americans, and the game was primarily intended for the North American
      market).

    **`aspect = false`**

    - For most games developed by European studios where the Amiga
      or Atari ST was the leading platform
    - Here's a non-exclusive list of the most important Amiga-first European
      studios: Bitmap Brothers, Psygnosis, Bullfrog, Horror Soft / Adventure
      Soft, Magnetic Scrolls, Delphine, Coktel Vision, Revolution, Ubisoft,
      Infogrames, DMA Design, Core Design, Level 9, Team 17, Sensible
      Software, Firebird, Digital Illusions, Silmarils, Thalion, Thalamus,
      Ocean


## Final configuration

Putting it all together, this is our final config:

```ini
[dosbox]
# uncomment for VGA CRT emulation on a 4K display
#machine = vgaonly

[cpu]
cycles = 25000

[sdl]
fullscreen = on
viewport_resolution = 1280x800

[render]
aspect = off
# uncomment for VGA CRT emulation on a 4K display
#glshader = crt/aperture

[sblaster]
sbtype = sbpro1

[midi]
mididevice = mt32

[mt32]
model = cm32l

[mixer]
reverb = large
chorus = strong

[autoexec]
imgmount d "cd/bass.iso" -t iso
c:
cd sky
mixer fm 50
sky
exit
```

With this config, you can switch between Roland MT-32 and Sound Blaster /
AdLib sound at will by only reconfiguring the game via `INSTALL.EXE`---you
don't need to make any further changes to the DOSBox config. In case you're
wondering, enabling reverb and chorus does not add these effects to the MT-32
output by default; that's undesirable as the MT-32 has its own built-in
reverb, and DOSBox is intelligent enough not to apply *double* reverb to its
output!
