@ECHO OFF

REM SPDX-License-Identifier: GPL-2.0-or-later
REM
REM Copyright (C) 2023-2025 The DOSBox Staging Team

SET script_dir=%~dp0
SET translation_dir=%~dp0\..\..\resources\translations
PUSHD "%translation_dir%"

IF NOT EXIST %script_dir%\tools\uconv.exe GOTO readme
IF NOT EXIST %script_dir%\tools\dos2unix.exe GOTO readme

ECHO In directory %translation_dir%:

FOR %%f IN (*.po) DO (
	CALL :nconv_t "%%f"
)

PUSHD "%script_dir%"
PAUSE
EXIT /B %ERRORLEVEL%

REM Normalization - for UTF-8 format locale

:nconv_t
ECHO Normalizing file %~1
%script_dir%\tools\uconv -f UTF-8 -t UTF-8 -x nfc "%~1" -o "~work.tmp"
%script_dir%\tools\dos2unix "~work.tmp" > NUL
MOVE /Y "~work.tmp" "%~1" > NUL
EXIT /B 0

:readme
PUSHD "%script_dir%"
ECHO.
ECHO You need to download the following tools:
ECHO - uconv (contained in the ICU package): https://github.com/unicode-org/icu/releases/latest
ECHO - dos2unix: https://dos2unix.sourceforge.io/
ECHO.
ECHO Extract the following files into the "\tools" subdirectory:
ECHO - uconv.exe (and its dlls)
ECHO - dos2unix.exe
ECHO.
PAUSE
EXIT /B
