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
#ifdef FTS_MAIL_USER_INIT_FOUR_ARGS
#include "fts-settings.h"
#include "settings-parser.h"
#include "settings.h"
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include <sqlite3.h>

#define XAPIAN_SLEEP std::chrono::milliseconds(200)

#define XAPIAN_PLUGIN_VERSION "1.9"

// Ressources limits
#define XAPIAN_FILE_PREFIX "xapian-indexes" // Locations of indexes
#define XAPIAN_TERM_SIZELIMIT 245L // Hard limit of Xapian library
#define XAPIAN_MAXTERMS_PERDOC 50000L // Nb of keywords max per email
#define XAPIAN_WRITING_CACHE 5000L // Max nb of emails processed in cache 
#define XAPIAN_DICT_MAX 60000L // Max nb of terms  in the dict
#define XAPIAN_MIN_RAM 300L // MB
#define XAPIAN_MAX_ERRORS 1024L 

// Word processing
#define XAPIAN_DEFAULT_PARTIAL 3L
#define XAPIAN_MAX_SEC_WAIT 15L

#define HDRS_NB 11
static const char * hdrs_emails[HDRS_NB] =  { "uid", "subject", "from", "to",  "cc",  "bcc",  "messageid", "listid", "body", "contenttype", ""  };
static const char * hdrs_xapian[HDRS_NB] =  { "Q",   "S",       "A",    "XTO", "XCC", "XBCC", "XMID",      "XLIST",  "XBDY", "XCT", "XBDY" };
static const char * hdrs_query[HDRS_NB]  =  { "a",   "b",       "c",    "d",   "d",   "e",    "f",         "g",      "h",    "i",   "h"    };
#define HDR_BODY 8L

static const char * createExpTable = "CREATE TABLE IF NOT EXISTS expunges(ID INTEGER PRIMARY KEY NOT NULL);";
static const char * selectExpUIDs = "select ID from expunges;";
static const char * replaceExpUID = "replace into expunges values (%d);";
static const char * deleteExpUID = "delete from expunges where ID=%d;";
static const char * suffixExp = "_exp.db";

static const char * createDictTable = "CREATE TABLE IF NOT EXISTS dict (keyword TEXT COLLATE NOCASE, header INTEGER, len INTEGER, UNIQUE(keyword,header));";
static const char * createDictIndexes = "CREATE INDEX IF NOT EXISTS dict_len ON dict (len); CREATE INDEX IF NOT EXISTS dict_h ON dict(header); CREATE INDEX IF NOT EXISTS dict_t ON dict(keyword);";
static const char * createTmpTable = "ATTACH DATABASE ':memory:' AS work; CREATE TABLE work.dict (keyword TEXT COLLATE NOCASE, header INTEGER, len INTEGER, UNIQUE(keyword,header) ); CREATE INDEX IF NOT EXISTS work.dict_h ON dict(header)";
static const char * replaceTmpWord ="INSERT OR IGNORE INTO work.dict VALUES('";
static const char * flushTmpWords = "BEGIN TRANSACTION; INSERT OR IGNORE INTO main.dict SELECT keyword, header, len FROM work.dict; DELETE FROM work.dict; COMMIT;";
static const char * searchDict1 = "SELECT keyword FROM dict WHERE keyword like '%";
static const char * searchDict2 = " ORDER BY len LIMIT 100";
static const char * suffixDict = "_dict.db";

#define CHAR_KEY "_"
#define CHAR_SPACE " "

#define CHARS_PB 21
static const char * chars_pb[] = { "<", ">", ".", "-", "@", "&", "%", "*", "|", "`", "#", "^", "\\", "'", "/", "~", "[", "]", "{", "}", "-" };

#define CHARS_SEP 16
static const char * chars_sep[] = { "\"", "\r", "\n", "\t", ",", ":", ";", "(", ")", "?", "!", "¿", "¡", "\u00A0", "‘", "“" };


struct fts_xapian_settings
{
#ifdef FTS_MAIL_USER_INIT_FOUR_ARGS
	pool_t pool;
#endif
	unsigned int verbose;
	unsigned int lowmemory;
	unsigned int partial;
	unsigned int maxthreads;
};

struct fts_xapian_user {
        union mail_user_module_context module_ctx;
#ifdef FTS_MAIL_USER_INIT_FOUR_ARGS
	struct fts_xapian_settings *set;
#endif
#ifndef FTS_MAIL_USER_INIT_FOUR_ARGS
        struct fts_xapian_settings set;
#endif
};

#define FTS_XAPIAN_USER_CONTEXT(obj) (struct fts_xapian_user *)MODULE_CONTEXT(obj, fts_xapian_user_module)
#if ((DOVECOT_VERSION_MINOR > 2) || (DOVECOT_VERSION_MAJOR > 2) || (FTS_MAIL_USER_INIT_FOUR_ARGS > 0))
#define FTS_XAPIAN_USER_CONTEXT_REQUIRE(obj) MODULE_CONTEXT_REQUIRE(obj, fts_xapian_user_module)
#endif

extern const char *fts_xapian_plugin_dependencies[];
extern MODULE_CONTEXT_DEFINE(fts_xapian_user_module, &mail_user_module_register);
extern struct fts_backend fts_backend_xapian;

#ifdef FTS_MAIL_USER_INIT_FOUR_ARGS
int fts_xapian_mail_user_get(struct mail_user *user, struct event *event,
                                struct fts_xapian_user **fuser_r,
                                const char **error_r);
#endif

void fts_xapian_plugin_init(struct module *module);
void fts_xapian_plugin_deinit(void);

#ifdef FTS_MAIL_USER_INIT_FOUR_ARGS
extern const struct setting_parser_info fts_xapian_setting_parser_info;
#endif

#endif
