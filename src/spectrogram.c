/*
** Copyright (C) 2007-2016 Erik de Castro Lopo <erikd@mega-nerd.com>
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
**	Generate a spectrogram as a PNG file from a given sound file.
*/

/*
**	Todo:
**      - Make magnitude to colour mapper allow abitrary scaling (ie cmdline
**        arg).
**      - Better cmdline arg parsing and flexibility.
*/

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>
#include <assert.h>

#include <cairo.h>
#include <fftw3.h>

#include <sndfile.h>

#include "window.h"
#include "common.h"
#include "spectrum.h"

#define TICK_LEN			6
#define	BORDER_LINE_WIDTH	1.8

#define	TITLE_FONT_SIZE		20.0
#define	NORMAL_FONT_SIZE	12.0

#define	LEFT_BORDER			70.0
#define	TOP_BORDER			30.0
#define	RIGHT_BORDER		75.0
#define	BOTTOM_BORDER		40.0

#define	SPEC_FLOOR_DB		-180.0


typedef struct
{	const char *sndfilepath, *pngfilepath, *filename ;
	int width, height ;
	bool border, log_freq, gray_scale ;
	double min_freq, max_freq, fft_freq ;
	enum WINDOW_FUNCTION window_function ;
	double spec_floor_db ;
} RENDER ;

typedef struct
{	int left, top, width, height ;
} RECT ;

static void
get_colour_map_value (float value, double spec_floor_db, unsigned char colour [3], bool gray_scale)
{	static unsigned char map [][3] =
	{	/* These values were originally calculated for a dynamic range of 180dB. */
		{	255,	255,	255	},	/* -0dB */
		{	240,	254,	216	},	/* -10dB */
		{	242,	251,	185	},	/* -20dB */
		{	253,	245,	143	},	/* -30dB */
		{	253,	200,	102	},	/* -40dB */
		{	252,	144,	66	},	/* -50dB */
		{	252,	75,		32	},	/* -60dB */
		{	237,	28,		41	},	/* -70dB */
		{	214,	3,		64	},	/* -80dB */
		{	183,	3,		101	},	/* -90dB */
		{	157,	3,		122	},	/* -100dB */
		{	122,	3,		126	},	/* -110dB */
		{	80,		2,		110	},	/* -120dB */
		{	45,		2,		89	},	/* -130dB */
		{	19,		2,		70	},	/* -140dB */
		{	1,		3,		53	},	/* -150dB */
		{	1,		3,		37	},	/* -160dB */
		{	1,		2,		19	},	/* -170dB */
		{	0,		0,		0	},	/* -180dB */
	} ;

	float rem ;
	int indx ;

	if (gray_scale)
	{	/* "value" is a negative value in decibels.
		 * black (0,0,0) is for <= -180.0, and the other 255 values
		 * should cover the range from -180 to 0 evenly.
		 * (value/spec_floor_db) is >=0.0  and <1.0
		 * because both value and spec_floor_db are negative.
		 * (v/s) * 255.0 goes from 0.0 to 254.9999999 and
		 * floor((v/s) * 255) gives us 0 to 254
		 * converted to 255 to 1 by subtracting it from 255. */
		int gray ; /* The pixel value */

		if (value <= spec_floor_db)
			gray = 0 ;
		else
		{	gray = 255 - lrint (floor ((value / spec_floor_db) * 255.0)) ;
			assert (gray >= 1 && gray <= 255) ;
			} ;
		colour [0] = colour [1] = colour [2] = gray ;
		return ;
		} ;

	if (value >= 0.0)
	{	colour [0] = colour [1] = colour [2] = 255 ;
		return ;
		} ;

	value = fabs (value * (-180.0 / spec_floor_db) * 0.1) ;

	indx = lrintf (floor (value)) ;

	if (indx < 0)
	{	printf ("\nError : colour map array index is %d\n\n", indx) ;
		exit (1) ;
		} ;

	if (indx >= ARRAY_LEN (map) - 1)
	{	colour [0] = colour [1] = colour [2] = 0 ;
		return ;
		} ;

	rem = fmod (value, 1.0) ;

	colour [0] = lrintf ((1.0 - rem) * map [indx][0] + rem * map [indx + 1][0]) ;
	colour [1] = lrintf ((1.0 - rem) * map [indx][1] + rem * map [indx + 1][1]) ;
	colour [2] = lrintf ((1.0 - rem) * map [indx][2] + rem * map [indx + 1][2]) ;

	return ;
} /* get_colour_map_value */


static void
read_mono_audio (SNDFILE * file, sf_count_t filelen, double * data, int datalen, int indx, int total)
{
	sf_count_t start ;

	memset (data, 0, datalen * sizeof (data [0])) ;

	start = (indx * filelen) / total - datalen / 2 ;

	if (start >= 0)
		sf_seek (file, start, SEEK_SET) ;
	else
	{	start = -start ;
		sf_seek (file, 0, SEEK_SET) ;
		data += start ;
		datalen -= start ;
		} ;

	sfx_mix_mono_read_double (file, data, datalen) ;

	return ;
} /* read_mono_audio */

static void
render_spectrogram (cairo_surface_t * surface, double spec_floor_db, float **mag2d, double maxval, double left, double top, double width, double height, bool gray_scale)
{
	unsigned char colour [3] = { 0, 0, 0 } ;
	unsigned char *data ;
	double linear_spec_floor ;
	int w, h, stride ;

	stride = cairo_image_surface_get_stride (surface) ;

	data = cairo_image_surface_get_data (surface) ;
	memset (data, 0, stride * cairo_image_surface_get_height (surface)) ;

	linear_spec_floor = pow (10.0, spec_floor_db / 20.0) ;

	for (w = 0 ; w < width ; w ++)
		for (h = 0 ; h < height ; h++)
		{	int x, y ;

			mag2d [w][h] = mag2d [w][h] / maxval ;
			mag2d [w][h] = (mag2d [w][h] < linear_spec_floor) ? spec_floor_db : 20.0 * log10 (mag2d [w][h]) ;

			get_colour_map_value (mag2d [w][h], spec_floor_db, colour, gray_scale) ;

			y = height + top - 1 - h ;
			x = (w + left) * 4 ;
			data [y * stride + x + 0] = colour [2] ;
			data [y * stride + x + 1] = colour [1] ;
			data [y * stride + x + 2] = colour [0] ;
			data [y * stride + x + 3] = 0 ;
			} ;

	cairo_surface_mark_dirty (surface) ;
} /* render_spectrogram */

static void
render_heat_map (cairo_surface_t * surface, double magfloor, const RECT *r, bool gray_scale)
{
	unsigned char colour [3], *data ;
	int w, h, stride ;

	stride = cairo_image_surface_get_stride (surface) ;
	data = cairo_image_surface_get_data (surface) ;

	for (h = 0 ; h < r->height ; h++)
	{	get_colour_map_value (magfloor * (r->height - h) / (r->height + 1), magfloor, colour, gray_scale) ;

		for (w = 0 ; w < r->width ; w ++)
		{	int x, y ;

			x = (w + r->left) * 4 ;
			y = r->height + r->top - 1 - h ;

			data [y * stride + x + 0] = colour [2] ;
			data [y * stride + x + 1] = colour [1] ;
			data [y * stride + x + 2] = colour [0] ;
			data [y * stride + x + 3] = 0 ;
			} ;
		} ;

	cairo_surface_mark_dirty (surface) ;
} /* render_heat_map */

static inline void
x_line (cairo_t * cr, double x, double y, double len)
{	cairo_move_to (cr, x, y) ;
	cairo_rel_line_to (cr, len, 0.0) ;
	cairo_stroke (cr) ;
} /* x_line */

static inline void
y_line (cairo_t * cr, double x, double y, double len)
{	cairo_move_to (cr, x, y) ;
	cairo_rel_line_to (cr, 0.0, len) ;
	cairo_stroke (cr) ;
} /* y_line */

/* The greatest number of linear ticks seems to occurs from 0-14000 (15 ticks).
** The greatest number of log ticks occurs 10-99999 or 11-100000 (35 ticks).
** Search for "worst case" for the commentary below that says why it is 35.
*/
typedef struct
{	double value [40] ;	/* 35 or more */
	double distance [40] ;
	/* The digit that changes from label to label.
	** This ensures that a range from 999 to 1001 prints 999.5 and 1000.5
	** instead of 999 1000 1000 1000 1001.
	*/
	int decimal_places_to_print ;
} TICKS ;

/* Decide where to put ticks and numbers on an axis.
**
** Graph-labelling convention is that the least significant digit that changes
** from one label to the next should change by 1, 2 or 5, so we step by the
** largest suitable value of 10^n * {1, 2 or 5} that gives us the required
** number of divisions / numeric labels.
*/

/* The old code used to make 6 to 14 divisions and number every other tick.
** What we now mean by "division" is one of teh gaps between numbered segments
** so we ask for a minimum of 3 to give the same effect as the old minimum of
** 6 half-divisions.
** This results in the same axis labelling for all maximum values
** from 0 to 12000 in steps of 1000 and gives sensible results from 13000 on,
** to a maximum of 7 divisions and 8 labels from 0 to 14000.
**/
#define TARGET_DIVISIONS 3

/* Value to store in the ticks.value[k] field to mean
** "Put a tick here, but don't print a number."
** NaN (0.0/0.0) is untestable without isnan() so use a random value.
*/
#define NO_NUMBER (M_PI)		/* They're unlikely to hit that! */

/* Is this entry in "ticks" one of the numberless ticks? */
#define JUST_A_TICK(ticks, k)	(ticks.value [k] == NO_NUMBER)

/* A tolerance to use in floating point < > <= >= comparisons so that
** imprecision doesn't prevent us from printing an initial or final label
** if it should fall exactly on min or max but doesn't due to FP problems.
** For example, for 0-24000, the calculations might give 23999.9999999999.
*/
#define DELTA (1e-10)

static int	/* Forward declaration */
calculate_log_ticks (double min, double max, double distance, TICKS * ticks) ;

/* log_scale is pseudo-boolean:
** 0 means use a linear scale,
** 1 means use a log scale and
** 2 is an internal value used when calling back from calculate_log_ticks() to
**   label the range with linear numbering but logarithmic spacing.
*/

static int
calculate_ticks (double min, double max, double distance, int log_scale, TICKS * ticks)
{
	double step ;	/* Put numbered ticks at multiples of this */
	double range = max - min ;
	int k ;
	double value ;	/* Temporary */

	if (log_scale == 1)
		return calculate_log_ticks (min, max, distance, ticks) ;

	/* Linear version */

	/* Choose a step between successive axis labels so that one digit
	** changes by 1, 2 or 5 amd that gives us at least the number of
	** divisions (and numberic labels) that we would like to have.
	**
	** We do this by starting "step" at the lowest power of ten <= max,
	** which can give us at most 9 divisions (e.g. from 0 to 9999, step 1000)
	** Then try 5*this, 2*this and 1*this.
	*/
	step = pow (10.0, floor (log10 (max))) ;
	do
	{	if (range / (step * 5) >= TARGET_DIVISIONS)
		{	step *= 5 ;
			break ;
			} ;
		if (range / (step * 2) >= TARGET_DIVISIONS)
		{	step *= 2 ;
			break ;
			} ;
		if (range / step >= TARGET_DIVISIONS)
			break ;
		step /= 10 ;
	} while (1) ;	/* This is an odd loop! */

	/* Ensure that the least significant digit that changes gets printed, */
	ticks->decimal_places_to_print = lrint (-floor (log10 (step))) ;
	if (ticks->decimal_places_to_print < 0)
		ticks->decimal_places_to_print = 0 ;

	/* Now go from the first multiple of step that's >= min to
	 * the last one that's <= max. */
	k = 0 ;
	value = ceil (min / step) * step ;

#define add_tick(val, just_a_tick) do \
	{	if (val >= min - DELTA && val < max + DELTA) \
		{	ticks->value [k] = just_a_tick ? NO_NUMBER : val ; \
			ticks->distance [k] = distance * \
				(log_scale == 2 \
					? /*log*/ (log (val) - log (min)) / (log (max) - log (min)) \
					: /*lin*/ (val - min) / range) ; \
			k++ ; \
			} ; \
		} while (0)

	/* Add the half-way tick before the first number if it's in range */
	add_tick (value - step / 2, true) ;

	while (value <= max + DELTA)
	{ 	/* Add a tick next to each printed number */
		add_tick (value, false) ;

		/* and at the half-way tick after the number if it's in range */
		add_tick (value + step / 2, true) ;

		value += step ;
		} ;

	return k ;
} /* calculate_ticks */

/* Number/tick placer for logarithmic scales.
**
** Some say we should number 1, 10, 100, 1000, 1000 and place ticks at
** 2,3,4,5,6,7,8,9, 20,30,40,50,60,70,80,90, 200,300,400,500,600,700,800,900
** Others suggest numbering 1,2,5, 10,20,50, 100,200,500.
**
** Ticking 1-9 is visually distinctive and emphasizes that we are using
** a log scale, as well as mimicking log graph paper.
** Numbering the powers of ten and, if that doesn't give enough labels,
** numbering also the 2 and 5 multiples might work.
**
** Apart from our [number] and tick styles:
** [1] 2 5 [10] 20 50 [100]  and
** [1] [2] 3 4 [5] 6 7 8 9 [10]
** the following are also seen in use:
** [1] [2] 3 4 [5] 6 7 [8] 9 [10]  and
** [1] [2] [3] [4] [5] [6] 7 [8] 9 [10]
** in https://www.lhup.edu/~dsimanek/scenario/errorman/graphs2.htm
**
** This works fine for wide ranges, not so well for narrow ranges like
** 5000-6000, so for ranges less than a decade we apply the above
** linear numbering style 0.2 0.4 0.6 0.8 or whatever, but calulating
** the positions of the legends logarithmically.
**
** Alternatives could be:
** - by powers or two from some starting frequency
**   defaulting to the Nyquist frequency (22050, 11025, 5512.5 ...) or from some
**   musical pitch (220, 440, 880, 1760)
** - with a musical note scale  C0 ' D0 ' E0 F0 ' G0 ' A0 ' B0 C1
** - with manuscript staff lines, piano note or guitar string overlay.
*/

/* Helper functions: add ticks and labels at start_value and all powers of ten
** times it that are in the min-max range.
** This is used to plonk ticks at 1, 10, 100, 1000 then at 2, 20, 200, 2000
** then at 5, 50, 500, 5000 and so on.
*/
static int
add_log_ticks (double min, double max, double distance, TICKS * ticks,
				int k, double start_value, bool include_number)
{	double value ;

	for (value = start_value ; value <= max + DELTA ; value *= 10.0)
	{	if (value < min - DELTA) continue ;
		ticks->value [k] = include_number ? value : NO_NUMBER ;
		ticks->distance [k] = distance * (log (value) - log (min)) / (log (max) - log (min)) ;
		k++ ;
		} ;
	return k ;
} /* add_log_ticks */

static int
calculate_log_ticks (double min, double max, double distance, TICKS * ticks)
{	int k = 0 ;	/* Number of ticks we have placed in "ticks" array */
	double underpinning ; 	/* Largest power of ten that is <= min */

	/* If the interval is less than a decade, just apply the same
	** numbering-choosing scheme as used with linear axis, with the
	** ticks positioned logarithmically.
	*/
	if (max / min < 10.0)
		return calculate_ticks (min, max, distance, 2, ticks) ;

	/* If the range is greater than 1 to 1000000, it will generate more than
	** 19 ticks.  Better to fail explicitly than to overflow.
	*/
	if (max / min > 1000000)
	{	printf ("Error: Frequency range is too great for logarithmic scale.\n") ;
		exit (1) ;
		} ;

	/* First hack: label the powers of ten. */

 	/* Find largest power of ten that is <= minimum value */
	underpinning = pow (10.0, floor (log10 (min))) ;

	/* Go powering up by 10 from there, numbering as we go. */
	k = add_log_ticks (min, max, distance, ticks, k, underpinning, true) ;

	/* Do we have enough numbers? If so, add numberless ticks at 2 and 5 */
	if (k >= TARGET_DIVISIONS + 1) /* Number of labels is n.of divisions + 1 */
	{
		k = add_log_ticks (min, max, distance, ticks, k, underpinning * 2.0, false) ;
		k = add_log_ticks (min, max, distance, ticks, k, underpinning * 5.0, false) ;
		}
	else
	{	int i ;
		/* Not enough numbers: add numbered ticks at 2 and 5 and
		 * unnumbered ticks at all the rest */
		for (i = 2 ; i <= 9 ; i++)
			k = add_log_ticks (min, max, distance, ticks, k,
								underpinning * (1.0 * i), i == 2 || i == 5) ;
		} ;

	/* Greatest possible number of ticks calculation:
	** The worst case is when the else clause adds 8 ticks with the maximal
	** number of divisions, which is when k == TARGET_DIVISIONS, 3,
	** for example 100, 1000, 10000.
	** The else clause adds another 8 ticks inside each division as well as
	** up to 8 ticks after the last number (from 20000 to 90000)
	** and 8 before to the first (from 20 to 90 in the example).
	** Maximum possible ticks is 3+8+8+8+8=35
	*/

	return k ;
} /* calculate_log_ticks */

static void
str_print_value (char * text, int text_len, double value, int decimal_places_to_print)
{
	if (fabs (value) < 1e-10)
		snprintf (text, text_len, "0") ;
	else	snprintf (text, text_len, "%.*f", decimal_places_to_print, value) ;
} /* str_print_value */

static void
render_spect_border (cairo_surface_t * surface, const char * filename, double left, double width, double seconds, double top, double height, double min_freq, double max_freq, bool log_freq)
{
	char text [512] ;
	cairo_t * cr ;
	cairo_text_extents_t extents ;
	cairo_matrix_t matrix ;

	TICKS ticks ;
	int k, tick_count ;

	cr = cairo_create (surface) ;

	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0) ;
	cairo_set_line_width (cr, BORDER_LINE_WIDTH) ;

	/* Print title. */
	cairo_select_font_face (cr, font_family, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL) ;
	cairo_set_font_size (cr, 1.0 * TITLE_FONT_SIZE) ;

	snprintf (text, sizeof (text), "Spectrogram: %s", filename) ;
	cairo_text_extents (cr, text, &extents) ;
	cairo_move_to (cr, left + 2, top - extents.height / 2) ;
	cairo_show_text (cr, text) ;

	/* Print labels. */
	cairo_select_font_face (cr, font_family, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL) ;
	cairo_set_font_size (cr, 1.0 * NORMAL_FONT_SIZE) ;

	/* Border around actual spectrogram. */
	cairo_rectangle (cr, left, top, width, height) ;

	/* Put ticks on Time axis */
	tick_count = calculate_ticks (0.0, seconds, width, false, &ticks) ;
	for (k = 0 ; k < tick_count ; k++)
	{	y_line (cr, left + ticks.distance [k], top + height, TICK_LEN) ;
		if (JUST_A_TICK (ticks, k))
			continue ;
		str_print_value (text, sizeof (text), ticks.value [k],
			ticks.decimal_places_to_print) ;
		cairo_text_extents (cr, text, &extents) ;
		cairo_move_to (cr, left + ticks.distance [k] - extents.width / 2, top + height + 8 + extents.height) ;
		cairo_show_text (cr, text) ;
		} ;

	/* Put ticks on Frequency axis */
	tick_count = calculate_ticks (min_freq, max_freq, height, log_freq, &ticks) ;
	for (k = 0 ; k < tick_count ; k++)
	{	x_line (cr, left + width, top + height - ticks.distance [k], TICK_LEN) ;
		if (JUST_A_TICK (ticks, k))
			continue ;
		str_print_value (text, sizeof (text), ticks.value [k],
			ticks.decimal_places_to_print) ;
		cairo_text_extents (cr, text, &extents) ;
		cairo_move_to (cr, left + width + 12, top + height - ticks.distance [k] + extents.height / 4.5) ;
		cairo_show_text (cr, text) ;
		} ;

	cairo_set_font_size (cr, 1.0 * NORMAL_FONT_SIZE) ;

	/* Label X axis. */
	snprintf (text, sizeof (text), "Time (secs)") ;
	cairo_text_extents (cr, text, &extents) ;
	cairo_move_to (cr, left + (width - extents.width) / 2, cairo_image_surface_get_height (surface) - 8) ;
	cairo_show_text (cr, text) ;

	/* Label Y axis (rotated). */
	snprintf (text, sizeof (text), "Frequency (Hz)") ;
	cairo_text_extents (cr, text, &extents) ;

	cairo_get_font_matrix (cr, &matrix) ;
	cairo_matrix_rotate (&matrix, -0.5 * M_PI) ;
	cairo_set_font_matrix (cr, &matrix) ;

	cairo_move_to (cr, cairo_image_surface_get_width (surface) - 12, top + (height + extents.width) / 2) ;
	cairo_show_text (cr, text) ;

	cairo_destroy (cr) ;
} /* render_spect_border */

static void
render_heat_border (cairo_surface_t * surface, double magfloor, const RECT *r)
{
	const char *decibels = "dB" ;
	char text [512] ;
	cairo_t * cr ;
	cairo_text_extents_t extents ;
	TICKS ticks ;
	int k, tick_count ;

	cr = cairo_create (surface) ;

	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0) ;
	cairo_set_line_width (cr, BORDER_LINE_WIDTH) ;

	/* Border around actual spectrogram. */
	cairo_rectangle (cr, r->left, r->top, r->width, r->height) ;
	cairo_stroke (cr) ;

	cairo_select_font_face (cr, font_family, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL) ;
	cairo_set_font_size (cr, 1.0 * NORMAL_FONT_SIZE) ;

	cairo_text_extents (cr, decibels, &extents) ;
	cairo_move_to (cr, r->left + (r->width - extents.width) / 2, r->top - 5) ;
	cairo_show_text (cr, decibels) ;

	tick_count = calculate_ticks (0.0, fabs (magfloor), r->height, false, &ticks) ;
	for (k = 0 ; k < tick_count ; k++)
	{	x_line (cr, r->left + r->width, r->top + ticks.distance [k], TICK_LEN) ;
		if (JUST_A_TICK (ticks, k))
			continue ;

		str_print_value (text, sizeof (text), -1.0 * ticks.value [k],
			ticks.decimal_places_to_print) ;
		cairo_text_extents (cr, text, &extents) ;
		cairo_move_to (cr, r->left + r->width + 2 * TICK_LEN, r->top + ticks.distance [k] + extents.height / 4.5) ;
		cairo_show_text (cr, text) ;
		} ;

	cairo_destroy (cr) ;
} /* render_heat_border */

/* Helper function:
** Map the index for an output pixel in a column to an index into the
** FFT result representing the same frequency.
** magindex is from 0 to maglen-1, representing min_freq to max_freq Hz.
** Return values from are from 0 to speclen representing frequencies from
** 0 to the Nyquist frequency.
** The result is a floating point number as it may fall between elements,
** allowing the caller to interpolate onto the input array.
*/
static double
magindex_to_specindex (int speclen, int maglen, int magindex, double min_freq, double max_freq, int samplerate, bool log_freq)
{
	double freq ; /* The frequency that this output value represents */

	if (!log_freq)
		freq = min_freq + (max_freq - min_freq) * magindex / (maglen - 1) ;
	else
		freq = min_freq * pow (max_freq / min_freq, (double) magindex / (maglen - 1)) ;

	return (freq * speclen / (samplerate / 2)) ;
}

/* Map values from the spectrogram onto an array of magnitudes, the values
** for display. Reads spec[0..speclen], writes mag[0..maglen-1].
*/
static void
interp_spec (float * mag, int maglen, const double *spec, int speclen, const RENDER *render, int samplerate)
{
	int k ;

	/* Map each output coordinate to where it depends on in the input array.
	** If there are more input values than output values, we need to average
	** a range of inputs.
	** If there are more output values than input values we do linear
	** interpolation between the two inputs values that a reverse-mapped
	** output value's coordinate falls between.
	**
	** spec points to an array with elements [0..speclen] inclusive
	** representing frequencies from 0 to samplerate/2 Hz. Map these to the
	** scale values min_freq to max_freq so that the bottom and top pixels
	** in the output represent the energy in the sound at min_ and max_freq Hz.
	*/

	for (k = 0 ; k < maglen ; k++)
	{	/* Average the pixels in the range it comes from */
		double this = magindex_to_specindex (speclen, maglen, k,
						render->min_freq, render->max_freq, samplerate,
						render->log_freq) ;
		double next = magindex_to_specindex (speclen, maglen, k+1,
						render->min_freq, render->max_freq, samplerate,
						render->log_freq) ;

		/* Range check: can happen if --max-freq > samplerate / 2 */
		if (this > speclen)
		{	mag [k] = 0.0 ;
			return ;
			} ;

		if (next > this + 1)
		{	/* The output indices are more sparse than the input indices
			** so average the range of input indices that map to this output,
			** making sure not to exceed the input array (0..speclen inclusive)
			*/
			/* Take a proportional part of the first sample */
			double count = 1.0 - (this - floor (this)) ;
			double sum = spec [(int) this] * count ;

			while ((this += 1.0) < next && (int) this <= speclen)
			{	sum += spec [(int) this] ;
				count += 1.0 ;
				}
			/* and part of the last one */
			if ((int) next <= speclen)
			{	sum += spec [(int) next] * (next - floor (next)) ;
				count += next - floor (next) ;
				} ;

			mag [k] = sum / count ;
			}
		else
		/* The output indices are more densely packed than the input indices
		** so interpolate between input values to generate more output values.
		*/
		{	/* Take a weighted average of the nearest values */
			mag [k] = spec [(int) this] * (1.0 - (this - floor (this)))
						+ spec [(int) this + 1] * (this - floor (this)) ;
			} ;
		} ;

	return ;
} /* interp_spec */

/* Pick the best FFT length good for FFTW?
**
** We use fftw_plan_r2r_1d() for which the documantation
** http://fftw.org/fftw3_doc/Real_002dto_002dReal-Transforms.html says:
**
** "FFTW is generally best at handling sizes of the form
** 2^a 3^b 5^c 7^d 11^e 13^f
** where e+f is either 0 or 1, and the other exponents are arbitrary."
*/

/* Helper function: does N have only 2, 3, 5 and 7 as its factors? */
static bool
is_2357 (int n)
{
	/* Just eliminate all factors os 2, 3, 5 and 7 and see if 1 remains */
	while (n % 2 == 0) n /= 2 ;
	while (n % 3 == 0) n /= 3 ;
	while (n % 5 == 0) n /= 5 ;
	while (n % 7 == 0) n /= 7 ;
	return (n == 1) ;
}

/* Helper function: is N a "fast" value for the FFT size? */
static bool
is_good_speclen (int n)
{
	/* It wants n, 11*n, 13*n but not (11*13*n)
	** where n only has as factors 2, 3, 5 and 7
	*/
	if (n % (11 * 13) == 0) return 0 ; /* No good */

	return is_2357 (n)	|| ((n % 11 == 0) && is_2357 (n / 11))
						|| ((n % 13 == 0) && is_2357 (n / 13)) ;
}

static void
render_to_surface (const RENDER * render, SNDFILE *infile, int samplerate, sf_count_t filelen, cairo_surface_t * surface)
{
	float ** mag_spec = NULL ; // Indexed by [w][h]

	spectrum *spec ;
	double max_mag = 0.0 ;
	int width, height, w, speclen ;

	if (render->border)
	{	width = lrint (cairo_image_surface_get_width (surface) - LEFT_BORDER - RIGHT_BORDER) ;
		height = lrint (cairo_image_surface_get_height (surface) - TOP_BORDER - BOTTOM_BORDER) ;
		}
	else
	{	width = render->width ;
		height = render->height ;
		}

	if (width < 1)
	{	printf ("Error : 'width' parameter must be >= %d\n",
			render->border ? (int) (LEFT_BORDER + RIGHT_BORDER) + 1 : 1) ;
		exit (1) ;
		} ;

	if (height < 1)
	{	printf ("Error : 'height' parameter must be >= %d\n",
			render->border ? (int) (TOP_BORDER + BOTTOM_BORDER) + 1 : 1) ;
		exit (1) ;
		} ;

	/*
	** Choose a speclen value, the spectrum length.
	** The FFT window size is twice this.
	*/
	if (render->fft_freq != 0.0)
		/* Choose an FFT window size of 1/fft_freq seconds of audio */
		speclen = (samplerate / render->fft_freq + 1) / 2 ;
	else
		/* Long enough to represent frequencies down to 20Hz. */
		speclen = height * (samplerate / 20 / height + 1) ;

	/* Find the nearest fast value for the FFT size. */
	{	int d ;	/* difference */

		for (d = 0 ; /* Will terminate */ ; d++)
		{	/* Logarithmically, the integer above is closer than
			** the integer below, so prefer it to the one below.
			*/
			if (is_good_speclen (speclen + d))
			{	speclen += d ;
				break ;
				}
			/* FFT length must also be >= the output height,
			** otherwise repeated pixel rows occur in the output.
			*/
			if (speclen - d >= height && is_good_speclen (speclen - d))
			{	speclen -= d ;
				break ;
				}
			}
		}

	mag_spec = calloc (width, sizeof (float *)) ;
	if (mag_spec == NULL)
	{	printf ("%s : Not enough memory.\n", __func__) ;
		exit (1) ;
		} ;
	for (w = 0 ; w < width ; w++)
	{	if ((mag_spec [w] = calloc (height, sizeof (float))) == NULL)
		{	printf ("%s : Not enough memory.\n", __func__) ;
			exit (1) ;
			} ;
		} ;

	spec = create_spectrum (speclen, render->window_function) ;

	if (spec == NULL)
	{	printf ("%s : line %d : create plan failed.\n", __FILE__, __LINE__) ;
		exit (1) ;
		} ;

	for (w = 0 ; w < width ; w++)
	{	double single_max ;

		read_mono_audio (infile, filelen, spec->time_domain, 2 * speclen, w, width) ;

		single_max = calc_magnitude_spectrum (spec) ;
		max_mag = MAX (max_mag, single_max) ;

		interp_spec (mag_spec [w], height, spec->mag_spec, speclen, render, samplerate) ;
		} ;

	destroy_spectrum (spec) ;

	if (render->border)
	{	RECT heat_rect ;

		heat_rect.left = 12 ;
		heat_rect.top = TOP_BORDER + TOP_BORDER / 2 ;
		heat_rect.width = 12 ;
		heat_rect.height = height - TOP_BORDER / 2 ;

		render_spectrogram (surface, render->spec_floor_db, mag_spec, max_mag, LEFT_BORDER, TOP_BORDER, width, height, render->gray_scale) ;

		render_heat_map (surface, render->spec_floor_db, &heat_rect, render->gray_scale) ;

		render_spect_border (surface, render->filename, LEFT_BORDER, width, filelen / (1.0 * samplerate), TOP_BORDER, height, render->min_freq, render->max_freq, render->log_freq) ;
		render_heat_border (surface, render->spec_floor_db, &heat_rect) ;
		}
	else
		render_spectrogram (surface, render->spec_floor_db, mag_spec, max_mag, 0, 0, width, height, render->gray_scale) ;

	for (w = 0 ; w < width ; w++)
		free (mag_spec [w]) ;
	free (mag_spec) ;

	return ;
} /* render_to_surface */

static void
render_cairo_surface (const RENDER * render, SNDFILE *infile, int samplerate, sf_count_t filelen)
{
	cairo_surface_t * surface = NULL ;
	cairo_status_t status ;

	/*
	**	CAIRO_FORMAT_RGB24 	 each pixel is a 32-bit quantity, with the upper 8 bits
	**	unused. Red, Green, and Blue are stored in the remaining 24 bits in that order.
	*/

	surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, render->width, render->height) ;
	if (surface == NULL || cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
	{	status = cairo_surface_status (surface) ;
		printf ("Error while creating surface : %s\n", cairo_status_to_string (status)) ;
		exit (1) ;
		} ;

	cairo_surface_flush (surface) ;

	render_to_surface (render, infile, samplerate, filelen, surface) ;

	status = cairo_surface_write_to_png (surface, render->pngfilepath) ;
	if (status != CAIRO_STATUS_SUCCESS)
	{	printf ("Error while creating PNG file : %s\n", cairo_status_to_string (status)) ;
		exit (1) ;
		} ;

	cairo_surface_destroy (surface) ;

	return ;
} /* render_cairo_surface */

static void
render_sndfile (RENDER * render)
{
	SNDFILE *infile ;
	SF_INFO info = { } ;

	infile = sf_open (render->sndfilepath, SFM_READ, &info) ;
	if (infile == NULL)
	{	printf ("Error : failed to open file '%s' : \n%s\n", render->sndfilepath, sf_strerror (NULL)) ;
		exit (1) ;
		} ;

	if (render->max_freq == 0.0)
		render->max_freq = (double) info.samplerate / 2 ;
	if (render->min_freq == 0.0 && render->log_freq)
		render->min_freq = 20.0 ;

	/* Do this sanity check here, as soon as max_freq has its default value */
	if (render->min_freq >= render->max_freq)
	{	printf ("Error : --min-freq (%g) must be less than max_freq (%g)\n",
			render->min_freq, render->max_freq) ;
		exit (1) ;
		} ;

	render_cairo_surface (render, infile, info.samplerate, info.frames) ;

	sf_close (infile) ;

	return ;
} /* render_sndfile */

static void
usage_exit (const char * argv0, int error)
{
	const char * progname ;

	progname = strrchr (argv0, '/') ;
	progname = (progname == NULL) ? argv0 : progname + 1 ;

	printf ("\nUsage :\n\n    %s [options] <sound file> <img width> <img height> <png name>\n\n", progname) ;

	puts (
		"    Create a spectrogram as a PNG file from a given sound file. The\n"
		"    spectrogram image will be of the given width and height.\n"
		) ;

	puts (
		"    Options:\n"
		"        --dyn-range=<number>   : Dynamic range (default is 180 for 180dB range)\n"
		"        --no-border            : Drop the border, scales, heat map and title\n"
		"        --min-freq=<number>    : Set the minimum frequency in the output\n"
		"        --max-freq=<number>    : Set the maximum frequency in the output\n"
		"        --fft-freq=<number>    : Set the lowest resolvable frequency and the\n"
		"                                 height of each band in the linear spectrogram.\n"
		"                                 Lower values increase frequency resolution but\n"
		"                                 smear the output horizontally and higher values\n"
		"                                 improve the temporal definition but decrease the\n"
		"                                 distinction between the lowest frequencies.\n"
		"        --log-freq             : Use a logarithmic frquency scale\n"
		"        --gray-scale           : Output gray pixels instead of a heat map\n"
		"        --kaiser               : Use a Kaiser window function (the default)\n"
		"        --rectangular          : Use a rectangular window function\n"
		"        --nuttall              : Use a Nuttall window function\n"
		"        --hann                 : Use a Hann window function\n"
		) ;

	exit (error) ;
} /* usage_exit */

int
main (int argc, char * argv [])
{	RENDER render =
	{	NULL, NULL, NULL,
		0, 0,				/* width, height */
		true, false, false, /* border, log_freq, gray_scale */
		0.0, 0.0, 0.0,		/* {min,max,fft}_freq */
		KAISER,
		SPEC_FLOOR_DB
		} ;
	int k ;

	if (argc < 5)
		usage_exit (argv [0], 0) ;

	for (k = 1 ; k < argc - 4 ; k++)
	{	double fval ;

		if (sscanf (argv [k], "--dyn-range=%lf", &fval) == 1)
		{	render.spec_floor_db = -1.0 * fabs (fval) ;
			continue ;
			}

		if (strcmp (argv [k], "--no-border") == 0)
		{	render.border = false ;
			continue ;
			} ;

		if (strcmp (argv [k], "--log-freq") == 0)
		{	render.log_freq = true ;
			continue ;
			} ;

		if (sscanf (argv [k], "--min-freq=%lf", &fval) == 1)
		{	if (fval < 0.0)
			{	printf ("--min-freq cannot be negative.\n") ;
				exit (1) ;
				} ;
			render.min_freq = fval ;
			continue ;
			}

		if (sscanf (argv [k], "--max-freq=%lf", &fval) == 1)
		{	render.max_freq = fabs (fval) ;
			continue ;
			}

		if (sscanf (argv [k], "--fft-freq=%lf", &fval) == 1)
		{	if (fval <= 0.0)
			{	printf ("--fft-freq must be positive.\n") ;
				exit (1) ;
				} ;
			render.fft_freq = fabs (fval) ;
			continue ;
			}

		if (sscanf (argv [k], "--dyn-range=%lf", &fval) == 1)
		{	render.spec_floor_db = -1.0 * fabs (fval) ;
			continue ;
			}

		if (strcmp (argv [k], "--gray-scale") == 0)
		{	render.gray_scale = true ;
			continue ;
			} ;

		if (strcmp (argv [k], "--rectangular") == 0)
		{	render.window_function = RECTANGULAR ;
			continue ;
			} ;

		if (strcmp (argv [k], "--kaiser") == 0)
		{	render.window_function = KAISER ;
			continue ;
			} ;

		if (strcmp (argv [k], "--nuttall") == 0)
		{	render.window_function = NUTTALL ;
			continue ;
			} ;

		if (strcmp (argv [k], "--hann") == 0)
		{	render.window_function = HANN ;
			continue ;
			} ;

		printf ("\nError : Bad command line argument '%s'\n", argv [k]) ;
		usage_exit (argv [0], 1) ;
		} ;

	render.sndfilepath = argv [k] ;
	render.width = parse_int_or_die (argv [k + 1], "width") ;
	render.height = parse_int_or_die (argv [k + 2], "height") ;
	render.pngfilepath = argv [k + 3] ;

	render.filename = strrchr (render.sndfilepath, '/') ;
	render.filename = (render.filename != NULL) ? render.filename + 1 : render.sndfilepath ;

	render_sndfile (&render) ;

	/* Certain FontConfig objects indirectly referenced via the Cairo
	 * static data are referenced by integer offsets rather than by
	 * pointers, so they appear lost to Valgrind unless we call this
	 * function. Otherwise we would not really care care about freeing
	 * static data before exiting.
	 */
	cairo_debug_reset_static_data () ;

	return 0 ;
} /* main */
