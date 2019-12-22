!define VER_MAYOR 6
!define VER_MINOR 0
!define APP_NAME "DOSBox Megabuild ${VER_MAYOR} Installer"
!define COMP_NAME "DOSBox Team and others"
!define COPYRIGHT "Copyright  2002-2009 DOSBox Team"
!define DESCRIPTION "DOSBox Installer"

VIProductVersion "${VER_MAYOR}.${VER_MINOR}.0.0"
VIAddVersionKey  "ProductName"  "${APP_NAME}"
VIAddVersionKey  "CompanyName"  "${COMP_NAME}"
VIAddVersionKey  "FileDescription"  "${DESCRIPTION}"
VIAddVersionKey  "FileVersion"  "${VER_MAYOR}.${VER_MINOR}.0.0"
VIAddVersionKey  "ProductVersion"  "${VER_MAYOR}, ${VER_MINOR}, 0, 0"
VIAddVersionKey  "LegalCopyright"  "${COPYRIGHT}"

; The name of the installer
Name "${APP_NAME}"

; The file to write
OutFile "..\bin\DOSBox_Megabuild${VER_MAYOR}-win32-installer.exe"

; The default installation directory
InstallDir "$PROGRAMFILES\DOSBox_MB${VER_MAYOR}"

; The text to prompt the user to enter a directory
DirText "This will install DOSBox Megabuild v${VER_MAYOR} on your computer. Choose a directory"
SetCompressor /solid lzma


LicenseData ..\COPYING
LicenseText "DOSBox is developed under the GPL License. This build is a development version with extra features. It is not supported by the DOSBox developers." "Next >"

; Else vista enables compatibility mode
RequestExecutionLevel admin
; Shortcuts in all users

ComponentText "Select components for DOSBox"
; The stuff to install
Section "!Core files" Core
SetShellVarContext all

  ; Set output path to the installation directory.
  ClearErrors
  SetOutPath $INSTDIR
  IfErrors error_createdir
  SectionIn RO

  ; Put file there
  
  CreateDirectory "$INSTDIR\doc"
  CreateDirectory "$INSTDIR\doc\images"
  CreateDirectory "$INSTDIR\zmbv"
  File /oname=README.txt ..\README
  File /oname=COPYING.txt ..\COPYING
  File /oname=THANKS.txt ..\THANKS
  File /oname=NEWS.txt ..\NEWS
  File /oname=AUTHORS.txt ..\AUTHORS
  File /oname=INSTALL.txt ..\INSTALL
  File ..\bin\DOSBox.exe
  File ..\bin\SDL.dll
  File ..\bin\msvcr71.dll
  File ..\bin\msvcp71.dll
  File ..\bin\SDL_net.dll
  File /oname=zmbv\zmbv.dll ..\bin\zmbv.dll
  File /oname=zmbv\zmbv.inf ..\src\libs\zmbv\zmbv.inf
  File /oname=zmbv\README.txt ..\docs\README.video
  
  File /oname=doc\index.html      ..\docs\html-doc\index.html
  File /oname=doc\db_ne2000.html  ..\docs\html-doc\db_ne2000.html
  File /oname=doc\pppatch.html    ..\docs\html-doc\pppatch.html

  File /oname=doc\images\db_ne2000patch_1.png       ..\docs\html-doc\images\db_ne2000patch_1.png
  File /oname=doc\images\db_ne2000patch_2.png       ..\docs\html-doc\images\db_ne2000patch_2.png
  File /oname=doc\images\db_ne2000patch_3.png       ..\docs\html-doc\images\db_ne2000patch_3.png
  File /oname=doc\images\db_ne2000patch_cfg1.png    ..\docs\html-doc\images\db_ne2000patch_cfg1.png
  File /oname=doc\images\db_ne2000patch_cfg2.png    ..\docs\html-doc\images\db_ne2000patch_cfg2.png
  File /oname=doc\images\db_ne2000patch_dosodi.png  ..\docs\html-doc\images\db_ne2000patch_dosodi.png
  File /oname=doc\images\db_ne2000patch_tvwin.png   ..\docs\html-doc\images\db_ne2000patch_tvwin.png
  File /oname=doc\images\db_ne2000patch_arachne.png ..\docs\html-doc\images\db_ne2000patch_arachne.png
  File /oname=doc\images\db_vesamodes.png           ..\docs\html-doc\images\db_vesamodes.png
  
  CreateDirectory "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}"
  CreateDirectory "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Video"
  CreateDirectory "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Configuration"
  CreateShortCut "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\DOSBox.lnk" "$INSTDIR\DOSBox.exe" "" "$INSTDIR\DOSBox.exe" 0
  CreateShortCut "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\DOSBox (noconsole).lnk" "$INSTDIR\DOSBox.exe" "-noconsole" "$INSTDIR\DOSBox.exe" 0
  CreateShortCut "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\README.lnk" "$INSTDIR\README.txt"
  CreateShortCut "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Configuration\Edit Configuration.lnk" "$INSTDIR\DOSBox.exe" "-editconf notepad.exe -editconf $\"%SystemRoot%\system32\notepad.exe$\" -editconf $\"%WINDIR%\notepad.exe$\""
  CreateShortCut "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Configuration\Reset Configuration.lnk" "$INSTDIR\DOSBox.exe" "-eraseconf"
  CreateShortCut "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Capture folder.lnk" "$INSTDIR\DOSBox.exe" "-opencaptures explorer.exe"
  CreateShortCut "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Video\Video instructions.lnk" "$INSTDIR\zmbv\README.txt"
  CreateShortCut "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Documentation.lnk" "$INSTDIR\doc\index.html"

;change outpath so the working directory gets set to zmbv
SetOutPath "$INSTDIR\zmbv"
  ; Shortcut creation depends on wether we are 9x of NT
  ClearErrors
  ReadRegStr $R0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
  IfErrors we_9x we_nt
we_nt:
  ;shortcut for win NT
  CreateShortCut "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Video\Install movie codec.lnk" "rundll32" "setupapi,InstallHinfSection DefaultInstall 128 $INSTDIR\zmbv\zmbv.inf"
  goto end
we_9x:
  ;shortcut for we_9x
  CreateShortCut "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Video\Install movie codec.lnk" "rundll" "setupx.dll,InstallHinfSection DefaultInstall 128 $INSTDIR\zmbv\zmbv.inf"
end:
SetOutPath $INSTDIR
WriteUninstaller "uninstall.exe"

  goto end_section

error_createdir:
  MessageBox MB_OK "Can't create DOSBox program directory, aborting."
  Abort
  goto end_section

end_section:
SectionEnd ; end the section

Section "Desktop Shortcut" SecDesktop
SetShellVarContext all

CreateShortCut "$DESKTOP\DOSBox Megabuild${VER_MAYOR}.lnk" "$INSTDIR\DOSBox.exe" "" "$INSTDIR\DOSBox.exe" 0

 SectionEnd ; end the section 
 
Section "Font files" SecFonts

  File ..\bin\script.ttf
  File ..\bin\sansserif.ttf
  File ..\bin\roman.ttf
  File ..\bin\fontlicense.txt
  File ..\bin\courier.ttf

SectionEnd

Section "Debug build" SecDebug
  File ..\bin\pdcurses.dll
  File ..\bin\dosbox-debug.exe
  CreateShortCut "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\DOSBox-Debug.lnk" "$INSTDIR\DOSBox-debug.exe" "" "$INSTDIR\DOSBox-debug.exe" 0


SectionEnd

UninstallText "This will uninstall DOSBox  v${VER_MAYOR}.${VER_MINOR}. Hit next to continue."

Section "Uninstall"

; Shortcuts in all users
SetShellVarContext all

  Delete "$DESKTOP\DOSBox Megabuild${VER_MAYOR}.lnk"
  ; remove registry keys
  ; remove files
  Delete $INSTDIR\README.txt
  Delete $INSTDIR\COPYING.txt
  Delete $INSTDIR\THANKS.txt
  Delete $INSTDIR\NEWS.txt
  Delete $INSTDIR\AUTHORS.txt
  Delete $INSTDIR\INSTALL.txt
  Delete $INSTDIR\DOSBox.exe
  Delete $INSTDIR\DOSBox-debug.exe
  Delete $INSTDIR\dosbox.conf
  Delete $INSTDIR\pdcurses.dll
  Delete $INSTDIR\SDL.dll
  Delete $INSTDIR\msvcr71.dll
  Delete $INSTDIR\msvcp71.dll
  Delete $INSTDIR\SDL_net.dll
  Delete $INSTDIR\zmbv\zmbv.dll
  Delete $INSTDIR\zmbv\zmbv.inf
  Delete $INSTDIR\zmbv\README.txt
  ;Files left by sdl taking over the console
  Delete $INSTDIR\stdout.txt
  Delete $INSTDIR\stderr.txt
  ;doc files
  Delete $INSTDIR\doc\index.html
  Delete $INSTDIR\doc\db_ne2000.html
  Delete $INSTDIR\doc\pppatch.html
  Delete $INSTDIR\doc\images\db_ne2000patch_1.png
  Delete $INSTDIR\doc\images\db_ne2000patch_2.png
  Delete $INSTDIR\doc\images\db_ne2000patch_3.png
  Delete $INSTDIR\doc\images\db_ne2000patch_cfg1.png
  Delete $INSTDIR\doc\images\db_ne2000patch_cfg2.png
  Delete $INSTDIR\doc\images\db_ne2000patch_dosodi.png
  Delete $INSTDIR\doc\images\db_ne2000patch_tvwin.png
  Delete $INSTDIR\doc\images\db_ne2000patch_arachne.png
  Delete $INSTDIR\doc\images\db_vesamodes.png
  ;Font files
  Delete $INSTDIR\script.ttf
  Delete $INSTDIR\sansserif.ttf
  Delete $INSTDIR\roman.ttf
  Delete $INSTDIR\fontlicense.txt
  Delete $INSTDIR\courier.ttf

  ; MUST REMOVE UNINSTALLER, too
  Delete $INSTDIR\uninstall.exe

  ; remove shortcuts, if any.
  Delete "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Uninstall.lnk"
  Delete "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\README.lnk"
  Delete "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\DOSBox.lnk"
  Delete "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\DOSBox-Debug.lnk"
  Delete "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\DOSBox (noconsole).lnk"
  Delete "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Configuration\Edit Configuration.lnk"
  Delete "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Configuration\Reset Configuration.lnk"
  Delete "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Capture folder.lnk"  
  Delete "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Video\Install movie codec.lnk"
  Delete "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Video\Video instructions.lnk"
  Delete "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Documentation.lnk"
; remove directories used.
  RMDir "$INSTDIR\zmbv"
  RMDir "$INSTDIR\capture"
  RMDir "$INSTDIR\doc\images"
  RMDir "$INSTDIR\doc"
  RMDir "$INSTDIR"
  RMDir "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Configuration"
  RMDir "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}\Video"
  RMDir "$SMPROGRAMS\DOSBox-MB${VER_MAYOR}"
SectionEnd

; eof
