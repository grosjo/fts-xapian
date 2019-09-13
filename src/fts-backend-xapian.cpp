/* Copyright (c) 2019 Joan Moreau <jom@grosjo.net>, see the included COPYING file */

#include <xapian.h>
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
#define XAPIAN_WILDCARD "wldcrd"

#define HDRS_NB 9
static const char * hdrs_emails[HDRS_NB] = { "uid", "subject", "from", "to",  "cc",  "bcc",  "message-id", "body", ""  };
static const char * hdrs_xapian[HDRS_NB] = { "Q",   "S",       "A",    "XTO", "XCC", "XBCC", "XMID",       "XBDY", "XBDY" }; 


struct xapian_fts_backend 
{
        struct fts_backend backend;
        char * path;

        struct mailbox *box;
	char * oldbox;

        char * db;
	Xapian::WritableDatabase * dbw;
	Xapian::Database * dbr;
        long partial,full;
	long nb_updates;

	long perf_pt;
	long perf_nb;
	long perf_uid;
	long perf_dt;
};

struct xapian_fts_backend_update_context
{
        struct fts_backend_update_context ctx;
        std::string tbi_field;
        bool tbi_isfield;
        uint32_t tbi_uid;
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
	unsigned int len;

        backend->dbw = NULL;
        backend->dbr = NULL;
        backend->db = NULL;
        backend->box = NULL;
	backend->path = NULL;
	backend->oldbox = NULL;

	env = mail_user_plugin_getenv(_backend->ns->user, "fts_xapian");
	if (env == NULL)
		return 0;

	for (tmp = t_strsplit_spaces(env, " "); *tmp != NULL; tmp++) 
	{
        	if (str_begins(*tmp, "partial=") && (str_to_uint(*tmp + 8, &len)>=0))
		{
			backend->partial=len;
		}	
        	else if (str_begins(*tmp, "full=") && (str_to_uint(*tmp + 5, &len)>=0))
		{
			backend->full=len;
		}
        	else 
		{
            		i_error("FTS Xapian: Invalid setting: %s", *tmp);
            		return -1;
        	}
    	}
    	if(backend->partial<2) 
    	{
        	i_error("FTS Xapian: 'partial' can not be null (try partial=2)");
        	return -1;
    	}
    	if(backend->full<1) 
    	{
        	i_error("FTS Xapian: 'full' can not be null (try full=20)");
        	return -1;
    	}
    	if(backend->partial > backend->full) 
    	{
        	i_error("FTS Xapian: 'full' must be equal or greater than 'partial'");
        	return -1;
    	}
    	if(backend->full > 50) 
    	{
        	i_error("FTS Xapian: 'full' above 50 is not realistic");
        	return -1;
    	}

	const char * path = mailbox_list_get_root_forced(_backend->ns->list, MAILBOX_LIST_PATH_TYPE_INDEX);
	long l=strlen(path)+strlen(XAPIAN_FILE_PREFIX)+2;
	backend->path = (char *)i_malloc(l*sizeof(char));
	snprintf(backend->path,l,"%s/%s",path,XAPIAN_FILE_PREFIX);

	struct stat sb;
	if(!( (stat(backend->path, &sb)==0) && S_ISDIR(sb.st_mode)))
	{
		if (mailbox_list_mkdir_root(backend->backend.ns->list, backend->path, MAILBOX_LIST_PATH_TYPE_INDEX) < 0)
		{
			i_error("FTS Xapian: can not create '%s'",backend->path);
                	return -1;
		}
	}

	return 0;
}

static void fts_backend_xapian_deinit(struct fts_backend *_backend)
{
	struct xapian_fts_backend *backend =
		(struct xapian_fts_backend *)_backend;

	fts_backend_xapian_unset_box(backend);

	if(backend->oldbox != NULL) i_free(backend->oldbox);
	backend->oldbox = NULL;
	if(backend->path != NULL) i_free(backend->path);
	backend->path = NULL;

	i_free(backend);
}


static int fts_backend_xapian_get_last_uid(struct fts_backend *_backend,
			       struct mailbox *box, uint32_t *last_uid_r)
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
		i_error("FTS Xapian: fts_backend_xapian_get_last_uid %s",backend->box->name);
		i_error("FTS Xapian: %s",e.get_msg().c_str());
		return -1;
	}
	//i_info("Get last UID of %s = %d",backend->box->name,*last_uid_r);
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
	
	ctx->ctx.backend = NULL;

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

	if(!fts_backend_xapian_check_write(backend))
	{
		i_error("FTS Xapian: Expunge UID=%d: Can not open db",uid);
		return ;
	}

    	try
	{
		char s[30];
		snprintf(s,30,"Q%d",uid);	
        	backend->dbw->delete_document(s);
	}
	catch(Xapian::Error e)
	{
		i_error("FTS Xapian: %s",e.get_msg().c_str());
	}
}

static bool fts_backend_xapian_update_set_build_key(struct fts_backend_update_context *_ctx, const struct fts_backend_build_key *key)
{
	struct xapian_fts_backend_update_context *ctx =
		(struct xapian_fts_backend_update_context *)_ctx;

	struct xapian_fts_backend *backend =
                (struct xapian_fts_backend *)ctx->ctx.backend;

	if(backend->box == NULL)
	{
		i_warning("FTS Xapian: Build key %s with no mailbox",key->hdr_name);
		return FALSE;
	}

	const char * field=key->hdr_name;

	if(field==NULL)
	{
		field="body";
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
		i_info("FTS Xapian: Partial indexing '%s' (%ld msgs in %ld ms, rate: %.1f)",backend->box->name,backend->perf_nb,dt,r);
	}
	/* End Performance calculator*/

	long i=0,j=strlen(field);
	std::string f2;
	while(i<j)
	{
        	if(field[i]>' ')
        	{
			f2+=tolower(field[i]);
		}
        	i++;
        }

	switch (key->type) 
    	{
    		case FTS_BACKEND_BUILD_KEY_HDR:
	    	case FTS_BACKEND_BUILD_KEY_MIME_HDR:
            		ctx->tbi_isfield=true;
            		ctx->tbi_field=f2;
            		ctx->tbi_uid=key->uid;
            		break;
        	case FTS_BACKEND_BUILD_KEY_BODY_PART:
            		ctx->tbi_field=f2;
            		ctx->tbi_isfield=false;
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

    	ctx->tbi_uid=0;
}

static int fts_backend_xapian_refresh(struct fts_backend * _backend)
{

        struct xapian_fts_backend *backend =
                (struct xapian_fts_backend *) _backend;

        if(backend->dbw !=NULL)
        {
                backend->dbw->commit();
		backend->dbw->close();
                delete(backend->dbw);
                backend->dbw=NULL;
                backend->nb_updates=0;
        }
        if(backend->dbr !=NULL)
        {
		backend->dbr->close();
                delete(backend->dbr);
                backend->dbr = NULL;
        }
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
	if(size<1) return 0;

	char * s = (char*)i_malloc(sizeof(char)*(size+1));
	strncpy(s,(char *)data,size);
	s[size]=0;


	if(!fts_backend_xapian_check_write(backend))
	{
		i_error("FTS Xapian: Buildmore: Can not open db");
		return -1;
	}

    	if(ctx->tbi_isfield)
    	{
        	if(!fts_backend_xapian_index_hdr(backend->dbw,ctx->tbi_uid,ctx->tbi_field.c_str(), s, backend->partial,backend->full))
        	{
            		return -1;
        	}
    	}
    	else
    	{
		if(!fts_backend_xapian_index_text(backend->dbw,ctx->tbi_uid,ctx->tbi_field.c_str(), s))
        	{
            		return -1;
        	}
    	}

        if((backend->oldbox == NULL) || (strcmp(backend->oldbox,backend->box->name)!=0))
        {
                fts_backend_xapian_oldbox(backend);
                backend->oldbox = i_strdup(backend->box->name);
                //i_info("Start indexing '%s' (%s)",backend->box->name,backend->db);
        }

	backend->nb_updates++;
	if(backend->nb_updates>XAPIAN_COMMIT_LIMIT) 
	{
//		i_info("refreshing...");
		fts_backend_xapian_refresh( ctx->ctx.backend);
	}
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

	return ftw(backend->path,fts_backend_xapian_empty_db,1024);
}

static int fts_backend_xapian_lookup(struct fts_backend *_backend, struct mailbox *box, struct mail_search_arg *args, enum fts_lookup_flags flags, struct fts_result *result)
{
	struct xapian_fts_backend *backend =
                (struct xapian_fts_backend *) _backend;

	if(fts_backend_xapian_set_box(backend, box)<0)
		return -1;

	if(!fts_backend_xapian_check_read(backend))
        {
                i_error("FTS Xapian: Lookup: Can not open db RO");
                return -1;
        }

	/* Performance calc */
	struct timeval tp;
        gettimeofday(&tp, NULL);
        long dt = tp.tv_sec * 1000 + tp.tv_usec / 1000;


	if(backend->dbw !=NULL)
        {
		//i_info("FTS Xapian: Committing changes %s",backend->box->name);
                backend->dbw->commit();
	}

	bool is_and=false;

	if((flags & FTS_LOOKUP_FLAG_AND_ARGS) != 0)
	{
		i_debug("FTS Xapian: FLAG=AND");
		is_and=true;
	}
	else
	{
		i_debug("FTS Xapian: FLAG=OR");
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
        gettimeofday(&tp, NULL);
        dt = tp.tv_sec * 1000 + tp.tv_usec / 1000 - dt;
	i_debug("FTS Xapian: %ld results in %ld ms",n,dt);

	return 0;
}

 

struct fts_backend fts_backend_xapian = {
	.name = "xapian",
	.flags = FTS_BACKEND_FLAG_NORMALIZE_INPUT,
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
		NULL,
		fts_backend_default_can_lookup,
		fts_backend_xapian_lookup,
		NULL,
        	NULL
	}
};

