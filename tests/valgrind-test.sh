#!/bin/bash -e

# Make sure we have valgrind.
valgrind=`which valgrind`
test -x "$valgrind" || (echo "Error : Can't find valgrind." ; exit 1)

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


vgtest bin/sndfile-generate-chirp 44100 1 $tmpdir/chirp.wav
vgtest bin/sndfile-resample -to 48000 -c 2 $tmpdir/chirp.wav $tmpdir/chirp2.wav
vgtest bin/sndfile-resample -to 48000 -c 3 $tmpdir/chirp.wav $tmpdir/chirp2.wav
vgtest bin/sndfile-resample -to 48000 -c 4 $tmpdir/chirp.wav $tmpdir/chirp2.wav
vgtest bin/sndfile-spectrogram $tmpdir/chirp.wav 640 480 $tmpdir/chirp.png
vgtest bin/sndfile-waveform $tmpdir/chirp.wav $tmpdir/wavform.png



rm -rf ./$tmpdir

exit 0
