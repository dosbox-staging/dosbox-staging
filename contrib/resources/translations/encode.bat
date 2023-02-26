@echo off

set trans_dir=%~dp0

pushd "%trans_dir%"

echo In directory %trans_dir%:

rem (default)
rem keyb us
call :nconv_t "utf-8/en.txt" , en

rem keyb de
call :nconv_t "utf-8/de.txt" , de

rem keyb es
call :nconv_t "utf-8/es.txt" , es

rem keyb fr
call :nconv_t "utf-8/fr.txt" , fr

rem keyb it
call :nconv_t "utf-8/it.txt" , it

rem keyb nl
call :nconv_t "utf-8/nl.txt" , nl

rem keyb pl
call :nconv_t "utf-8/pl.txt" , pl

rem keyb ru
call :nconv_t "utf-8/ru.txt" , ru

exit

rem Normalization - for UTF-8 format locale
:nconv_t
echo Normalizing %~1 to %~2.lng
tools\uconv -f UTF-8 -t UTF-8 -x nfc "%~1" > "%~2.lng"
tools\dos2unix "%~2.lng"