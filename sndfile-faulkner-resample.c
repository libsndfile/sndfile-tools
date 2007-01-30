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
**	Faulkner Resampler. Uses SRC_SINC_BEST_QUAILTY to upsample to 4
**	times the destination ratem then use an average of 4 samples to
**	get to the final target sample rate.
**
**	http://stereophile.com/reference/104law/index.html
**	http://stereophile.com/reference/104law/index1.html
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <sndfile.h>
#include <samplerate.h>

#define	INPUT_LEN			(1 << 14)
#define ARRAY_LEN(x)		((int) ((sizeof (x)) / (sizeof (x [0]))))

static void usage_exit (void) ;

static sf_count_t sample_rate_convert (SNDFILE *infile, SNDFILE *outfile, double src_ratio, int channels, double * gain) ;
static double apply_gain (float * data, long frames, int channels, double max, double gain) ;

int
main (int argc, char ** argv)
{
	SNDFILE *infile, *outfile ;
	SF_INFO sfinfo ;
	sf_count_t count ;
	double src_ratio = -1.0, gain = 1.0 ;
	int new_sample_rate = -1 ;

	if (argc != 5)
		usage_exit () ;

	if (strcmp (argv [1], "-to") == 0)
		new_sample_rate = strtol (argv [2], NULL, 0) ;
	else if (strcmp (argv [1], "-by") == 0)
		src_ratio = strtod (argv [2], NULL) ;
	else
		usage_exit () ;

	if (new_sample_rate <= 0 && src_ratio <= 0.0)
		usage_exit () ;

	if (strcmp (argv [argc - 2], argv [argc - 1]) == 0)
	{	printf ("Error : input and output file names are the same.\n") ;
		exit (1) ;
		} ;


	printf ("Input File    : %s\n", argv [argc - 2]) ;
	printf ("Sample Rate   : %d\n", sfinfo.samplerate) ;
	printf ("Input Frames  : %ld\n\n", (long) sfinfo.frames) ;

	if (new_sample_rate > 0)
	{	src_ratio = (1.0 * new_sample_rate) / sfinfo.samplerate ;
		sfinfo.samplerate = new_sample_rate ;
		}
	else if (src_is_valid_ratio (src_ratio))
		sfinfo.samplerate = (int) floor (sfinfo.samplerate * src_ratio) ;
	else
	{	printf ("Not able to determine new sample rate. Exiting.\n") ;
		sf_close (infile) ;
		exit (1) ;
		} ;

	if (fabs (src_ratio - 1.0) < 1e-20)
	{	printf ("Target samplerate and input samplerate are the same. Exiting.\n") ;
		sf_close (infile) ;
		exit (0) ;
		} ;

	printf ("SRC Ratio     : %f\n", src_ratio) ;

	/* Delete the output file length to zero if already exists. */
	remove (argv [argc - 1]) ;

	if ((outfile = sf_open (argv [argc - 1], SFM_WRITE, &sfinfo)) == NULL)
	{	printf ("Error : Not able to open output file '%s'\n", argv [argc - 1]) ;
		sf_close (infile) ;
		exit (1) ;
		} ;

	/* Update the file header after every write. */
	sf_command (outfile, SFC_SET_UPDATE_HEADER_AUTO, NULL, SF_TRUE) ;
	sf_command (outfile, SFC_SET_CLIPPING, NULL, SF_TRUE) ;

	printf ("Output file   : %s\n", argv [argc - 1]) ;
	printf ("Sample Rate   : %d\n", sfinfo.samplerate) ;

	do
		count = sample_rate_convert (infile, outfile, src_ratio, sfinfo.channels, &gain) ;
	while (count < 0) ;

	printf ("Output Frames : %ld\n\n", (long) count) ;

	sf_close (infile) ;
	sf_close (outfile) ;

	return 0 ;
} /* main */

static sf_count_t
sample_rate_convert (SNDFILE *infile, SNDFILE *outfile, double src_ratio, int channels, double * gain)
{	static float input [INPUT_LEN] ;
	static float output [5 * INPUT_LEN] ;

	SRC_STATE	*src_state ;
	SRC_DATA	src_data ;
	int			error ;
	double		max = 0.0 ;
	sf_count_t	output_count = 0 ;

	sf_seek (infile, 0, SEEK_SET) ;
	sf_seek (outfile, 0, SEEK_SET) ;

	/* Initialize the sample rate converter. */
	if ((src_state = src_new (SRC_SINC_BEST_QUALITY, channels, &error)) == NULL)
	{	printf ("\n\nError : src_new() failed : %s.\n\n", src_strerror (error)) ;
		exit (1) ;
		} ;

	src_data.end_of_input = 0 ; /* Set this later. */

	/* Start with zero to force load in while loop. */
	src_data.input_frames = 0 ;
	src_data.data_in = input ;

	src_data.src_ratio = src_ratio ;

	src_data.data_out = output ;
	src_data.output_frames = ARRAY_LEN (output) /channels ;

	while (1)
	{
		/* If the input buffer is empty, refill it. */
		if (src_data.input_frames == 0)
		{	src_data.input_frames = sf_readf_float (infile, input, INPUT_LEN / channels) ;
			src_data.data_in = input ;

			/* The last read will not be a full buffer, so snd_of_input. */
			if (src_data.input_frames < INPUT_LEN / channels)
				src_data.end_of_input = SF_TRUE ;
			} ;

		if ((error = src_process (src_state, &src_data)))
		{	printf ("\nError : %s\n", src_strerror (error)) ;
			exit (1) ;
			} ;

		/* Terminate if done. */
		if (src_data.end_of_input && src_data.output_frames_gen == 0)
			break ;

		max = apply_gain (src_data.data_out, src_data.output_frames_gen, channels, max, *gain) ;

		/* Write output. */
		sf_writef_float (outfile, output, src_data.output_frames_gen) ;
		output_count += src_data.output_frames_gen ;

		src_data.data_in += src_data.input_frames_used * channels ;
		src_data.input_frames -= src_data.input_frames_used ;
		} ;

	src_state = src_delete (src_state) ;

	if (max > 1.0)
	{	*gain = 1.0 / max ;
		printf ("\nOutput has clipped. Restarting conversion to prevent clipping.\n\n") ;
		output_count = 0 ;
		sf_command (outfile, SFC_FILE_TRUNCATE, &output_count, sizeof (output_count)) ;
		return -1 ;
		} ;

	return output_count ;
} /* sample_rate_convert */

static double
apply_gain (float * data, long frames, int channels, double max, double gain)
{
	long k ;

	for (k = 0 ; k < frames * channels ; k++)
	{	data [k] *= gain ;

		if (fabs (data [k]) > max)
			max = fabs (data [k]) ;
		} ;

	return max ;
} /* apply_gain */

static void
usage_exit (void)
{
	puts ("\n"
		"Usage :\n\n"
		"    sndfile-faulkner-resample -to <new sample rate> <input file> <output file>\n"
		"    sndfile-faulkner-resample -by <amount> <input file> <output file>\n"
		) ;
	exit (0) ;
} /* usage_exit */

