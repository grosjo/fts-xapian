/* Copyright (c) 2019 Joan Moreau <jom@grosjo.net>, see the included COPYING file */


#ifndef FTS_XAPIAN_PLUGIN_H
#define FTS_XAPIAN_PLUGIN_H

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

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <vm/vm_param.h>
#endif

#define XAPIAN_FILE_PREFIX "xapian-indexes"
#define XAPIAN_TERM_SIZELIMIT 245L
#define XAPIAN_COMMIT_ENTRIES 1000000L
#define XAPIAN_COMMIT_TIMEOUT 300L
#define XAPIAN_WILDCARD "wldcrd"
#define XAPIAN_EXPUNGE_HEADER 9
#define XAPIAN_MIN_RAM 250L // MB
#define XAPIAN_DEFAULT_PARTIAL 3L
#define XAPIAN_DEFAULT_FULL 20L

struct fts_xapian_settings
{
	int verbose;
	unsigned long pagesize;
	long lowmemory;
	long partial,full;
};

struct fts_xapian_user {
        union mail_user_module_context module_ctx;
        struct fts_xapian_settings set;
};

#define FTS_XAPIAN_USER_CONTEXT(obj) (struct fts_xapian_user *)MODULE_CONTEXT(obj, fts_xapian_user_module)
#define FTS_XAPIAN_USER_CONTEXT_REQUIRE(obj) MODULE_CONTEXT_REQUIRE(obj, fts_xapian_user_module)

extern const char *fts_xapian_plugin_dependencies[];
extern MODULE_CONTEXT_DEFINE(fts_xapian_user_module, &mail_user_module_register);
extern struct fts_backend fts_backend_xapian;

void fts_xapian_plugin_init(struct module *module);
void fts_xapian_plugin_deinit(void);

#endif
