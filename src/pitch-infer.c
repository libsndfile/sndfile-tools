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
#include <assert.h>

#if (HAVE_FFTW3)

#include <fftw3.h>

#include "common.h"
#include "window.h"

#define	FFT_LEN			(1 << 15)

#define ARRAY_LEN(x)	((int) (sizeof (x) / sizeof (x [0])))

#define LOG_FLOOR		15.0	/* decibels */

static void usage_exit (const char *progname) ;
static void pitch_guess (SNDFILE * file, const SF_INFO * sfinfo, int analysis_length) ;

typedef struct
{	const char* note ;
	char octave ;
	double freq ;
} pitch_t ;

static const pitch_t pitch_table [] =
{	{ "C",	4, 261.6255653006 },
	{ "C#", 4, 277.1826309769 },
	{ "D",	4, 293.6647679174 },
	{ "D#", 4, 311.1269837221 },
	{ "E",	4, 329.6275569129 },
	{ "F",	4, 349.2282314330 },
	{ "F#", 4, 369.9944227116 },
	{ "G",	4, 391.9954359817 },
	{ "G#", 4, 415.3046975799 },
	{ "A",	4, 440.0000000000 },
	{ "A#", 4, 466.1637615181 },
	{ "B",	4, 493.8833012561 },
	{ "C",	5, 523.2511306012 },
} ;

int
main (int argc, char *argv [])
{	SNDFILE	*file = NULL ;
	SF_INFO sfinfo ;
	int analysis_length ;

	if (argc != 2)
		usage_exit (argv [0]) ;

	if ((file = sf_open (argv [1], SFM_READ, &sfinfo)) == NULL)
	{	printf ("Error : Not able to open input file '%s'\n", argv [argc - 2]) ;
		exit (1) ;
		} ;

	/* Want at least 6 cycles of 40Hz at current sample rate. */
	analysis_length = 6 * sfinfo.samplerate / 40 ;

	if (sfinfo.channels != 1)
		puts ("\nSorry, this only works with monophonic files.\n") ;
	else if (sfinfo.frames < analysis_length)
		printf ("\nSorry, this file only has %" PRId64 " frames and we need at least %d frames.\n",
				sfinfo.frames, analysis_length) ;
	else
		pitch_guess (file, &sfinfo, analysis_length) ;

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


static void
read_dc_block_and_window (SNDFILE * file, double * data, int datalen)
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
			printf ("%s : datalen > ARRAY_LEN (window)\n", __func__) ;
			exit (1) ;
		} ;

		calc_kaiser_window (window, datalen, 20.0) ;
	} ;

	for (k = 0 ; k < datalen ; k++)
		data [k] *= window [k] ;

	return ;
} /* read_dc_block_and_window */

static inline int
is_power2 (int x)
{	return (x > 0 && (x & (x - 1)) == 0) ;
} /* is_power2 */

static inline double
freq_of_fft_bin (int bin, int fftlen, int samplerate)
{	return (bin * samplerate) / (1.0 * fftlen) ;
} /* freq_of_fft_bin */

static inline int
fft_bin_of_freq (double freq, int fftlen, int samplerate)
{	return lrint ((freq * fftlen) / (1.0 * samplerate)) ;
} /* fft_bin_of_freq */


static void
check_peaks (const double * mag, int mlen)
{	int k ;
	int greater = 0, less = 0 ;

	for (k = 1 ; k < mlen ; k++)
	{	if (mag [k] >= 0.5)
			greater ++ ;
		else
			less ++ ;

		if (k >= 128 && is_power2 (k) && 3 * greater > less)
			fprintf (stderr, "is_power2 %d  (%d, %d)\n", k, greater, less) ;
		} ;

} /* check_peaks */

typedef struct
{	int start, end, width ;
	double bin_mean ;
	double freq ;
	double mag_max ;
	double freq_mult ;
} peak_t ;

typedef struct
{	peak_t	peaks [12] ;

	int		samplerate ;
	int		fft_len ;
	double	fundamental ;
	double	std_dev ;
} peak_data_t ;

enum
{	STATE_TROUGH = 1,
	STATE_PEAK
} ;

enum
{	MAG_ZERO,
	MAG_BELOW,
	MAG_ABOVE
} ;


static const char *
str_of_state (int state)
{
#define	CASE_NAME(x)	case x : return #x ; break ;
	switch (state)
	{	CASE_NAME (STATE_TROUGH) ;
		CASE_NAME (STATE_PEAK) ;
		default : break ;
		} ;
	return "BAD_STATE" ;
} /* str_of_state */

#define	MAG_THRESHOLD 0.5

static inline int
mag_func (double mag)
{	if (mag < 0.01)
		return MAG_ZERO ;
	if (mag >= MAG_THRESHOLD)
		return MAG_ABOVE ;
	return MAG_BELOW ;
} /* mag_func */


static void
calc_freq (const double * mag, int mlen, peak_t * peak)
{	double sum = 0.0, wsum = 0.0, mean ;
	int k, length ;

	assert (peak->end < mlen) ;
	assert (peak->start < peak->end) ;

	length = peak->end - peak->start + 1 ;
	for (k = peak->start ; k <= peak->end ; k++)
	{	sum += mag [k] ;
		wsum += k * mag [k] ;
		} ;

	mean = sum / (1.0 * length) ;

	peak->bin_mean = wsum / mean / length ;
} /* calc_freq */


#define	PEAK_FUNC(state,mag)	((state) + ((mag) << 8))


static int
find_peaks (const double * mag, int mlen, peak_t * peaks, int plen)
{	int k, pcount = 0, state = STATE_TROUGH ;

	peaks [0].start = 0 ;

	for (k = 0 ; k < mlen && pcount < plen ; k++)
	{	switch (PEAK_FUNC (state, mag_func (mag [k])))
		{
			case PEAK_FUNC (STATE_TROUGH, MAG_BELOW) :
			case PEAK_FUNC (STATE_TROUGH, MAG_ZERO) :
				break ;

			case PEAK_FUNC (STATE_TROUGH, MAG_ABOVE) :
				peaks [pcount].start = k ;
				state = STATE_PEAK ;
				break ;

			case PEAK_FUNC (STATE_PEAK, MAG_ABOVE) :
			case PEAK_FUNC (STATE_PEAK, MAG_BELOW) :
				break ;

			case PEAK_FUNC (STATE_PEAK, MAG_ZERO) :
				peaks [pcount].end = k ;
				while (peaks [pcount].end > peaks [pcount].start && mag [peaks [pcount].end] < MAG_THRESHOLD)
					peaks [pcount].end -- ;

				peaks [pcount].width = peaks [pcount].end - peaks [pcount].start + 1 ;

				if (peaks [pcount].end - peaks [pcount].start > 1)
				{	int m ;

					calc_freq (mag, mlen, peaks + pcount) ;

					for (m = peaks [pcount].start ; m <= peaks [pcount].end ; m++)
						peaks [pcount].mag_max = MAX (peaks [pcount].mag_max, mag [m]) ;

					/* Throw away peaks that are too wide */
					if (pcount >= 1 && peaks [pcount].width < 2 * peaks [0].width)
						pcount ++ ;

					/* Always accept the first peak. */
					if (pcount < 1)
						pcount ++ ;
					} ;

				state = STATE_TROUGH ;
				break ;

			default :
				printf ("%s : bad state (%s, %f)\n", __func__, str_of_state (state), mag [k]) ;
				exit (1) ;
			} ;
		} ;

	return pcount ;
} /* find_peaks */


static void
find_fundamental (peak_data_t * pdata, int plen)
{	double min, max, divisor ;
	int k, multiplier = 1 ;

	pdata->peaks [0].freq = pdata->peaks [0].bin_mean * pdata->samplerate / (1.0 * pdata->fft_len) ;
	pdata->peaks [0].freq_mult = 1.0 ;

	if (plen <= 1)
	{	pdata->fundamental = pdata->peaks [0].freq ;
		return ;
		} ;

	for (k = 1 ; k < plen ; k++)
	{	pdata->peaks [k].freq = pdata->peaks [k].bin_mean * pdata->samplerate / (1.0 * pdata->fft_len) ;
		pdata->peaks [k].freq_mult = pdata->peaks [k].freq / pdata->peaks [0].freq ;
		} ;

	for (k = 1 ; multiplier == 1 && k < plen ; k++)
	{	double fa = pdata->peaks [k].freq_mult ;
		double fb = round (fa) ;
		int m ;

		if (fabs (fa - fb) < 0.01)
			continue ;

		for (m = 2 ; multiplier == 1 && m <= 3 ; m++)
		{	fa = pdata->peaks [k].freq_mult * m ;
			fb = round (fa) ;

			if (fabs (fa - fb) < 0.01)
				multiplier = m ;
			} ;
		} ;

	if (multiplier > 1)
		for (k = 0 ; k < plen ; k++)
			pdata->peaks [k].freq_mult *= multiplier ;

	pdata->fundamental = 0.0 ;
	min = max = pdata->peaks [0].freq / pdata->peaks [0].freq_mult ;
	divisor = 0 ;
	for (k = 0 ; k < plen ; k++)
	{	double freq = pdata->peaks [k].freq / round (pdata->peaks [k].freq_mult) ;
		min = MIN (min, freq) ;
		max = MIN (max, freq) ;
		pdata->fundamental += freq * pdata->peaks [k].mag_max ;
		divisor += pdata->peaks [k].mag_max ;
		} ;

	pdata->fundamental /= divisor ;

	for (k = 0 ; k < plen ; k++)
	{	double diff = pdata->fundamental - pdata->peaks [k].freq / round (pdata->peaks [k].freq_mult) ;
		pdata->std_dev += diff * diff ;
		} ;

	pdata->std_dev = sqrt (pdata->std_dev) ;

	return ;
} /* find_fundamental */

static void
pitch_guess (SNDFILE * file, const SF_INFO * sfinfo, int analysis_length)
{	const double log_floor = LOG_FLOOR ;
	const double noise_floor = pow (10.0, -LOG_FLOOR / 20.0) ;

	static double audio [FFT_LEN] ;
	static double freq [FFT_LEN] ;
	static double mag [FFT_LEN / 2] ;
	fftw_plan plan ;
	peak_data_t peak_data ;
	double max = 0.0, mult = 1.0 ;
	int k, zero_bins, pcount ;

	memset (audio, 0, sizeof (audio)) ;
	memset (&peak_data, 0, sizeof (peak_data)) ;
	peak_data.samplerate = sfinfo->samplerate ;
	peak_data.fft_len = FFT_LEN ;

	plan = fftw_plan_r2r_1d (FFT_LEN, audio, freq, FFTW_R2HC, FFTW_MEASURE | FFTW_PRESERVE_INPUT) ;
	if (plan == NULL)
	{	printf ("%s : line %d : create plan failed.\n", __FILE__, __LINE__) ;
		exit (1) ;
		} ;

	sf_seek (file, sfinfo->frames / 8, SEEK_CUR) ;
	read_dc_block_and_window (file, audio, analysis_length) ;

	fftw_execute (plan) ;
	calc_magnitude (freq, ARRAY_LEN (freq), mag) ;

	zero_bins = fft_bin_of_freq (20.0, FFT_LEN, sfinfo->samplerate) ;
	for (k = 0 ; k < ARRAY_LEN (mag) ; k++)
	{	if (k < zero_bins)
			mag [k] = 0.0 ;
		else
		{	mag [k] *= mult ;
			mult *= 0.9998 ;
			} ;
		max = MAX (max, mag [k]) ;
		} ;

	for (k = 0 ; k < ARRAY_LEN (mag) ; k++)
	{	mag [k] /= max ;
		mag [k] = mag [k] < noise_floor ? -log_floor : 20.0 * log10 (mag [k]) ;
		mag [k] = (log_floor + mag [k]) / log_floor ;
		printf ("% 10.8f\n", mag [k]) ;
		} ;

	check_peaks (mag, ARRAY_LEN (mag)) ;
	pcount = find_peaks (mag, ARRAY_LEN (mag), peak_data.peaks, ARRAY_LEN (peak_data.peaks)) ;

	fprintf (stderr, "\npeaks : %d\n", pcount) ;

	find_fundamental (&peak_data, pcount) ;

	for (k = 0 ; k < pcount ; k++)
	{	fprintf (stderr, "%2d : %4d - %4d       %12.6f  ->  %12.6f       %f    %15.12f  ->  %12.8f\n", k,
				peak_data.peaks [k].start, peak_data.peaks [k].end, peak_data.peaks [k].bin_mean,
				peak_data.peaks [k].freq, peak_data.peaks [k].mag_max, peak_data.peaks [k].freq_mult,
				peak_data.peaks [k].freq / round (peak_data.peaks [k].freq_mult)) ;
		} ;

	fprintf (stderr, "fundamental : %f  (std. dev. = %f)\n\n", peak_data.fundamental, peak_data.std_dev) ;

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
