/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2020  The dosbox-staging team
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
Copyright (C) 2020 The dosbox-staging team.
License GPLv2+: GNU GPL version 2 or later
<https://www.gnu.org/licenses/gpl-2.0.html>

This is free software, and you are welcome to change and redistribute it
under certain conditions; please read the COPYING file thoroughly before
doing so.  There is NO WARRANTY, to the extent permitted by law.

This program (dosbox-staging) is modified version of DOSBox.
Copyright (C) 2020 The DOSBox Team, published under GNU GPLv2+
Read AUTHORS file for more details.
)";

constexpr char help_msg[] =
        R"(Usage: dosbox [-fullscreen] [-startmapper] [-noautoexec] [-securemode]
[-user‐conf] [-scaler scaler|-forcescaler scaler] [-conf configfile]
[-lang langfile] [-machine machinetype] [-socket socketnumber] [-c command]
[-exit] [-v, --version] [-h, --help] [NAME]
Options:
  -h, --help              Displays this message.
  -fullscreen             Start dosbox in fullscreen mode.
  -startmapper            Start the internal keymapper on startup of dosbox.
  -noautoexec             Skips the [autoexec] section of the loaded
                          configuration file.

  -securemode             Same as -noautoexec, but adds config.com -securemode
                          at the end of AUTOEXEC.BAT

  -userconf               Load the configuration file located in
                          ~/.config/dosbox. Can be combined with the -conf
                          option.

  -scaler <scaler>        Uses the graphical scaler specified by <scaler>.
  -forcescaler <scaler>   Similar to the -scaler parameter, but tries to force
                          usage of the specified scaler even if it might not
                          fit.

  -conf <configfile>      Start dosbox with the options specified in
                          <configfile>. Multiple configfiles can be present at
                          the commandline.

  -lang <langfile>        Start dosbox with the language specified in
                          <langfile>.

  -machine {hercules|cga|tandy|pcjr|ega|vgaonly|svga_s3|svga_et3000|
            svga_et4000|svga_paradise|vesa_nolfb|vesa_oldvbe}

                          Setup dosbox to emulate a specific type of machine.
                          The default value is 'svga_s3'. The machinetype has
                          influence on both the videocard and the available
                          soundcards.

  -socket <socketnumber>  Passes the socket number <socketnumber> to the
                          nullmodem emulation. See README for details.

  -c <command>            Runs the specified command before running file.
                          Multiple commands can be specified.

  -exit                   Dosbox will close itself when the DOS program
                          specified by file ends.

  -v, --version           Output version information and exit.
)";

#endif
