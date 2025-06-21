@ECHO OFF

REM SPDX-License-Identifier: GPL-2.0-or-later
REM
REM Copyright (C) 2023-2023 The DOSBox Staging Team

SET translation_dir=%~dp0
PUSHD "%translation_dir%"

IF NOT EXIST tools\uconv.exe GOTO readme
IF NOT EXIST tools\dos2unix.exe GOTO readme

ECHO In directory %translation_dir%:

FOR %%f IN (*.lng) DO (
	CALL :nconv_t "%%f"
)

PAUSE
EXIT /B %ERRORLEVEL%

REM Normalization - for UTF-8 format locale

:nconv_t
ECHO Normalizing file %~1
tools\uconv -f UTF-8 -t UTF-8 -x nfc "%~1" -o "~work.tmp"
tools\dos2unix "~work.tmp" > NUL
MOVE /Y "~work.tmp" "%~1" > NUL
EXIT /B 0

:readme
TYPE tools\readme.txt
ECHO.
PAUSE
EXIT /B
