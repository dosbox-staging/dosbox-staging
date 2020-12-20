@echo off
chcp 1252>nul
color F0
if exist "%~dp0\dosbox.exe" (GOTO :OK)
cls
echo.
echo.
echo.
echo    Filerne du for søger at installére skal
echo   kopieres til din "DOSBox-Program-folder".
echo    Og "%~nx0",
echo   skal startes fra "DOSBox-Program-folderen".
echo.
echo.
pause
exit

:OK
mode con cols=55 lines=35

cls
echo.
echo.
echo.
echo    I.  Åbner Config-fil og tekstfil med
echo.
echo      ny language linie, til erstatning
echo.
echo      af "language=" linien, (kopier og erstat
echo.
echo      til config filen). Gém Config-filen.
echo.
echo.
echo    C.  Åbn også den Danske config-fil. Hvis
echo.
echo      du ønsker kan du kopiére indholdet af
echo.
echo      dosbox-0.74-2_DK.conf til config filen
echo.
echo      (Dansk tekstet config-fil)(gém den
echo.
echo      originale før du erstatter den). Gém.
echo.
echo       Hvis du udover dansk config-fil ønsker
echo.
echo      "Dansk DOS", skal du også erstatte linien
echo.
echo      "language=", med linien fra "!delme.txt".

echo.
echo.
echo.

echo     x. afslut
echo.
echo.
choice /C:CI2x /N
if errorlevel 3 (exit)
if errorlevel 2 (GOTO :LANG)
start "" "%WINDIR%\notepad.exe" "%~dp0dosbox-0.74-3_DK.conf"
:LANG
echo language=%~dp0lng.0.74-3.DK>"%temp%\!delme.txt"
start "" "DOSBox.exe" -editconf notepad.exe -editconf %SystemRoot%\system32\notepad.exe -editconf %WINDIR%\notepad.exe
start /W "" "%WINDIR%\notepad.exe" "%temp%\!delme.txt"

del /Q "%temp%\!delme.txt"
exit
