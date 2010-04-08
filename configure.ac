AC_INIT(configure.ac)

AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE(serve, 0.7.1)

AC_ARG_WITH(libmagic,
[AS_HELP_STRING([--with-libmagic],[enable libmagic support])],
[AC_CHECK_HEADER(magic.h,
[AC_CHECK_LIB(magic,magic_open,,
[AC_MSG_ERROR([Could not find libmagic])],)],
[AC_MSG_ERROR([Could not find magic.h])])
AC_DEFINE(USE_LIBMAGIC,[1],
[Define if you want serve to use libmagic for auto-detecting MIME types])
AC_MSG_NOTICE([Using libmagic])],
[AC_MSG_NOTICE([Not using libmagic])])

AC_ARG_WITH(sendfile,
[AS_HELP_STRING([--with-sendfile],[use sendfile instead of read and write])],
[AC_CHECK_HEADER(sys/sendfile.h,,
[AC_MSG_ERROR([Could not find sys/sendfile.h])])
AC_DEFINE(USE_SENDFILE,[1],
[Define if you want serve to use the sendfile function - probably Linux-only])
AC_MSG_NOTICE([Using sendfile])],
[AC_MSG_NOTICE([Not using sendfile])])

AC_ARG_WITH(zlib,
[AS_HELP_STRING([--with-zlib],[use zlib for gzip compression where possible])],
[AC_CHECK_HEADER(zlib.h,
[AC_CHECK_LIB(z,zlibVersion,,[AC_MSG_ERROR([Could not find zlib])],)],
[AC_MSG_ERROR([Could not find zlib.h])])
AC_DEFINE(USE_GZIP,[1],
[Define if you want to use gzip compression])
AC_MSG_NOTICE([Using zlib])],
[AC_MSG_NOTICE([Not using zlib])])

AC_LANG_C
AC_PROG_CC
AM_PROG_LIBTOOL

AC_OUTPUT(Makefile src/Makefile misc/Makefile)
