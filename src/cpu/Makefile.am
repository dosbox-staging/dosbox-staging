SUBDIRS = core_16 core_full
AM_CPPFLAGS = -I$(top_srcdir)/include

noinst_LIBRARIES = libcpu.a
libcpu_a_SOURCES = callback.cpp cpu.cpp flags.cpp modrm.cpp modrm.h slow_16.cpp core_full.cpp instructions.h	\
		   paging.cpp lazyflags.h 