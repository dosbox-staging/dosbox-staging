AM_CPPFLAGS = -I$(top_srcdir)/include

SUBDIRS = cpu debug dos fpu gui hardware libs ints misc shell platform 

bin_PROGRAMS = dosbox

dosbox_SOURCES = dosbox.cpp
dosbox_LDADD = cpu/libcpu.a debug/libdebug.a dos/libdos.a fpu/libfpu.a  hardware/libhardware.a gui/libgui.a \
               ints/libints.a misc/libmisc.a shell/libshell.a hardware/serialport/libserial.a
	       
if HAVE_WINDRES
dosbox_LDADD += dosbox_ico.o
endif

EXTRA_DIST = dosbox.rc dosbox.ico
dosbox_ico.o: dosbox.rc dosbox.ico
	$(WINDRES) dosbox.rc dosbox_ico.o

