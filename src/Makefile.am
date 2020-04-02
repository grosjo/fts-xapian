AM_CPPFLAGS = \
	$(LIBDOVECOT_INCLUDE) \
	$(LIBDOVECOT_STORAGE_INCLUDE) \
	$(LIBDOVECOT_DOVEADM_INCLUDE) \
	$(LIBDOVECOT_LIBFTS_INCLUDE) \
	$(LIBDOVECOT_FTS_INCLUDE) \
	$(XAPIAN_INCLUDE) \
	$(ICU_CFLAGS) \
	$(SQL_CFLAGS)

AM_CXXFLAGS = -std=gnu++11 \
	$(AM_CPPFLAGS) \
	$(XAPIAN_LIBS) \
	$(ICU_LIBS) \
	$(SQL_LIBS) \
	-Wl,-rpath=$(ICU_LDFLAGS)

lib21_fts_xapian_plugin_la_LDFLAGS = -module -avoid-version

module_LTLIBRARIES = lib21_fts_xapian_plugin.la

lib21_fts_xapian_plugin_la_LIBADD = $(XAPIAN_LIBS) $(ICU_LIBS) $(SQL_LIBS)

lib21_fts_xapian_plugin_la_SOURCES = \
	fts-xapian-plugin.c \
	fts-backend-xapian.cpp
EXTRA_DIST = \
	fts-backend-xapian-functions.cpp

noinst_HEADERS = fts-xapian-plugin.h



