/* Copyright (c) 2019 Joan Moreau <jom@grosjo.net>, see the included COPYING file */

#include <xapian.h>
#include <thread>
#include <cstdio>
#include <vector>
#include <mutex>
#include <regex>
#include <chrono>
#include <cmath>
extern "C" {
#include "fts-xapian-plugin.h"
}
#include <dirent.h>
#include <filesystem>
#include <unicode/utypes.h>
#include <unicode/unistr.h>
#include <unicode/translit.h> 
#include <sys/time.h>
#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/vmmeter.h>
#endif

#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>

#include <syslog.h>

class XDoc;
class XDocsWriter;

struct xapian_fts_backend
{
	struct fts_backend backend;
	char * path;

	char * guid;
	char * boxname;

	char * xap_db;
	char * exp_db;
	char * version_file;
	char * dict_db;
	long dict_nb;

	sqlite3 * ddb;
	Xapian::WritableDatabase * dbw;
	long pending;

	char * old_guid;
	char * old_boxname;

        std::vector<XDoc *> docs;
	std::vector<XDocsWriter *> threads;
	std::timed_mutex mutex;
	std::unique_lock<std::timed_mutex> * mutex_t;
	unsigned int max_threads;

	long lastuid;
	long total_docs;
	long start_time;
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

	backend->xap_db = NULL;
	backend->exp_db = NULL;
	backend->dict_db = NULL;

	backend->docs.clear();
	backend->threads.clear();
	backend->total_docs =0;
	
	backend->lastuid = -1;

	backend->dbw = NULL;
	backend->ddb = NULL;
	backend->guid = NULL;
	backend->path = NULL;
	backend->old_guid = NULL;
	backend->old_boxname = NULL;
	if(fts_xapian_settings.maxthreads>0) 
	{
		backend->max_threads=fts_xapian_settings.maxthreads;
	}
	else
	{
		backend->max_threads = std::thread::hardware_concurrency()-1;
	}
	if(backend->max_threads<2) backend->max_threads = 2;

	struct fts_xapian_user *fuser = FTS_XAPIAN_USER_CONTEXT(_backend->ns->user);
	fts_xapian_settings = fuser->set;

	if(fts_backend_xapian_set_path(backend)<0) return -1;

	openlog("xapian-docswriter",0,LOG_MAIL);

        if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Starting version %s with partial=%d verbose=%d max_threads=%u lowmemory=%ld MB", XAPIAN_PLUGIN_VERSION, fts_xapian_settings.partial,fts_xapian_settings.verbose,backend->max_threads,fts_xapian_settings.lowmemory);

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

	closelog();
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
		i_error("FTS Xapian: GetLastUID: Can not open db RO (%s)",backend->xap_db);
		return 0;
	}

	try
	{
		*last_uid_r = Xapian::sortable_unserialise(dbr->get_value_upper_bound(1));
	}
	catch(Xapian::Error e)
	{
		i_warning("FTS Xapian: fts_backend_xapian_get_last_uid for '%s' (%s)",backend->boxname,backend->guid);
		i_warning("FTS Xapian: %s",e.get_msg().c_str());
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
	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Opening expunge DB(%s)",backend->exp_db);

	if(sqlite3_open_v2(backend->exp_db,&expdb,SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_READWRITE,NULL))
	{
		i_error("FTS Xapian: Expunging UID=%d : Can not open %s",uid,backend->exp_db);
		return;
	}
	char *zErrMsg = 0;
	char * u = i_strdup_printf(replaceExpUID,uid);
	if(sqlite3_exec(expdb,u,NULL,0,&zErrMsg) != SQLITE_OK)
	{
		i_error("FTS Xapian: Expunging (3) UID=%d : Can not add UID : %s",uid,zErrMsg);
		if(zErrMsg!=NULL) sqlite3_free(zErrMsg);
	}
	i_free(u);
	sqlite3_close(expdb);
	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian : Expunge done");
}

static bool fts_backend_xapian_update_set_build_key(struct fts_backend_update_context *_ctx, const struct fts_backend_build_key *key)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_update_set_build_key");

	struct xapian_fts_backend_update_context *ctx = (struct xapian_fts_backend_update_context *)_ctx;

	struct xapian_fts_backend *backend = (struct xapian_fts_backend *)ctx->ctx.backend;

	ctx->tbi_isfield=false;
	ctx->tbi_uid=0;
	ctx->tbi_field=NULL;

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
	}

	const char * type = key->body_content_type;
	const char * disposition = key->body_content_disposition;

	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: New part (Header=%s,Type=%s,Disposition=%s)",key->hdr_name,type,disposition);

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

	if(!fts_backend_xapian_sqlite3_dict_open(backend)) return FALSE;

	// Verify content-disposition
	ctx->isattachment=false;
	if((disposition != NULL) && ((strstr(disposition,"filename=")!=NULL) || (strstr(disposition,"attachment")!=NULL)))
	{
		if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: Found part as attachment of type '%s' and disposition '%s'",type,disposition);
		ctx->isattachment=true;
	}

	long field = fts_backend_xapian_clean_header(key->hdr_name);
	if(field<0)
	{
		if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: Unknown header '%s' of part",ctx->tbi_field);
		return FALSE;
	}
	if(field<1) field=10;

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

	ctx->tbi_field = i_strdup_printf("%ld",field);

	if((ctx->tbi_uid>0) && (ctx->tbi_uid != backend->lastuid))
        {
		std::string s("FTS Xapian: New doc incoming (#");
                s.append(std::to_string(ctx->tbi_uid)+")");

		if(fts_xapian_settings.verbose>0) i_info("%s",s.c_str());

		fts_backend_xapian_get_lock(backend, fts_xapian_settings.verbose, s.c_str());

		if(backend->threads.size() < backend->max_threads )
                {
                        XDocsWriter * x = new XDocsWriter(backend,backend->threads.size()+1);
                        x->launch(s.c_str());
			backend->threads.push_back(x);
                }
                       
                long n = backend->threads.size();
                while(n>0)
                {
                        n--;
                        if(!(backend->threads.at(n)->started))
                        {
                                backend->threads[n]->launch("Relaunch post error");
                        }
                }

		if(backend->lastuid>0)
		{
			if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Previous doc ready to index (#%ld)",backend->lastuid);
			backend->docs.front()->status=1;	
		}
                backend->lastuid = ctx->tbi_uid;
		backend->docs.insert(backend->docs.begin(),new XDoc(backend));
		
		if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Start indexing #%ld (%s) : Queue size = %ld",backend->lastuid, backend->boxname,backend->docs.size());

		fts_backend_xapian_release_lock(backend, fts_xapian_settings.verbose, s.c_str());

		n=0;
		long m = fts_backend_xapian_get_free_memory(fts_xapian_settings.verbose);
		while ( (m>0) && (m<2*(fts_xapian_settings.lowmemory * 1024)))
		{
			n++;
			if((n>50) && (fts_xapian_settings.verbose>0))
			{
				i_info("FTS Xapian: Waiting for queue to be absorbed (pending=%ld)",backend->docs.size());
				n=0;
			}
			std::this_thread::sleep_for(XAPIAN_SLEEP);
			m = fts_backend_xapian_get_free_memory(fts_xapian_settings.verbose);
		}
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

	return 0;
}

static int fts_backend_xapian_update_build_more(struct fts_backend_update_context *_ctx, const unsigned char *data, size_t size)
{
	struct xapian_fts_backend_update_context *ctx = (struct xapian_fts_backend_update_context *)_ctx;
	struct xapian_fts_backend *backend = (struct xapian_fts_backend *) ctx->ctx.backend;

	if(ctx->tbi_uid<1) return 0;
	if(strlen(ctx->tbi_field)<1) return 0;
	if(data == NULL) return 0;

	const char * d = (const char *) data;
	if(strlen(d)<(unsigned long)fts_xapian_settings.partial) return 0;

	long h = atol(ctx->tbi_field);

	backend->docs.front()->raw_load(h,d,size,fts_xapian_settings.verbose,"fts_backend_xapian_index");
        	
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
	std::string s;
	uint32_t uid;
	int ret=0;
	std::vector<uint32_t> uids; uids.clear();
	char *zErrMsg = 0;
	XResultSet * result = NULL;
	Xapian::docid docid =0;
	while ((dp = readdir(dirp)) != NULL)
	{
		s = dp->d_name;
		if((dp->d_type == DT_REG) && s.starts_with("db_") && s.ends_with(suffixExp) )
		{
			uids.clear();
			i_info("FTS Xapian: Optimize (1) : Checking expunges from %s",dp->d_name);
			if(sqlite3_open_v2(dp->d_name,&expdb,SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_READWRITE,NULL) == SQLITE_OK)
			{
				if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Optimize (2) : Executing %s",selectExpUIDs);
				if(sqlite3_exec(expdb,selectExpUIDs,fts_backend_xapian_sqlite3_vector_int,&uids,&zErrMsg) != SQLITE_OK)	
				{
					i_error("FTS Xapian: Optimize (3) : Can not select IDs (%s) : %s",selectExpUIDs,zErrMsg);
					if(zErrMsg!=NULL) sqlite3_free(zErrMsg);
					ret =-1;
				}
				s = s.substr(0,s.length()-strlen(suffixExp));
				if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Optimize (4) : Opening Xapian DB (%s)",s.c_str());
				try
				{
					bool ok=false;
					while(!ok)
					{
						try
                                		{
							db = new Xapian::WritableDatabase(s.c_str(),Xapian::DB_CREATE_OR_OPEN | Xapian::DB_BACKEND_GLASS);
                                        		ok=true;
                                		}
                                		catch(Xapian::Error e)
                                		{
                                		        i_warning("FTS Xapian: Retrying opening DB : %s - %s %s",e.get_type(),e.get_msg().c_str(),e.get_error_string());
                                        		std::this_thread::sleep_for(XAPIAN_SLEEP);
						}
                                	}
					long c=0;
					for(uint32_t n=0;n<uids.size();n++)
					{
						uid=uids[n];
						if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Optimize (5) Removing DOC UID=%d",uid);
						XQuerySet * xq = new XQuerySet();
						xq->add(uid);
						result=fts_backend_xapian_query(db,xq,1);
						docid=0; 
						if((result!=NULL) && (result->size>0))
						{
							try
							{
								docid = result->data[0];
								if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Optimize (5) Removing DOC UID=%d DOCID=%d",uid,docid);
								db->delete_document(docid);
								c++;
								if(c>XAPIAN_WRITING_CACHE) 
								{
									i_info("FTS Xapian: Flushing changes on disk");
									db->commit();
									c=0;
								}
							}
							catch(Xapian::Error e)
							{
								i_error("FTS Xapian: Optimize (6) %s",e.get_msg().c_str());
							}
						}
						else if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Optimize UID=%d (DOCID=%d) inexistent",uid,docid);
						if(result!=NULL) { delete(result); result=NULL; }
						delete(xq);
						char * u = i_strdup_printf(deleteExpUID,uid);
						if (sqlite3_exec(expdb,u,NULL,0,&zErrMsg) != SQLITE_OK )
						{
							i_error("FTS Xapian : Optimize Sqlite error: %s",zErrMsg);
							if(zErrMsg!=NULL) sqlite3_free(zErrMsg);
						}
						i_free(u);
					}
					if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Optimize - Closing DB %s",s.c_str());
					fts_backend_xapian_close_db(db,s.c_str(),"fts_optimize",fts_xapian_settings.verbose);
				}
				catch(Xapian::Error e)
				{
					i_error("FTS Xapian: Optimize (7) %s",e.get_msg().c_str());
				}
				sqlite3_close(expdb);
			}
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
		i_error("FTS Xapian: Index folder (%s) inexistent",backend->path);
		return -1;
	}

	std::error_code errorCode;
	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Rescan by deleting %s",backend->path);
	std::filesystem::remove_all(backend->path,errorCode);

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

	fts_backend_xapian_build_qs(qs,args,backend->dict_db);

	XResultSet * r=fts_backend_xapian_query(dbr,qs);

	long n=r->size;
	if(fts_xapian_settings.verbose>0) { i_info("FTS Xapian: Query '%s' -> %ld results",qs->get_string().c_str(),n); }

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
