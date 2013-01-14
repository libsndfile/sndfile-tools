/*
** Copyright (C) 2012 Erik de Castro Lopo <erikd@mega-nerd.com>
** Copyright (C) 2013 driedfruit <driedfruit@mindloop.net>
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
#include <unistd.h>
#include <math.h>
#include <sys/wait.h>

#include "common.h"

static void parse_int_test (void) ;

int
main (void)
{
	parse_int_test () ;
	return 0 ;
} /* main */

/*===============================================================================
*/

static void
fork_parse_int (const char * str, int value, int should_parse)
{	pid_t pid ;
	int status = 0 ;

	if ((pid = fork ()) < 0)
	{	printf ("Error : fork() failed.\n") ;
		exit (1) ;
		} ;

	if (pid == 0)
	{	int parsed ;

		freopen ("/dev/null", "w", stderr) ;

		parsed = parse_int_or_die (str, "test") ;
		if (should_parse && parsed != value)
		{	printf ("Error : Parse of '%s' resulted in %d, not %d\n", str, parsed, value) ;
			exit (1) ;
			} ;
		exit (0) ;
		} ;

	if (waitpid (pid, &status, 0) != pid)
	{	printf ("Error : waitpid() failed.\n") ;
		exit (1) ;
		} ;

	if (should_parse && status != 0)
		exit (1) ;

	return ;
} /* fork_parse_int */

static void
parse_int_test (void)
{
	printf ("%-37s : ", __func__) ;
	fflush (stdout) ;
	fork_parse_int ("1234", 1234, SF_TRUE) ;
	fork_parse_int ("+1234", +1234, SF_TRUE) ;
	fork_parse_int ("-1234", -1234, SF_TRUE) ;
	fork_parse_int ("10000000000000000000000000000000000000000000000000", 0, SF_FALSE) ;
	fork_parse_int ("die", 0, SF_FALSE) ;
	puts ("ok") ;
} /* parse_int_test */
