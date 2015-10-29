# Add the static library to configure.ac

AC_PREREQ([2.60])
AC_INIT([arcfile], [0.1], [rwogburn@gmail.com])
AC_CONFIG_SRCDIR([src/lib/readarc.c])
# AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE

# Checks for programs. These may set default variables, such as CFLAGS
AC_PROG_CPP
AC_PROG_CC_C99
AC_PROG_RANLIB

# Use the C language and compiler for the following checks
AC_LANG([C])

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdio.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# --------------------------------------------
# Check if we should build python extension
# --------------------------------------------
AC_PATH_PROG(PYTHON, python)
AM_CONDITIONAL(HAVE_PYTHON, test x"$PYTHON" != x)
AC_ARG_ENABLE(
[python],
[  --enable-python         Enable build of python extension (default: disabled)],
[case "${enableval}" in
  yes) python_ext=yes ;;
   no) python_ext=no ;;
    *) AC_MSG_ERROR([bad value ${enableval} for --enable-python-ext option]) ;;
esac],
python_ext=no)

if test x"$python_ext" = xyes; then
        if test x"$PYTHON" = x; then
                AC_MSG_ERROR([Can't build python extension, as python executable could not be found])
        fi
        CFLAGS="$CFLAGS -fPIC"
        # AC_PYTHON_DEVEL
fi

AM_CONDITIONAL(BUILD_PYTHON_EXTENSION, test x"$python_ext" = xyes)

# --------------------------------------------
# Check if we should build octave extension
# --------------------------------------------
AC_PATH_PROG(MKOCTFILE, mkoctfile)
AM_CONDITIONAL(HAVE_MKOCTFILE, test x"$MKOCTFILE" != x)
AC_ARG_ENABLE(
[octave],
[  --enable-octave         Enable build of octave extension (default: disabled)],
[case "${enableval}" in
  yes) octave_ext=yes ;;
   no) octave_ext=no ;;
    *) AC_MSG_ERROR([bad value ${enableval} for --enable-octave-ext option]) ;;
esac],
octave_ext=no)

if test x"$octave_ext" = xyes; then
        if test x"$MKOCTFILE" = x; then
                AC_MSG_ERROR([Can't build octave extension, as mkoctfile executable could not be found])
        fi

        # AC_OCTAVE_DEVEL
fi

AM_CONDITIONAL(BUILD_OCTAVE_EXTENSION, test x"$octave_ext" = xyes)

# --------------------------------------------
# Check if we should build matlab extension
# --------------------------------------------
AC_PATH_PROG(MATLABMEX, mex)
AM_CONDITIONAL(HAVE_MATLABMEX, test x"$MATLABMEX" != x)
AC_ARG_ENABLE(
[matlab],
[  --enable-matlab        Enable build of matlab extension (default: disabled)],
[case "${enableval}" in
  yes) mex_ext=yes ;;
   no) mex_ext=no ;;
    *) AC_MSG_ERROR([bad value ${enableval} for --enable-matlab-ext option]) ;;
esac],
mex_ext=no)

if test x"$mex_ext" = xyes; then
        if test x"$MATLABMEX" = x; then
                AC_MSG_ERROR([Can't build Matlab extension, as mex executable could not be found])
        fi

        # AC_MATLAB_DEVEL
fi

AM_CONDITIONAL(BUILD_MATLAB_EXTENSION, test x"$mex_ext" = xyes)


# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 src/lib/Makefile
                 src/bin/Makefile
                 oct_ext/Makefile
                 py_ext/Makefile
                 mex_ext/Makefile])
AC_OUTPUT

