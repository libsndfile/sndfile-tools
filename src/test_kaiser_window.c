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

#include <stdio.h>

#include "window.h"

#define ARRAY_LEN(x)	((int) ((sizeof (x)) / (sizeof (x [0]))))

int
main (void)
{
	double window [20] ;
	int k ;

	calc_kaiser_window (window, ARRAY_LEN (window), 1.0) ;

	for (k = 0 ; k < ARRAY_LEN (window) ; k++)
		printf (" % f\n", window [k]) ;

	return 0 ;
} /* main */
