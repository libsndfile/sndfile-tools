/*
** Copyright (C) 2007-2009 Erik de Castro Lopo <erikd@mega-nerd.com>
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
**	Generate a waveform as a PNG file from a given sound file.
*/

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include <cairo.h>
#include <fftw3.h>

#include <sndfile.h>

#include "common.h"

#define	MIN_WIDTH	640
#define	MIN_HEIGHT	480
#define	MAX_WIDTH	8192
#define	MAX_HEIGHT	4096

#define TICK_LEN			6
#define	BORDER_LINE_WIDTH	1.8

#define	TITLE_FONT_SIZE		20.0
#define	NORMAL_FONT_SIZE	12.0

#define	LEFT_BORDER			65.0
#define	TOP_BORDER			30.0
#define	RIGHT_BORDER		75.0
#define	BOTTOM_BORDER		40.0

#define	SPEC_FLOOR_DB		-180.0


typedef struct
{	const char *sndfilepath, *pngfilepath, *filename ;
	int width, height ;
	bool border, log_freq ;
	double spec_floor_db ;
} RENDER ;

typedef struct
{	int left, top, width, height ;
} RECT ;

static const char font_family [] = "Terminus" ;

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
render_spectrogram (cairo_surface_t * surface, double spec_floor_db, float mag2d [MAX_WIDTH][MAX_HEIGHT], double maxval, double left, double top, double width, double height)
{
	/*
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

			get_colour_map_value (mag2d [w][h], spec_floor_db, colour) ;

			y = height + top - 1 - h ;
			x = (w + left) * 4 ;
			data [y * stride + x + 0] = colour [2] ;
			data [y * stride + x + 1] = colour [1] ;
			data [y * stride + x + 2] = colour [0] ;
			data [y * stride + x + 3] = 0 ;
			} ;

	cairo_surface_mark_dirty (surface) ;
	*/
} /* render_spectrogram */

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

typedef struct
{	double value [15] ;
	double distance [15] ;
} TICKS ;

static inline int
calculate_ticks (double max, double distance, TICKS * ticks)
{	const int div_array [] =
	{	10, 10, 8, 6, 8, 10, 6, 7, 8, 9, 10, 11, 12, 12, 7, 14, 8, 8, 9
		} ;

	double scale = 1.0, scale_max ;
	int k, leading, divisions ;

	if (max < 0)
	{	printf ("\nError in %s : max < 0\n\n", __func__) ;
		exit (1) ;
		} ;

	while (scale * max >= ARRAY_LEN (div_array))
		scale *= 0.1 ;

	while (scale * max < 1.0)
		scale *= 10.0 ;

	leading = lround (scale * max) ;
	divisions = div_array [leading % ARRAY_LEN (div_array)] ;

	/* Scale max down. */
	scale_max = leading / scale ;
	scale = scale_max / divisions ;

	if (divisions > ARRAY_LEN (ticks->value) - 1)
	{	printf ("Error : divisions (%d) > ARRAY_LEN (ticks->value) (%d)\n", divisions, ARRAY_LEN (ticks->value)) ;
		exit (1) ;
		} ;

	for (k = 0 ; k <= divisions ; k++)
	{	ticks->value [k] = k * scale ;
		ticks->distance [k] = distance * ticks->value [k] / max ;
		} ;

	return divisions + 1 ;
} /* calculate_ticks */

static void
str_print_value (char * text, int text_len, double value)
{
	if (fabs (value) < 1e-10)
		snprintf (text, text_len, "0") ;
	else if (fabs (value) >= 10.0)
		snprintf (text, text_len, "%1.0f", value) ;
	else if (fabs (value) >= 1.0)
		snprintf (text, text_len, "%3.1f", value) ;
	else
		snprintf (text, text_len, "%4.2f", value) ;

	return ;
} /* str_print_value */

static void
render_spect_border (cairo_surface_t * surface, const char * filename, double left, double width, double seconds, double top, double height, double max_freq)
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

	tick_count = calculate_ticks (seconds, width, &ticks) ;
	for (k = 0 ; k < tick_count ; k++)
	{	y_line (cr, left + ticks.distance [k], top + height, TICK_LEN) ;
		if (k % 2 == 1)
			continue ;
		str_print_value (text, sizeof (text), ticks.value [k]) ;
		cairo_text_extents (cr, text, &extents) ;
		cairo_move_to (cr, left + ticks.distance [k] - extents.width / 2, top + height + 8 + extents.height) ;
		cairo_show_text (cr, text) ;
		} ;

	tick_count = calculate_ticks (max_freq, height, &ticks) ;
	for (k = 0 ; k < tick_count ; k++)
	{	x_line (cr, left + width, top + height - ticks.distance [k], TICK_LEN) ;
		if (k % 2 == 1)
			continue ;
		str_print_value (text, sizeof (text), ticks.value [k]) ;
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
render_to_surface (const RENDER * render, SNDFILE *infile, int samplerate, sf_count_t filelen, cairo_surface_t * surface)
{
	int width, height, w, speclen ;

	if (render->border)
	{	width = lrint (cairo_image_surface_get_width (surface) - LEFT_BORDER - RIGHT_BORDER) ;
		height = lrint (cairo_image_surface_get_height (surface) - TOP_BORDER - BOTTOM_BORDER) ;
		}
	else
	{	width = render->width ;
		height = render->height ;
		}

	/*
	for (w = 0 ; w < width ; w++)
	{	double single_max ;

		read_mono_audio (infile, filelen, time_domain, 2 * speclen, w, width) ;


		fftw_execute (plan) ;

		single_max = calc_magnitude (freq_domain, 2 * speclen, single_mag_spec) ;
		max_mag = MAX (max_mag, single_max) ;

		interp_spec (mag_spec [w], height, single_mag_spec, speclen) ;
		} ;

	fftw_destroy_plan (plan) ;

	if (render->border)
	{	RECT heat_rect ;

		heat_rect.left = 12 ;
		heat_rect.top = TOP_BORDER + TOP_BORDER / 2 ;
		heat_rect.width = 12 ;
		heat_rect.height = height - TOP_BORDER / 2 ;

		render_spectrogram (surface, render->spec_floor_db, mag_spec, max_mag, LEFT_BORDER, TOP_BORDER, width, height) ;

		render_heat_map (surface, render->spec_floor_db, &heat_rect) ;

		render_spect_border (surface, render->filename, LEFT_BORDER, width, filelen / (1.0 * samplerate), TOP_BORDER, height, 0.5 * samplerate) ;
		render_heat_border (surface, render->spec_floor_db, &heat_rect) ;
		}
	else
		render_spectrogram (surface, render->spec_floor_db, mag_spec, max_mag, 0, 0, width, height) ;
	*/

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
		return ;
		} ;

	cairo_surface_flush (surface) ;

	render_to_surface (render, infile, samplerate, filelen, surface) ;

	status = cairo_surface_write_to_png (surface, render->pngfilepath) ;
	if (status != CAIRO_STATUS_SUCCESS)
		printf ("Error while creating PNG file : %s\n", cairo_status_to_string (status)) ;

	cairo_surface_destroy (surface) ;

	return ;
} /* render_cairo_surface */

static void
render_sndfile (const RENDER * render)
{
	SNDFILE *infile ;
	SF_INFO info ;

	if (render->log_freq)
	{	printf ("Error : --log-freq option not working yet.\n\n") ;
		return ;
		} ;

	memset (&info, 0, sizeof (info)) ;

	infile = sf_open (render->sndfilepath, SFM_READ, &info) ;
	if (infile == NULL)
	{	printf ("Error : failed to open file '%s' : \n%s\n", render->sndfilepath, sf_strerror (NULL)) ;
		return ;
		} ;

	render_cairo_surface (render, infile, info.samplerate, info.frames) ;

	sf_close (infile) ;

	return ;
} /* render_sndfile */

static void
check_int_range (const char * name, int value, int lower, int upper)
{
	if (value < lower || value > upper)
	{	printf ("Error : '%s' parameter must be in range [%d, %d]\n", name, lower, upper) ;
		exit (1) ;
		} ;
} /* check_int_range */

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
		"        --dyn-range=<number>   : Dynamic range (ie 100 for 100dB range)\n"
		"        --no-border            : Drop the border, scales, heat map and title\n"
		/*-"        --log-freq             : Use a logarithmic frquency scale\n" -*/
		) ;

	exit (error) ;
} /* usage_exit */

int
main (int argc, char * argv [])
{	RENDER render =
	{	NULL, NULL, NULL,
		0, 0,
		true, false,
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

		printf ("\nError : Bad command line argument '%s'\n", argv [k]) ;
		usage_exit (argv [0], 1) ;
		} ;

	render.sndfilepath = argv [k] ;
	render.width = atoi (argv [k + 1]) ;
	render.height = atoi (argv [k + 2]) ;
	render.pngfilepath = argv [k + 3] ;

	check_int_range ("width", render.width, MIN_WIDTH, MAX_WIDTH) ;
	check_int_range ("height", render.height, MIN_HEIGHT, MAX_HEIGHT) ;


	render.filename = strrchr (render.sndfilepath, '/') ;
	render.filename = (render.filename != NULL) ? render.filename + 1 : render.sndfilepath ;

	render_sndfile (&render) ;

	return 0 ;
} /* main */
