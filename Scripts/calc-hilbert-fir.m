# For all the gory details, I suggest the paper:
#
#    Andrew Reilly and Gordon Frazer and Boualem Boashash:
#     Analytic signal generation---tips and traps,
#     IEEE Transactions on Signal Processing,
#     no. 11, vol. 42, Nov. 1994, pp. 3241-3245.
#
# For comp.dsp, the gist is:
#
# 1. Design a half-bandwidth real low-pass FIR filter using whatever optimal method
#    you choose, with the principle design criterion being minimization of the
#    maximum attenuation in the band f_s/4 to f_s/2.
#
# 2. Modulate this by exp(2 pi f_s/4 t), so that now your stop-band is the negative
#    frequencies, the pass-band is the positive frequencies, and the roll-off at
#    each end does not extend into the negative frequency band.
#
# 3. either use it as a complex FIR filter, or a pair of I/Q real filters in
#    whatever FIR implementation you have available.
#
# If your original filter design produced an impulse response with an even number
# of taps, then the filtering in 3 will introduce a spurious half-sample delay
# (resampling the real signal component), but that does not matter for many
# applications, and such filters have other features to recommend them.
#
# Andrew Reilly [Reilly@zeta.org.au]


# Don't make the filter a full half bandwidth filter.
# We need the bandwidth to extend well down towards DC, but we don't really
# need the bandwidht to extend all the way up to fs/2.
#
# Therefore build a filter that is slightly less than half band and modulate
# into place with a special factor.

nargin ;

if nargin >= 1,
	arg_list = argv () ;
	file = fopen (arg_list {1}, "w") ;
else
	file = stdout ;
	endif

b = remez (255, [0 0.356 0.4 1], [1, 1, 0, 0]) ;

len = length (b) ;

if rem (len , 2) !+ 0
	error ("Filter length should be even.") ;
	endif

half_len = len / 2 ;

# Normally, when using something like a half band filter, the factor would be
# 0.5. We choose something less, so the low frequency transition band
# straddles zero frquency.

factor = 0.386 ;
modulator = exp (factor * pi * i * linspace (-(len-1)/2, (len-1)/2, len)) ;

# plot (real (modulator)) ; pause ; exit (0) ;

bc = 2 * (b' .* modulator) ;

# plot (1:len, real (bc), 1:len, imag (bc)) ; pause ; exit (0) ;

magspec = abs (fft ([bc zeros(1,1000-length(b))])) ;

# plot (real (magspec)) ; pause ; exit (0) ;

bchalf = bc (1:half_len) ;

error = abs (bchalf - conj (fliplr (bc (half_len+1:len)))) ;

if error > 1e-10,
	# plot (error) ; pause ; exit (0) ;
	fprintf (stderr, "Error : %f\n", max (error)) ;
	exit (1) ;
	endif

fprintf (file, "#define FIR_HILBERT_HALF_LEN %d\n\n", half_len) ;

fprintf (file, "static complex_t half_hilbert_coeffs [FIR_HILBERT_HALF_LEN] =\n{") ;

for coeff = fliplr (bchalf)
	fprintf (file, "\t{\t%22.18f, %22.18f },\n", real (coeff), imag (coeff)) ;
	endfor

fprintf (file, "} ; \n") ;

if file != stdout,
	fclose (file) ;
	endif

# fprintf (stderr, "\nPress any key to exit.\n") ;
# plot (20 * log10 (magspec)) ; grid ; pause

