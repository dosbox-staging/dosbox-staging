DOSBox v0.74 | Deutsche Lokalisierung von Peter
unter Zuhilfenahme der deutschen Übersetzung 0.73 Andreas (weapon.zero@gmx.net)


=========
Anmerkung
=========


Natürlich hoffen wir, dass sich irgendwann einmal wirklich jedes erdenkliche
PC-Programm unter DOSBox ausführen lässt, aber noch sind wir nicht ganz so
weit. Im Moment fühlt sich DOSBox auf einem High-End-Rechner in etwa wie ein
guter 486er an.

DOSBox kann so eingestellt werden, dass darauf eine große Bandbreite von IBM-
kompatiblen Spielen - von den Klassikern aus der Zeit von CGA, Tandy und PCjr
bis hin zu Titeln der Quake-Ära - läuft.



======
Inhalt
======


1. Schnelleinstieg
2. FAQ
3. Syntax
4. Interne Befehle/Programme
5. Tastaturkürzel
6. Der Keymapper
7. Tastaturlayouts
8. Mehrspieler mittels Nullmodem-Emulation
9. Verbessern der Spielgeschwindigkeit
10. Fehlerbehebung
11. Die Konfigurationsdatei
12. Die Sprachdatei
13. Eine eigene Version von DOSBox erstellen
14. Danksagungen
15. Kontaktinformation



==================
1. Schnelleinstieg
==================


Starten Sie DOSBox und tippen Sie "INTRO", um eine kurze Einführung zu
erhalten.

Sie sollten sich unbedingt mit dem Konzept des Einbindens ("mounten") von
Laufwerken und Verzeichnissen vertraut machen, da DOSBox nicht von sich aus
auf Laufwerke (oder Teile davon) zugreift und diese in der Emulation
bereitstellt.
Lesen Sie dazu den Eintrag "Mein C-Prompt sagt Z:\>!" in den FAQ und die
Beschreibung des Befehls "MOUNT" in Abschnitt 4.



======
2. FAQ
======


Hier einige Fragen, Antworten, Quintessenzen:

F: Mein C-Prompt sagt Z:\>!
F: Muss ich diese Befehle jedes Mal eingeben? Wie geht das automatisch?
F: Wie komme ich in den Vollbildmodus?
F: Mein CD-ROM-Laufwerk geht nicht!
F: Die Maus funktioniert nicht!
F: Es kommt kein Sound!
F: Der Sound stockt/stottert (oder klingt sonstwie seltsam)!
F: Ich kann keinen Backslash ("\") tippen! Und wo ist der Doppelpunkt?
F: Die Tastatur "hängt"!
F: Der Cursor bewegt sich nur in eine Richtung!
F: Das Spiel/Programm kann seine CD-ROM nicht finden!
F: Das Spiel/Programm läuft viel zu langsam!
F: Kann DOSBox meinen Computer beschädigen?
F: Wie ändere ich die Größe des Arbeitsspeichers/des EMS, die Prozessor-
   Geschwindigkeit oder den Soundblaster-IRQ?
F: Welche Sound-Hardware emuliert DOSBox?
F: Auf meinem Linux läuft aRts und DOSBox stürzt beim Start ab!
F: Tolle README, aber ich kapier's noch immer nicht...


F: Mein C-Prompt sagt Z:\>!

A: DOSBox muss erst Ordner/Verzeichnisse freigegeben bekommen, um mit (bzw.
   in) ihnen arbeiten zu können. Dafür gibt es den MOUNT-Befehl.

   Unter Windows würde der Befehl "MOUNT C D:\DOS" Ihnen innerhalb von DOSBox
   ein C-Laufwerk einbinden, das auf den Windows-Ordner D:\DOS verweist.
   Unter Linux würde "MOUNT C /home/username" Ihnen in DOSBox ein C-Laufwerk
   anlegen, das auf das Linux-Verzeichnis /home/username verweist.

   Um auf das eben eingebundene Laufwerk zu wechseln, tippen Sie "C:". Jetzt
   sollte DOSBox Ihnen die Eingabeaufforderung "C:\>" anzeigen.


F: Muss ich diese Befehle jedes Mal eingeben? Wie geht das automatisch?

A: In der DOSBox-Konfigurationsdatei gibt es den Abschnitt [autoexec], wo Sie
   Befehle eintragen können, die dann beim Start von DOSBox ausgeführt werden.


F: Wie komme ich in den Vollbildmodus?

A: Drücken Sie Alt-Enter. Alternativ können Sie auch die DOSBox-
   Konfigurationsdatei bearbeiten und die Zeile "fullscreen=false" in
   "fullscreen=true" ändern. Wenn Ihnen der Vollbildmodus irgendwie komisch
   vorkommt, probieren Sie verschiedene Werte für die Variable
   "fullresolution" aus. Um den Vollbildmodus wieder zu verlassen, drücken Sie
   erneut Alt-Enter.


F: Mein CD-ROM-Laufwerk geht nicht!

A: Um in DOSBox ein CD-ROM-Laufwerk einbinden zu können, können Sie zwischen
   verschiedenen Möglichkeiten wählen.
   Für einfachste CD-ROM-Unterstützung mit MSCDEX:
     MOUNT D F:\ -T cdrom
   Für "low-level" CD-ROM-Unterstützung (wenn möglich über IOCTL):
     MOUNT D F:\ -T cdrom -USECD 0
   Für "low-level" SDL-Unterstützung:
     MOUNT D F:\ -T cdrom -USECD 0 -NOIOCTL
   Für "low-level" ASPI-Unterstützung (Windows 98/Me mit ASPI-Layer):
     MOUNT D F:\ -T cdrom -USECD 0 -ASPI

   Wobei
     "D" für den gewünschten DOSBox-Laufwerksbuchstaben,
     "F:\" für die Pfad des CD-ROM-Laufwerks und
     "0" für die Nummer steht, die Sie nach Eingabe von "MOUNT -CD" für das
     betreffende CD-ROM-Laufwerk erhalten haben.

   Siehe auch FAQ: Das Spiel/Programm kann seine CD-ROM nicht finden!


F: Die Maus funktioniert nicht!

A: Normalerweise erkennt DOSBox die Maus, wenn ein Spiel sie benutzt. Durch
   einmaliges Klicken innerhalb des DOSBox-Fensters sollte sie erfasst werden
   und funktionieren. Bei bestimmten Spielen kommt es aber vor, dass die
   Mauserkennung fehlschlägt. In diesem Fall müssen Sie das Erfassen u.U.
   erzwingen: drücken Sie dazu Strg-F10.


F: Es kommt kein Sound!

A: Stellen Sie zunächst sicher, dass der Sound für das Spiel richtig
   eingestellt ist. Möglicherweise müssen Sie dafür ein zum Spiel gehörendes
   Programm namens SETUP, SETSOUND oder INSTALL ausführen. Prüfen Sie, ob es
   dort eine "Autodetect"-Option gibt. Falls nicht, wählen Sie eine "Sound
   Blaster"- oder "SoundBlaster 16"-Karte mit den Einstellungen "Adresse=220,
   IRQ=7, DMA=1" aus. Wenn Sie ein MIDI-Gerät auswählen können/wollen, wählen
   Sie die Einstellung "Adresse=330". In der DOSBox-Konfigurationsdatei können
   Sie diese Werte auch ändern.

   Falls das Problem damit nicht behoben ist, setzen Sie den "core" in der
   Konfigurationsdatei von "auto" auf "normal" und reduzieren Sie die Zahl der
   Cycles auf einen niedrigen statischen Wert (zum Beispiel "cycles=2000").
   Prüfen Sie auch, ob ihr Wirtsbetriebssystem überhaupt Sound ausgibt.


F: Der Sound stockt/stottert (oder klingt sonstwie seltsam)!

A: Sie benutzen zuviel Prozessorleistung, um die DOSBox-Geschwindigkeit zu
   halten. Sie sollten entweder die Zahl der Cycles verringern, mehr Frames
   überspringen oder die Abtastfrequenz (sampling rate) der verwendeten
   Soundkartenemulation reduzieren (siehe Konfigurationsdatei). Oder Sie
   schaffen sich einen schnelleren Rechner an.
   Sie können auch versuchen, in der Konfigurationsdatei den "prebuffer"
   für die Soundemulation zu erhöhen.

   Wenn die Cycles auf "max" oder "auto" eingestellt sind, stellen Sie sicher,
   dass keine Hintergrundprozesse stören (insbesondere, wenn diese auf die
   Festplatte zugreifen).


F: Ich kann keinen Backslash ("\") tippen! Und wo ist der Doppelpunkt?

A: Dieses Problem tritt auf, wenn Sie ein anderes als das US-amerikanische
   Tastaturlayout benutzen. Mögliche Abhilfen:
     1. Das Tastaturlayout ändern (siehe Abschnitt "Tastaturlayouts")
     2. Im Wirtsbetriebssystem das US-Tastaturlayout einstellen.
     3. Statt Backslash den Schrägstrich ("/") benutzen.
     4. In der Konfigurationsdatei die Variable "usescancodes" von "false"
        nach "true" setzen.
     5. Die gewünschten Befehle in der Konfigurationsdatei eintragen.
     6. Die Kombinationen Alt-58 und Alt-92 für ":" und "\" verwenden.
     7. Auf der deutschen Tastatur liegt der Backslash auf der "#"-Taste, den
        Doppelpunkt bekommt man mit Shift-Ö.
     8. Benutzen Sie das Programm KEYB (http://projects.freedos.net/keyb/),
        am besten in der Version 2.0 pre4, weil neuere wie ältere Versionen
        einen Programmfehler in den Laderoutinen aufweisen.


F: Die Tastatur "hängt"!

A: Reduzieren Sie in der Konfigurationsdatei die Priorität von DOSBox im
   System (z.B. "priority=normal,normal"). Eine andere Möglichkeit besteht
   darin, die Zahl der Cycles zu verringern.


F: Der Cursor bewegt sich nur in eine Richtung!

A: Schalten Sie die Joystick-Simulation ab und prüfen Sie, ob das Problem dann
   immer noch besteht (setzen Sie "joysticktype=none" im Abschnitt [joysticks]
   der DOSBox-Konfigurationsdatei. Sie können auch einfach den Joystick
   abklemmen (Stecker ziehen).
   Wenn Sie in dem Spiel einen Joystick verwenden möchten, setzen Sie
   versuchshalber "timed=false" und kalibrieren Sie den Joystick sowohl in
   Ihrem Betriebssystem als auch im Setup-Programm des Spiels.


F: Das Spiel/Programm kann seine CD-ROM nicht finden!

A: Stellen Sie sicher, dass Sie die CD-ROM mit dem Parameter "-T cdrom"
   eingebunden haben, dadurch wird das MSCDEX-Interface geladen, mit dem
   DOS-Spiele mit CD-ROM-Laufwerken kommunizieren.

   Sie können auch versuchen, das richtige CD-Label (Laufwerksbezeichnung)
   anzugeben ("-LABEL Bezeichnung"). Für "low-level" CD-ROM-Unterstützung
   benutzen Sie alternativ den Parameter "-USECD x", wobei das "x" für die
   Nummer des CD-Laufwerks steht. Diese erhalten Sie durch Eingabe von
   "MOUNT -CD". Unter Windows können Sie auch noch "-IOCTL", "-ASPI" oder
   "-NOIOCTL" angeben (dazu mehr bei der Beschreibung des MOUNT-Befehls).
   Siehe auch FAQ: Mein CD-ROM-Laufwerk geht nicht!

   Versuchen Sie, ein CD-ROM-Image anzulegen (vorzugsweise im Format CUE/BIN)
   und binden Sie dieses mit dem DOSBox-Befehl IMGMOUNT ein. Dadurch erhalten
   Sie unter jedem Betriebssystem eine ausgezeichnete "low-level" CD-ROM-
   Unterstützung.


F: Das Spiel/Programm läuft viel zu langsam!

A: Siehe Abschnitt "Verbessern der Spielgeschwindigkeit"


F: Kann DOSBox meinen Computer beschädigen?

A: DOSBox kann Ihren Computer nicht mehr beschädigen als jedes andere Programm
   mit hohen Systemvoraussetzungen. Wenn Sie die Zahl der Cycles erhöhen, wird
   nicht wirklich Ihr Prozessor übertaktet. Eine zu hoch eingestellte Cycles-
   Zahl hat negative Auswirkungen auf die Geschwindigkeit, mit der innerhalb
   von DOSBox Programme ausgeführt werden.


F: Wie ändere ich die Größe des Arbeitsspeichers/des EMS, die Prozessor-
   Geschwindigkeit oder den Soundblaster-IRQ?

A: Erstellen Sie eine Konfigurationsdatei: "CONFIG -WRITECONF dosbox.conf";
   öffnen Sie diese in Ihrem Lieblings-Text-Editor und schauen Sie sich die
   Einstellungen an. Um DOSBox mit den neuen Einstellungen zu laden, benutzen
   Sie "dosbox -CONF dosbox.conf" (ändern Sie u.U. Ihre Verknüpfung
   entsprechend).


F: Welche Sound-Hardware emuliert DOSBox?

A: DOSBox emuliert verschiedene alte Soundwiedergabe-Geräte:
   - PC-Lautsprecher
     Die Emulation umfasst den Ton-Generator und verschiedene Formen von
     digitaler Soundausgabe über den internen Lautsprecher.
   - Creative CMS/Gameblaster
     Die erste Karte von Creative Labs. In der Voreinstellung belegt sie
     Port 0x220. Vorsicht: wenn Sie die Karte zusammen mit der Adlib-Emulation
     aktivieren, kann es zu Problemen kommen.
   - Tandy 3 Voice
     Diese Hardware wird bis auf den Noise-Channel komplett emuliert. Dieser
     ist nicht besonders gut dokumentiert und was die Sauberkeit des Sounds
     angeht, half nur raten.
   - Adlib
     Diese Emulation wurde von MAME entliehen und ist fast perfekt. Sie umfasst
     auch die Fähigkeit des Adlib, beinahe digitalisierten Sound wiederzugeben.
   - SoundBlaster 16, SoundBlaster Pro I & II, SoundBlaster I & II
     In der Voreinstellung bietet DOSBox 16-Level/16-bit Stereo-Sound.
     Sie können in der Konfigurationsdatei auch ein anderes Modell einstellen.
   - Disney Soundsource
     Diese Soundkarte benutzt den Druckeranschluss und gibt nur Digital-Sound
     aus.
   - Gravis Ultrasound
     Diese Hardware wird fast komplett emuliert, jedoch wurden die MIDI-
     Funktionen weggelassen, da anderweitig ein MPU-401 emuliert wird.
   - MPU-401
     Auch ein MIDI-Interface wird emuliert, allerdings nur, wenn sie über ein
     General MIDI- oder MT-32-Gerät verfügen.


F: Auf meinem Linux läuft aRts und DOSBox stürzt beim Start ab!

A: Auch wenn es sich hierbei nicht wirklich um ein DOSBox-Problem handelt, so
   kann es beseitigt werden, indem Sie die Umgebungsvariable "SDL_AUDIODRIVER"
   nach "alsa" oder "oss" ändern.


F: Tolle README, aber ich kapier's noch immer nicht...

A: Vielleicht kann Ihnen "The Newbie's pictorial guide to DOSBox" unter
   http://vogons.zetafleet.com/viewforum.php?f=39 weiterhelfen.
   Alternativ können Sie es im DOSBox-Wiki probieren:
   http://dosbox.sourceforge.net/wiki/
   Beide Angebote sind nur in englischer Sprache verfügbar, es sollten sich
   über die bekannten Suchmaschinen aber auch zahlreiche deutschsprachige
   Tutorials und Anleitungen finden lassen.


Sollten Sie noch Fragen haben, lesen diese README aufmerksam durch und/oder
schauen Sie auf folgender Seite und dem dazugehörigen Forum nach:
http://dosbox.sourceforge.net



=========
3. Syntax
=========


Dieser Abschnitt gibt einen Überblick über die Kommandozeilenparameter, mit
denen DOSBox gestartet werden kann. Windows-Benutzer sollten dazu ihre DOSBox-
Verknüpfung entsprechend bearbeiten ("Ziel:") oder die Eingabeaufforderung
benutzen (CMD.EXE bzw. COMMAND.COM).

Die Parameter gelten - wenn nicht anders angegeben - für alle verwendeten
Betriebssysteme.

dosbox [Name] [-EXIT] [-C Befehl] [-FULLSCREEN] [-CONF Konfigurationsdatei]
       [-LANG Sprachdatei] [-MACHINE System] [-NOCONSOLE]
       [-STARTMAPPER] [-NOAUTOEXEC] [-SCALER Filter] [-FORCESCALER Filter]

dosbox -VERSION


  Name
        Wenn "Name" ein Verzeichnis ist, wird es als C-Laufwerk eingebunden.
        Wenn "Name" ein Programm ist, wird das Verzeichnis von "Name"
        eingebunden und "Name" ausgeführt.

  -EXIT
        DOSBox wird nach der Ausführung von "Name" beendet.

  -C Befehl
        Führt vor dem Aufrufen von "Name" einen bezeichneten "Befehl" aus.
        Es können mehrere Befehle angegeben werden, vor jedem sollte aber "-C"
        stehen.
        Mögliche "Befehle" sind: ein internes Programm, ein DOS-Befehl oder
        ein Programm auf einem eingebundenen Laufwerk.

  -FULLSCREEN
        Führt DOSBox im Vollbildmodus aus.

  -CONF Konfigurationsdatei
        Führt DOSBox mit den in "Konfigurationsdatei" angegebenen Optionen
        aus. Der Parameter "-CONF" kann mehrfach benutzt werden.
        Siehe dazu den Abschnitt "Die Konfigurationsdatei".

  -LANG Sprachdatei
        DOSBox benutzt die in "Sprachdatei" angegebenen Zeichenketten.

  -MACHINE System
        DOSBox anweisen, ein bestimmtes Computermodell zu emulieren. Mögliche
        Werte sind: hercules, cga, pcjr, tandy, vga (Voreinstellung).
        Die Wahl des Systems hat Einfluss auf den Grafikmodus und verfügbare
        Soundkarten.

  -NOCONSOLE (nur unter Windows)
        DOSBox ohne Status-Fenster laden. Meldungen werden in die Dateien
        stdout.txt und stderr.txt umgeleitet.

  -STARTMAPPER
        Ruft direkt beim Programmstart den Keymapper auf. Hilfreich bei
        Tastaturproblemen.

  -NOAUTOEXEC
        DOSBox ignoriert den Abschnitt [autoexec] der Konfigurationsdatei.

  -SCALER Filter
        Benutzt den durch "Filter" angegebenen Scaling-Methode. Die
        verfügbaren Filter finden Sie in der DOSBox-Konfigurationsdatei.

  -FORCESCALER Filter
        Ähnlich dem Parameter -SCALER; erzwingt die Anwendung des gewählten
        Filters auch wenn dieser möglicherweise nicht passt.

  -VERSION
        Gibt Versions-Information aus und beendet DOSBox. Zur Verwendung durch
        Frontends.


ANMERKUNG: Wenn Name/Befehl/Konfigurationsdatei/Sprachdatei ein Leerzeichen
enthält, setzen Sie Name/Befehl/Konfigurationsdatei/Sprachdatei komplett in
Anführungszeichen ("hier ein Beispiel").

Wenn Sie innerhalb der Anführungszeichen weitere Anführungszeichen benutzen
müssen (am wahrscheinlichsten bei den Parametern -C und -MOUNT), benutzen Sie
unter Windows und OS/2 einfache Anführungszeichen (') innerhalb der doppelten
Anführungszeichen, unter anderen Betriebssystemen versuchen Sie einen
vorangestellten umgekehrten Schrägstrich (\).

  Beispiel Windows:
    -C "MOUNT C 'C:\Mein Ordner'"
  Beispiel Linux:
    -C "MOUNT C \"/tmp/mein spiel\""


Beispiel:

dosbox C:\ATLANTIS\ATLANTIS.EXE -C "MOUNT D C:\SAVES"
  Bindet den Ordner C:\ATLANTIS als C-Laufwerk ein und führt das Programm
  ATLANTIS.EXE aus, nachdem C:\SAVES als D-Laufwerk eingebunden wurde.


Unter Windows können Verzeichnisse oder Dateien auch mittels Drag & Drop auf
die dosbox.exe gezogen werden.



============================
4. Interne Befehle/Programme
============================


DOSBox unterstützt die meisten DOS-Befehle aus COMMAND.COM; eine Liste der
internen Kommandos erhalten Sie durch den Befehl HELP.

Zusätzlich sind folgende Befehle verfügbar:


MOUNT "Virtueller Laufwerksbuchstabe" "Laufwerk oder Ordner"
      [-T Typ] [-ASPI] [-IOCTL] [-NOICTL] [-USECD Nummer]
      [-SIZE Laufwerksgröße] [-LABEL Bezeichnung] [-FREESIZE Speicherplatz]
MOUNT -CD
MOUNT -U "Virtueller Laufwerksbuchstabe"

  Programm zum Einbinden lokaler Laufwerke bzw. Verzeichnisse/Ordner als
  virtuelle Laufwerke innerhalb von DOSBox.

  "Virtueller Laufwerksbuchstabe"
        Der Laufwerksbuchstabe innerhalb von DOSBox (z.B. C)

  "Laufwerk oder Ordner"
        Das lokale Verzeichnis, das in DOSBox verfügbar gemacht werden soll.
        Es wird empfohlen, ganze Laufwerke nur dann einzubinden, wenn es sich
        bei ihnen um CD-ROMs handelt.

  -T Typ
        Typ des eingebundenen Verzeichisses. Unterstützt werden:
        dir (Voreinstellung), floppy, cdrom.

  -SIZE Laufwerksgröße
        Legt die Größe des Laufwerks fest. Format für Laufwerksgröße:
        "BpS,SpK,ACg,ACf"
          BpS: Bytes pro Sektor, in der Grundeinstellung 512 für normale und
            2048 für CD-ROM-Laufwerke
          SpC: Sektoren pro Cluster, normalerweise zwischen 1 und 127
          ACg: Anzahl der Cluster (gesamt), zwischen 1 und 65534
          ACf: Anzahl der freien Cluster, zwischen 1 und ACg

  -FREESIZE Speicherplatz
        Eine vereinfachte Version von -SIZE; legt die Größe des auf dem
        Laufwerk verfügbaren freien Speichers fest.
        Für normale Laufwerke wird der Wert in Megabytes interpretiert, für
        Disketten in Kilobytes.

  -LABEL Bezeichnung
        Laufwerksbezeichnung als "Bezeichnung" festlegen. Wird manchmal
        benötigt, wenn das CD-Label nicht richtig gelesen wird. Kann hilfreich
        sein, wenn ein Programm seine CD-ROM nicht findet. Falls keine
        Bezeichnung angegeben und keine "low-level" Unterstützung (-USECD
        und/oder -ASPI bzw. -NOICTL) gewählt wird:
          Windows: Bezeichnung wird übernommen von "Verzeichnis"
          Linux: Bezeichnung lautet "NO_LABEL"
        Wenn eine Bezeichnung angegeben wird, ist diese solange gültig, wie
        das Laufwerk eingebunden ist und wird nicht geupdatet (CD-Wechsel)!

  -ASPI
        Immer ASPI-Layer benutzen. Nur gültig, wenn eine CD-ROM auf einem
        Windows-System mit ASPI-Layer eingebunden wird.

  -IOCTL
        Immer IOCTL-Befehle benutzen. Nur gültig, wenn eine CD-ROM auf einem
        kompatiblen System eingebunden wird (Windows NT/2000/XP).

  -NOICTL
        Immer SDL-CD-ROM-Layer benutzen. Gültig auf allen Systemen.

  -USECD Nummer
        SDL-CD-ROM-Unterstützung für bestimmte Laufwerksnummer erzwingen. Die
        Nummer erfahren sie mit -CD. Gültig auf allen Systemen.

  -CD
        Zeigt alle erkannten CD-ROM-Laufwerke und ihre Nummern. Zur Benutzung
        mit -USECD.

  -U
        Laufwerk "entbinden". Nicht zu verwenden mit Z:\.

  ANMERKUNG: Ein lokales Verzeichnis kann auch als CD-ROM-Laufwerk eingebunden
  werden, die Hardware-Unterstützung fehlt dann.

  Im Grunde kann mit MOUNT echte Hardware mit dem von DOSBox emulierten PC
  verbunden werden. MOUNT C C:\GAMES weist DOSBox an, Ihren Ordner C:\GAMES
  als C-Laufwerk innerhalb DOSBox zu verwenden. Es erlaubt Ihnen auch, die
  Laufwerksbuchstaben für Programme zu ändern, die bestimmte
  Laufwerksbuchstaben benötigen.

  Das Spiel Touché: Adventures of the Fifth Musketeer muss beispielsweise
  vom C-Laufwerk gestartet werden. Mit DOSBox und dem MOUNT-Befehl kann man
  dem Spiel vorgaukeln, es befände sich auf dem C-Laufwerk, während Sie es
  ablegen können, wo Sie wollen. Wenn das Spiel also in D:\GAMES\TOUCHE wäre,
  könnten Sie den Befehl "MOUNT C D:\GAMES" benutzen, um Touché vom D-Laufwerk
  auszuführen.

  Es wird DRINGEND davon abgeraten, das gesamte C-Laufwerk mit "MOUNT C C:\"
  einzubinden. Dasselbe gilt für die Urverzeichnisse egal welches Laufwerks,
  mit Ausnahme von CD-ROMs (da diese schreibgeschützt sind). Falls Ihnen oder
  DOSBox ein Fehler unterläuft, könnten alle Daten verloren gehen.
  Es wird deshalb empfohlen, alle Programme/Spiele in einem Unterverzeichnis
  abzulegen und dieses einzubinden.

  Allgemeine MOUNT-Beispiele:

  1. C:\OrdnerX als Diskettenlaufwerk A einbinden:
       MOUNT A C:\OrdnerX -T floppy
  2. Das CD-ROM-Laufwerk E als CD-ROM-Laufwerk D in DOSBox einbinden:
       MOUNT D E:\ -T cdrom
  3. Das CD-ROM-Laufwerk bei /media/cdrom als Laufwerk D in DOSBox einbinden:
       MOUNT D /media/cdrom -T cdrom -USECD 0
  4. D:\ als Laufwerk mit ~870 MB freiem Speicher einbinden (einfach):
       MOUNT C D:\ -FREESIZE 870
  5. D:\ als Laufwerk mit ~870 MB freiem Speicher einbinden (komplex, nur für
     Profis!):
       MOUNT C D:\ -SIZE 512,127,16513,13500
  6. Verzeichnis /home/user/OrdnerY als C-Laufwerk in DOSBox einbinden:
       MOUNT C /home/user/OrdnerY
  7. Das DOSBox-Verzeichnis als Laufwerk D einbinden:
       MOUNT D


MEM

  Zeigt die Größe des freien Arbeitsspeichers an.


CONFIG -WRITECONF Datei
CONFIG -WRITELANG Datei
CONFIG -SET Abschnitt Variable=Wert
CONFIG -GET Abschnitt Variable

  Mit CONFIG können verschiedene Einstellungen von DOSBox abgefragt oder
  geändert werden, während das Programm läuft. Die gewählten Einstellungen
  und Zeichenketten können direkt gespeichert werden. Informationen über
  mögliche Abschnitte und Variablen finden Sie im Abschnitt "Die
  Konfigurationsdatei".

  -WRITECONF Datei
       Die aktuelle Konfigurationsdatei in "Datei" abspeichern. "Datei"
       befindet sich im lokalen DOSBox-Verzeichnis, nicht im eingebundenen
       DOSBox-Laufwerk!).

       Die Konfigurationsdatei steuert verschiedene DOSBox-Einstellungen: die
       Größe des emulierten Speichers, die emulierten Soundkarten und vieles
       mehr. Sie beinhaltet auch die AUTOEXEC.BAT-Einstellungen. Mehr Infor-
       mationen im Abschnitt "Die Konfigurationsdatei".

  -WRITELANG Datei
       Die aktuellen Spracheinstellungen abspeichern, wobei "Datei" sich im
       lokalen DOSBox-Verzeichnis befindet, nicht im eingebundenen DOSBox-
       Laufwerk. Die Sprachdatei beinhaltet (fast) alle auf dem Bildschirm
       sichtbaren Ausgaben der internen Befehle und des internen DOS.

  -SET Abschnitt Variable=Wert
       CONFIG versucht, der Variablen einen neuen Wert zuzuordnen. CONFIG kann
       zu diesem Zeitpunkt nicht ausgeben, ob der Versuch erfolgreich war.

  -GET Abschnitt Variable
       Der aktuelle Wert der Variable wird ausgegeben und und in der Umge-
       bungsvariable %CONFIG% gespeichert. Hilfreich, wenn mit Stapeldateien
       gearbeitet wird.

  Sowohl "-SET" als auch "-GET" funktionieren aus Stapeldateien heraus und
  können benutzt werden, um für einzelne Spiele eigene Einstellungen festzu-
  legen.

  Beispiele:

  1. Eine Konfigurationsdatei im DOSBox-Verzeichnis abspeichern:
       CONFIG -WRITECONF dosbox.conf
  2. Die CPU-Cycles auf 10000 setzen:
       CONFIG -SET cpu cycles=10000
  3. EMS-Speicheremulation abschalten:
       CONFIG -SET dos ems=off
  4. Prüfen, wieviel Prozessorleistung DOSBox im System nutzt:
       CONFIG -GET cpu core


LOADFIX [-Größe] [Programm] [Programm-Parameter]
LOADFIX -F

  Programm zum Speicher-"Fressen". Hilfreich bei alten Programmen, die wenig
  freien Speicher erwarten.

  -Größe
        Größe des zu "fressenden" Speichers in KB, Voreinstellung: 64 KB

  -F
        Vorher zugeteilten Speicher freigeben.

  Beispiele:

  1. Programm MM2.EXE laden und 64 KB Speicher zuteilen (MM2 erhält 64 KB
     weniger Speicher):
       LOADFIX MM2.EXE
  2. Programm MM2.EXE laden und 32 KB Speicher zuteilen:
       LOADFIX -32 MM2.EXE
  3. Vorher zugeteilten Speicher freisetzen:
       LOADFIX -F


RESCAN

  DOSBox liest die Verzeichnisstruktur erneut ein. Hilfreich, wenn außerhalb
  von DOSBox etwas in dem/den eingebundenen Verzeichnis/sen geändert wurde.
  Alternativ kann auch Strg-F4 benutzt werden.


MIXER Kanal Links:Rechts [/NOSHOW] [/LISTMIDI]

  Zeigt die Lautstärkeeinstellungen an und ändert sie.

  Kanal
        Einer der folgenden Werte: master, disney, spkr, gus, sb, fm.

  Links:Rechts
        Eine Lautstärkeangabe in Prozent (für die jeweilige Seite).
        Ein vorangestelltes D sorgt dafür, dass die Zahl als Dezibelangabe
        interpretiert wird (Beispiel: MIXER gus d-10).

  /NOSHOW
        Die Lautstärkeänderung erfolgt stumm (keine Textmeldung).

  /LISTMIDI
        Zeigt die auf Ihrem Rechner verfügbaren MIDI-Geräte an (Windows).
        Um ein anderes Gerät als den standardmäßigen MIDI-Mapper auszuwählen,
        fügen Sie eine Zeile "config=id" im Abschnitt [midi] der
        Konfigurationsdatei hinzu, wobei "id" die Nummer ist, die Sie nach
        Benutzung von /LISTMIDI für das betreffende Gerät erhalten.


IMGMOUNT

  Programm zum Einbinden von Disketten-, Festplatten- und CD-ROM-Images
  in DOSBox.

  IMGMOUNT "Virtueller Laufwerksbuchstabe" Imagedatei -T Imagetyp
           -FS Imageformat
           -SIZE Sektorengröße, Sektoren pro Kopf, Köpfe, Zylinder

  Imagedatei
        Adresse der in DOSBox einzubindenden Imagedatei. Diese befindet sich
        entweder auf einem in DOSBox eingebundenen Laufwerk oder einem
        physischen Gerät. Es können auch CD-ROM-Images (ISO oder CUE/BIN)
        eingebunden werden. Wenn Sie mehrere CD-ROMs benötigen, geben Sie
        die Images hintereinander an. Diese können dann jederzeit mit Strg-F4
        gewechselt werden.

  -T Imagetyp
      Gültige Imagetypen:
        floppy: Gibt ein Disketten-Image bzw. -Images an. DOSBox erkennt das
                Format automatisch (360 KB, 1,2 MB, 720 KB, 1,44 MB usw.)
        iso:    Gibt ein CD-ROM-Image an. Das Format wird automatisch erkannt.
                Es kann ein ISO- oder CUE/BIN-Image benutzt werden.
        hdd:    Gibt ein Festplatten-Image an. Es muss die korrekte CHS-
                Laufwerksgeometrie angegeben werden.

  -FS Imageformat
      Gültige Dateisystem-Formate:
        iso:    ISO 9660 CD-ROM-Format.
        fat:    FAT-Dateisystem. DOSBox versucht, dieses Image als Laufwerk
                in DOSBox einzubinden und die Dateien in DOSBox verfügbar zu
                machen.
        none:   DOSBox versucht nicht, das Dateisystem auf dem Image zu lesen.
                Dies kann nützlich sein, wenn man es formatieren oder mit dem
                BOOT-Befehl davon booten will. Für das "none"-Dateisystem muss
                statt einem Laufwerksbuchstaben eine Laufwerksnummer angegeben
                werden (2 oder 3; wobei 2=Master, 3=Slave).
                Um beispielsweise ein 70 MB-Image D:\TEST.IMG als Slave-
                Laufwerk einzubinden, benutzen Sie:
                  IMGMOUNT 3 D:\TEST -SIZE 512,63,16,142 -FS none
                Zum Vergleich der Befehl, um das Laufwerk in DOSBox zu lesen:
                  IMGMOUNT E D:\TEST.IMG -SIZE 512,63,16,142

  -SIZE
        Die Laufwerksgeometrie (Angaben zur Anzahl von Zylindern, Köpfen und
        Sektoren) des Laufwerks. Wird benötigt, um Festplatten-Images
        einbinden zu können.

  Ein Beispiel zum Einbinden von CD-ROM-Images:

    1a. MOUNT C /tmp
    1b. IMGMOUNT D C:\CDROM.ISO -T iso
  oder alternativ:
    2. IMGMOUNT D /tmp/CDROM.ISO -T iso


BOOT

  BOOT startet Disketten- oder Festplatten-Images unabhängig von der DOS-
  Emulation von DOSBox. Damit können sog. Booter-Spiele oder andere
  Betriebssysteme innerhalb von DOSBox gebootet werden.
  Wenn das emulierte System ein PCjr ist ("machine=pcjr"), können mit dem
  BOOT-Befehl auch PCjr-Cartridges (.jrc) geladen werden.

  BOOT [Image1.img Image2.img ... ImageN.img] [-L Laufwerksbuchstabe]
  BOOT Cartridge.jrc
    (nur gültig bei PCjr-Emulation)

  ImageN.img
        Eine beliebige Anzahl von Disketten-Images, die eingebunden werden
        sollen, nachdem DOSBox vom angegebenen Laufwerk gebootet hat.
        Um zwischen den eingebundenen Disketten zu wechseln, benutzen Sie
        Strg-F4. Dadurch wird die aktuelle Diskette "ausgeworfen" und die
        nächste auf der Liste "eingelegt". Sobald die letzte Diskette auf der
        Liste ausgeworfen wurde, wird wieder bei der ersten begonnen.

  -L Laufwerksbuchstabe
        Gibt das Laufwerk an, von dem gebootet werden soll.
        Die Voreinstellung ist A, also das Diskettenlaufwerk. Es kann auch von
        einem als Master eingebundenen Festplatten-Image gebootet werden,
        dazu muss "-L C" angegeben werden. Soll das Laufwerk als Slave
        behandelt werden, lautet der Parameter: "-L D".

   Cartridge.jrc
        Wenn das emulierte System ein PCjr ist, können auch PCjr-Cartridges
        gebootet werden. Die Emulation ist aber noch nicht perfekt.


IPX

  Die IPX-Netzwerkunterstützung muss zunächst in der Konfigurationsdatei
  aktiviert werden ("ipx=true").

  Alles, was mit dem IPX-Netzwerk zusammenhängt, wird durch den internen
  Befehl IPXNET gesteuert. Um Hilfe über das IPX-Netzwerk innerhalb von DOSBox
  zu erhalten, benutzen Sie "IPXNET HELP" (ohne Anführungszeichen). Das
  Programm wird Ihnen eine Liste der gültigen Parameter und dazugehörige
  Informationen ausgeben.

  Um das Netzwerk nun tatsächlich einrichten zu können, muss ein System als
  Server fungieren. Um es einzurichten, geben Sie unter DOSBox den Befehl
  "IPXNET STARTSERVER" ein. Es wird sich automatisch eine DOSBox-
  Serversitzung zum virtuellen IPX-Netzwerk hinzufügen. Auf den Rechnern, die
  mit dem virtuellen IPX-Netzwerk verbunden werden sollen, geben Sie jeweils
  den Befehl "IPXNET CONNECT Adresse" ein.

  Wenn der Server zum Beispiel horst.dosbox.com heißt, müssen Sie folgenden
  Befehl auf jedem Client eingeben: "IPXNET CONNECT horst.dosbox.com".

  Für Spiele, die NetBIOS benutzen, wird ein Programm namens NETBIOS.EXE von
  Novell benötigt. Richten Sie die IPX-Verbindung wie oben beschrieben ein
  und starten Sie NetBIOS.

  Es folgt die IPXNET-Syntax:

  IPXNET CONNECT Adresse Port
        öffnet eine Verbindung zu einem IPX-Tunnelserver innerhalb einer
        anderen DOSBox-Sitzung. Der Parameter "Adresse" gibt die IP-Adresse
        oder den Host-Namen des Servers an. Sie können auch einen UDP-Port
        angeben, den Sie benutzen wollen. Standardmäßig benutzt IPXNET den
        Port 213. Dies ist der Anschluss, der von IANA für den IPX-
        Tunnelserver zugewiesen wird.

  IPXNET DISCONNECT
        beendet die Verbindung mit dem IPX-Tunnelserver.

  IPXNET STARTSERVER Port
        startet einen IPX-Tunnelserver innerhalb der aktuellen DOSBox-Sitzung.
        Standardmäßig akzeptiert der Server Verbindungen auf UDP-Port 213,
        Sie können diese Einstellung aber ändern.
        Wenn dem Server ein Router vorgeschaltet ist, muss der Port angegeben
        werden.
        Auf Linux-/Unix-basierten Systemen können Ports unterhalb von 1023 nur
        von Root-Accounts benutzt werden. Auf diesen Systemen sollten Sie
        Ports oberhalb von 1023 benutzen.

  IPXNET STOPSERVER
        beendet den IPX-Tunnelserver der aktuellen DOSBox-Sitzung. Sie sollten
        vorsichtshalber sicherstellen, dass alle anderen Verbindungen
        ebenfalls beendet wurden, ansonsten könnten sich die noch auf den
        Tunnelrechner zugreifenden Rechner eventuell aufhängen.

  IPXNET PING
        sendet einen Broadcast-Ping durch das Tunnelnetzwerk, auf den die
        angeschlossenen Rechner antworten und die Zeit angeben, die benötigt
        wurde, um den Ping zu senden und zu empfangen.

  IPXNET STATUS
        zeigt den Status des IPX-Tunnelnetzwerks der aktuellen DOSBox-Session
        an. Um eine Liste aller an das Netzwerk angeschlossenen Rechner zu
        erhalten, benutzen Sie den Befehl IPXNET PING.


KEYB [Tastaturcode [Codeseite [Tastaturdefinitionsdatei]]]

  Programm zum Ändern des Tastaturcodes (Tastaturlayouts). Mehr darüber im
  Abschnitt "Tastaturlayouts".

  Tastaturcode
        Der aus zwei (oder mehr) Buchstaben bestehende Tastaturcode für ein
        Land, beispielsweise GK (Griechenland) oder IT (Italien). Damit wird
        das zu verwendende Tastaturlayout ausgewählt.

  Codeseite
        Die Nummer der zu verwendenden Codeseite (Codepage). Wenn der
        Tastaturcode nicht mit der Codeseite kompatibel ist, kann diese nicht
        geladen werden.
        Wenn keine Codeseite angegeben wird, benutzt DOSBox automatisch das
        entsprechende Standard-Layout.

  Tastaturdefinitionsdatei
        Diese kann/muss angegeben werden, falls die gewünschte Codeseite nicht
        durch DOSBox zur Verfügung gestellt wird und wird auch nur in solchen
        Fällen benötigt.

  Beispiele:

  1) Standard-Tastaturlayout für Deutschland und Österreich (es wird
     automatisch die Codeseite 858 verwendet):
       KEYB GR
  2) Russisches Tastaturlayout mit Codeseite 866:
       KEYB RU 866
         (Benutzen Sie LinksAlt-RechtsShift für kyrillische Zeichen)
  3) Französisches Tastaturlayout mit Codeseite 850 aus der Datei EGACPI.DAT:
       KEYB FR 850 EGACPI.DAT
  4) Codeseite 858 (ohne Tastaturlayout):
       KEYN none 858


Um Hilfe für einen internen Befehl oder ein bestimmtes Programm zu erhalten,
kann auch der Kommandozeilenparameter "/?" benutzt werden.



=================
5. Tastaturkürzel
=================


Die folgende Auflistung erhalten Sie auch mit dem Befehl INTRO SPECIAL:

Alt-Enter       Vollbildmodus an/aus
Alt-Pause       Emulation unterbrechen
Strg-F1         Keymapper starten
Strg-F4         Disketten-Image wechseln; Verzeichnisstruktur neu lesen
Strg-F5         Screenshot als PNG-Datei abspeichern
Strg-Alt-F5     Bildschirmausgabe und Sound als Videoclip speichern an/aus
Strg-F6         Soundausgabe in WAV-Datei schreiben an/aus
Strg-Alt-F7     OPL-Daten aufzeichnen an/aus
Strg-Alt-F8     Raw-MIDI-Daten aufzeichnen an/aus
Strg-F7         Frames überspringen -1
Strg-F8         Frames überspringen +1
Strg-F9         DOSBox schließen
Strg-F10        Maus erfassen/freigeben
Strg-F11        Emulation verlangsamen (DOSBox-Cycles verringern)
Strg-F12        Emulation beschleunigen (DOSBox-Cycles erhöhen)
Alt-F12         Drosselung aufheben (Turbo-Knopf)

Diese Voreinstellungen können im Keymapper geändert werden.

Gespeicherte/aufgenommene Dateien werden im Unterverzeichnis "capture" im
DOSBox-Ordner abgelegt (diese Einstellung kann in der Konfigurationsdatei
geändert werden).
Das Verzeichnis muss vor dem Start von DOSBox vorhanden sein, sonst kann
nichts gespeichert/aufgenommen werden!

ANMERKUNG: Sobald die DOSBox-Cycles derart erhöht wurden, dass es das
Leistungsmaximum Ihres Rechners übersteigt, hat das denselben Effekt, als
würden Sie die Emulation verlangsamen. Dieses Maximum ist von Rechner zu
Rechner unterschiedlich, es gibt keinen allgemein gültigen Wert.



================
6. Der Keymapper
================


Wenn Sie den Keymapper starten (entweder mit Strg-F1 oder dem
Kommandozeilenparameter "-STARTMAPPER"), erhalten Sie eine virtuelle Tastatur
und einen virtuellen Joystick.

Diese virtuellen Geräte entsprechen den Tasten, die DOSBox an die ausgeführten
Programme übergibt. Durch einen Mausklick auf die jeweilige Taste sehen Sie
links unten, mit welcher Taste/Funktion diese verknüpft ist (EVENT).

EVENT: Taste/Funktion
BIND: Belegung
                        Add   Del
mod1  hold              Next
mod2
mod3

  EVENT
    Die Taste/Joystick-Knopf bzw -Achse, die DOSBox den emulierten Programmen
    meldet.

  BIND
    Die Taste auf Ihrer Tastatur/Ihrem Joystick (bzw. die Joystick-Achse), die
    dem EVENT (Taste/Funktion) durch SDL zugewiesen wird.

  mod1,2,3
    Zusatztasten: die Tasten, die zusammen mit BIND gedrückt werden müssen.
    mod1=Strg, mod2=Alt. Diese werden i.A. nur gebraucht, wenn die DOSBox-
    Sondertasten geändert werden sollen.

  Add
    Dem EVENT ein neues BIND zuweisen, also: eine Taste (oder einen Joystick-
    Knopf usw.) in DOSBox mit einem EVENT belegen.

  Del
    Den BIND des EVENTs löschen. Wenn ein EVENT keine BINDs hat, ist die Taste
    in DOSBox nicht belegt. Die entsprechende Taste/der Knopf hat also keine
    Funktion.

  Next
    Den nächsten BIND eines EVENTs anzeigen.


Beispiele für die Tastaturbelegung:

  1. Das X auf Ihrer Tastatur soll in DOSBox ein U ausgeben:
     Mit der Maus auf das U im Keymapper klicken. "Add" anklicken. Die X-Taste
     auf der Tastatur drücken.
  2. Wenn Sie jetzt ein paarmal auf "Next" klicken, werden Sie bemerken, dass
     das U auf Ihrer Tastatur auch in DOSBox ein U ausgibt.
     Wählen Sie also erneut U aus und klicken sie solange auf "Next", bis Sie
     das U auf Ihrer Tastatur haben. Klcken Sie jetzt auf "Del".
  3. Wenn Sie in der DOSBox-Shell jetzt probeweise X drücken, wird ein "UX"
     ausgegeben.
     Das X auf Ihrer Tastatur ist noch immer mit X belegt. Klicken Sie im
     Keymapper auf das X und suchen Sie mit "Next", bis Sie die Belegung X
     gefunden haben. Klicken Sie auf "Del".


Beispiele für die Joystick-Tastenbelegung:

  Es ist ein Joystick angeschlossen, der durch DOSBox einwandfrei erkannt
  wird, und Sie möchten damit ein Spiel steuern, dass normalerweise nur die
  Tastatur erkennt (wir nehmen an, das Spiel wird über die Richtungstasten
  gesteuert):
  1. Starten Sie den Keymapper und klicken Sie auf eine der Pfeiltasten im
     mittleren linken Bereich des Fensters (direkt über den Mod1/Mod2-Tasten).
     Als EVENT sollte "key_left" angegeben sein. Klicken Sie auf "Add" und
     bewegen Sie das Achsenkreuz des Joysticks nach links. Jetzt sollte dem
     BIND der Event zugewiesen worden sein.
  2. Wiederholen Sie das Obengenannte für die drei anderen Richtungen;
     zusätzlich können die Joystick-Knöpfe anderen Events zugewiesen werden,
     etwa den Funktionen "springen" oder "schießen".
  3. Klicken Sie auf "Save", dann auf "Exit" und starten Sie das Spiel, um
     auszuprobieren, ob die Einstellungen erfolgreich waren.

  Die Y-Achse soll so geändert werden, dass eine Bewegung nach oben der nach
  unten entspricht - und umgekehrt -, weil Sie damit besser zurechtkommen und
  sich das Spiel selbst nicht entsprechend konfigurieren lässt:
  1. Starten Sie den Keymapper und klicken Sie auf "Y-" im Joystick-Bereich
     oben rechts (falls Sie zwei Joysticks angeschlossen haben, stellen Sie
     damit den ersten Joystick ein), oder im Joystick-Bereich weiter unten
     (zweiter Joystick, oder - falls Sie nur einen angeschlossen haben - das
     zweite Achsenkreuz des ersten Joysticks).
     Also EVENT sollte jetzt angegeben sein: "jaxis_0_1-" (bzw. "jaxis_1_1-).
  2. Klicken Sie auf "Del", um die aktuelle Verknüpfung zu löschen, klicken
     Sie danach auf "Add" und bewegen Sie das Achsenkreuz des Joysticks nach
     unten. Es sollte ein neuer Bind angelegt worden sein.
  3. Gehen Sie entsprechend für "Y+" vor, speichern Sie die Belegung ab und
     probieren Sie es aus.


Wenn Sie die Einstellungen bearbeitet haben, können Sie die Änderungen
abspeichern, indem Sie auf "Save" klicken. DOSBox speichert die Belegungen am
in der Konfigurationsdatei angegebenen Ort ab ("mapperfile=mapper.txt").

Beim Start von DOSBox wird die Tastenbelegung geladen, sofern die angegebene
Datei vorhanden ist.



==================
7. Tastaturlayouts
==================


Um das verwendete Tastaturlayout (den Tastaturcode) zu ändern, kann entweder
die Variable "keyboardlayout" im Abschnitt [dos] der Konfigurationsdatei
bearbeitet werden, oder Sie benutzen das interne Programm KEYB unter DOSBox.
Bei beiden Möglichkeiten können Sie DOS-konforme Sprachcodes (siehe unten)
verwenden, eine benutzerdefinierte Codeseite kann jedoch nur mit KEYB
ausgewählt werden.


Einstellen des Tastaturcodes

  DOSBox bringt bereits eine ganze Anzahl von voreingestellten Tastaturlayouts
  und Codeseiten mit. Wenn das gewünschte Layout dabei ist, muss es nur noch
  in der Konfigurationsdatei oder über das Programm KEYB angegeben werden (zum
  Beispiel "keyboardlayout=sv" in der Konfigurationsdatei oder "KEYB SV"
  an der DOSBox-Eingabeaufforderung).

  Voreingestellte Tastaturcodes:
    BG     (Bulgarien)                  NL (Niederlande)
    CZ243  (Tschechien)                 NO (Norwegen)
    FR     (Frankreich)                 PL (Polen)
    GK     (Griechenland)               RU (Russland)
    GR     (Deutschland/Österreich)     SK (Slowakei)
    HR     (Kroatien)                   SP (Spanien)
    HU     (Ungarn)                     SU (Finnland)
    IT     (Italien)                    SV (Schweden)

  Bestimmte Layouts (z.B. GK mit Codeseite 869 und RU mit Codeseite 808)
  unterstützen kombinierte Layouts mit lateinischen und länderspezifischen
  Zeichen, die durch die Tastenkombination LinksAlt-RechtsShift aktiviert, und
  mit LinksAlt-LinksShift deaktiviert werden.


Unterstütze Tastaturdefinitionsdateien

  Es werden sowohl die FreeDOS-Tastaturdefinitionsdateien mit der Endung "KL"
  ("FreeDOS Keyb2 Keyboard Layoutfiles") als auch die aus allen verfügbaren
  FreeDOS-Tastaturdefinitionsdateien bestehenden Bibliotheken Keyboard.sys,
  Keybrd2.sys und Keybrd3.sys unterstützt. Falls die in DOSBox integrierten
  Layouts aus irgendwelchen Gründen nicht funktionieren oder neuere verfügbar
  werden, finden Sie diese unter http://projects.freedos.net/keyb/

  Ebenfalls verwendet werden können Dateien mit den Endungen CPI (DOS-
  kompatible Codeseiten-Spezifikationen) und CPX- (mit UPX komprimierte
  FreeDOS-Codeseiten-Dateien). DOSBox beinhaltet schon einige Codeseiten, so
  dass es selten erforderlich ist, externe Codeseiten-Dateien zu verwenden.
  Falls Sie eine andere - oder eigene - Codeseiten-Datei benutzen möchten,
  kopieren Sie diese in denselben Ordner wie die Konfigurationsdatei, so dass
  DOSBox auf sie zugreifen kann.

  Zusätzliche Layouts können verwendet werden, indem Sie die entsprechende KL-
  Datei in denselben Ordner wie die Konfigurationsdatei kopieren und den
  Dateinamen (ohne Endung) als Tastaturcode verwenden.
  Für die Datei UZ.KL (das Tastaturlayout für Usbekistan) würden Sie also in
  der Konfigurationsdatei "keyboardlayout=uz" angeben.
  Die Verwendung von Bibliotheken wie Keybrd2.sys funktioniert ähnlich.


ANMERKUNG: Zwar können mit einem eingestellten Tastaturlayout Sonderzeichen
an der Kommandozeile getippt werden, jedoch werden Dateinamen aus solchen
Zeichen (Umlaute usw.) nicht unterstützt. Versuchen Sie, diese in DOSBox und
für Dateien, auf die DOSBox zugreifen kann/soll, zu vermeiden.



==========================================
8. Mehrspieler mittels Nullmodem-Emulation
==========================================


DOSBox kann ein serielles Nullmodem-Kabel über Ihr Netzwerk oder das Internet
emulieren.

Dieses muss zunächst im Abschnitt [serialports] in der Konfigurationsdatei
eingerichtet werden. Hierbei fungiert ein Rechner als Server, der andere als
Client.

In der Konfigurationsdatei des Servers tragen Sie unter [serialports]
folgendes ein:
  serial1=nullmodem

Die entsprechende Eintragung beim Client:
  serial1=nullmodem server:Adresse
    "Adresse" bezeichnet die IP-Adresse oder den Host-Namen des Servers.

Starten Sie jetzt das Spiel und wählen Sie als Mehrspieler-Methode an COM1:

  "Nullmodem", "Serial Cable", "already connected"

Stellen Sie auf beiden Rechnern dieselbe Baudrate ein.


Es können weitere Parameter angegeben werden, um die Nullmodem-Verbindung
einzurichten:

  port
    TCP-Port. Voreinstellung: 23

  rxdelay
    Wie lange sollen empfangene Daten bereitgehalten werden, wenn das
    Interface nicht bereit ist (in Millisekunden)?
    Erhöhen Sie diesen Wert, wenn das DOSBox-Status-Fenster wiederholt
    die Meldung "overrun error" ausgibt. Voreinstellung: 100

  txdelay
    Wie lange sollen Daten gesammelt werden, bis ein Paket versendet wird?
    Voreinstellung: 12 (kann die Netzwerk-Belastung reduzieren)

  server:Adresse
    Diese Seite des Nullmodems agiert als Client. Wenn der Parameter fehlt,
    agiert diese Seite als Server.

  transparent:1
    Nur serielle Daten senden, keine RTS/DTR-Kontrolle. Benutzen Sie diesen
    Parameter, wenn zu etwas anderem als einem Nullmodem verbunden wird.

  telnet:1
    Telnet-Daten des anderen Rechners auswerten.
    Parameter "transparent" wird automatisch gesetzt.

  usedtr:1
    Die Verbindung wird erst eingerichtet, wenn DTR vom DOS-Programm auf
    "an" gestellt ist. Zur Verwendung mit Modem-Terminals.
    Parameter "transparent" wird automatisch gesetzt.

  inhsocket:1
    Einen Socket verwenden, der DOSBox durch die Kommandozeile angegeben
    wurde (um alte DOS-Spiele über neue Mailboxen spielen zu können).
    Parameter "transparent" wird automatisch gesetzt.


Beispiel:

  Einen Client einrichten, der TCP-Port 5000 abhört:
    serial1=nullmodem server:Adresse port:5000 rxdelay:1000



======================================
9. Verbessern der Spielgeschwindigkeit
======================================


DOSBox emuliert den Prozessor, Sound- und Grafikkarten und einiges anderes
mehr - und das alles gleichzeitig. Die Geschwindigkeit, mit der ein DOS-
Programm emuliert werden kann, hängt davon ab, wie viele Maschinenbefehle
innerhalb einer gegebenen Zeit emuliert werden können. Diese Zahl von
Prozessorzyklen oder "Cycles" kann in der Konfigurationsdatei eingestellt
werden (Abschnitt [cpu]).


Cycles

  Die Voreinstellung ("cycles=auto") weist DOSBox an, automatisch zu erkennen,
  ob das Spiel bei höchstmöglicher Geschwindigkeit (so viele Maschinenbefehle
  wie möglich) ausgeführt werden muss. Durch die Einstellung "cycles=max"
  kann dieses Maximum auch zum Standard gemacht werden. In der Titelleiste des
  DOSBox-Fensters wird dann der Text "Cpu Cycles: max" angezeigt. In diesem
  Modus können Sie die Zahl der Cycles relativ zum Maximum (also in Prozent)
  reduzieren (Strg-F11) oder wieder erhöhen (Strg-F12).

  Manchmal werden bessere Ergebnisse erreicht, wenn die Zahl der Cycles
  manuell eingestellt wird, beispielsweise indem Sie "cycles=30000" angeben.
  Bei einigen DOS-Programmen können Sie vielleicht sogar einen noch höheren
  Wert einstellen (Strg-F12), die Geschwindigkeit ihres Prozessors wird Ihnen
  aber irgendwann Grenzen setzen.
  Die CPU-Auslastung kann im Task-Manager (unter Windows 2000/XP) bzw. im
  System-Monitor (Windows 98/Me) abgelesen werden. Sobald diese Auslastung
  100% erreicht hat, kann DOSBox nicht noch schneller werden, es sei denn, die
  Rechenleistung, die DOSBox für Peripheriegeräte aufbringen muss, wird
  reduziert.


Cores

  Wenn Ihr Rechner mit einem Intel-kompatiblen CPU (x86-Architektur) läuft,
  können Sie versuchen, die Benutzung eines sich dynamisch rekompilierendieren
  Kerns zu erzwingen (setzen Sie in der Konfigurationsdatei die Einstellung
  "core=dynamic"). Sollte die automatische Erkennung ("core=auto") scheitern,
  werden damit oft bessere Ergebnisse erreicht. Am besten benutzen Sie diese
  Einstellung zusammen mit "cycles=max". Bitte beachten Sie aber, dass manche
  Spiele mit dem dynamischen Kern unter Umständen schlechter oder sogar
  überhaupt nicht laufen.


Grafik-Emulation

  Die Emulation der VGA-Grafik ist - was die Prozessorleistung angeht - ein
  sehr anspruchsvoller Teil von DOSBox. Drücken Sie Strg-F8, um die Zahl der
  übersprungenen Frames (in Einserschritten) zu erhöhen. Wenn sie einen
  statischen Cycles-Wert benutzen, sollte sich jetzt die Prozessorbelastung
  reduzieren und der Cycles-Wert kann erhöht werden (Strg-F12).
  Wiederholen Sie dies so lange, bis das Spiel schnell genug läuft. Beachten
  Sie bitte, dass Sie hier nur einen Kompromiss aushandeln können: zwar steigt
  die Geschwindigkeit, dafür ist das Bild aber nicht mehr so flüssig.


Sound-Emulation

  Sie können auch versuchen, den Sound abzustellen (im Setup-Programm des
  Spiels), um dadurch die Prozessorbelastung zu verringern. Bitte beachten
  Sie, dass die Einstellung "nosound=true" nicht die Emulation der
  Soundkarten unterbindet, sondern lediglich die Soundausgabe.


Versuchen Sie grundsärtlich, nur so wenige Hintergrundprozesse (oder andere
Programme) wie nötig am Laufen zu haben, damit DOSBox möglichst viele
Systemressourcen zur Verfügung stehen.


Erweiterte Cycles-Konfiguration:

  Die Einstellungen "cycles=auto" und "cycles=max" können parametriesiert
  werden, um unterschiedliche Voreinstellungen zu setzen:

    cycles=auto "Realmode-Standard" "Protected Mode-Standard"%
                limit "Cycles-Höchstzahl"
    cycles=max "Protected Mode-Standard"% limit "Cycles-Höchstzahl"

    Beispiel: cycles=auto 1000 80% limit 20000
      Benutzt "cycles=10000" im Real Mode sowie 80% CPU-Leistung und höchstens
      20000 Cycles im Protected Mode.



==================
10. Fehlerbehebung
==================


Wenn DOSBox direkt nach dem Start abstürzt:

  - bearbeiten Sie die Konfigurationsdatei und probieren Sie verschiedene
    Werte für die Variable "output"
  - prüfen Sie, ob das Problem mit einem Update des Grafikkartentreibers
    oder von DirectX behoben werden kann.


Wenn DOSBox bei bestimmten Spielen abstürzt, sich schließt, oder sich mit
irgendwelchen Fehlermeldungen aufhängt:

  - probieren Sie, ob das Spiel mit einer frischen DOSBox-Installation
    funktioniert (unbearbeitete Konfigurationsdatei);
  - deaktivieren Sie den Sound (entweder über das Setup-Programm des Spiels
    oder mit den Einstellungen "sbtype=none" und/oder "gus=false" in der
    Konfigurationsdatei;
  - versuchen Sie unterschiedliche Einstellungen in der Konfigurationsdatei,
    insbesondere folgende (u.U. auch eine Kombination davon):
      core=normal
      statischer Cylces-Wert (z.B. "cycles=10000")
      ems=false
      xms=false
  - führen Sie vor dem Spiel das interne Programm LOADFIX aus.


Wenn DOSBox das Programm anscheinend lädt, dann aber mit einer Fehlermeldung
zur Eingabeaufforderung zurückkehrt:

  - lesen Sie die Fehlermeldung und versuchen Sie, den Fehler abzustellen;
  - gehen Sie wie beim o.g. Fehler vor;
  - versuchen Sie, das Spieleverzeichnis in einen anderen Ordner oder ein
    anderes Laufwerk einzubinden; manche Spiele lassen sich nur von bestimmten
    Pfaden aus starten; versuchen Sie z.B. statt "MOUNT D D:\GAMES\EARTH"
    den Befehl "MOUNT C D:\GAMES\EARTH" oder auch "MOUNT C D:\GAMES";
  - falls das Spiel eine eingelegte CD-ROM erwartet, stellen Sie sicher, dass
    Sie diese mit dem Parameter "-T cdrom" eingebunden haben und versuchen Sie
    auch zusätzliche Parameter (-IOCTL, -USECD, -LABEL; siehe den
    entsprechenden Abschnitt);
  - prüfen Sie, ob das Spiel auf alle Dateien zugreifen kann (Attribut
    "schreibgeschützt" entfernen, andere Programme schließen, die eventuell
    auf die Dateien zugreifen usw.);
  - versuchen Sie, das Spiel innerhalb von DOSBox neu zu installieren.



===========================
11. Die Konfigurationsdatei
===========================


Eine Konfigurationsdatei kann mithilfe von CONFIG.COM erstellt werden, das
beim Start von DOSBox auf dem internen Z-Laufwerk liegt. Nähere Informationen
zur Syntax von CONFIG finden Sie im Abschnitt "Interne Befehle/Programme"
dieser README.
Sie können die Konfigurationsdatei mit einem Text-Editor bearbeiten, um DOSBox
Ihren Bedürfnisse anzupassen.

Die Datei besteht aus verschiedenen, durch eckige Klammern gekennzeichneten
Abschnitten. In einigen Abschnitten können Sie Optionen (Werte) setzen/ändern.
Kommentarzeilen sind durch die Zeichen "#" oder "%" gekennzeichnet.
Die generierte Konfigurationsdatei beinhaltet die aktuellen Einstellungen. Sie
können diese ändern und DOSBox mit dem Parameter "-CONF Datei" starten, um die
Datei zu laden und die neuen Einstellungen zu benutzen.

Zuerst verarbeitet DOSBox die in "~/.dosboxrc" (Linux), bzw. "~\dosbox.conf"
(Windows) oder "~/Library/Preferences/DOSBox Preferences" (MacOS X)
gewählten Einstellungen. Danach verarbeitet DOSBox alle Konfigurationsdateien,
die durch den Parameter "-CONF" angegeben wurden. Falls keine angegeben wurde,
wird im aktuellen Verzeichnis nach dosbox.conf gesucht.



===================
12. Die Sprachdatei
===================


Eine Sprachdatei kann mithilfe von CONFIG.COM erstellt werden.
Lesen Sie diese, und Sie werden mit hoher Wahrscheinlichkeit verstehen, wie
man Sie anpasst.

Um Ihre neue Sprachdatei zu verwenden, starten Sie DOSBox mit dem Parameter
"-LANG Datei", alternativ können Sie den Dateinamen auch im entsprechenden
Abschnitt [dosbox] in der Konfigurationsdatei eintragen ("language=Datei").



============================================
13. Eine eigene Version von DOSBox erstellen
============================================


Laden Sie sich den Source-Code herunter und folgen Sie den Anweisungen in der
im Paket enthaltenen Datei INSTALL.



================
14. Danksagungen
================


Danksagungen der Entwickler finden Sie in der Datei THANKS.



======================
15. Kontaktinformation
======================


E-Mail-Adressen der Entwickler finden Sie unter auf der Crew-Page unter:
http://dosbox.sourceforge.net
