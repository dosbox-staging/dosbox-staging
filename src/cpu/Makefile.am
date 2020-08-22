AM_CPPFLAGS = -I$(top_srcdir)/include

SUBDIRS = \
	core_dynrec \
	core_dyn_x86 \
	core_full \
	core_normal

noinst_LIBRARIES = libcpu.a

libcpu_a_SOURCES = \
	callback.cpp \
	core_dynrec.cpp \
	core_dyn_x86.cpp \
	core_full.cpp \
	core_normal.cpp \
	core_prefetch.cpp \
	core_simple.cpp \
	cpu.cpp \
	dyn_cache.h \
	flags.cpp \
	instructions.h	\
	lazyflags.h \
	modrm.cpp \
	modrm.h \
	paging.cpp
