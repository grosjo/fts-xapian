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
	private:
		icu::Transliterator *accentsConverter;
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
		accentsConverter=NULL;
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
		accentsConverter=NULL;
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
		if(accentsConverter != NULL) delete(accentsConverter);
	}

	void add(const char * h,const char * t)
	{
		add(h,t,false);
	}

	void add(const char * h,const char * t, bool is_neg)
	{
		if(h==NULL) return;
		if(t==NULL) return;

		icu::UnicodeString h2 = icu::UnicodeString::fromUTF8(icu::StringPiece(h));
		icu::UnicodeString t2 = icu::UnicodeString::fromUTF8(icu::StringPiece(t));

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
		t->findAndReplace("@"," ");
		t->findAndReplace("-","_");

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

		if(accentsConverter == NULL)
		{
			UErrorCode status = U_ZERO_ERROR;
                	accentsConverter = icu::Transliterator::createInstance("NFD; [:M:] Remove; NFC", UTRANS_FORWARD, status);
                	if(U_FAILURE(status))
                	{
                	        i_error("FTS Xapian: Can not allocate ICU translator (2)");
                	        accentsConverter = NULL;
                	}
		}
		if(accentsConverter != NULL) accentsConverter->transliterate(*t);

		std::string tmp2;
		t->toUTF8String(tmp2);
		char * t2 = i_strdup(tmp2.c_str());

		if(strcmp(h2,XAPIAN_WILDCARD)==0)
		{
			q2 = new XQuerySet(false,is_neg,limit);
			for(i=1;i<HDRS_NB;i++)
			{
				if(i!=XAPIAN_EXPUNGE_HEADER) q2->add(hdrs_emails[i],t2,is_neg);
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
			if(fts_xapian_settings.verbose>1) i_error("FTS Xapian: Unknown header (lookup) '%s'",h2);
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
		long hardlimit;
		const char * prefix;
		bool onlyone;
		icu::Transliterator *accentsConverter;

	public:
		char ** data;
		long size,maxlength;
		long memory;

	XNGram(const char * pre)
	{
		size=0;
		memory=0;
		maxlength=0;
		data=NULL;
		prefix=pre;
		hardlimit=XAPIAN_TERM_SIZELIMIT-strlen(prefix);
		onlyone=false;
		if(strcmp(prefix,"XMID")==0) onlyone=true;
		accentsConverter = NULL;
	}

	~XNGram()
	{
		long i;
		if (data != NULL)
		{
			for(i=0;i<size;i++)
			{
				i_free(data[i]);
			}
			i_free(data);
		}
		data=NULL;
		if(accentsConverter != NULL) delete(accentsConverter);
	}

	void add(const char * s)
	{
		if(s==NULL) return;

		icu::UnicodeString d = icu::UnicodeString::fromUTF8(icu::StringPiece(s));
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
		d->findAndReplace("@"," ");
		d->findAndReplace("-","_");
		
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
		if(l<fts_xapian_settings.partial) return;

		if(accentsConverter == NULL)
		{
			UErrorCode status = U_ZERO_ERROR;
			accentsConverter = icu::Transliterator::createInstance("NFD; [:M:] Remove; NFC", UTRANS_FORWARD, status);
			if(U_FAILURE(status))
			{
				i_error("FTS Xapian: Can not allocate ICU translator (1)");
				accentsConverter = NULL;
			}
		}
		if(accentsConverter != NULL) accentsConverter->transliterate(*d);

		if(onlyone)
		{
			add_stem(d);
			return;
		}

		for(i=0;i<=l-fts_xapian_settings.partial;i++)
		{
			for(long j=fts_xapian_settings.partial;(j+i<=l)&&(j<=fts_xapian_settings.full);j++)
			{
				r = new icu::UnicodeString(*d,i,j);
				add_stem(r);
				delete(r);
			}
		}
		if(l>fts_xapian_settings.full) add_stem(d);
	}

	void add_stem(icu::UnicodeString *d)
	{
		d->trim();
		long l=d->length();
		if(l<fts_xapian_settings.partial) return;

		std::string s;
		d->toUTF8String(s);
		l  = s.length();
		if(l>hardlimit)
		{
			if(fts_xapian_settings.verbose>0) i_warning("FTS Xapian: Term too long to be indexed (%s ...)",s.substr(0,100).c_str());
			return;
		}

		char * s2 = i_strdup(s.c_str());
		long p =0;

		if(size<1)
		{
			data=(char **)i_malloc(sizeof(char*));
			size=0;
			p=0;
		}
		else
		{
			p=0;
			while((p<size) && (strcmp(data[p],s2)<0))
			{
				p++;
			}
			if((p<size) && (strcmp(data[p],s2)==0))
			{
				i_free(s2);
				return;
			}
			data=(char **)i_realloc(data,size*sizeof(char*),(size+1)*sizeof(char*));
			long i=size;
			while(i>p)
			{
				data[i]=data[i-1];
				i--;
			}
		}
		if(l>maxlength) { maxlength=l; }
		data[p]=s2;
		size++;
		memory = memory + ((l+1) * sizeof(data[p][0]));
	}
};

static long fts_backend_xapian_current_time()
{
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

static long fts_backend_xapian_get_free_memory() // KB
{
	struct rlimit rl;
	getrlimit(RLIMIT_AS,&rl);

	long limit = rl.rlim_cur / 1024.0;
	if(fts_xapian_settings.verbose>1) i_warning("FTS Xapian: RLIM AS =%ld",limit);

	getrlimit(RLIMIT_DATA,&rl);
	long l2 = rl.rlim_cur / 1024.0;
        if(fts_xapian_settings.verbose>1) i_warning("FTS Xapian: RLIM DATA =%ld",l2);

	if((l2>0) && ((limit>l2) || (limit<1))) limit=l2;

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__)	
	uint32_t m;
	size_t len = sizeof(m);
	sysctlbyname("vm.stats.vm.v_free_count", &m, &len, NULL, 0);
	if(fts_xapian_settings.verbose>1) i_warning("FTS Xapian: (BSD) Free pages %ld",long(m));
	m = m * fts_xapian_settings.pagesize / 1024.0;
	if((limit>0) && (m>limit)) m = limit;
	if(fts_xapian_settings.verbose>1) i_warning("FTS Xapian: (BSD) Free memory %ld MB",long(m/1024.0));
	return long(m);
#else
	long m = long(sysconf(_SC_AVPHYS_PAGES) * fts_xapian_settings.pagesize / 1024.0);
	if((limit>0) && (m>limit)) m = limit;
	if(fts_xapian_settings.verbose>1) i_warning("FTS Xapian: Free memory %ld MB",long(m/1024.0));
	return m;
#endif
}

static bool fts_backend_xapian_test_memory()
{
	long fri = fts_backend_xapian_get_free_memory(); // KB

	if(fts_xapian_settings.verbose>1) i_warning("FTS Xapian: Free memory %ld MB vs %ld MB minimum",long(fri/1024.0),fts_xapian_settings.lowmemory);

	return(fri > ( fts_xapian_settings.lowmemory * 1024 ) );
}

static bool fts_backend_xapian_open_readonly(struct xapian_fts_backend *backend, Xapian::Database ** dbr)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_open_readonly");

	if((backend->db == NULL) || (strlen(backend->db)<1))
	{
		if(fts_xapian_settings.verbose>0) i_warning("FTS Xapian: Open DB Read Only : no DB name");
		return false;
	}

	try
	{
		if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: Opening DB (RO) %s",backend->db);
		*dbr = new Xapian::Database(backend->db,Xapian::DB_OPEN | Xapian::DB_NO_SYNC);
	}
	catch(Xapian::Error e)
	{
		i_error("FTS Xapian: Can not open RO index (%s) %s : %s - %s",backend->boxname,backend->db,e.get_type(),e.get_error_string());
		return false;
	}
	return true;
}

static bool fts_backend_xapian_check_access(struct xapian_fts_backend *backend)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_check_access");

	if((backend->db == NULL) || (strlen(backend->db)<1))
	{
		if(fts_xapian_settings.verbose>0) i_warning("FTS Xapian: check_write : no DB name");
		return false;
	}

	if(backend->dbw != NULL) return true;

	try
	{
		if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Opening DB (RW) %s",backend->db);
		backend->dbw = new Xapian::WritableDatabase(backend->db,Xapian::DB_CREATE_OR_OPEN | Xapian::DB_RETRY_LOCK | Xapian::DB_BACKEND_GLASS | Xapian::DB_NO_SYNC);
	}
	catch(Xapian::Error e)
	{
		i_error("FTS Xapian: Can't open Xapian DB (RW) (%s) %s : %s - %s",backend->boxname,backend->db,e.get_type(),e.get_error_string());
		return false;
	}
	if(fts_xapian_settings.verbose>0) 
	{
		long n = backend->dbw->get_doccount();
		i_info("FTS Xapian: Opening DB (RW) %s (%ld docs stored): Done",backend->db,n);
	}
	return true;
}

static void fts_backend_xapian_oldbox(struct xapian_fts_backend *backend)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_oldbox");

	if(backend->old_guid != NULL)
	{
		/* Performance calculator*/
		long dt = fts_backend_xapian_current_time() - backend->perf_dt;
		double r=0;
		if(dt>0)
		{
			r=backend->perf_nb*1000.0;
			r=r/dt;
		}
		/* End Performance calculator*/

		if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Done indexing '%s' (%s) (%ld msgs in %ld ms, rate: %.1f)",backend->old_boxname, backend->old_guid,backend->perf_nb,dt,r);

		i_free(backend->old_guid); backend->old_guid = NULL;
		i_free(backend->old_boxname); backend->old_boxname = NULL;
	}
}

static void fts_backend_xapian_release(struct xapian_fts_backend *backend, const char * reason, long commit_time)
{
	bool err=false;

	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: fts_backend_xapian_release (%s)",reason);

	if(commit_time<1) commit_time = fts_backend_xapian_current_time();

	long n = 0;
	if(backend->dbw !=NULL)
	{
		if(fts_xapian_settings.verbose>0) n = backend->dbw->get_doccount();
		try
		{
			backend->dbw->commit();
			backend->dbw->close();
		}
		catch(Xapian::Error e)
		{
			i_error("FTS Xapian: %s : %s - %s",reason,e.get_type(),e.get_error_string());
			err=true;
		}
		delete(backend->dbw);
		backend->dbw = NULL;
		backend->commit_updates = 0;
		backend->commit_time = commit_time;
	}

	if(err)
	{
		if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Re-creating index database due to error");
		try
		{
			Xapian::WritableDatabase * db = new Xapian::WritableDatabase(backend->db,Xapian::DB_CREATE_OR_OVERWRITE | Xapian::DB_RETRY_LOCK | Xapian::DB_BACKEND_GLASS | Xapian::DB_NO_SYNC);
			db->close();
			delete(db);
		}
		catch(Xapian::Error e)
		{
			i_error("FTS Xapian: Can't re-create Xapian DB (%s) %s : %s - %s",backend->boxname,backend->db,e.get_type(),e.get_error_string());
		}
	}

	if(fts_xapian_settings.verbose>0)
	{
		if(n>0)
		{
			i_info("FTS Xapian: Committed '%s' in %ld ms (%ld docs in index)",reason,fts_backend_xapian_current_time() - commit_time,n);
		}
		else
		{
			i_info("FTS Xapian: Committed '%s' in %ld ms",reason,fts_backend_xapian_current_time() - commit_time);
		}
	}
}

XResultSet * fts_backend_xapian_query(Xapian::Database * dbx, XQuerySet * query, long limit=0)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_query");

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
		i_error("FTS Xapian: xapian_query %s - %s",e.get_type(),e.get_error_string());
	}
	return set;
}

static void fts_backend_xapian_do_expunge(const char *fpath)
{
	Xapian::WritableDatabase * dbw = NULL;

	long dt = fts_backend_xapian_current_time();

	try
	{
		dbw = new Xapian::WritableDatabase(fpath,Xapian::DB_CREATE_OR_OPEN | Xapian::DB_RETRY_LOCK | Xapian::DB_BACKEND_GLASS | Xapian::DB_NO_SYNC);
	}
	catch(Xapian::Error e)
	{
		i_error("FTS Xapian: Can't open Xapian DB %s : %s - %s",fpath,e.get_type(),e.get_error_string());
		return;
	}

	XResultSet * result = NULL;
	XQuerySet * xq = new XQuerySet();

	try
	{
		xq->add(hdrs_emails[XAPIAN_EXPUNGE_HEADER],"1");
		result=fts_backend_xapian_query(dbw,xq,1);
	}
	catch(Xapian::Error e)
	{
		i_error("FTS Xapian: Expunging (1) '%s' : %s - %s",fpath,e.get_type(),e.get_error_string());
		if(result != NULL) { delete(result); result = NULL; }
	}

	delete(xq);

	if(result == NULL) { dbw->close(); delete(dbw); return; }

	Xapian::docid docid;

	long j=result->size;
	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Expunging (2) '%s' : %ld to do",fpath,j);

	while(j>0)
	{
		if(dbw != NULL) 
		{
			if(!fts_backend_xapian_test_memory())
			{
				if(fts_xapian_settings.verbose>0) i_warning("FTS Xapian: Expunging with low memory (%ld MB)-> Committing on disk",long(fts_backend_xapian_get_free_memory()/1024.0));
				dbw->commit();
				dbw->close();
				delete(dbw);
				dbw=NULL;
			}
		}

		if(dbw == NULL)
		{
			try
        		{
				dbw = new Xapian::WritableDatabase(fpath,Xapian::DB_CREATE_OR_OPEN | Xapian::DB_RETRY_LOCK | Xapian::DB_BACKEND_GLASS | Xapian::DB_NO_SYNC);
			}
			catch(Xapian::Error e)
			{
				i_error("FTS Xapian: Can't reopen Xapian DB %s : %s - %s",fpath,e.get_type(),e.get_error_string());
				delete(result);
				return;
			}
			catch(const std::exception &e)
                        {
				i_error("FTS Xapian: Can't reopen Xapian DB for expunging (%s) : Low memory (%s)",fpath, e.what());
				delete(result);
				return;
                        }
		}

		docid=result->data[j-1];
		try
		{
			if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Expunging (3a) UID=%d (Free mem= %ld MB) '%s'",docid,long(fts_backend_xapian_get_free_memory()/1024.0),fpath);
			if(docid>0) dbw->delete_document(docid);
			if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Expunging (3b) UID=%d (Free mem= %ld MB) done",docid,long(fts_backend_xapian_get_free_memory()/1024.0));
			j--;
		}
		catch(Xapian::Error e)
		{
			i_error("FTS Xapian: Expunging (4a) UID=%d '%s' : %s - %s",docid,fpath,e.get_type(),e.get_error_string());
			dbw->commit();
			dbw->close();
			delete(dbw);
			dbw=NULL;
		}
		catch(const std::exception &e2)
		{
			i_error("FTS Xapian: Expunging (4b) UID=%d '%s' : %s",docid,fpath,e2.what());
			dbw->commit();
			dbw->close();
			delete(dbw);
			dbw=NULL;
		}
	}
	delete(result);
	
	if(dbw != NULL)
	{
		dbw->commit();
		dbw->close();
		delete(dbw);
	}

	dt = fts_backend_xapian_current_time() - dt;
	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Expunging (5) '%s' done in %.2f secs",fpath,dt/1000.0);
}

static int fts_backend_xapian_unset_box(struct xapian_fts_backend *backend)
{
	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Unset box '%s' (%s)",backend->boxname,backend->guid);

	long commit_time = fts_backend_xapian_current_time();

	fts_backend_xapian_oldbox(backend);
	fts_backend_xapian_release(backend,"unset_box",commit_time);

	if(backend->db != NULL)
	{
		i_free(backend->db);
		backend->db = NULL;

		i_free(backend->guid);
		backend->guid = NULL;

		i_free(backend->boxname);
		backend->boxname = NULL;
	}

	return 0;
}

static int fts_backend_xapian_set_path(struct xapian_fts_backend *backend)
{
	struct mail_namespace * ns = backend->backend.ns;
	if(ns->alias_for != NULL)
	{
		if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Switching namespace");
		ns = ns->alias_for;
	}

	const char * path = mailbox_list_get_root_forced(ns->list, MAILBOX_LIST_PATH_TYPE_INDEX);

	if(backend->path != NULL) i_free(backend->path);
	backend->path = i_strconcat(path, "/" XAPIAN_FILE_PREFIX, static_cast<const char*>(NULL));

	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Index path = %s",backend->path);

	struct stat sb;
	if(!( (stat(backend->path, &sb)==0) && S_ISDIR(sb.st_mode)))
	{
		if (mailbox_list_mkdir_root(backend->backend.ns->list, backend->path, MAILBOX_LIST_PATH_TYPE_INDEX) < 0)
		{
			i_error("FTS Xapian: can not create '%s'",backend->path);
			i_error("FTS Xapian: You need to set mail_uid and mail_gid in your dovecot.conf according to the user of mail_location (%s)", path);
			return -1;
		}
	}
	return 0;
}

static int fts_backend_xapian_set_box(struct xapian_fts_backend *backend, struct mailbox *box)
{
	if (box == NULL)
	{
		if(backend->guid != NULL) fts_backend_xapian_unset_box(backend);
		if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: Box is empty");
		return 0;
	}

	const char * mb;
	fts_mailbox_get_guid(box, &mb );

	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Set box '%s' (%s)",box->name,mb);

	if((mb == NULL) || (strlen(mb)<3))
	{
		i_error("FTS Xapian: Invalid box");
		return -1;
	}

	if((backend->guid != NULL) && (strcmp(mb,backend->guid)==0))
	{
		if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: Box is unchanged");
		return 0;
	}

	if(backend->guid != NULL) fts_backend_xapian_unset_box(backend);

	if(fts_backend_xapian_set_path(backend)<0) return -1;

	long current_time = fts_backend_xapian_current_time();

	backend->commit_updates = 0;
	backend->commit_time = current_time;
	backend->guid = i_strdup(mb);
	backend->boxname = i_strdup(box->name);
	backend->db = i_strdup_printf("%s/db_%s",backend->path,mb);

	char * t = i_strdup_printf("%s/termlist.glass",backend->db);
	struct stat sb;
	if(!( (stat(t, &sb)==0) && S_ISREG(sb.st_mode)))
	{
		i_info("FTS Xapian: '%s' (%s) indexes do not exist. Initializing DB",backend->boxname,backend->db);
		try
		{
			Xapian::WritableDatabase * db = new Xapian::WritableDatabase(backend->db,Xapian::DB_CREATE_OR_OVERWRITE | Xapian::DB_RETRY_LOCK | Xapian::DB_BACKEND_GLASS);
			db->close();
			delete(db);
		}
		catch(Xapian::Error e)
		{
			i_error("FTS Xapian: Can't create Xapian DB (%s) %s : %s - %s",backend->boxname,backend->db,e.get_type(),e.get_error_string());
		}
	}
	i_free(t);

	/* Performance calculator*/
	backend->perf_dt = current_time;
	backend->perf_uid=0;
	backend->perf_nb=0;
	backend->perf_pt=0;
	/* End Performance calculator*/

	return 0;
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
			long i=0,j=strlen(hdr);
			std::string f2;
			while(i<j)
			{
				if((hdr[i]>' ') && (hdr[i]!='"') && (hdr[i]!='\'') && (hdr[i]!='-'))
				{
					f2+=tolower(hdr[i]);
				}
				i++;
			}
			char * h = i_strdup(f2.c_str());
			qs->add(h,a->value.str,a->match_not);
			i_free(h);
		}
		a->match_always=true;
		a = a->next;
	}
}

bool fts_backend_xapian_index_hdr(struct xapian_fts_backend *backend, uint uid, const char* field, icu::UnicodeString* data)
{
	bool ok=true;

	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: fts_backend_xapian_index_hdr");

	Xapian::WritableDatabase * dbx = backend->dbw;

	if(data->length()<fts_xapian_settings.partial) return true;

	if(strlen(field)<1) return true;

	long i=0;
	while((i<HDRS_NB) && (strcmp(field,hdrs_emails[i])!=0))
	{
		i++;
	}
	if(i>=HDRS_NB) return true;

	const char * h = hdrs_xapian[i];

	XQuerySet * xq = new XQuerySet();
	char *u = i_strdup_printf("%d",uid);
	xq->add("uid",u);
	i_free(u);

	XResultSet * result=fts_backend_xapian_query(dbx,xq,1);

	Xapian::docid docid;
	Xapian::Document * doc = NULL;
	try
	{
		if(result->size<1)
		{
			doc = new Xapian::Document();
			doc->add_value(1,Xapian::sortable_serialise(uid));
			u = i_strdup_printf("Q%d",uid);
			doc->add_term(u);
			docid=dbx->add_document(*doc);
			i_free(u);
		}
		else
		{
			docid=result->data[0];
			doc = new Xapian::Document(dbx->get_document(docid));
		}
	}
	catch(Xapian::Error e)
	{
		i_error("FTS Xapian: fts_backend_xapian_index_hdr : %s - %s",e.get_type(),e.get_error_string());
		if(doc!=NULL) delete(doc);
		ok=false;
	}

	delete(result);
	delete(xq);

	if(!ok) return false;

	XNGram * ngram = new XNGram(h);
	ngram->add(data);

	if(fts_xapian_settings.verbose>0)
	{
		i_info("FTS Xapian: Ngram(%s) -> %ld items (total %ld KB)",h,ngram->size, ngram->memory/1024);
	}

	for(i=0;i<ngram->size;i++)
	{
		u = i_strdup_printf("%s%s",h,ngram->data[i]);
		try
		{
			doc->add_term(u);
		}
		catch(Xapian::Error e)
		{
			i_error("FTS Xapian: fts_backend_xapian_index_hdr : %s - %s",e.get_type(),e.get_error_string());
			ok=false;
		}
		i_free(u);
	}
	delete(ngram);

	if(ok)
	{
		try
		{
			dbx->replace_document(docid,*doc);
		}
		catch (const std::exception &e)
		{
			i_warning("FTS Xapian: Memory too low (hdr) '%s'",e.what());
			ok = false;
		}
	}

	delete(doc);

	return ok;
}

bool fts_backend_xapian_index_text(struct xapian_fts_backend *backend,uint uid, const char * field, icu::UnicodeString * data)
{
	bool ok = true;

	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: fts_backend_xapian_index_text");

	Xapian::WritableDatabase * dbx = backend->dbw;

	if(data->length()<fts_xapian_settings.partial) return true;

	XQuerySet * xq = new XQuerySet();

	const char *u = t_strdup_printf("%d",uid);
	xq->add("uid",u);

	XResultSet * result=fts_backend_xapian_query(dbx,xq,1);

	Xapian::docid docid = 0;
	Xapian::Document * doc = NULL;

	try
	{
		if(result->size<1)
		{
			doc = new Xapian::Document();
			doc->add_value(1,Xapian::sortable_serialise(uid));
			u = t_strdup_printf("Q%d",uid);
			doc->add_term(u);
			docid=dbx->add_document(*doc);
		}
		else
		{
			docid=result->data[0];
			doc = new Xapian::Document(dbx->get_document(docid));
		}
	}
	catch(Xapian::Error e)
	{
		i_error("FTS Xapian: fts_backend_xapian_index_text : %s - %s",e.get_type(),e.get_error_string());
		if(doc!=NULL) delete(doc);
		ok=false;
	}

	delete(result);
	delete(xq);

	if(!ok) return false;

	Xapian::Document * doc2 = new Xapian::Document();
	Xapian::TermGenerator * termgenerator = new Xapian::TermGenerator();;
	Xapian::Stem stem("none");
	termgenerator->set_stemmer(stem);
	termgenerator->set_document(*doc2);

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
	termgenerator->set_stemming_strategy(Xapian::TermGenerator::STEM_NONE);
	termgenerator->index_text_without_positions(s, 1, h);

	long l = strlen(h);
	long n = doc2->termlist_count();
	Xapian::TermIterator * ti = new Xapian::TermIterator(doc2->termlist_begin());

	XNGram * ngram = new XNGram(h);
	const char * c;
	while(n>0)
	{
		s = *(*ti);
		c=s.c_str();
		if(strncmp(c,h,l)==0)
		{
			ngram->add(c+l);
		}
		(*ti)++;
		n--;
	}

	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: NGRAM(%s,%s) -> %ld items, max length=%ld, (total %ld KB)",field,h,ngram->size,ngram->maxlength,ngram->memory/1024);

	char *t = (char*)i_malloc(sizeof(char)*(ngram->maxlength+6));
	for(n=0;n<ngram->size;n++)
	{
		snprintf(t,ngram->maxlength+6,"%s%s",h,ngram->data[n]);
		try
		{
			doc->add_term(t);
		}
		catch(Xapian::Error e)
		{
			i_error("FTS Xapian: xapian_index_text : %s - %s",e.get_type(),e.get_error_string());
			ok=false;
		}
	}
	i_free(t);
	delete(ngram);
	delete(ti);
	delete(termgenerator);
	delete(doc2);

	if(ok)
	{
		try
		{
			dbx->replace_document(docid,*doc);
		}
		catch (const std::exception &e)
		{
			i_warning("FTS Xapian: Memory too low (text) '%s'",e.what());
			ok = false;
		}
	}

	delete(doc);

	return ok;
}

