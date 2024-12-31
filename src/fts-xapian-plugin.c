/* Copyright (c) 2019 Joan Moreau <jom@grosjo.net>, see the included COPYING file */


#include "fts-xapian-plugin.h"

const char *fts_xapian_plugin_version = DOVECOT_ABI_VERSION;

struct fts_xapian_user_module fts_xapian_user_module = MODULE_CONTEXT_INIT(&mail_user_module_register);

static void fts_xapian_mail_user_deinit(struct mail_user *user)
{
#if ((DOVECOT_VERSION_MINOR > 2) || (DOVECOT_VERSION_MAJOR > 2))
	struct fts_xapian_user *fuser = FTS_XAPIAN_USER_CONTEXT_REQUIRE(user);
#else
	struct fts_xapian_user *fuser = FTS_XAPIAN_USER_CONTEXT(user);
#endif

        fts_mail_user_deinit(user);
	fuser->module_ctx.super.deinit(user);
}

static void fts_xapian_mail_user_created(struct mail_user *user)
{
        const char *error;

	struct mail_user_vfuncs *v = user->vlast;
	struct fts_xapian_user *fuser;

	fuser = p_new(user->pool, struct fts_xapian_user, 1);

        fuser->set.verbose 	= 0;
        fuser->set.lowmemory 	= XAPIAN_MIN_RAM;
        fuser->set.partial 	= XAPIAN_DEFAULT_PARTIAL;

	const char * env = mail_user_plugin_getenv(user, "fts_xapian");
        if (env == NULL)
        {
                i_warning("FTS Xapian: missing configuration - Using default values");
        }
	else
	{
		long len;
		const char *const *tmp;

        	for (tmp = t_strsplit_spaces(env, " "); *tmp != NULL; tmp++)
        	{
                	if (strncmp(*tmp, "partial=",8)==0)
                	{
                	        len=atol(*tmp + 8);
                	        if(len<3)
                	        {
                	                i_error("FTS Xapian: 'partial' parameter is incorrect (%ld). Try 'partial=%ld'",len,XAPIAN_DEFAULT_PARTIAL);
                	                len=XAPIAN_DEFAULT_PARTIAL;
                	        }
                	        fuser->set.partial = len;
                	}
                	else if (strncmp(*tmp,"verbose=",8)==0)
                	{
                	        len=atol(*tmp + 8);
                	        if(len>0) { fuser->set.verbose = len; }
                	}
                	else if (strncmp(*tmp,"lowmemory=",10)==0)
                	{
                	        len=atol(*tmp + 10);
                	        if(len>0) { fuser->set.lowmemory = len; }
                	}
                	else if (strncmp(*tmp,"attachments=",12)==0)
                	{
                	        // Legacy
                	}
			else if (strncmp(*tmp,"full=",5)==0)
                        {
                                // Legacy
                        }
			else if (strncmp(*tmp,"detach=",7)==0)
                        {
				// Legacy
			}
                	else
                	{
                	        i_error("FTS Xapian: Invalid setting: %s", *tmp);
                	}
        	}
	}

#ifdef FTS_MAIL_USER_INIT_THREE_ARGS
	if (fts_mail_user_init(user, FALSE, &error) < 0) 
#else
	if (fts_mail_user_init(user, &error) < 0)
#endif
	{
		if ( fuser->set.verbose > 1 ) i_warning("FTS Xapian: %s", error);
	}

	fuser->module_ctx.super = *v;
	user->vlast = &fuser->module_ctx.super;
	v->deinit = fts_xapian_mail_user_deinit;

	MODULE_CONTEXT_SET(user, fts_xapian_user_module, fuser);
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
