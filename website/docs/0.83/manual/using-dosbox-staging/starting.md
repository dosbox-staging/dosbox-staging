---
toc_depth: 3
---

# Starting the emulator

Before continuing, it's helpful to understand that DOSBox Staging can be used
in many different ways. Simply launching the emulator by itself won't usually
be enough to run a game --- you'll be presented with the [DOS
prompt](shell.md) where you can execute [DOS commands](commands.md), but
before you can run most software you'll need to [mount](storage.md) your game
files and may also need to configure the emulator for the game you're playing.

For most users, the recommended approach is to organise each game in its own
folder and launch DOSBox Staging from that folder. If you're new to DOSBox
Staging, we recommend reading the [Getting Started
guide](../../getting-started/introduction.md) first. It introduces the
recommended per-game workflow through practical examples.

The next section explains the concept of the [working
directory](#the-working-directory), which underpins the recommended workflow.
The remainder of this page then describes the platform-specific ways to launch
DOSBox Staging with the correct working directory.


## The working directory

The **working directory** is the folder from which DOSBox Staging is launched.
For the recommended per-game workflow, this is simply the game's folder.

When DOSBox Staging starts, it uses the working directory to discover
game-specific resources. Depending on your setup, it can:

<div class="compact" markdown>
- [automatically mount](storage.md#automounting) the game's files as DOS
  drives;
- load a local [`dosbox.conf`](configuration.md#local-configuration) with
  game-specific settings;
- resolve relative paths used by the [configuration](configuration.md) and other files.
</div>

The launch methods described below are simply different ways of starting
DOSBox Staging with the correct working directory. See the
[Storage](storage.md) and [Configuration](configuration.md) sections for
further details.

!!! note "Terminology"

    DOS refers to folders as **directories**, while modern operating systems
    generally use the term **folder**. They refer to the same thing, and
    throughout this manual we use the two terms interchangeably.


## Windows

### Start from Windows explorer

!!! warning "Windows 11 note"

    The Windows Explorer context menu integration only works in
    Windows 8 and 10 currently.

If you have used the installer with the default options to set up DOSBox
Staging, simply right-click on a folder in Windows Explorer and select **Open
with DOSBox Staging** in the context menu to launch DOSBox Staging with that
folder as the [working directory](#the-working-directory).

Alternatively, navigate to the folder in Windows Explorer, right-click any
empty area inside the folder, and then select **Open with DOSBox Staging** in
the context menu.

### Start using launch icons

1. Create a batch file called `Start DOSBox Staging.bat` with the following
    content:

    ```
    C:\Users\%USERNAME%\AppData\Local\DOSBox\dosbox.exe
    ```

    That's the default installation path chosen by the installer. `%USERNAME%`
    is your Windows user name. If you have installed DOSBox Staging to a
    different folder (perhaps by using the portable ZIP package), you should
    adjust the path accordingly.

2. Copy this batch file into your individual game folders and rename them to
   the names of the games (e.g., `Prince of Persia.bat`).

3. Right-click on the batch file icon and select **Send to --> Desktop (create
   shortcut)** in the context menu.

4. Now you can double-click on the new **Prince of Persia.bat - Shortcut**
   icon on your desktop to start the game (of course, you can rename the icon
   to **Prince of Persia** or whatever you like; this won't change the name of
   the batch file it references).

Alternatively, you can create the shortcut manually, enter the path to the
DOSBox Staging executable, then either set the startup folder to the
game's folder, or pass it to the emulator with the
[`--working-dir`](command-line.md#-working-dir-path) option.


### Start from the command line

1. Create a batch file called `dosbox.bat` or just `db.bat` that launches DOSBox
   Staging:

      ```
      C:\Users\%USERNAME%\AppData\Local\DOSBox\dosbox.exe %*
      ```

    That's the default installation path chosen by the installer. `%USERNAME%`
    is your Windows user name. If you have installed DOSBox Staging to a
    different folder (perhaps by using the portable ZIP package), you should
    adjust the path accordingly.

2. Put this batch file in a folder that's in your path. Alternatively, you can
   add the folder where the DOSBox Staging executable lives to your path
   directly (but make sure there are no other DOSBox variants in your path
   already).

3. If you use a file manager such as **Total Commander**, you can simply
   navigate to your game folder and type `dosbox` (or `db`) then press
   ++enter++ to start the game. You can also use the command prompt to
   navigate to the game's folder manually then start the batch file, but
   that's less convenient.


### Windows Defender

Windows Defender (the built-in Windows antivirus starting from Windows 8)
might prevent you from running DOSBox Staging when you start it for the first
time. This is a false positive; DOSBox Staging has no malicious code
whatsoever, it's simply that antivirus software often mistakenly flags
emulators as "malware" because they can run other programs.

If this happens, please follow the steps below to grant Windows permission to
run DOSBox Staging. You may need to repeat these steps after upgrading to a
newer version.

!!! info "Explanation"

    Starting from Windows 8, [Microsoft Defender
    SmartScreen's](https://docs.microsoft.com/en-us/windows/security/threat-protection/windows-defender-smartscreen/windows-defender-smartscreen-overview)
    pop-up encumbers the execution of newly-developed applications.  To
    prevent this, developers are expected to pay
    Microsoft's [EV
    certification](https://docs.microsoft.com/en-gb/archive/blogs/ie/microsoft-smartscreen-extended-validation-ev-code-signing-certificates)
    vendors a yearly fee (300 to 500 USD) and put the software on Windows Store.

    As DOSBox Staging is a volunteer effort, we are not in a position to make such
    payments. We therefore ask users to manually unblock DOSBox Staging and be
    patient while Microsoft's Application Reputation Scheme eventually whitelists
    DOSBox Staging.


#### Method 1

Start the application, then click on **More info** in the dialog that opens.
Click the **Run anyway** button in the second dialog.

<div class="image-grid" markdown>

{{ figure(
    "images/smartscreen1.png", alt="SmartScreen window 1", small=False
) }}

{{ figure(
    "images/smartscreen2.png", alt="SmartScreen window 2", small=False
) }}

</div>

Consider also performing [Method 3](#method-3) to make DOSBox Staging start up
faster.


#### Method 2

In your installation folder, right-click on `dosbox.exe`, select **Properties**, tick
**Unblock** in the dialog that opens, then press **OK**.

{{ figure(
    "images/properties.png", alt="Properties window", small=False
) }}

Consider also performing [Method 3](#method-3) to make DOSBox Staging start up
faster.


#### Method 3

Add an exclusion to Windows Security to whitelist the DOSBox Staging
executable or the folder in which it resides. We recommend doing so even if
you've already performed either of the previous methods, as it can eliminate
the 3--5 second startup delay caused by the real-time antivirus scan.

See the steps on how to do this [here](https://support.microsoft.com/en-au/windows/add-an-exclusion-to-windows-security-811816c0-4dfd-af4a-47e4-c301afe13b26)
and [here](https://docs.rackspace.com/docs/set-windows-defender-folder-exclusions).


## macOS 

### Using document packages

The easiest way to launch DOSBox Staging on macOS is to use document packages.
Simply add the **.dosbox** extension to a game folder's name in Finder (e.g.,
**Prince of Persia** becomes **Prince of Persia.dosbox**), then double-click
it to launch DOSBox Staging.

### Start using launch icons

1. Mount the DOSBox Staging installer `.dmg` image file.

2. Copy the **Start DOSBox Staging** icon from the window that appears into
   your game folder (e.g., `Prince of Persia`).

3. Right-click or ++ctrl++-click the icon, then select the topmost
   **Open** menu item.

4. A dialog with the following text will appear: *macOS cannot verify the
   developer of "Start DOSBox Staging". Are you sure you want to open it?*

5. Click the *Open* button.

6. A dialog will open, asking for permission to allow DOSBox Staging access
   to your Documents folder. Click the **OK** button.

You only need to perform this procedure the first time you open the **Start
DOSBox Staging**. After the first launch, you can use it like any other
regular icon.

It's recommended to rename the **Start DOSBox Staging** icons in the individual
game folders to the names of the games; then, you can use Spotlight Search to
start a game.

For example, rename **Start DOSBox Staging** in the `Prince of Persia` folder to
**Prince of Persia**. Start Spotlight Search by pressing ++cmd+space++, then
type in "Prince". The **Prince of Persia** icon will show up in the search
results --- you can simply press ++enter++ on it to launch the game.

!!! tip

    You don't need to repeat this procedure for every new game. Once you've
    opened an icon once, just copy that same icon into any future game folder
    --- no need to fetch a fresh one from the `.dmg` each time.


### Apple Gatekeeper

For releases before 0.81.0 and the [development snapshot
builds](../../../releases/development-builds.md), you'll need to do the
following when launching DOSBox Staging for the first time. Later releases are
all notarized — they'll just work.

You'll need to repeat these steps after upgrading to a more recent development
build.

!!! info "Explanation"

    Apple's Gatekeeper feature only permits the running of notarized software, one
    aspect of which involves developers making yearly payments to Apple.

    As DOSBox Staging is a volunteer effort, we were not in a position to make
    such payments prior to version 0.81.0, and therefore asked users to bypass
    Apple's Gatekeeper manually.

    Read the first section of [this Apple Support
    article](https://support.apple.com/en-us/102445) for further info.


#### macOS Sequoia 15

- Open the **DOSBox Staging** app, then click **Done** in the dialog that
  opens.
- Follow [these instructions](https://support.apple.com/en-us/102445#openanyway) to allow
  launching DOSBox Staging in the **Privacy & Security** tab in **System
  Settings**.

#### macOS Sonoma 14 or earlier

- ++ctrl++ click (or right-click) on the **DOSBox Staging** app, then click
  **Open**.
- Click **OK** to close the dialog that opens.
- Open the app a second time --- now Gatekeeper will show an
  **Open** button. Press this to launch DOSBox Staging.

{{ figure(
    "images/gatekeeper.png", alt="Gatekeeper window", small=False, width="400"
) }}


## Linux

### Start from the command line

Open your favourite terminal and `cd` into your game folder, then run the
`dosbox` command from there.

Use the `--version` argument to check that you're running DOSBox Staging and
not some other DOSBox variant:

    % dosbox --version
    dosbox-staging, version 0.83.0

    Copyright (C) 2020-2026  The DOSBox Staging Team
    License: GNU GPL-2.0-or-later <https://www.gnu.org/licenses/gpl-2.0.html>

Alternatively, create a shell script or a shortcut on your desktop that
executes the following command:

    dosbox --working-dir <PATH>

`<PATH>` is the absolute path of your game folder.


### Start using launch icons

The easiest way is to create a shell script with the following content (modify
the path passed in with the
[`--working-dir`](command-line.md#-working-dir-path) option so it points to
your game folder):

```bash
#!/bin/bash
dosbox --working-dir "$HOME/Documents/DOS Games/Prince of Persia"
```

Then, create an icon on your desktop that launches this script, or start it
using your desktop environment's preferred launcher.

