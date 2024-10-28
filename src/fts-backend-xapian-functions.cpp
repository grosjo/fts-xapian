/* Copyright (c) 2019 Joan Moreau <jom@grosjo.net>, see the included COPYING file */

static long fts_backend_xapian_current_time()
{
        struct timeval tp;
        gettimeofday(&tp, NULL);
        return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

static long fts_backend_xapian_get_free_memory() // KB  
{
#if defined(__FreeBSD__) || defined(__NetBSD__)
        u_int page_size;
        uint_size uint_size = sizeof(page_size);
        sysctlbyname("vm.stats.vm.v_page_size", &page_size, &uint_size, NULL, 0);
	struct vmtotal vmt;
	size_t vmt_size = sizeof(vmt);
	sysctlbyname("vm.vmtotal", &vmt, &vmt_size, NULL, 0);
	long m = vmt.t_free * page_size / 1024.0f;
#else
	struct rlimit rl;
        if(getrlimit(RLIMIT_AS,&rl)!=0) i_warning("GETRLIMIT %s",strerror(errno));
        long m,l = rl.rlim_cur;

	char buffer[300];
	const char *p;
	FILE *f;
	if((l<1) && ((f=fopen("/proc/meminfo","r"))!=NULL))
	{
		m=0;
		while(!feof(f))
	        {
        	        if ( fgets (buffer , 100 , f) == NULL ) break;
			p = strstr(buffer,"MemAvailable:");
			if(p!=NULL)
                	{
                        	m=atol(p+13);
				break;
			}
		}
		if(m>0) l = m * 1024;
	}	
	long pid=getpid();
	sprintf(buffer,"/proc/%ld/status",pid);
        f=fopen(buffer,"r");
	m=0;
	if(f != NULL)
	{
        	while(!feof(f))                                                 
        	{                              
        	        if ( fgets (buffer , 100 , f) == NULL ) break;
        	        p = strstr(buffer,"VmData:");
        	        if(p!=NULL)
        	        {
        	                m+=atol(p+7);
        	        }                                                  
        	        p = strstr(buffer,"VmStk:");
        	        if(p!=NULL)
        	        {
        	                m+=atol(p+6);
        	        }
        	}
		fclose(f);
	}
	m = (l/1024.0f) - m;
#endif
	if(fts_xapian_settings.verbose>0) syslog(LOG_WARNING,"FTS Xapian: Available memory %ld MB",long(m/1024.0));
	return m;
}

static void fts_backend_xapian_get_lock(struct xapian_fts_backend *backend, long verbose, const char *s)
{
	std::unique_lock<std::timed_mutex> *lck;
	lck = new std::unique_lock<std::timed_mutex>(backend->mutex,std::defer_lock);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        while(!(lck->try_lock_for(std::chrono::milliseconds(1000 + std::rand() % 1000))))
	{
        	if(verbose>1) 
		{
			std::string sl("FTS Xapian: Waiting unlock... (");
			sl.append(s);
			sl.append(")");
			syslog(LOG_INFO,"%s",sl.c_str());
                }
	}
#pragma GCC diagnostic pop
	if(verbose>1)
	{
		std::string sl("FTS Xapian: Got lock (");
		sl.append(s);
                sl.append(")");
                syslog(LOG_INFO,"%s",sl.c_str());
	}
	backend->mutex_t = lck;
}

static void fts_backend_xapian_release_lock(struct xapian_fts_backend *backend, long verbose, const char *s)
{
	if(verbose>1)
	{
		std::string sl("FTS Xapian: Releasing lock (");
		sl.append(s);
		sl.append(")");
		syslog(LOG_INFO,"%s",sl.c_str());
	}
	if(backend->mutex_t !=NULL)
	{
		std::unique_lock<std::timed_mutex> *lck = backend->mutex_t;
		backend->mutex_t= NULL;
		delete(lck);
	}
}


static bool fts_backend_xapian_clean_accents(icu::UnicodeString *t)
{
	UErrorCode status = U_ZERO_ERROR;
	icu::Transliterator * accentsConverter = icu::Transliterator::createInstance("NFD; [:M:] Remove; NFC", UTRANS_FORWARD, status);
	if(U_FAILURE(status))
	{
		std::string s("FTS Xapian: Can not allocate ICU translator + FreeMem="+std::to_string(long(fts_backend_xapian_get_free_memory()/1024.0))+"MB");
		syslog(LOG_ERR,"%s",s.c_str());
		accentsConverter = NULL;
		return false;
	}
	accentsConverter->transliterate(*t);	
	delete(accentsConverter);
	return true;
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
		const char * header;
		icu::UnicodeString * text;
		XQuerySet ** qs;
		Xapian::Query::op global_op;
		bool item_neg; // for the term
		long qsize;

	public:
		long limit;

	XQuerySet()
	{
		qsize=0; qs=NULL;
		limit=1;
		header=NULL;
		text=NULL;
		global_op = Xapian::Query::op::OP_OR;
	}

	XQuerySet(Xapian::Query::op op, long l)
	{
		qsize=0; qs=NULL;
		limit=1;
		if(l>1) { limit=l; }
		header=NULL;
		text=NULL;
		global_op=op;
	}

	~XQuerySet()
	{
		if(text!=NULL) { delete(text); text=NULL; }

		for(long j=0;j<qsize;j++)
		{
			delete(qs[j]);
		}
		if(qsize>0) i_free(qs);
		qsize=0; qs=NULL;
	}

	void add(long uid)
	{
		std::string s = std::to_string(uid);
		icu::UnicodeString t(s.c_str());
		add(hdrs_emails[0],&t,false,false,false);
	}
		
	void add(const char * h2, icu::UnicodeString *t, bool is_neg,bool checkspaces, bool checklength)
	{
		if(h2==NULL) return;
		if(t==NULL) return;

		icu::UnicodeString h(h2);
		h.trim();
                if(h.length()<1) return;

		long i,j,k;
		XQuerySet * q2;
		icu::UnicodeString *r1,*r2;

		if(checkspaces)
		{
			h.toLower();
			t->toLower();
		
			k=CHARS_PB;
                	while(k>0)
                	{
                	        t->findAndReplace(chars_pb[k-1],CHAR_KEY);
                	        k--;
                	}

			k=CHARS_SEP;
			while(k>0)
                	{
                	        t->findAndReplace(chars_sep[k-1],CHAR_SPACE);
                	        k--;
                	}
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
                                q2->add(h2,r1,false,false,true);
                                delete(r1);
                                t->truncate(i);
                                t->trim();
                                i = t->lastIndexOf(CHAR_SPACE);
                        }
                        q2->add(h2,t,false,false,true);
                        if(q2->count()>0) add(q2); else delete(q2);
                        return;
                }

		if(h.compare(XAPIAN_WILDCARD)==0)
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
				if(i!=XAPIAN_EXPUNGE_HEADER) q2->add(hdrs_emails[i],t,false,false,true);
			}
			add(q2);
			return;
		}
                else
		{
                        i=0;
                        while((i<HDRS_NB) && (h.compare(hdrs_emails[i])!=0))
                        {
                                i++;
                        }
                        if(i>=HDRS_NB)
                        {
                                return;
                        }
			h2=hdrs_emails[i];
                }

		k=t->length()-fts_xapian_settings.full;
		if(checklength && (k>0))
		{
			if(is_neg)
                        {
                                q2 = new XQuerySet(Xapian::Query::OP_AND_NOT,limit);
                        }
                        else 
                        {
                                q2 = new XQuerySet(Xapian::Query::OP_OR,limit);
                        }
			q2->add(h2,t,false,false,false);
	
			icu::UnicodeString sub;
			for(j=0;j<k;j++)
			{
				sub.remove();
                                t->extract(j,j+fts_xapian_settings.full,sub);
                                q2->add(h2,&sub,false,false,false);
			}
			add(q2);
			return;
		}
		if(text==NULL)
		{
			text=new icu::UnicodeString(*t);
			header=h2;
			item_neg=is_neg;
			return;
		}

                q2 = new XQuerySet(Xapian::Query::OP_AND,limit);
		q2->add(h2,t,is_neg,false,false);
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
			text->toUTF8String(s);
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
                        text->toUTF8String(s);
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
		bool onlyone;
		icu::UnicodeString * prefix;
		icu::UnicodeString * * * storage;
		long * size;
		const char * title;
		long verbose;
	
	public:
		long maxlength;

	XNGram(icu::UnicodeString * * * d, long * asize, const char * t, long v)
	{
		verbose=v;
		maxlength = 0;
		storage = d;
		size = asize;
		title=t;
	}

	void setPrefix(icu::UnicodeString *pre)
	{
		onlyone = (pre->compare("XMID")==0);
		prefix = pre;
	}

	~XNGram()
	{
	}

	bool isBase64(icu::UnicodeString *d)
	{
		std::string s;
                d->toUTF8String(s);
		bool ok=false;
		std::regex base64Regex("^[A-Za-z0-9+/]*={0,2}$");
		if( (s.length()>=56) && (s.length() % 4 == 0))
		{
			ok=std::regex_match(s, base64Regex);
		}
		if(ok && (verbose>0)) syslog(LOG_INFO,"Testing Base64 (%s) -> %ld",s.c_str(),(long)ok);
		return ok;
	}

	void add(icu::UnicodeString *d)
	{
		if((*size)>XAPIAN_MAXTERMS_PERDOC) return;

		long i,j,k;
		icu::UnicodeString *r;

		d->trim();
                while((i = d->lastIndexOf(CHAR_SPACE))>0)
                {
			r = new icu::UnicodeString(*d,i+1);
			add(r);
			delete(r);
			d->truncate(i);
			d->trim();
                }

                k = d->length();
                if(k<fts_xapian_settings.partial) return;

		if(onlyone)
                {
                        add_stem(d);
                        return;
                }

		if(isBase64(d)) return;

		icu::UnicodeString * sub;

		for(i=0;i<=k-fts_xapian_settings.partial;i++)
		{
			for(j=fts_xapian_settings.partial;(j+i<=k)&&(j<=fts_xapian_settings.full);j++)
			{
				sub = new icu::UnicodeString();
				d->extract(i,j,*sub);
				add_stem(sub);
				delete(sub);
			}
		}
		if(k>fts_xapian_settings.full) add_stem(d);
	}

	bool stem_trim(icu::UnicodeString *d)
	{
		bool res=false;

		while(d->startsWith(CHAR_SPACE) || d->startsWith(CHAR_KEY))
		{
			d->remove(0,1);
			res=true;
		}
		while(d->endsWith(CHAR_SPACE) || d->endsWith(CHAR_KEY))
		{
			d->truncate(d->length()-1);
			res=true;
		}
		
        	return res;
	} 

	int psearch(icu::UnicodeString *d,long pos, long l)
	{
		if(l==0) return pos;

		long n = std::floor(l*0.5f);
		int c = (*storage)[pos+n]->compare(*d);

		// If already exist, return neg
		if(c==0) return -1;

		// If middle pos is lower than d, search after pos+n
		if(c<0) return psearch(d,pos+n+1,l-n-1);

		// All other case, search before
		return psearch(d,pos,n);
	}

	void add_stem(icu::UnicodeString *d)
	{
		long l,l2,i,p;

		if((*size)>XAPIAN_MAXTERMS_PERDOC) return;

		d->trim();
		if(d->length()<fts_xapian_settings.partial) return;
		
		icu::UnicodeString * st = new icu::UnicodeString(*d);
		st->insert(0,*prefix);
		l = st->length();

		{	
			std::string s;
			st->toUTF8String(s);
			l2 = strlen(s.c_str());
		}

		if(l2<XAPIAN_TERM_SIZELIMIT)
		{
			if((*size)<1)
			{
				*storage=(icu::UnicodeString **)malloc(sizeof(icu::UnicodeString *));
				(*size)=1;
				(*storage)[0]=st;
			}
			else
			{
				p=psearch(st,0,*size);
				if(p>=0)
				{
					i=(*size);
					(*storage)=(icu::UnicodeString **)realloc((*storage),(i+1)*sizeof(icu::UnicodeString *));
					while(i>p)
					{
						(*storage)[i]=(*storage)[i-1];
						i--;
					}
					(*storage)[p]=st;
					(*size)++;
				}
				else delete(st);
			}
			if(l>maxlength) { maxlength=l; }
		}
		else delete(st);
		if(stem_trim(d)) add_stem(d);
	}
};

class XDoc
{
	private:
                icu::UnicodeString * * data;
		std::vector<icu::UnicodeString *> * strings;
		std::vector<icu::UnicodeString *> * headers;
	public:
		long uid,size,stems;
		char * uterm;
		Xapian::Document * xdoc;
		long status;
 
        XDoc(long luid)
	{
		uid=luid;
		data = NULL;
		strings = new std::vector<icu::UnicodeString *>;
		strings->clear();
		headers = new std::vector<icu::UnicodeString *>;
		headers->clear();
		size=0;
		stems=0;
		std::string s;
                s.append("Q"+std::to_string(uid));
		uterm = (char*)malloc((s.length()+1)*sizeof(char));
		strcpy(uterm,s.c_str());
		xdoc=NULL;
		status=0;
	}

	~XDoc() 
	{
		if(data != NULL)
		{
			for(long i=0;i<stems;i++)
			{
				delete(data[i]);
			}
			free(data);
			data=NULL;
		}
		
		for (icu::UnicodeString * h : *headers)
		{
			delete(h);
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

	std::string getSummary()
	{
		std::string s("Doc "); 
		s.append(std::to_string(uid));
		s.append(" uterm=");
		s.append(uterm);
		s.append(" #lines=" + std::to_string(size));
		s.append(" #stems=" + std::to_string(stems));
		s.append(" status=" + std::to_string(status));
		return s;
	}

	void add(const char *h, icu::UnicodeString* t, long verbose, const char * title)
	{
		icu::UnicodeString * prefix = new icu::UnicodeString(h);
		prefix->trim();
		headers->push_back(prefix);

		icu::UnicodeString * t2 = new icu::UnicodeString(*t);
		t2->toLower();
		fts_backend_xapian_clean_accents(t2);

		long k=CHARS_SEP;
                while(k>0)
                {
                        t2->findAndReplace(chars_sep[k-1],CHAR_SPACE);
                        k--;
                }
		t2->trim();

		k=CHARS_PB;
                while(k>0)
                {
                        t2->findAndReplace(chars_pb[k-1],CHAR_KEY);
                        k--;
                }
		
		strings->push_back(t2);
		std::string s;
		t2->toUTF8String(s);
		size++;
	}

	void populate_stems(long verbose, const char * title)
	{
		long i,j,k;
		long t = fts_backend_xapian_current_time();
		k=headers->size();	
		if(verbose>0) syslog(LOG_INFO,"%s %s : Populate %ld headers with strings",title,getSummary().c_str(),k);

		XNGram * ngram = new XNGram(&data,&stems,title,verbose);
		j=headers->size();
		while(j>0)
		{
			j--;
			if(verbose>0) 
			{
				std::string s; headers->at(j)->toUTF8String(s);
				syslog(LOG_INFO,"%s %s : Populate %ld / %ld Header=%s TextLength=%ld",title,getSummary().c_str(),j+1,k,s.c_str(),(long)strings->at(j)->length());
			}
			
			ngram->setPrefix(headers->at(j));
			ngram->add(strings->at(j));
			delete(headers->at(j)); headers->at(j)=NULL; headers->pop_back();
			delete(strings->at(j)); strings->at(j)=NULL; strings->pop_back();
                }
		delete(ngram);
	
		t = fts_backend_xapian_current_time() -t;
		if(verbose>0) syslog(LOG_INFO,"%s %s : Done populating in %ld ms (%ld stems/sec)",title,getSummary().c_str(), t, (long)(stems*1000.0/t));
	}

	void create_document(long verbose, const char * title)
	{
		long i=stems;

		if(verbose>0) syslog(LOG_INFO,"%s adding %ld terms",title,stems);
		xdoc = new Xapian::Document();
		xdoc->add_value(1,Xapian::sortable_serialise(uid));
		xdoc->add_term(uterm);
		std::string *s;
		while(i>0)
		{
			i--;
			s = new std::string();
			data[i]->toUTF8String(*s);
 			xdoc->add_term(s->c_str());
			if(verbose>0) syslog(LOG_INFO,"%s adding terms for (%s) : %s",title,uterm,s->c_str());
			delete(s);
			delete(data[i]);
			data[i]=NULL;
		}
		free(data);
		data=NULL;
		if(verbose>0) syslog(LOG_INFO,"%s create_doc done (%s)",title,getSummary().c_str());
	} 
};

static void fts_backend_xapian_worker(void *p);

class XDocsWriter
{
	private:
		XDoc * doc;
		long verbose, lowmemory;
		std::thread *t;
		char * title;
		struct xapian_fts_backend *backend;

	public:
		bool started,toclose,terminated;
		
	XDocsWriter(struct xapian_fts_backend *b, long n)
	{
		backend=b;
		std::string s;
                s.clear(); s.append("DW #"+std::to_string(n)+" (");
                s.append(backend->boxname);
                s.append(",");
                s.append(backend->db);
                s.append(") - ");
                title=(char *)malloc((s.length()+1)*sizeof(char));
                strcpy(title,s.c_str());

		t=NULL;
		doc=NULL;
		toclose=false;
		terminated=false;
		started=false;
		verbose=fts_xapian_settings.verbose;
		lowmemory = fts_xapian_settings.lowmemory;
	}

	bool checkDB()
	{
		if(backend->dbw != NULL) return true;
             
		backend->pending=0;
           
                try
                {
			if(verbose>0)
			{
				std::string s(title);
				s.append("Opening DB");
				syslog(LOG_INFO,"%s",s.c_str());
			}
			backend->dbw = new Xapian::WritableDatabase(backend->db,Xapian::DB_CREATE_OR_OPEN | Xapian::DB_BACKEND_GLASS);
			return true;
		}
                catch(Xapian::DatabaseLockError e)
                {
			std::string s(title);
			s.append("Can't lock the DB : ");
			s.append(e.get_type());
			s.append(" - ");
			s.append(e.get_msg());
                        syslog(LOG_WARNING,"%s",s.c_str());
		}
                catch(Xapian::Error e)
                {
			std::string s(title);
			s.append("Can't open the DB RW : ");
                        s.append(e.get_type());
                        s.append(" - ");
                        s.append(e.get_msg());
                        syslog(LOG_WARNING,"%s",s.c_str());
                }
		return false;
	}

	~XDocsWriter()
	{
		free(title);
	}

	std::string getSummary()
	{
		std::string s(title);
		s.append(" remaining docs="+std::to_string(backend->docs.size()));
		s.append(" terminated="+std::to_string(terminated));
		return s;
	}

	bool launch(const char * from)
	{
		if(verbose>0)
		{
			std::string s(title);
			s.append("Launching thread from ");
			s.append(from);
			syslog(LOG_INFO,"%s",s.c_str());
		}

		try
		{
			t = new std::thread(fts_backend_xapian_worker,this);
		}
		catch(std::exception e)
		{
			std::string s(title);
			s.append("Thread error ");
			s.append(e.what());
			syslog(LOG_ERR,"%s",s.c_str());
			return false;
		}
		started=true;
		return true;
	}

	void close()
	{
		toclose=false;
		if(t!=NULL)
		{
			t->join();
			delete(t);
		}
		t=NULL;
		terminated=true;
	}

	void worker()
	{
		long start_time = fts_backend_xapian_current_time();
		XDoc *doc = NULL;
		long totaldocs=0;
		std::string s;

		while(!toclose)
		{
			if(doc==NULL)
			{
				if(verbose>0) { s=title; s.append("Searching doc"); if(verbose>0) syslog(LOG_INFO,"%s",s.c_str()); }

				fts_backend_xapian_get_lock(backend, verbose, title);
				long n=backend->docs.size();
				long i=0;
				while((i<n) && (backend->docs.at(i)->status!=1)) i++;
				if(i<n)
                        	{
					doc = backend->docs.at(i);
					backend->docs.at(i) = NULL;
					backend->docs.erase(backend->docs.begin() + i);
				}
				fts_backend_xapian_release_lock(backend, verbose, title);
			}

			if(doc==NULL)
			{
				if(verbose>0) { s=title; s.append("No-op"); syslog(LOG_INFO,"%s",s.c_str());	}
				std::this_thread::sleep_for(XSLEEP);
			}
			else if(doc->status==1)	
			{
				if(verbose>0) { s=title; s.append("Populating stems : "+doc->getSummary()); syslog(LOG_INFO,"%s",s.c_str()); }
				doc->populate_stems(verbose,title);
				doc->status=2;
			}
			else if(doc->status==2)
			{
				s=title; s.append("Creating Xapian doc : "+doc->getSummary());
				if(verbose>0) syslog(LOG_INFO,"%s",s.c_str());
				doc->create_document(verbose,s.c_str());
				doc->status=3;
			}
                        else
			{
				if(verbose>0) { s=title; s.append("Pushing : "+doc->getSummary()); syslog(LOG_INFO,"%s",s.c_str()); }
                        	if(doc->stems > 0)
                        	{
					fts_backend_xapian_get_lock(backend, verbose, title);

					// Memory check
                                        long m = fts_backend_xapian_get_free_memory();
                                        if(verbose>1) { s=title; s.append("Memory : Free = "+std::to_string((long)(m / 1024.0f))+" MB vs limit = "+std::to_string(lowmemory)+" MB"); syslog(LOG_WARNING,"%s",s.c_str()); }
                                        if((backend->dbw!=NULL) && ((backend->pending > XAPIAN_WRITING_CACHE) || (m<(lowmemory*1024)))) // too little memory or too many pendings
                                        {
					        try
                                                {
                                        		s=title;
                                                        s.append("Committing "+std::to_string(backend->pending)+" docs due to low free memory ("+ std::to_string((long)(m/1024.0f))+" MB)");
                                                        syslog(LOG_WARNING,"%s",s.c_str());
                                                        backend->dbw->close();
                                                        delete(backend->dbw);
                                                        backend->dbw=NULL;
							backend->pending = 0;
						}
                                                catch(Xapian::Error e)
                                                {
                                                        std::string s(title);
                                                        s.append("Can't commit DB1 : ");
                                                        s.append(e.get_type());
                                                        s.append(" - ");
                                                        s.append(e.get_msg());
                                                        syslog(LOG_ERR,"%s",s.c_str());
                                                }
                                                catch(std::exception e)
                                                {
                                                        std::string s(title);
                                                        s.append("Can't commit DB2 : ");
                                                        s.append(e.what());
                                                        syslog(LOG_ERR,"%s",s.c_str());
                                                }
					}
					
					if(checkDB())
					{
						try
                               	        	{
							if(verbose>0)
                                                       	{
                                                               	s=title;
                                                               	s.append("Replace doc : "+doc->getSummary()+" Free memory : "+std::to_string(long(m/1024.0))+"MB");
                                                               	syslog(LOG_INFO,"%s",s.c_str());
                                                       	}
                               	                	backend->dbw->replace_document(doc->uterm,*(doc->xdoc));
							backend->pending++;
							delete(doc);
							doc=NULL;
							if(verbose>0)
                                                        {
                                                                s=title;
                                                                s.append("Doc done");
                                                                syslog(LOG_INFO,"%s",s.c_str());
                                                        }
							totaldocs++;
                        	        	}
						catch(Xapian::Error e)
                               	                {
                               	                        s=title;
                               	                        s.append("Can't write doc1 : ");
                               	                        s.append(e.get_type());
                               	                        s.append(" - ");
                               	                        s.append(e.get_msg());
                               	                        syslog(LOG_ERR,"%s",s.c_str());
                               	                }
                               	                catch(std::exception e)
                               	                {
                               	                        s=title;
                               	                        s.append("Can't write doc2");
                               	                        syslog(LOG_ERR,"%s",s.c_str());
                               	                }
					}
					fts_backend_xapian_release_lock(backend, verbose, title);	
				}
				else 
				{
					delete(doc);
					doc=NULL;
				}
                        }
                }
		terminated=true;
                if(verbose>0) 
		{
			std::string s(title);
			s.append("Wrote "+std::to_string(totaldocs)+" within "+std::to_string(fts_backend_xapian_current_time() - start_time)+" ms");
			syslog(LOG_INFO,"%s",s.c_str());
		}
	}
};

static void fts_backend_xapian_worker(void *p)
{
	XDocsWriter *xw = (XDocsWriter *)p;
	xw->worker();
}
	
static bool fts_backend_xapian_open_readonly(struct xapian_fts_backend *backend, Xapian::Database ** dbr)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_open_readonly");

	if((backend->db == NULL) || (strlen(backend->db)<1))
	{
		i_warning("FTS Xapian: Open DB Read Only : no DB name");
		return false;
	}

	try
	{
		if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Opening DB (RO) %s",backend->db);
		*dbr = new Xapian::Database(backend->db,Xapian::DB_CREATE_OR_OPEN | Xapian::DB_BACKEND_GLASS);
	}
	catch(Xapian::Error e)
	{
		i_error("FTS Xapian: Can not open RO index (%s) %s : %s - %s %s ",backend->boxname,backend->db,e.get_type(),e.get_msg().c_str(),e.get_error_string());
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
			r=backend->total_docs*1000.0;
			r=r/dt;
		}
		/* End Performance calculator*/

		if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Done indexing '%s' (%s) (%ld msgs in %ld ms, rate: %.1f)",backend->old_boxname, backend->db,backend->total_docs,dt,r);

		i_free(backend->old_guid); backend->old_guid = NULL;
		i_free(backend->old_boxname); backend->old_boxname = NULL;
	}

	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_oldbox - done");
}

static void fts_backend_xapian_close_db(Xapian::WritableDatabase * dbw,char * dbpath,char * boxname, long verbose, bool sysl)
{
        long t;

	if(verbose>0)
	{
		t = fts_backend_xapian_current_time();
		if(sysl) syslog(LOG_INFO,"FTS Xapian : Closing DB (%s,%s)",boxname,dbpath);
		else i_info("FTS Xapian : Closing DB (%s,%s)",boxname,dbpath);
	}
        try
        {
		dbw->close();
                delete(dbw);
	}
        catch(Xapian::Error e)
        {
                if(sysl) syslog(LOG_ERR, "FTS Xapian: Can't close Xapian DB (%s) %s : %s - %s %s",boxname,dbpath,e.get_type(),e.get_msg().c_str(),e.get_error_string());
		else i_error("FTS Xapian: Can't close Xapian DB (%s) %s : %s - %s %s",boxname,dbpath,e.get_type(),e.get_msg().c_str(),e.get_error_string());
        }
	catch(std::exception e)
        {
                if(sysl) syslog(LOG_ERR, "FTS Xapian : CLosing db (%s) error %s",dbpath,e.what());
		else i_error("FTS Xapian : CLosing db (%s) error %s",dbpath,e.what());
        }

	if(verbose>0) 
	{
		t = fts_backend_xapian_current_time()-t;
		if(sysl) syslog(LOG_INFO,"FTS Xapian : DB (%s,%s) closed in %ld ms",boxname,dbpath,t);
		else i_info("FTS Xapian : DB (%s,%s) closed in %ld ms",boxname,dbpath,t);
	}
	free(dbpath);
	free(boxname);
}

static void fts_backend_xapian_close(struct xapian_fts_backend *backend, const char * reason)
{
	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian : Closing all DWs (%s)",reason);
	
	long i=0;
	fts_backend_xapian_get_lock(backend,fts_xapian_settings.verbose,reason);
	long n=backend->docs.size();
	while(i<n)
	{
		if(backend->docs.at(i)->status<1) backend->docs.at(i)->status=1;
		i++;
	}		
        fts_backend_xapian_release_lock(backend,fts_xapian_settings.verbose,reason);

	while(backend->docs.size()>0)
	{
                if(fts_xapian_settings.verbose>0) i_info("FTS Xapian: Waiting for all pending documents (%ld) to be processed (Sleep5) with %ld threads",backend->docs.size(),backend->threads.size());
                std::this_thread::sleep_for(XSLEEP);
        }
	while((i=backend->threads.size())>0)
	{
		i--;
		backend->threads.at(i)->toclose=true;
	
		if(!(backend->threads.at(i)->started))
		{
			delete(backend->threads.at(i));
			if(fts_xapian_settings.verbose>0) i_info("FTS Xapian : Closing #%ld because not started",i);
			backend->threads.at(i)=NULL;
			backend->threads.pop_back();
		}
		else if(backend->threads.at(i)->terminated)
		{
			if(fts_xapian_settings.verbose>0) i_info("FTS Xapian : Closing #%ld because terminated : %s",i,(backend->threads)[i]->getSummary().c_str());
			backend->threads.at(i)->close();
			delete(backend->threads.at(i));
			backend->threads.at(i)=NULL;
			backend->threads.pop_back();
		}
		else
		{
			if(fts_xapian_settings.verbose>0) i_info("FTS Xapian : Waiting for #%ld (Sleep4) : %s",i,(backend->threads)[i]->getSummary().c_str());
			std::this_thread::sleep_for(XSLEEP);
			i++;
		}
	}
	if(fts_xapian_settings.verbose>0) i_info("FTS Xapian : All DWs (%s) closed",reason);

	if(backend->dbw!=NULL)
	{
		char * dbpath = (char*) malloc(sizeof(char)*(strlen(backend->db)+1));	
		strcpy(dbpath,backend->db);
		/* struct stat fileinfo;
		stat(dbpath,&fileinfo); */
		char * boxname = (char*) malloc(sizeof(char)*(strlen(backend->boxname)+1));
		strcpy(boxname,backend->boxname);
		try
        	{
			if(fts_xapian_settings.detach)
			{
				(new std::thread(fts_backend_xapian_close_db,backend->dbw,dbpath,boxname,fts_xapian_settings.verbose,true))->detach();
			}
			else
			{
				fts_backend_xapian_close_db(backend->dbw,dbpath,boxname,fts_xapian_settings.verbose,false);
			}
        	}
        	catch(std::exception e)
        	{
        	        i_error("FTS Xapian : Closing process error %s",e.what());
        	}
		backend->dbw=NULL;
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
		i_error("FTS Xapian: xapian_query %s - %s %s",e.get_type(),e.get_msg().c_str(),e.get_error_string());
	}
	delete(q);
	return set;
}

static int fts_backend_xapian_unset_box(struct xapian_fts_backend *backend)
{
	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: Unset box '%s' (%s)",backend->boxname,backend->guid);

	long commit_time = fts_backend_xapian_current_time();

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

	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: Index path = %s",backend->path);

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
		if(fts_xapian_settings.verbose>0) i_warning("FTS Xapian: Box is empty");
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
        backend->threads.clear();
	backend->total_docs =0;

	char * t = i_strdup_printf("%s/termlist.glass",backend->db);
	struct stat sb;
	if(!( (stat(t, &sb)==0) && S_ISREG(sb.st_mode)))
	{
		i_info("FTS Xapian: '%s' (%s) indexes do not exist. Initializing DB",backend->boxname,backend->db);
		try
		{
			Xapian::WritableDatabase * db = new Xapian::WritableDatabase(backend->db,Xapian::DB_CREATE_OR_OVERWRITE | Xapian::DB_BACKEND_GLASS);
			db->close();
			delete(db);
		}
		catch(Xapian::Error e)
		{
			i_error("FTS Xapian: Can't create Xapian DB (%s) %s : %s - %s %s",backend->boxname,backend->db,e.get_type(),e.get_msg().c_str(),e.get_error_string());
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
			std::string f2; f2.clear();
			while(i<j)
			{
				if((hdr[i]>' ') && (hdr[i]!='"') && (hdr[i]!='\'') && (hdr[i]!='-'))
				{
					f2+=tolower(hdr[i]);
				}
				i++;
			}
			icu::StringPiece sp(a->value.str);
			icu::UnicodeString t = icu::UnicodeString::fromUTF8(sp);
			fts_backend_xapian_clean_accents(&t);
	
			char * h = i_strdup(f2.c_str());
			qs->add(h,&t,a->match_not,true,true);
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
	while((i<HDRS_NB-1) && (strcmp(field,hdrs_emails[i])!=0))
	{
		i++;
	}
	if(i>=HDRS_NB) i=HDRS_NB-1;
	const char * h = hdrs_xapian[i];

	fts_backend_xapian_get_lock(backend,fts_xapian_settings.verbose,"fts_backend_xapian_index");
	backend->docs.back()->add(h,data,fts_xapian_settings.verbose,"fts_backend_xapian_index");
	fts_backend_xapian_release_lock(backend,fts_xapian_settings.verbose,"fts_backend_xapian_index");

	if(fts_xapian_settings.verbose>1) i_info("FTS Xapian: fts_backend_xapian_index %s done",field);

	return TRUE;
}

