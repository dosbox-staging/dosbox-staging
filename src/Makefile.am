AM_CPPFLAGS = -I$(top_srcdir)/include

SUBDIRS = cpu debug dos fpu gui hardware libs ints midi misc shell platform

bin_PROGRAMS = dosbox

if HAVE_WINDRES
ico_stuff = winres.rc
endif

.rc.o:
	$(WINDRES) -o $@ $<

dosbox_SOURCES = dosbox.cpp $(ico_stuff) main.cpp
dosbox_LDADD = cpu/libcpu.a \
               debug/libdebug.a \
               dos/libdos.a \
               fpu/libfpu.a \
               gui/libgui.a \
               hardware/libhardware.a \
               hardware/mame/libmame.a \
               hardware/serialport/libserial.a \
               ints/libints.a \
               libs/decoders/libdecoders.a \
               libs/gui_tk/libgui_tk.a \
               libs/nuked/libnuked.a \
               libs/ppscale/libppscale.a \
               midi/libmidi.a \
               misc/libmisc.a \
               shell/libshell.a

EXTRA_DIST = winres.rc
