# GNU Makefile for PDCurses - OS/2
#
# Usage: make [-f path\Makefile] [DEBUG=Y] [target]
#
# where target can be any of:
# [all|demos|pdcurses.a|testcurs.exe...]

O = o
E = .exe
RM = del

ifndef PDCURSES_SRCDIR
	PDCURSES_SRCDIR = ..
endif

include $(PDCURSES_SRCDIR)/common/libobjs.mif

osdir		= $(PDCURSES_SRCDIR)/os2

PDCURSES_OS2_H	= $(osdir)/pdcos2.h

CC		= gcc

CFLAGS = -c -I$(PDCURSES_SRCDIR) -Ic:/emx/include -Wall

ifeq ($(DEBUG),Y)
	CFLAGS  += -g -DPDCDEBUG
	LDFLAGS = -g
else
	CFLAGS  += -O2
	LDFLAGS =
endif

LINK		= gcc
EMXBIND		= emxbind
EMXOMF		= emxomf

LIBEXE		= ar
LIBFLAGS	= rcv

LIBCURSES = pdcurses.a
LIBDEPS = $(LIBOBJS) $(PDCOBJS)
PDCLIBS = $(LIBCURSES)
EXEPOST = $(EMXBIND) $*
TUIPOST = $(EMXBIND) tuidemo
CLEAN = *.a testcurs ozdemo xmas tuidemo firework ptest rain worm

.PHONY: all libs clean demos dist

all:	libs

libs:	$(PDCLIBS)

clean:
	-$(RM) *.o
	-$(RM) *.exe
	-$(RM) $(CLEAN)

demos:	$(DEMOS)

DEMOOBJS = testcurs.o ozdemo.o xmas.o tui.o tuidemo.o firework.o \
ptest.o rain.o worm.o

$(LIBCURSES) : $(LIBDEPS)
	$(LIBEXE) $(LIBFLAGS) $@ $?

$(LIBOBJS) $(PDCOBJS) $(DEMOOBJS) : $(PDCURSES_HEADERS)
$(PDCOBJS) : $(PDCURSES_OS2_H)
$(DEMOS) : $(LIBCURSES)
panel.o ptest.o: $(PANEL_HEADER)

$(LIBOBJS) : %.o: $(srcdir)/%.c
	$(CC) $(CFLAGS) -o$@ $<

$(PDCOBJS) : %.o: $(osdir)/%.c
	$(CC) $(CFLAGS) -o$@ $<

firework.exe ozdemo.exe rain.exe testcurs.exe worm.exe xmas.exe \
ptest.exe: %.exe: %.o
	$(LINK) $(LDFLAGS) -o $* $< $(LIBCURSES)
	$(EXEPOST)

tuidemo.exe:	tuidemo.o tui.o
	$(LINK) $(LDFLAGS) -o tuidemo tuidemo.o tui.o $(LIBCURSES)
	$(TUIPOST)

firework.o ozdemo.o ptest.o rain.o testcurs.o worm.o xmas.o: %.o: \
$(demodir)/%.c
	$(CC) $(CFLAGS) -o$@ $<

tui.o: $(demodir)\tui.c $(demodir)\tui.h
	$(CC) $(CFLAGS) -I$(demodir) -o $@ $<

tuidemo.o: $(demodir)\tuidemo.c
	$(CC) $(CFLAGS) -I$(demodir) -o $@ $<
