/*
** Copyright (C) 2007 Erik de Castro Lopo <erikd@mega-nerd.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; version 2 of the License only.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
*/

/* Generate a sound file containing a chirp */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include <sndfile.h>

static void
write_log_chirp (SNDFILE * file, int samplerate, int seconds)
{
	double w0, w1, log10_w0, log10_w1, total_samples ;
	double instantaneous_w, current_phase ;
	float * data ;
	int sec, k ;

	data = malloc (samplerate * sizeof (data [0])) ;
	if (data == NULL)
	{	printf ("\nError : malloc failed : %s\n", strerror (errno)) ;
		exit (1) ;
		} ;

	w0 = 2.0 * M_PI * 200.0 / samplerate ;
	w1 = 2.0 * M_PI * 0.5 ;

	log10_w0 = log10 (w0) ;
	log10_w1 = log10 (w1) ;

	total_samples = (1.0 * seconds) * samplerate ;

	printf ("w0 : %0.4f    w1 : %0.4f\n", w0, w1) ;

	current_phase = 0.0 ;
	instantaneous_w = w0 ;

	printf ("instantaneous angluar freq : %f (%7.1f Hz)\n", instantaneous_w, instantaneous_w * samplerate / (2.0 * M_PI)) ;

	for (sec = 0 ; sec < seconds ; sec ++)
	{	for (k = 0 ; k < samplerate ; k++)
		{	int current ;
		
			data [k] = sin (current_phase) ;

			current = sec * samplerate + k ;
			instantaneous_w = pow (10.0, log10_w0 + (log10_w1 - log10_w0) * current / total_samples) ;
			current_phase = fmod (current_phase + instantaneous_w, 2.0 * M_PI) ;
			} ;

		sf_write_float (file, data, samplerate) ;
		} ;

	printf ("instantaneous angluar freq : %f (%7.1f Hz)\n", instantaneous_w, instantaneous_w * samplerate / (2.0 * M_PI)) ;

	free (data) ;
} /* write_log_chirp */

static void
generate_file (const char * filename, int format, int samplerate, int seconds)
{
	SNDFILE * file ;
	SF_INFO info ;

	memset (&info, 0, sizeof (info)) ;

	info.format = format ;
	info.samplerate = samplerate ;
	info.channels = 1 ;

	file = sf_open (filename, SFM_WRITE, &info) ;
	if (file == NULL)
	{	printf ("\nError : Not able to create file named '%s' : %s/\n", filename, sf_strerror (NULL)) ;
		exit (1) ;
		} ;

	sf_set_string (file, SF_STR_TITLE, "Logarithmic chirp signal.") ;
	sf_set_string (file, SF_STR_SOFTWARE, "sndfile-generate-chirp") ;
	sf_set_string (file, SF_STR_COPYRIGHT, "No copyright.") ;

	write_log_chirp (file, samplerate, seconds) ;

	sf_close (file) ; 
} /* generate_file */

static int
guess_major_format (const char * filename)
{
	const char * ext ;

	ext = strrchr (filename, '.') ;
	if (ext == NULL)
	{	printf ("\nError : cannot figure out file type from file name '%s'.\n\n", filename) ;
		exit (1) ;
		} ;

	if (strcasecmp (ext, ".wav") == 0)
		return SF_FORMAT_WAV ;
	if (strcasecmp (ext, ".aif") == 0 || strcasecmp (ext, ".aiff") == 0)
		return SF_FORMAT_AIFF ;
	if (strcasecmp (ext, ".au") == 0)
		return SF_FORMAT_AU ;
	if (strcasecmp (ext, ".caf") == 0)
		return SF_FORMAT_CAF ;
	if (strcasecmp (ext, ".w64") == 0)
		return SF_FORMAT_W64 ;

	printf ("\nError : Can only generate files with extentions 'wav', 'aiff', 'aif', 'au', 'w64' and 'caf'.\n\n") ;
	exit (1) ;
	return 0 ;
} /* guess_major_format */

static void
check_int_range (const char * name, int value, int lower, int upper)
{
	if (value < lower || value > upper)
	{	printf ("Error : '%s' parameter must be in range [%d, %d]\n", name, lower, upper) ;
		exit (1) ;
		} ;
} /* check_int_range */

static void
usage_exit (const char * argv0)
{
	const char * progname ;

	progname = strrchr (argv0, '/') ;
	progname = (progname == NULL) ? argv0 : progname + 1 ;

	printf ("\nUsage :\n\n    %s <sample rate> <length in seconds> <sound file>\n\n", progname) ;

	puts ("    Create a file containing a logarithmic chirp.\n") ;

	exit (0) ;
} /* usage_exit */

int
main (int argc, char * argv [])
{
	const char * filename ;
	int samplerate, seconds, format ;

	if (argc != 4)
		usage_exit (argv [0]) ;

	samplerate = atoi (argv [1]) ;
	seconds = atoi (argv [2]) ;
	filename = argv [3] ;

	check_int_range ("sample rate", samplerate, 1000, 200 * 1000) ;
	check_int_range ("seconds", seconds, 1, 1000) ;

	format = guess_major_format (filename) | SF_FORMAT_FLOAT ;

	generate_file (filename, format, samplerate, seconds) ;

	return 0 ;
} /* main */
