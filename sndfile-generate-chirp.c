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

typedef double (*freq_func_t) (double w0, double w1, double total_length) ;

static void usage_exit (const char * argv0) ;
static void check_int_range (const char * name, int value, int lower, int upper) ;
static freq_func_t parse_sweep_type (const char * name) ;
static int guess_major_format (const char * filename) ;
static void generate_file (const char * filename, int format, int samplerate, int seconds, freq_func_t sweep_func) ;


int
main (int argc, char * argv [])
{
	const char * filename ;
	freq_func_t sweep_func ;
	int samplerate, seconds, format ;

	if (argc != 5)
		usage_exit (argv [0]) ;

	sweep_func = parse_sweep_type (argv [1]) ;
	samplerate = atoi (argv [2]) ;
	seconds = atoi (argv [3]) ;
	filename = argv [4] ;

	check_int_range ("sample rate", samplerate, 1000, 200 * 1000) ;
	check_int_range ("seconds", seconds, 1, 1000) ;

	format = guess_major_format (filename) | SF_FORMAT_FLOAT ;

	generate_file (filename, format, samplerate, seconds, sweep_func) ;

	return 0 ;
} /* main */

/*==============================================================================
*/

static void
usage_exit (const char * argv0)
{
	const char * progname ;

	progname = strrchr (argv0, '/') ;
	progname = (progname == NULL) ? argv0 : progname + 1 ;

	puts ("\nCreate a sound file containing a swept sine wave (ie a chirp).") ;

	printf ("\nUsage :\n\n    %s <sweep type> <sample rate> <length in seconds> <sound file>\n\n", progname) ;

	puts (
		"    Where <sweep type> is one of :\n"
		"           -log      logarithmic sweep\n"
		"           -qrad     quadratic sweep\n"
		"           -linear   linear sweep\n"
		) ;

	puts (
		"    The output file will contain floating point samples in the range [-1.0, 1.0].\n"
		"    The ouput file type is determined by the file name extension which should be one\n"
		"    of 'wav', 'aif', 'aiff', 'au', 'caf' and 'w64'.\n"
		) ;

	exit (0) ;
} /* usage_exit */

static void
check_int_range (const char * name, int value, int lower, int upper)
{
	if (value < lower || value > upper)
	{	printf ("Error : '%s' parameter must be in range [%d, %d]\n", name, lower, upper) ;
		exit (1) ;
		} ;
} /* check_int_range */


static void
write_chirp (SNDFILE * file, int samplerate, int seconds, double w0, double w1, freq_func_t sweep_func)
{
	double total_samples ;
	double instantaneous_w, current_phase ;
	float * data ;
	int sec, k ;

	data = malloc (samplerate * sizeof (data [0])) ;
	if (data == NULL)
	{	printf ("\nError : malloc failed : %s\n", strerror (errno)) ;
		exit (1) ;
		} ;

	total_samples = (1.0 * seconds) * samplerate ;

	current_phase = 0.0 ;
	instantaneous_w = w0 ;

	printf ("Start frequency : %8.1f Hz (%f rad/sec)\n", instantaneous_w * samplerate / (2.0 * M_PI), instantaneous_w) ;

	for (sec = 0 ; sec < seconds ; sec ++)
	{	for (k = 0 ; k < samplerate ; k++)
		{	int current ;
		
			data [k] = sin (current_phase) ;

			current = sec * samplerate + k ;
			instantaneous_w = sweep_func (w0, w1, current / total_samples) ;
			current_phase = fmod (current_phase + instantaneous_w, 2.0 * M_PI) ;
			} ;

		sf_write_float (file, data, samplerate) ;
		} ;

	printf ("End   frequency : %8.1f Hz (%f rad/sec)\n", instantaneous_w * samplerate / (2.0 * M_PI), instantaneous_w) ;

	free (data) ;
} /* write_linear_chirp */

static void
generate_file (const char * filename, int format, int samplerate, int seconds, freq_func_t sweep_func)
{
	SNDFILE * file ;
	SF_INFO info ;
	double w0, w1 ;

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

	w0 = 2.0 * M_PI * 200.0 / samplerate ;
	w1 = 2.0 * M_PI * 0.5 ;

	write_chirp (file, samplerate, seconds, w0, w1, sweep_func) ;

	sf_close (file) ; 
} /* generate_file */

static double
log_freq_func (double w0, double w1, double indx)
{	return pow (10.0, log10 (w0) + (log10 (w1) - log10 (w0)) * indx) ;
} /* log_freq_func */

static double
quad_freq_func (double w0, double w1, double indx)
{
	puts ("quad_freq_func not working yet.") ;
	exit (1) ;
	return pow (10.0, log10 (w0) + (log10 (w1) - log10 (w0)) * indx) ;
} /* log_freq_func */

static double
linear_freq_func (double w0, double w1, double indx)
{	return w0 + (w1 - w0) * indx ;
} /* linear_freq_func */

static freq_func_t
parse_sweep_type (const char * name)
{
	if (strcmp (name, "-log") == 0)
		return log_freq_func ;
	if (strcmp (name, "-quad") == 0)
		return quad_freq_func ;
	if (strcmp (name, "-linear") == 0)
		return linear_freq_func ;

	printf ("\nError : Bad sweep type. Should be one of '-log', '-quad' and '-linear'.\n\n") ;
	exit (1) ;
	return NULL ;
} /* parse_sweep_type */

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

