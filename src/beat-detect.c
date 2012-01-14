/*
** Copyright (C) 2011 Erik de Castro Lopo <erikd@mega-nerd.com>
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
**	Detect the tempo of a given given piece of music.
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include <assert.h>

#include <sndfile.h>

#include "common.h"
#include "fir_hilbert_coeffs.h"

#define ARRAY_LEN(x)	((int) (sizeof (x) / sizeof (x [0])))


typedef struct
{	float	memory [1024] ;
	int		indx ;
	int		init ;
	double	last ;
	int		peak_count ;
} fir_hilbert_t ;


static void usage_exit (const char *progname) ;
static void beat_detect (SNDFILE * file, const SF_INFO * sfinfo) ;
static int hilbert_mag_filter (fir_hilbert_t * state, const float * input, int len, float * output, int samplerate) ;


int
main (int argc, char *argv [])
{	SNDFILE	*file = NULL ;
	SF_INFO sfinfo ;

	if (argc != 2)
		usage_exit (argv [0]) ;

	memset (&sfinfo, 0, sizeof (sfinfo)) ;

	if ((file = sf_open (argv [1], SFM_READ, &sfinfo)) == NULL)
	{	printf ("Error : Not able to open input file '%s'\n", argv [argc - 2]) ;
		exit (1) ;
		} ;

	if (sfinfo.channels != 1)
	{	puts ("\nSorry, this only works with monophonic files.\n") ;
		exit (1) ;
		} ;


	beat_detect (file, &sfinfo) ;

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
		"  Detect the temp of a single channel sound file.\n"
		"\n"
		"  Usage : \n"
		"\n"
		"       %s <input file>\n"
		, progname) ;

	puts ("") ;

	exit (1) ;
} /* usage_exit */

#define	CHANNEL_COUNT  3

static void
beat_detect (SNDFILE * file, const SF_INFO * sfinfo)
{	static float data [16 * 1024] ;
	static float env [CHANNEL_COUNT * 16 * 1024] ;
	fir_hilbert_t hilbert ;
	int read_count ;

	memset (&hilbert, 0, sizeof (hilbert)) ;

	SNDFILE * outa, *outb ;
	SF_INFO outinfo = *sfinfo ;
	outinfo.channels = CHANNEL_COUNT ;
	outinfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT ;
	if ((outa = sf_open ("/tmp/envelope.wav", SFM_WRITE, &outinfo)) == NULL)
	{	puts ("Open outa failed.") ;
		exit (1) ;
		} ;

	if ((outb = sf_open ("/tmp/peaks.wav", SFM_WRITE, &outinfo)) == NULL)
	{	puts ("Open outa failed.") ;
		exit (1) ;
		} ;

	while ((read_count = sf_read_float (file, data, ARRAY_LEN (data))) > 0)
	{	int env_count ;

		env_count = hilbert_mag_filter (&hilbert, data, ARRAY_LEN (data), env, sfinfo->samplerate) ;

		sf_writef_float (outa, env, env_count) ;

		sf_seek (file, env_count - read_count, SEEK_CUR) ;
		} ;

	sf_close (outa) ;
	sf_close (outb) ;
} /* beat_detect */

/*
 * http://www.clear.rice.edu/elec301/Projects01/beat_sync/beatalgo.html
 *
 */


static int
hilbert_mag_filter (fir_hilbert_t * state, const float * input, int len, float * output, int samplerate)
{	double last ;
	int k, end_count, peak_count ;

	end_count = len - ARRAY_LEN (half_hilbert_coeffs) ;

	last = state->last ;
	peak_count = state->peak_count ;

	for (k = 0 ; k < end_count ; k++)
	{	double real = 0.0, imag = 0.0, mag, peak ;
		int j ;

		for (j = ARRAY_LEN (half_hilbert_coeffs) - 1 ; j >= 0  ; j --)
		{	int indx = (state->indx + ARRAY_LEN (state->memory) - 1 - j) & (ARRAY_LEN (state->memory) - 1) ;
			double mem = state->memory [indx] ;

			real += half_hilbert_coeffs [j].r * (input [k + j] + mem) ;
			imag += half_hilbert_coeffs [j].i * (input [k + j] - mem) ;
			} ;

		state->memory [state->indx] = input [k] ;
		state->indx = (state->indx + 1) & (ARRAY_LEN (state->memory) - 1) ;

		mag = sqrt (real * real + imag * imag) ;
		peak = 0.0 ;

		if (mag > last)
		{	peak_count = samplerate / 10 ;
			peak = mag ;
			}
		else if (peak_count > 0)
		{	peak_count -- ;
			mag = last ;
			}
		else
			mag = last > 0.0 ? last - 0.00001 : mag ;

		output [CHANNEL_COUNT * k] = input [k] ;
		output [CHANNEL_COUNT * k + 1] = mag ;
		output [CHANNEL_COUNT * k + 2] = peak ;

		last = mag ;
		} ;

	state->last = last ;
	state->peak_count = peak_count ;

	return k ;
} /* hilbert_mag_filter */

