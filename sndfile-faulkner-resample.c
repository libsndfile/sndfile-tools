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

#include <sndfile.h>
#include <samplerate.h>

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

int
main (int argc, char ** argv)
{
	double src_ratio = -1.0 ;
	int new_sample_rate = -1 ;

	if (argc != 5)
		usage_exit () ;

	if (strcmp (argv [1], "-to") == 0)
		new_sample_rate = strtol (argv [2], NULL, 0) ;
	else if (strcmp (argv [1], "-by") == 0)
		src_ratio = strtod (argv [2], NULL) ;
	else
		usage_exit () ;

	return 0 ;
} /* main */
