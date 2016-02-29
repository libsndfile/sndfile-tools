#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>

#include <fftw3.h>

#include <sndfile.h>

#include "common.h"
#include "window.h"
#include "spectrum.h"



spectrum *
create_spectrum (int speclen, enum WINDOW_FUNCTION window_function)
{	spectrum *spec ;

	spec = calloc (1, sizeof (spectrum)) ;
	if (spec == NULL)
	{	printf ("%s : Not enough memory.\n", __func__) ;
		exit (1) ;
		} ;

	spec->wfunc = window_function ;
	spec->speclen = speclen ;

	/* mag_spec has values from [0..speclen] inclusive for 0Hz to Nyquist.
	** time_domain has an extra element to be able to interpolate between
	** samples for better time precision, hoping to eliminate artifacts.
	*/
	spec->time_domain = calloc (2 * speclen + 1, sizeof (double)) ;
	spec->window = calloc (2 * speclen, sizeof (double)) ;
	spec->freq_domain = calloc (2 * speclen, sizeof (double)) ;
	spec->mag_spec = calloc (speclen + 1, sizeof (double)) ;
	if (spec->time_domain == NULL
		|| spec->window == NULL
		|| spec->freq_domain == NULL
		|| spec->mag_spec == NULL)
	{	printf ("%s : Not enough memory.\n", __func__) ;
		exit (1) ;
		} ;

	spec->plan = fftw_plan_r2r_1d (2 * speclen, spec->time_domain, spec->freq_domain, FFTW_R2HC, FFTW_MEASURE | FFTW_PRESERVE_INPUT) ;
	if (spec->plan == NULL)
	{	printf ("%s:%d : fftw create plan failed.\n", __func__, __LINE__) ;
		free (spec) ;
		exit (1) ;
		} ;

	switch (spec->wfunc)
	{	case RECTANGULAR :
			break ;
		case KAISER :
			calc_kaiser_window (spec->window, 2 * speclen, 20.0) ;
			break ;
		case NUTTALL:
			calc_nuttall_window (spec->window, 2 * speclen) ;
			break ;
		case HANN :
			calc_hann_window (spec->window, 2 * speclen) ;
			break ;
		default :
			printf ("Internal error: Unknown window_function.\n") ;
			free (spec) ;
			exit (1) ;
		} ;

	return spec ;
} /* create_spectrum */


void
destroy_spectrum (spectrum * spec)
{
	fftw_destroy_plan (spec->plan) ;
	free (spec->time_domain) ;
	free (spec->window) ;
	free (spec->freq_domain) ;
	free (spec->mag_spec) ;
	free (spec) ;
} /* destroy_spectrum */

double
calc_magnitude_spectrum (spectrum * spec)
{
	double max ;
	int k, freqlen ;

	freqlen = 2 * spec->speclen ;

	if (spec->wfunc != RECTANGULAR)
		for (k = 0 ; k < 2 * spec->speclen ; k++)
			spec->time_domain [k] *= spec->window [k] ;


	fftw_execute (spec->plan) ;

	/* Convert from FFTW's "half complex" format to an array of magnitudes.
	** In HC format, the values are stored:
	** r0, r1, r2 ... r(n/2), i(n+1)/2-1 .. i2, i1
	**/
	max = spec->mag_spec [0] = fabs (spec->freq_domain [0]) ;

	for (k = 1 ; k < spec->speclen ; k++)
	{	double re = spec->freq_domain [k] ;
		double im = spec->freq_domain [freqlen - k] ;
		spec->mag_spec [k] = sqrt (re * re + im * im) ;
		max = MAX (max, spec->mag_spec [k]) ;
		} ;
	/* Lastly add the point for the Nyquist frequency */
	spec->mag_spec [spec->speclen] = fabs (spec->freq_domain [spec->speclen]) ;

	return max ;
} /* calc_magnitude_spectrum */
