AM_CPPFLAGS = -I$(top_srcdir)/include

SUBDIRS = cpu debug dos fpu gui hardware libs ints misc shell platform

bin_PROGRAMS = dosbox

if HAVE_WINDRES
ico_stuff = winres.rc
endif

.rc.o:
	$(WINDRES) -o $@ $<

dosbox_SOURCES = dosbox.cpp $(ico_stuff)
dosbox_LDADD = cpu/libcpu.a debug/libdebug.a dos/libdos.a fpu/libfpu.a  hardware/libhardware.a gui/libgui.a \
               ints/libints.a misc/libmisc.a shell/libshell.a hardware/mame/libmame.a \
               hardware/serialport/libserial.a libs/gui_tk/libgui_tk.a libs/decoders/libdecoders.a \
               libs/decoders/internal/lib/libopusfile.a libs/decoders/internal/lib/libspeexdsp.a \
               libs/decoders/internal/lib/libopus.a libs/decoders/internal/lib/libogg.a

libs/decoders/internal/lib/libopusfile.a:
	cd libs/decoders/internal \
	&& $(MAKE) -j$(THREADS) lib/libopusfile.a

libs/decoders/internal/lib/libspeexdsp.a:
	cd libs/decoders/internal \
	&& $(MAKE) -j$(THREADS) lib/libspeexdsp.a

EXTRA_DIST = winres.rc dosbox.ico

LIBS +=  -lmt32emu
