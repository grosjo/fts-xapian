/* Copyright (c) 2019 Joan Moreau <jom@grosjo.net>, see the included COPYING file */


#ifndef FTS_XAPIAN_PLUGIN_H
#define FTS_XAPIAN_PLUGIN_H

#include "config.h"
#include "lib.h"
#include "mail-user.h"
#include "fts-api.h"
#include "fts-user.h"
#include "mail-search.h"
#include "mail-storage-private.h"
#include "restrict-process-size.h"
#include "mail-storage-hooks.h"
#include "module-context.h"
#include "fts-api-private.h"
#include "master-service.h"
#ifdef FTS_DOVECOT24
#include "fts-settings.h"
#include "settings-parser.h"
#include "settings.h"
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include <sqlite3.h>

#define XAPIAN_PLUGIN_VERSION "1.9.1"
#define XAPIAN_LABEL "fts_xapian"

// Main parameters
#define XAPIAN_FILE_PREFIX "xapian-indexes" // Locations of indexes
#define XAPIAN_MIN_RAM 300L // MB
#define XAPIAN_DEFAULT_PARTIAL 3L

struct fts_xapian_settings
{
#ifdef FTS_DOVECOT24
	pool_t pool;
#endif
	unsigned int verbose;
	unsigned int lowmemory;
	unsigned int partial;
	unsigned int maxthreads;
};

struct fts_xapian_user {
        union mail_user_module_context module_ctx;
#ifdef FTS_DOVECOT24
	struct fts_xapian_settings *set;
#else
        struct fts_xapian_settings set;
#endif
};

#define FTS_XAPIAN_USER_CONTEXT(obj) (struct fts_xapian_user *)MODULE_CONTEXT(obj, fts_xapian_user_module)
#if ((DOVECOT_VERSION_MINOR > 2) || (DOVECOT_VERSION_MAJOR > 2) || (FTS_DOVECOT24 > 0))
#define FTS_XAPIAN_USER_CONTEXT_REQUIRE(obj) MODULE_CONTEXT_REQUIRE(obj, fts_xapian_user_module)
#endif

extern const char *fts_xapian_plugin_dependencies[];
extern MODULE_CONTEXT_DEFINE(fts_xapian_user_module, &mail_user_module_register);
extern struct fts_backend fts_backend_xapian;

#ifdef FTS_DOVECOT24

int fts_xapian_mail_user_get(struct mail_user *user, struct event *event,
                                struct fts_xapian_user **fuser_r,
                                const char **error_r);

extern const struct setting_parser_info fts_xapian_setting_parser_info;

#endif

void fts_xapian_plugin_init(struct module *module);
void fts_xapian_plugin_deinit(void);

#endif
