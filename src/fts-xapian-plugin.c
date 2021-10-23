/* Copyright (c) 2019 Joan Moreau <jom@grosjo.net>, see the included COPYING file */


#include "fts-xapian-plugin.h"

const char *fts_xapian_plugin_version = DOVECOT_ABI_VERSION;

static void fts_xapian_mail_user_created(struct mail_user *user)
{
        struct mail_user_vfuncs *v = user->vlast;
        const char *env, *error;

        if (fts_mail_user_init(user, FALSE, &error) < 0) {
                i_error("FTS Xapian: %s", error);
        }
}

static struct mail_storage_hooks fts_xapian_mail_storage_hooks = 
{
        .mail_user_created = fts_xapian_mail_user_created
};

void fts_xapian_plugin_init(struct module *module ATTR_UNUSED)
{
	fts_backend_register(&fts_backend_xapian);
	mail_storage_hooks_add(module, &fts_xapian_mail_storage_hooks);
}

void fts_xapian_plugin_deinit(void)
{
	fts_backend_unregister(fts_backend_xapian.name);
	mail_storage_hooks_remove(&fts_xapian_mail_storage_hooks);
}

const char *fts_xapian_plugin_dependencies[] = { "fts", NULL };
