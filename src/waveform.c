/*
** Copyright (C) 2007-2015 Erik de Castro Lopo <erikd@mega-nerd.com>
** Copyright (C) 2012 Robin Gareus <robin@gareus.org>
** Copyright (C) 2013 driedfruit <driedfruit@mindloop.net>
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
#include <limits.h>
#include <libgen.h>
#include <getopt.h>

#include <cairo.h>

#include "common.h"

#include "config.h"

#define	MIN_WIDTH		(120)
#define	MIN_HEIGHT		(32)

#define TICK_LEN		(6)
#define TXT_TICK_LEN	(8)
#define	BORDER_LINE_WIDTH	(1.8)

#define	TITLE_FONT_SIZE		(20.0)
#define	NORMAL_FONT_SIZE	(12.0)

#define	LEFT_BORDER			(10.0)
#define	TOP_BORDER			(30.0)
#define	RIGHT_BORDER		(75.0)
#define	BOTTOM_BORDER		(40.0)

#define EXIT_FAILURE 1

#define C_COLOUR(X)	(X)->r, (X)->g, (X)->b, (X)->a

typedef struct COLOUR
{	double r ;
	double g ;
	double b ;
	double a ;
} COLOUR ;

typedef struct AGC
{	float min, max, rms ;
} AGC ;

typedef struct
{	const char *sndfilepath, *pngfilepath, *filename ;
	int width, height, channel_separation ;
	int channel ;
	int what ;
	bool autogain ;
	bool border, geometry_no_border, logscale, rectified ;
	COLOUR c_fg, c_rms, c_bg, c_ann, c_bbg, c_cl ;
	int tc_num, tc_den ;
	double tc_off ;
	bool parse_bwf ;
	double border_width ;
} RENDER ;

enum WHAT { PEAK = 1, RMS = 2 } ;

typedef struct DRECT
{	double x1, y1 ;
	double x2, y2 ;
} DRECT ;


#ifndef SF_BROADCAST_INFO_2K
typedef SF_BROADCAST_INFO_VAR (2048) SF_BROADCAST_INFO_2K ;
#endif

static inline void
set_colour (COLOUR * c, int h)
{	c->a = ((h >> 24) & 0xff) / 255.0 ;
	c->r = ((h >> 16) & 0xff) / 255.0 ;
	c->g = ((h >> 8) & 0xff) / 255.0 ;
	c->b = ((h) & 0xff) / 255.0 ;
} /* set_colour */

/* copied from ardour3 */
static inline float
_log_meter (float power, double lower_db, double upper_db, double non_linearity)
{
	return (power < lower_db ? 0.0 : pow ((power - lower_db) / (upper_db - lower_db), non_linearity)) ;
}

static inline float
alt_log_meter (float power)
{
	return _log_meter (power, -192.0, 0.0, 8.0) ;
}

static inline float coefficient_to_dB (float coeff) {
	return 20.0f * log10 (coeff) ;
}
/* end of ardour copy */

#ifdef EQUAL_VSPACE_LOG_TICKS
static inline float dB_to_coefficient (float dB) {
	return dB > -318.8f ? pow (10.0f, dB * 0.05f) : 0.0f ;
}

static inline float
inv_log_meter (float power)
{
	return (power < 0.00000000026 ? -192.0 : (pow (power, 0.125) * 192.0) -192.0) ;
}
#endif

static void
draw_cairo_line (cairo_t* cr, DRECT *pts, const COLOUR *c)
{
	cairo_set_source_rgba (cr, C_COLOUR (c)) ;
	cairo_move_to (cr, pts->x1, pts->y1) ;
	cairo_line_to (cr, pts->x2, pts->y2) ;
	cairo_stroke (cr) ;
}

static void
calc_peak (SNDFILE *infile, SF_INFO *info, double width, int channel, AGC *agc)
{
	int x = 0 ;
	float s_min, s_max, s_rms ;
	int channels ;
	long frames_per_buf, buffer_len ;

	const float frames_per_bin = info->frames / (float) width ;
	const long max_frames_per_bin = ceilf (frames_per_bin) ;
	float* data ;
	long f_offset = 0 ;

	if (channel < 0 || channel > info->channels)
	{	printf ("invalid channel\n") ;
		return ;
		} ;

	data = malloc (sizeof (float) * max_frames_per_bin * info->channels) ;
	if (!data)
	{	printf ("out of memory.\n") ;
		return ;
		} ;

	sf_seek (infile, 0, SEEK_SET) ;

	channels = (channel > 0) ? 1 : info->channels ;
	s_min = 1.0 ; s_max = -1.0 ; s_rms = 0.0 ;

	frames_per_buf = floorf (frames_per_bin) ;
	buffer_len = frames_per_buf * info->channels ;

	while ((sf_read_float (infile, data, buffer_len)) > 0)
	{	int frame ;
		float min, max, rms ;
		min = 1.0 ; max = -1.0 ; rms = 0.0 ;
		for (frame = 0 ; frame < frames_per_buf ; frame++)
		{	int ch ;
			for (ch = 0 ; ch < info->channels ; ch++)
			{	if (channel > 0 && ch + 1 != channel)
					continue ;
				if (frame * info->channels + ch > buffer_len)
				{	fprintf (stderr, "index error!\n") ;
					break ;
					} ;
				{
					const float sample_val = data [frame * info->channels + ch] ;
					max = MAX (max, sample_val) ;
					min = MIN (min, sample_val) ;
					rms += (sample_val * sample_val) ;
					} ;
				} ;
			} ;

		/* TODO: use a sliding window for RMS - independent of buffer_len */
		rms /= (frames_per_buf * channels) ;
		rms = sqrt (rms) ;

		if (min < s_min) s_min = min ;
		if (max > s_max) s_max = max ;
		if (rms > s_rms) s_rms = rms ;

		x++ ;
		if (x > width) break ;

		f_offset += frames_per_buf ;
		frames_per_buf = floorf ((x + 1) * frames_per_bin) - f_offset ;
		buffer_len = frames_per_buf * info->channels ;
		} ;

	agc->min = s_min ;
	agc->max = s_max ;
	agc->rms = s_rms ;
	free (data) ;
} /* calc_peak */

static void
render_waveform (cairo_surface_t * surface, RENDER *render, SNDFILE *infile, SF_INFO *info, double left, double top, double width, double height, int channel, float gain)
{
	cairo_t * cr ;

	float pmin = 0 ;
	float pmax = 0 ;
	float prms = 0 ;

	int x = 0 ;
	int channels ;
	long frames_per_buf, buffer_len ;

	const float frames_per_bin = info->frames / (float) width ;
	const long max_frames_per_bin = 1 + ceilf (frames_per_bin) ;
	float* data ;
	long f_offset = 0 ;

	if (channel < 0 || channel > info->channels)
	{	printf ("invalid channel\n") ;
		return ;
		} ;

	data = malloc (sizeof (float) * max_frames_per_bin * info->channels) ;
	if (!data)
	{	printf ("out of memory.\n") ;
		return ;
		} ;

	sf_seek (infile, 0, SEEK_SET) ;

	cr = cairo_create (surface) ;
	cairo_set_line_width (cr, render->border_width) ;
	cairo_rectangle (cr, left, top, width, height) ;
	cairo_stroke_preserve (cr) ;
	cairo_set_source_rgba (cr, C_COLOUR (&render->c_bg)) ;
	cairo_fill (cr) ;

	cairo_set_line_width (cr, 2.0) ;

	channels = (channel > 0) ? 1 : info->channels ;
	frames_per_buf = floorf (frames_per_bin) ;
	buffer_len = frames_per_buf * info->channels ;

	while ((sf_read_float (infile, data, buffer_len)) > 0)
	{	int frame ;
		float min, max, rms ;
		double yoff ;

		min = 1.0 ; max = -1.0 ; rms = 0.0 ;

		for (frame = 0 ; frame < frames_per_buf ; frame++)
		{	int ch ;
			for (ch = 0 ; ch < info->channels ; ch++)
			{	if (channel > 0 && ch + 1 != channel)
					continue ;
				if (frame * info->channels + ch > buffer_len)
				{	fprintf (stderr, "index error!\n") ;
					break ;
					} ;
				{
					const float sample_val = data [frame * info->channels + ch] ;
					max = MAX (max, sample_val) ;
					min = MIN (min, sample_val) ;
					rms += (sample_val * sample_val) ;
					} ;
				} ;
			} ;

		rms /= frames_per_buf * channels ;
		rms = sqrt (rms) ;
		if (gain != 1.0)
		{	min *= gain ;
			max *= gain ;
			rms *= gain ;
			} ;

		if (render->logscale)
		{	if (max > 0)
				max = alt_log_meter (coefficient_to_dB (max)) ;
			else
				max = -alt_log_meter (coefficient_to_dB (-max)) ;

			if (min > 0)
				min = alt_log_meter (coefficient_to_dB (min)) ;
			else
				min = -alt_log_meter (coefficient_to_dB (-min)) ;

			rms = alt_log_meter (coefficient_to_dB (rms)) ;
			} ;

		if (render->rectified)
		{	yoff = height ;
			min = height * MAX (fabsf (min), fabsf (max)) ;
			max = 0 ;
			rms = height * rms ;
			}
		else
		{	yoff = 0.5 * height ;
			min = min * yoff ;
			max = max * yoff ;
			rms = rms * yoff ;
			} ;

		/* Draw background - box */
		if ((render->what & (PEAK | RMS)) == PEAK)
		{
			if (render->rectified)
			{
				DRECT pts2 = { left + x, top + yoff - MIN (min, pmin), left + x, top + yoff } ;
				draw_cairo_line (cr, &pts2, &render->c_fg) ;
				}
			else
			{
				DRECT pts2 = { left + x, top + yoff - MAX (pmin, min), left + x, top + yoff - MIN (pmax, max) } ;
				draw_cairo_line (cr, &pts2, &render->c_fg) ;
				}
			}

		if (render->what & RMS)
		{
			if (render->rectified)
			{
				DRECT pts2 = { left + x, top + yoff - MIN (prms, rms), left + x, top + yoff } ;
				draw_cairo_line (cr, &pts2, &render->c_rms) ;
				}
			else
			{
				DRECT pts2 = { left + x, top + yoff - MIN (prms, rms), left + x, top + yoff + MIN (prms, rms) } ;
				draw_cairo_line (cr, &pts2, &render->c_rms) ;
				}
			}

		/* Draw Foreground - line */
		if (render->what & RMS)
		{
			DRECT pts0 = { left + x - 0.5, top + yoff - prms, left + x + 0.5, top + yoff - rms } ;
			draw_cairo_line (cr, &pts0, &render->c_rms) ;

			if (!render->rectified)
			{
				DRECT pts1 = { left + x - 0.5, top + yoff + prms, left + x + 0.5, top + yoff + rms } ;
				draw_cairo_line (cr, &pts1, &render->c_rms) ;
				}
			} ;

		if (render->what & PEAK)
		{
			DRECT pts0 = { left + x - 0.5, top + yoff - pmin, left + x + 0.5, top + yoff - min } ;
			draw_cairo_line (cr, &pts0, &render->c_fg) ;
			if (!render->rectified)
			{
				DRECT pts1 = { left + x - 0.5, top + yoff - pmax, left + x + 0.5, top + yoff - max } ;
				draw_cairo_line (cr, &pts1, &render->c_fg) ;
				}
			}

		pmin = min ;
		pmax = max ;
		prms = rms ;

		x++ ;
		if (x > width) break ;

		f_offset += frames_per_buf ;
		frames_per_buf = floorf ((x + 1) * frames_per_bin) - f_offset ;
		frames_per_buf = frames_per_buf > max_frames_per_bin ? max_frames_per_bin : frames_per_buf ;
		buffer_len = frames_per_buf * info->channels ;
		} ;

	if (!render->rectified)		// center line
	{	DRECT pts = { left, top + (0.5 * height) - 0.5, left + width, top + (0.5 * height) + 0.5 } ;
		cairo_set_line_width (cr, BORDER_LINE_WIDTH) ;
		draw_cairo_line (cr, &pts, &render->c_cl) ;
		} ;

	cairo_surface_mark_dirty (surface) ;
	cairo_destroy (cr) ;
	free (data) ;
} /* render_waveform */

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
	{	printf ("\nError in %s: max < 0\n\n", __func__) ;
		exit (EXIT_FAILURE) ;
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
	{	printf ("Error: divisions (%d) > ARRAY_LEN (ticks->value) (%d)\n", divisions, ARRAY_LEN (ticks->value)) ;
		exit (EXIT_FAILURE) ;
		} ;

	for (k = 0 ; k <= divisions ; k++)
	{	ticks->value [k] = k * scale ;
		ticks->distance [k] = distance * ticks->value [k] / max ;
		} ;

	return divisions + 1 ;
} /* calculate_ticks */

static inline int
calculate_log_ticks (bool rect, double distance, float gain, TICKS * ticks)
{	int cnt, i ;
#ifdef EQUAL_VSPACE_LOG_TICKS // equally spaced ticks, log values.
	cnt = calculate_ticks (rect ? 1.0 : 2.0, distance, ticks) ;
	for (i = 0 ; i < cnt ; i++)
	{	double v = ticks->value [i] ;
		if (!rect) v -= 1.0 ;
		if (v >= 0)
			v = (dB_to_coefficient (inv_log_meter (v))) ;
		else
			v = - (dB_to_coefficient (inv_log_meter (-v))) ;
		if (!rect) v += gain ;
		ticks->value [i] = v / gain ;
		} ;
#else // log spaced ticks,
	cnt = calculate_ticks (rect ? 1.0 : 2.0, distance, ticks) ;

	const double dx = rect ? distance : 0.5 * distance ;
	const double dd = rect ? 0.0 : 0.5 * distance ;

	for (i = 0 ; i < cnt ; i++)
	{	double d = (ticks->distance [i] - dd) / dx ;

		d *= gain ;

		if (d > 0)
			d = alt_log_meter (coefficient_to_dB (d)) ;
		else
			d = -alt_log_meter (coefficient_to_dB (-d)) ;

		ticks->distance [i] = d*dx+dd ;
		} ;
#endif
	return cnt ;
} /* calculate_log_ticks */

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


static bool
str_print_timecode (char * text, int text_len, double sec, int fps_num, int fps_den, double samplerate)
{
	const double flen = fps_num / fps_den ;

	const int hours	= (int) floor (sec / 3600.0) ;
	const int mins	= (int) floor ((sec - (3600.0 * hours)) / 60.0) ;
	const int secs	= (int) floor (sec) % 60 ;
	const int frame	= (int) floor ((sec - floor (sec)) * (1.0 * fps_num) / (1.0 * fps_den)) ;

	if (flen < 0.0)
		snprintf (text, text_len, "%ld", (long) rint (sec * samplerate)) ;
	else if (flen <= 1.0)
		snprintf (text, text_len, "%02d:%02d:%02d", hours, mins, secs) ;
	else if (flen <= 10.0)
		snprintf (text, text_len, "%02d:%02d:%02d.%01d", hours, mins, secs, frame) ;
	else if (flen <= 100.0)
		snprintf (text, text_len, "%02d:%02d:%02d.%02d", hours, mins, secs, frame) ;
	else
		snprintf (text, text_len, "%02d:%02d:%02d.%03d", hours, mins, secs, frame) ;

	text [text_len-1] = '\0' ;
	return (flen < 0.0) ? true : false ;
} /* str_print_timecode */

static void
render_title (cairo_surface_t * surface, const RENDER * render, double left, double top, int file_channels)
{
	int cxoffset = 0 ;
	int cyoffset = 0 ;
	char text [512] ;
	cairo_t * cr ;
	cairo_text_extents_t extents ;

	cr = cairo_create (surface) ;

	cairo_set_source_rgba (cr, C_COLOUR (&render->c_ann)) ;
	cairo_set_line_width (cr, BORDER_LINE_WIDTH) ;

	/* Print title. */
	cairo_select_font_face (cr, font_family, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL) ;
	cairo_set_font_size (cr, 1.0 * TITLE_FONT_SIZE) ;


	snprintf (text, sizeof (text), "Waveform: %s", render->filename) ;
	cairo_text_extents (cr, text, &extents) ;
	cairo_move_to (cr, left + 2, top - extents.height / 2) ;
	cairo_show_text (cr, text) ;

	if (render->channel > 0)
	{	snprintf (text, sizeof (text), " (channel: %d)", render->channel) ;
		cxoffset = extents.width ;
		cyoffset = extents.height ;
		}
	else if (render->channel == 0 && file_channels > 1)
	{	snprintf (text, sizeof (text), " (downmixed to mono)") ;
		cxoffset = extents.width ;
		cyoffset = extents.height ;
		}

	if (cxoffset > 0)
	{	cairo_set_font_size (cr, 1.0 * NORMAL_FONT_SIZE) ;
		cairo_text_extents (cr, text, &extents) ;
		cairo_move_to (cr, left + 2 + cxoffset, top - cyoffset / 2) ;
		cairo_show_text (cr, text) ;
		}

	cairo_destroy (cr) ;
} /* render_title */


static void
render_timeaxis (cairo_surface_t * surface, const RENDER * render, const SF_INFO *info, double left, double width, double top, double height)
{
	char text [32] ;
	double seconds = info->frames / (1.0 * info->samplerate) ;
	cairo_t * cr ;
	cairo_text_extents_t extents ;

	TICKS ticks ;
	int k, tick_count ;

	cr = cairo_create (surface) ;

	cairo_set_source_rgba (cr, C_COLOUR (&render->c_ann)) ;
	cairo_set_line_width (cr, BORDER_LINE_WIDTH) ;

	/* Print labels. */
	cairo_select_font_face (cr, font_family, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL) ;
	cairo_set_font_size (cr, 1.0 * NORMAL_FONT_SIZE) ;

	/* X-Axis -- time */
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

	cairo_set_font_size (cr, 1.0 * NORMAL_FONT_SIZE) ;

	/* Label X axis. */
	snprintf (text, sizeof (text), "Time (secs)") ;
	cairo_text_extents (cr, text, &extents) ;
	cairo_move_to (cr, left + (width - extents.width) / 2, cairo_image_surface_get_height (surface) - 8) ;
	cairo_show_text (cr, text) ;

	cairo_destroy (cr) ;
} /* render_timeaxis */

static void
render_timecode (cairo_surface_t * surface, const RENDER * render, const SF_INFO *info, double left, double width, double top, double height)
{
	char text [32] ;
	bool print_label = false ;
	double seconds = info->frames / (1.0 * info->samplerate) ;
	cairo_t * cr ;
	cairo_text_extents_t extents ;

	TICKS ticks ;
	int k, tick_count ;

	cr = cairo_create (surface) ;

	cairo_set_source_rgba (cr, C_COLOUR (&render->c_ann)) ;
	cairo_set_line_width (cr, BORDER_LINE_WIDTH) ;

	/* Print labels. */
	cairo_select_font_face (cr, font_family, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL) ;
	cairo_set_font_size (cr, 1.0 * NORMAL_FONT_SIZE) ;

	/* X-Axis -- time */
	tick_count = calculate_ticks (seconds, width, &ticks) ;
	for (k = 0 ; k < tick_count ; k++)
	{	double yoff = 0 ;
		y_line (cr, left + ticks.distance [k], top + height, TICK_LEN) ;
		if (k % 2 == 1)
			yoff = 1.0 * NORMAL_FONT_SIZE ;

		print_label = str_print_timecode (text, sizeof (text), ticks.value [k] + render->tc_off, render->tc_num, render->tc_den, info->samplerate) ;
		cairo_text_extents (cr, text, &extents) ;
		cairo_move_to (cr, left + ticks.distance [k] - extents.width / 8, top + height + 8 + extents.height +yoff) ;
		cairo_show_text (cr, text) ;
		} ;

	cairo_set_font_size (cr, 1.0 * NORMAL_FONT_SIZE) ;

	if (print_label)
	{
		/* Label X axis. */
		snprintf (text, sizeof (text), "Time [Frames]") ;
		cairo_text_extents (cr, text, &extents) ;
		cairo_move_to (cr, left + width + RIGHT_BORDER - extents.width -2 , cairo_image_surface_get_height (surface) - 8) ;
		cairo_show_text (cr, text) ;
		} ;

	cairo_destroy (cr) ;
} /* render_timecode */

static void
render_wav_border (cairo_surface_t * surface, const RENDER * render, double left, double width, double top, double height, float gain)
{
	char text [8] ;
	cairo_t * cr ;
	cairo_text_extents_t extents ;

	cr = cairo_create (surface) ;

	cairo_set_source_rgba (cr, C_COLOUR (&render->c_ann)) ;
	cairo_set_line_width (cr, BORDER_LINE_WIDTH) ;

	/* Border around actual spectrogram. */
	cairo_rectangle (cr, left, top, width, height) ;
	cairo_stroke (cr) ;

	cairo_select_font_face (cr, font_family, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL) ;
	cairo_set_font_size (cr, 1.0 * NORMAL_FONT_SIZE) ;

	if (render->logscale)
	{	TICKS ticks ;
		int k, tick_count ;
		tick_count = calculate_log_ticks (render->rectified, height, gain, &ticks) ;
		for (k = 0 ; k < tick_count ; k++)
		{	if (ticks.distance [k] < 0) continue ;
			if (ticks.distance [k] > height) continue ;
			x_line (cr, left + width, top + height - ticks.distance [k], (k % 2) ? TICK_LEN : TXT_TICK_LEN) ;
			if (k % 2 == 1)
				continue ;
			str_print_value (text, sizeof (text), ticks.value [k] - (render->rectified ? 0.0 : 1.0)) ;
			cairo_text_extents (cr, text, &extents) ;
			cairo_move_to (cr, left + width + 12, top + height - ticks.distance [k] + extents.height / 4.5) ;
			cairo_show_text (cr, text) ;
			} ;
		}
	else
	{	TICKS ticks ;
		int k, tick_count ;
		tick_count = calculate_ticks ((render->rectified ? 1.0 : 2.0), height, &ticks) ;
		for (k = 0 ; k < tick_count ; k++)
		{	x_line (cr, left + width, top + height - ticks.distance [k], (k % 2) ? TICK_LEN : TXT_TICK_LEN) ;
			if (k % 2 == 1)
				continue ;
			str_print_value (text, sizeof (text), (ticks.value [k] - (render->rectified ? 0.0 : 1.0)) / gain) ;
			cairo_text_extents (cr, text, &extents) ;
			cairo_move_to (cr, left + width + 12, top + height - ticks.distance [k] + extents.height / 4.5) ;
			cairo_show_text (cr, text) ;
			} ;
		} ;

#ifdef WITH_Y_LABEL
	cairo_matrix_t matrix ;
	cairo_set_font_size (cr, 1.0 * NORMAL_FONT_SIZE) ;

	/* Label Y axis (rotated). */
	snprintf (text, sizeof (text), "Value") ;
	cairo_text_extents (cr, text, &extents) ;

	cairo_get_font_matrix (cr, &matrix) ;
	cairo_matrix_rotate (&matrix, -0.5 * M_PI) ;
	cairo_set_font_matrix (cr, &matrix) ;

	cairo_move_to (cr, cairo_image_surface_get_width (surface) - 12, top + (height + extents.width) / 2) ;
	cairo_show_text (cr, text) ;
#endif

	cairo_destroy (cr) ;
} /* render_wav_border */

static void
render_y_legend (cairo_surface_t * surface, const RENDER * render, double top, double height)
{
#ifndef WITH_Y_LABEL
	double lx, ly, dxy, dh ;
	cairo_t * cr ;
	cairo_text_extents_t extents ;

	cr = cairo_create (surface) ;

	dh = 0 ;
	dxy= NORMAL_FONT_SIZE * 0.65 ;

	if (render->what & RMS)
	{	cairo_text_extents (cr, "RMS", &extents) ;
		dh += dxy + extents.width ;
		} ;
	if (render->what & PEAK)
	{	cairo_text_extents (cr, "Peak", &extents) ;
		dh += dxy + extents.width ;
		} ;
	if ((render->what & (PEAK | RMS)) == (PEAK | RMS)) { dh += 8 ; }

	lx = cairo_image_surface_get_width (surface) - 12 - dxy ;
	ly = top + (height + dh) / 2 ;

	cairo_set_line_width (cr, 2.0) ;

	if (render->what & RMS)
	{	cairo_matrix_t matrix ;

		cairo_set_source_rgba (cr, C_COLOUR (&render->c_bg)) ;
		cairo_rectangle (cr, lx, ly, dxy, dxy) ;
		cairo_fill (cr) ;
		cairo_set_source_rgba (cr, C_COLOUR (&render->c_rms)) ;
		cairo_rectangle (cr, lx, ly, dxy, dxy) ;
		cairo_fill (cr) ;
		cairo_set_source_rgba (cr, C_COLOUR (&render->c_ann)) ;
		cairo_rectangle (cr, lx, ly, dxy, dxy) ;
		cairo_stroke (cr) ;
		ly -= dxy + 2.5 ;

		cairo_set_font_size (cr, 1.0 * NORMAL_FONT_SIZE) ;
		cairo_set_source_rgba (cr, C_COLOUR (&render->c_ann)) ;
		cairo_move_to (cr, lx + dxy +.5 , ly + dxy) ;
		cairo_get_font_matrix (cr, &matrix) ;
		cairo_matrix_rotate (&matrix, -0.5 * M_PI) ;
		cairo_set_font_matrix (cr, &matrix) ;
		cairo_show_text (cr, "RMS") ;

		cairo_text_extents (cr, "RMS", &extents) ;
		ly -= extents.height + 8 ;
		}

	if (render->what & PEAK)
	{	cairo_matrix_t matrix ;

		cairo_set_source_rgba (cr, C_COLOUR (&render->c_bg)) ;
		cairo_rectangle (cr, lx, ly, dxy, dxy) ;
		cairo_fill (cr) ;
		cairo_set_source_rgba (cr, C_COLOUR (&render->c_fg)) ;
		cairo_rectangle (cr, lx, ly, dxy, dxy) ;
		cairo_fill (cr) ;
		cairo_set_source_rgba (cr, C_COLOUR (&render->c_ann)) ;
		cairo_rectangle (cr, lx, ly, dxy, dxy) ;
		cairo_stroke (cr) ;
		ly -= dxy + 2.5 ;

		cairo_set_font_size (cr, 1.0 * NORMAL_FONT_SIZE) ;
		cairo_set_source_rgba (cr, C_COLOUR (&render->c_ann)) ;
		cairo_move_to (cr, lx + dxy +.5 , ly + dxy) ;
		cairo_get_font_matrix (cr, &matrix) ;
		cairo_matrix_rotate (&matrix, -0.5 * M_PI) ;
		cairo_set_font_matrix (cr, &matrix) ;
		cairo_show_text (cr, "Peak") ;
		}

	cairo_destroy (cr) ;
#endif
} /* render_y_legend */

static void
render_to_surface (RENDER * render, SNDFILE *infile, SF_INFO *info, cairo_surface_t * surface)
{
	double width, height ;

	if (render->border)
	{	width = lrint (cairo_image_surface_get_width (surface) - LEFT_BORDER - RIGHT_BORDER) ;
		height = lrint (cairo_image_surface_get_height (surface) - TOP_BORDER - BOTTOM_BORDER) ;
		}
	else
	{	width = render->width ;
		height = render->height ;
		}

	cairo_t * cr ;
	cr = cairo_create (surface) ;

	/* wave-form background */
	cairo_rectangle (cr, 0, 0, render->width, render->height) ;
	cairo_set_line_width (cr, render->border_width) ;
	cairo_stroke_preserve (cr) ;
	cairo_set_source_rgba (cr, C_COLOUR (&render->c_bbg)) ;
	cairo_fill (cr) ;

	if (render->channel < 0)
	{	const double chnsep = render->channel_separation ;
		const double mheight = (height - (info->channels - 1) * chnsep) / (1.0 * info->channels) ;
		int ch ;
		float gain = 1.0 ;
		/* calc gain of all channels */
		if (render->autogain)
		{	float mxv = 0.0 ;
			for (ch = 0 ; ch < info->channels ; ch++)
			{
				AGC agc ;
				calc_peak (infile, info, width, ch + 1, &agc) ;
				if (render->what & PEAK)
					mxv = MAX (mxv, MAX (agc.max, -agc.min)) ;
				if (render->what & RMS)
					mxv = MAX (mxv, agc.rms) ;
				}
			if (mxv != 0)
				gain = 1.0 / mxv ;
			}

		for (ch = 0 ; ch < info->channels ; ch++)
		{
			render_waveform (surface, render, infile, info,
					(render->border ? LEFT_BORDER : 0),
					(render->border ? TOP_BORDER : 0) + ((mheight + chnsep) * (1.0 * ch)),
					width, mheight, ch + 1, gain) ;

			if (render->border)
				render_wav_border (surface, render,
						LEFT_BORDER, width,
						TOP_BORDER + (mheight + chnsep) * (1.0 * ch), mheight, gain) ;
			else if (ch > 0 && chnsep > 0)
			{	cairo_rectangle (cr, 0, ((mheight + chnsep) * (1.0 * ch)) - chnsep, render->width, chnsep) ;
				cairo_stroke_preserve (cr) ;
				cairo_set_source_rgba (cr, C_COLOUR (&render->c_bg)) ;
				cairo_fill (cr) ;
				} ;
		}
	} else
	{	float gain = 1.0 ;
		if (render->autogain)
		{	AGC agc ;
			calc_peak (infile, info, width, render->channel, &agc) ;
			float mxv = 0.0 ;
				if (render->what & PEAK)
					mxv = MAX (mxv, MAX (agc.max, -agc.min)) ;
				if (render->what & RMS)
					mxv = MAX (mxv, agc.rms) ;
			if (mxv != 0)
				gain = 1.0 / mxv ;
			} ;
		render_waveform (surface, render, infile, info,
			(render->border ? LEFT_BORDER : 0.0), (render->border ? TOP_BORDER : 0.0),
			width, height, render->channel, gain) ;
		if (render->border)
			render_wav_border (surface, render, LEFT_BORDER, width, TOP_BORDER, height, gain) ;
		} ;

	if (render->border)
	{	render_title (surface, render, LEFT_BORDER, TOP_BORDER, info->channels) ;
		render_y_legend (surface, render, TOP_BORDER, height) ;
		if (render->tc_den > 0)
			render_timecode (surface, render, info, LEFT_BORDER, width, TOP_BORDER, height) ;
		else
			render_timeaxis (surface, render, info, LEFT_BORDER, width, TOP_BORDER, height) ;
		} ;

	cairo_destroy (cr) ;
	return ;
} /* render_to_surface */

static void
render_cairo_surface (RENDER * render, SNDFILE *infile, SF_INFO *info)
{
	cairo_surface_t * surface = NULL ;
	cairo_status_t status ;

	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, render->width, render->height) ;
	if (surface == NULL || cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
	{	status = cairo_surface_status (surface) ;
		printf ("Error while creating surface: %s\n", cairo_status_to_string (status)) ;
		return ;
		} ;

	cairo_surface_flush (surface) ;

	render_to_surface (render, infile, info, surface) ;

	status = cairo_surface_write_to_png (surface, render->pngfilepath) ;
	if (status != CAIRO_STATUS_SUCCESS)
		printf ("Error while creating PNG file: %s\n", cairo_status_to_string (status)) ;

	cairo_surface_destroy (surface) ;

	return ;
} /* render_cairo_surface */

static void
render_sndfile (RENDER * render)
{
	SNDFILE *infile ;
	SF_INFO info = { } ;
	sf_count_t max_width ;

	infile = sf_open (render->sndfilepath, SFM_READ, &info) ;
	if (infile == NULL)
	{	printf ("Error: failed to open file '%s': \n%s\n", render->sndfilepath, sf_strerror (NULL)) ;
		exit (EXIT_FAILURE) ;
		} ;

	if (render->channel > info.channels)
	{	printf ("Error: channel parameter must be in range [%d, %d]\n", -1, info.channels) ;
		sf_close (infile) ;
		exit (EXIT_FAILURE) ;
		} ;

	max_width = info.frames ;
	if (render->border)
		max_width += LEFT_BORDER + RIGHT_BORDER ;

	if (render->width > max_width)
	{	printf ("Error: soundfile is too short. Decrease image width below %ld.\n", (long int) max_width) ;
		sf_close (infile) ;
		exit (EXIT_FAILURE) ;
		} ;

	if (render->geometry_no_border)	// given geometry applies to wave-form (per channel) without border.
	{	if (render->channel < 0)
			render->height = render->height * info.channels + (info.channels-1) * render->channel_separation ;

		if (render->border)
		{	render->width += LEFT_BORDER + RIGHT_BORDER ;
			render->height += TOP_BORDER + BOTTOM_BORDER ;
			} ;
		} ;


	if (render->tc_den > 0 && render->parse_bwf)	/* use BWF timecode offset */
	{	SF_BROADCAST_INFO_2K binfo = { } ;
		if (sf_command (infile, SFC_GET_BROADCAST_INFO, &binfo, sizeof (binfo)))
		{	int64_t to = binfo.time_reference_high ;
			to = (to << 32) + binfo.time_reference_low ;
			render->tc_off = (double) to ;
			}
		else
			render->tc_off = 0 ;
		} ;
	render->tc_off /= 1.0 * info.samplerate ;

	render_cairo_surface (render, infile, &info) ;

	sf_close (infile) ;

	return ;
} /* render_sndfile */

static void
check_int_range (const char * name, int value, int lower, int upper)
{
	if (value < lower || value > upper)
	{	printf ("Error: '%s' parameter must be in range [%d, %d]\n", name, lower, upper) ;
		exit (EXIT_FAILURE) ;
		} ;
} /* check_int_range */


/* NOTE: after editing this, run
 * make && help2man -N -n 'waveform image generator' ./bin/sndfile-waveform -o man/sndfile-waveform.1
 */
static void
usage_exit (char * argv0, int status)
{
	printf ("%s - waveform image generator\n\n", basename (argv0)) ;
	printf (
		"Creates a PNG image depicting the wave-form of an audio file.\n"
		"Peak-signal and RMS values can be displayed in the same plot,\n"
		"where the horizontal axis always represents time.\n"
		"\n"
		"The vertical axis can be plotted logarithmically, and the signal\n"
		"can optionally be rectified.\n"
		"\n"
		"The Time-axis annotation unit is either seconds, audio-frames or timecode\n"
		"using broadcast-wave time reference meta-data.\n"
		"\n"
		"The tool can plot individual channels, reduce the file to mono,\n"
		"or plot all channels in vertically arrangement.\n"
		"\n"
		"Colours (ARGB) and image- or waveform geometry can be freely specified.\n"
		"\n") ;

	printf ("Usage: %s [OPTION]  <sound-file> <png-file>\n", argv0) ;
	printf ("\n"
		"Options:\n"
		"  -A, --textcolour <COL>    specify text and border colour; default 0xffffffff\n"
		"                            all colours as hexadecimal AA RR GG BB values\n"
		"  -b, --border              display a border with annotations\n"
		"  -B, --background <COL>    specify background colour; default 0x8099999f\n"
		"  -c, --channel             choose channel (s) to plot, 0: merge to mono;\n"
		"                            < 0: render all channels vertically separated;\n"
		"                            > 0: render only specified channel. (default: 0)\n"
		"  -C, --centerline <COL>    set colour of zero/center line (default 0x4cffffff)\n"
		"  -F, --foreground <COL>    specify foreground colour; default 0xff333333\n"
		"  -g <w>x<h>, --geometry <w>x<h>\n"
		"                            specify the size of the image to create\n"
		"                            default: 800x192\n"
		"  -G, --borderbg <COL>      specify border/annotation background colour;\n"
		"                            default 0xb3ffffff\n"
		"  -h, --help                display this help and exit\n"
		"  -l, --logscale            use logarithmic scale\n"
		"  --no-peak                 only draw RMS signal using foreground colour\n"
		"  --no-rms                  only draw signal peaks (exclusive with --no-peak).\n"
		"  -r, --rectified           rectify waveform\n"
		"  -R, --rmscolour  <COL>    specify RMS colour; default 0xffb3b3b3\n"
		"  -s, --gainscale           zoom into y-axis, map max signal to height.\n"
		"  -S, --separator <px>      vertically separate channels by N pixels\n"
		"                            (default: 12) - only used with -c -1\n"
		"  -t <NUM>[/<DEN>], --timecode <NUM>[/<DEN>]\n"
		"                            use timecode instead of seconds for x-axis;\n"
		"                            The numerator must be set, the denominator\n"
		"                            defaults to 1 if omitted.\n"
		"                            If the value is negative, audio-frames are used.\n"
		"  -T <offset>               override the BWF time-reference (if any);\n"
		"                            the offset is specified in audio-frames\n"
		"                            and only used with timecode (-t) annotation.\n"
		"  -O, --border-width        change outer border width\n"
		"                            default: 1.0\n"
		"  -V, --version             output version information and exit\n"
		"  -W, --wavesize            given geometry applies to the plain wave-form.\n"
		"                            image height depends on number of channels.\n"
		"                            border-sizes are added to width and height.\n"
		"\n"
		"Report bugs to <robin@gareus.org>.\n"
		"Website and manual: <https://github.com/libsndfile/sndfile-tools/>\n"
		"Example images: <http://gareus.org/wiki/sndfile-waveform/>\n"
		"\n") ;
	exit (status) ;
} /* usage_exit */

static struct option const long_options [] =
{
	{ "help", no_argument, 0, 'h' },
	{ "version", no_argument, 0, 'V' },

	{ "border", no_argument, 0, 'b' },
	{ "logscale", no_argument, 0, 'l' },
	{ "rectified", no_argument, 0, 'r' },
	{ "rectify", no_argument, 0, 'r' },
	{ "border-width", required_argument, 0, 'O' },

	{ "geometry", required_argument, 0, 'g' },
	{ "separator", required_argument, 0, 'S' },
	{ "wavesize", no_argument, 0, 'W' },
	{ "gainscale", no_argument, 0, 's' },

	{ "channel", required_argument, 0, 'c' },

	{ "textcolour", required_argument, 0, 'A' },
	{ "foreground", required_argument, 0, 'F' },
	{ "background", required_argument, 0, 'B' },
	{ "rmscolour", required_argument, 0, 'R' },
	{ "borderbg", required_argument, 0, 'G' },

	{ "timecode", required_argument, 0, 't' },
	{ "timeoffset", required_argument, 0, 'T' },

	{ "no-peak", no_argument, 0, 1 },
	{ "no-rms", no_argument, 0, 2 },
	{ NULL, 0, NULL, 0 }
} ;


int
main (int argc, char * argv [])
{	RENDER render =
	{	NULL, NULL, NULL,
		/*width*/ 800, /*height*/ 200,
		/*channel_separation*/ NORMAL_FONT_SIZE,
		/*channel*/ 0,
		/*what*/ PEAK | RMS,
		/*autogain*/ false,
		/*border*/ false,
		/*geometry_no_border*/ false,
		/*logscale*/ false, /*rectified*/ false,
		/*foreground*/	{ 0.2, 0.2, 0.2, 1.0 },
		/*wave-rms*/	{ 0.7, 0.7, 0.7, 1.0 },
		/*background*/	{ 0.6, 0.6, 0.6, 0.5 },
		/*annotation*/	{ 1.0, 1.0, 1.0, 1.0 },
		/*border-bg*/	{ 0.0, 0.0, 0.0, 0.7 },
		/*center-line*/	{ 1.0, 1.0, 1.0, 0.3 },
		/*timecode num*/ 0, /*den*/ 0, /*offset*/ 0.0,
		/*parse BWF*/ true,
		/*border-width*/ 2.0f,
		} ;

	int c ;
	while ((c = getopt_long (argc, argv,
				"A:"	/*	--annotation	*/
				"b"		/*	--border	*/
				"B:"	/*	--background	*/
				"c:"	/*	--channel	*/
				"C:"	/*	--centerline	*/
				"F:"	/*	--foreground	*/
				"G:"	/*	--borderbg	*/
				"g:"	/*	--geometry	*/
				"O:"	/*	--border-width	*/
				"h"		/*	--help	*/
				"l"		/*	--logscale	*/
				"r"		/*	--rectified	*/
				"R:"	/*	--rmscolour	*/
				"t:"	/*	--timecode	*/
				"s"		/*	--gainscale	*/
				"S:"	/*	--separator	*/
				"T:"	/*	--timeoffset	*/
				"W"		/*	--wavesize	*/
				"1"		/*	--no-peak	*/
				"2"		/*	--no-rms	*/
				"V",	/*	--version	*/
				long_options,	NULL))	!=	EOF)
		switch (c)
		{	case 'A' :		/* --annotation */
				set_colour (&render.c_ann, strtoll (optarg, NULL, 16)) ;
				break ;
			case 'B' :		/* --background */
				set_colour (&render.c_bg, strtoll (optarg, NULL, 16)) ;
				break ;
			case 'b' :		/* --border */
				render.border = true ;
				break ;
			case 'c' :		/* --channel */
				render.channel = parse_int_or_die (optarg, "channel") ;
				break ;
			case 'C' :		/* --centerline */
				set_colour (&render.c_cl, strtoll (optarg, NULL, 16)) ;
				break ;
			case 'F' :		/* --foreground */
				set_colour (&render.c_fg, strtoll (optarg, NULL, 16)) ;
				break ;
			case 'G' :		/* --borderbg */
				set_colour (&render.c_bbg, strtoll (optarg, NULL, 16)) ;
				break ;
			case 'W' :		/* --borderbg */
				render.geometry_no_border = true ;
				break ;
			case 'l' :		/* --logscale */
				render.logscale = true ;
				break ;
			case 'g' :		/* --geometry*/
				{
					char *b = strdup (optarg) ;
					render.width = atoi (optarg) ;
					if (strtok (b, "x:/"))
					{	char *tmp = strtok (NULL, "x:/") ;
						if (tmp) render.height = atoi (tmp) ;
						} ;
					free (b) ;
				} ;
				break ;
			case 'r' :		/* --rectified */
				render.rectified = true ;
				break ;
			case 'R' :		/* --rmscolour */
				set_colour (&render.c_rms, strtoll (optarg, NULL, 16)) ;
				break ;
			case 's' :		/* --gainscale */
				render.autogain = true ;
				break ;
			case 'S' :		/* --separator */
				render.channel_separation = parse_int_or_die (optarg, "separator") ;
				break ;
			case 't' :		/* --timecode*/
				{
					char *b = strdup (optarg) ;
					render.tc_num = atoi (optarg) ;
					render.tc_den = 1 ;
					if (strtok (b, ":/"))
					{	char *tmp = strtok (NULL, ":/") ;
						if (tmp) render.tc_den = atoi (tmp) ;
						} ;
					free (b) ;
				} ;
				break ;
			case 'T' :		/* --timeoffset */
				render.parse_bwf = false ;
				render.tc_off = strtod (optarg, NULL) ;
				break ;
			case 'O' :
				render.border_width = strtod (optarg, NULL) * 2.0f ;
				break ;
			case 1 :
				memcpy (&render.c_rms, &render.c_fg, sizeof (COLOUR)) ;
				render.what &= ~PEAK ;
				break ;
			case 2 :
				render.what &= ~RMS ;
				break ;
			case 'V' :
				printf ("%s %s\n\n", argv [0], PACKAGE_VERSION) ;
				printf (
					"Copyright (C) 2007-2012 Erik de Castro Lopo <erikd@mega-nerd.com>\n"
					"Written 2011,2012 by Robin Gareus <robin@gareus.org>\n\n"
					"This is free software; see the source for copying conditions.  There is NO\n"
					"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n"
					) ;
				exit (0) ;
			case 'h' :
				usage_exit (argv [0], 0) ;
				/* Falls through. */
			default :
				usage_exit (argv [0], EXIT_FAILURE) ;
			} ;

	if (optind + 2 > argc)
		usage_exit (argv [0], EXIT_FAILURE) ;

	render.sndfilepath = argv [optind] ;
	render.pngfilepath = argv [optind + 1] ;

	if ((render.what & (RMS | PEAK)) == 0)
	{	printf ("Error: at least one of RMS or PEAK must be rendered\n") ;
		exit (EXIT_FAILURE) ;
		} ;

	check_int_range ("width", render.width, MIN_WIDTH, INT_MAX) ;
	check_int_range ("height", render.height, MIN_HEIGHT +
			((!render.geometry_no_border && render.border) ? (TOP_BORDER + BOTTOM_BORDER) : 0),
			INT_MAX) ;

	render.filename = strrchr (render.sndfilepath, '/') ;
	render.filename = (render.filename != NULL) ? render.filename + 1 : render.sndfilepath ;

	render_sndfile (&render) ;

	return 0 ;
} /* main */
// vim: ts=4 sw=4:
