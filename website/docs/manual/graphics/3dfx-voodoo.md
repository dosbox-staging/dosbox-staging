# 3dfx Voodoo

Games accessed the Voodoo through 3dfx's proprietary **Glide API**, which
talked to the hardware directly and unlocked effects that software rendering
couldn't touch. When a Glide game launched, the Voodoo took over the display
entirely; quit to DOS, and the signal fell back to your 2D card. Titles like
[Tomb Raider](https://www.mobygames.com/game/348/tomb-raider/) and [Screamer
2](https://www.mobygames.com/game/375/screamer-2/)were transformed by it. Even
the fastest Pentium MMX machines could barely manage software-rendered 3D at
640&times;480 — we're talking single-digit framerates. The Voodoo was a luxury, but
what a luxury it was!

Most Glide games need a DOS driver file called **`GLIDE2X.OVL`** in the
game's directory. Many shipped with their own version of this driver;
for those that didn't, you'll need to supply a suitable one (not all drivers
work with every game).

DOSBox Staging emulates the Voodoo at the hardware level --- no shortcuts or
OpenGL passthrough --- which makes it CPU-intensive but highly accurate. Voodoo
emulation is enabled by default, so most DOS Glide games should work out of
the box.

??? note "Complete list of DOS games with 3dfx Voodoo graphics support"

    <div class="compact" markdown>

    - [Actua Soccer (1995)](https://www.mobygames.com/game/5573/action-soccer/)
    - [Archimedean Dynasty (1996)](https://www.mobygames.com/game/2881/archimedean-dynasty/)
    - [Battle Arena Toshinden (1996)](https://www.mobygames.com/game/6238/battle-arena-toshinden/)
    - [Battlecruiser 3000AD (1996)](https://www.mobygames.com/game/42358/battlecruiser-3000ad/)
    - [Blood (1997)](https://www.mobygames.com/game/980/blood/)
    - [Burnout: Championship Drag Racing (1998)](https://www.mobygames.com/game/4544/burnout-championship-drag-racing/)
    - [Carmageddon (1997)](https://www.mobygames.com/game/367/carmageddon/)
    - [Descent II (1996)](https://www.mobygames.com/game/694/descent-ii/)
    - [Dreams to Reality (1997)](https://www.mobygames.com/game/6882/dreams-to-reality/)
    - [EuroFighter 2000 (1995)](https://www.mobygames.com/game/4478/ef-2000/)
    - [Extreme Assault (1997)](https://www.mobygames.com/game/1396/extreme-assault/)
    - [Grand Theft Auto (1997)](https://www.mobygames.com/game/417/grand-theft-auto/)
    - [Jet Fighter III (1997)](https://www.mobygames.com/game/5219/jetfighter-iii/)
    - [Lands Of Lore 2: Guardians of Destiny (1997)](https://www.mobygames.com/game/491/lands-of-lore-guardians-of-destiny/)
    - [NASCAR Racing 1999 Edition (1999)](https://www.mobygames.com/game/3223/nascar-racing-1999-edition/)
    - [NASCAR Racing 2 (1996)](https://www.mobygames.com/game/1607/nascar-racing-2/)
    - [Prost Grand Prix 1998 (1998)](https://www.mobygames.com/game/20599/prost-grand-prix-1998/)
    - [Pył (1999)](https://www.mobygames.com/game/59965/py%C5%82/)
    - [Redguard: The Elder Scroll Adventures (1998)](https://www.mobygames.com/game/972/the-elder-scrolls-adventures-redguard/)
    - [Screamer 2 (1996)](https://www.mobygames.com/game/375/screamer-2/)
    - [Shadow Warrior (1997)](https://www.mobygames.com/game/387/shadow-warrior/)
    - [Starfighter 3000 (1996)](https://www.mobygames.com/game/26049/star-fighter/)
    - [Tie Break Tennis 98 (1998)](https://www.mobygames.com/game/34251/extreme-tennis/)
    - [Tomb Raider (1996)](https://www.mobygames.com/game/348/tomb-raider/)
    - [UEFA Champions League 1996/97 (1997)](https://www.mobygames.com/game/41198/uefa-champions-league-199697/)
    - [VR Soccer '96 (1995)](https://www.mobygames.com/game/1190/vr-soccer-96/)
    - [Whiplash (1995)](https://www.mobygames.com/game/685/whiplash/)
    - [XCar: Experimental Racing (1997)](https://www.mobygames.com/game/6481/xcar-experimental-racing/)

    </div>


## Configuration settings

You can set the 3dfx Voodoo parameters in the `[voodoo]` configuration
section.


##### voodoo

:   Enable 3dfx Voodoo emulation. This is authentic low-level emulation of
    the Voodoo card without any OpenGL passthrough, so it requires a powerful
    CPU. Most games need the DOS Glide driver called `GLIDE2X.OVL` to be in
    the path for 3dfx mode to work. Many games include their own Glide driver
    variants, but for some you need to provide a suitable `GLIDE2X.OVL`
    version. A small number of games integrate the Glide driver into their
    code, so they don't need `GLIDE2X.OVL`.

    Possible values: `on` *default*{ .default }, `off`


##### voodoo_memsize

:   Set the amount of video memory for 3dfx Voodoo graphics. The memory is
    used by the Frame Buffer Interface (FBI) and Texture Mapping Unit (TMU).

    Possible values:

    <div class="compact" markdown>

    - `4` *default*{ .default } -- 2 MB for the FBI and one TMU with 2 MB.
    - `12` -- 4 MB for the FBI and two TMUs, each with 4 MB.

    </div>


##### voodoo_threads

:   Use threads to improve 3dfx Voodoo performance.

    Possible values:

    - `auto` *default*{ .default } -- Use up to 16 threads based on
      available CPU cores.
    - `<number>` -- Set a specific number of threads between 1 and 128.

    !!! note

        Setting this to a higher value than the number of logical CPUs your
        hardware supports is very likely to harm performance. This has been
        measured to scale well up to 8--16 threads, but it has not been tested
        on a many-core CPU. If you have a Threadripper or similar CPU, please
        let us know how it goes.


##### voodoo_bilinear_filtering

:   Use bilinear filtering to emulate the 3dfx Voodoo's texture smoothing
    effect. This is enabled by default to match the look of real Voodoo
    hardware, which always applied bilinear texture filtering. Disabling it
    gives a sharper but less authentic look, and may help performance on
    slower systems.

    Possible values: `on` *default*{ .default }, `off`


