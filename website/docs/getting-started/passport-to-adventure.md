# Passport to Adventure


## Default configuration

By the way, how do you learn about all the available configuration parameters?
When you start DOSBox for the very first time, it creates a so called *default
configuration* file in a standard location with the name `dosbox-staging.conf`.
This file contains the full list of available parameters with their default
values, plus a short explanatory text for each.

For example, this is the description of the `windowresolution` parameter:

    Set the viewport size (drawable area) within the window/screen:
      fit:       Fit the viewport to the available window/screen (default).
      <custom>:  Limit the viewport within to a custom resolution or percentage
                 of the desktop. Specified in WxH, N%, N.M%.
                 Examples: 960x720 or 50%

Here are the locations of `dosbox-staging.conf` on each platform:

<div class="compact" markdown>

| <!-- --> | <!-- --> |
|----------|----------|
| **Windows**  | `C:\Users\<USERNAME>\AppData\Local\DOSBox\dosbox-staging.conf` |
| **macOS**    | `~/Library/Preferences/DOSBox/dosbox-staging.conf` |
| **Linux**    | `~/.config/dosbox/dosbox-staging.conf` |

!!! tip "Showing the Library folder in Finder"

     The `Library` folder is hidden by default in the macOS Finder. To enable
     it, go to your home folder in Finder, press <kbd>Cmd</kbd>+<kbd>J</kbd> to
     bring up the view options dialog, then check **Show Library Folder**.

</div>

If you know the exact name of the configuration parameter, you can display the
same explanatory text using the `config` built-in DOSBox command. The
invocation is `config -h` followed by a space and the parameter name. For
example:

![](images/config-help.png){ style="margin: 0.9rem 0;" }

It is highly recommended to look up the descriptions of the various
configuration parameters as you encounter them in this guide. That's a good
way to get gradually aquainted with them and further your knowledge about the
available options.



