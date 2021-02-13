/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_GUI_MSGS_H
#define DOSBOX_GUI_MSGS_H

constexpr char version_msg[] =
        R"(dosbox (dosbox-staging), version %s

Copyright (C) 2020-2021  The DOSBox Staging Team
License: GNU GPL-2.0-or-later <https://www.gnu.org/licenses/gpl-2.0.html>

This is free software, and you are welcome to change and redistribute it under
certain conditions; please read the COPYING file thoroughly before doing so.
There is NO WARRANTY, to the extent permitted by law.
)";

constexpr char help_msg[] =
        R"(Usage: dosbox [OPTION]... [FILE]

These are common options:

  -h, --help          Displays this message.

  --printconf         Prints the location of the default configuration file.

  --editconf          Open the default configuration file in a text editor.

  -c <command>        Runs the specified DOS command before running FILE.
                      Multiple commands can be specified.

  -conf <configfile>  Start dosbox with the options specified in <configfile>.
                      Multiple configfiles can be present at the commandline.

  -userconf           Load the configuration file located in
                      ~/.config/dosbox. Can be combined with the -conf
                      option.

  -fullscreen         Start dosbox in fullscreen mode.

  -lang <langfile>    Start dosbox with the language specified in
                      <langfile>.

  -machine <type>     Setup dosbox to emulate a specific type of machine.
                      The machine type has influence on both the videocard
                      and the emulated soundcards.  Valid choices are:
                      hercules, cga, cga_mono, tandy, pcjr, ega, vgaonly,
                      svga_s3 (default), svga_et3000, svga_et4000,
                      svga_paradise, vesa_nolfb, vesa_oldvbe.

  -exit               Dosbox will close itself when the DOS program
                      specified by FILE ends.

  -v, --version       Output version information and exit.

You can find full list of options in the man page: dosbox(1)
And in file: /usr/share/doc/dosbox-staging/README
)";

#endif
