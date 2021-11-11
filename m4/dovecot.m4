dnl dovecot.m4 - Check presence of dovecot -*-Autoconf-*-
dnl
dnl   Copyright (C) 2010 Dennis Schridded
dnl
dnl This file is free software; the authors give
dnl unlimited permission to copy and/or distribute it, with or without
dnl modifications, as long as this notice is preserved.

# serial 28

dnl
dnl Check for support for D_FORTIFY_SOURCE=2
dnl

AC_DEFUN([AC_CC_D_FORTIFY_SOURCE],[
    AC_REQUIRE([gl_UNKNOWN_WARNINGS_ARE_ERRORS])
    AS_IF([test "$enable_hardening" = yes], [
      case "$host" in
        *)
          gl_COMPILER_OPTION_IF([-O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2], [
            CFLAGS="$CFLAGS -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2"
            ],
            [],
            [AC_LANG_PROGRAM()]
          )
      esac
    ])
])

dnl * gcc specific options
AC_DEFUN([DC_DOVECOT_CFLAGS],[
  AS_IF([test "x$ac_cv_c_compiler_gnu" = "xyes"], [
        dnl Use gcc support for C99, available since 4.5.0 [2010-04-14]
        CFLAGS="$CFLAGS -std=gnu99"
  ])
  AS_IF([test "$have_clang" = "yes"], [
    dnl clang specific options
    AS_IF([test "$want_devel_checks" = "yes"], [
      dnl FIXME: enable once md[45], sha[12] can be compiled without
      dnl CFLAGS="$CFLAGS -fsanitize=integer,undefined -ftrapv"
      :
    ])
  ])
])

AC_DEFUN([AC_LD_WHOLE_ARCHIVE], [
    LD_WHOLE_ARCHIVE=
    LD_NO_WHOLE_ARCHIVE=
    AC_MSG_CHECKING([for linker option to include whole archive])
    ld_help="`$CC -Wl,-help 2>&1`"
    case "$ld_help" in
        *"--whole-archive"*)
            LD_WHOLE_ARCHIVE="--whole-archive"
            LD_NO_WHOLE_ARCHIVE="--no-whole-archive"
        ;;
    esac
    AS_IF([test "x$LD_WHOLE_ARCHIVE" != "x"],
      [AC_MSG_RESULT([-Wl,$LD_WHOLE_ARCHIVE])],
      [AC_MSG_RESULT([not supported])]
    )
    AC_SUBST([LD_WHOLE_ARCHIVE])
    AC_SUBST([LD_NO_WHOLE_ARCHIVE])
    AM_CONDITIONAL([HAVE_WHOLE_ARCHIVE], [test "x$LD_WHOLE_ARCHIVE" != "x"])
])

dnl
dnl Check for -z now and -z relro linker flags
dnl
dnl Copyright (C) 2013 Red Hat, Inc.
dnl
dnl This library is free software; you can redistribute it and/or
dnl modify it under the terms of the GNU Lesser General Public
dnl License as published by the Free Software Foundation; either
dnl version 2.1 of the License, or (at your option) any later version.
dnl
dnl This library is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl Lesser General Public License for more details.
dnl
dnl You should have received a copy of the GNU Lesser General Public
dnl License along with this library.  If not, see
dnl <http://www.gnu.org/licenses/>.
dnl

AC_DEFUN([AC_LD_RELRO],[
    RELRO_LDFLAGS=
    AS_IF([test "$enable_hardening" = yes], [
      AC_MSG_CHECKING([for how to force completely read-only GOT table])
      ld_help=`$CC -Wl,-help 2>&1`
      case $ld_help in
          *"-z relro"*) RELRO_LDFLAGS="-Wl,-z -Wl,relro" ;;
      esac
      case $ld_help in
          *"-z now"*) RELRO_LDFLAGS="$RELRO_LDFLAGS -Wl,-z -Wl,now" ;;
      esac
      AS_IF([test "x$RELRO_LDFLAGS" != "x"],
        [AC_MSG_RESULT([$RELRO_LDFLAGS])],
        [AC_MSG_RESULT([unknown])]
      )
   ])
   AC_SUBST([RELRO_LDFLAGS])
])

dnl
dnl Check for support for position independent executables
dnl
dnl Copyright (C) 2013 Red Hat, Inc.
dnl
dnl This library is free software; you can redistribute it and/or
dnl modify it under the terms of the GNU Lesser General Public
dnl License as published by the Free Software Foundation; either
dnl version 2.1 of the License, or (at your option) any later version.
dnl
dnl This library is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl Lesser General Public License for more details.
dnl
dnl You should have received a copy of the GNU Lesser General Public
dnl License along with this library.  If not, see
dnl <http://www.gnu.org/licenses/>.
dnl

AC_DEFUN([AC_CC_PIE],[
    AC_REQUIRE([gl_UNKNOWN_WARNINGS_ARE_ERRORS])
    PIE_CFLAGS=
    PIE_LDFLAGS=

    AS_IF([test "$enable_hardening" = yes], [
      OLD_CFLAGS=$CFLAGS
      case "$host" in
        *-*-mingw* | *-*-msvc* | *-*-cygwin* )
           ;; dnl All code is position independent on Win32 target
        *)
          CFLAGS="-fPIE -DPIE"
          gl_COMPILER_OPTION_IF([-pie], [
            PIE_CFLAGS="-fPIE -DPIE"
            PIE_LDFLAGS="-pie"
            ], [
              dnl some versions of clang require -Wl,-pie instead of -pie
              gl_COMPILER_OPTION_IF([[-Wl,-pie]], [
                PIE_CFLAGS="-fPIE -DPIE"
                PIE_LDFLAGS="-Wl,-pie"
                ], [AC_MSG_RESULT([not supported])],
                [AC_LANG_PROGRAM()]
              )
            ],
            [AC_LANG_PROGRAM()]
          )
      esac
      CFLAGS=$OLD_CFLAGS
    ])
    AC_SUBST([PIE_CFLAGS])
    AC_SUBST([PIE_LDFLAGS])
])

dnl
dnl Check for support for Retpoline
dnl

AC_DEFUN([AC_CC_RETPOLINE],[
    AC_REQUIRE([gl_UNKNOWN_WARNINGS_ARE_ERRORS])
])

dnl
dnl Check for support for -fstack-protector or -strong
dnl

AC_DEFUN([AC_CC_F_STACK_PROTECTOR],[
    AC_REQUIRE([gl_UNKNOWN_WARNINGS_ARE_ERRORS])
    AS_IF([test "$enable_hardening" = yes], [
      case "$host" in
        *)
          gl_COMPILER_OPTION_IF([-fstack-protector-strong], [
            CFLAGS="$CFLAGS -fstack-protector-strong"
            ],
            [
               gl_COMPILER_OPTION_IF([-fstack-protector], [
                 CFLAGS="$CFLAGS -fstack-protector"
                 ], [], [AC_LANG_PROGRAM()])
            ],
            [AC_LANG_PROGRAM()]
          )
      esac
    ])
])

AC_DEFUN([DC_DOVECOT_MODULEDIR],[
	AC_ARG_WITH(moduledir,
	[  --with-moduledir=DIR    Base directory for dynamically loadable modules],
		[moduledir="$withval"] ,
		[moduledir="$dovecot_moduledir"]
	)
	AC_SUBST(moduledir)
])

AC_DEFUN([DC_PLUGIN_DEPS],[
	_plugin_deps=yes
	AC_MSG_CHECKING([whether OS supports plugin dependencies])
	case "$host_os" in
	  darwin*)
	    dnl OSX loads the plugins twice, which breaks stuff
	    _plugin_deps=no
	    ;;
	esac
	AC_MSG_RESULT([$_plugin_deps])
	AM_CONDITIONAL([DOVECOT_PLUGIN_DEPS], [test "x$_plugin_deps" = "xyes"])
	unset _plugin_deps
])

AC_DEFUN([DC_DOVECOT_TEST_WRAPPER],[
  AC_CHECK_PROG(VALGRIND, valgrind, yes, no)
  AS_IF([test "$VALGRIND" = yes], [
    cat > run-test.sh <<_DC_EOF
#!/bin/sh
top_srcdir=\$[1]
shift

if test "\$NOUNDEF" != ""; then
  noundef="--undef-value-errors=no"
else
  noundef=""
fi

if test "\$NOCHILDREN" != ""; then
  trace_children="--trace-children=no"
else
  trace_children="--trace-children=yes"
fi

skip_path="\$top_srcdir/run-test-valgrind.exclude"
if test -r "\$skip_path" && grep -w -q "\$(basename \$[1])" "\$skip_path"; then
  NOVALGRIND=true
fi

if test "\$NOVALGRIND" != ""; then
  \$[*]
  ret=\$?
else
  test_out="test.out~\$\$"
  trap "rm -f \$test_out" 0 1 2 3 15
  supp_path="\$top_srcdir/run-test-valgrind.supp"
  if test -r "\$supp_path"; then
    valgrind -q \$trace_children --leak-check=full --suppressions="\$supp_path" --log-file=\$test_out \$noundef \$[*]
  else
    valgrind -q \$trace_children --leak-check=full --log-file=\$test_out \$noundef \$[*]
  fi
  ret=\$?
  if test -s \$test_out; then
    cat \$test_out
    ret=1
  fi
fi
if test \$ret != 0; then
  echo "Failed to run: \$[*]" >&2
fi
exit \$ret
_DC_EOF
    RUN_TEST='$(SHELL) $(top_builddir)/run-test.sh $(top_srcdir)'
  ], [
    RUN_TEST=''
  ])
  AC_SUBST(RUN_TEST)
])

dnl Substitute every var in the given comma separated list
AC_DEFUN([AX_SUBST_L],[
	m4_foreach([__var__], [$@], [AC_SUBST(__var__)])
])

AC_DEFUN([DC_DOVECOT_HARDENING],[
        AC_ARG_ENABLE(hardening,
        AS_HELP_STRING([--enable-hardening=yes], [Enable various hardenings (default: yes)]),
                enable_hardening=$enableval,
                enable_hardening=yes)

        AC_MSG_CHECKING([Whether to enable hardening])
        AC_MSG_RESULT([$enable_hardening])

	AC_CC_PIE
	AC_CC_F_STACK_PROTECTOR
	AC_CC_D_FORTIFY_SOURCE
	AC_CC_RETPOLINE
	AC_LD_RELRO
])

AC_DEFUN([DC_DOVECOT],[
	AC_ARG_WITH(dovecot,
	  [  --with-dovecot=DIR      Dovecot base directory],
			[ dovecotdir="$withval" ], [
			  dc_prefix=$prefix
			  test "x$dc_prefix" = xNONE && dc_prefix=$ac_default_prefix
			  dovecotdir="$dc_prefix/lib/dovecot"
			]
	)

	AC_ARG_WITH(dovecot-install-dirs,
		[AS_HELP_STRING([--with-dovecot-install-dirs],
		[Use install directories configured for Dovecot (default)])],
	AS_IF([test x$withval = xno], [
		use_install_dirs=no
	], [
		use_install_dirs=yes
	]),
	use_install_dirs=yes)

	AC_MSG_CHECKING([for "$dovecotdir/dovecot-config"])
	AS_IF([test -f "$dovecotdir/dovecot-config"], [
		AC_MSG_RESULT([$dovecotdir/dovecot-config])
	], [
		AC_MSG_RESULT([not found])
		AC_MSG_NOTICE([])
		AC_MSG_NOTICE([Use --with-dovecot=DIR to provide the path to the dovecot-config file.])
		AC_MSG_ERROR([dovecot-config not found])
	])

	old=`pwd`
	cd $dovecotdir
	abs_dovecotdir=`pwd`
	cd $old
	DISTCHECK_CONFIGURE_FLAGS="--with-dovecot=$abs_dovecotdir --without-dovecot-install-dirs"

	dnl Make sure dovecot-config doesn't accidentically override flags
	ORIG_CFLAGS="$CFLAGS"
	ORIG_LDFLAGS="$LDFLAGS"
	ORIG_BINARY_CFLAGS="$BINARY_CFLAGS"
	ORIG_BINARY_LDFLAGS="$BINARY_LDFLAGS"

	eval `grep -i '^dovecot_[[a-z_]]*=' "$dovecotdir"/dovecot-config`
	eval `grep '^LIBDOVECOT[[A-Z0-9_]]*=' "$dovecotdir"/dovecot-config`

        CFLAGS="$ORIG_CFLAGS"
        LDFLAGS="$ORIG_LDFLAGS"
        BINARY_CFLAGS="$ORIG_BINARY_CFLAGS"
        BINARY_LDFLAGS="$ORIG_BINARY_LDFLAGS"

	dovecot_installed_moduledir="$dovecot_moduledir"

	AS_IF([test "$use_install_dirs" = "no"], [
		dnl the main purpose of these is to fix make distcheck for plugins
		dnl other than that, they don't really make much sense
		dovecot_pkgincludedir='$(pkgincludedir)'
		dovecot_pkglibdir='$(pkglibdir)'
		dovecot_pkglibexecdir='$(libexecdir)/dovecot'
		dovecot_docdir='$(docdir)'
		dovecot_moduledir='$(moduledir)'
		dovecot_statedir='$(statedir)'
	])

	CC_CLANG
	DC_DOVECOT_CFLAGS
	DC_DOVECOT_HARDENING

	AX_SUBST_L([DISTCHECK_CONFIGURE_FLAGS], [dovecotdir], [dovecot_moduledir], [dovecot_installed_moduledir], [dovecot_pkgincludedir], [dovecot_pkglibexecdir], [dovecot_pkglibdir], [dovecot_docdir], [dovecot_statedir])
	AX_SUBST_L([DOVECOT_INSTALLED], [DOVECOT_CFLAGS], [DOVECOT_LIBS], [DOVECOT_SSL_LIBS], [DOVECOT_SQL_LIBS], [DOVECOT_COMPRESS_LIBS], [DOVECOT_BINARY_CFLAGS], [DOVECOT_BINARY_LDFLAGS])
	AX_SUBST_L([LIBDOVECOT], [LIBDOVECOT_LOGIN], [LIBDOVECOT_SQL], [LIBDOVECOT_SSL], [LIBDOVECOT_COMPRESS], [LIBDOVECOT_LDA], [LIBDOVECOT_STORAGE], [LIBDOVECOT_DSYNC], [LIBDOVECOT_LIBFTS])
	AX_SUBST_L([LIBDOVECOT_DEPS], [LIBDOVECOT_LOGIN_DEPS], [LIBDOVECOT_SQL_DEPS], [LIBDOVECOT_SSL_DEPS], [LIBDOVECOT_COMPRESS_DEPS], [LIBDOVECOT_LDA_DEPS], [LIBDOVECOT_STORAGE_DEPS], [LIBDOVECOT_DSYNC_DEPS], [LIBDOVECOT_LIBFTS_DEPS])
	AX_SUBST_L([LIBDOVECOT_INCLUDE], [LIBDOVECOT_LDA_INCLUDE], [LIBDOVECOT_AUTH_INCLUDE], [LIBDOVECOT_DOVEADM_INCLUDE], [LIBDOVECOT_SERVICE_INCLUDE], [LIBDOVECOT_STORAGE_INCLUDE], [LIBDOVECOT_LOGIN_INCLUDE], [LIBDOVECOT_SQL_INCLUDE])
	AX_SUBST_L([LIBDOVECOT_IMAP_LOGIN_INCLUDE], [LIBDOVECOT_CONFIG_INCLUDE], [LIBDOVECOT_IMAP_INCLUDE], [LIBDOVECOT_POP3_INCLUDE], [LIBDOVECOT_SUBMISSION_INCLUDE], [LIBDOVECOT_LMTP_INCLUDE], [LIBDOVECOT_DSYNC_INCLUDE], [LIBDOVECOT_IMAPC_INCLUDE], [LIBDOVECOT_FTS_INCLUDE])
	AX_SUBST_L([LIBDOVECOT_NOTIFY_INCLUDE], [LIBDOVECOT_PUSH_NOTIFICATION_INCLUDE], [LIBDOVECOT_ACL_INCLUDE], [LIBDOVECOT_LIBFTS_INCLUDE])

	AM_CONDITIONAL(DOVECOT_INSTALLED, test "$DOVECOT_INSTALLED" = "yes")

	DC_PLUGIN_DEPS
	DC_DOVECOT_TEST_WRAPPER
])

AC_DEFUN([DC_CC_WRAPPER],[
  AS_IF([test "$want_shared_libs" != "yes"], [
    dnl want_shared_libs=no is for internal use. the liblib.la check is for plugins
    AS_IF([test "$want_shared_libs" = "no" || echo "$LIBDOVECOT" | grep "/liblib.la" > /dev/null], [
      AS_IF([test "$with_gnu_ld" = yes], [
	dnl libtool can't handle using whole-archive flags, so we need to do this
	dnl with a CC wrapper.. shouldn't be much of a problem, since most people
	dnl are building with shared libs.
	cat > cc-wrapper.sh <<_DC_EOF
#!/bin/sh

if echo "\$[*]" | grep -- -ldl > /dev/null; then
  # the binary uses plugins. make sure we include everything from .a libs
  exec $CC -Wl,--whole-archive \$[*] -Wl,--no-whole-archive
else
  exec $CC \$[*]
fi
_DC_EOF
	chmod +x cc-wrapper.sh
	CC=`pwd`/cc-wrapper.sh
      ])
    ])
  ])
])

AC_DEFUN([DC_PANDOC], [
  AC_ARG_VAR(PANDOC, [Path to pandoc program])

  dnl Optional tool for making documentation
  AC_CHECK_PROGS(PANDOC, [pandoc], [true])

  AS_IF([test "$PANDOC" = "true"], [
   AS_IF([test ! -e README], [
     AC_MSG_ERROR([Cannot produce documentation without pandoc - disable with PANDOC=false ./configure])
   ])
  ])
])
# warnings.m4 serial 11
dnl Copyright (C) 2008-2015 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Simon Josefsson


# gl_COMPILER_OPTION_IF(OPTION, [IF-SUPPORTED], [IF-NOT-SUPPORTED],
#                       [PROGRAM = AC_LANG_PROGRAM()])
# -----------------------------------------------------------------
# Check if the compiler supports OPTION when compiling PROGRAM.
#
# FIXME: gl_Warn must be used unquoted until we can assume Autoconf
# 2.64 or newer.
AC_DEFUN([gl_COMPILER_OPTION_IF],
[AS_VAR_PUSHDEF([gl_Warn], [gl_cv_warn_[]_AC_LANG_ABBREV[]_$1])dnl
AS_VAR_PUSHDEF([gl_Flags], [_AC_LANG_PREFIX[]FLAGS])dnl
AS_LITERAL_IF([$1],
  [m4_pushdef([gl_Positive], m4_bpatsubst([$1], [^-Wno-], [-W]))],
  [gl_positive="$1"
case $gl_positive in
  -Wno-*) gl_positive=-W`expr "X$gl_positive" : 'X-Wno-\(.*\)'` ;;
esac
m4_pushdef([gl_Positive], [$gl_positive])])dnl
AC_CACHE_CHECK([whether _AC_LANG compiler handles $1], m4_defn([gl_Warn]), [
  gl_save_compiler_FLAGS="$gl_Flags"
  AC_LINK_IFELSE([m4_default([$4], [AC_LANG_PROGRAM([])])],
                 [AS_VAR_SET(gl_Warn, [yes])],
                 [AS_VAR_SET(gl_Warn, [no])])
  gl_Flags="$gl_save_compiler_FLAGS"
])
AS_VAR_IF(gl_Warn, [yes], [$2], [$3])
m4_popdef([gl_Positive])dnl
AS_VAR_POPDEF([gl_Flags])dnl
AS_VAR_POPDEF([gl_Warn])dnl
])

# Local Variables:
# mode: autoconf
# End:
dnl * clang check
AC_DEFUN([CC_CLANG],[
  AC_MSG_CHECKING([whether $CC is clang 3.3+])
  AS_IF([$CC -dM -E -x c /dev/null | grep __clang__ > /dev/null 2>&1], [
      AS_VAR_SET([have_clang], [yes])
  ], [
      AS_VAR_SET([have_clang], [no])
  ])
  AC_MSG_RESULT([$have_clang])
])
