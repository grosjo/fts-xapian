/* Copyright (c) 2019 Joan Moreau <jom@grosjo.net>, see the included COPYING file */

class XResultSet
{
    public:
	long size;
    	Xapian::docid * data;

    XResultSet() { size=0; data=NULL; }
    ~XResultSet() { if (size>0) { i_free(data); } }

    void add(Xapian::docid did)
    {
        if(data==NULL)
        {
            data=(Xapian::docid *)i_malloc(sizeof(Xapian::docid));
        }
        else
        {
            data=(Xapian::docid *)i_realloc(data,size*sizeof(Xapian::docid),(size+1)*sizeof(Xapian::docid));
        }
        data[size]=did;
        size++;
    }
};
   
class XQuerySet
{
	public:
	char * header;
	char * text;
	XQuerySet ** qs;
	bool global_and; // global
	bool global_neg; // global
	bool item_neg; // for the term
	long qsize;
	long limit;

	XQuerySet()
	{
		qsize=0; qs=NULL;
                limit=1;
		global_and=true;
		header=NULL;
                text=NULL;
		global_neg=false;
	}

	XQuerySet(bool is_and, bool is_neg, long l)
	{
		qsize=0; qs=NULL;
		limit=1;
		if(l>1) { limit=l; }
		header=NULL;
		text=NULL;
		global_and=is_and;
		global_neg=is_neg;
	}

	~XQuerySet()
        {
                if(text!=NULL) { i_free(text); text=NULL; }
                if(header!=NULL) { i_free(header); header=NULL; }

                for(long j=0;j<qsize;j++)
                {
                        delete(qs[j]);
                }
                if(qsize>0) i_free(qs);
                qsize=0; qs=NULL;
        }

	void add(const char * h,const char * t)
	{
		add(h,t,false);
	}

        void add(const char * h,const char * t, bool is_neg)
        {
                if(h==NULL) return;
                if(t==NULL) return;

                icu::StringPiece sp_h(h);
                icu::UnicodeString h2 = icu::UnicodeString::fromUTF8(sp_h);

                icu::StringPiece sp_t(t);
                icu::UnicodeString t2 = icu::UnicodeString::fromUTF8(sp_t);

                add(&h2,&t2,is_neg);
        }

	void add(icu::UnicodeString *h, icu::UnicodeString *t, bool is_neg)
        {
		long i,j;
		XQuerySet * q2;
		icu::UnicodeString *r;

	        t->findAndReplace("'"," ");
		t->findAndReplace("\""," ");
                t->findAndReplace(":"," ");
                t->findAndReplace(";"," ");
                t->findAndReplace("\""," ");
                t->findAndReplace("<"," ");
                t->findAndReplace(">"," ");
                t->findAndReplace("\n"," ");
                t->findAndReplace("\r"," ");

		h->trim();
		t->trim();
                h->toLower();
                t->toLower();

		if(h->length()<1) return;
		if(t->length()<limit) return;

		i = t->lastIndexOf(" ");
                if(i>0)
                {
                        q2 = new XQuerySet(true,false,limit);
                        while(i>0)
                        {
                                j = t->length();
                                r = new icu::UnicodeString(*t,i+1,j-i-1);
                                q2->add(h,r,false);
                                delete(r);
                                t->truncate(i);
				t->trim();
                                i = t->lastIndexOf(" ");
                        }
                        q2->add(h,t,false);
                        if(q2->count()>0) add(q2); else delete(q2);
                        return;
                }

		i = t->indexOf(".");
		if(i>=0)
		{
			r = new icu::UnicodeString(*t);
			r->findAndReplace(".","_");
			q2 = new XQuerySet(false,false,limit);
			q2->add(h,r,false);
			delete(r);

			t->findAndReplace("."," ");
			t->trim();
			q2->add(h,t,false);
                	
			if(q2->count()>0) add(q2); else delete(q2);
			return;
		}
		
                std::string tmp1;
                h->toUTF8String(tmp1);
                char * h2 = i_strdup(tmp1.c_str());
                std::string tmp2;
                t->toUTF8String(tmp2);
                char * t2 = i_strdup(tmp2.c_str());

		if(strcmp(h2,XAPIAN_WILDCARD)==0)
		{
			q2 = new XQuerySet(false,is_neg,limit);
			for(i=1;i<HDRS_NB;i++)
			{
				q2->add(hdrs_emails[i],t2,is_neg);
			}
			add(q2);
			i_free(h2);
                        i_free(t2);
			return;
		}

		i=0;
		while((i<HDRS_NB) && (strcmp(h2,hdrs_emails[i])!=0))
		{
			i++;
		}
		if(i>=HDRS_NB)
		{
			i_error("FTS Xapian: Unknown header '%s'",h2);
			i_free(h2); i_free(t2);
			return;
		}

                if(has(h2,t2,true))
                {
                        i_free(h2);
                        i_free(t2);
                        return;
                }

                if(text==NULL)
                {
                        text=t2;
                        header=h2;
			item_neg=is_neg;
			return;
                }

		q2 = new XQuerySet(global_and,is_neg,limit);
		q2->add(h,t,false);
		add(q2);
	}

	void add(XQuerySet *q2)
	{
		if(qsize<1)
                {
                        qs=(XQuerySet **)i_malloc(sizeof(XQuerySet*));
		}
		else
		{
			qs=(XQuerySet **)i_realloc(qs,qsize*sizeof(XQuerySet*),(qsize+1)*sizeof(XQuerySet*));
		}
		qs[qsize]=q2;
		qsize++;
	}

	bool has(const char *h, const char *t, bool loop)
	{
		if((text!=NULL) && (strcmp(h,header)==0) && (strcmp(t,text)==0)) return true;
		if(loop)
		{
			for(long i=0; i<qsize; i++)
			{
				if(qs[i]->has(h,t,false)) return true;
			}
		}
		return false;
	}
	
	int count()
	{
		int c=0;
		if(text!=NULL) c=1;
		c+=qsize;
		return c;
	} 

	std::string get_string()
	{
		std::string s;

		if(count()<1) return s;

		if(text!=NULL)
		{
			if(item_neg) s.append("NOT( ");
			s.append(header); 
			//s.append(":");
			s.append(":\"");
			s.append(text);
			s.append("\"");
			if(item_neg) s.append(" )");
		}

		const char * op=" OR ";
		if(global_and) op=" AND ";

		for (int i=0;i<qsize;i++)
		{
			int c=qs[i]->count();
			if(c<1) continue;

			if(s.length()>0) s.append(op);

			if(qs[i]->global_neg)
			{
				s.append("NOT(");
				s.append(qs[i]->get_string());
				s.append(")");
			}
			else if(c>1)
			{
				s.append("(");
				s.append(qs[i]->get_string());
                                s.append(")");
			}
			else s.append(qs[i]->get_string());
		}
		if(global_neg) 
		{
			s="NOT("+s+")";
		}
		return s;
	}

	Xapian::Query * get_query(Xapian::Database * db)
        {
                if(count()<1)
                {
			return new Xapian::Query(Xapian::Query::MatchNothing);
                }
		
		Xapian::QueryParser * qp = new Xapian::QueryParser();
		
		for(int i=0; i< HDRS_NB; i++) qp->add_prefix(hdrs_emails[i], hdrs_xapian[i]);

		char *s = i_strdup(get_string().c_str());
	
		if(verbose>1) { i_info("FTS Xapian: Query= %s",s); }

		qp->set_database(*db);
	
		Xapian::Query * q = new Xapian::Query(qp->parse_query(s,Xapian::QueryParser::FLAG_DEFAULT));// | Xapian::QueryParser::FLAG_PARTIAL));
                
		i_free(s);
                delete(qp);
                return q;
	}
};

class XNGram
{
	private:
		long partial,full,hardlimit;
		const char * prefix;
		bool onlyone;

	public:
		char ** data;
		long size,maxlength;
  
        XNGram(long p, long f, const char * pre) 
	{ 
		partial=p; full=f; 
		size=0; 
		maxlength=0;
		data=NULL; 
		prefix=pre;
		hardlimit=XAPIAN_TERM_SIZELIMIT-strlen(prefix);
		onlyone=false;
		if(strcmp(prefix,"XMID")==0) onlyone=true;
	}

        ~XNGram() 
	{ 
		if (size>0) 
		{ 
			for(long i=0;i<size;i++) 
			{ 
				i_free(data[i]); 
			} 
			i_free(data); 
		}
		data=NULL;
	}

	void add(const char * s)
        {
        	if(s==NULL) return;

		icu::StringPiece sp(s);
		icu::UnicodeString d = icu::UnicodeString::fromUTF8(sp);
		add(&d);
	}

	void add(icu::UnicodeString *d)
	{
		icu::UnicodeString * r;

		d->toLower();
		d->findAndReplace("'"," ");
		d->findAndReplace("\""," ");
		d->findAndReplace(":"," ");
		d->findAndReplace(";"," ");
		d->findAndReplace("\""," ");
		d->findAndReplace("<"," ");
		d->findAndReplace(">"," ");
		d->findAndReplace("\n"," ");
		d->findAndReplace("\r"," ");

		long i = d->indexOf(".");
		if(i>=0)
		{
			r = new icu::UnicodeString(*d);
			r->findAndReplace(".","_");
			add(r);
			delete(r);
			d->findAndReplace("."," ");
		}

		d->trim();
		i = d->indexOf(" ");

		if(i>0)
		{
			r = new icu::UnicodeString(*d,i+1);
			add(r);
			delete(r);
			d->truncate(i);
			d->trim();
		}
	
		long l = d->length();
		if(l<partial) return;

		if(onlyone)
		{
			add_stem(d);
			return;
		}
	
		for(i=0;i<=l-partial;i++)
		{
			for(long j=partial;(j+i<=l)&&(j<=full);j++)
			{
				r = new icu::UnicodeString(*d,i,j);
				add_stem(r);
				delete(r);
			}
		}
		if(l>full) add_stem(d);
	}
	
	void add_stem(icu::UnicodeString *d)
	{
		d->trim();
		long l=d->length();
		if(l<partial) return;

		std::string s;
		d->toUTF8String(s);
		l  = s.length();
		if(l>hardlimit)
		{
			if(verbose>0) i_warning("FTS Xapian: Term too long to be indexed (%s ...)",s.substr(0,100).c_str());
			return;
		}

		char * s2 = i_strdup(s.c_str());
 
                if(size<1)
                {
                	data=(char **)i_malloc(sizeof(char*));
			size=0;
                }
                else
                {
			bool existing=false;
			long i=0;
			while((!existing) && (i<size))
                	{
				if(strcmp(data[i],s2)==0)
				{
					existing=true;
				}
				i++;
			}
			if(existing) { i_free(s2); return; }
                	data=(char **)i_realloc(data,size*sizeof(char*),(size+1)*sizeof(char*));
                }
		if(l>maxlength) { maxlength=l; }
                data[size]=s2;
                size++;
	}
};


static bool fts_backend_xapian_check_write(struct xapian_fts_backend *backend)
{
	if((backend->db == NULL) || (strlen(backend->db)<1))
	{
		if(verbose>0) i_warning("FTS Xapian: check_write : no DB name");
		return false;
	}

	if(backend->dbw != NULL) return true;

	try
	{
		if(verbose>1) i_info("FTS Xapian: Opening DB (RW) %s",backend->db);
		backend->dbw = new Xapian::WritableDatabase(backend->db,Xapian::DB_CREATE_OR_OPEN | Xapian::DB_RETRY_LOCK);
	}
	catch(Xapian::Error e)
	{
		i_error("FTS Xapian: Can't open RW index (%s) %s",backend->boxname,backend->db);
		i_error("FTS Xapian: %s",e.get_msg().c_str());
		return false;
	}
	return true;
}

static void fts_backend_xapian_oldbox(struct xapian_fts_backend *backend)
{
	if(backend->old_guid != NULL)
	{
		/* Performance calculator*/
		struct timeval tp;
		gettimeofday(&tp, NULL);
		long dt = tp.tv_sec * 1000 + tp.tv_usec / 1000 - backend->perf_dt;
		double r=0;
		if(dt>0)
		{
			r=backend->perf_nb*1000.0;
			r=r/dt;
		}
		/* End Performance calculator*/
                
		if(verbose>0) i_info("FTS Xapian: Done indexing '%s' (%s) (%ld msgs in %ld ms, rate: %.1f)",backend->old_boxname, backend->old_guid,backend->perf_nb,dt,r);

		i_free(backend->old_guid); backend->old_guid = NULL;
		i_free(backend->old_boxname); backend->old_boxname = NULL;
        }
}

static void fts_backend_xapian_add_expunge(struct xapian_fts_backend *backend, Xapian::docid docid, unsigned int uid)
{
	char *zErrMsg = 0;

	char * sql = i_strdup_printf("REPLACE INTO IDS (DOCID,ID) VALUES (%d,%d);",docid,uid);

	if(verbose>0) i_info("FTS Xapian: adding expunge IDs : %s",sql);

	int rc = sqlite3_exec(backend->db_expunge, sql, 0, 0, &zErrMsg);
	if(rc != SQLITE_OK)
	{
		i_error("FTS Xapian: adding expunge IDs (%s) : err(%d,%s)",sql,rc,zErrMsg);
		sqlite3_free(zErrMsg);
	}
	i_free(sql);
}

static int fts_backend_xapian_hold(struct xapian_fts_backend * backend, const char * boxname, const char * mb )
{
	char *zErrMsg = 0;
        struct stat sb;

	struct timeval tp;
	long current_time;

	gettimeofday(&tp, NULL);
	current_time = tp.tv_sec * 1000 + tp.tv_usec / 1000;

	backend->guid = NULL;
	backend->db = NULL;
	backend->commit_updates=0;
	backend->commit_time = current_time;
	backend->boxname = NULL;
	backend->db_expunge = NULL;

	/* Performance calculator*/
	backend->perf_dt = current_time;
	backend->perf_uid=0;
	backend->perf_nb=0;
	backend->perf_pt=0;
	/* End Performance calculator*/


        char * ename = i_strdup_printf("%s/expunge_%s",backend->path,mb);
        bool exists = ( (stat(ename, &sb)==0) && S_ISREG(sb.st_mode) );

        int rc = sqlite3_open(ename, &(backend->db_expunge));
        if(rc != SQLITE_OK)
        {
                i_error("FTS Xapian: Can not open expunge db '%s' for box '%s'",ename,boxname);
                backend->db_expunge = NULL;
		i_free(ename);
		return -1;
        }
        else if(!exists)
        {
                if(verbose>0) i_info("FTS Xapian: Creating expunge database : %s",ename);

                rc = sqlite3_exec(backend->db_expunge, "CREATE TABLE IDS (DOCID UNSIGNED BIG INT PRIMARY KEY NOT NULL, ID UNSIGNED BIG INT NOT NULL)", 0, 0, &zErrMsg);
                if( rc != SQLITE_OK )
                {
                        i_error("FTS Xapian: Error in expunge file : %s",zErrMsg);
                        sqlite3_free(zErrMsg);
			i_free(ename);
			return -1;
                }
        }

	backend->guid = i_strdup(mb);
	backend->boxname = i_strdup(boxname);
        backend->db = i_strdup_printf("%s/db_%s",backend->path,mb);

	i_free(ename);
	return 0;
}

static void fts_backend_xapian_release(struct xapian_fts_backend *backend, const char * reason)
{
        if(backend->dbr !=NULL)
        {
                backend->dbr->close();
                delete(backend->dbr);
                backend->dbr = NULL;
        }
}

static void fts_backend_xapian_commit(struct xapian_fts_backend *backend, const char * reason, long commit_time)
{
	if(backend->dbw !=NULL)
        {
		try
                {
                        backend->dbw->commit();
                        backend->dbw->close();
                }
                catch(Xapian::Error e)
                {
                        i_error("FTS Xapian: (%s) %s",reason, e.get_msg().c_str());
                }
                delete(backend->dbw);
                backend->dbw=NULL;
		backend->commit_updates=0;
		backend->commit_time = commit_time;
	}

	if(verbose>0)
	{
		struct timeval tp;
        	gettimeofday(&tp, NULL);
        	long current_time = tp.tv_sec * 1000 + tp.tv_usec / 1000;
		i_info("FTS Xapian: Committed '%s' in %ld ms",reason,current_time - commit_time);
	}
}

static int fts_backend_xapian_expunge_callback(void *data, int argc, char **argv, char **azColName)
{
	struct xapian_fts_expunge * xfe = (struct xapian_fts_expunge *)data;

	xfe->did[xfe->index]=atoi(argv[0]);
	xfe->uid[xfe->index]=atoi(argv[1]);
	xfe->index++;

	return 0;
}

static void fts_backend_xapian_expunge(struct xapian_fts_backend *backend, const char * reason)
{
	if(backend->db_expunge == NULL) 
	{
		if(verbose>0) i_info("FTS Xapian: Expunge on an empty box (%s)",reason);
		return;
	}

	Xapian::docid d;
	long u;
	struct xapian_fts_expunge xfe;
	int i,rc;
	char *zErrMsg = 0;

	xfe.index=0;
	i=XAPIAN_EXPUNGE_SIZE;

	char * sql = i_strdup_printf("SELECT DOCID,ID from IDS ORDER BY DOCID LIMIT %d",i);

	rc = sqlite3_exec(backend->db_expunge, sql, fts_backend_xapian_expunge_callback, (void*)(&xfe), &zErrMsg);
	if( rc != SQLITE_OK )
	{
		i_error("FTS Xapian: Error in expunge fct (%s) (%s) : %s",sql,zErrMsg,reason);
		sqlite3_free(zErrMsg);
	}
	i_free(sql);

	if(xfe.index <1) return;

	if(!fts_backend_xapian_check_write(backend)) return;

	for(i=0;i<xfe.index;i++)
	{
		d = xfe.did[i];
		u = xfe.uid[i];

		if(verbose>0) i_info("FTS Xapian: Deleting terms for expunged UID=%ld (Docid=%d) (%s)",u,d,reason);
		try
		{
			backend->dbw->delete_document(d);
		}
		catch(Xapian::Error e)
		{
			if(verbose>0) i_info("FTS Xapian: Can not delete document %d (%s)",d,reason);
		}
		
		sql = i_strdup_printf("DELETE FROM IDS WHERE DOCID=%d",d);
		rc = sqlite3_exec(backend->db_expunge, sql, 0 ,0, &zErrMsg);
		if( rc != SQLITE_OK )
		{
			i_error("FTS Xapian: Error in cleaning expunge (%s): %s",sql,zErrMsg);
			sqlite3_free(zErrMsg);
		}
		i_free(sql);
	}
}

static int fts_backend_xapian_unset_box(struct xapian_fts_backend *backend)
{
	if(verbose>0) i_info("FTS Xapian: Unset box '%s' (%s)",backend->boxname,backend->guid);

	fts_backend_xapian_oldbox(backend);

        struct timeval tp;
        gettimeofday(&tp, NULL);
        long commit_time = tp.tv_sec * 1000 + tp.tv_usec / 1000;

	fts_backend_xapian_release(backend,"unset_box");
	fts_backend_xapian_expunge(backend,"unset_box");
	fts_backend_xapian_commit(backend,"unset_box", commit_time);

	if(backend->db != NULL) 
	{
		i_free(backend->db);
		backend->db = NULL;
		
		i_free(backend->guid);
		backend->guid = NULL;
		
		i_free(backend->boxname);
		backend->boxname = NULL;
	}
	if(backend->db_expunge != NULL)
	{
		sqlite3_close(backend->db_expunge);
		backend->db_expunge = NULL;
	}

	return 0;
}

static int fts_backend_xapian_set_box(struct xapian_fts_backend *backend, struct mailbox *box)
{
	if (box == NULL)
	{
		fts_backend_xapian_unset_box(backend);
		if(verbose>0) i_info("FTS Xapian: Box is empty");
                return 0;
	}

	const char * mb;
	fts_mailbox_get_guid(box, &mb );

	if(verbose>0) i_info("FTX Xapian: Set box '%s' (%s)",box->name,mb);

	if((mb == NULL) || (strlen(mb)<3))
	{
		i_error("FTS Xapian: Invalid box");
		return -1;
	}

	if((backend->guid != NULL) && (strcmp(mb,backend->guid)==0))
	{
		if(verbose>1) i_info("FTS Xapian: Box is unchanged");
		return 0;
	}

	fts_backend_xapian_unset_box(backend);

	return fts_backend_xapian_hold(backend,box->name,mb);
}

static bool fts_backend_xapian_check_read(struct xapian_fts_backend *backend)
{
	if((backend->db == NULL) || (strlen(backend->db)<1)) 
	{
		if(verbose>0) i_warning("FTS Xapian: check_read : no DB name");
		return false;
	}

        if(backend->dbr != NULL) return true;

	struct stat sb;

	if(!((stat(backend->db, &sb) == 0) && S_ISDIR(sb.st_mode)))
	{
		try
		{
			Xapian::WritableDatabase db(backend->db,Xapian::DB_CREATE_OR_OPEN);
			db.commit();
                       	db.close();
		}
		catch(Xapian::Error e)
		{
			if(verbose>0) i_warning("FTS Xapian: Tried to create an existing db '%s'",backend->db);
		}
	}
	try
	{
		if(verbose>1) i_info("FTS Xapian: Opening DB (RO) %s",backend->db);
		backend->dbr = new Xapian::Database(backend->db,Xapian::DB_OPEN); 
	}
	catch(Xapian::Error e)
	{
		i_error("FTS Xapian: Can not open RO index (%s) %s",backend->boxname,backend->db);
		i_error("FTS Xapian: %s",e.get_msg().c_str());
		return false;
	}
	return true;
}



static void fts_backend_xapian_build_qs(XQuerySet * qs, struct mail_search_arg *a)
{
        const char * hdr;

        while(a != NULL)
        {
		switch (a->type) 
		{
        		case SEARCH_TEXT: 
        		case SEARCH_BODY: 
        		case SEARCH_HEADER:
        		case SEARCH_HEADER_ADDRESS:
        		case SEARCH_HEADER_COMPRESS_LWSP: break;
        		default: a = a->next; continue;
        	}
		
		if((a->hdr_field_name == NULL)||(strlen(a->hdr_field_name)<1))
                {
                        if(a->type == SEARCH_BODY)
                        {
                                hdr="body";
                        }
                        else
                        {
                                hdr=XAPIAN_WILDCARD;
                        }
                }
                else
                {
                        hdr=a->hdr_field_name;
                }
                if((a->value.str == NULL) || (strlen(a->value.str)<1))
                {
                        XQuerySet * q2 = new XQuerySet(false,a->match_not,qs->limit);
                        fts_backend_xapian_build_qs(q2,a->value.subargs);
                        if(q2->count()>0)
                        {
                                qs->add(q2);
                        }
                        else
                        {
                                delete(q2);
                        }
                }
                else
                {
                        qs->add(hdr,a->value.str,a->match_not);
                }
                a->match_always=true;
		a = a->next;
        }
}

XResultSet * fts_backend_xapian_query(Xapian::Database * dbx, XQuerySet * query, long limit=0)
{
    	XResultSet * set= new XResultSet();
   
    	try
    	{
		Xapian::Enquire enquire(*dbx);

		Xapian::Query * q = query->get_query(dbx);
		
		enquire.set_query(*q);
		enquire.set_docid_order(Xapian::Enquire::DESCENDING);
		delete(q);

        	long offset=0;
        	long pagesize=100; if(limit>0) { pagesize=std::min(pagesize,limit); }
        	Xapian::MSet m = enquire.get_mset(0, pagesize);
        	while(m.size()>0)
        	{

        		Xapian::MSetIterator i = m.begin();
		        while (i != m.end()) 
            		{
                		Xapian::Document doc = i.get_document();
	            		set->add(doc.get_docid());
	            		i++;
            		}
            		offset+=pagesize;
            		m = enquire.get_mset(offset, pagesize);
        	}
    	}
    	catch(Xapian::Error e)
    	{
		i_error("FTS Xapian: %s",e.get_msg().c_str());
    	}
    	return set;
}

bool fts_backend_xapian_index_hdr(Xapian::WritableDatabase * dbx, uint uid, const char* field, icu::UnicodeString* data,long p, long f)
{
	if(data->length()<p) { return true; }

	if(strlen(field)<1) { return true; }

	long i=0;
	while((i<HDRS_NB) && (strcmp(field,hdrs_emails[i])!=0))
	{
		i++;
	}
	if(i>=HDRS_NB) return true;

	const char * h=hdrs_xapian[i];

	try
	{
		XQuerySet * xq = new XQuerySet();
        	const char *u = t_strdup_printf("%d",uid);
        	xq->add("uid",u);

        	XResultSet * result=fts_backend_xapian_query(dbx,xq,1);

		Xapian::docid docid;
		Xapian::Document doc;
		if(result->size<1)
        	{
			doc.add_value(1,Xapian::sortable_serialise(uid));
			u = t_strdup_printf("Q%d",uid);
			doc.add_term(u);
			docid=dbx->add_document(doc);
        	}
		else
		{
			docid=result->data[0];
			doc = dbx->get_document(docid);
		}
		delete(result);
		delete(xq);
	
		XNGram * ngram = new XNGram(p,f,h);
	        ngram->add(data);
	
		char *t = (char*)i_malloc(sizeof(char)*(ngram->maxlength+6));
	
		for(i=0;i<ngram->size;i++)
		{
			snprintf(t,ngram->maxlength+6,"%s%s",h,ngram->data[i]);
			try
			{
				doc.add_term(t);
			}
			catch(Xapian::Error e)
			{
				i_error("FTS Xapian: %s",e.get_msg().c_str());
			}
		}
		i_free(t);
		delete(ngram);
		
		dbx->replace_document(docid,doc);
	    	return true;
	}
	catch(Xapian::Error e)
	{
		std::string s;
		data->toUTF8String(s);
		i_error("FTS Xapian: fts_backend_xapian_index_hdr (%s) -> %s",field,s.c_str());
		i_error("FTS Xapian: %s",e.get_msg().c_str());
	}
	return false;
}

bool fts_backend_xapian_index_text(Xapian::WritableDatabase * dbx,uint uid, const char * field, icu::UnicodeString * data,long p, long f)
{
	if(data->length()<p) { return true; }

	try
        {
		XQuerySet * xq = new XQuerySet();

		const char *u = t_strdup_printf("%d",uid);
		xq->add("uid",u);

                XResultSet * result=fts_backend_xapian_query(dbx,xq,1);
  
                Xapian::docid docid;
                Xapian::Document doc;
                if(result->size<1)
                {
			doc.add_value(1,Xapian::sortable_serialise(uid));
                        u = t_strdup_printf("Q%d",uid);
                        doc.add_term(u);
                        docid=dbx->add_document(doc);
                }
                else
                {
			docid=result->data[0];
                        doc = dbx->get_document(docid);
                }
		delete(result);
		delete(xq);

		Xapian::Document doc2;
		Xapian::TermGenerator termgenerator;
		Xapian::Stem stem("en");
		termgenerator.set_stemmer(stem);
		termgenerator.set_document(doc2);
	
		const char * h;
                if(strcmp(field,"subject")==0) 
		{
			h="S";
		}
		else
		{
			h="XBDY";
		}
		std::string s;
		data->toUTF8String(s);
		termgenerator.set_stemming_strategy(Xapian::TermGenerator::STEM_ALL);
		termgenerator.index_text(s, 1, h);
	
		long l = strlen(h);
		long n = doc2.termlist_count();
		Xapian::TermIterator ti = doc2.termlist_begin();
		XNGram * ngram = new XNGram(p,f,h);
		const char * c;
		while(n>0)
		{
			s = *ti;
			c=s.c_str();	
			if(strncmp(c,h,l)==0)
			{
				ngram->add(c+l);
			}
			ti++;
			n--;
		}
		if(verbose>1) i_info("FTS Xapian: NGRAM(%s,%s) %ld max=%ld",field,h,ngram->size,ngram->maxlength);

		char *t = (char*)i_malloc(sizeof(char)*(ngram->maxlength+6));
		for(n=0;n<ngram->size;n++)
                {
                        snprintf(t,ngram->maxlength+6,"%s%s",h,ngram->data[n]);
                        try
                        {
                                doc.add_term(t);
                        }
                        catch(Xapian::Error e)
                        {
                                i_error("FTS Xapian: %s",e.get_msg().c_str());
                        }
                }
                i_free(t);
                delete(ngram);

                dbx->replace_document(docid,doc);
          }
          catch(Xapian::Error e)
          {
		i_error("FTS Xapian: fts_backend_xapian_index_text");
		i_error("FTS Xapian: %s",e.get_msg().c_str());
		return false;
          }
          return true;
}

static int fts_backend_xapian_empty_db_remove(const char *fpath, const struct stat *sb, int typeflag)
{
	if(typeflag == FTW_F)
	{
		if(verbose>0) i_info("FTS Xapian: Removing file %s",fpath);
		remove(fpath);
	}
	return 0;
}

static int fts_backend_xapian_empty_db(const char *fpath, const struct stat *sb, int typeflag)
{
	const char * s = fpath;
	const char * sl = fpath;

	while(sl !=NULL)
	{
		s=sl;
		sl=strstr(sl,"/")+1;
	}

	if((typeflag == FTW_D) && (strncmp(s,"db_",3)==0))
	{
		if(verbose>0) i_info("FTS Xapian: Emptying %s",fpath);
		ftw(fpath,fts_backend_xapian_empty_db_remove,100);
		if(verbose>0) i_info("FTS Xapian: Removing directory %s",fpath);
		rmdir(fpath);
	}
	else if((typeflag == FTW_F) && (strncmp(s,"expunge_",8)==0))
	{
		if(verbose>0) i_info("FTS Xapian: Removing file %s",fpath);
		remove(fpath);
	}
        return 0;
}
