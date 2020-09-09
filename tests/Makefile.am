if BUILD_TESTS

AM_CPPFLAGS = -I$(top_srcdir)/include

SUBDIRS = ../src/misc

bin_PROGRAMS = tests

tests_CXXFLAGS = $(AM_CXXFLAGS) $(CXXFLAGS)
if USE_GTEST_STATIC
tests_CXXFLAGS += -Wl,-Bstatic -lgtest_main -lgtest -Wl,-Bdynamic
else
tests_CXXFLAGS += -lgtest_main -lgtest
endif

tests_SOURCES = \
	example.cpp \
	support.cpp

tests_LDADD = ../src/misc/libmisc.a

endif
