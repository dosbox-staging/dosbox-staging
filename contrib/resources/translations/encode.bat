@ECHO OFF

SET trans_dir=%~dp0
PUSHD "%trans_dir%"
ECHO In directory %trans_dir%:

REM (default)
REM keyb us
CALL :nconv_t "utf-8\en.txt", en

REM keyb de
CALL :nconv_t "utf-8\de.txt", de

REM keyb es
CALL :nconv_t "utf-8\es.txt", es

REM keyb fr
CALL :nconv_t "utf-8\fr.txt", fr

REM keyb it
CALL :nconv_t "utf-8\it.txt", it

REM keyb nl
CALL :nconv_t "utf-8\nl.txt", nl

REM keyb pl
CALL :nconv_t "utf-8\pl.txt", pl

REM keyb ru
CALL :nconv_t "utf-8\ru.txt", ru

PAUSE
EXIT /B %ERRORLEVEL%

REM Normalization - for UTF-8 format locale
:nconv_t
ECHO Normalizing %~1 to %~2.lng
tools\uconv -f UTF-8 -t UTF-8 -x nfc "%~1" > "%~2.lng"
tools\dos2unix "%~2.lng"
EXIT /B 0