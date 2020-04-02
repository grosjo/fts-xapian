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

PKG_PROG_PKG_CONFIG([0.25]) # check and set $PKG_CONFIG
PKG_CHECK_MODULES([ICU_UC], [icu-uc >= 60], [have_icu_uc=true], [have_icu_uc=false])
PKG_CHECK_MODULES([ICU_I18N], [icu-i18n >= 60], [have_icu_i18n=true], [have_icu_i18n=false])
if !($have_icu_uc && $have_icu_i18n); then
  AC_MSG_ERROR([icu 60 or higher is required, but was not found.])
fi

ICU_LDFLAGS=$($PKG_CONFIG --libs-only-L icu-uc | sed 's/-L//g')
ICU_LIBS=$($PKG_CONFIG --libs icu-uc icu-io icu-i18n)
ICU_CFLAGS=$($PKG_CONFIG --cflags-only-I icu-uc icu-io icu-i18n)
ICU_INCLUDE=$($PKG_CONFIG --variable=includedir icu-uc)

AC_SUBST(ICU_CFLAGS)
AC_SUBST(ICU_LIBS)
AC_SUBST(ICU_INCLUDE)
AC_SUBST(ICU_LDFLAGS)

PKG_CHECK_MODULES([SQLITE3], [sqlite3 >= 3.0], , AC_MSG_ERROR([SQLite3 missing]))

SQL_LDFLAGS=$($PKG_CONFIG --libs-only-L sqlite3 | sed 's/-L//g')
SQL_LIBS=$($PKG_CONFIG --libs sqlite3)
SQL_CFLAGS=$($PKG_CONFIG --cflags-only-I sqlite3)
SQL_INCLUDE=$($PKG_CONFIG --variable=includedir sqlite3)

AC_SUBST(SQL_CFLAGS)
AC_SUBST(SQL_LIBS)
AC_SUBST(SQL_INCLUDE)
AC_SUBST(SQL_LDFLAGS)

# xapian checking - we need 1.4 at least
AC_CHECK_PROG(XAPIAN_CONFIG,xapian-config,xapian-config,no)
AM_CONDITIONAL(HAVE_XAPIAN,test "x$XAPIAN_CONFIG" != "xno")
AS_IF([test "x$XAPIAN_CONFIG" = "xno"],[
   AC_MSG_ERROR([
   *** xapian could not be found; please install it
   *** e.g., in debian/ubuntu the package would be 'libxapian-dev'
   *** If you compiled it yourself, you should ensure that xapian-config
   *** is in your PATH.])],
   [XAPIAN_VERSION=$($XAPIAN_CONFIG --version | sed -e 's/.* //')])

AS_CASE([$XAPIAN_VERSION],
    [1.[[4-9]].[[0-9]]*],
      [AC_DEFINE([XAPIAN_HAVE_OP_WILDCARD],[1],[Xapian::Query::OP_WILDCARD?])],
      [AC_MSG_ERROR([*** xapian version >= 1.4 needed, but version $XAPIAN_VERSION found.])])

XAPIAN_CXXFLAGS="$($XAPIAN_CONFIG --cxxflags)"
XAPIAN_LIBS="$($XAPIAN_CONFIG --libs)"

AC_SUBST(XAPIAN_CXXFLAGS)
AC_SUBST(XAPIAN_LIBS)

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

