
EXTRA_DIST = M4/lrint.m4 M4/lrintf.m4


# Tell automake to put the object file for apple/apple.c in dir apple/
AUTOMAKE_OPTIONS := subdir-objects

# The installable executable.
bin_PROGRAMS = src/sndfile-generate-chirp src/sndfile-spectrogram \
	src/sndfile-mix-to-mono

INCLUDES = $(SNDFILE_CFLAGS) $(FFTW3_CFLAGS) $(CAIRO_CFLAGS)

#=========================================================================================
# The bin targets.

src_sndfile_generate_chirp_SOURCES = src/sndfile-generate-chirp.c
src_sndfile_generate_chirp_LDADD = $(SNDFILE_LIBS)

src_sndfile_spectrogram_SOURCES = src/sndfile-spectrogram.c src/window.h src/common.h \
	src/window.c src/common.c
src_sndfile_spectrogram_LDADD = $(SNDFILE_LIBS) $(FFTW3_LIBS) $(CAIRO_LIBS)

src_sndfile_mix_to_mono_SOURCES = src/sndfile-mix-to-mono.c src/common.h src/common.c
src_sndfile_mix_to_mono_LDADD = $(SNDFILE_LIBS)



