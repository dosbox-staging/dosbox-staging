# macOS DMG disk image guide

## Create DMG CI step

The macOS DMG disk image is built by the "Package" and "Create dmg" steps of
the [.github/workflows/macos.yml](/.github/workflows/macos.yml) GitHub action:

1. The "Package" step calls [scripts/create-package.sh](/scripts/create-package.sh).
   The `pkg_macos` function of that script prepares the contents of the DMG
   file, so if you want to make changes to it (e.g. add/rename files), this is
   the function to modify.

2. The "Create dmg" step then creates the actual DMG disk image.


## macOS DMG disk image customisations

We're doing a few nifty things here to customise the looks of the Finder
window that appears when opening the macOS `.dmg` disk image.

The `DS_Store` file stores the visual appearance of the folder, including its
size, the position and size of the icons it contains, and a link to the
background image (among a few other things).


### If you only want to change the background image

1. Make edits to `contrib/macos/background/background.svg` in Inkscape. Make
   sure you have the font [Open Sans][open-sans] installed.

   [open-sans]: https://fonts.google.com/specimen/Open+Sans

2. Export it twice using the "Page" export mode to `contrib/macos/background`
   at 96 and 192 dpi as `background-1x.png` and `background-2x.png`,
   respectively.

3. In `contrib/macos/background` run `merge-background.sh` to re-generate
   `background.tiff`, which is a multi-resolution TIFF file (needed for Retina
   support)

4. Commit & push the new background image, and let the GitHub CI workflow
   build the new DMG image.


### To fully customise the appearance of the Finder window

1. Download the latest DMG image from the GitHub CI workflow.

2. Create a writeable copy of it with the following command:

    ```
    hdiutil convert <dmg-file>.dmg -format UDRW -o dosbox-rw.dmg
    ```

3. Mount the writeable `dosbox-rw.dmg` in the Finder.

4. Customise the appearance of the root folder of the mounted disk image as
   you like. [This guide][dmg-guide] explains the process of setting up a
   background image from scratch in great detail, and contains some very
   useful tips (there are a few gotchas that are hard to figure out).

   [dmg-guide]: https://www.ej-technologies.com/resources/install4j/help/doc/concepts/dmgStyling.html

5. When you're done, copy the hidden `.DS_Store` from the root of the mounted
   volume and overwrite `contrib/macos/DS_Store` with it.

6. Commit & push the new background image, and let the GitHub CI workflow
   build the new DMG image.

