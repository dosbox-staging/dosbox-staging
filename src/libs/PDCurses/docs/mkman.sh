echo Definitions and Variables \(curses.h\) > MANUAL.md
echo ==================================== >> MANUAL.md
./manext.awk ../curses.h >> MANUAL.md
echo Functions >> MANUAL.md
echo ========= >> MANUAL.md
./manext.awk ../pdcurses/*.c >> MANUAL.md
./manext.awk ../x11/*.c >> MANUAL.md
