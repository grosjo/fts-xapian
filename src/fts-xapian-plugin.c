/* Copyright (c) 2019 Joan Moreau <jom@grosjo.net>, see the included COPYING file */

#include "fts-xapian-plugin.h"

const char *fts_xapian_plugin_version = DOVECOT_ABI_VERSION;

struct fts_xapian_user_module fts_xapian_user_module = MODULE_CONTEXT_INIT(&mail_user_module_register);

static void fts_xapian_mail_user_deinit(struct mail_user *user)
{
#if ((DOVECOT_VERSION_MINOR > 2) || (DOVECOT_VERSION_MAJOR > 2) || (FTS_DOVECOT24 > 0))
	struct fts_xapian_user *fuser = FTS_XAPIAN_USER_CONTEXT_REQUIRE(user);
#else
	struct fts_xapian_user *fuser = FTS_XAPIAN_USER_CONTEXT(user);
#endif

#ifdef FTS_DOVECOT24
	settings_free(fuser->set);
#else
	fts_mail_user_deinit(user);
#endif
	fuser->module_ctx.super.deinit(user);
}

#ifdef FTS_DOVECOT24

int fts_xapian_mail_user_get(struct mail_user *user, struct event *event,
	struct fts_xapian_user **fuser_r, const char **error_r)
{
	struct fts_xapian_user *fuser = FTS_XAPIAN_USER_CONTEXT_REQUIRE(user);
	struct fts_xapian_settings *set;

	if (settings_get(event, &fts_xapian_setting_parser_info, 0, &set, error_r) < 0) 
	{
		return -1;
	}

	/* Reference the user even when fuser is already initialized */
	if (fts_mail_user_init(user, event, FALSE, error_r) < 0) 
	{
		settings_free(set);
		return -1;
	}
	if (fuser->set == NULL)
		fuser->set = set;
	else
		settings_free(set);

	*fuser_r = fuser;
	return 0;
}

static void fts_xapian_mail_user_created(struct mail_user *user)
{
	struct fts_xapian_user *fuser;
	struct mail_user_vfuncs *v = user->vlast;

	fuser = p_new(user->pool, struct fts_xapian_user, 1);
	fuser->module_ctx.super = *v;
	user->vlast = &fuser->module_ctx.super;
	v->deinit = fts_xapian_mail_user_deinit;
	MODULE_CONTEXT_SET(user, fts_xapian_user_module, fuser);
}

#else

static void fts_xapian_mail_user_created(struct mail_user *user)
{
	const char *error;

	struct mail_user_vfuncs *v = user->vlast;
	struct fts_xapian_user *fuser;

	fuser = p_new(user->pool, struct fts_xapian_user, 1);

	fuser->set.verbose		= 0;
	fuser->set.lowmemory	= XAPIAN_MIN_RAM;
	fuser->set.partial		= XAPIAN_DEFAULT_PARTIAL;
	fuser->set.maxthreads	= 0;

	const char * env = mail_user_plugin_getenv(user, XAPIAN_LABEL);
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
			else if (strncmp(*tmp,"maxthreads=",11)==0)
			{
				len=atol(*tmp + 11);
				if(len>0) { fuser->set.maxthreads = len; }
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
		if ( fuser->set.verbose > 0 ) i_warning("FTS Xapian: %s", error);
	}

	fuser->module_ctx.super = *v;
	user->vlast = &fuser->module_ctx.super;
	v->deinit = fts_xapian_mail_user_deinit;

	MODULE_CONTEXT_SET(user, fts_xapian_user_module, fuser);
}
#endif

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
