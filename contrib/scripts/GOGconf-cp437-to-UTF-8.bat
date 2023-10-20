@ECHO OFF

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