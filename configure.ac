# Copyright (C) 2002-2017 Erik de Castro Lopo <erikd@mega-nerd.com>

dnl Require autoconf version >= 2.69)
AC_PREREQ([2.69])

AC_INIT([sndfile-tools],[1.04],[sndfile@mega-nerd.com],
		[sndfile-tools],[https://github.com/libsndfile/sndfile-tools/])

dnl Check whether we want to set defaults for CFLAGS, CPPFLAGS and LDFLAGS
AC_MSG_CHECKING([whether configure should try to set CFLAGS/CPPFLAGS/LDFLAGS])
AS_IF([test "x${CFLAGS+set}" = "xset" || test "x${CPPFLAGS+set}" = "xset" || test "x${LDFLAGS+set}" = "xset"], [
		enable_flags_setting=no
		: ${CFLAGS=""}
	], [
		enable_flags_setting=yes
		dnl Set to empty flags so AC_PROG_CC does not add -g -O2
		CFLAGS=""
	])
AC_MSG_RESULT([${enable_flags_setting}])

dnl Put config stuff in 'build-aux'.
AC_CONFIG_AUX_DIR([build-aux])

AC_CONFIG_SRCDIR([src/spectrogram.c])
AC_CANONICAL_HOST

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([src/config.h])

AM_INIT_AUTOMAKE([1.14 foreign dist-bzip2 no-dist-gzip subdir-objects])
AM_SILENT_RULES([yes])

dnl ====================================================================================

AC_PROG_CC
AC_PROG_CC_C99

AS_IF([test "x$ac_cv_prog_cc_c99" = "xno"], [
		AC_MSG_ERROR([sndfile-tools requires a C99 capable compiler!])
	])

AC_LANG([C])
AX_COMPILER_VENDOR
AX_COMPILER_VERSION

AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LN_S
LT_INIT

dnl ====================================================================================
dnl  Finished checking, handle options.

AC_ARG_ENABLE([werror],
	[AS_HELP_STRING([--enable-werror], [enable -Werror in all Makefiles])])

AC_ARG_ENABLE([jack],
	[AS_HELP_STRING([--disable-jack], [disable use of JACK (default=autodetect)])], [], [enable_jack=auto])

dnl ====================================================================================
dnl  Check for functions.

AC_SEARCH_LIBS([floor], [m], [], [
		AC_MSG_ERROR([unable to find the floor() function!])
	])
AC_CHECK_FUNCS([floor ceil fmod lrint lrintf])

dnl ====================================================================================
dnl  Check for libsndfile.

PKG_CHECK_MODULES([SNDFILE], [sndfile >= 1.0.19], [
		AC_DEFINE([HAVE_SNDFILE], [1], [Set to 1 if you have libsndfile])
	], [
		AC_MSG_ERROR([libsndfile could not be found!])
	])

dnl ====================================================================================
dnl  Check for libsamplerate.

PKG_CHECK_MODULES([SAMPLERATE], [samplerate >= 0.1.5], [
		AC_DEFINE([HAVE_SAMPLERATE], [1], [Set to 1 if you have libsamplerate])
	], [
		AC_MSG_ERROR([libsamplerate could not be found!])
	])

dnl ====================================================================================
dnl  Check for libfftw3 which is required for src/sndfile-spectrogram.c).

PKG_CHECK_MODULES([FFTW3], [fftw3 >= 0.15.0], [
		AC_DEFINE([HAVE_FFTW3], [1], [Set to 1 if you have FFTW])
	], [
		AC_MSG_ERROR([FFTW could not be found!])
	])

dnl ====================================================================================
dnl  Check for libcairo which is required for src/sndfile-spectrogram.c.

PKG_CHECK_MODULES([CAIRO], [cairo >= 1.4.0], [
		AC_DEFINE([HAVE_CAIRO], [1], [Set to 1 if you have Cairo])
	], [
		AC_MSG_ERROR([Cairo could not be found!])
	])

dnl ====================================================================================
dnl  Check for JACK which is required for src/sndfile-jackplay.c.

AS_IF([test "x$enable_jack" != "xno"], [
		PKG_CHECK_MODULES([JACK], [jack >= 0.100], [
				AC_DEFINE([HAVE_JACK], [1], [Set to 1 if you have JACK])
				enable_jack="yes"

				AX_PTHREAD
				JACK_CFLAGS="${JACK_CFLAGS} ${PTHREAD_CFLAGS}"
				JACK_LIBS="${JACK_LIBS} ${PTHREAD_LIBS}"
			], [
				AS_IF([test "x$enable_jack" = "xyes"], [
						dnl explicitly passed --enable-jack, hence error out loud and clearly
						AC_MSG_ERROR([You explicitly requested JACK support, but JACK could not be found!])
					], [
						dnl did not explicitly pass --enable-jack, relying on default automagic on
						enable_jack="no (auto)"
					])
			])
	])
AM_CONDITIONAL([HAVE_JACK], [test "x$enable_jack" = "xyes"])

dnl ====================================================================================
dnl Compiler stuff.

AS_IF([test "x$enable_flags_setting" = "xyes"], [
		AX_APPEND_COMPILE_FLAGS([-O2 -pipe], [CFLAGS])

		AS_CASE([${host_os}],
			[darwin*], [
				ldflags_test="-Wl,-dead_strip_dylibs"],
			[linux*], [
				ldflags_test="-Wl,-O1 -Wl,--as-needed -Wl,--no-undefined -Wl,--gc-sections"]
			)
		AX_APPEND_LINK_FLAGS([${ldflags_test}], [LDFLAGS])
	])

AS_IF([test "x$enable_werror" = "xyes"], [
		AX_APPEND_COMPILE_FLAGS([-Werror], [CFLAGS])
	])

AX_APPEND_COMPILE_FLAGS([-Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes -Waggregate-return -Wcast-align -Wcast-qual -Wnested-externs -Wshadow -Wpointer-arith], [CFLAGS])

dnl ====================================================================================
dnl  Now use the information from the checking stage.

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

dnl ====================================================================================

AX_RECURSIVE_EVAL([$bindir], [full_absolute_bindir])
AC_MSG_RESULT([
-=-=-=-=-=-=-=-=-=-= Configuration Complete =-=-=-=-=-=-=-=-=-=-=-

  Configuration summary :

    sndfile-tools version : ............... ${VERSION}

    Host CPU : ............................ ${host_cpu}
    Host Vendor : ......................... ${host_vendor}
    Host OS : ............................. ${host_os}

    CFLAGS : .............................. ${CFLAGS}
    CPPFLAGS : ............................ ${CPPFLAGS}
    LDFLAGS : ............................. ${LDFLAGS}

  Tools :

    C Compiler Vendor is : ................ ${ax_cv_c_compiler_vendor} (${ax_cv_c_compiler_version})

  Extra tools required for testing and examples :

    Found libjack ......................... ${enable_jack}

  Installation directories :

    Program directory : ................... ${full_absolute_bindir}

])
