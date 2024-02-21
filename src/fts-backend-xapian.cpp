/* Copyright (c) 2019 Joan Moreau <jom@grosjo.net>, see the included COPYING file */

#include <xapian.h>
#include <thread>
#include <cstdio>
#include <vector>
extern "C" {
#include "fts-xapian-plugin.h"
}
#include <dirent.h>
#include <unicode/utypes.h>
#include <unicode/unistr.h>
#include <unicode/translit.h>
#include <sys/time.h>

#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <syslog.h>

struct xapian_fts_backend
{
	struct fts_backend backend;
	char * path;

	char * guid;
	char * boxname;
	char * db;
	char * expdb;

	char * old_guid;
	char * old_boxname;

	Xapian::WritableDatabase * dbw;
        Xapian::Document * * qdocs;
	long qcur;
	long qsize; 

	long lastuid;
	long total_added_docs;
	long start_time;

	long nbdocs = 0;
};

struct xapian_fts_backend_update_context
{
	struct fts_backend_update_context ctx;
	char * tbi_field=NULL;
	bool isattachment=false;
	bool tbi_isfield;
	uint32_t tbi_uid=0;
};

static struct fts_xapian_settings fts_xapian_settings;

#include "fts-backend-xapian-functions.cpp"


static struct fts_backend *fts_backend_xapian_alloc(void)
{
	struct xapian_fts_backend *backend;

	backend = i_new(struct xapian_fts_backend, 1);
	backend->backend = fts_backend_xapian;
	return &backend->backend;
}

static int fts_backend_xapian_init(struct fts_backend *_backend, const char **error_r)
{
	(void)error_r;

	struct xapian_fts_backend *backend = (struct xapian_fts_backend *)_backend;

	backend->db = NULL;
	backend->dbw = NULL;

	backend->qdocs = NULL;
	backend->qcur = -1;
	backend->qsize = 0;
	backend->lastuid = -1;

	backend->guid = NULL;
	backend->path = NULL;
	backend->old_guid = NULL;
	backend->old_boxname = NULL;

	struct fts_xapian_user *fuser = FTS_XAPIAN_USER_CONTEXT(_backend->ns->user);
	fts_xapian_settings = fuser->set;

	if(fts_backend_xapian_set_path(backend)<0) return -1;

        if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Starting with partial=%ld full=%ld verbose=%d lowmemory=%ld MB vs freemem=%ld MB", fts_xapian_settings.partial,fts_xapian_settings.full,fts_xapian_settings.verbose,fts_xapian_settings.lowmemory, long(fts_backend_xapian_get_free_memory()/1024.0));

	return 0;
}

static void fts_backend_xapian_deinit(struct fts_backend *_backend)
{
	struct xapian_fts_backend *backend = (struct xapian_fts_backend *)_backend;

	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: Deinit %s)",backend->path);

	if(backend->guid != NULL) fts_backend_xapian_unset_box(backend);

	if(backend->old_guid != NULL) i_free(backend->old_guid);
	backend->old_guid = NULL;

	if(backend->old_boxname != NULL) i_free(backend->old_boxname);
	backend->old_boxname = NULL;

	if(backend->path != NULL) i_free(backend->path);
	backend->path = NULL;

	i_free(backend);
}


static int fts_backend_xapian_get_last_uid(struct fts_backend *_backend, struct mailbox *box, uint32_t *last_uid_r)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_get_last_uid");

	struct xapian_fts_backend *backend = (struct xapian_fts_backend *)_backend;

	*last_uid_r = 0;

	if(fts_backend_xapian_set_box(backend, box) < 0)
	{
		i_error("FTS Xapian: get_last_uid: Can not select mailbox '%s'",box->name);
		return -1;
	}

	Xapian::Database * dbr;

	if(!fts_backend_xapian_open_readonly(backend, &dbr))
	{
		i_error("FTS Xapian: GetLastUID: Can not open db RO (%s)",backend->db);
		return 0;
	}

	try
	{
		*last_uid_r = Xapian::sortable_unserialise(dbr->get_value_upper_bound(1));
	}
	catch(Xapian::Error e)
	{
		i_error("FTS Xapian: fts_backend_xapian_get_last_uid for '%s' (%s)",backend->boxname,backend->guid);
		i_error("FTS Xapian: %s",e.get_msg().c_str());
		dbr->close();
		delete(dbr);
		return -1;
	}

	dbr->close();
	delete(dbr);

	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: Get last UID of %s (%s) = %d",backend->boxname,backend->guid,*last_uid_r);

	return 0;
}


static struct fts_backend_update_context * fts_backend_xapian_update_init(struct fts_backend *_backend)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_update_context");

	struct xapian_fts_backend_update_context *ctx;

	ctx = i_new(struct xapian_fts_backend_update_context, 1);
	ctx->ctx.backend = _backend;
	return &ctx->ctx;
}

static int fts_backend_xapian_update_deinit(struct fts_backend_update_context *_ctx)
{
	struct xapian_fts_backend_update_context *ctx = (struct xapian_fts_backend_update_context *)_ctx;
	struct xapian_fts_backend *backend = (struct xapian_fts_backend *)ctx->ctx.backend;

	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_update_deinit (%s)",backend->path);

	fts_backend_xapian_commitdocs(backend,"update_deinit");
	fts_backend_xapian_release(backend,"update_deinit",0,true);

	i_free(ctx);

	return 0;
}

static void fts_backend_xapian_update_set_mailbox(struct fts_backend_update_context *_ctx, struct mailbox *box)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_update_set_mailbox");

	struct xapian_fts_backend_update_context *ctx = (struct xapian_fts_backend_update_context *)_ctx;
	struct xapian_fts_backend *backend = (struct xapian_fts_backend *)ctx->ctx.backend;

	fts_backend_xapian_set_box(backend, box);
}

static void fts_backend_xapian_update_expunge(struct fts_backend_update_context *_ctx, uint32_t uid)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_update_expunge");

	struct xapian_fts_backend_update_context *ctx = (struct xapian_fts_backend_update_context *)_ctx;
	struct xapian_fts_backend *backend = (struct xapian_fts_backend *)ctx->ctx.backend;

	sqlite3 * expdb = NULL;
	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Opening expunge DB(%s)",backend->expdb);

	if(sqlite3_open(backend->expdb,&expdb))
	{
		i_error("FTS Xapian: Expunging (1) UID=%d : Can not open %s",uid,backend->expdb);
		return;
	}
	char *zErrMsg = 0;
	if(sqlite3_exec(expdb,createTable,NULL,0,&zErrMsg) != SQLITE_OK )
	{
		i_error("FTS Xapian: Expunging (2) UID=%d : Can not create table (%s) : %s",uid,createTable,zErrMsg);
		sqlite3_free(zErrMsg);
	}
	else
	{
		char * u = i_strdup_printf("replace into docs values (%d)",uid);
		if(sqlite3_exec(expdb,u,NULL,0,&zErrMsg) != SQLITE_OK)
		{
			i_error("FTS Xapian: Expunging (3) UID=%d : Can not add UID : %s",uid,zErrMsg);
			sqlite3_free(zErrMsg);
		}
		i_free(u);
	}
	sqlite3_close(expdb);
}

static bool fts_backend_xapian_update_set_build_key(struct fts_backend_update_context *_ctx, const struct fts_backend_build_key *key)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_update_set_build_key");

	struct xapian_fts_backend_update_context *ctx = (struct xapian_fts_backend_update_context *)_ctx;

	struct xapian_fts_backend *backend = (struct xapian_fts_backend *)ctx->ctx.backend;

	ctx->tbi_isfield=false;
	ctx->tbi_uid=0;

	if(backend->guid == NULL)
	{
		if(fts_xapian_settings.verbose>0) i_warning("FTS Xapian: Build key %s with no mailbox",key->hdr_name);
		return FALSE;
	}

	if((backend->old_guid == NULL) || (strcmp(backend->old_guid,backend->guid)!=0))
	{
		fts_backend_xapian_oldbox(backend);
		backend->old_guid = i_strdup(backend->guid);
		backend->old_boxname = i_strdup(backend->boxname);
		if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Start indexing '%s' (%s)",backend->boxname,backend->db);
	}

	const char * field=key->hdr_name;
	const char * type = key->body_content_type;
	const char * disposition = key->body_content_disposition;

	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: New part (Header=%s,Type=%s,Disposition=%s)",field,type,disposition);

	// Verify content-type

	if(key->type == FTS_BACKEND_BUILD_KEY_BODY_PART_BINARY)
	{
		if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: Skipping binary part of type '%s'",type);
		return FALSE;
	}

	if((type != NULL) && (strncmp(type,"text",4)!=0) && ((disposition==NULL) || ((strstr(disposition,"filename=")==NULL) && (strstr(disposition,"attachment")==NULL))))
	{
		if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: Non-binary & non-text part of type '%s'",type);
		return FALSE;
	}

	// Verify content-disposition
	ctx->isattachment=false;
	if((disposition != NULL) && ((strstr(disposition,"filename=")!=NULL) || (strstr(disposition,"attachment")!=NULL)))
	{
		if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: Found part as attachment of type '%s' and disposition '%s'",type,disposition);
		ctx->isattachment=true;
	}

	// Fill-in field
	if(field==NULL)
	{
		field="body";
	}

	long i=0,j=strlen(field);
	std::string f2;
	while(i<j)
	{
		if((field[i]>' ') && (field[i]!='"') && (field[i]!='\'') && (field[i]!='-'))
		{
			f2+=tolower(field[i]);
		}
		i++;
	}
	ctx->tbi_field=i_strdup(f2.c_str());

	i=0;
	while((i<HDRS_NB) && (strcmp(ctx->tbi_field,hdrs_emails[i])!=0))
	{
		i++;
	}
	if(i>=HDRS_NB)
	{
		if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Unknown header '%s' of part",ctx->tbi_field);
		i_free(ctx->tbi_field);
		ctx->tbi_field=NULL;
		return FALSE;
	}

	switch (key->type)
	{
		case FTS_BACKEND_BUILD_KEY_HDR:
		case FTS_BACKEND_BUILD_KEY_MIME_HDR:
			ctx->tbi_isfield=true;
			ctx->tbi_uid=key->uid;
			break;
		case FTS_BACKEND_BUILD_KEY_BODY_PART:
			ctx->tbi_uid=key->uid;
			break;
		case FTS_BACKEND_BUILD_KEY_BODY_PART_BINARY:
		default:
			return FALSE;
	}

	if((ctx->tbi_uid>0) && (ctx->tbi_uid != backend->lastuid))
        {
		long fri = fts_backend_xapian_test_memory();
		if(fri>=0)
        	{
                	i_warning("FTS Xapian: Warning Free memory %ld MB < %ld MB minimum",long(fri/1024.0),fts_xapian_settings.lowmemory);
			fts_backend_xapian_commitdocs(backend,"Low memory indexing");
                	fts_backend_xapian_release(backend,"Low memory indexing", 0, false);
        	}
                else if(backend->qsize>=XAPIAN_ADDED_DOCS)
                {
                        if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Refreshing after %ld updates (vs %ld) over %ld added docs ...", backend->qsize, XAPIAN_ADDED_DOCS,backend->total_added_docs);
			fts_backend_xapian_commitdocs(backend,"refreshing max docs");
                        fts_backend_xapian_release(backend,"refreshing max docs", fts_backend_xapian_current_time(), false);
                }

		backend->qcur = -1;
                backend->lastuid = ctx->tbi_uid;
        }

	return TRUE;
}

static void fts_backend_xapian_update_unset_build_key(struct fts_backend_update_context *_ctx)
{
	struct xapian_fts_backend_update_context *ctx = (struct xapian_fts_backend_update_context *)_ctx;

	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_update_unset_build_key");

	if(ctx->tbi_field!=NULL)
	{
		i_free(ctx->tbi_field);
	}
	ctx->tbi_uid=0;
	ctx->tbi_field=NULL;
}

static int fts_backend_xapian_refresh(struct fts_backend * _backend)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_refresh");

	struct xapian_fts_backend *backend = (struct xapian_fts_backend *) _backend;

	fts_backend_xapian_commitdocs(backend,"refresh");
	fts_backend_xapian_release(backend,"refresh", 0, true);

	return 0;
}

static int fts_backend_xapian_update_build_more(struct fts_backend_update_context *_ctx, const unsigned char *data, size_t size)
{
	struct xapian_fts_backend_update_context *ctx = (struct xapian_fts_backend_update_context *)_ctx;
	struct xapian_fts_backend *backend = (struct xapian_fts_backend *) ctx->ctx.backend;

	if(ctx->tbi_uid<1) return 0;

	if(data == NULL) return 0;

	icu::StringPiece sp_d((const char *)data,(int32_t )size);
	icu::UnicodeString d2 = icu::UnicodeString::fromUTF8(sp_d);
	
	if(d2.length() < fts_xapian_settings.partial) return 0;

	if(fts_backend_xapian_index(backend,ctx->tbi_field, &d2)) return 0;
        	
	i_error("FTS Xapian: Buildmore: Error Index");
	return -1;
}

static int fts_backend_xapian_optimize_callback(void *data, int argc, char **argv, char **azColName)
{
	(void) azColName;
	if (argc != 1) {
		i_error("FTS Xapian: fts_backend_xapian_optimize_callback called with %d!=1 arguments", argc);
		return -1;
	}
	uint32_t uid = atol(argv[0]);
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_optimize_callback : Adding %d",uid);
	std::vector<uint32_t> * uids = (std::vector<uint32_t> *) data;
	uids->push_back(uid);
	return 0;
}
	
static int fts_backend_xapian_optimize(struct fts_backend *_backend)
{
	struct xapian_fts_backend *backend = (struct xapian_fts_backend *) _backend;

	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_optimize '%s'",backend->path);

	struct stat sb;
	if(!( (stat(backend->path, &sb)==0) && S_ISDIR(sb.st_mode)))
	{
		i_error("FTS Xapian: Optimize(0) Index folder inexistent");
		return -1;
	}

	Xapian::WritableDatabase * db = NULL;
	sqlite3 * expdb = NULL;
	DIR* dirp = opendir(backend->path);
	struct dirent * dp;
	char *s;
	uint32_t uid;
	int ret=0;
	std::vector<uint32_t> uids(0);
	char *zErrMsg = 0;
	XResultSet * result = NULL;
	Xapian::docid docid =0;

	while ((dp = readdir(dirp)) != NULL)
	{
		if((dp->d_type == DT_DIR) && (strncmp(dp->d_name,"db_",3)==0))
		{
			uids.clear();
			s = i_strdup_printf("%s/%s_exp.db",backend->path,dp->d_name);
			if(fts_xapian_settings.verbose>0) i_info("Optimize (1) %s : Checking expunges",s);
			if(sqlite3_open(s,&expdb) == SQLITE_OK)
			{
				if(fts_xapian_settings.verbose>0) i_info("Optimize (1b) Executing %s",createTable);
				if(sqlite3_exec(expdb,createTable,NULL,0,&zErrMsg) != SQLITE_OK )
				{
					i_error("FTS Xapian: Optimize (2) Can not create table (%s) : %s",createTable,zErrMsg);
					sqlite3_free(zErrMsg);
					ret=-1;
				}
				else
				{
					if(fts_xapian_settings.verbose>0) i_info("Optimize (1c) Executing %s",selectUIDs);
					if(sqlite3_exec(expdb,selectUIDs,fts_backend_xapian_optimize_callback,&uids,&zErrMsg) != SQLITE_OK)	
					{
						i_error("FTS Xapian: Optimize (3) Can not select IDs (%s) : %s",selectUIDs,zErrMsg);
						sqlite3_free(zErrMsg);
						ret =-1;
					}
				}
				i_free(s);
				s = i_strdup_printf("%s/%s",backend->path,dp->d_name);
				if(fts_xapian_settings.verbose>0) i_info("Optimize (4) Opening Xapian DB (%s)",s);
				try
				{
					db = new Xapian::WritableDatabase(s,Xapian::DB_CREATE_OR_OPEN | Xapian::DB_RETRY_LOCK | Xapian::DB_BACKEND_GLASS | Xapian::DB_NO_SYNC);
					for(uint32_t n=0;n<uids.size();n++)
					{
						uid=uids[n];
						if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Optimize (5) Removing DOC UID=%d",uid);
						XQuerySet * xq = new XQuerySet();
						char *u = i_strdup_printf("%d",uid);
						xq->add("uid",u);
						i_free(u);
						result=fts_backend_xapian_query(db,xq,1);
						docid=0;
						if((result!=NULL) && (result->size>0))
						{
							try
							{
								docid = result->data[0];
								if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Optimize (5) Removing DOC UID=%d DOCID=%d",uid,docid);
								db->delete_document(docid);
							}
							catch(Xapian::Error e)
							{
								i_error("FTS Xapian: Optimize (6) %s",e.get_msg().c_str());
							}
						}
						else if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Optimize UID=%d (DOCID=%d) inexistent",uid,docid);
						if(result!=NULL) { delete(result); result=NULL; }
						delete(xq);
						u = i_strdup_printf("delete from docs where ID=%d",uid);
						if (sqlite3_exec(expdb,u,NULL,0,&zErrMsg) != SQLITE_OK )
						{
							i_error("FTS Xapian : Optimize Sqlite error %s",zErrMsg);
							sqlite3_free(zErrMsg);
						}
						i_free(u);
					}
					db->commit();
					db->close();
					delete(db);
				}
				catch(Xapian::Error e)
				{
					i_error("FTS Xapian: Optimize (6) %s",e.get_msg().c_str());
				}
				sqlite3_close(expdb);
			}
			i_free(s);
		}
	}
	closedir(dirp);
	return ret;
}

static int fts_backend_xapian_rescan(struct fts_backend *_backend)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_rescan");

	struct xapian_fts_backend *backend = (struct xapian_fts_backend *) _backend;

	struct stat sb;
	if(!( (stat(backend->path, &sb)==0) && S_ISDIR(sb.st_mode)))
	{
		i_error("FTS Xapian: Index folder inexistent");
		return -1;
	}

	DIR* dirp = opendir(backend->path);
	struct dirent * dp;
	char *s,*s2;
	while ((dp = readdir(dirp)) != NULL)
	{
		s = i_strdup_printf("%s/%s",backend->path,dp->d_name);

		if((dp->d_type == DT_REG) && (strncmp(dp->d_name,"expunge_",8)==0))
		{
			if(fts_xapian_settings.verbose>0) i_info("Removing[1] %s",s);
			remove(s);
		}
		else if((dp->d_type == DT_DIR) && (strncmp(dp->d_name,"db_",3)==0))
		{
			DIR * d2 = opendir(s);
			struct dirent *dp2;
			while ((dp2 = readdir(d2)) != NULL)
			{
				s2 = i_strdup_printf("%s/%s",s,dp2->d_name);
				if(dp2->d_type == DT_REG)
				{
					if(fts_xapian_settings.verbose>0) i_info("Removing[2] %s",s2);
					remove(s2);
				}
				i_free(s2);
			}
			closedir(d2);
			if(fts_xapian_settings.verbose>0) i_info("Removing dir %s",s);
			remove(s);
		}
		i_free(s);
	}
	closedir(dirp);

	return 0;
}

static int fts_backend_xapian_lookup(struct fts_backend *_backend, struct mailbox *box, struct mail_search_arg *args, enum fts_lookup_flags flags, struct fts_result *result)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_lookup");

	struct xapian_fts_backend *backend = (struct xapian_fts_backend *) _backend;

	if(fts_backend_xapian_set_box(backend, box)<0) return -1;

	long current_time = fts_backend_xapian_current_time();

	Xapian::Database * dbr;

	i_array_init(&(result->maybe_uids),0);
	i_array_init(&(result->scores),0);

	if(!fts_backend_xapian_open_readonly(backend, &dbr))
	{
		i_array_init(&(result->definite_uids),0);
		return 0;
	}

	XQuerySet * qs;

	if((flags & FTS_LOOKUP_FLAG_AND_ARGS) != 0)
	{
		if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: FLAG=AND");
		qs = new XQuerySet(Xapian::Query::OP_AND,fts_xapian_settings.partial);
	}
	else
	{
		if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: FLAG=OR");
		qs = new XQuerySet(Xapian::Query::OP_OR,fts_xapian_settings.partial);
	}

	fts_backend_xapian_build_qs(qs,args);

	XResultSet * r=fts_backend_xapian_query(dbr,qs);

	long n=r->size;
	if(fts_xapian_settings.verbose>0) { i_info("FTS Xapian: QUery '%s' -> %ld results",qs->get_string().c_str(),n); }

	i_array_init(&(result->definite_uids),r->size);

	uint32_t uid;
	for(long i=0;i<n;i++)
	{
		try
		{
			uid=Xapian::sortable_unserialise(dbr->get_document(r->data[i]).get_value(1));
			seq_range_array_add(&result->definite_uids, uid);
		}
		catch(Xapian::Error e)
		{
			i_error("FTS Xapian: %s",e.get_msg().c_str());
		}
	}
	delete(r);
	delete(qs);

	dbr->close();
	delete(dbr);

	/* Performance calc */
	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: %ld results in %ld ms",n,fts_backend_xapian_current_time() - current_time);

	return 0;
}

static int fts_backend_xapian_lookup_multi(struct fts_backend *_backend, struct mailbox *const boxes[], struct mail_search_arg *args, enum fts_lookup_flags flags, struct fts_multi_result *result)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_lookup_multi");

	ARRAY(struct fts_result) box_results;

	struct fts_result *box_result;
	int i;

	p_array_init(&box_results, result->pool, 0);
	for (i = 0; boxes[i] != NULL; i++)
	{
		box_result = array_append_space(&box_results);
		box_result->box = boxes[i];
		if(fts_backend_xapian_lookup(_backend, boxes[i], args, flags, box_result)<0)
		{
			void* p=&box_results;
			p_free(result->pool, p);
			return -1;
		}
	}

	array_append_zero(&box_results);
	result->box_results = array_idx_modifiable(&box_results, 0);

	return 0;
}

struct fts_backend fts_backend_xapian =
{
	.name = "xapian",
	.flags = FTS_BACKEND_FLAG_BUILD_FULL_WORDS,
	.v = {
		.alloc = fts_backend_xapian_alloc,
		.init = fts_backend_xapian_init,
		.deinit = fts_backend_xapian_deinit,
		.get_last_uid = fts_backend_xapian_get_last_uid,
		.update_init = fts_backend_xapian_update_init,
		.update_deinit = fts_backend_xapian_update_deinit,
		.update_set_mailbox = fts_backend_xapian_update_set_mailbox,
		.update_expunge = fts_backend_xapian_update_expunge,
		.update_set_build_key = fts_backend_xapian_update_set_build_key,
		.update_unset_build_key = fts_backend_xapian_update_unset_build_key,
		.update_build_more = fts_backend_xapian_update_build_more,
		.refresh = fts_backend_xapian_refresh,
		.rescan = fts_backend_xapian_rescan,
		.optimize = fts_backend_xapian_optimize,
		.can_lookup = fts_backend_default_can_lookup,
		.lookup = fts_backend_xapian_lookup,
		.lookup_multi = fts_backend_xapian_lookup_multi,
	}
};
