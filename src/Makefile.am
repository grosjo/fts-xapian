AM_CPPFLAGS = \
	$(LIBDOVECOT_INCLUDE) \
	$(LIBDOVECOT_STORAGE_INCLUDE) \
	$(LIBDOVECOT_DOVEADM_INCLUDE) \
	$(LIBDOVECOT_LIBFTS_INCLUDE) \
	$(LIBDOVECOT_FTS_INCLUDE)

# -fpermissive is unfortunately needed for str.h
AM_CXXFLAGS = \
	$(AM_CPPFLAGS) \
	$(XAPIAN_CXXFLAGS) \
	-fpermissive

lib21_fts_xapian_plugin_la_LDFLAGS = -module -avoid-version

module_LTLIBRARIES = lib21_fts_xapian_plugin.la

lib21_fts_xapian_plugin_la_LIBADD = $(XAPIAN_LIBS) $(ICU_LIBS)

lib21_fts_xapian_plugin_la_SOURCES = \
	fts-xapian-plugin.c \
	fts-backend-xapian.cpp
EXTRA_DIST = \
	fts-backend-xapian-functions.cpp

noinst_HEADERS = fts-xapian-plugin.h



