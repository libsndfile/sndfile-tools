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
#include <math.h>

#include "window.h"

#define ARRAY_LEN(x)	((int) ((sizeof (x)) / (sizeof (x [0]))))

int
main (void)
{
	double window [2000] ;
	int k ;

	calc_kaiser_window (window, ARRAY_LEN (window), 1.0) ;

	for (k = 0 ; k < ARRAY_LEN (window) ; k++)
	{	if (window [k] > 1.0)
		{	printf ("\nError (%s %d) : window [%d] > 1.0.\n", __func__, __LINE__, k) ;
			exit (1) ;
			} ;

		if (window [k] < 0.0)
		{	printf ("\nError (%s %d) : window [%d] < 0.0.\n", __func__, __LINE__, k) ;
			exit (1) ;
			} ;
		} ;

	if (fabs (window [0] - window [ARRAY_LEN (window) - 1]) > 1e-20)
	{	printf ("\nError (%s %d) : fabs (%f - %f) > 1e-20)\n", __func__, __LINE__, window [0], window [ARRAY_LEN (window) - 1]) ;
		exit (1) ;
		} ;

	calc_kaiser_window (window, ARRAY_LEN (window) - 1, 1.0) ;

	if (fabs (window [0] - window [ARRAY_LEN (window) - 2]) > 1e-20)
	{	printf ("\nError (%s %d) : fabs (%f - %f) > 1e-20)\n", __func__, __LINE__, window [0], window [ARRAY_LEN (window) - 1]) ;
		exit (1) ;
		} ;

	puts ("----------------------\n        Passed\n----------------------") ;

	return 0 ;
} /* main */
