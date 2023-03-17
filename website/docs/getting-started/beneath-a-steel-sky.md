# Beneath a Steel Sky

The next game we're going to set up is [Beneath a Steel
Sky](https://en.wikipedia.org/wiki/Beneath_a_Steel_Sky), a cyberpunk sci-fi
adventure game from 1994. It's one of the standout timeless classics of the
adventure genre, and the best of all, Revolution Software released the game as
freeware in 2003.


## Mounting a CD image

We're going to set up the liberated "talkie" CD-ROM version of the game that
has full voice-acting. You know the drill; create a new `Beneath a Steel Sky`
folder inside your `DOS Games` subfolder and the usual `drives/c` subfolder
within it. Then download the [BASS.iso]() CD-ROM image and put it into a `cd`
subfolder inside your game folder. The name of the `cd` subfolder has no
special significance; you could put the `.iso` image anywhere, but it's good to
get into the habit of organising your game files systematically (e.g., you
could create a `Manual` of `Extras` subfolder too and put the scanned manuals
and all other extra files there).

For the visually inclined, this is the structure we'll end up with:

![](images/bass-tree.png){ .skip-lightbox style="width: 12.4rem; margin: 0.9rem calc(50% - 8.5rem);" }

We need to mount the CD image to be able to use it. Our C drive is the hard
drive, so we'll mount the CD-ROM image as the next letter D by convention.
This is equivalent to having a CD-ROM drive in our emulated computer assigned
to the drive letter D, and inserting a CD into it.

Mounting an image file, such as a floppy or CD-ROM image, is accomplished with
the `imgmount` command. It is pretty simple to use: the first argument is the
drive letter (`d`), the second the path to the CD-ROM image (`cd/BASS.iso`),
and the third the type of the image file (`-t iso`).

So this is what we need to put into our config:


```ini
[autoexec]
imgmount d cd/BASS.iso -t iso
```

You can always run `imgmount /?` if you need a little reminder on how to use
it (although the full list of options can be a little overwhelming).

This is what you should see in the DOSBox window after startup if everything is
set up correctly:

<pre class="dos-prompt">
Local directory drives/c/ mounted as C drive
Z:\>imgmount d cd/BASS.iso -t iso
MSCDEX installed.
ISO image cd/BASS.iso mounted as D drive
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

We're greeted by a pretty standard looking installer. Either press a any key
or wait a few seconds to progress to the second screen where you need to
select where the game will be installed:

<figure markdown>
  ![](images/bass-setup1.png){ width=80% }
</figure>

The interface is navigable by the cursor keys, <kbd>Esc</kbd>,
<kbd>Enter</kbd>, and the mouse. The default `C:\SKY` install location is
perfectly fine, so just press `Enter`.

That will take you to the setup screen where you can select the language of
the in-game text (the voice-acting is in English only), and the sound card
settings: 

<figure markdown>
  ![](images/bass-setup2.png){ width=80% }
</figure>

English is fine, and the game has auto-detected the Sound Blaster correctly, so
just accept these defaults for now. And now, the counterintuitive part: to
finish the installation and save the settings, you need to press the *Exit
Install* button, which will take you to the (guess what?) *Exit Install*
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
but it does the job. Expect all DOS-era install and setup utilities to be
similarly slightly illogical---often it's not completely clear what to do, but it's not to hard to figure and it out with some trial and error.

Anyway, after hitting the *Save Setup* the installer will exit and print the
following instructions on the screen:

<pre class="dos-prompt">

BENEATH A STEEL SKY has been installed to directory:

C:\SKY


To run the game type:

C:
CD \SKY
SKY

</pre>

Okey-dokey, let's just do as the computer says!


