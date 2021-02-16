if BUILD_TESTS

AM_CPPFLAGS = -I$(top_srcdir)/include

SUBDIRS = ../src/misc

bin_PROGRAMS = tests

tests_CXXFLAGS = $(AM_CXXFLAGS) $(CXXFLAGS)
if USE_GTEST_STATIC
tests_CXXFLAGS += -Wl,-Bstatic -lgtest_main -lgtest -Wl,-Bdynamic -pthread
else
tests_CXXFLAGS += -lgtest_main -lgtest -pthread
endif

tests_SOURCES = \
	example.cpp \
	fs_utils.cpp \
	readerwritercircularbuffer.cpp \
	setup.cpp \
	soft_limiter.cpp \
	string_utils.cpp \
	stubs.cpp \
	support.cpp

tests_LDADD = ../src/misc/libmisc.a

# Override automake's distclean target to prevent it recursing into
# SUBDIRS and failing (it was already invoked in there via src/Makefile.am).
distclean:
	-rm tests$(EXEEXT)
	-rm *.o
	-rm Makefile

endif
