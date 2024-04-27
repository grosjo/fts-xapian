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

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include <sqlite3.h>

#define XAPIAN_MIN_RAM 250L // MB
#define XAPIAN_DEFAULT_PARTIAL 3L
#define XAPIAN_DEFAULT_FULL 20L

#define XAPIAN_FILE_PREFIX "xapian-indexes"
#define XAPIAN_TERM_SIZELIMIT 245L
#define XAPIAN_THREAD_SIZE 3L
#define XAPIAN_WRITING_CACHE 2000L

#define XAPIAN_WILDCARD "wldcrd"
#define XAPIAN_EXPUNGE_HEADER 9

#define XAPIAN_MAX_SEC_WAIT 15L

#define HDRS_NB 10
static const char * hdrs_emails[HDRS_NB] = { "uid", "subject", "from", "to",  "cc",  "bcc",  "messageid", "listid", "body", ""  };
static const char * hdrs_xapian[HDRS_NB] = { "Q",   "S",       "A",    "XTO", "XCC", "XBCC", "XMID",      "XLIST",  "XBDY", "XBDY" };
static const char * createTable = "CREATE TABLE IF NOT EXISTS docs(ID INT PRIMARY KEY NOT NULL);";
static const char * selectUIDs = "select ID from docs;";
#define CHAR_KEY "_"
#define CHAR_SPACE " "
#define TRIM_SPACE " "
#define TRIM_ALL "_ "

#define CHARS_PB 14
static const char * chars_pb[] = { "<", ">", ".", "-", "@", "&", "%", "*", "|", "`", "#", "~", "^", "\\" };

#define CHARS_SEP 12
static const char * chars_sep[] = { "'", "\"", "\r", "\n", "\t", ",", ":", ";", "(", ")", "?", "!" };


struct fts_xapian_settings
{
	long verbose;
	long lowmemory;
	long partial,full;
	bool detach;
};

struct fts_xapian_user {
        union mail_user_module_context module_ctx;
        struct fts_xapian_settings set;
};

#define FTS_XAPIAN_USER_CONTEXT(obj) (struct fts_xapian_user *)MODULE_CONTEXT(obj, fts_xapian_user_module)
#if ((DOVECOT_VERSION_MINOR > 2) || (DOVECOT_VERSION_MAJOR > 2))
#define FTS_XAPIAN_USER_CONTEXT_REQUIRE(obj) MODULE_CONTEXT_REQUIRE(obj, fts_xapian_user_module)
#endif

extern const char *fts_xapian_plugin_dependencies[];
extern MODULE_CONTEXT_DEFINE(fts_xapian_user_module, &mail_user_module_register);
extern struct fts_backend fts_backend_xapian;

void fts_xapian_plugin_init(struct module *module);
void fts_xapian_plugin_deinit(void);

#endif
