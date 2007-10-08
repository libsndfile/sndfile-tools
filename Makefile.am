
# Tell automake to put the object file in the same dir as the source file.
AUTOMAKE_OPTIONS := subdir-objects

EXTRA_DIST = M4/lrint.m4 M4/lrintf.m4

bin_PROGRAMS = src/sndfile-generate-chirp src/sndfile-spectrogram \
	src/sndfile-mix-to-mono

noinst_PROGRAMS = src/kaiser_window_test

INCLUDES = $(SNDFILE_CFLAGS) $(FFTW3_CFLAGS) $(CAIRO_CFLAGS)

CLEANFILES = src/*~

#=========================================================================================
# The bin targets.

src_sndfile_generate_chirp_SOURCES = src/sndfile-generate-chirp.c
src_sndfile_generate_chirp_LDADD = $(SNDFILE_LIBS)

src_sndfile_spectrogram_SOURCES = src/sndfile-spectrogram.c src/window.h src/common.h \
	src/window.c src/common.c
src_sndfile_spectrogram_LDADD = $(SNDFILE_LIBS) $(FFTW3_LIBS) $(CAIRO_LIBS)

src_sndfile_mix_to_mono_SOURCES = src/sndfile-mix-to-mono.c src/common.h src/common.c
src_sndfile_mix_to_mono_LDADD = $(SNDFILE_LIBS)

#=========================================================================================
# The make check targets.

src_kaiser_window_test_SOURCES = src/kaiser_window_test.c src/window.h src/window.c

check : $(noinst_PROGRAMS)
	src/kaiser_window_test
