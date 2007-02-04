!define VER_MAYOR 0
!define VER_MINOR 66rc1

; The name of the installer
Name "DOSBox ${VER_MAYOR}.${VER_MINOR} Installer"

; The file to write
OutFile "DOSBox${VER_MAYOR}.${VER_MINOR}-win32-installer.exe"

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
  CreateDirectory "$INSTDIR\capture"
  CreateDirectory "$INSTDIR\zmbv"
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
  File /oname=zmbv\zmbv.dll zmbv.dll
  File /oname=zmbv\zmbv.inf zmbv.inf
  File /oname=zmbv\README.txt README.video
  
  CreateDirectory "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}"
  CreateDirectory "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\Video"
  CreateShortCut "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\DOSBox.lnk" "$INSTDIR\DOSBox.exe" "-conf $\"$INSTDIR\dosbox.conf$\"" "$INSTDIR\DOSBox.exe" 0
  CreateShortCut "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\README.lnk" "$INSTDIR\README.txt"
  CreateShortCut "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\DOSBox.conf.lnk" "notepad.exe" "$INSTDIR\dosbox.conf"
  CreateShortCut "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\Capture folder.lnk" "$INSTDIR\capture"
  CreateShortCut "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\Video\Video instructions.lnk" "$INSTDIR\zmbv\README.txt"
;change outpath so the working directory gets set to zmbv
SetOutPath "$INSTDIR\zmbv"
  ; Shortcut creation depends on wether we are 9x of NT
  ClearErrors
  ReadRegStr $R0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
  IfErrors we_9x we_nt
we_nt:
  ;shortcut for win NT
  CreateShortCut "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\Video\Install movie codec.lnk" "rundll32" "setupapi,InstallHinfSection DefaultInstall 128 $INSTDIR\zmbv\zmbv.inf"
  goto end
we_9x:
  ;shortcut for we_9x
  CreateShortCut "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\Video\Install movie codec.lnk" "rundll" "setupx.dll,InstallHinfSection DefaultInstall 128 $INSTDIR\zmbv\zmbv.inf"
end:
SetOutPath $INSTDIR
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
  Delete $INSTDIR\zmbv\zmbv.dll
  Delete $INSTDIR\zmbv\zmbv.inf
  Delete $INSTDIR\zmbv\README.txt
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
  Delete "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\Capture folder.lnk"  
  Delete "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\Video\Install movie codec.lnk"
  Delete "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\Video\Video instructions.lnk"
; remove directories used.
  RMDir "$INSTDIR\zmbv"
  RMDir "$INSTDIR\capture"
  RMDir "$INSTDIR"
  RMDir "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}\Video"
  RMDir "$SMPROGRAMS\DOSBox-${VER_MAYOR}.${VER_MINOR}"
SectionEnd

; eof
