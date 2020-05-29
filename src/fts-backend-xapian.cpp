/* Copyright (c) 2019 Joan Moreau <jom@grosjo.net>, see the included COPYING file */

#include <xapian.h>
#include <sqlite3.h>
#include <cstdio>
extern "C" {
#include "fts-xapian-plugin.h"
}
#include <ftw.h>
#include <unicode/unistr.h>
#include <sys/time.h>

#define XAPIAN_FILE_PREFIX "xapian-indexes"
#define XAPIAN_TERM_SIZELIMIT 245
#define XAPIAN_COMMIT_LIMIT 1000
#define XAPIAN_COMMIT_TIMEOUT 300
#define XAPIAN_WILDCARD "wldcrd"
#define XAPIAN_EXPUNGE_SIZE 5

#define HDRS_NB 10
static const char * hdrs_emails[HDRS_NB] = { "uid", "subject", "from", "to",  "cc",  "bcc",  "messageid", "listid", "body", ""  };
static const char * hdrs_xapian[HDRS_NB] = { "Q",   "S",       "A",    "XTO", "XCC", "XBCC", "XMID",      "XLIST",  "XBDY", "XBDY" }; 

static int verbose = 0;

struct xapian_fts_backend
{
        struct fts_backend backend;
        char * path;
	long partial,full;
	bool attachments;

	char * guid;
	char * boxname;
	char * db;

	char * old_guid;
	char * old_boxname;

	Xapian::Database * dbr;
	Xapian::WritableDatabase * dbw;
	sqlite3 * db_expunge;

	long commit_updates;
	long commit_time;

	long perf_pt;
	long perf_nb;
	long perf_uid;
	long perf_dt;

};

struct xapian_fts_expunge
{
	Xapian::docid did[XAPIAN_EXPUNGE_SIZE];
	long uid[XAPIAN_EXPUNGE_SIZE];
	int index;
};

struct xapian_fts_backend_update_context
{
        struct fts_backend_update_context ctx;
        char * tbi_field=NULL;
        bool tbi_isfield;
        uint32_t tbi_uid=0;
};

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
	struct xapian_fts_backend *backend =
		(struct xapian_fts_backend *)_backend;
	const char *const *tmp, *env;
	long len;

	backend->dbw = NULL;
	backend->dbr = NULL;
	backend->db = NULL;
	backend->guid = NULL;
	backend->path = NULL;
	backend->old_guid = NULL;
	backend->old_boxname = NULL;
	backend->attachments = false;
	backend->db_expunge = NULL;
	verbose = 0;
	backend->partial = 0;
	backend->full = 0;

	env = mail_user_plugin_getenv(_backend->ns->user, "fts_xapian");
	if (env == NULL) 
	{
		i_error("FTS Xapian: missing configuration");
		return -1;
	}

	for (tmp = t_strsplit_spaces(env, " "); *tmp != NULL; tmp++)
	{
        	if (strncmp(*tmp, "partial=",8)==0)
		{
			len=atol(*tmp + 8);
			if(len>0) backend->partial=len;
		}
        	else if (strncmp(*tmp,"full=",5)==0)
		{
			len=atol(*tmp + 5);
			if(len>0) backend->full=len;
		}
		else if (strncmp(*tmp,"verbose=",8)==0)
		{
			len=atol(*tmp + 8);
			if(len>0) verbose=len;
		}
		else if (strncmp(*tmp,"attachments=",12)==0)
		{
			if(atol(*tmp + 12)>0) backend->attachments=true;
		}
		else 
		{
            		i_error("FTS Xapian: Invalid setting: %s", *tmp);
            		return -1;
        	}
    	}

    	if(backend->partial < 2)
    	{
        	i_error("FTS Xapian: 'partial' parameter is incorrect (%ld). Try 'partial=2'",backend->partial);
        	return -1;
    	}
    	if(backend->full<1)
    	{
        	i_error("FTS Xapian: 'full' parameter is incorrect (%ld). Try 'full=20'",backend->full);
        	return -1;
    	}
    	if(backend->partial > backend->full)
    	{
        	i_error("FTS Xapian: 'full' parameter must be equal or greater than 'partial'");
        	return -1;
    	}
    	if(backend->full > 50)
    	{
        	i_error("FTS Xapian: 'full' parameter above 50 is not realistic");
        	return -1;
    	}

	const char * path = mailbox_list_get_root_forced(_backend->ns->list, MAILBOX_LIST_PATH_TYPE_INDEX);
	backend->path = i_strconcat(path, "/" XAPIAN_FILE_PREFIX, NULL);

	struct stat sb;
	if(!( (stat(backend->path, &sb)==0) && S_ISDIR(sb.st_mode)))
	{
		if (mailbox_list_mkdir_root(backend->backend.ns->list, backend->path, MAILBOX_LIST_PATH_TYPE_INDEX) < 0)
		{
			i_error("FTS Xapian: can not create '%s'",backend->path);
                	return -1;
		}
	}

	if(verbose>0) i_info("FTS Xapian: Starting with partial=%ld full=%ld attachments=%d verbose=%d",backend->partial,backend->full,backend->attachments,verbose);

	return 0;
}

static void fts_backend_xapian_deinit(struct fts_backend *_backend)
{
	struct xapian_fts_backend *backend =
		(struct xapian_fts_backend *)_backend;

	if(verbose>1) i_info("Deinit %s",backend->path);

	fts_backend_xapian_unset_box(backend);

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
	struct xapian_fts_backend *backend =
		(struct xapian_fts_backend *)_backend;

	*last_uid_r = 0;

	if(fts_backend_xapian_set_box(backend, box) < 0)
	{
		i_error("FTS Xapian: get_last_uid: Can not select mailbox '%s'",box->name);
		return -1;
	}

	if(!fts_backend_xapian_check_read(backend))
	{
		i_error("FTS Xapian: get_last_uid: can not open DB %s",backend->db);
		return -1;
	}

    	try
	{
		*last_uid_r = Xapian::sortable_unserialise(backend->dbr->get_value_upper_bound(1));
	}
	catch(Xapian::Error e)
	{
		i_error("FTS Xapian: fts_backend_xapian_get_last_uid for '%s' (%s)",backend->boxname,backend->guid);
		i_error("FTS Xapian: %s",e.get_msg().c_str());
		return -1;
	}

	if(verbose>0) i_info("FTS Xapian: Get last UID of %s (%s) = %d",backend->boxname,backend->guid,*last_uid_r);

        return 0;
}


static struct fts_backend_update_context * fts_backend_xapian_update_init(struct fts_backend *_backend)
{
	struct xapian_fts_backend_update_context *ctx;

	ctx = i_new(struct xapian_fts_backend_update_context, 1);
	ctx->ctx.backend = _backend;
	return &ctx->ctx;
}

static int fts_backend_xapian_update_deinit(struct fts_backend_update_context *_ctx)
{
	struct xapian_fts_backend_update_context *ctx =
		(struct xapian_fts_backend_update_context *)_ctx;
	struct xapian_fts_backend *backend =
		(struct xapian_fts_backend *)ctx->ctx.backend;

	if(verbose>1) i_info("Update deinit %s",backend->path);

	i_free(ctx);

	return 0;
}

static void fts_backend_xapian_update_set_mailbox(struct fts_backend_update_context *_ctx, struct mailbox *box)
{
	struct xapian_fts_backend_update_context *ctx =
		(struct xapian_fts_backend_update_context *)_ctx;
	struct xapian_fts_backend *backend =
		(struct xapian_fts_backend *)ctx->ctx.backend;

    	fts_backend_xapian_set_box(backend, box);
}

static void fts_backend_xapian_update_expunge(struct fts_backend_update_context *_ctx, uint32_t uid)
{
	struct xapian_fts_backend_update_context *ctx =
		(struct xapian_fts_backend_update_context *)_ctx;
	struct xapian_fts_backend *backend =
		(struct xapian_fts_backend *)ctx->ctx.backend;

	if(!fts_backend_xapian_check_read(backend))
	{
		i_error("FTS Xapian: Expunge UID=%d: Can not open db",uid);
		return ;
	}

    	try
	{
		if(verbose>0) i_info("FTS Xapian: Expunge UID=%d",uid);

		XQuerySet * xq = new XQuerySet();
		const char *u = t_strdup_printf("%d",uid);
		xq->add("uid",u);

		XResultSet * result=fts_backend_xapian_query(backend->dbr,xq,1);
		
		if(result->size>0)
		{
			Xapian::docid docid=result->data[0];
			if(docid>0) fts_backend_xapian_add_expunge(backend,docid,uid);
		}

		delete(result);
		delete(xq);
	}
	catch(Xapian::Error e)
	{
		i_error("FTS Xapian: (deleting) %s",e.get_msg().c_str());
	}
}

static bool fts_backend_xapian_update_set_build_key(struct fts_backend_update_context *_ctx, const struct fts_backend_build_key *key)
{
	struct xapian_fts_backend_update_context *ctx =
		(struct xapian_fts_backend_update_context *)_ctx;

	struct xapian_fts_backend *backend =
                (struct xapian_fts_backend *)ctx->ctx.backend;

	ctx->tbi_isfield=false;
        ctx->tbi_uid=0;

	if(backend->guid == NULL)
	{
		if(verbose>0) i_warning("FTS Xapian: Build key %s with no mailbox",key->hdr_name);
		return FALSE;
	}

	if((backend->old_guid == NULL) || (strcmp(backend->old_guid,backend->guid)!=0))
        {
		fts_backend_xapian_oldbox(backend);
		backend->old_guid = i_strdup(backend->guid);
		backend->old_boxname = i_strdup(backend->boxname);
		if(verbose>0) i_info("FTS Xapian: Start indexing '%s' (%s)",backend->boxname,backend->guid);
	}

	/* Performance calculator*/
	if( backend->perf_uid != key->uid )
	{
		backend->perf_nb++;
		backend->perf_uid = key->uid;
	}
	if((backend->perf_nb - backend->perf_pt)>=200)
	{
		backend->perf_pt = backend->perf_nb;
		struct timeval tp;
		gettimeofday(&tp, NULL);
		long dt = tp.tv_sec * 1000 + tp.tv_usec / 1000 - backend->perf_dt;
		double r=0;
		if(dt>0)
		{
			r=backend->perf_nb*1000.0;
			r=r/dt;
		}
		if(verbose>0) i_info("FTS Xapian: Partial indexing '%s' (%ld msgs in %ld ms, rate: %.1f)",backend->boxname,backend->perf_nb,dt,r);
	}
	/* End Performance calculator*/

	const char * field=key->hdr_name;
	const char * type = key->body_content_type;
	const char * disposition = key->body_content_disposition;

	if(verbose>1) i_info("FTS Xapian: New part (Header=%s,Type=%s,Disposition=%s)",field,type,disposition);

	// Verify content-type
	if((type != NULL) && (strncmp(type,"text",4)!=0))
	{
		if(verbose>0) i_info("FTS Xapian: Skipping part of type '%s'",type);
		return FALSE;
	}

	// Verify content-disposition
	if((disposition != NULL) && (!backend->attachments) && ((strstr(disposition,"filename=")!=NULL) || (strstr(disposition,"attachment")!=NULL)))
	{
		if(verbose>0) i_info("FTS Xapian: Skipping part of type '%s' and disposition '%s'",type,disposition);
		return FALSE;
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
		if(verbose>1) i_info("FTS Xapian: Unknown header '%s'",ctx->tbi_field); 
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
			i_unreached();
	}

	return TRUE;
}

static void fts_backend_xapian_update_unset_build_key(struct fts_backend_update_context *_ctx)
{
	struct xapian_fts_backend_update_context *ctx =
		(struct xapian_fts_backend_update_context *)_ctx;

	if(ctx->tbi_field!=NULL)
	{
		i_free(ctx->tbi_field);
	}
    	ctx->tbi_uid=0;
	ctx->tbi_field=NULL;
}

static int fts_backend_xapian_refresh(struct fts_backend * _backend)
{
	struct xapian_fts_backend *backend =
		(struct xapian_fts_backend *) _backend;

	struct timeval tp;
        gettimeofday(&tp, NULL);
        long current_time = tp.tv_sec * 1000 + tp.tv_usec / 1000;

	fts_backend_xapian_release(backend,"refresh");
	fts_backend_xapian_expunge(backend,"refresh");
	fts_backend_xapian_commit(backend,"refresh", current_time);
        
	return 0;
}

static int fts_backend_xapian_update_build_more(struct fts_backend_update_context *_ctx, const unsigned char *data, size_t size)
{
	struct xapian_fts_backend_update_context *ctx =
		(struct xapian_fts_backend_update_context *)_ctx;
	struct xapian_fts_backend *backend =
                (struct xapian_fts_backend *) ctx->ctx.backend;

	if(ctx->tbi_uid<1) return 0;

	if(data == NULL) return 0;

	icu::StringPiece sp_d((const char *)data,(int32_t )size);
	icu::UnicodeString d2 = icu::UnicodeString::fromUTF8(sp_d);
	if(d2.length() < backend->partial) return 0;

	if(!fts_backend_xapian_check_write(backend))
	{
		i_error("FTS Xapian: Buildmore: Can not open db");
		return -1;
	}

	bool ok=true;

    	if(ctx->tbi_isfield)
    	{
        	ok=fts_backend_xapian_index_hdr(backend->dbw,ctx->tbi_uid,ctx->tbi_field, &d2, backend->partial,backend->full);
    	}
    	else
    	{
		ok=fts_backend_xapian_index_text(backend->dbw,ctx->tbi_uid,ctx->tbi_field, &d2, backend->partial,backend->full);
    	}

	backend->commit_updates++;

	struct timeval tp;
	gettimeofday(&tp, NULL);
	long current_time = tp.tv_sec * 1000 + tp.tv_usec / 1000;

	if( (backend->commit_updates>XAPIAN_COMMIT_LIMIT) || ((current_time - backend->commit_time) > XAPIAN_COMMIT_TIMEOUT*1000) )
	{
		if(verbose>1) i_info("FTS Xapian: Refreshing after %ld ms and %ld updates...", current_time - backend->commit_time, backend->commit_updates);
		fts_backend_xapian_release(backend,"refreshing");
		fts_backend_xapian_expunge(backend,"refreshing");
		fts_backend_xapian_commit(backend,"refreshing", current_time);
	}
    	
	if(!ok) return -1;
	return 0;
}

static int fts_backend_xapian_optimize(struct fts_backend *_backend)
{
	struct xapian_fts_backend *backend =
		(struct xapian_fts_backend *) _backend;

	if(verbose>0) i_info("Optimize function");
	
	struct timeval tp;
        gettimeofday(&tp, NULL);
        long current_time = tp.tv_sec * 1000 + tp.tv_usec / 1000;
	
	fts_backend_xapian_expunge(backend,"optimize");
        fts_backend_xapian_commit(backend,"optimize",current_time);

	return 0;
}

static int fts_backend_xapian_rescan(struct fts_backend *_backend)
{
	struct xapian_fts_backend *backend =
                (struct xapian_fts_backend *) _backend;

	struct stat sb;
        if(!( (stat(backend->path, &sb)==0) && S_ISDIR(sb.st_mode)))
        {
		i_error("FTS Xapian: Index folder inexistent");
		return -1;
	}

	return ftw(backend->path,fts_backend_xapian_empty_db,512);
}

static int fts_backend_xapian_lookup(struct fts_backend *_backend, struct mailbox *box, struct mail_search_arg *args, enum fts_lookup_flags flags, struct fts_result *result)
{
	struct xapian_fts_backend *backend =
                (struct xapian_fts_backend *) _backend;

	if(fts_backend_xapian_set_box(backend, box)<0)
		return -1;

	/* Performance calc */
	struct timeval tp;
        gettimeofday(&tp, NULL);
        long current_time = tp.tv_sec * 1000 + tp.tv_usec / 1000;

	fts_backend_xapian_commit(backend,"lookup", current_time);

	if(!fts_backend_xapian_check_read(backend))
        {
                i_error("FTS Xapian: Lookup: Can not open db RO");
                return -1;
        }

	bool is_and=false;

	if((flags & FTS_LOOKUP_FLAG_AND_ARGS) != 0)
	{
		if(verbose>1) i_info("FTS Xapian: FLAG=AND");
		is_and=true;
	}
	else
	{
		if(verbose>1) i_info("FTS Xapian: FLAG=OR");
	}

	XQuerySet * qs = new XQuerySet(is_and,false,backend->partial);
	fts_backend_xapian_build_qs(qs,args);

	XResultSet * r=fts_backend_xapian_query(backend->dbr,qs);

	long n=r->size;

	i_array_init(&(result->definite_uids),r->size);
        i_array_init(&(result->maybe_uids),0);
	i_array_init(&(result->scores),0);

	uint32_t uid;
	for(long i=0;i<n;i++)
	{
		try
		{
			uid=Xapian::sortable_unserialise(backend->dbr->get_document(r->data[i]).get_value(1));
			seq_range_array_add(&result->definite_uids, uid);
		}
		catch(Xapian::Error e)
		{
			i_error("FTS Xapian: %s",e.get_msg().c_str());
		}
	}
	delete(r);
	delete(qs);

	/* Performance calc */
        if(verbose>0)
	{
		gettimeofday(&tp, NULL);
        	long dt = tp.tv_sec * 1000 + tp.tv_usec / 1000 - current_time;
		i_info("FTS Xapian: %ld results in %ld ms",n,dt);
	}

	return 0;
}

static int fts_backend_xapian_lookup_multi (struct fts_backend *_backend, struct mailbox *const boxes[], struct mail_search_arg *args, enum fts_lookup_flags flags, struct fts_multi_result *result)
{
	struct xapian_fts_backend *backend =
                (struct xapian_fts_backend *) _backend;

	ARRAY(struct fts_result) box_results;
	struct fts_result *box_result;
	int i;

	p_array_init(&box_results, result->pool, 0);
	for (i = 0; boxes[i] != NULL; i++) 
	{
		box_result = array_append_space(&box_results);
		box_result->box = boxes[i];
		if(fts_backend_xapian_lookup(_backend, boxes[i], args, flags, box_result)<1) return -1;
	}
	return 0;
}

struct fts_backend fts_backend_xapian = {
	.name = "xapian",
	.flags = FTS_BACKEND_FLAG_BUILD_FULL_WORDS,
	{
		fts_backend_xapian_alloc,
		fts_backend_xapian_init,
		fts_backend_xapian_deinit,
		fts_backend_xapian_get_last_uid,
		fts_backend_xapian_update_init,
		fts_backend_xapian_update_deinit,
		fts_backend_xapian_update_set_mailbox,
		fts_backend_xapian_update_expunge,
		fts_backend_xapian_update_set_build_key,
		fts_backend_xapian_update_unset_build_key,
		fts_backend_xapian_update_build_more,
		fts_backend_xapian_refresh,
		fts_backend_xapian_rescan,
		fts_backend_xapian_optimize,
		fts_backend_default_can_lookup,
		fts_backend_xapian_lookup,
		fts_backend_xapian_lookup_multi,
        	NULL
	}
};

