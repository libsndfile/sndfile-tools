/*
** Copyright (C) 2007-2015 Erik de Castro Lopo <erikd@mega-nerd.com>
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
#include <assert.h>
#include <math.h>

#include "window.h"

#define ARRAY_LEN(x)		((int) (sizeof (x) / sizeof (x [0])))

void
calc_hanning_window (double * data, int datalen)
{
	int k ;

	/*
	**	Hanning window function from :
	**
	**	http://en.wikipedia.org/wiki/Window_function
	*/

	for (k = 0 ; k < datalen ; k++)
		data [k] = 0.5 * (1.0 - cos(2.0 * M_PI * k / (datalen - 1)));

	return ;
} /* calc_hanning_window */
