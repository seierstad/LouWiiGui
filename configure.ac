AC_INIT([LouWiiGui], [0.1], [louwiigui@bluenoi.se])
AC_USE_SYSTEM_EXTENSIONS
AM_INIT_AUTOMAKE([1.11 foreign -Wall -Werror])
LT_INIT
AC_SUBST([LIBTOOL_DEPS])
AC_CONFIG_MACRO_DIRS([m4])
#AC_CONFIG_AUX_DIR([build-aux])

AC_PROG_CC
AC_PROG_LIBTOOL
AM_PROG_LIBTOOL

# needed to handle 64-bit architecture
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(size_t)


AC_CONFIG_SRCDIR([include/lou.h])
AC_CONFIG_FILES([Makefile src/Makefile])
AM_CONFIG_HEADER(config.h)
AC_CHECK_LIB([jack], [jack_client_open],, 
	AC_MSG_ERROR([jack library not found]))
AC_CHECK_LIB([cwiid], [cwiid_set_rpt_mode],, 
	AC_MSG_ERROR([cwiid library not found]))
AC_CHECK_LIB([xml2], [xmlNewTextWriterDoc],, 
	AC_MSG_ERROR([xml2 library not found]))
AC_CHECK_LIB([m], [sqrt],, 
	AC_MSG_ERROR([math library not found]))

XML_CFLAGS="`pkg-config libxml-2.0 --cflags`"
XML_LDFLAGS="`pkg-config libxml-2.0 --libs-only-L`"
XML_LIBS="`pkg-config libxml-2.0 --libs-only-l`"


AM_PATH_XML2(2.5.0,,AC_MSG_ERROR([
*** Libxml is required to build LouWiiGui; Available at
http://www.libxml.org/.]))

PKG_CHECK_MODULES(XMLLIB, libxml-2.0)
AC_SUBST(XMLLIB_CFLAGS)
AC_SUBST(XMLLIB_LIBS)

AC_HEADER_STDC
AC_CHECK_HEADER([stdlib.h],,
	AC_MSG_ERROR([stdlib.h not found]))
AC_CHECK_HEADER([signal.h],,
	AC_MSG_ERROR([signal.h not found]))
AC_CHECK_HEADER([unistd.h],,
	AC_MSG_ERROR([unistd.h not found]))
AC_CHECK_HEADER([string.h],,
	AC_MSG_ERROR([string.h not found]))
AC_CHECK_HEADER([time.h],,
	AC_MSG_ERROR([time.h not found]))
AC_CHECK_HEADER([math.h],,
	AC_MSG_ERROR([math.h not found]))
AC_CHECK_HEADER([jack/jack.h],,
	AC_MSG_ERROR([jack/jack.h not found]))
AC_CHECK_HEADER([jack/midiport.h],,
	AC_MSG_ERROR([jack/midiport.h not found]))
AC_CHECK_HEADER([bluetooth/bluetooth.h],,
	AC_MSG_ERROR([bluetooth/bluetooth.h not found]))
AC_CHECK_HEADER([cwiid.h],,
	AC_MSG_ERROR([cwiid.h not found]))
#AC_CHECK_HEADER([libxml/xmlversion.h],,
#	AC_MSG_ERROR([libxml/xmlversion.h not found]))
#AC_CHECK_HEADER([libxml/encoding.h],,
#	AC_MSG_ERROR([libxml2/libxml/encoding.h not found]))
#AC_CHECK_HEADER([libxml2/libxml/xmlwriter.h],,
#	AC_MSG_ERROR([libxml2/libxml/xmlwriter.h not found]))

AC_OUTPUT
