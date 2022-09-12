;           ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
;           ³                                            ³
;           ³    There is no need to process with RAR!   ³
;           ³                                            ³
;           ³     JUST RUN THIS SFX MODULE AND ENJOY!    ³
;           ³                                            ³
;           ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
;
;
;
;
;
; iSFX script will not be shown as the archive comment with RAR 1.54 and above!
DestDir="INSTALL"
; Total files in archive. CHANGE TO PROPER VALUE!
files=1
; Disk space needed to extract files, bytes. CHANGE TO PROPER VALUE!
diskneed=1024
; Define filebar
defbar BLACK,WHITE,WHITE,BLACK,14,7,52
; Print out the standard ansi comment ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
clrscr
outtext ansi on
RAR Installation SFX module
Created by RAR 2.00

outtext ansi off

; AV validation
if AVPresent == -1
      Title="Warning"
      Message="Invalid authenticity information"
      call ErrMsg
endif

if AVPresent == 0
      Title="Warning"
      Message="Missing authenticity information"
      call ErrMsg
endif

; Main background
setcolor DARKGRAY,BLACK
window 1,1,80,25
clrscr 219
; Main window
window 1,1,80,25

; AV info
box 3,3,68,8,YELLOW,BLUE,SINGLE
setcolor YELLOW,BLUE
gotoxy 24,3
text " Archive information "
setcolor DARKGRAY,BLACK
window 5,9,69,9
clrscr 177
window 69,4,69,9
clrscr 177
window 4,4,67,7
setcolor LIGHTCYAN,BLUE
clrscr
; Fields
gotoxy 1,1
text "  Valid archive name:  "
gotoxy 1,2
text "  Actual archive name: "
gotoxy 1,3
text "  Archive date:        "
gotoxy 1,4
text "  Created by:          "
; Values
setcolor WHITE,BLUE
gotoxy 26,1
if AVArcName!="AVArcName"
    text AVArcName
endif
gotoxy 26,2
text ArcName
gotoxy 26,3
if AVDate!="AVDate"
    text "released ",AVDate
endif
gotoxy 26,4
if AVUserName!="AVUserName"
    text AVUserName
endif

window 1,1,80,25
; Bottom line
setcolor LIGHTCYAN,DARKGRAY
gotoxy 1,25
text "                         Press F1 for help, Alt-X to exit                       "

; Shadow on the meadow
setcolor DARKGRAY,BLACK
window 25,17,56,17
clrscr 177
window 54,11,56,17
clrscr 177
window 1,1,80,25

; Sound on the same meadow
sound 90,1

; Installation Menu:

Menu:

Choice = menu 25,10,BLACK,WHITE,1,"Installation menu","     Read license"," Destination directory ","       Install","         Exit"

if Choice == 0
      goto Quit
endif

if Choice==1
     savescr 1
FirstPage:
     window 1,1,80,25
     setcolor LIGHTCYAN,BLUE
     clrscr
     box 1,1,80,25,LIGHTCYAN,BLUE,DOUBLE
     gotoxy 8,25
     ctext " Press PgDn for the second page or Esc for menu... "
     gotoxy 1,1
     ctext " License "
     gotoxy 1,1
     window 2,2,80,25
     setcolor YELLOW,BLUE
     sound 140,5
     outtext on
     outtext off
GetKey1:
     Key=getkey
     if Key==27
           restscr 1
           goto Menu
     endif
     if Key==337
SecondPage:
          window 1,1,80,25
          setcolor LIGHTCYAN,BLUE
          clrscr
          box 1,1,80,25,LIGHTCYAN,BLUE,DOUBLE
          gotoxy 8,25
          ctext " Press PgUp for first page or Esc for menu ..."
          window 2,2,80,25
          setcolor YELLOW,BLUE
          gotoxy 1,1
          sound 145,5
          outtext on
          outtext off
GetKey2:
          Key=getkey
          if Key==27
                restscr 1
                goto Menu
          endif
          if Key == 329
               goto FirstPage
          endif
          goto GetKey2
     endif
     goto GetKey1
endif


if Choice == 2
     savescr 1
     setcolor WHITE,CYAN
     window 11,10,67,15
     clrscr
     box 4,2,54,5,WHITE,CYAN,SINGLE
     gotoxy 20,2
     text " Destination directory "
     window 15,12,63,13
     clrscr
     setcolor WHITE,CYAN
     gotoxy 4,1 
     text "Enter directory to which to install"
     window 18,13,60,13
     Res=INPUT DARKGRAY,WHITE,DestDir,DestDir,42
     restscr 1
     window 1,1,80,25
     goto Menu
endif

if Choice == 3
  box 6,11,76,24,LIGHTCYAN,BLUE,DOUBLE
  setcolor LIGHTCYAN,BLUE
  gotoxy 27,11
  text " Installation status: "
  window 7,12,75,23
  clrscr
; Check free space
  DiskSpace=getdfree
  KDiskSpace=DiskSpace/1024
  KDiskNeed=diskneed/1024
  text "Installation started.\n"
  text "Disk space required:  ",KDiskNeed," Kb\n"
  text "Disk space available: ",KDiskSpace," Kb\n"
  inswrow=4
  window 1,1,80,25

  if DiskSpace < diskneed
          savescr 1
          window 1,1,80,25
          sound 2300,25
          delay 100
          sound 1450,25
          delay 100
          sound 2300,100
          Choice = menu 24,16,WHITE,RED,1,"Out of disk space","Continue with installation","          Exit"
          if Choice == 0
               goto Quit
          endif
          if Choice == 2
               goto Quit
          endif
          restscr 1
     endif
endif

if Choice == 4
Quit:
     setcolor WHITE,BLACK
     clrscr
     gotoxy 2,3
     text "The SFX package has not yet been extracted."
     sound 380,25
     delay 210
     sound 190,25
     exit 1
     goto Menu
endif

; HERE INSTALLATION STARTS!
; ctext "Installation has started."

proc OnKey
     window 1,1,80,25
     if Par1 == 3
          savescr 15
          Par1=-1
          sound 2300,25
          delay 100
          sound 1450,100
          Choice=Menu 22,11,WHITE,LIGHTRED,1,"Ctrl-Break","             Abort           ","   Continue with installation   "
          if Choice == 1
               exit 1
          endif
          restscr 15
     endif

     if Par1 == 301
           goto Quit
     endif

     if Par1 == 315
          savescr 15
          call OutHelp
          Par1=-1
          restscr 15
     endif
endp

proc ChangeVol

     window 7,12,75,23
     setcolor YELLOW,BLUE
     gotoxy 1,inswrow

     if Par2 == 0
           text ".. VOLUME #",Par1," should be installed."
           savescr 14
           Par1=-1
           sound 2300,25
           delay 100
           sound 1450,100
           sound 5450,50
           sound 7450,25
           sound 1450,100

           Choice=Menu 22,11,WHITE,GREEN,1," Change volume ","   Process the next volume        ","         Exit immediately    "

           restscr 14

           if Choice == 2
                 window 7,12,75,23
                 inswrow=inswrow+1
                 gotoxy 1,inswrow
                 text " Volume not found. Exiting..          "
                 exit 1
           endif
     endif

     if Par2 != 0
           setcolor LIGHTCYAN,BLUE
           text ".. VOLUME #",Par1," has been processed."
     endif

     if inswrow = 12
           text "\n"
     endif

     if inswrow < 12
           inswrow=inswrow+1
     endif
     window 1,1,80,25
endp

proc FileDone
     gotoxy 49,11
     setcolor LIGHTCYAN,BLUE
     Percent=Par1*100
     Percent=Percent/files
     text Percent,"%% "
     window 7,12,75,23
     setcolor LIGHTCYAN,BLUE
     gotoxy 1,inswrow
     text DestFileName
     if inswrow = 12
           text "\n"
     endif
     if inswrow < 12
           inswrow=inswrow+1
     endif
     window 1,1,80,25
endp

proc Error

     window 7,12,75,23
     setcolor LIGHTCYAN,BLUE
     gotoxy 1,inswrow

     Title="Error"

     ; Par1 is assigned with error code:

     if Par1 == 1
           Message="Fatal error"
           call ErrMsg
     endif

     if Par1 == 2
           Message="CRC error, broken archive"
           call ErrMsg
     endif

     if Par1 == 3
           Message="Write error"
           call ErrMsg
     endif

     if Par1 == 4
           Message="File create error"
           call ErrMsg
     endif

     if Par1 == 5
           Message="Read error"
           call ErrMsg
     endif

     if Par1 == 6
           Message="File close error"
           call ErrMsg
     endif

     if Par1 == 7
           Message="File open error"
           call ErrMsg
     endif

     if Par1 == 8
           Message="Not enough memory"
           call ErrMsg
     endif

     if inswrow = 12
           text "\n"
     endif

     if inswrow < 12
           inswrow=inswrow+1
     endif
     window 1,1,80,25
endp

proc ErrMsg
     window 17,14,62,21
     setcolor WHITE,RED
     clrscr
     window 1,1,80,25
     box 20,15,59,20,WHITE,RED,DOUBLE
     gotoxy 1,15
     ctext " ",Title," "
     gotoxy 1,17
     ctext Message
     setcolor WHITE,BLACK
     gotoxy 1,19
     ctext " Ok "
     sound 20,5
     sound 80,50
     delay 15000
     window 1,1,80,25
endp

proc ArcDone
     sound 3300,25
     delay 100
     sound 2450,100
     window 7,12,75,23
     setcolor LIGHTCYAN,BLUE
     gotoxy 1,inswrow
     text "Done. Installation completed"
     delay 500
     savescr 2
     Title="Installation status"
     Message=" Completed "
     call OkMsg
     restscr 2
endp

proc OkMsg
     window 24,14,57,21
     setcolor BLACK,WHITE
     clrscr
     window 1,1,80,25
     box 27,15,53,20,BLACK,WHITE,DOUBLE
     gotoxy 1,15
     ctext " ",Title," "
     gotoxy 1,17
     ctext Message
     setcolor WHITE,BLACK
     gotoxy 1,19
     ctext " Ok "
     delay 50000
     window 1,1,80,25
endp

proc OutHelp
     setcolor BLACK,CYAN
     box 14,6,65,20,BLACK,CYAN,SINGLE
     gotoxy 37,6
     text " Help "
     window 15,7,64,19
     clrscr
     outtext on

    To install choose the destination directory,
    then press Enter on the "Install" item
    of the menu.

    After installation files should be
    in the specified directory.

     outtext off
     delay
endp
;
;
;
;
;
;
;
;
;
;
;
;           ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
;           ³                                            ³
;           ³    There is no need to process with RAR!   ³
;           ³                                            ³
;           ³     JUST RUN THIS SFX MODULE AND ENJOY!    ³
;           ³                                            ³
;           ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
;
;
;
;
;
; iSFX script will not be shown as the archive comment with RAR 1.54 and above!
