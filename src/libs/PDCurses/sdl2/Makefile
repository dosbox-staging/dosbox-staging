# Makefile for PDCurses for SDL2

O = o

ifndef PDCURSES_SRCDIR
	PDCURSES_SRCDIR	= ..
endif

osdir		= $(PDCURSES_SRCDIR)/sdl2
common		= $(PDCURSES_SRCDIR)/common

include $(common)/libobjs.mif

ifeq ($(OS),Windows_NT)
	E = .exe
	CC = gcc
	RM = cmd /c del

	include $(osdir)/versions.mif

	PLATFORM = $(shell $(CC) -dumpmachine)

	SFLAGS = -I$(SDLBASE)/$(PLATFORM)/include/SDL2
	SLIBS = -L$(SDLBASE)/$(PLATFORM)/lib -lSDL2main -lSDL2

	TFLAGS = -I$(TTFBASE)/$(PLATFORM)/include/SDL2
	TLIBS =-L$(TTFBASE)/$(PLATFORM)/lib -lSDL2_ttf

	DEMOFLAGS = -mwindows
else
	RM = rm -f

	SFLAGS = $(shell sdl2-config --cflags)
	SLIBS = $(shell sdl2-config --libs)

	TLIBS = -lSDL2_ttf
endif

PDCURSES_SDL_H	= $(osdir)/pdcsdl.h

ifeq ($(DEBUG),Y)
	CFLAGS  = -g -Wall -DPDCDEBUG -fPIC
else
	CFLAGS  += -O2 -Wall -fPIC
endif

ifeq ($(WIDE),Y)
	CFLAGS += -DPDC_WIDE
	SFLAGS += $(TFLAGS)
	SLIBS  += $(TLIBS)
endif

ifeq ($(UTF8),Y)
	CFLAGS += -DPDC_FORCE_UTF8
endif

ifeq ($(OS)_$(DLL),Windows_NT_Y)
	CFLAGS += -DPDC_DLL_BUILD
	LIBEXE = $(CC)
	LIBFLAGS = -Wl,--out-implib,pdcurses.a -shared -o
	LIBCURSES = pdcurses.dll
	CLEAN = $(LIBCURSES) *.a
	RESOURCE = pdcurses.o
	LIBLIBS = $(SLIBS)
	LDFLAGS = $(LIBCURSES)
else
	LIBEXE = $(AR)
	LIBFLAGS = rcv
	LIBCURSES = pdcurses.a
	LDFLAGS = $(LIBCURSES) $(SLIBS)
	CLEAN = *.a
endif

BUILD		= $(CC) $(CFLAGS) -I$(PDCURSES_SRCDIR)

LINK		= $(CC)

DEMOS		+= sdltest$(E)

.PHONY: all libs clean demos

all:	libs

libs:	$(LIBCURSES)

clean:
	-$(RM) *.o trace $(CLEAN) $(DEMOS)

demos:	$(DEMOS)
ifneq ($(DEBUG),Y)
	strip $(DEMOS)
endif

$(LIBCURSES) : $(LIBOBJS) $(PDCOBJS) $(RESOURCE)
	$(LIBEXE) $(LIBFLAGS) $@ $? $(LIBLIBS)

pdcurses.o: $(common)/pdcurses.rc
	windres -i $(common)/pdcurses.rc pdcurses.o

$(LIBOBJS) $(PDCOBJS) : $(PDCURSES_HEADERS)
$(PDCOBJS) : $(PDCURSES_SDL_H)
$(DEMOS) : $(PDCURSES_CURSES_H) $(LIBCURSES)
tui.o tuidemo.o : $(PDCURSES_CURSES_H)
panel.o ptest$(E): $(PANEL_HEADER)

$(LIBOBJS) : %.o: $(srcdir)/%.c
	$(BUILD) -c $<

$(PDCOBJS) : %.o: $(osdir)/%.c
	$(BUILD) $(SFLAGS) -c $<

firework$(E): $(demodir)/firework.c
	$(BUILD) $(DEMOFLAGS) -o $@ $< $(LDFLAGS)

ozdemo$(E): $(demodir)/ozdemo.c
	$(BUILD) $(DEMOFLAGS) -o $@ $< $(LDFLAGS)

ptest$(E): $(demodir)/ptest.c
	$(BUILD) $(DEMOFLAGS) -o $@ $< $(LDFLAGS)

rain$(E): $(demodir)/rain.c
	$(BUILD) $(DEMOFLAGS) -o $@ $< $(LDFLAGS)

testcurs$(E): $(demodir)/testcurs.c
	$(BUILD) $(DEMOFLAGS) -o $@ $< $(LDFLAGS)

tuidemo$(E): tuidemo.o tui.o
	$(LINK) tui.o tuidemo.o -o $@ $(LDFLAGS)

worm$(E): $(demodir)/worm.c
	$(BUILD) $(DEMOFLAGS) -o $@ $< $(LDFLAGS)

xmas$(E): $(demodir)/xmas.c
	$(BUILD) $(DEMOFLAGS) -o $@ $< $(LDFLAGS)

sdltest$(E): $(osdir)/sdltest.c
	$(BUILD) $(SFLAGS) $(DEMOFLAGS) -o $@ $< $(LIBCURSES) $(SLIBS)

tui.o: $(demodir)/tui.c $(demodir)/tui.h
	$(BUILD) -c $(DEMOFLAGS) $(demodir)/tui.c

tuidemo.o: $(demodir)/tuidemo.c
	$(BUILD) -c $(DEMOFLAGS) $(demodir)/tuidemo.c

include $(demodir)/nctests.mif
