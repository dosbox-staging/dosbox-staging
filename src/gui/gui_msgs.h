/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2023  The DOSBox Staging Team
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

Copyright (C) 2020-2023  The DOSBox Staging Team
License: GNU GPL-2.0-or-later <https://www.gnu.org/licenses/gpl-2.0.html>

This is free software, and you are welcome to change and redistribute it under
certain conditions; please read the COPYING file thoroughly before doing so.
There is NO WARRANTY, to the extent permitted by law.
)";

constexpr char help_msg[] =
        R"(Usage: dosbox [OPTION]... [FILE]

List of available options:

  --conf <config_file>     Start with the options specified in <config_file>.
                           Multiple configuration files can be specified.
                           Example: --conf conf1.conf --conf conf2.conf

  --printconf              Print the location of the primary configuration file.

  --editconf               Open the primary configuration file in a text editor.

  --eraseconf              Delete the primary configuration file.

  --noprimaryconf          Don't load or create the the primary configuration file.

  --nolocalconf            Don't load the local "dosbox.conf" configuration file
                           if present in the current working directory.

  --set <setting>=<value>  Set a configuration setting. Multiple configuration
                           settings can be specified. Example:
                           --set mididevice=fluidsynth --set soundfont=mysoundfont.sf2

  --working-dir <path>     Set working directory to <path>. DOSBox will act as if
                           started from this directory.

  --list-glshaders         List all available OpenGL shaders and their paths.
                           Results are useable in the "glshader = " config setting.

  --fullscreen             Start in fullscreen mode.

  --lang <lang_file>       Start with the language specified in <lang_file>.

  --machine <type>         Emulate a specific type of machine. The machine type has
                           influence on both the emulated video and sound cards.
                           Valid choices are: hercules, cga, cga_mono, tandy,
                           pcjr, ega, svga_s3 (default), svga_et3000, svga_et4000,
                           svga_paradise, vesa_nolfb, vesa_oldvbe.

  -c <command>             Run the specified DOS command before running FILE.
                           Multiple commands can be specified.

  --noautoexec             Don't execute DOS commands from any [autoexec] sections.

  --exit                   Exit after the DOS program specified by FILE has ended.
                           If no FILE has been specified, execute [autoexec] and
                           terminate.

  --startmapper            Run the mapper GUI.

  --erasemapper            Delete the default mapper file.

  --securemode             Enable secure mode. The [config] and [autoexec] sections
                           of the loaded configurations will be ignored, and
                           commands such as MOUNT and IMGMOUNT are disabled.

  --socket <num>           Run nullmodem on the specified socket number.

  -h, --help               Print this help message and exit.

  -v, --version            Print version information and exit.
)";

#endif
