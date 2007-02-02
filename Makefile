TARGETS = sndfile-spectrogram sndfile-generate-chirp sndfile-faulkner-resample sndfile-mix-to-mono
TEST_PROGS = src/test_kaiser_window

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
	rm -f $(TARGETS) $(TEST_PROGS) src/*.o

check : $(TEST_PROGS)
	src/test_kaiser_window

sndfile-spectrogram : src/window.o src/common.o src/sndfile-spectrogram.c
	$(CC) $(CFLAGS) $(CAIRO_INC) $(SNDFILE_INC) $(FFTW_INC) $^ $(CAIRO_LIB) $(SNDFILE_LIB) $(FFTW_LIB) -lm -o $@

sndfile-generate-chirp : src/sndfile-generate-chirp.c
	$(CC) $(CFLAGS) $(SNDFILE_INC) $^ $(SNDFILE_LIB) -lm -o $@

sndfile-mix-to-mono : src/common.o src/sndfile-mix-to-mono.c
	$(CC) $(CFLAGS) $(SNDFILE_INC) $^ $(SNDFILE_LIB) -lm -o $@

sndfile-faulkner-resample : src/sndfile-faulkner-resample.c
	$(CC) $(CFLAGS) $(SNDFILE_INC) $(SRC_INC) $^ $(SNDFILE_LIB) $(SRC_LIB) -lm -o $@

src/common.o : src/common.c src/common.h
	$(CC) $(CFLAGS) $(SNDFILE_INC) $< -c -o $@
	

#---------------------------------------------------------------------
# Test programs.

src/test_kaiser_window : src/window.o src/test_kaiser_window.c
	$(CC) $(CFLAGS) $^ -lm -o $@


#---------------------------------------------------------------------
# Dependancies.

src/window.o : src/window.c src/window.h
src/common.o : src/common.c src/common.h

