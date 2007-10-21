# SF_CHECK_DECL(SYMBOL,
#               [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND],
#               [INCLUDES = DEFAULT-INCLUDES])
# -------------------------------------------------------
# Check whether SYMBOL (a function, variable, or constant) is declared.

AC_DEFUN([SF_CHECK_DECL],
[AS_VAR_PUSHDEF([ac_Symbol], [ac_cv_have_decl_$1])dnl
AC_CACHE_CHECK([whether $1 is declared], [ac_Symbol],
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT([$4])],
[#ifndef $1
  (void) $1;
#endif
])],
		   [AS_VAR_SET([ac_Symbol], [yes])],
		   [AS_VAR_SET([ac_Symbol], [no])])])
AS_IF([test AS_VAR_GET([ac_Symbol]) = yes], [$2], [$3])[]dnl
AS_VAR_POPDEF([ac_Symbol])dnl
])# SF_CHECK_DECL
