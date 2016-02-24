#!/bin/bash

# Make sure we have valgrind.
valgrind=$(which valgrind)
if test $? -ne 0 ; then
	echo "Sorry, valgrind isn't installed so this test cannot be run."
	exit 0
	fi

tmpdir=tmp-`date "+%Y%m%dT%H%M%S"`

mkdir $tmpdir
logfile=$tmpdir/valgrind.txt

function vgtest {
	printf "valgrind %-28s : " $1
	$valgrind $@ > $logfile 2>&1
	errors=`grep 'ERROR SUMMARY' $logfile | sed "s/.*Y: //;s/ .*//"`
	lost=`grep 'definitely lost' $logfile | sed "s/.*: //;s/ .*//;s/,//"`
	if test -z "$lost" ; then
		lost="0"
		fi

	if test "$lost:$errors" != "0:0" ; then
		echo "$errors errors, $lost bytes leaked"
	else
		echo "ok"
		fi
}

function asantest {
	printf "asan %-28s     : " $1
	$@ > $logfile 2>&1
	if test $? -ne 0 ; then
		echo "error"
		cat $logfile
		exit 1
		fi
	echo "ok"
}

function testwrap {
	if test $(ldd $1 | grep -c libasan) -eq 0 ; then
		vgtest $@
	else
		asantest $@
		fi
}

testwrap bin/sndfile-generate-chirp 44100 1 $tmpdir/chirp.wav
testwrap bin/sndfile-resample -to 48000 -c 2 $tmpdir/chirp.wav $tmpdir/chirp2.wav
testwrap bin/sndfile-resample -to 48000 -c 3 $tmpdir/chirp.wav $tmpdir/chirp2.wav
testwrap bin/sndfile-resample -to 48000 -c 4 $tmpdir/chirp.wav $tmpdir/chirp2.wav
testwrap bin/sndfile-spectrogram $tmpdir/chirp.wav 640 480 $tmpdir/chirp.png
testwrap bin/sndfile-waveform $tmpdir/chirp.wav $tmpdir/wavform.png



rm -rf ./$tmpdir

exit 0
