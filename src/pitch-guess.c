/*
** Copyright (C) 2010 Erik de Castro Lopo <erikd@mega-nerd.com>
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 or version 3 of the
** License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
**	Guess the pitch of a given sound file.
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

#if (HAVE_FFTW3)

#include <fftw3.h>

#include "common.h"
#include "window.h"

#define	FFT_LEN			(1 << 16)

#define ARRAY_LEN(x)	((int) (sizeof (x) / sizeof (x [0])))

#define LOG_FLOOR		40.0

static void usage_exit (const char *progname) ;
static void pitch_guess (SNDFILE * file, const SF_INFO * sfinfo) ;

int
main (int argc, char *argv [])
{	SNDFILE	*file = NULL ;
	SF_INFO sfinfo ;

	if (argc != 2)
		usage_exit (argv [0]) ;

	if ((file = sf_open (argv [1], SFM_READ, &sfinfo)) == NULL)
	{	printf ("Error : Not able to open input file '%s'\n", argv [argc - 2]) ;
		exit (1) ;
		} ;

	if (sfinfo.channels != 1)
		puts ("\nSorry, this only works with monophonic files.\n") ;
	else if (sfinfo.frames < FFT_LEN)
		printf ("\nSorry, this file only has %" PRId64 " frames and we need at least %d frames.\n",
				sfinfo.frames, FFT_LEN) ;
	else
		pitch_guess (file, &sfinfo) ;

	sf_close (file) ;

	return 0 ;
} /* main */

/*==============================================================================
*/

static void
usage_exit (const char *progname)
{	const char	*cptr ;

	if ((cptr = strrchr (progname, '/')) != NULL)
		progname = cptr + 1 ;

	if ((cptr = strrchr (progname, '\\')) != NULL)
		progname = cptr + 1 ;

	printf ("\n"
		"  Guess the pitch of a single channel sound file.\n"
		"\n"
		"  Usage : \n"
		"\n"
		"       %s <input file>\n"
		, progname) ;

	puts ("") ;

	exit (1) ;
} /* usage_exit */

static double
calc_magnitude (const double * freq, int freqlen, double * magnitude)
{
	int k ;
	double max = 0.0 ;

	for (k = 1 ; k < freqlen / 2 ; k++)
	{	magnitude [k] = sqrt (freq [k] * freq [k] + freq [freqlen - k - 1] * freq [freqlen - k - 1]) ;
		max = MAX (max, magnitude [k]) ;
		} ;
	magnitude [0] = 0.0 ;

	return max ;
} /* calc_magnitude */


static void
read_with_window (SNDFILE * file, double * data, int datalen)
{
	static double window [FFT_LEN] ;
	static int window_len = 0 ;
	int k ;

	sf_read_double (file, data, datalen) ;

	if (window_len != datalen)
	{
		window_len = datalen ;
		if (datalen > ARRAY_LEN (window))
		{
			printf ("%s : datalen >  MAX_HEIGHT\n", __func__) ;
			exit (1) ;
		} ;

		calc_kaiser_window (window, datalen, 20.0) ;
	} ;

	for (k = 0 ; k < datalen ; k++)
		data [k] *= window [k] ;
} /* read_with_window */

static void
pitch_guess (SNDFILE * file, const SF_INFO * sfinfo)
{	const double log_floor = LOG_FLOOR ;
	const double noise_floor = pow (10.0, -LOG_FLOOR / 20.0) ;

	static double audio [FFT_LEN] ;
	static double freq [FFT_LEN] ;
	static double mag [FFT_LEN / 2] ;
	fftw_plan plan ;
	double max ;
	int k, analysis_length ;

	memset (audio, 0, sizeof (audio)) ;

	plan = fftw_plan_r2r_1d (FFT_LEN, audio, freq, FFTW_R2HC, FFTW_MEASURE | FFTW_PRESERVE_INPUT) ;
	if (plan == NULL)
	{	printf ("%s : line %d : create plan failed.\n", __FILE__, __LINE__) ;
		exit (1) ;
		} ;

	/* Want at least 4 cycles of 40Hz at current sample rate. */
	analysis_length = 4 * sfinfo->samplerate / 40 ;

	sf_seek (file, sfinfo->frames / 5, SEEK_CUR) ;
	read_with_window (file, audio, ARRAY_LEN (audio) / 16) ;

	fftw_execute (plan) ;
	max = calc_magnitude (freq, ARRAY_LEN (freq), mag) ;

	for (k = 0 ; k < ARRAY_LEN (mag) ; k++)
	{	mag [k] /= max ;
		mag [k] = mag [k] < noise_floor ? -log_floor : 20.0 * log10 (mag [k]) ;
		mag [k] = (log_floor + mag [k]) / log_floor ;
		printf ("% 10.8f\n", mag [k]) ;
		} ;

} /* pitch_guess */

/*==============================================================================
*/

#else /* (HAVE_FFTW3 == 0) */

/* Alternative main function when libfftw3 is not available. */

int
main (void)
{	puts (
		"\n"
		"****************************************************************\n"
		"  This program was compiled without libfftw3.\n"
		"  It is therefore completely broken and non-functional.\n"
		"****************************************************************\n"
		"\n"
		) ;

	return 0 ;
} /* main */

#endif
