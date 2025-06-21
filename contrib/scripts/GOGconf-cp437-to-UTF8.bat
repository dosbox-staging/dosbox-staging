@ECHO OFF

REM SPDX-License-Identifier: GPL-2.0-or-later
REM
REM Copyright (C) 2023-2023 The DOSBox Staging Team

REM This batch file converts GOG config files,
REM which use the old 437 code page, to UTF-8.
REM Syntax:
REM         GOGconf-cp437-to-UTF8 [args] PATH
REM         GOGconf-cp437-to-UTF8 [args] "PATH_WITH_SPACES"
REM [args] can be:
REM -    /R or /r Recursive search through all subdirectories.
REM -    /?       Show the help screen.

IF NOT "%ComSpec%"=="C:\WINDOWS\system32\cmd.exe" GOTO os_error

setlocal enabledelayedexpansion
PUSHD %~dp0

IF NOT EXIST tools\uconv.exe GOTO readme
IF NOT EXIST tools\icudt*.dll GOTO readme
IF NOT EXIST tools\icuin*.dll GOTO readme
IF NOT EXIST tools\icuuc*.dll GOTO readme
IF NOT EXIST tools\file.exe GOTO readme
IF NOT EXIST tools\magic1.dll GOTO readme
IF NOT EXIST tools\regex2.dll GOTO readme
IF NOT EXIST tools\zlib1.dll GOTO readme
IF NOT EXIST tools\magic GOTO readme

SET argCount=0
FOR %%r IN (%*) DO (
    SET /A argCount+=1
)
IF %argCount% GTR 2 (
    GOTO help
)

IF NOT "%~2"=="" IF /I NOT "%~1"=="/R" (
    GOTO help
)

IF "%~1"=="/?" (
    GOTO help
)

IF /I "%~1"=="/R" (
    SET recursive=true
) ELSE (
    SET recursive=false
)

IF "%~1"=="" (
    CALL :menu
) ELSE (
    SET conversion_dir=%~1
)

IF NOT "%~2"=="" (
    SET conversion_dir=%~2
)

IF NOT EXIST "%conversion_dir%" (
    GOTO dir_not_found
)

ECHO In directory %conversion_dir%:

IF "%recursive%"=="false" (
    CALL :normal_search
) ELSE (
    CALL :recursive_search
)

PAUSE
EXIT /B %ERRORLEVEL%

:menu
ECHO 1. Normal search from the directory of your GOG game (e.g. D:\GAMES\DOOM)
ECHO 2. Recursive search from the root directory of your GOG games (e.g. D:\GAMES)
CHOICE /C 12 /N /M "Enter choice (1,2): "
IF "%ERRORLEVEL%"=="1" SET recursive=false
IF "%ERRORLEVEL%"=="2" SET recursive=true
SET /P conversion_dir="Enter path: "
ECHO.
EXIT /B 0

:normal_search
SET count=0
SET countSkipped=0
FOR %%f IN ("%conversion_dir%\dosbox*.conf") DO (   
    FOR /F "tokens=*" %%a IN ('tools\file --mime-encoding --brief "%%f" -m tools\magic') DO (
        SET enc=%%a
    )
    IF NOT "!enc!"=="utf-8" CALL :nconv_t "%%f"
    IF NOT "!enc!"=="utf-8" SET /A count+=1
    IF "!enc!"=="utf-8" SET /A countSkipped+=1
)
IF %count% GTR 0 (
    ECHO %count% file/s converted, %countSkipped% file/s skipped.
) ELSE (
    ECHO No files converted.
)
EXIT /B 0

:recursive_search
SET count=0
SET countSkipped=0
FOR /R "%conversion_dir%" %%f IN (dosbox*.conf) DO (
    FOR /F "tokens=*" %%a IN ('tools\file --mime-encoding --brief "%%f" -m tools\magic') DO (
        SET enc=%%a
    )
    IF NOT "!enc!"=="utf-8" CALL :nconv_t "%%f"
    IF NOT "!enc!"=="utf-8" SET /A count+=1
    IF "!enc!"=="utf-8" SET /A countSkipped+=1
)
IF %count% GTR 0 (
    ECHO %count% file/s converted, %countSkipped% file/s skipped.
) ELSE (
    ECHO No files converted.
)
EXIT /B 0

:nconv_t
ECHO Converting file %~1
tools\uconv -f 437 -t UTF-8 -x nfc "%~1" -o "%~n1.tmp"
MOVE /Y "%~n1.tmp" "%~1" > NUL
EXIT /B 0

:readme
TYPE tools\readme-gog437.txt
ECHO.
PAUSE
EXIT /B

:dir_not_found
ECHO Directory not found.
ECHO.
PAUSE
EXIT /B

:help
ECHO This batch file converts GOG config files,
ECHO which use the old 437 code page, to UTF-8.
ECHO Syntax:
ECHO         GOGconf-cp437-to-UTF8 [args] PATH
ECHO         GOGconf-cp437-to-UTF8 [args] "PATH_WITH_SPACES"
ECHO [args] can be:
ECHO -    /R or /r Recursive search through all subdirectories.
ECHO -    /?       Show the help screen.
EXIT /B

:os_error
ECHO You are using this batch program on MS-DOS or Windows 9x.
ECHO Please use a more modern version of Windows.
ECHO.
PAUSE
