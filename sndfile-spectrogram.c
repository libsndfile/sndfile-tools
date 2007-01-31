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

/*
**	Generate a spectrogram as a PNG file from a given sound file.
*/

/*
**	Todo:
**      - Decouple height of image from FFT length. FFT length should be
*         greater that height and then interpolated to height.
**      - Add border, scales.
**      - Make magnitude to colour mapper allow abitrary scaling (ie cmdline
**        arg).
**      - Better cmdline arg parsing and flexibility.
*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <cairo.h>
#include <fftw3.h>

#include <sndfile.h>

#include "window.h"

#define	MAX_WIDTH	4096
#define	MAX_HEIGHT	2048

#define ARRAY_LEN(x)		((int) (sizeof (x) / sizeof (x [0])))
#define MAX(x,y)			((x) > (y) ? (x) : (y))

static void
get_colour_map_value (float value, unsigned char colour [3])
{	static unsigned char map [][3] =
	{
		{	255, 255, 255 },  /* -0dB */
		{	240, 254, 216 },  /* -10dB */
		{	242, 251, 185 },  /* -20dB */
		{	253, 245, 143 },  /* -30dB */
		{	253, 200, 102 },  /* -40dB */
		{	252, 144,  66 },  /* -50dB */
		{	252,  75,  32 },  /* -60dB */
		{	237,  28,  41 },  /* -70dB */
		{	214,   3,  64 },  /* -80dB */
		{	183,   3, 101 },  /* -90dB */
		{	157,   3, 122 },  /* -100dB */
		{	122,   3, 126 },  /* -110dB */
		{	 80,   2, 110 },  /* -120dB */
		{	 45,   2,  89 },  /* -130dB */
		{	 19,   2,  70 },  /* -140dB */
		{	  1,   3,  53 },  /* -150dB */
		{	  1,   3,  37 },  /* -160dB */
		{	  1,   2,  19 },  /* -170dB */
		{	  0,   0,   0 },  /* -180dB */
	} ;

	float rem ;
	int indx ;

	if (value >= 0.0)
	{	colour = map [0] ;
		return ;
		} ;

	value = fabs (value * 0.1) ;

	indx = lrintf (floor (value)) ;

	if (indx < 0)
	{	printf ("\nError : colour map array index is %d\n\n", indx) ;
		exit (1) ;
		} ;

	if (indx >= ARRAY_LEN (map) - 1)
	{	colour = map [ARRAY_LEN (map) - 1] ;
		return ;
		} ;

	rem = fmod (value, 1.0) ;

	colour [0] = lrintf ((1.0 - rem) * map [indx][0] + rem * map [indx + 1][0]) ;
	colour [1] = lrintf ((1.0 - rem) * map [indx][1] + rem * map [indx + 1][1]) ;
	colour [2] = lrintf ((1.0 - rem) * map [indx][2] + rem * map [indx + 1][2]) ;

	return ;
} /* get_colour_map_value */


static void
read_audio_data (SNDFILE * infile, sf_count_t filelen, double * data, int datalen, int index, int total)
{
	sf_count_t start ;

	memset (data, 0, datalen * sizeof (data [0])) ;

	start = (index * filelen) / total - datalen / 2 ;

	if (start < 0)
	{	start = -start ;
		sf_seek (infile, 0, SEEK_SET) ;
		sf_read_double (infile, data + start, datalen - start) ;
		}
	else
	{	sf_seek (infile, start, SEEK_SET) ;
		sf_read_double (infile, data, datalen) ;
		} ;

	return ;
} /* read_audio_data */

static void
apply_window (double * data, int datalen)
{
	static double window [2 * MAX_HEIGHT] ;
	static int window_len = 0 ;
	int k ;

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

	return ;
} /* apply_window */

static double
calc_magnitude (const double * freq, int freqlen, float * magnitude)
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
render_spectrogram (float mag2d [MAX_WIDTH][MAX_HEIGHT], double maxval, int width, int height, cairo_surface_t * surface)
{
	unsigned char colour [3], *data ;
	int w, h, stride ;

	stride = cairo_image_surface_get_stride (surface) ;

	printf ("width %d    height %d    stride %d\n", width, height, stride) ;

	data = cairo_image_surface_get_data (surface) ;
	memset (data, 0, stride * height) ;

	for (w = 0 ; w < width ; w ++)
		for (h = 0 ; h < height ; h++)
		{	int hindex ;

			mag2d [w][h] = mag2d [w][h] / maxval ;
			mag2d [w][h] = (mag2d [w][h] < 1e-15) ? -200.0 : 20.0 * log10 (mag2d [w][h]) ;

			get_colour_map_value (mag2d [w][h], colour) ;

			hindex = height - 1 - h ;

			data [hindex * stride + w * 4 + 0] = colour [2] ;
			data [hindex * stride + w * 4 + 1] = colour [1] ;
			data [hindex * stride + w * 4 + 2] = colour [0] ;
			data [hindex * stride + w * 4 + 3] = 0 ;
			} ;

	return ;
} /* render_spectrogram */


static void
render_to_surface (SNDFILE *infile, sf_count_t filelen, cairo_surface_t * surface)
{
	static double time_domain [2 * MAX_HEIGHT] ;
	static double freq_domain [2 * MAX_HEIGHT] ;
	static float mag_spec [MAX_WIDTH][MAX_HEIGHT] ;

	fftw_plan plan ;
	int width, height, w ;
	double max_mag = 0.0 ;

	width = cairo_image_surface_get_width (surface) ;
	height = cairo_image_surface_get_height (surface) ;

	if (2 * height > ARRAY_LEN (time_domain))
	{	printf ("%s : 2 * height > ARRAY_LEN (time_domain)\n", __func__) ;
		exit (1) ;
		} ;

	plan = fftw_plan_r2r_1d (2 * height, time_domain, freq_domain, FFTW_R2HC, FFTW_MEASURE | FFTW_PRESERVE_INPUT) ;
	if (plan == NULL)
	{	printf ("%s : line %d : create plan failed.\n", __FILE__, __LINE__) ;
		exit (1) ;
		} ;

	for (w = 0 ; w < width ; w++)
	{	double temp ;

		read_audio_data (infile, filelen, time_domain, 2 * height, w, width) ;

		apply_window (time_domain, 2 * height) ;

		fftw_execute (plan) ;

		temp = calc_magnitude (freq_domain, 2 * height, mag_spec [w]) ;
		max_mag = MAX (temp, max_mag) ;
		} ;

	render_spectrogram (mag_spec, max_mag, width, height, surface) ;

	fftw_destroy_plan (plan) ;

	return ;
} /* render_to_surface */

static void
open_cairo_surface (SNDFILE *infile, sf_count_t filelen, int width, int height, const char * pngfilename)
{
	cairo_surface_t * surface = NULL ;
	cairo_status_t status ;

	/*
	**	CAIRO_FORMAT_RGB24 	 each pixel is a 32-bit quantity, with the upper 8 bits
	**	unused. Red, Green, and Blue are stored in the remaining 24 bits in that order.
	*/

	surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, width, height) ;
	if (surface == NULL || cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
	{	status = cairo_surface_status (surface) ;
		printf ("Error while creating surface : %s\n", cairo_status_to_string (status)) ;
		return ;
		} ;

	cairo_surface_flush (surface) ;

	render_to_surface (infile, filelen, surface) ;

	cairo_surface_mark_dirty (surface) ;

	status = cairo_surface_write_to_png (surface, pngfilename) ;
	if (status != CAIRO_STATUS_SUCCESS)
		printf ("Error while creating PNG file : %s\n", cairo_status_to_string (status)) ;

	cairo_surface_destroy (surface) ;

	return ;
} /* open_cairo_surface */

static void
open_sndfile (const char *sndfilename, int width, int height, const char * pngfilename)
{
	SNDFILE *infile ;
	SF_INFO info ;

	memset (&info, 0, sizeof (info)) ;

	infile = sf_open (sndfilename, SFM_READ, &info) ;
	if (infile == NULL)
	{	printf ("Error : failed to open file '%s' : \n%s\n", sndfilename, sf_strerror (NULL)) ;
		return ;
		} ;

	if (info.channels == 1)
		open_cairo_surface (infile, info.frames, width, height, pngfilename) ;
	else
		printf ("Error : sorry, can't render files with more than one channel.\n"
				"File '%s' has %d channels.\n\n", sndfilename, info.channels) ;

	sf_close (infile) ;

	return ;
} /* open_sndfile */

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

	printf ("\nUsage :\n\n    %s <sound file> <img width> <img height> <png name>\n\n", progname) ;

	puts (
		"    Create a spectrogram as a PNG file from a given sound file. The\n"
		"    spectrogram image will be of the given width and height.\n"
		) ;

	exit (0) ;
} /* usage_exit */

int
main (int argc, char * argv [])
{
	int width, height ;

	if (argc != 5)
		usage_exit (argv [0]) ;

	width = atoi (argv [2]) ;
	height = atoi (argv [3]) ;

	check_int_range ("width", width, 1, MAX_WIDTH) ;
	check_int_range ("height", height, 1, MAX_HEIGHT) ;

	open_sndfile (argv [1], width, height, argv [4]) ;

	return 0 ;
} /* main */
