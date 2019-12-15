# Get ICU_CXXFLAGS, ICU_LIBS, and ICU_VERSION from icu-config and
# AC_SUBST() them.

# serial 17

# AC_PROVIDE_IFELSE(MACRO-NAME, IF-PROVIDED, IF-NOT-PROVIDED)
# -----------------------------------------------------------
# If this macro is not defined by Autoconf, define it here.
m4_ifdef([AC_PROVIDE_IFELSE],
	 [],
	 [m4_define([AC_PROVIDE_IFELSE],
		 [m4_ifdef([AC_PROVIDE_$1],
			   [$2], [$3])])])

# XO_LIB_ICU([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND[ ,ICU-CONFIG]]])
# --------------------------------------------------------
# AC_SUBST-s ICU_CXXFLAGS, ICU_LIBS, and ICU_VERSION for use in
# Makefile.am
#
# If ACTION-IF-FOUND and ACTION-IF-NOT-FOUND are both unset, then an
# appropriate AC_MSG_ERROR is used as a default ACTION-IF-NOT-FOUND.
# This allows XO_LIB_ICU to be used without any arguments in the
# common case where Xapian is a requirement (rather than optional).
#
# ICU-CONFIG provides the default name for the icu-config script
# (which the user can override with "./configure ICU_CONFIG=/path/to/it").
# If unset, the default is icu-config.  Support for this third parameter
# was added in Xapian 1.1.3.
AC_DEFUN([XO_LIB_ICU],
[
  AC_ARG_VAR(ICU_CONFIG, [Location of icu-config (default:] ifelse([$3], [], icu-config, [$3]) [on PATH)])
  dnl AC_PATH_PROG ignores an existing user setting of ICU_CONFIG unless it
  dnl has a full path, so add special handling for such cases.
  icu_config_to_check_for="ifelse([$3], [], icu-config, [$3])"
  case $ICU_CONFIG in
  "") ;;
  */configure)
    AC_MSG_ERROR([ICU_CONFIG should point to a icu-config script, not a configure script.])
    ;;
  [[\\/]* | ?:[\\/]*)]
    # ICU_CONFIG has an absolute path, so AC_PATH_PROG can handle it.
    ;;
  [*[\\/]?*)]
    # Convert a relative path to an absolute one.
    ICU_CONFIG=`pwd`/$ICU_CONFIG
    ;;
  *)
    # If there's no path on ICU_CONFIG, use it as the name of the tool to
    # search PATH for, so that things like this work:
    #   ./configure ICU_CONFIG=icu-config-1.3
    icu_config_to_check_for=$ICU_CONFIG
    ICU_CONFIG=
    ;;
  esac
  AC_PATH_PROG(ICU_CONFIG, "$icu_config_to_check_for")
  if test -z "$ICU_CONFIG"; then
    ifelse([$2], ,
      [ifelse([$1], , [
	dnl Simple check to see if the problem is likely to
	dnl be that we're using a "packaged" icu-core but
	dnl only have the runtime package installed.
	for sfx in '' 32 64 ; do
	  set /usr/lib$sfx/libicu.so.*
	  if test "/usr/lib$sfx/libicu.so.*" != "$1" ; then
	    if test -r /etc/debian_version ; then
	      pkg="libicu-dev"
	    else
	      pkg="icu-core-devel"
	    fi
	    AC_MSG_ERROR([Can't find icu-config, although the icu-core runtime library seems to be installed.  If you've installed icu-core from a package, you probably need to install an extra package called something like $pkg in order to be able to build code using the Xapian library.])
	  fi
	done
	AC_MSG_ERROR([Can't find icu-config.  If you have installed the Xapian library, you need to add ICU_CONFIG=/path/to/icu-config to your configure command.])],
	:)],
      [$2])
  else
    AC_MSG_CHECKING([$ICU_CONFIG works])
    dnl check for --ldflags but not --libs as "icu-config --libs" will
    dnl fail if icu isn't installed...

    dnl run with exec to avoid leaking output on "real" bourne shells
    if (exec >&5 2>&5 ; $ICU_CONFIG --ldflags --cxxflags; exit $?) then
      AC_MSG_RESULT(yes)
    else
      case $? in
      127)
	AC_MSG_ERROR(['$ICU_CONFIG' not found, aborting])
	;;
      126)
	if test -d "$ICU_CONFIG" ; then
	  AC_MSG_ERROR(['$ICU_CONFIG' is a directory; it should be the filename of the icu-config script])
	fi
	AC_MSG_ERROR(['$ICU_CONFIG' not executable, aborting])
	;;
      esac
      AC_MSG_ERROR(['$ICU_CONFIG --ldflags --cxxflags' doesn't work, aborting])
    fi

dnl If LT_INIT, AC_PROG_LIBTOOL or the deprecated older version
dnl AM_PROG_LIBTOOL has already been expanded, enable libtool support now.
dnl Otherwise add hooks to the end of LT_INIT, AC_PROG_LIBTOOL and
dnl AM_PROG_LIBTOOL to enable it if one of these is expanded later.
    ICU_VERSION=`$ICU_CONFIG --version|sed 's/.* //;s/_.*$//'`
    ICU_CXXFLAGS=`$ICU_CONFIG --cxxflags`
    AC_PROVIDE_IFELSE([LT_INIT],
      [ICU_LIBS=`$ICU_CONFIG --ldflags`],
      [AC_PROVIDE_IFELSE([AC_PROG_LIBTOOL],
	[ICU_LIBS=`$ICU_CONFIG --ldflags`],
	[AC_PROVIDE_IFELSE([AM_PROG_LIBTOOL],
	  [ICU_LIBS=`$ICU_CONFIG --ldflags`],
	  dnl Pass magic option so icu-config knows we called it (so it
	  dnl can choose a more appropriate error message if asked to link
	  dnl with an uninstalled libicu).  Also pass ac_top_srcdir
	  dnl so the error message can correctly say "configure.ac" or
	  dnl "configure.in" according to which is in use.
	  [ICU_LIBS=`ac_top_srcdir="$ac_top_srcdir" $ICU_CONFIG --from-xo-lib-icu --libs`
	  m4_ifdef([LT_INIT],
	    [m4_define([LT_INIT], m4_defn([LT_INIT])
		   [ICU_LIBS=`$ICU_CONFIG --ldflags`])])
	  m4_ifdef([AC_PROG_LIBTOOL],
	    [m4_define([AC_PROG_LIBTOOL], m4_defn([AC_PROG_LIBTOOL])
		 [ICU_LIBS=`$ICU_CONFIG --ldflags`])])
	  m4_ifdef([AM_PROG_LIBTOOL],
	    [m4_define([AM_PROG_LIBTOOL], m4_defn([AM_PROG_LIBTOOL])
		 [ICU_LIBS=`$ICU_CONFIG --ldflags`])])])])])
    ifelse([$1], , :, [$1])
  fi
  AC_SUBST(ICU_CXXFLAGS)
  AC_SUBST(ICU_LIBS)
  AC_SUBST(ICU_VERSION)
  m4_define([XO_LIB_ICU_EXPANDED_], [])
])

# XO_ICU_REQUIRE(VERSION[, ACTION-IF-LESS-THAN[, ACTION-IF-GREATHER-THAN-OR-EQUAL]])
# --------------------------------------------------------
# Check if $ICU_VERSION is at least VERSION.  This macro should
# be used after XO_LIB_ICU.
#
# If ACTION-IF-LESS-THAN is unset, it defaults to an
# appropriate AC_MSG_ERROR saying that Xapian >= VERSION is needed.
#
# If ACTION-IF-GREATHER-THAN-OR-EQUAL is unset, the default is no
# addtional action.
AC_DEFUN([XO_ICU_REQUIRE],
[
  m4_ifndef([XO_LIB_ICU_EXPANDED_],
      [m4_fatal([XO_ICU_REQUIRE can only be used after XO_LIB_ICU])])
dnl [Version component '$v' is not a number]
  AC_MSG_CHECKING([if $ICU_CONFIG version >= $1])
  old_IFS=$IFS
  IFS=.
  set x `echo "$ICU_VERSION"|sed 's/_.*//'`
  IFS=$old_IFS
  res=
  m4_foreach([min_component], m4_split([$1], [\.]), [
ifelse(regexp(min_component, [^[0-9][0-9]*$]), [-1], [m4_fatal(Component `min_component' not numeric)])dnl
  if test -z "$res" ; then
    shift
    if test "$[]1" -gt 'min_component' ; then
      res=1
    elif test "$[]1" -lt 'min_component' ; then
      res=0
    fi
  fi])
  if test "$res" = 0 ; then
    AC_MSG_RESULT([no ($ICU_VERSION)])
    m4_default([$2], [AC_ERROR([ICU_VERSION is $ICU_VERSION, but >= $1 required])])
  else
    AC_MSG_RESULT([yes ($ICU_VERSION)])
    $3
  fi
])
