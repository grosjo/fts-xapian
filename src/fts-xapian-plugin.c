/* Copyright (c) 2019 Joan Moreau <jom@grosjo.net>, see the included COPYING file */


#include "fts-xapian-plugin.h"

const char *fts_xapian_plugin_version = DOVECOT_ABI_VERSION;

void fts_xapian_plugin_init(struct module *module ATTR_UNUSED)
{
	//i_warning("fts_xapian_plugin_init");
	fts_backend_register(&fts_backend_xapian);
}

void fts_xapian_plugin_deinit(void)
{
	//i_warning("fts_xapian_plugin_deinit");
	fts_backend_unregister(fts_backend_xapian.name);
}

const char *fts_xapian_plugin_dependencies[] = { "fts", NULL };
