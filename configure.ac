## Initialization
AC_PREREQ(2.59)
AC_INIT([poloka-psf], 0.1.0, [poloka team])
AC_CONFIG_SRCDIR([poloka/imagepsf.cc])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR(autoconf)
AM_INIT_AUTOMAKE([foreign])

## Keep this one before libtool check
AM_DISABLE_STATIC

## Checks for programs to build poloka-core library
AC_PROG_CXX
AC_PROG_LIBTOOL

## Check for mandatory poloka-core
PKG_CHECK_MODULES([POLOKA_CORE],
		  [poloka-core],,
		  [AC_MSG_ERROR([Cound not find the poloka-core library])])

AC_CONFIG_FILES([Makefile
		 poloka-psf.pc
		 datacards/Makefile
                 doc/Doxyfile
                 doc/Makefile
                 poloka/Makefile
		 tools/Makefile
                 ])
AC_OUTPUT
