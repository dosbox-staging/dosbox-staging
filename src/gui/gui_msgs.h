/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
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
        R"(%s, version %s

Copyright (C) 2020-2022  The DOSBox Staging Team
License: GNU GPL-2.0-or-later <https://www.gnu.org/licenses/gpl-2.0.html>

This is free software, and you are welcome to change and redistribute it under
certain conditions; please read the COPYING file thoroughly before doing so.
There is NO WARRANTY, to the extent permitted by law.
)";

constexpr char help_msg[] =
        R"(Usage: dosbox [OPTION]... [FILE]

These are the common options:

  --printconf          Print the location of the default configuration file.

  --editconf           Open the default configuration file in a text editor.

  -c <command>         Run the specified DOS command before running FILE.
                       Multiple commands can be specified.

  -noautoexec          Don't perform any [autoexec] actions.

  -noprimaryconf       Don't read settings from the primary configuration file
                       located in your user folder.

  -nolocalconf         Don't read settings from "dosbox.conf" if present in
                       the current working directory.

  -conf <configfile>   Start DOSBox with the options specified in <configfile>.
                       Multiple configfiles can be specified.

  --working-dir <path> Set working directory to <path>. DOSBox will act as if
                       started from this directory.

  -fullscreen          Start DOSBox in fullscreen mode.

  -lang <langfile>     Start DOSBox with the language specified in <langfile>.

  --list-glshaders     List available GLSL shaders and their directories.
                       Results are useable in the "glshader = " config setting.

  -machine <type>      Setup DOSBox to emulate a specific type of machine.
                       The machine type has influence on both the videocard
                       and the emulated soundcards. Valid choices are:
                       hercules, cga, cga_mono, tandy, pcjr, ega, vgaonly,
                       svga_s3 (default), svga_et3000, svga_et4000,
                       svga_paradise, vesa_nolfb, vesa_oldvbe.

  -exit                DOSBox will exit after the DOS program specified by
                       FILE has ended.

  -h, --help           Print this help message.

  --version            Print version information and exit.

You can find full list of options in the man page: dosbox(1)
And in the file: /usr/share/doc/dosbox-staging/README
)";

#endif
