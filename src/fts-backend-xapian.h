/* Copyright (c) 2019 Joan Moreau <jom@grosjo.net>, see the included COPYING file */

#ifndef FTS_XAPIAN_BACKEND_H
#define FTS_XAPIAN_BACKEND_H

#define XAPIAN_SLEEP std::chrono::milliseconds(200)

// Ressources limits
#define XAPIAN_TERM_SIZELIMIT 245L // Hard limit of Xapian library
#define XAPIAN_MAXTERMS_PERDOC 50000L // Nb of keywords max per email
#define XAPIAN_WRITING_CACHE 5000L // Max nb of emails processed in cache 
#define XAPIAN_DICT_MAX 60000L // Max nb of terms	in the dict
#define XAPIAN_MAX_ERRORS 1024L 
#define XAPIAN_MAX_SEC_WAIT 15L

#define HDRS_NB 11
static const char * hdrs_emails[HDRS_NB] =  { "uid", "subject", "from", "to",	 "cc",  "bcc",	 "messageid", "listid", "body", "contenttype", ""	};
static const char * hdrs_xapian[HDRS_NB] =  { "Q", "S", "A", "XTO", "XCC", "XBCC", "XMID", "XLIST", "XBDY", "XCT", "XBDY" };
static const char * hdrs_query[HDRS_NB]  =  { "a", "b", "c", "d",   "d",   "e",    "f",	   "g",     "h",    "i",   "h" };
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

#endif
