!define VER_MAYOR 0
!define VER_MINOR 62

; The name of the installer
Name "DOSBox ${VER_MAYOR}.${VER_MINOR} Installer"

; The file to write
OutFile "Dosbox${VER_MAYOR}.${VER_MINOR}-win32-installer.exe"

; The default installation directory
InstallDir "$PROGRAMFILES\DOSBox-${VER_MAYOR}.${VER_MINOR}"

; The text to prompt the user to enter a directory
DirText "This will install DOSBox v${VER_MAYOR}.${VER_MINOR} on your computer. Choose a directory"
SetCompressor bzip2

LicenseData COPYING
LicenseText "DOSBox v${VER_MAYOR}.${VER_MINOR} License" "Next >"

; The stuff to install
Section "ThisNameIsIgnoredSoWhyBother?"
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR

  ; Put file there
  File /oname=README.txt README
  File /oname=COPYING.txt COPYING
  File /oname=THANKS.txt THANKS
  File /oname=NEWS.txt NEWS
  File /oname=AUTHORS.txt AUTHORS
  File /oname=INSTALL.txt INSTALL
  File DOSBox.exe
  File dosbox.conf
  File SDL.dll
  File SDL_net.dll
  File libpng12.dll

  CreateDirectory "$INSTDIR\capture"
  CreateDirectory "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}"
  CreateShortCut "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\DOSBox.lnk" "$INSTDIR\DOSBox.exe" "-conf $\"$INSTDIR\dosbox.conf$\""
  CreateShortCut "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\README.lnk" "$INSTDIR\README.txt"
  CreateShortCut "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\DOSBox.conf.lnk" "notepad.exe" "$INSTDIR\dosbox.conf"

WriteUninstaller "uninstall.exe"

SectionEnd ; end the section

UninstallText "This will uninstall DOSBox  v${VER_MAYOR}.${VER_MINOR}. Hit next to continue."

Section "Uninstall"
  ; remove registry keys
  ; remove files
  Delete $INSTDIR\README.txt
  Delete $INSTDIR\COPYING.txt
  Delete $INSTDIR\THANKS.txt
  Delete $INSTDIR\NEWS.txt
  Delete $INSTDIR\AUTHORS.txt
  Delete $INSTDIR\INSTALL.txt
  Delete $INSTDIR\DOSBox.exe
  Delete $INSTDIR\dosbox.conf
  Delete $INSTDIR\SDL.dll
  Delete $INSTDIR\SDL_net.dll
  Delete $INSTDIR\libpng12.dll
  ;Files left by sdl taking over the console
  Delete $INSTDIR\stdout.txt
  Delete $INSTDIR\stderr.txt

  ; MUST REMOVE UNINSTALLER, too
  Delete $INSTDIR\uninstall.exe

  ; remove shortcuts, if any.
  Delete "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\Uninstall.lnk"
  Delete "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\README.lnk"
  Delete "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\DOSBox.lnk"
  Delete "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\DOSBox.conf.lnk"  

; remove directories used.
  RMDir "$INSTDIR\capture"
  RMDir "$INSTDIR"
  RMDir "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}"
SectionEnd

; eof
