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
		long size,tsize,hsize,limit;
    		char ** data;
		char ** header;
		char ** terms;
		char ** hdrs;
    		bool is_and, is_global, display;

    	XQuerySet(bool op,long l=0, bool d=false) 
	{ 
		is_and=op;
		size=0; 
		tsize=0;
		hsize=0;
		is_global=false;
		limit=l;
		if(limit<1) { limit=1; }
		data=NULL; 
		header=NULL;
		terms=NULL;
		hdrs=NULL;
		display=d;
	}
    	
	~XQuerySet() 
	{ 
		long j;
		for(j=0;j<size;j++) 
		{ 
			i_free(data[j]); 
			i_free(header[j]);
		}
		if(size>0)
		{
			i_free(data); 
			i_free(header);
		}
		for(j=0;j<tsize;j++)
		{
			i_free(terms[j]);
		}
		if(tsize>0) i_free(terms);
		for(j=0;j<hsize;j++)
                {
                        i_free(hdrs[j]);
                }
                if(hsize>0) i_free(hdrs);
	}

	void set_global()
	{
		i_info("global ON");
		is_global=true;
	}

	void add_hdr(icu::UnicodeString *d)
        {
                std::string s;
                d->toUTF8String(s);
		add_hdr(s.c_str());
	}

	void add_hdr(const char * s)
	{
                long l = strlen(s);
		if(l<1) return;

                long i=0,pos;
                while((i<hsize)&&(strcmp(hdrs[i],s)<0))
                {
                        i++;
                }
                if((i<hsize) && (strcmp(hdrs[i],s)==0))
                {
                        return;
                }
                
		pos=i;
                
		if(hdrs == NULL)
                {
                        hdrs =(char **)i_malloc(sizeof(char*));
                }
                else
                {
                        hdrs =(char**)i_realloc(hdrs,sizeof(char*)*hsize,sizeof(char*)*(hsize+1));
                }
		
		hsize++;
                for(i=hsize-1;i>pos;i--)
                {
                        hdrs[i]=hdrs[i-1];
                }
		
                hdrs[pos] = i_strdup(s);
        }
	
	void add_term(const char * s)
        {
                long l = strlen(s);
                if(l<1) return;

		long i=0,pos;

                while((i<tsize)&&(strcmp(terms[i],s)<0))
                {
                	i++;
                }
		if((i<tsize) && (strcmp(terms[i],s)==0))
		{
			return;
		}

		pos=i;

		tsize++;
		if(terms == NULL) 
		{
			terms =(char **)i_malloc(sizeof(char*));
		}
		else
		{
			terms =(char**)i_realloc(terms,sizeof(char*)*tsize,sizeof(char*)*(tsize+1));
		}
		for(i=tsize-1;i>pos;i--)
		{
			terms[i]=terms[i-1];
		}
		
		terms[pos] = i_strdup(s);
	}

	void add(const char * type,const char * s)
        {
                if(s==NULL) return;
		if(type==NULL) return;

                icu::StringPiece sp(type);
                icu::UnicodeString t = icu::UnicodeString::fromUTF8(sp);

		icu::StringPiece sp2(s);
                icu::UnicodeString s2 = icu::UnicodeString::fromUTF8(sp2);
	
		s2->findAndReplace("'"," ");
                s2->findAndReplace(":"," ");
                s2->findAndReplace(";"," ");
                s2->findAndReplace("\""," ");
                s2->findAndReplace("<"," ");
                s2->findAndReplace(">"," ");

		s2->toLower();
		t2->toLower();
	
                add(&t,&s2);
        }

        void add(icu::UnicodeString *t, icu::UnicodeString *s)
        {
                s->trim();
		t->trim();

		if((t->length()<1) || (t->length()<1)) return;

                long i = s->indexOf(" ");
                if(i>0)
                {
                        icu::UnicodeString * r = new icu::UnicodeString(*s,i+1);
                        add(t,r);
                        delete(r);
                        s->truncate(i);
		}

		std::string tmp1;
                t->toUTF8String(tmp1);
		char * t2 = i_strdup(tmp1.c_str());
		std::string tmp2;
		s->toUTF8String(tmp2);
		char * s2 = i_strdup(tmp2.c_str());

        	if(size==0)
        	{
            		data=(char **)i_malloc(sizeof(char*));
			data[0]=s2;
			header=(char **)i_malloc(sizeof(char*));
			header[0]=t2;
			size=1;
        	}
        	else
        	{
			long i=0,pos;
			while((i<size)&&(strcmp(header[i],t2)<0))
			{
				i++;
			}
			if(((i<size)&&(strcmp(header[i],t2)>0)) || (i==size))
			{
				pos=i;
			}
			else
			{
				while((i<size)&&(strcmp(header[i],t2)==0)&&(strcmp(data[i],s2)<0))
				{
					i++;
				}
				if((i<size)&&(strcmp(header[i],t2)==0)&&(strcmp(data[i],s2)==0))
				{
					i_free(s2); 
					i_free(t2); 
					return;
				}
				pos=i;
			}
			data=(char **)i_realloc(data,size*sizeof(char*),(size+1)*sizeof(char*));
			header=(char **)i_realloc(header,size*sizeof(char*),(size+1)*sizeof(char*));
			
			for(i=size;i>pos;i--)
			{
				data[i]=data[i-1];
				header[i]=header[i-1];
			}
			data[pos]=s2;
			header[pos]=t2;
        		size++;
		}
		add_hdr(t2);
		add_term(s2);
    	}

    	Xapian::Query * get_query(Xapian::Database * db)
    	{
		if(size<1)
		{
			return new Xapian::Query(Xapian::Query::MatchAll);
		}
		
		long i,j,n=0;
		char *s;

		Xapian::QueryParser * qp = new Xapian::QueryParser();
		for(i=0; i< HDRS_NB; i++) qp->add_prefix(hdrs_emails[i], hdrs_xapian[i]);
                
		if(is_global)
		{
			for(j=0;j<hsize;j++)
			{
				for(i=0;i<tsize;i++)
                		{
                        		n+=strlen(hdrs[j])+strlen(terms[i])+5;
                		}
				n+=10;
			}
			s = (char *)i_malloc(sizeof(char)*(n+1));
			strcpy(s,"( ");
			n=2;
			for(i=0;i<tsize;i++)
                        {
				if(i>0)
				{
					strcpy(s+n," ) AND ( ");
					n+=9;
				}
				for(j=0;j<hsize;j++)
				{
					if(j>0)
					{
						strcpy(s+n," OR ");
						n+=4;
					}
					sprintf(s+n,"%s:%s",hdrs[j],terms[i]);
					n+=1+strlen(hdrs[j])+strlen(terms[i]);
				}
			}
			strcpy(s+n," )");
			n+=2;
			s[n]=0;
		}
		else
		{	
			for(i=0;i<size;i++)
                	{
                	       	n=n+strlen(data[i])+strlen(header[i])+15;
                	}
			s = (char *)i_malloc(sizeof(char)*(n+1));
			std::string prefix(header[0]);
			strcpy(s,"( "); n=2;
			for(i=0;i<size;i++)
			{
				if(i>0)
				{
					if(is_and || (prefix.compare(header[i])==0))
					{
						strcpy(s+n," AND ");
                	                        n+=5;
                	                }
					else
					{
						strcpy(s+n," ) OR ( ");
						n+=8;
						prefix=header[i];
					}
				}
				sprintf(s+n,"%s:%s",header[i],data[i]);
				n+=strlen(data[i])+strlen(header[i])+1;
			}
			strcpy(s+n," )");
			n+=2;
			s[n]=0;
        	}
	        qp->set_database(*db);

                Xapian::Query * q = new Xapian::Query(qp->parse_query(s));
		if(display) i_info("Query : %s",s); 
                i_free(s);
                delete(qp);
		return q;
    	}
};

class XHeaderTerm
{
	public:
		long size,partial,full,maxlength;
		char ** data;
		bool onlyone;
  
        XHeaderTerm(long p, long f, bool o) 
	{ 
		partial=p; full=f; 
		size=0; 
		maxlength=0;
		data=NULL; 
		onlyone=o;
	}

        ~XHeaderTerm() 
	{ 
		if (size>0) 
		{ 
			for(long i=0;i<size;i++) 
			{ 
				i_free(data[i]); 
			} 
			i_free(data); 
		} 
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
		d->findAndReplace(":"," ");
		d->findAndReplace(";"," ");
		d->findAndReplace("\""," ");
		d->findAndReplace("<"," ");
		d->findAndReplace(">"," ");
		d->trim();
	
		long l = d->length();

		if(l<partial) return;

                if(onlyone)
                {
                        add_stem(d);
                        return;
                }

		long i = d->indexOf(" ");

		if(i>0)
		{
			r = new icu::UnicodeString(*d,i+1);
			add(r);
			delete(r);
			d->truncate(i);
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
		std::string s;
		d->toUTF8String(s);
		
		long l = s.length();
		if(l>XAPIAN_TERM_SIZELIMIT) return;

		char * s2 = i_strdup(s.c_str());
 
                if(data==NULL)
                {
                	data=(char **)i_malloc(sizeof(char*));
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


static void fts_backend_xapian_oldbox(struct xapian_fts_backend *backend)
{
        if(backend->oldbox != NULL)
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
	
                i_info("Done indexing '%s' (%d msgs in %d ms, rate: %.1f)",backend->oldbox,backend->perf_nb,dt,r);
                i_free(backend->oldbox);
                backend->oldbox=NULL;
        }
}

static int fts_backend_xapian_unset_box(struct xapian_fts_backend *backend)
{
	fts_backend_xapian_oldbox(backend);

	backend->box = NULL;

	if(backend->db != NULL) 
	{
		i_free(backend->db);
		backend->db = NULL;
	}

	if(backend->dbw !=NULL)
	{
		backend->dbw->commit();
		backend->dbw->close();
		delete(backend->dbw);
		backend->dbw=NULL;
	}
	if(backend->dbr !=NULL)
        {
		backend->dbr->close();
                delete(backend->dbr);
		backend->dbr = NULL;
	}
	return 0;
}

static int fts_backend_xapian_set_box(struct xapian_fts_backend *backend, struct mailbox *box)
{

	fts_backend_xapian_unset_box(backend);

	if (box == NULL)
	{
                return 0;
	}

	if(box==backend->box)
	{
		i_info("FTS Xapian: Box is unchanged");
	}
	const char * mb;
	fts_mailbox_get_guid(box, &mb );

	long l=strlen(backend->path)+strlen(mb)+5; 
	backend->db = (char *)i_malloc(l*sizeof(char));
	snprintf(backend->db,l,"%s/db_%s",backend->path,mb);

	backend->box = box;
	backend->nb_updates=0;

	 /* Performance calculator*/
        struct timeval tp;
        gettimeofday(&tp, NULL);
        backend->perf_dt = tp.tv_sec * 1000 + tp.tv_usec / 1000;
	backend->perf_uid=0;
	backend->perf_nb=0;
	/* End Performance calculator*/

	return 0;
}

static bool fts_backend_xapian_check_read(struct xapian_fts_backend *backend)
{
	if((backend->db == NULL) || (strlen(backend->db)<1)) 
	{
		i_warning("FTS Xapian: check_read : no DB name");
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
			i_info("FTS Xapian: Tried to create an existing db '%s'",backend->db);
		}
	}
	try
	{
                backend->dbr = new Xapian::Database(backend->db); 
	}
        catch(Xapian::Error e)
        {
                i_error("Xapian: Can not open RO index (%s) %s",backend->box->name,backend->db);
		i_error("Xapian: %s",e.get_msg().c_str());
                return false;
        }
        return true;
}

static bool fts_backend_xapian_check_write(struct xapian_fts_backend *backend)
{
	if((backend->db == NULL) || (strlen(backend->db)<1)) 
	{
		i_warning("FTS Xapian: check_write : no DB name");
		return false;
	}

	if(backend->dbw != NULL) return true;

	try
	{
		backend->dbw = new Xapian::WritableDatabase(backend->db,Xapian::DB_CREATE_OR_OPEN | Xapian::DB_RETRY_LOCK);
	}
	catch(Xapian::Error e)
        {
		i_error("Xapian: Can't open RW index (%s) %s",backend->box->name,backend->db);
                i_error("Xapian: %s",e.get_msg().c_str());
                return false;
        }
	return true;	
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
		i_error("Xapian: %s",e.get_msg().c_str());
    	}
    	return set;
}

bool fts_backend_xapian_index_hdr(Xapian::WritableDatabase * dbx, uint uid, const char* field, const char* data,long p, long f)
{
	try
	{
		XQuerySet xq(false);
        	char u[30]; snprintf(u,30,"%d",uid);
        	xq.add("uid",u);

        	XResultSet *result=fts_backend_xapian_query(dbx,&xq,1);

		Xapian::docid docid;
		Xapian::Document doc;
		if(result->size<1)
        	{
			doc.add_value(1,Xapian::sortable_serialise(uid));
			snprintf(u,30,"Q%d",uid);
			doc.add_term(u);
			docid=dbx->add_document(doc);
        	}
		else
		{
			docid=result->data[0];
			doc = dbx->get_document(docid);
		}
		delete(result);
	
		if(strlen(field)<1) { return true; }
		long i=0;
		while((i<HDRS_NB) && (strcmp(field,hdrs_emails[i])!=0))
		{
			i++;
		}

		if(i>=HDRS_NB) return true;
		const char * h=hdrs_xapian[i];

		XHeaderTerm xhs(p,f,strcmp(h,"XMID")==0);
                xhs.add(data);

		char *t = (char*)i_malloc(sizeof(char)*(xhs.maxlength+6));
	
		for(i=0;i<xhs.size;i++)
		{
			snprintf(t,xhs.maxlength+6,"%s%s",h,xhs.data[i]);
			try
			{
				doc.add_term(t);
			}
			catch(Xapian::Error e)
			{
				i_error("Xapian: %s",e.get_msg().c_str());
			}
		}
		i_free(t);

		dbx->replace_document(docid,doc);
	    	return true;
	}
	catch(Xapian::Error e)
	{
		i_error("Xapian: fts_backend_xapian_index_hdr (%s) -> %s",field,data);
		i_error("Xapian: %s",e.get_msg().c_str());
	}
	return false;
}

bool fts_backend_xapian_index_text(Xapian::WritableDatabase * dbx,uint uid, const char * field, const char * data)
{
	try
        {
        	XQuerySet xq(false);
                char u[30]; snprintf(u,30,"%d",uid);
                xq.add("uid",u);
                XResultSet * result=fts_backend_xapian_query(dbx,&xq,1);
  
                Xapian::docid docid;
                Xapian::Document doc;
                if(result->size<1)
                {
			doc.add_value(1,Xapian::sortable_serialise(uid));
                        snprintf(u,30,"Q%d",uid);
                        doc.add_term(u);
                        docid=dbx->add_document(doc);
                }
                else
                {
			docid=result->data[0];
                        doc = dbx->get_document(docid);
                }
		delete(result);

		Xapian::TermGenerator termgenerator;
		termgenerator.set_stemmer(Xapian::Stem("en"));
		termgenerator.set_document(doc);
	
		const char * h;
                if(strcmp(field,"subject")==0) 
		{
			h="S";
		}
		else
		{
			h="XBDY";
		}
		std::string d(data);
		termgenerator.index_text(d, 1, h);
		
                dbx->replace_document(docid,doc);
          }
          catch(Xapian::Error e)
          {
		i_error("Xapian: fts_backend_xapian_index_text");
		i_error("Xapian: %s",e.get_msg().c_str());
		return false;
          }
          return true;
}

static int fts_backend_xapian_empty_db_remove(const char *fpath, const struct stat *sb, int typeflag)
{
	if(typeflag == FTW_F)
	{
		i_info("Removing file %s",fpath);
		remove(fpath);
	}
	return 0;
}

static int fts_backend_xapian_empty_db(const char *fpath, const struct stat *sb, int typeflag)
{       
        if(typeflag == FTW_D)
        {
		const char * sl = fpath;
		while(strstr(sl,"/")!=NULL)
		{
			sl=strstr(sl,"/")+1;
		}
		if(strncmp(sl,"db_",3)!=0) return 0;

                try
                {
			i_info("FTS Xapian: Emptying %s",fpath);
                        Xapian::WritableDatabase db(fpath,Xapian::DB_CREATE_OR_OPEN);
			db.close();
			ftw(fpath,fts_backend_xapian_empty_db_remove,100);
			i_info("Removing directory %s",fpath);
			rmdir(fpath);
                }
                catch(Xapian::Error e)
                {
                        i_error("Xapian: %s",e.get_msg().c_str());
                }
        }
        return 0;
}
