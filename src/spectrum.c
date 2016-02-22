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

	spec = calloc (1, sizeof (spectrum) + ((2 + 2 + 2 + 1) * speclen + 1) * sizeof (double)) ;

	spec->wfunc = window_function ;
	spec->speclen = speclen ;

	spec->time_domain = spec->data ;
	spec->window = spec->time_domain + 2 * speclen ;
	spec->freq_domain = spec->window + 2 * speclen ;
	spec->mag_spec = spec->freq_domain + 2 * speclen ;	/* mag_spec has values from [0..speclen] inclusive */

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
	spec->mag_spec [spec->speclen] = fabs(spec->freq_domain [spec->speclen]) ;

	return max ;
} /* calc_magnitude_spectrum */
