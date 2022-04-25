# Makefile for PDCurses for SDL

O = o

ifndef PDCURSES_SRCDIR
	PDCURSES_SRCDIR	= ..
endif

include $(PDCURSES_SRCDIR)/common/libobjs.mif

osdir		= $(PDCURSES_SRCDIR)/sdl1

PDCURSES_SDL_H	= $(osdir)/pdcsdl.h

SFLAGS		= $(shell sdl-config --cflags)
SLIBS		= $(shell sdl-config --libs)

ifeq ($(DEBUG),Y)
	CFLAGS  = -g -Wall -DPDCDEBUG -fPIC
else
	CFLAGS  = -O2 -Wall -fPIC
endif

ifeq ($(WIDE),Y)
	CFLAGS	+= -DPDC_WIDE
	SLIBS	+= -lSDL_ttf
endif

ifeq ($(UTF8),Y)
	CFLAGS	+= -DPDC_FORCE_UTF8
endif

BUILD		= $(CC) $(CFLAGS) -I$(PDCURSES_SRCDIR)

LINK		= $(CC)
LDFLAGS		= $(LIBCURSES) $(SLIBS)
RANLIB		= ranlib
LIBCURSES	= pdcurses.a

DEMOS		+= sdltest

.PHONY: all libs clean demos

all:	libs

libs:	$(LIBCURSES)

clean:
	-rm -rf *.o trace $(LIBCURSES) $(DEMOS)

demos:	$(DEMOS)
ifneq ($(DEBUG),Y)
	strip $(DEMOS)
endif

$(LIBCURSES) : $(LIBOBJS) $(PDCOBJS)
	ar rv $@ $?
	-$(RANLIB) $@

$(LIBOBJS) $(PDCOBJS) : $(PDCURSES_HEADERS)
$(PDCOBJS) : $(PDCURSES_SDL_H)
$(DEMOS) : $(PDCURSES_CURSES_H) $(LIBCURSES)
tui.o tuidemo.o : $(PDCURSES_CURSES_H)
panel.o ptest: $(PANEL_HEADER)

$(LIBOBJS) : %.o: $(srcdir)/%.c
	$(BUILD) -c $<

$(PDCOBJS) : %.o: $(osdir)/%.c
	$(BUILD) $(SFLAGS) -c $<

firework: $(demodir)/firework.c
	$(BUILD) $(DEMOFLAGS) -o $@ $< $(LDFLAGS)

ozdemo: $(demodir)/ozdemo.c
	$(BUILD) $(DEMOFLAGS) -o $@ $< $(LDFLAGS)

ptest: $(demodir)/ptest.c
	$(BUILD) $(DEMOFLAGS) -o $@ $< $(LDFLAGS)

rain: $(demodir)/rain.c
	$(BUILD) $(DEMOFLAGS) -o $@ $< $(LDFLAGS)

testcurs: $(demodir)/testcurs.c
	$(BUILD) $(DEMOFLAGS) -o $@ $< $(LDFLAGS)

tuidemo: tuidemo.o tui.o
	$(LINK) tui.o tuidemo.o -o $@ $(LDFLAGS)

worm: $(demodir)/worm.c
	$(BUILD) $(DEMOFLAGS) -o $@ $< $(LDFLAGS)

xmas: $(demodir)/xmas.c
	$(BUILD) $(DEMOFLAGS) -o $@ $< $(LDFLAGS)

sdltest: $(osdir)/sdltest.c
	$(BUILD) $(DEMOFLAGS) $(SFLAGS) -o $@ $< $(LDFLAGS)

tui.o: $(demodir)/tui.c $(demodir)/tui.h
	$(BUILD) -c $(DEMOFLAGS) $(demodir)/tui.c

tuidemo.o: $(demodir)/tuidemo.c
	$(BUILD) -c $(DEMOFLAGS) $(demodir)/tuidemo.c

include $(demodir)/nctests.mif
