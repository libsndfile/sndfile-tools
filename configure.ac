# Copyright (C) 2002-2004 Erik de Castro Lopo (erikd AT mega-nerd DOT com).

dnl Require autoconf version >= 2.54)
AC_PREREQ(2.54)

AC_INIT([sndfile-tools],[1.03],[erikd@mega-nerd.com])

# Put config stuff in Cfg.
AC_CONFIG_AUX_DIR(Cfg)

AC_CONFIG_SRCDIR([src/sndfile-spectrogram.c])
AC_CANONICAL_TARGET([])

AM_INIT_AUTOMAKE($PACKAGE_NAME,$PACKAGE_VERSION)
AM_CONFIG_HEADER(src/config.h)

dnl Add parameters for aclocal
AC_SUBST(ACLOCAL_AMFLAGS, "-I M4")

AC_LANG([C])

#------------------------------------------------------------------------------------

AC_PROG_CC
AM_PROG_CC_C_O

# Do not check for F77.
define([AC_LIBTOOL_LANG_F77_CONFIG], [:])dnl

AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LN_S
AC_PROG_LIBTOOL
AM_PROG_LIBTOOL

AC_HEADER_STDC

#====================================================================================
# Finished checking, handle options.

AC_ARG_ENABLE(gcc-werror,
	AC_HELP_STRING([--enable-gcc-werror], [enable -Werror in all Makefiles]),
	ac_arg_gcc_werror=yes, ac_arg_gcc_werror=no)

AC_ARG_ENABLE(gcc-pipe,
		AC_HELP_STRING([--disable-gcc-pipe], [disable gcc -pipe option ]),
		ac_arg_gcc_pipe="N", ac_arg_gcc_pipe="Y")

AC_ARG_ENABLE(gcc-opt,
		AC_HELP_STRING([--disable-gcc-opt], [disable gcc optimisations ]),
		ac_arg_gcc_opt="N", ac_arg_gcc_opt="Y")

AC_ARG_ENABLE(fftw,
		AC_HELP_STRING([--disable-fftw], [disable usage of FFTW ]),
		ac_arg_fftw="N", ac_arg_fftw="Y")

AC_ARG_ENABLE(jack,
	AC_HELP_STRING([--disable-jack], [disable use of JACK]))

AC_ARG_ENABLE(debug,
		AC_HELP_STRING([--enable-debug], [enable debugging ]),
		ac_arg_debug=yes, ac_arg_debug=no)

AC_ARG_ENABLE(cpu-clip,
		AC_HELP_STRING([--disable-cpu-clip], [disable tricky cpu specific clipper]),
		ac_arg_cpu_clip="N", ac_arg_cpu_clip="Y")

if test x$ac_arg_debug = xyes ; then
	AC_DEFINE([ENABLE_DEBUG],1,[Set to 1 to enable debugging.])
else
	AC_DEFINE([ENABLE_DEBUG],0,[Set to 1 to enable debugging.])
	fi

#====================================================================================
# Check for functions.

AC_CHECK_LIB([m],floor)
AC_CHECK_FUNCS(floor ceil fmod)

MN_C99_FUNC_LRINT
MN_C99_FUNC_LRINTF
# AC_C99_FUNC_LLRINT  Don't need this (yet?).

case "x$ac_cv_c99_lrint$ac_cv_c99_lrintf" in
	xyesyes)
		;;
	*)
		AC_MSG_WARN([[*** Missing C99 standard functions lrint() and lrintf().]])
		AC_MSG_WARN([[*** This may cause benign compiler warnings on some systems (ie Solaris).]])
		;;
	esac

#====================================================================================
# Check for libsndfile.

PKG_CHECK_MODULES(SNDFILE, sndfile >= 1.0.19, ac_cv_sndfile=1, ac_cv_sndfile=0)

AC_DEFINE_UNQUOTED([HAVE_SNDFILE],$ac_cv_sndfile,[Set to 1 if you have libsndfile.])

if test x$ac_cv_sndfile = x1 ; then
	ac_cv_sndfile=yes
	HAVE_SNDFILE=1
else
	ac_cv_sndfile=no
	HAVE_SNDFILE=0
	fi

#====================================================================================
# Check for libfftw3 which is required for src/sndfile-spectrogram.c).

if test $ac_arg_fftw = "Y" ; then
	PKG_CHECK_MODULES(FFTW3, fftw3 >= 0.15.0, ac_cv_fftw3=1, ac_cv_fftw3=0)
	AC_DEFINE_UNQUOTED([HAVE_FFTW3],$ac_cv_fftw3,[Set to 1 if you have libfftw3.])

	if test x$ac_cv_fftw3 = x1 ; then
		ac_cv_fftw3=yes
		HAVE_FFTW3=1
		fi
	fi

if test x$ac_cv_fftw3 != xyes ; then
	ac_cv_fftw3=no
	HAVE_FFTW3=0
	fi

#====================================================================================
# Check for libcairo which is required for src/sndfile-spectrogram.c.

PKG_CHECK_MODULES(CAIRO, cairo >= 1.4.0, ac_cv_cairo=1, ac_cv_cairo=0)

AC_DEFINE_UNQUOTED([HAVE_CAIRO],$ac_cv_cairo,[Set to 1 if you have libcairo.])

if test x$ac_cv_cairo = x1 ; then
	ac_cv_cairo=yes
	HAVE_CAIRO=1
else
	ac_cv_cairo=no
	HAVE_CAIRO=0
	fi

#====================================================================================
# Check for JACK which is required for src/sndfile-jackplay.c.

ac_cv_jack=no
if test x$enable_jack != xno ; then
	PKG_CHECK_MOD_VERSION(JACK, jack >= 0.100, ac_cv_jack=yes, ac_cv_jack=no)
	fi


if test x$ac_cv_jack = "xyes" ; then
	HAVE_JACK=1
else
	HAVE_JACK=0
	fi

AC_DEFINE_UNQUOTED([HAVE_JACK],$HAVE_JACK,[Set to 1 if you have JACK.])

#====================================================================================
# GCC stuff.

if test $ac_cv_c_compiler_gnu = yes ; then
	CFLAGS="$CFLAGS -std=gnu99 -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes -Waggregate-return -Wcast-align -Wcast-qual -Wnested-externs -Wshadow -Wpointer-arith"
	# -Wundef -Wbad-function-cast -Wmissing-declarations -Wconversion -Winline"

	if test x$ac_arg_gcc_werror = "xyes" ; then
		CFLAGS="-Werror $CFLAGS"
		fi

	AC_DEFINE([COMPILER_IS_GCC],1, [Set to 1 if the compile is GNU GCC.])
	GCC_MAJOR_VERSION=`$CC -dumpversion | sed "s/\..*//"`
	AC_DEFINE_UNQUOTED([GCC_MAJOR_VERSION],${GCC_MAJOR_VERSION}, [Major version of GCC or 3 otherwise.])
	fi

#====================================================================================
# Now use the information from the checking stage.

AC_SUBST(COMPILER_IS_GCC)
AC_SUBST(GCC_MAJOR_VERSION)
AC_SUBST(ENABLE_DEBUG)

AC_SUBST(HAVE_SNDFILE)
AC_SUBST(SNDFILE_CFLAGS)
AC_SUBST(SNDFILE_LIBS)

AC_SUBST(HAVE_FFTW3)
AC_SUBST(FFTW3_CFLAGS)
AC_SUBST(FFTW3_LIBS)

AC_SUBST(HAVE_CAIRO)
AC_SUBST(CAIRO_CFLAGS)
AC_SUBST(CAIRO_LIBS)

AC_SUBST(HAVE_JACK)
AC_SUBST(JACK_CFLAGS)
AC_SUBST(JACK_LIBS)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

#====================================================================================

AC_MSG_RESULT([
-=-=-=-=-=-=-=-=-=-= Configuration Complete =-=-=-=-=-=-=-=-=-=-=-

  Configuration summary :

    Version ....................... ${VERSION}])
AC_MSG_RESULT([    Enable debugging .............. ${ac_arg_debug}
])


if test x$ac_cv_c_compiler_gnu = xyes ; then
	echo -e "  Tools :\n"
	echo "    Compiler is GCC ............... ${ac_cv_c_compiler_gnu}"
	echo "    GCC major version ............. ${GCC_MAJOR_VERSION}"
	fi

AC_MSG_RESULT([
  Extra tools required for testing and examples :

    Found SNDFILE ................. ${ac_cv_sndfile}
    Found FFTW .................... ${ac_cv_fftw3}
    Found CAIRO ................... ${ac_cv_cairo}
    Found JACK .................... ${ac_cv_jack}
])

if test "x$ac_cv_sndfile$ac_cv_fftw3$ac_cv_cairo$ac_cv_jack" != "xyesyesyesyes" ; then
	AC_MSG_RESULT([**** Error : You will need to install the missing libaries. ****])
	echo
	exit 1
	fi

AC_MSG_RESULT([  Installation directories :

    Program directory ............. ${prefix}/bin
])


