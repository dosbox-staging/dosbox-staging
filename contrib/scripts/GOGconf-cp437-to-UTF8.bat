@ECHO OFF

REM SPDX-License-Identifier: GPL-2.0-or-later
REM
REM Copyright (C) 2023-2023  The DOSBox Staging Team

REM This batch file converts GOG config files,
REM which use the old 437 code page, to UTF-8.
REM Simply copy the .conf files you want to convert to 
REM the same folder as this file and run this file.

IF NOT "%ComSpec%"=="C:\WINDOWS\system32\cmd.exe" GOTO os_error

SET conversion_dir=%~dp0
PUSHD "%conversion_dir%"

IF NOT EXIST tools\uconv.exe GOTO readme

ECHO In directory %conversion_dir%:

FOR %%f IN (*.conf) DO (
	CALL :nconv_t "%%f"
)

PAUSE
EXIT /B %ERRORLEVEL%

:nconv_t
ECHO Converting file %~1
tools\uconv -f 437 -t UTF-8 -x nfc "%~1" -o "%~n1.tmp"
MOVE /Y "%~n1.tmp" "%~1" > NUL
EXIT /B 0

:readme
TYPE tools\readme-uconv.txt
ECHO.
PAUSE
EXIT /B

:os_error
ECHO You are using this batch program on MS-DOS or Windows 9x.
ECHO Please use a more modern version of Windows.
ECHO.
PAUSE
