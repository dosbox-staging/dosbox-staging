AM_CPPFLAGS = -I$(top_srcdir)/include

SUBDIRS = cpu debug dos fpu gui hardware libs ints misc shell platform 

bin_PROGRAMS = dosbox

if HAVE_WINDRES
ico_stuff = dosbox_ico.o
endif


dosbox_SOURCES = dosbox.cpp
dosbox_LDADD = cpu/libcpu.a debug/libdebug.a dos/libdos.a fpu/libfpu.a  hardware/libhardware.a gui/libgui.a \
               ints/libints.a misc/libmisc.a shell/libshell.a hardware/serialport/libserial.a \
	       $(ico_stuff)

EXTRA_DIST = dosbox.rc dosbox.ico
dosbox_ico.o: dosbox.rc dosbox.ico
	$(WINDRES) dosbox.rc dosbox_ico.o

