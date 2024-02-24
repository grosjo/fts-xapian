/* Copyright (c) 2019 Joan Moreau <jom@grosjo.net>, see the included COPYING file */

static void fts_backend_xapian_lock(struct xapian_fts_backend *backend, const char * reason)
{
	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian : Mutex ON : %s",reason);
	backend->mutex.lock();
	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian : Mutex ON OK : %s",reason);
}

static void fts_backend_xapian_unlock(struct xapian_fts_backend *backend, const char * reason)
{               
        if(fts_xapian_settings.verbose>0) i_info("FTS Xapian : Mutex OFF : %s",reason);
        backend->mutex.unlock();
        if(fts_xapian_settings.verbose>0) i_info("FTS Xapian : Mutex OFF OK : %s",reason);
}
 
static long fts_backend_xapian_current_time()
{
        struct timeval tp;
        gettimeofday(&tp, NULL);
        return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

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
		Xapian::Query::op global_op;
		bool item_neg; // for the term
		long qsize;
		long limit;

	XQuerySet()
	{
		qsize=0; qs=NULL;
		limit=1;
		header=NULL;
		text=NULL;
		global_op = Xapian::Query::op::OP_OR;		
		accentsConverter=NULL;
	}

	XQuerySet(Xapian::Query::op op, long l)
	{
		qsize=0; qs=NULL;
		limit=1;
		if(l>1) { limit=l; }
		header=NULL;
		text=NULL;
		global_op=op;
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
		h->trim();
                h->toLower();
                if(h->length()<1) return;

		long i,j,k;
		XQuerySet * q2;
		icu::UnicodeString *r1,*r2;
		std::string st1,st2;

		t->toLower();
		k=CHARS_SEP;
		while(k>0)
                {
                        t->findAndReplace(chars_sep[k-1],CHAR_SPACE);
                        k--;
                }
		t->trim();
		if(t->length()<limit) return;
	
		i = t->lastIndexOf(CHAR_SPACE);
                if(i>0)
                {
			if(is_neg)
			{
                        	q2 = new XQuerySet(Xapian::Query::OP_AND_NOT,limit);
			}
			else
			{
				q2 = new XQuerySet(Xapian::Query::OP_AND,limit);
			}
                        while(i>0)
                        {
                                j = t->length();
                                r1 = new icu::UnicodeString(*t,i+1,j-i-1);
                                q2->add(h,r1,false);
                                delete(r1);
                                t->truncate(i);
                                t->trim();
                                i = t->lastIndexOf(CHAR_SPACE);
                        }
                        q2->add(h,t,false);
                        if(q2->count()>0) add(q2); else delete(q2);
                        return;
                }
		st1.clear();
		h->toUTF8String(st1);
		char * h2 = i_strdup(st1.c_str());
		i=0;
                while((i<HDRS_NB) && (strcmp(h2,hdrs_emails[i])!=0))
                {
                        i++;
                }
                if(i>=HDRS_NB)
                {
                        i_free(h2);
                        return;
                }

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

		k=CHARS_PB;
                while(k>0)
                {
                        t->findAndReplace(chars_pb[k-1],CHAR_KEY);
                        k--;
                }
		st2.clear();
		t->toUTF8String(st2);
		char * t2 = i_strdup(st2.c_str());

		if(strcmp(h2,XAPIAN_WILDCARD)==0)
		{
			if(is_neg)
			{
				q2 = new XQuerySet(Xapian::Query::OP_AND_NOT,limit);
			}
			else
			{
                        	q2 = new XQuerySet(Xapian::Query::OP_OR,limit);
			}
			for(i=1;i<HDRS_NB;i++)
			{
				if(i!=XAPIAN_EXPUNGE_HEADER) q2->add(hdrs_emails[i],t2,false);
			}
			add(q2);
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

		i_free(h2); i_free(t2);
		q2 = new XQuerySet(Xapian::Query::OP_AND,limit);
		q2->add(h,t,is_neg);
		add(q2);
	}

	void add(XQuerySet *q2)
	{
		if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: XQuerySet->addQ()");
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

	int count()
	{
		int c=0;
		if(text!=NULL) c=1;
		c+=qsize;
		return c;
	}

	std::string get_string()
	{
		std::string s("");

		if(count()<1) return s;

		if(text!=NULL)
		{
			if(item_neg) s.append("NOT ( ");
			s.append(header);
			s.append(":");
			s.append("\"");
			s.append(text);
			s.append("\"");
			if(item_neg) s.append(")");
		}

		const char * op;
		switch(global_op)
		{
			case Xapian::Query::OP_OR : op=" OR "; break;
			case Xapian::Query::OP_AND : op=" AND "; break;
			case Xapian::Query::OP_AND_NOT : op=" AND NOT "; break;
			default : op=" ERROR ";
		}	

		for (int i=0;i<qsize;i++)
		{
			int c=qs[i]->count();
			if(c<1) continue;

			if(s.length()>0) s.append(op);

			if(c>1)
			{
				s.append("(");
				s.append(qs[i]->get_string());
				s.append(")");
			}
			else s.append(qs[i]->get_string());
		}
		return s;
	}

	Xapian::Query * get_query(Xapian::Database * db)
	{
		Xapian::Query * q = NULL;
		Xapian::Query *q2, *q3;

		if(text!=NULL)
                {
			std::string s(header);
                        s.append(":");
                        s.append("\"");
                        s.append(text);
                        s.append("\"");

			Xapian::QueryParser * qp = new Xapian::QueryParser();
			for(int i=0; i< HDRS_NB; i++) qp->add_prefix(hdrs_emails[i], hdrs_xapian[i]);
			qp->set_database(*db);
			q = new Xapian::Query(qp->parse_query(s.c_str(),Xapian::QueryParser::FLAG_DEFAULT));
			delete (qp);
			if(item_neg)
			{
				q2 = new Xapian::Query(Xapian::Query::MatchAll);
				q3 = new Xapian::Query(Xapian::Query::OP_AND_NOT,*q2,*q);
				delete(q2);
				delete(q);
				q=q3;
			}
		}
		if(qsize<1)
		{
			if(q==NULL) q = new Xapian::Query(Xapian::Query::MatchNothing);
			return q;
                }

		if(q==NULL)
		{	
			q=qs[0]->get_query(db);
		}
		else
		{
			q2 = new Xapian::Query(global_op,*q,*(qs[0]->get_query(db)));
			delete(q);
			q=q2;
		}
		for (int i=1;i<qsize;i++)
		{
			q2 = new Xapian::Query(global_op,*q,*(qs[i]->get_query(db)));
			delete(q);
			q=q2;
		}
		return q;
	}
};

class XNGram
{
	private:
		long hardlimit;
		bool onlyone;
		icu::Transliterator *accentsConverter;
	
	public:
		long size;
		long memory;
		long maxlength;
		char ** data;

	XNGram(const char * pre)
	{
		size = 0;
		memory = 0;
		maxlength = 0;
		data = NULL;
		hardlimit = XAPIAN_TERM_SIZELIMIT-strlen(pre);
		onlyone = false;
		if(strcmp(pre,"XMID")==0) onlyone=true;
		accentsConverter = NULL;
	}

	~XNGram()
	{
		long i;
		if (data != NULL)
		{
			for(i=0;i<size;i++)
			{
				free(data[i]);
			}
			free(data);
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
		long i,j,k;
		icu::UnicodeString *r1,*r2;

		d->toLower();
		k=CHARS_SEP;
                while(k>0)
                {
                        d->findAndReplace(chars_sep[k-1],CHAR_SPACE);
                        k--;
                }
		d->trim();
                i = d->lastIndexOf(CHAR_SPACE);
                if(i>0)
                {
                        r1 = new icu::UnicodeString(*d,0,i);
                        r2 = new icu::UnicodeString(*d,i+1,d->length()-i-1);
                        add(r1);
                        add(r2);
                        delete(r1);
                        delete(r2);
                        return;
                }

		k=CHARS_PB;
                while(k>0)
                {
			d->findAndReplace(chars_pb[k-1],CHAR_KEY);
			k--;
		}
                
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
       
                k = d->length();
                if(k<fts_xapian_settings.partial) return;

                if(onlyone)
                {
                        add_stem(d);
                        return;
                }
	
		for(i=0;i<=k-fts_xapian_settings.partial;i++)
		{
			for(j=fts_xapian_settings.partial;(j+i<=k)&&(j<=fts_xapian_settings.full);j++)
			{
				r1 = new icu::UnicodeString(*d,i,j);
				add_stem(r1);
				delete(r1);
			}
		}
		if(k>fts_xapian_settings.full) add_stem(d);
	}

	bool stem_trim(icu::UnicodeString* d)
	{
        	bool b=false;
        	while((d->indexOf(CHAR_KEY)==0)|| (d->indexOf(CHAR_SPACE)==0))
        	{
                	d->remove(0,1);
                	b=true;
        	}
        	long i = std::max(d->lastIndexOf(CHAR_KEY),d->lastIndexOf(CHAR_SPACE));
        	while((i>0) && (i==d->length()-1))
        	{       
                	d->remove(i,1);
                	i = std::max(d->lastIndexOf(CHAR_KEY),d->lastIndexOf(CHAR_SPACE));
                	b=true;
        	}
        	return b;
	} 

	void add_stem(icu::UnicodeString *d)
	{
		long l,i,p;
		std::string s;
		char * s2;

		d->trim();
		l=d->length();
		if(l<fts_xapian_settings.partial) return;

		s.clear();
		d->toUTF8String(s);
		l  = s.length();
		if(l<=hardlimit)
		{
			l = strlen(s.c_str());
			s2 = (char *)malloc((l+1)*sizeof(char));
			strcpy(s2, s.c_str());
			p=0;

			if(size<1)
			{
				data=(char **)malloc(sizeof(char*));
				size=1;
				data[0]=s2;
				memory+= ((l+1) * sizeof(data[0][0]));
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
					free(s2);
				}
				else
				{
					//data=(char **)i_realloc(data,size*sizeof(char*),(size+1)*sizeof(char*));
					data=(char **)realloc(data,(size+1)*sizeof(char*));
					i=size;
					while(i>p)
					{
						data[i]=data[i-1];
						i--;
					}
					data[p]=s2;
					size++;
					memory+= ((l+1) * sizeof(data[p][0]));
				}
			}
			if(l>maxlength) { maxlength=l; }
		}
		if(stem_trim(d)) add_stem(d);
	}
};

class XDoc
{
	private:
                std::vector<std::string *> * data;
		std::vector<icu::UnicodeString *> * strings;
		std::vector<std::string *> * headers;
	public:
		long uid,size,stems;
		char * uterm;
		Xapian::Document * xdoc;
 
        XDoc(long luid)
	{
		uid=luid;
		data = new std::vector<std::string *>;
		data->clear();
		strings = new std::vector<icu::UnicodeString *>;
		strings->clear();
		headers = new std::vector<std::string *>;
		headers->clear();
		size=0;
		stems=0;
		std::string s;
                s.append("Q"+std::to_string(uid));
		uterm = (char*)malloc((s.length()+1)*sizeof(char));
		strcpy(uterm,s.c_str());
		xdoc=NULL;
	}

	~XDoc() 
	{
		for (std::string *s : *data)
		{
			delete(s);
		}
		data->clear(); delete(data);
		for (std::string *s : *headers)
		{
			delete(s);
		}
		headers->clear(); delete(headers);
		for (icu::UnicodeString * t : *strings)
		{
			delete(t);
		}
		strings->clear(); delete(strings);
		if(xdoc!=NULL) delete(xdoc);
		free(uterm);
	}

	void add(const char *h, icu::UnicodeString* t)
	{
		icu::UnicodeString * t2 = new icu::UnicodeString(*t);
		strings->push_back(t2);

		std::string * h2 = new std::string(h);
		headers->push_back(h2);
	
		size++;
	}

	void populate_stems()
	{
		long i,j; 
		while((j=headers->size())>0)
		{
			XNGram * ngram = new XNGram(headers->at(j-1)->c_str());
			ngram->add(strings->at(j-1));

			for(i=0;i<ngram->size;i++)
                	{
				std::string *s = new std::string();
				s->clear();
				s->append(headers->at(j-1)->c_str()); 
				s->append(ngram->data[i]);
                        	add(s);
			}
			delete(headers->at(j-1)); headers->pop_back();
			delete(strings->at(j-1)); strings->pop_back();
                }
	}

	void add(std::string *s)
	{
		long i=data->size();
		while((i>0) && (data->at(i-1)->compare(*s)>0))
                {
                        i--;
                }
		if((i>0) && (data->at(i-1)->compare(*s)==0))
		{
			delete(s);
			return;
		}
		stems++;
		data->insert(data->begin()+i,s);
	}

	void create_document()
	{
		xdoc = new Xapian::Document();
		xdoc->add_value(1,Xapian::sortable_serialise(uid));
		xdoc->add_term(uterm);
		for(long i=0;i<data->size();i++)
		{
 			xdoc->add_term(data->at(i)->c_str());
			delete(data->at(i)); 
			data->at(i)=NULL;
		}
		data->clear();
	} 
};

static void fts_backend_xapian_worker(void *p);

class XDocsWriter
{
	private:
		char * title;
		char * dbpath;
		XDocs * docs;
		std::mutex * m;
		bool terminated;
		Xapian::WritableDatabase * * dbw;
		bool verbose;
		long start_time;
		long * totaldocs;
	public:
		
	XDocsWriter(struct xapian_fts_backend *backend)
	{
		dbpath=(char *)malloc((strlen(backend->db)+1)*sizeof(char));
		strcpy(dbpath,backend->db);
        	backend->threads_total++;
                        
        	std::string s;
		s.clear(); s.append("FTS Xapian: DocsWriter #");
        	s.append(std::to_string(backend->threads_total));
        	s.append(" (");
        	s.append(backend->boxname);
        	s.append(") - ");
		title=(char *)malloc((s.length()+1)*sizeof(char));
		strcpy(title,s.c_str());

		docs = backend->docs;
		backend->docs = new XDocs();
		m=&(backend->mutex);
		terminated = false;
		dbw=&(backend->dbw);
		totaldocs = &(backend->total_added_docs);
		verbose=(fts_xapian_settings.verbose>0);
	}

	~XDocsWriter()
	{
		if(docs !=NULL)
		{
			long i;
			while((i=docs->size())>0)
			{
				delete(docs->at(i-1));
				docs->pop_back();
			}
			delete(docs);
			docs=NULL;
		}
		free(dbpath);
		free(title);
	}

	void lock(const char * reason)
	{       
        	if(verbose) syslog(LOG_INFO,"%sMutex ON : %s",title,reason);
        	m->lock();
		if(verbose) syslog(LOG_INFO,"%sMutex ON OK : %s",title,reason);
	}

	void unlock(const char * reason)
        {
                if(verbose) syslog(LOG_INFO,"%sMutex OFF : %s",title,reason);
                m->unlock();
                if(verbose) syslog(LOG_INFO,"%sMutex OFF OK : %s",title,reason);
        }

	void terminate()
	{
		lock("xw terminate");
		terminated=true;
		unlock("xw terminate");
	}

	bool isTerminated()
	{
		return terminated;
	}

	void launch()
	{
		i_info("%s LAUNCH",title);
		if(strlen(dbpath)<1)
                {
                        i_info("%sOpenDB: no DB name",title);
			terminate();
			return;
		}

                if((docs == NULL) || (docs->size()<1))
                {
                        i_info("%sOpenDB: no docs to write",title);
			terminate();
			return;
		}
		new std::thread(fts_backend_xapian_worker,this);
	}

	bool checkDB()
	{
		if(*dbw == NULL)
                {
                        try
                        {
                                if(verbose) syslog(LOG_INFO,"%sOpening %s",title,dbpath);
                                *dbw = new Xapian::WritableDatabase(dbpath,Xapian::DB_CREATE_OR_OPEN | Xapian::DB_RETRY_LOCK | Xapian::DB_BACKEND_GLASS);
                                if(verbose) syslog(LOG_INFO,"%sDBW created",title);
                        }
                        catch(Xapian::Error e)
                        {
                                syslog(LOG_ERR,"%sCan't open Xapian DB : %s - %s",title,e.get_type(),e.get_error_string());
                                terminate();
                                return false;
                        }
                        long nbdocs = (*dbw)->get_doccount();
                        if(verbose) syslog(LOG_INFO,"%sOpenDB successful (%ld docs existing)",title,nbdocs);
                }
		return true;
	}

	void worker()
	{
		start_time = fts_backend_xapian_current_time();
                long n=docs->size();
		long i=0;
                while(i<n)
                {
                        if(verbose) syslog(LOG_INFO,"%sProcessing #%ld (%ld/%ld)",title,docs->at(i)->uid,i+1,n);
                        docs->at(i)->populate_stems();
                        docs->at(i)->create_document();
			i++;
                }
                bool err=false;
                long newdoc=0;
		i=n;
                XDoc * doc;
                while(i>0)
                {
                        i--;
                        doc = docs->at(i);
                        docs->pop_back();
                        if(verbose) syslog(LOG_INFO,"%sPushing Doc %ld (%ld/%ld) with %ld strings and %ld stems",title,doc->uid,i+1,n,doc->size,doc->stems);
                        if(doc->stems > 0)
                        {
                                try
                                {
					lock("replace doc");
					if(checkDB())
					{
                                        	(*dbw)->replace_document(doc->uterm,*(doc->xdoc));
						(*totaldocs)++;
					}
					unlock("replace doc");
                                }
                                catch(Xapian::Error e)
                                {
                                        syslog(LOG_ERR,"%sCan't add document : %s - %s",title,e.get_type(),e.get_error_string());
                                        err=true;
                                }
                                catch(std::exception e)
                                {
                                        syslog(LOG_ERR,"%sCan't add document2 : %s",title,e.what());
                                        err=true;
                                }
                        }
                        delete(doc);
                        newdoc++;
                }
                if(verbose) syslog(LOG_INFO,"%sWrote %ld new docs in %ld ms",title,newdoc,fts_backend_xapian_current_time() - start_time);
		terminate();
	}
};

static void fts_backend_xapian_worker(void *p)
{
	openlog("xapian_worker",LOG_PID,LOG_MAIL);
	XDocsWriter *xw = (XDocsWriter *)p;
	xw->worker();
	closelog();
}
	
static long fts_backend_xapian_get_free_memory() // KB
{
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__)	
	uint32_t m,n;
	size_t len = sizeof(m);
	sysctlbyname("vm.stats.vm.v_free_count", &m, &len, NULL, 0);
	if(fts_xapian_settings.verbose>1) i_warning("FTS Xapian: (BSD) Free pages %ld",long(m));
	sysctlbyname("vm.stats.vm.v_cache_count", &n, &len, NULL, 0);
	if(fts_xapian_settings.verbose>1) i_warning("FTS Xapian: (BSD) Cached pages %ld",long(n));
	m = (m+n) * fts_xapian_settings.pagesize / 1024.0;
	if(fts_xapian_settings.verbose>1) i_warning("FTS Xapian: (BSD) Available memory %ld MB",long(m/1024.0));
	return long(m);
#else
        long m=0;
        char buffer[500];
        const char *p;
        FILE *f=fopen("/proc/meminfo","r");
        while(!feof(f))
        {       
                if ( fgets (buffer , 100 , f) == NULL ) break;
                p = strstr(buffer,"MemFree");
                if(p!=NULL)
                {
                        m+=atol(p+8);
                }
                p = strstr(buffer,"Cached");
                if(p==buffer)
                {
                        m+=atol(p+7);
                }
        }       
        if(fts_xapian_settings.verbose>1) i_warning("FTS Xapian: Free memory %ld MB",long(m/1024.0));
        fclose (f);     
        return m;
#endif
}

static long fts_backend_xapian_test_memory()
{
	long fri = fts_backend_xapian_get_free_memory(); // KB

	if(fri < ( fts_xapian_settings.lowmemory * 1024L ) ) return fri;
	
	return -1L;
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
		*dbr = new Xapian::Database(backend->db,Xapian::DB_CREATE_OR_OPEN  | Xapian::DB_RETRY_LOCK | Xapian::DB_BACKEND_GLASS);
	}
	catch(Xapian::Error e)
	{
		i_error("FTS Xapian: Can not open RO index (%s) %s : %s - %s",backend->boxname,backend->db,e.get_type(),e.get_error_string());
		return false;
	}
	return true;
}

static void fts_backend_xapian_oldbox(struct xapian_fts_backend *backend)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_oldbox");

	if(backend->old_guid != NULL)
	{
		/* Performance calculator*/
		long dt = fts_backend_xapian_current_time() - backend->start_time;
		double r=0;
		if(dt>0)
		{
			r=backend->total_added_docs*1000.0;
			r=r/dt;
		}
		/* End Performance calculator*/

		i_info("FTS Xapian: Done indexing '%s' (%s) (%ld msgs in %ld ms, rate: %.1f)",backend->old_boxname, backend->old_guid,backend->total_added_docs,dt,r);

		i_free(backend->old_guid); backend->old_guid = NULL;
		i_free(backend->old_boxname); backend->old_boxname = NULL;
	}

	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_oldbox - done");
}

static bool fts_backend_xapian_push(struct xapian_fts_backend *backend, const char * reason)
{
	if(backend->docs == NULL) 
	{
		backend->docs = new XDocs();
	}
	if(backend->docs->size()<1) return true;

	if((backend->threads).size()<(backend->threads_max))
	{
		XDocsWriter * x = new XDocsWriter(backend);
		(backend->threads).push_back(x);
		x->launch();
		return true;
	}
	fts_backend_xapian_lock(backend,"push");
	long i=0;
	bool found=false;
	while((i<(backend->threads).size()) && ((backend->threads)[i]!=NULL) && !((backend->threads)[i]->isTerminated()))
	{
		i++;
	}
	if(i<(backend->threads).size())
	{
		found=true;
		if((backend->threads)[i]!=NULL) delete((backend->threads)[i]);
		(backend->threads)[i] = new XDocsWriter(backend);
	}
	fts_backend_xapian_unlock(backend,"push");
	if(found) (backend->threads)[i]->launch();
	return found;
}

static void fts_backend_xapian_close(struct xapian_fts_backend *backend, const char * reason)
{
	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian : Closing threads (%s)",reason);
	long i;
	while((i=(backend->threads).size())>0)
	{
		i--;
		fts_backend_xapian_lock(backend,"close");
		if((backend->threads)[i]==NULL)
		{
			(backend->threads).pop_back();
			fts_backend_xapian_unlock(backend,"close1");
		}
		else if((backend->threads)[i]->isTerminated())
		{
			delete((backend->threads)[i]);
			(backend->threads).pop_back();
			fts_backend_xapian_unlock(backend,"close2");
		}
		else
		{
			fts_backend_xapian_unlock(backend,"close3");
			i_info("SLEEP 2");
			//usleep(500000);
			sleep(1);
		}
	}
	try     
        {
		if(backend->dbw != NULL) backend->dbw->close();        
                delete(backend->dbw);
		backend->dbw=NULL;
        }       
        catch(Xapian::Error e)
        {
                i_error("FTS Xapian: Can't close Xapian DB (%s) %s : %s - %s",backend->boxname,backend->db,e.get_type(),e.get_error_string());      
        }
}

static bool fts_backend_xapian_isnormalprocess()
{
    	char buff[PATH_MAX];
    	buff[0]=0;
    	ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff)-1);
    	if (len>0)  buff[len] = 0;
    	return (strstr(buff,"doveadm")==NULL);
}

XResultSet * fts_backend_xapian_query(Xapian::Database * dbx, XQuerySet * query, long limit=0)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_query (%s)",query->get_string().c_str());

	XResultSet * set= new XResultSet();
	Xapian::Query * q = query->get_query(dbx);

	try
	{
		Xapian::Enquire enquire(*dbx);
		enquire.set_query(*q);
		enquire.set_docid_order(Xapian::Enquire::DESCENDING);

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
	delete(q);
	return set;
}

static int fts_backend_xapian_unset_box(struct xapian_fts_backend *backend)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: Unset box '%s' (%s)",backend->boxname,backend->guid);

	long commit_time = fts_backend_xapian_current_time();

	while(!fts_backend_xapian_push(backend,"unset_box")) 
	{ 
		i_info("SLEEP 3");
		sleep(1); //usleep(500000); 
	}
	fts_backend_xapian_close(backend,"unset box");
	fts_backend_xapian_oldbox(backend);

	if(backend->db != NULL)
	{
		i_free(backend->db);
		backend->db = NULL;

		i_free(backend->guid);
		backend->guid = NULL;

		i_free(backend->boxname);
		backend->boxname = NULL;

		i_free(backend->expdb);
		backend->expdb = NULL;
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

	backend->start_time = current_time;
	backend->lastuid = -1;
	backend->guid = i_strdup(mb);
	backend->boxname = i_strdup(box->name);
	backend->db = i_strdup_printf("%s/db_%s",backend->path,mb);
	backend->expdb = i_strdup_printf("%s_exp.db",backend->db);
	backend->docs = NULL;
        backend->threads.clear();
        backend->threads_total = 0;

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

	return 0;
}

static void fts_backend_xapian_build_qs(XQuerySet * qs, struct mail_search_arg *a)
{
	const char * hdr;

	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_build_qs");

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
			XQuerySet * q2;
			if(a->match_not)
			{
				q2 = new XQuerySet(Xapian::Query::OP_AND_NOT,qs->limit);
			}
			else
			{
				q2 = new XQuerySet(Xapian::Query::OP_OR,qs->limit);
			}
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

bool fts_backend_xapian_index(struct xapian_fts_backend *backend, const char* field, icu::UnicodeString* data)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_index %s : %ld",field,(long)(data->length()));

	if(data->length()<fts_xapian_settings.partial) return true;

	if(strlen(field)<1) return true;

	long i=0;
	while((i<HDRS_NB) && (strcmp(field,hdrs_emails[i])!=0))
	{
		i++;
	}
	if(i>=HDRS_NB) i=HDRS_NB-1;
	const char * h = hdrs_xapian[i];

	backend->docs->back()->add(h,data);

	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_index %s done",field);

	return TRUE;
}

