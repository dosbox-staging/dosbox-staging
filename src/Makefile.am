AM_CPPFLAGS = -I$(top_srcdir)/include

SUBDIRS = cpu debug dos fpu gui hardware ints misc shell platform 

bin_PROGRAMS = dosbox

dosbox_SOURCES = dosbox.cpp
dosbox_LDADD = cpu/libcpu.a debug/libdebug.a dos/libdos.a fpu/libfpu.a  hardware/libhardware.a gui/libgui.a \
               ints/libints.a misc/libmisc.a shell/libshell.a -lcurses -lpng

