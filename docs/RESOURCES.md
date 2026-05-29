# Resources

Starting with version 0.79, DOSBox Staging needs access to bundled resource
files.

If you're using one of the project-provided install packages (zip, dmg, Windows
installer) - then there is no action necessary; read on if you're curious
though.

If you build from source or are a developer, then you can simply run your
executable directly from your build area, with no further action needed (feel
free to use an alias or symlink to make that easier).

If you plan to install or move DOSBox Staging from its build area, then the
`"resources"` directory needs to come along with the binary.

## How to bundle the resources

The `"resources"` can be provided in two ways:

1. Along-side the executable, as a portable package:

    Visual Studio both populates the build area with the compiled
    `"dosbox"` executable (plus DLLs on Windows) and the `"resources"`
    directory relative to it, which together form a stand-alone and portable
    application.

    This group can be zipped or moved together, and is how we ship the Linux,
    Windows, and macOS packages built by the CI systems.

2. Pointed to by the `XDG_DATA_HOME` or `XDG_DATA_DIRS` paths.


## How the resources are found during runtime

At runtime, the executable will check the following paths for the resources, in
the following priority order:

1. Beside the executable:

    `dosbox` (executable) `"resources"subdirs/...` (on
    macOS:`../Resources/subdirs/...`)

    This first instance should also be the prefered packaging layout for wrapped
    formats like FlatPak, Snap, AppImage, etc.

2. Up one directory from the executable (which allows unit tests to access
   resources):

    `dosbox` `../`"resources"`subdirs/...` (on macOS:
    `../../Resources/subdirs/...`)

3. In `XDG_DATA_HOME` followed by the `XDG_DATA_DIRS` paths, where:

    `${XDG_DATA}/dosbox-staging/subdirs/...`

4. In the user's configuration path:

    `home/<user>/.config/dosbox/subdirs/...` (or the Windows configuration path)

## FAQ

Q: Why can't these be embedded inside the executable?

A1: Encoding binary data into the project's source tree as massive hex strings
is a form of obfuscation that makes it harder to understand what they contain.

With raw files, anyone can use those files as intended by 3rd party software,
diff or compare them, notify the project when they become outdated, and more
importantly can maintain them without needing to setup a development
environment.

A2: Turning binary files into source involves placing tens of thousands of lines
of hex values into the code base.

These get parsed and become a persistent carrying "load" for source editors and
development IDEs. They generate false-positives when grepping the source for
number patterns, so much so that they often flood the screen and need extra
effort to filter them out.

Q: How are the `"resources"` deployed into the build area by Visual Studio?

A: Search the vcxproj files for `"contrib\resources"`. These snippets perform
the recursive copying.
