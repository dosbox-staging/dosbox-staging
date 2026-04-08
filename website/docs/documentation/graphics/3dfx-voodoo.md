# 3dfx Voodoo


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


##### voodoo_bilinear_filtering

:   Use bilinear filtering to emulate the 3dfx Voodoo's texture smoothing
    effect. Bilinear filtering can impact frame rates on slower systems; try
    turning it off if you're not getting adequate performance.

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
        hardware supports is very likely to harm performance.
