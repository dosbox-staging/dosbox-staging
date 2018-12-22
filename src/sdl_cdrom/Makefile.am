SUBDIRS = macos macosx
AM_CPPFLAGS = -I$(top_srcdir)/include

noinst_LIBRARIES = libcsdlcdrom.a
EXTRA_DIST = compat_SDL_cdrom.h SDL_syscdrom.h
libcsdlcdrom_a_SOURCES = compat_SDL_cdrom.c SDL_syscdrom_aix.c SDL_syscdrom_beos.cc \
			 SDL_syscdrom_bsdi.c SDL_syscdrom_dc.c SDL_syscdrom_dummy.c \
			 SDL_syscdrom_freebsd.c SDL_syscdrom_linux.c SDL_syscdrom_mint.c \
			 SDL_syscdrom_openbsd.c SDL_syscdrom_os2.c SDL_syscdrom_osf.c \
			 SDL_syscdrom_qnx.c SDL_syscdrom_win32.c
