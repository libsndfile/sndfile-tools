/*
** Copyright (c) 2002-2020, Erik de Castro Lopo <erikd@mega-nerd.com>
** All rights reserved.
**
** This code is released under 2-clause BSD license. Please see the
** file at : https://github.com/erikd/libsamplerate/blob/master/COPYING
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include "common.h"

#include <samplerate.h>
#include <sndfile.h>

#define DEFAULT_CONVERTER SRC_SINC_MEDIUM_QUALITY

#define	BUFFER_LEN		4096	/*-(1<<16)-*/

static void usage_exit (const char *progname) ;
static sf_count_t sample_rate_convert (SNDFILE *infile, SNDFILE *outfile, int converter, double src_ratio, int channels, double * gain, int normalize, sf_count_t nframes) ;
static double apply_gain (float * data, long frames, int channels, double max, double gain) ;

int
main (int argc, char *argv [])
{	SNDFILE	*infile, *outfile = NULL ;
	SF_INFO sfinfo = { } ;
	sf_count_t nframes ;

	int normalize = 1 ;
	sf_count_t	count ;
	double		src_ratio = -1.0, gain = 1.0 ;
	int			new_sample_rate = -1, k, converter, max_speed = SF_FALSE, raw_bits ;
	char		raw_type ;

	if (argc == 2 && strcmp (argv [1], "--version") == 0)
	{	char buffer [64], *cptr ;

		if ((cptr = strrchr (argv [0], '/')) != NULL)
			argv [0] = cptr + 1 ;
		if ((cptr = strrchr (argv [0], '\\')) != NULL)
			argv [0] = cptr + 1 ;

		sf_command (NULL, SFC_GET_LIB_VERSION, buffer, sizeof (buffer)) ;

		printf ("%s (%s,%s)\n", argv [0], src_get_version (), buffer) ;
		exit (0) ;
		} ;

	if (argc < 5 || argc > 10)
		usage_exit (argv [0]) ;

	/* Set default converter. */
	converter = DEFAULT_CONVERTER ;

	for (k = 1 ; k < argc - 2 ; k++)
	{	if (strcmp (argv [k], "--max-speed") == 0)
			max_speed = SF_TRUE ;
		else if (strcmp (argv [k], "--no-normalize") == 0)
			normalize = 0 ;
		else if (strcmp (argv [k], "-to") == 0)
		{	k ++ ;
			new_sample_rate = parse_int_or_die (argv [k], "sample rate") ;
			}
		else if (strcmp (argv [k], "-by") == 0)
		{	k ++ ;
			src_ratio = atof (argv [k]) ;
			}
		else if (strcmp (argv [k], "-c") == 0)
		{	k ++ ;
			converter = parse_int_or_die (argv [k], "converter") ;
			}
		else if (strcmp (argv [k], "-r") == 0)
		{	k ++ ;
			if (sscanf (argv [k], "%d,%d,%c,%d", &sfinfo.samplerate, &sfinfo.channels, &raw_type, &raw_bits) != 4)
			{	printf ("Error : unable to decode raw audio parameters.\n") ;
				usage_exit (argv [0]) ;
				}
			sfinfo.format = SF_FORMAT_RAW ;
			if (raw_type == 'i' && (raw_bits == 8 || raw_bits == 16 || raw_bits == 24 || raw_bits == 32))
			{	switch (raw_bits)
				{	case 8:
						sfinfo.format |= SF_FORMAT_PCM_S8 ;
						break ;

					case 16:
						sfinfo.format |= SF_FORMAT_PCM_16 ;
						break ;

					case 24:
						sfinfo.format |= SF_FORMAT_PCM_24 ;
						break ;

					case 32:
						sfinfo.format |= SF_FORMAT_PCM_32 ;
						break ;

					default:
						printf ("Error : PCM bits per sample should be 8, 16, 24 or 32.\n") ;
						usage_exit (argv [0]) ;
					}
				}
			else if (raw_type == 'f' && (raw_bits == 32 || raw_bits == 64))
			{	switch (raw_bits)
				{	case 32:
						sfinfo.format |= SF_FORMAT_FLOAT ;
						break ;

					case 64:
						sfinfo.format |= SF_FORMAT_DOUBLE ;
						break ;

					default:
						printf ("Error : Floating point bits per sample should be 32 or 64.\n") ;
						usage_exit (argv [0]) ;
					}
				}
			else
			{	printf ("Error : invalid raw audio parameters.\n") ;
				usage_exit (argv [0]) ;
				}
			}
		else
			usage_exit (argv [0]) ;
		} ;

	if (new_sample_rate <= 0 && src_ratio <= 0.0)
		usage_exit (argv [0]) ;

	if (src_get_name (converter) == NULL)
	{	printf ("Error : bad converter number.\n") ;
		usage_exit (argv [0]) ;
		} ;

	if (strcmp (argv [argc - 2], argv [argc - 1]) == 0)
	{	printf ("Error : input and output file names are the same.\n") ;
		exit (1) ;
		} ;

	if ((infile = sf_open (argv [argc - 2], SFM_READ, &sfinfo)) == NULL)
	{	printf ("Error : Not able to open input file '%s'\n", argv [argc - 2]) ;
		exit (1) ;
		} ;

	printf ("Input File    : %s\n", argv [argc - 2]) ;
	printf ("Sample Rate   : %d\n", sfinfo.samplerate) ;
	printf ("Input Frames  : %ld\n\n", (long) sfinfo.frames) ;

	nframes = sfinfo.frames ;

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
	printf ("Converter     : %s\n\n", src_get_name (converter)) ;

	if (src_is_valid_ratio (src_ratio) == 0)
	{	printf ("Error : Sample rate change out of valid range.\n") ;
		sf_close (infile) ;
		exit (1) ;
		} ;

	/* Delete the output file length to zero if already exists. */
	remove (argv [argc - 1]) ;

	printf ("Output File   : %s\n", argv [argc - 1]) ;
	printf ("Sample Rate   : %d\n", sfinfo.samplerate) ;

	do
	{	sf_close (outfile) ;

		if ((outfile = sf_open (argv [argc - 1], SFM_WRITE, &sfinfo)) == NULL)
		{	printf ("Error : Not able to open output file '%s'\n", argv [argc - 1]) ;
			sf_close (infile) ;
			exit (1) ;
			} ;

		if (max_speed)
		{	/* This is mainly for the comparison program tests/src-evaluate.c */
			sf_command (outfile, SFC_SET_ADD_PEAK_CHUNK, NULL, SF_FALSE) ;
			}
		else
		{	/* Update the file header after every write. */
			sf_command (outfile, SFC_SET_UPDATE_HEADER_AUTO, NULL, SF_TRUE) ;
			} ;

		sf_command (outfile, SFC_SET_CLIPPING, NULL, SF_TRUE) ;

		count = sample_rate_convert (infile, outfile, converter, src_ratio, sfinfo.channels, &gain, normalize, nframes) ;
		}
	while (count < 0) ;

	printf ("Output Frames : %ld\n\n", (long) count) ;

	sf_close (infile) ;
	sf_close (outfile) ;

	return 0 ;
} /* main */

/*==============================================================================
*/

static sf_count_t
sample_rate_convert (SNDFILE *infile, SNDFILE *outfile, int converter, double src_ratio, int channels, double * gain, int normalize, sf_count_t nframes)
{	static float input [BUFFER_LEN] ;
	static float output [BUFFER_LEN] ;

	SRC_STATE	*src_state ;
	SRC_DATA	src_data ;
	int			error ;
	double		max = 0.0 ;
	sf_count_t	output_count = 0 ;

	char		anim [4] = "-\\|/" ;
	short		p_anim = 0 ;

	sf_seek (infile, 0, SEEK_SET) ;
	sf_seek (outfile, 0, SEEK_SET) ;

	/* Initialize the sample rate converter. */
	if ((src_state = src_new (converter, channels, &error)) == NULL)
	{	printf ("\n\nError : src_new() failed : %s.\n\n", src_strerror (error)) ;
		exit (1) ;
		} ;

	src_data.end_of_input = 0 ; /* Set this later. */

	/* Start with zero to force load in while loop. */
	src_data.input_frames = 0 ;
	src_data.data_in = input ;

	src_data.src_ratio = src_ratio ;

	src_data.data_out = output ;
	src_data.output_frames = BUFFER_LEN /channels ;

	while (1)
	{
		/* If the input buffer is empty, refill it. */
		if (src_data.input_frames == 0)
		{	src_data.input_frames = sf_readf_float (infile, input, BUFFER_LEN / channels) ;
			src_data.data_in = input ;

			/* The last read will not be a full buffer, so snd_of_input. */
			if (src_data.input_frames < BUFFER_LEN / channels)
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
		nframes -= src_data.input_frames_used ;
		printf (" %c remaining  : %19li\r", anim [p_anim], nframes) ;
		p_anim = (p_anim + 1) % 4 ;
		} ;
	printf ("\n") ;

	src_delete (src_state) ;

	if (normalize && max > 1.0)
	{	*gain = 1.0 / max ;
		printf ("\nOutput has clipped. Restarting conversion to prevent clipping.\n\n") ;
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
usage_exit (const char *progname)
{	char lsf_ver [128] ;
	const char	*cptr ;
	int		k ;

	if ((cptr = strrchr (progname, '/')) != NULL)
		progname = cptr + 1 ;

	if ((cptr = strrchr (progname, '\\')) != NULL)
		progname = cptr + 1 ;


	sf_command (NULL, SFC_GET_LIB_VERSION, lsf_ver, sizeof (lsf_ver)) ;

	printf ("\n"
		"  A Sample Rate Converter using libsndfile for file I/O and Secret \n"
		"  Rabbit Code (aka libsamplerate) for performing the conversion.\n"
		"  It works on any file format supported by libsndfile with any \n"
		"  number of channels (limited only by host memory).\n"
		"\n"
		"       %s\n"
		"       %s\n"
		"\n"
		"  Usage : \n"
		"       %s -to <new sample rate> [-c <number>] <input file> <output file>\n"
		"       %s -by <amount> [-c <number>] <input file> <output file>\n"
		"\n", src_get_version (), lsf_ver, progname, progname) ;

	puts (
		"  The optional -c argument allows the converter type to be chosen from\n"
		"  the following list :"
		"\n"
		) ;

	for (k = 0 ; (cptr = src_get_name (k)) != NULL ; k++)
		printf ("       %d : %s%s\n", k, cptr, k == DEFAULT_CONVERTER ? " (default)" : "") ;

	puts ("\n"
		"  The --no-normalize option disables clipping check and normalization.") ;

	puts ("\n"
		"  Sound parameters for raw input may be specified using option -r RRRR,C,s,BB\n"
		"  where RRRR is the sample rate, C is the channels number, s is 'i' (as integer)\n"
		"  or 'f' (as float) and BB the bits per sample (8, 16, ...)\n"
		"  Note: when raw audio input is used, then a raw output audio file will be\n"
		"  created as well.") ;

	puts ("") ;

	exit (1) ;
} /* usage_exit */
