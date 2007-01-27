#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "kaiser_window.h"

static double iv (double v, double x) ;
static double hyperg (double a, double b, double x) ;
static double hy1f1p (double a, double b, double x, double * err) ;
static double hy1f1a (double a, double b, double x, double * err) ;
static double hyp2f0 (double a, double b, double x, int type, double * err) ;
static double lgam (double x) ;
static double p1evl (double x, double coef [], int N) ;
static double polevl (double x, double coef [], int N) ;

void
calc_kaiser_window (double * data, int datalen, double beta)
{
	/*
	**          besseli (0, beta * sqrt (1- (2*x/n).^2))
	** w (x) =  -------------------------------------,  -n/2 <= x <= n/2
	**                 besseli (0, beta)
	*/

	double two_k_on_n, denom ;
	int n, k ;

	denom = iv (0.0, beta) ;

	if (! isfinite (denom))
	{	printf ("denom : %f\nExiting\n", denom) ;
		exit (1) ;
		} ;

	for (k = 0 ; k < datalen ; k++)
	{	n = k - datalen / 2 ;
		two_k_on_n = (2.0 * k) / n ;
		data [k] = iv (0.0, beta * sqrt (1.0 - two_k_on_n * two_k_on_n)) / denom ;
		} ;

	return ;
} /* calc_kaiser_window */

/*							iv.c
 *
 *	Modified Bessel function of noninteger order
 *
 *
 *
 * SYNOPSIS:
 *
 * double v, x, y, iv () ;
 *
 * y = iv (v, x) ;
 *
 * DESCRIPTION:
 *
 * Returns modified Bessel function of order v of the
 * argument.  If x is negative, v must be integer valued.
 *
 * The function is defined as Iv (x) = Jv (ix).  It is
 * here computed in terms of the confluent hypergeometric
 * function, according to the formula
 *
 *              v  -x
 * Iv (x) = (x/2)  e   hyperg (v+0.5, 2v+1, 2x) / gamma (v+1)
 *
 * If v is a negative integer, then v is replaced by -v.
 *
 *
 * ACCURACY:
 *
 * Tested at random points (v, x), with v between 0 and
 * 30, x between 0 and 28.
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC       0,30          2000      3.1e-15     5.4e-16
 *    IEEE      0,30         10000      1.7e-14     2.7e-15
 *
 * Accuracy is diminished if v is near a negative integer.
 *
 * See also hyperg.c.
 *
 */

/*							iv.c	*/
/*	Modified Bessel function of noninteger order		*/
/* If x < 0, then v must be an integer. */


/*
Cephes Math Library Release 2.8:  June, 2000
Copyright 1984, 1987, 1988, 2000 by Stephen L. Moshier
*/

static double MAXNUM = 3e38 ;

/* Recalculated. */
static double machine_eps = 1.0 ;

static double
iv (double v, double x)
{
	int sign ;
	double t, ax ;

	/* If v is a negative integer, invoke symmetry */
	t = floor (v) ;
	if (v < 0.0)
	{	if (t == v)
		{	v = -v ;	/* symmetry */
			t = -t ;
			} ;
		} ;

	/* If x is negative, require v to be an integer */
	sign = 1 ;
	if (x < 0.0)
	{	if (t != v)
		{	puts ("iv : DOMAIN") ;
			return 0.0 ;
			} ;

		if (v != 2.0 * floor (v / 2.0))
			sign = -1 ;
		} ;

	/* Avoid logarithm singularity */
	if (x == 0.0)
	{	if (v == 0.0)
			return 1.0 ;
		if (v < 0.0)
		{	puts ("iv : OVERFLOW") ;
			return MAXNUM ;
			}
		else
			return 0.0 ;
		} ;

	ax = fabs (x) ;
	t = v * log (0.5 * ax) - x ;
	t = sign * exp (t) / gamma (v + 1.0) ;
	ax = v + 0.5 ;
	return t * hyperg (ax, 2.0 * ax, 2.0 * x) ;
} /* iv */


/*==============================================================================
*/

/*							hyperg.c
 *
 *	Confluent hypergeometric function
 *
 *
 *
 * SYNOPSIS:
 *
 * double a, b, x, y, hyperg () ;
 *
 * y = hyperg (a, b, x) ;
 *
 *
 *
 * DESCRIPTION:
 *
 * Computes the confluent hypergeometric function
 *
 *                          1           2
 *                       a x    a (a+1) x
 *   F ( a,b ;x)  =  1 + ---- + --------- + ...
 *  1 1                  b 1!   b (b+1) 2!
 *
 * Many higher transcendental functions are special cases of
 * this power series.
 *
 * As is evident from the formula, b must not be a negative
 * integer or zero unless a is an integer with 0 >= a > b.
 *
 * The routine attempts both a direct summation of the series
 * and an asymptotic expansion.  In each case error due to
 * roundoff, cancellation, and nonconvergence is estimated.
 * The result with smaller estimated error is returned.
 *
 *
 *
 * ACCURACY:
 *
 * Tested at random points (a, b, x), all three variables
 * ranging from 0 to 30.
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC       0,30         2000       1.2e-15     1.3e-16
 qtst1:
 21800   max =  1.4200E-14   rms =  1.0841E-15  ave = -5.3640E-17
 ltstd:
 25500   max = 1.2759e-14   rms = 3.7155e-16  ave = 1.5384e-18
 *    IEEE      0,30        30000       1.8e-14     1.1e-15
 *
 * Larger errors can be observed when b is near a negative
 * integer or zero.  Certain combinations of arguments yield
 * serious cancellation error in the power series summation
 * and also are not in the region of near convergence of the
 * asymptotic series.  An error message is printed if the
 * self-estimated relative error is greater than 1.0e-12.
 *
 */

/*							hyperg.c */


/*
Cephes Math Library Release 2.8:  June, 2000
Copyright 1984, 1987, 1988, 2000 by Stephen L. Moshier
*/

static double
hyperg (double a, double b, double x)
{
	double asum, psum, acanc, pcanc, temp ;

	/* See if a Kummer transformation will help */
	temp = b - a ;
	if (fabs (temp) < 0.001 * fabs (a))
		return exp (x) * hyperg (temp, b, -x) ;


	psum = hy1f1p (a, b, x, &pcanc) ;
	if (pcanc < 1.0e-15)
		goto done ;


	/* try asymptotic series */

	asum = hy1f1a (a, b, x, &acanc) ;


	/* Pick the result with less estimated error */

	if (acanc < pcanc)
		{
		pcanc = acanc ;
		psum = asum ;
		}

	done:
	if (pcanc > 1.0e-12)
		puts ("hyperg : PLOSS") ;

	return psum ;
}




/* Power series summation for confluent hypergeometric function		*/


static double hy1f1p (double a, double b, double x, double * err)
{
	double n, a0, sum, t, u, temp ;
	double an, bn, maxt, pcanc ;


	/* set up for power series summation */
	an = a ;
	bn = b ;
	a0 = 1.0 ;
	sum = 1.0 ;
	n = 1.0 ;
	t = 1.0 ;
	maxt = 0.0 ;


	while (t > machine_eps)
		{
		if (bn == 0)			/* check bn first since if both	*/
			{
			puts ("hyperg : SING") ;
			return MAXNUM ;	/* an and bn are zero it is	*/
			}
		if (an == 0)			/* a singularity		*/
			return sum ;
		if (n > 200)
			goto pdone ;
		u = x * ( an / (bn * n)) ;

		/* check for blowup */
		temp = fabs (u) ;
		if ((temp > 1.0) && (maxt > (MAXNUM/temp)))
			{
			pcanc = 1.0 ;	/* estimate 100% error */
			goto blowup ;
			}

		a0 *= u ;
		sum += a0 ;
		t = fabs (a0) ;
		if (t > maxt)
			maxt = t ;
	/*
		if ((maxt/fabs (sum)) > 1.0e17)
			{
			pcanc = 1.0 ;
			goto blowup ;
			}
	*/
		an += 1.0 ;
		bn += 1.0 ;
		n += 1.0 ;
		}

	pdone:

	/* estimate error due to roundoff and cancellation */
	if (sum != 0.0)
		maxt /= fabs (sum) ;
	maxt *= machine_eps ; 	/* this way avoids multiply overflow */
	pcanc = fabs (machine_eps * n + maxt) ;

	blowup:

	*err = pcanc ;

	return sum ;
}


/*							hy1f1a ()	*/
/* asymptotic formula for hypergeometric function:
 *
 *        (    -a
 *  --    ( |z|
 * |  (b) ( -------- 2f0 (a, 1+a-b, -1/x)
 *        (  --
 *        ( |  (b-a)
 *
 *
 *                                x    a-b                    )
 *                               e  |x|                       )
 *                             + -------- 2f0 (b-a, 1-a, 1/x))
 *                                --                          )
 *                               |  (a)                       )
 */

static double
hy1f1a (double a, double b, double x, double *err)
{
	double h1, h2, t, u, temp, acanc, asum, err1, err2 ;

	if (x == 0)
		{
		acanc = 1.0 ;
		asum = MAXNUM ;
		goto adone ;
		}
	temp = log (fabs (x)) ;
	t = x + temp * (a-b) ;
	u = -temp * a ;

	if (b > 0)
		{
		temp = lgam (b) ;
		t += temp ;
		u += temp ;
		}

	h1 = hyp2f0 (a, a-b+1, -1.0/x, 1, &err1) ;

	temp = exp (u) / gamma (b-a) ;
	h1 *= temp ;
	err1 *= temp ;

	h2 = hyp2f0 (b-a, 1.0-a, 1.0/x, 2, &err2) ;

	if (a < 0)
		temp = exp (t) / gamma (a) ;
	else
		temp = exp (t - lgam (a)) ;

	h2 *= temp ;
	err2 *= temp ;

	if (x < 0.0)
		asum = h1 ;
	else
		asum = h2 ;

	acanc = fabs (err1) + fabs (err2) ;

	if (b < 0)
		{
		temp = gamma (b) ;
		asum *= temp ;
		acanc *= fabs (temp) ;
		}


	if (asum != 0.0)
		acanc /= fabs (asum) ;

	acanc *= 30.0 ;	/* fudge factor, since error of asymptotic formula
			 * often seems this much larger than advertised */

adone:
	*err = acanc ;
	return asum ;
}

/*							hyp2f0 ()	*/

static double
hyp2f0 (double a, double b, double x, int type, double * err)
{
	double a0, alast, t, tlast, maxt ;
	double n, an, bn, u, sum, temp ;

	an = a ;
	bn = b ;
	a0 = 1.0e0 ;
	alast = 1.0e0 ;
	sum = 0.0 ;
	n = 1.0e0 ;
	t = 1.0e0 ;
	tlast = 1.0e9 ;
	maxt = 0.0 ;

	do
		{
		if (an == 0)
			goto pdone ;
		if (bn == 0)
			goto pdone ;

		u = an * (bn * x / n) ;

		/* check for blowup */
		temp = fabs (u) ;
		if ((temp > 1.0) && (maxt > (MAXNUM/temp)))
			goto error ;

		a0 *= u ;
		t = fabs (a0) ;

		/* terminating condition for asymptotic series */
		if (t > tlast)
			goto ndone ;

		tlast = t ;
		sum += alast ;	/* the sum is one term behind */
		alast = a0 ;

		if (n > 200)
			goto ndone ;

		an += 1.0e0 ;
		bn += 1.0e0 ;
		n += 1.0e0 ;
		if (t > maxt)
			maxt = t ;
		}
	while (t > machine_eps) ;


pdone:	/* series converged! */

	/* estimate error due to roundoff and cancellation */
	*err = fabs ( machine_eps * (n + maxt) ) ;

	alast = a0 ;
	goto done ;

ndone:	/* series did not converge */

	/* The following "Converging factors" are supposed to improve accuracy,
	 * but do not actually seem to accomplish very much. */

	n -= 1.0 ;
	x = 1.0/x ;

	switch (type)	/* "type" given as subroutine argument */
	{
	case 1:
		alast *= (0.5 + (0.125 + 0.25 * b - 0.5 * a + 0.25 * x - 0.25 * n) / x) ;
		break ;

	case 2:
		alast *= 2.0 / 3.0 - b + 2.0 * a + x - n ;
		break ;

	default:
		 ;
	}

	/* estimate error due to roundoff, cancellation, and nonconvergence */
	*err = machine_eps * (n + maxt) + fabs ( a0) ;


done:
	sum += alast ;
	return sum ;

/* series blew up: */
error:
	*err = MAXNUM ;
	puts ("hyperg : TLOSS") ;
	return sum ;
}

/*==============================================================================
*/
/*							gamma.c
 *
 *	Gamma function
 *
 *
 *
 * SYNOPSIS:
 *
 * double x, y, gamma () ;
 * extern int sgngam ;
 *
 * y = gamma (x) ;
 *
 *
 *
 * DESCRIPTION:
 *
 * Returns gamma function of the argument.  The result is
 * correctly signed, and the sign (+1 or -1) is also
 * returned in a global (extern) variable named sgngam.
 * This variable is also filled in by the logarithmic gamma
 * function lgam ().
 *
 * Arguments |x| <= 34 are reduced by recurrence and the function
 * approximated by a rational function of degree 6/7 in the
 * interval (2,3).  Large arguments are handled by Stirling's
 * formula. Large negative arguments are made positive using
 * a reflection formula.
 *
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC      -34, 34      10000       1.3e-16     2.5e-17
 *    IEEE    -170,-33      20000       2.3e-15     3.3e-16
 *    IEEE     -33,  33     20000       9.4e-16     2.2e-16
 *    IEEE      33, 171.6   20000       2.3e-15     3.2e-16
 *
 * Error for arguments outside the test range will be larger
 * owing to error amplification by the exponential function.
 *
 */
/*							lgam ()
 *
 *	Natural logarithm of gamma function
 *
 *
 *
 * SYNOPSIS:
 *
 * double x, y, lgam () ;
 * extern int sgngam ;
 *
 * y = lgam (x) ;
 *
 *
 *
 * DESCRIPTION:
 *
 * Returns the base e (2.718...) logarithm of the absolute
 * value of the gamma function of the argument.
 * The sign (+1 or -1) of the gamma function is returned in a
 * global (extern) variable named sgngam.
 *
 * For arguments greater than 13, the logarithm of the gamma
 * function is approximated by the logarithmic version of
 * Stirling's formula using a polynomial approximation of
 * degree 4. Arguments between -33 and +33 are reduced by
 * recurrence to the interval [2,3] of a rational approximation.
 * The cosecant reflection formula is employed for arguments
 * less than -33.
 *
 * Arguments greater than MAXLGM return MAXNUM and an error
 * message.  MAXLGM = 2.035093e36 for DEC
 * arithmetic or 2.556348e305 for IEEE arithmetic.
 *
 *
 *
 * ACCURACY:
 *
 *
 * arithmetic      domain        # trials     peak         rms
 *    DEC     0, 3                  7000     5.2e-17     1.3e-17
 *    DEC     2.718, 2.035e36       5000     3.9e-17     9.9e-18
 *    IEEE    0, 3                 28000     5.4e-16     1.1e-16
 *    IEEE    2.718, 2.556e305     40000     3.5e-16     8.3e-17
 * The error criterion was relative when the function magnitude
 * was greater than one but absolute when it was less than one.
 *
 * The following test used the relative error criterion, though
 * at certain points the relative error could be much higher than
 * indicated.
 *    IEEE    -200, -4             10000     4.8e-16     1.3e-16
 *
 */

/*							gamma.c	*/
/*	gamma function	*/

/*
Cephes Math Library Release 2.2:  July, 1992
Copyright 1984, 1987, 1989, 1992 by Stephen L. Moshier
Direct inquiries to 30 Frost Street, Cambridge, MA 02140
*/


/* log (sqrt (2*pi)) */
static double LS2PI = 0.91893853320467274178 ;

#define MAXLGM 2.556348e305

static const double LOGPI = 1.14472988584940017414 ;

static double A [] ={
	 8.11614167470508450300e-4,
	-5.95061904284301438324e-4,
	 7.93650340457716943945e-4,
	-2.77777777730099687205e-3,
	 8.33333333333331927722e-2
} ;

static double B [] = {
	-1.37825152569120859100e3,
	-3.88016315134637840924e4,
	-3.31612992738871184744e5,
	-1.16237097492762307383e6,
	-1.72173700820839662146e6,
	-8.53555664245765465627e5
} ;

static double C [] = {
	/* 1.00000000000000000000e0, */
	-3.51815701436523470549e2,
	-1.70642106651881159223e4,
	-2.20528590553854454839e5,
	-1.13933444367982507207e6,
	-2.53252307177582951285e6,
	-2.01889141433532773231e6
} ;

static double
lgam (double x)
{
	static int sgngam = 0 ;
	double p, q, u, w, z ;
	int i ;

	sgngam = 1 ;
	if (isnan (x))
		return x ;

	if (!isfinite (x))
	{	puts ("lgam : INF") ;
		return INFINITY ;
		} ;

	if (x < -34.0)
	{	q = -x ;
		w = lgam (q) ; /* note this modifies sgngam! */
		p = floor (q) ;
		if (p == q)
		{
	lgsing:
			puts ("lgam : SING") ;
			return INFINITY ;
			} ;
		i = p ;
		if ((i & 1) == 0)
			sgngam = -1 ;
		else
			sgngam = 1 ;
		z = q - p ;
		if (z > 0.5)
		{	p += 1.0 ;
			z = p - q ;
			} ;
		z = q * sin (M_PI * z) ;
		if (z == 0.0)
			goto lgsing ;
	/*	z = log (PI) - log (z) - w ;*/
		z = LOGPI - log (z) - w ;
		return z ;
		}

	if (x < 13.0)
	{	z = 1.0 ;
		p = 0.0 ;
		u = x ;
		while (u >= 3.0)
		{	p -= 1.0 ;
			u = x + p ;
			z *= u ;
			} ;
		while (u < 2.0)
		{	if (u == 0.0)
				goto lgsing ;
			z /= u ;
			p += 1.0 ;
			u = x + p ;
			} ;
		if (z < 0.0)
		{	sgngam = -1 ;
			z = -z ;
			}
		else
			sgngam = 1 ;
		if (u == 2.0)
			return log (z) ;
		p -= 2.0 ;
		x = x + p ;
		p = x * polevl (x, B, 5) / p1evl (x, C, 6) ;
		return log (z) + p ;
		} ;

	if (x > MAXLGM)
	{	puts ("lgam : INFINITY") ;
		return sgngam * INFINITY ;
		} ;

	q = ( x - 0.5) * log (x) - x + LS2PI ;
	if (x > 1.0e8)
		return q ;

	p = 1.0/ (x * x) ;
	if (x >= 1000.0)
		q += ((7.9365079365079365079365e-4 * p
			- 2.7777777777777777777778e-3) *p
			+ 0.0833333333333333333333) / x ;
	else
		q += polevl (p, A, 4) / x ;
	return q ;
}

/*==============================================================================
*/

/*							polevl.c
 *							p1evl.c
 *
 *	Evaluate polynomial
 *
 *
 *
 * SYNOPSIS:
 *
 * int N ;
 * double x, y, coef[N+1], polevl[] ;
 *
 * y = polevl (x, coef, N) ;
 *
 *
 *
 * DESCRIPTION:
 *
 * Evaluates polynomial of degree N:
 *
 *                     2          N
 * y  =  C  + C x + C x  +...+ C x
 *        0    1     2          N
 *
 * Coefficients are stored in reverse order:
 *
 * coef[0] = C  , ..., coef[N] = C  .
 *            N                   0
 *
 *  The function p1evl () assumes that coef[N] = 1.0 and is
 * omitted from the array.  Its calling arguments are
 * otherwise the same as polevl ().
 *
 *
 * SPEED:
 *
 * In the interest of speed, there are no checks for out
 * of bounds arithmetic.  This routine is used by most of
 * the functions in the library.  Depending on available
 * equipment features, the user may wish to rewrite the
 * program in microcode or assembly language.
 *
 */


/*
Cephes Math Library Release 2.1:  December, 1988
Copyright 1984, 1987, 1988 by Stephen L. Moshier
Direct inquiries to 30 Frost Street, Cambridge, MA 02140
*/


static double
polevl (double x, double coef [], int N)
{
	double ans ;
	int i ;
	double *p ;

	p = coef ;
	ans = *p++ ;
	i = N ;

	do
		ans = ans * x + *p++ ;
	while (--i) ;

	return ans ;
}

/*							p1evl ()	*/
/*                                          N
 * Evaluate polynomial when coefficient of x  is 1.0.
 * Otherwise same as polevl.
 */

static double
p1evl (double x, double coef [], int N)
{
	double ans ;
	double *p ;
	int i ;

	p = coef ;
	ans = x + *p++ ;
	i = N - 1 ;

	do
		ans = ans * x + *p++ ;
	while (--i) ;

	return ans ;
}

static void calculate_machine_eps (void) __attribute__ ((constructor)) ;

static void
calculate_machine_eps (void)
{
   while (1)
   {
      machine_eps *= 0.5 ;
      if ((1.0 + (machine_eps * 0.5)) == 1.0)
         break ;
   }

   printf ("\nCalculated Machine epsilon: %g\n", machine_eps) ;
}
