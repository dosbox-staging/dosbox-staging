noinst_LIBRARIES = libdecoders.a

libdecoders_a_SOURCES = \
	archive.h            \
	dr_flac.h            \
	dr_mp3.h             \
	dr_wav.h             \
	flac.c               \
	mp3.cpp              \
	mp3_seek_table.cpp   \
	mp3_seek_table.h     \
	opus.c               \
	SDL_sound.c          \
	SDL_sound.h          \
	SDL_sound_internal.h \
	stb.h                \
	stb_vorbis.h         \
	vorbis.c             \
	wav.c                \
	xxh3.h               \
	xxhash.c             \
	xxhash.h

libdecoders_a_CXXFLAGS = \
	$(AM_CXXFLAGS) \
	$(CXXFLAGS) \
	-Wpedantic \
	-Wall

libdecoders_a_CFLAGS = \
	$(AM_CFLAGS) \
	$(CFLAGS) \
	-Wpedantic \
	-Wall
