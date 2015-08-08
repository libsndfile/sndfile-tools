
# Tell automake to put the object file in the same dir as the source file.
AUTOMAKE_OPTIONS = subdir-objects

ACLOCAL_AMFLAGS = -I M4

EXTRA_DIST = Readme.md M4/lrint.m4 M4/lrintf.m4 doc/index.html doc/sweep.png \
	tests/valgrind-test.sh Scripts/calc-hilbert-fir.m $(man_MANS)

bin_PROGRAMS = bin/sndfile-generate-chirp bin/sndfile-spectrogram \
	bin/sndfile-mix-to-mono bin/sndfile-jackplay bin/sndfile-resample \
	bin/sndfile-waveform

# Test programs or programs not yet working.
noinst_PROGRAMS = tests/kaiser_window_test tests/common_tests

AM_CPPFLAGS = $(SNDFILE_CFLAGS) $(FFTW3_CFLAGS) $(CAIRO_CFLAGS) $(JACK_CFLAGS) \
			$(SAMPLERATE_CFLAGS)

CLEANFILES = src/*~

man_MANS = man/sndfile-generate-chirp.1 man/sndfile-spectrogram.1 \
	man/sndfile-mix-to-mono.1 man/sndfile-jackplay.1

#=========================================================================================
# The bin targets.

bin_sndfile_generate_chirp_SOURCES = src/generate-chirp.c src/common.c src/common.h
bin_sndfile_generate_chirp_LDADD = $(SNDFILE_LIBS)

bin_sndfile_spectrogram_SOURCES = src/spectrogram.c src/window.c src/common.c \
	src/window.h src/common.h
bin_sndfile_spectrogram_LDADD = $(SNDFILE_LIBS) $(FFTW3_LIBS) $(CAIRO_LIBS)

bin_sndfile_mix_to_mono_SOURCES = src/mix-to-mono.c src/common.h src/common.c
bin_sndfile_mix_to_mono_LDADD = $(SNDFILE_LIBS)

bin_sndfile_waveform_SOURCES = src/waveform.c src/common.c src/common.h
bin_sndfile_waveform_LDADD = $(SNDFILE_LIBS) $(CAIRO_LIBS)

bin_sndfile_jackplay_SOURCES = src/jackplay.c
bin_sndfile_jackplay_LDADD = $(SNDFILE_LIBS) $(JACK_LIBS)

bin_sndfile_resample_SOURCES = src/resample.c src/common.c src/common.h
bin_sndfile_resample_LDADD = $(SNDFILE_LIBS) $(SAMPLERATE_LIBS)

#=========================================================================================
# The make check targets.

tests_kaiser_window_test_SOURCES = tests/kaiser_window_test.c src/window.h src/window.c

tests_common_tests_SOURCES = tests/common_tests.c src/common.c src/common.h
tests_common_tests_LDADD = $(SNDFILE_LIBS) $(FFTW3_LIBS)

check : $(noinst_PROGRAMS) $(bin_PROGRAMS)
	@$(top_builddir)/tests/common_tests
	@$(top_builddir)/tests/kaiser_window_test
	@$(top_srcdir)/tests/valgrind-test.sh
