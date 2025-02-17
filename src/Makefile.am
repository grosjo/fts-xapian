AM_CPPFLAGS = -O2 \
	$(LIBDOVECOT_INCLUDE) \
	$(LIBDOVECOT_STORAGE_INCLUDE) \
	$(LIBDOVECOT_DOVEADM_INCLUDE) \
	$(LIBDOVECOT_LIBFTS_INCLUDE) \
	$(LIBDOVECOT_FTS_INCLUDE) \
	$(XAPIAN_INCLUDE) \
	$(USERINIT_ARGS) \
	$(ICU_CFLAGS) \
	$(SQLITE_CFLAGS)

AM_CXXFLAGS = -std=gnu++20 -Wall

lib21_fts_xapian_plugin_la_LDFLAGS = -module -avoid-version

module_LTLIBRARIES = lib21_fts_xapian_plugin.la

lib21_fts_xapian_plugin_la_LIBADD = $(XAPIAN_LIBS) $(ICU_LIBS) $(SQLITE_LIBS)

lib21_fts_xapian_plugin_la_SOURCES = \
	fts-xapian-plugin.c \
	fts-xapian-settins.c \
	fts-backend-xapian.cpp
EXTRA_DIST = \
	fts-backend-xapian-functions.cpp

noinst_HEADERS = fts-xapian-plugin.h



