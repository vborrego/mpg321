# mpg321 - a fully free clone of mpg123.
# Copyright (C) 2001 Joe Drew
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Library General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.50)
AC_INIT
AC_CONFIG_SRCDIR([mad.c])
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE(mpg321, 0.2.10)

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S

# Checks for libraries.
AC_CHECK_LIB(mad,mad_decoder_run,,AC_MSG_ERROR(libmad is required to compile mpg321. See http://www.mars.org/home/rob/proj/mpeg/))
AC_CHECK_LIB(id3tag,id3_tag_new,,AC_MSG_ERROR("libid3tag is required to compile mpg321. Find it in the MAD distribution at http://www.mars.org/home/rob/proj/mpeg/ - version 0.14.1 or better."), -lz)

LIBS="$LIBS -lz -lm"

XIPH_PATH_AO(,AC_MSG_ERROR(libao needed!))

dnl Check for LFS
AC_SYS_LARGEFILE

CFLAGS="$CFLAGS $AO_CFLAGS"
LIBS="$LIBS $AO_LIBS"

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h errno.h fcntl.h limits.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h sys/ioctl.h sys/time.h unistd.h)
AC_HEADER_TIME

# Checks for typedefs, structures, and compiler characteristics.
AC_C_BIGENDIAN
AC_C_CONST
AC_C_INLINE

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_STAT
AC_FUNC_MMAP
AC_TYPE_SIGNAL

# From lsh-1.3.4:
# Some systems (in particular, Unixware) doesn't have socklen_t, but
# uses size_t. Falling back to int will cause some warnings.

# AC_CHECK_TYPE doesn't work, probably because the type in
# question is not defined in sys/types.h.

AC_TYPE_SOCKLEN_T

AC_CHECK_FUNCS([gethostbyname memset munmap socket strchr strdup strerror strrchr strstr gettimeofday select getenv putenv setenv unsetenv strcasecmp])

AC_ARG_ENABLE(mpg123_symlink,
[  --enable-mpg123-symlink Enable symlink of mpg123 to mpg321 [[default=yes]] ],
,enable_mpg123_symlink=yes)
AM_CONDITIONAL(DO_SYMLINK, test x$enable_mpg123_symlink = xyes)
    
# borrowed from a mail from Russ Allbery - from INN
dnl The rat's nest of networking libraries.  The common cases are not to
dnl need any extra libraries, or to need -lsocket -lnsl.  We need to avoid
dnl linking with libnsl unless we need it, though, since on some OSes where
dnl it isn't necessary it will totally break networking.  Unisys also
dnl includes gethostbyname in libsocket but needs libnsl for socket().
AC_SEARCH_LIBS(gethostbyname, nsl)
AC_CHECK_FUNC(socket, , [
    AC_CHECK_LIB(socket, socket, LIBS="$LIBS -lsocket", [
        AC_CHECK_LIB(nsl, socket, LIBS="$LIBS -lsocket -lnsl", , -lsocket)
    ], "$LIBS")
])

AC_ARG_WITH(default_audio, 
[  --with-default-audio=DA Specify default libao output plugin to use 
                            [[oss,sun,alsa(09),esd,arts]] ])
                            
if test "x$with_default_audio" != "x" -a "x$with_default_audio" != "xyes"; then
    AC_DEFINE_UNQUOTED(AUDIO_DEFAULT, "$with_default_audio",[Define the default libao output device.])
else
# yes, literally 'the libao default', since this is used in output strings
    AC_DEFINE_UNQUOTED(AUDIO_DEFAULT, "the libao default", [Define the default libao output device.])
fi

AC_CONFIG_FILES([Makefile m4/Makefile])
AC_OUTPUT
