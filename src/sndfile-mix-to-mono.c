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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "common.h"

static void usage_exit (void) ;
static void mix_to_mono (SNDFILE * infile, SNDFILE * outfile) ;

int
main (int argc, char ** argv)
{
	SNDFILE *infile, *outfile ;
	SF_INFO sfinfo ;

	if (argc != 3)
		usage_exit () ;

	if (strcmp (argv [argc - 2], argv [argc - 1]) == 0)
	{	printf ("Error : input and output file names are the same.\n") ;
		exit (1) ;
		} ;

	/* Delete the output file length to zero if already exists. */
	remove (argv [argc - 1]) ;

	memset (&sfinfo, 0, sizeof (sfinfo)) ;
	if ((infile = sf_open (argv [argc - 2], SFM_READ, &sfinfo)) == NULL)
	{	printf ("Error : Not able to open input file '%s'\n", argv [argc - 2]) ;
		sf_close (infile) ;
		exit (1) ;
		} ;

	if (sfinfo.channels == 1)
	{	printf ("Input file '%s' already mono. Exiting.\n", argv [argc - 2]) ;
		sf_close (infile) ;
		exit (0) ;
		} ;

	/* Force output channels to mono. */
	sfinfo.channels = 1 ;

	if ((outfile = sf_open (argv [argc - 1], SFM_WRITE, &sfinfo)) == NULL)
	{	printf ("Error : Not able to open output file '%s'\n", argv [argc - 1]) ;
		sf_close (infile) ;
		exit (1) ;
		} ;

	mix_to_mono (infile, outfile) ;

	sf_close (infile) ;
	sf_close (outfile) ;

	return 0 ;
} /* main */

static void
mix_to_mono (SNDFILE * infile, SNDFILE * outfile)
{	double buffer [1024] ;
	sf_count_t count ;
	
	while ((count = sfx_mix_mono_read_double (infile, buffer, ARRAY_LEN (buffer))) > 0)
		sf_write_double (outfile, buffer, count) ;

	return ;
} /* mix_to_mono */

static void
usage_exit (void)
{
	puts ("\n"
		"Usage :\n\n"
		"    sndfile-mix-to-mono <input file> <output file>\n"
		) ;
	exit (0) ;
} /* usage_exit */
