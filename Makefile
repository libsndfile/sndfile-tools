TARGETS = sndfile-spectrogram sndfile-generate-chirp sndfile-faulkner-resample
TEST_PROGS = test_kaiser_window

CC = gcc
CFLAGS = -ggdb -W -Wall -Werror -std=gnu99

SNDFILE_INC = $(shell pkg-config --cflags sndfile)
SNDFILE_LIB = $(shell pkg-config --libs sndfile)

CAIRO_INC = $(shell pkg-config --cflags cairo)
CAIRO_LIB = $(shell pkg-config --libs cairo)

FFTW_INC = $(shell pkg-config --cflags fftw3)
FFTW_LIB = $(shell pkg-config --libs fftw3)

SRC_INC = $(shell pkg-config --cflags samplerate)
SRC_LIB = $(shell pkg-config --libs samplerate)


all : $(TARGETS)

clean :
	rm -f $(TARGETS) $(TEST_PROGS) *.o

check : $(TEST_PROGS)
	./test_kaiser_window

sndfile-spectrogram : window.o sndfile-spectrogram.c
	$(CC) $(CFLAGS) $(CAIRO_INC) $(SNDFILE_INC) $(FFTW_INC) $^ $(CAIRO_LIB) $(SNDFILE_LIB) $(FFTW_LIB) -lm -o $@

sndfile-generate-chirp : sndfile-generate-chirp.c
	$(CC) $(CFLAGS) $(SNDFILE_INC) $^ $(SNDFILE_LIB) -lm -o $@

sndfile-faulkner-resample : sndfile-faulkner-resample.c
	$(CC) $(CFLAGS) $(SNDFILE_INC) $(SRC_INC) $^ $(SNDFILE_LIB) $(SRC_LIB) -lm -o $@


#---------------------------------------------------------------------
# Test programs.

test_kaiser_window : window.o test_kaiser_window.c
	$(CC) $(CFLAGS) $^ -lm -o $@


#---------------------------------------------------------------------
# Dependancies.

window.o : window.c window.h

