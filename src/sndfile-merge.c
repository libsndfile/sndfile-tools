/*
** Copyright (C) 2008 Jonatan Liljedahl <lijon@kymatica.com>
**
** This program is free software ; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation ; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY ; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program ; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sndfile.h>

static void
print_usage (void)
{	puts ("\nUsage : sndfile-merge <left input file> <right input file> <output file>\n") ;
	puts ("Merge two mono files to one stereo file\n") ;
} /* print_usage */

static void
do_merge (SNDFILE *infile [2], SNDFILE *outfile)
{	static double dataL [512], dataR [512], data [1024] ;
	int r1, r2, i, j ;

	while (1)
	{	r1 = sf_readf_double (infile [0], dataL, 512) ;
		r2 = sf_readf_double (infile [1], dataR, 512) ;

		if (r1 == 0 || r2 == 0)
			break ;

		if (r2 < r1)
			r1 = r2 ;
		for (i = 0, j = 0 ; i < r1 ; i++, j += 2)
		{	data [j] = dataL [i] ;
			data [j + 1] = dataR [i] ;
			} ;
		sf_writef_double (outfile, data, r1) ;
		} ;

	return ;
} /* do_merge */

int
main (int argc, char **argv)
{	SNDFILE *infile [2], *outfile ;
	SF_INFO sfinfo ;
	char *infilename [2], *outfilename ;
	int i ;

	if (argc!=4)
	{	print_usage () ;
		return 1 ;
		} ;

	infilename [0] = argv [1] ;
	infilename [1] = argv [2] ;
	outfilename = argv [3] ;

	for (i=0 ;i<2 ;i++)
	{	if ((infile [i] = sf_open (infilename [i], SFM_READ, &sfinfo)) == 0)
		{	printf ("Not able to open left input file '%s'\n%s\n", infilename [i], sf_strerror (NULL)) ;
			return 1 ;
			} ;

		if (sfinfo.channels != 1)
		{	printf ("Input files must be mono.\n") ;
			return 1 ;
			} ;
		} ;

	sfinfo.channels = 2 ;

	if ((outfile = sf_open (outfilename, SFM_WRITE, &sfinfo)) == 0)
	{	printf ("Not able to open output file '%s'\n%s\n", outfilename, sf_strerror (NULL)) ;
		return 1 ;
		} ;

	do_merge (infile, outfile) ;

	sf_close (infile [0]) ;
	sf_close (infile [1]) ;
	sf_close (outfile) ;

	return 0 ;
} /* main */

