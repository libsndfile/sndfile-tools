TARGETS = sndfile-spectrogram sndfile-generate-chirp

CC = gcc
CFLAGS = -ggdb -W -Wall -Werror -std=gnu99

SNDFILE_INC = $(shell pkg-config --cflags sndfile)
SNDFILE_LIB = $(shell pkg-config --libs sndfile)

CAIRO_INC = $(shell pkg-config --cflags cairo)
CAIRO_LIB = $(shell pkg-config --libs cairo)

FFTW_INC = $(shell pkg-config --cflags fftw3)
FFTW_LIB = $(shell pkg-config --libs fftw3)


all : $(TARGETS)

clean :
	rm -f $(TARGETS)

sndfile-spectrogram : sndfile-spectrogram.c
	$(CC) $(CFLAGS) $(CAIRO_INC) $(SNDFILE_INC) $(FFTW_INC) $^ $(CAIRO_LIB) $(SNDFILE_LIB) $(FFTW_LIB) -lm -o $@

sndfile-generate-chirp : sndfile-generate-chirp.c
	$(CC) $(CFLAGS) $(SNDFILE_INC) $^ $(SNDFILE_LIB) -lm -o $@
