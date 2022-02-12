AC_INIT([Dovecot FTS Xapian],[1.5.2],[jom@grosjo.net],[dovecot-fts-xapian])
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_MACRO_DIR([m4])

# Autoheader is not needed and does more harm than good for this package
# However, it is tightly integrated in autoconf/automake and therefore it is
# difficult not to use it. As a workaround we give autoheader a dummy config
# header to chew on and we handle the real config header ourselves.
AC_CONFIG_HEADERS([dummy-config.h fts-xapian-config.h])

AM_INIT_AUTOMAKE([no-define foreign tar-ustar])
AM_MAINTAINER_MODE
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
LT_INIT

PKG_PROG_PKG_CONFIG

DC_DOVECOT
DC_DOVECOT_MODULEDIR
LIBDOVECOT_INCLUDE="$LIBDOVECOT_INCLUDE"
CFLAGS="$CFLAGS $EXTRA_CFLAGS $DOVECOT_FLAGS -O2"
LIBS="$DOVECOT_LIBS"
BINARY_LDFLAGS="$PIE_LDFLAGS $RELRO_LDFLAGS"
BINARY_CFLAGS="$PIE_CFLAGS"
AC_SUBST(LIBDOVECOT_INCLUDE)

PKG_PROG_PKG_CONFIG() # check and set $PKG_CONFIG
PKG_CHECK_MODULES([ICU_UC], [icu-uc >= 50], [have_icu_uc=true], [have_icu_uc=false])
PKG_CHECK_MODULES([ICU_I18N], [icu-i18n >= 50], [have_icu_i18n=true], [have_icu_i18n=false])

if !($have_icu_uc && $have_icu_i18n); then
  AC_MSG_ERROR([icu 50 or higher is required, but was not found.])
fi

ICU_LDFLAGS=$($PKG_CONFIG --libs-only-L icu-uc | sed 's/-L//g')
ICU_LIBS=$($PKG_CONFIG --libs icu-uc icu-io icu-i18n)
ICU_CFLAGS=$($PKG_CONFIG --cflags-only-I icu-uc icu-io icu-i18n)
ICU_INCLUDE=$($PKG_CONFIG --variable=includedir icu-uc)

AC_MSG_CHECKING([for fts_mail_user_init])
 
AC_COMPILE_IFELSE(
 	[AC_LANG_PROGRAM([[
		#include "$dovecot_pkgincludedir/config.h"
		#include "$dovecot_pkgincludedir/lib.h"
		#include "$dovecot_pkgincludedir/mail-user.h"
		#include "$dovecot_pkgincludedir/fts-user.h"
		int f() 
		{
			void *ptr = &fts_mail_user_init;
			(void)ptr;
			return 0;
		}
	]])], 
	[
		AC_MSG_RESULT([yes])
	],[
		AC_MSG_RESULT([no])
		AC_MSG_ERROR([fts_mail_user_init not found 
			dovecot headers 
			are not installed correctly])
	])

AC_MSG_CHECKING([whether fts_mail_user_init has 3 args])

AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM([[
		#include "$dovecot_pkgincludedir/config.h"
		#include "$dovecot_pkgincludedir/lib.h"
		#include "$dovecot_pkgincludedir/mail-user.h"
		#include "$dovecot_pkgincludedir/fts-user.h"
                void f(struct mail_user *s, const char **error) {
                      fts_mail_user_init(s, 0, error);
                    }
                ]])], 
	[
		AC_MSG_RESULT([yes])
		THREE_ARGS="-DFTS_MAIL_USER_INIT_THREE_ARGS=1"
	],[
		AC_MSG_RESULT([no])
	])

AC_SUBST(ICU_CFLAGS)
AC_SUBST(ICU_LIBS)
AC_SUBST(ICU_INCLUDE)
AC_SUBST(ICU_LDFLAGS)
AC_SUBST(THREE_ARGS)

PKG_CHECK_MODULES([SQLITE3], [sqlite3], [have_sqlite3=true], [have_sqlite3=false]);
if !($have_sqlite3); then
  AC_MSG_ERROR([SQLite3 is required, but was not found.])
fi

AC_CHECK_PROG(XAPIAN_CONFIG,xapian-config,xapian-config,no)
AM_CONDITIONAL(HAVE_XAPIAN,test "x$XAPIAN_CONFIG" != "xno")
AS_IF([test "x$XAPIAN_CONFIG" = "xno"],[
   AC_MSG_ERROR([
   *** xapian could not be found; please install it
   *** e.g., in debian/ubuntu the package would be 'libxapian-dev'
   *** If you compiled it yourself, you should ensure that xapian-config
   *** is in your PATH.])],
   [XAPIAN_VERSION=$($XAPIAN_CONFIG --version | sed -e 's/.* //')])

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

