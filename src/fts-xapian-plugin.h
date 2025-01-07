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

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include <sqlite3.h>

#define XAPIAN_SLEEP std::chrono::milliseconds(200)

#define XAPIAN_DEFAULT_PARTIAL 3L

// Ressources limits
#define XAPIAN_FILE_PREFIX "xapian-indexes" // Locations of indexes
#define XAPIAN_TERM_SIZELIMIT 245L // Hard limit of Xapian library
#define XAPIAN_MAXTERMS_PERDOC 50000L // Nb of keywords max per email
#define XAPIAN_WRITING_CACHE 4000L // Max nb of emails processed in cache 
#define XAPIAN_DICT_MAX 50000L // Max nb of terms  in the dict
#define XAPIAN_MIN_RAM 500L // MB
#define XAPIAN_MAX_ERRORS 1024L 

// Word processing
#define XAPIAN_WILDCARD "wldcrd"
#define XAPIAN_EXPUNGE_HEADER 9

#define XAPIAN_MAX_SEC_WAIT 15L

#define HDRS_NB 11
static const char * hdrs_emails[HDRS_NB] = { "uid", "subject", "from", "to",  "cc",  "bcc",  "messageid", "listid", "body", "contenttype", ""  };
static const char * hdrs_xapian[HDRS_NB] = { "Q",   "S",       "A",    "XTO", "XCC", "XBCC", "XMID",      "XLIST",  "XBDY", "XCT", "XBDY" };

static const char * createExpTable = "CREATE TABLE IF NOT EXISTS expunges(ID INTEGER PRIMARY KEY NOT NULL);";
static const char * selectExpUIDs = "select ID from expunges;";
static const char * replaceExpUID = "replace into expunges values (%d);";
static const char * deleteExpUID = "delete from expunges where ID=%d;";
static const char * suffixExp = "_exp.db";

static const char * createDictTable = "CREATE TABLE IF NOT EXISTS dict (keyword TEXT, len INTEGER ); CREATE UNIQUE INDEX IF NOT EXISTS dict_idx ON dict (keyword COLLATE NOCASE); CREATE INDEX IF NOT EXISTS dict_len ON dict (len);";
static const char * replaceDictWord ="INSERT OR IGNORE INTO dict VALUES('";
static const char * searchDict1 = "SELECT keyword FROM dict WHERE (keyword like '%";
static const char * searchDict2 = "%') ORDER BY len LIMIT 100";
static const char * suffixDict = "_dict.db";

#define CHAR_KEY "_"
#define CHAR_SPACE " "

#define CHARS_PB 16
static const char * chars_pb[] = { "<", ">", ".", "-", "@", "&", "%", "*", "|", "`", "#", "^", "\\", "'", "/", "~" };

#define CHARS_SEP 16
static const char * chars_sep[] = { "\"", "\r", "\n", "\t", ",", ":", ";", "(", ")", "?", "!", "¿", "¡", "\u00A0", "‘", "“" };


struct fts_xapian_settings
{
	long verbose;
	long lowmemory;
	long partial;
	long maxthreads;
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
