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

/* Generate a sound file containing a chirp */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <sndfile.h>

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

	printf ("\nUsage :\n\n    %s <sample rate> <length in seconds> <sound file>\n\n", progname) ;

	puts (
		"    Create containing a logarithmic chirp.\n"
		) ;

	exit (0) ;
} /* usage_exit */

int
main (int argc, char * argv [])
{
	int samplerate, seconds ;

	if (argc != 4)
		usage_exit (argv [0]) ;

	samplerate = atoi (argv [2]) ;
	seconds = atoi (argv [3]) ;

	check_int_range ("sample rate", samplerate, 1000, 200 * 1000) ;
	check_int_range ("seconds", seconds, 1, 1000) ;

	return 0 ;
} /* main */
