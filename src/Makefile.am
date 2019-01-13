
AM_CPPFLAGS = \
	-I$(top_srcdir)/src/lib \
	-I$(top_srcdir)/src/lib-mail \
	-I$(top_srcdir)/src/lib-index \
	-I$(top_srcdir)/src/lib-storage \
	-I$(top_srcdir)/src/plugins/fts \
	-fpermissive

NOPLUGIN_LDFLAGS =
lib21_fts_xapian_plugin_la_LDFLAGS = -module -avoid-version

module_LTLIBRARIES = lib21_fts_xapian_plugin.la

lib21_fts_xapian_plugin_la_LIBADD = ../fts/lib20_fts_plugin.la -lxapian

lib21_fts_xapian_plugin_la_SOURCES = fts-xapian-plugin.c fts-backend-xapian.cpp 

noinst_HEADERS = fts-xapian-plugin.h


