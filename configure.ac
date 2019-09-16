#                                               -*- Autoconf -*-
# Process this file with ./autogen.sh to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([socketcand], [0.6.1], [https://github.com/linux-can/socketcand/issues])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_SRCDIR([state_bcm.c])

CFLAGS="${CFLAGS} -Wall -Wno-parentheses -DPF_CAN=29 -DAF_CAN=PF_CAN"

# Enable debug mode option
AC_ARG_ENABLE(debug, [  --enable-debug Enable debug mode], AC_DEFINE(DEBUG, 1, [Debug mode]))

# init scripts. default is only to create an init.d script
AC_SUBST(init_script, "yes")
AC_SUBST(rc_script, "no")
AC_SUBST(sysroot, $sysroot)

AC_ARG_ENABLE(rc_script, [  --enable-rc-script Enable creation of rc.d start script], 
              AC_SUBST(rc_script, "yes"))

AC_ARG_ENABLE(init_script, [  --disable-init-script Enable creation of init.d start script], 
              AC_SUBST(init_script, "no"), AC_SUBST(init_script, "yes"))

# Checks for libconfig.
AC_ARG_WITH([config], [AS_HELP_STRING([--with-config], [use libconfig])], [], [with_config=no])

LIBCONFIG=
          AS_IF([test "x$with_config" != xno],
            [AC_CHECK_LIB([config], [config_init],
              [], [AC_MSG_FAILURE(
                 [config test failed (--without-config to disable)])],
              [-lconfig])])

# Checks for programs.
AC_PROG_CC

# Checks for pthread.
AC_CHECK_LIB([pthread], [pthread_create], [], AC_MSG_ERROR([libpthread not installed]))

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h syslog.h unistd.h pthread.h], [], AC_MSG_ERROR([not all required headers are present]))

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([gethostname gettimeofday inet_ntoa memset select socket strerror])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([debian_pack], [chmod +x debian_pack])
AC_CONFIG_FILES([init.d/socketcand])
AC_OUTPUT
