AC_INIT([Dovecot FTS Xapian], [1.0], [jom@grosjo.net], [dovecot-fts-xapian])
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_MACRO_DIR([m4])

# Autoheader is not needed and does more harm than good for this package
# However, it is tightly integrated in autoconf/automake and therefore it is
# difficult not to use it. As a workaround we give autoheader a dummy config
# header to chew on and we handle the real config header ourselves.
AC_CONFIG_HEADERS([dummy-config.h fts-xapian-config.h])

AC_DEFINE_UNQUOTED(FTS_XAPIAN_NAME, "$PACKAGE_NAME",
        [Define to the full name of Dovecot FTS Xapian.])
AC_DEFINE_UNQUOTED(FTS_XAPIAN_VERSION, "$PACKAGE_VERSION",
        [Define to the version of Dovecot FTS Xapian.])

AM_INIT_AUTOMAKE([no-define foreign tar-ustar])
AM_MAINTAINER_MODE
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_LIBTOOL

PKG_PROG_PKG_CONFIG

DC_DOVECOT
DC_DOVECOT_MODULEDIR
LIBDOVECOT_INCLUDE="$LIBDOVECOT_INCLUDE"
CFLAGS="$CFLAGS $EXTRA_CFLAGS"
LIBS="$DOVECOT_LIBS"
BINARY_LDFLAGS="$PIE_LDFLAGS $RELRO_LDFLAGS"
BINARY_CFLAGS="$PIE_CFLAGS"
AC_SUBST(LIBDOVECOT_INCLUDE)

XO_LIB_XAPIAN
XO_REQUIRE([1.4.0], AC_MSG_ERROR([Xapian version 1.4.0 or better required]),)

XO_LIB_ICU
XO_ICU_REQUIRE([64], AC_MSG_ERROR([ICU version 64 or better required]),)

if test "$DOVECOT_INSTALLED" != "yes"; then
	DOVECOT_FTS_PLUGIN="$abs_dovecotdir/src/plugins/fts/lib20_fts_plugin.la"
else
	DOVECOT_FTS_PLUGIN="$dovecot_installed_moduledir/lib20_fts_plugin.la"
fi
AC_SUBST(DOVECOT_FTS_PLUGIN)

AC_CONFIG_FILES([
Makefile
src/Makefile
stamp.h])

AC_OUTPUT

