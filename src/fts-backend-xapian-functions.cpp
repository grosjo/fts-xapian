/* Copyright (c) 2019 Joan Moreau <jom@grosjo.net>, see the included COPYING file */

class XResultSet
{
    public:
	int size;
    	Xapian::docid * data;

    XResultSet() { size=0; data=NULL; }
    ~XResultSet() { if (size>0) { free(data); } }

    void add(Xapian::docid did)
    {
        if(data==NULL)
        {
            data=(Xapian::docid *)malloc(sizeof(Xapian::docid));
        }
        else
        {
            data=(Xapian::docid *)realloc(data,(size+1)*sizeof(Xapian::docid));
        }
        data[size]=did;
        size++;
    }
};
    
class XQuerySet
{
    	public:
		int size,tsize,hsize;
    		char ** data;
		char ** header;
		char ** terms;
		char ** hdrs;
    		bool is_and, is_global;
		int limit;

    	XQuerySet(bool op,int l=0) 
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
	}
    	
	~XQuerySet() 
	{ 
		int j;
		for(j=0;j<size;j++) 
		{ 
			free(data[j]); 
			free(header[j]);
		}
		if(size>0)
		{
			free(data); 
			free(header);
		}
		for(j=0;j<tsize;j++)
		{
			free(terms[j]);
		}
		if(tsize>0) free(terms);
		for(j=0;j<hsize;j++)
                {
                        free(hdrs[j]);
                }
                if(hsize>0) free(hdrs);
	}

	void set_global()
	{
		i_info("global ON");
		is_global=true;
	}

	bool is_ok(char s)
	{
		char p=tolower(s);
		if((p>='0')&&(p<='9')) return true;
		if((p>='a')&&(p<='z')) return true;
		if(p=='@') return true;
		if(p=='_') return true;
		if((p>='#')&&(p<='/')) return true;
		return false;
	}
 
	char * clean(const char *s)
  	{
		char * s2;
		if(s==NULL)
		{
			s2=(char *)malloc(sizeof(char));
			s2[0]=0;
			return s2;
		}	
		
		int i=0,j=0;
        	s2 = (char *)malloc(sizeof(char)*(strlen(s)+1));
		while((s[i]>0) && (!is_ok(s[i])))
		{
			i++;
		}
		
          	while(s[i]>0)
          	{
			if(is_ok(s[i]) || (s[i]==' '))
			{
				s2[j]=tolower(s[i]);
				j++;
			}
			i++;
		}
		
		while((j>0)&&(!is_ok(s2[j-1])))
		{
			j--;
		}
		s2[j]=0;

          	return s2;
  	}

	void add_hdr(const char *s)
	{
                int i=0,pos;
                while((i<hsize)&&(strcmp(hdrs[i],s)<0))
                {
                        i++;
                }
                if((i<hsize) && (strcmp(hdrs[i],s)==0))
                {
                        return;
                }
                pos=i;
                hsize++;
                if(hdrs == NULL)
                {
                        hdrs =(char **)malloc(sizeof(char*));
                }
                else
                {
                        hdrs =(char**)realloc(hdrs,sizeof(char*)*hsize);
                }
                char * s2 = (char *)malloc(sizeof(char)*(strlen(s)+1));
                strcpy(s2,s);
                for(i=hsize-1;i>pos;i--)
                {
                        hdrs[i]=hdrs[i-1];
                }
                hdrs[pos]=s2;
        }
	
	void add_term(const char *s)
	{
		int i=0,pos;

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
			terms =(char **)malloc(sizeof(char*));
		}
		else
		{
			terms =(char**)realloc(terms,sizeof(char*)*tsize);
		}
		char * s2 = (char *)malloc(sizeof(char)*(strlen(s)+1));
		strcpy(s2,s);
		for(i=tsize-1;i>pos;i--)
		{
			terms[i]=terms[i-1];
		}
		terms[pos]=s2;
	}

    	void add(const char * type,const char * s)
    	{
		char *t2=clean(type);
		char *s2=clean(s);
        	if((strlen(s2)<limit) || (strcmp(s2,"and")==0) || (strcmp(s2,"or")==0) || (strlen(t2)<1))
		{ 
			free(s2); 
			free(t2);
			return; 
		}

        	char * blank=strstr(s2," ");
        	if(blank!=NULL)
        	{
            		blank[0]=0;
            		add(t2,blank+1);
        	}

        	if(size==0)
        	{
            		data=(char **)malloc(sizeof(char*));
			data[0]=s2;
			header=(char **)malloc(sizeof(char*));
			header[0]=t2;
			size=1;
        	}
        	else
        	{
			int i=0,pos;
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
					free(s2); free(t2); return;
				}
				pos=i;
			}
			data=(char **)realloc(data,(size+1)*sizeof(char*));
			header=(char **)realloc(header,(size+1)*sizeof(char*));
			
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

    	char * get_query()
    	{
		long n=0;
                int i,j;
		char *s;

		if(size<1) 
		{ 
			s = (char *)malloc(sizeof(char)); 
			s[0]=0; 
			return s; 
		}

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
			s = (char *)malloc(sizeof(char)*(n+1));
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
			return s;
		}
	
		for(i=0;i<size;i++)
                {
                       	n=n+strlen(data[i])+strlen(header[i])+15;
                }
		s = (char *)malloc(sizeof(char)*(n+1));
		char pref[100]; strcpy(pref,header[0]);
		strcpy(s,"( "); n=2;
		for(i=0;i<size;i++)
		{
			if(i>0)
			{
				if(is_and || (strcmp(header[i],pref)==0))
				{
					strcpy(s+n," AND ");
                                        n+=5;
                                }
				else
				{
					strcpy(s+n," ) OR ( ");
					n+=8;
					strcpy(pref,header[i]);
				}
			}
			sprintf(s+n,"%s:%s",header[i],data[i]);
			n+=strlen(data[i])+strlen(header[i])+1;
		}
		strcpy(s+n," )");
		n+=2;
		s[n]=0;
        	return s;
    	}
};

class XHeaderTerm
{
	public:
		int size,partial,full,maxlength;
		char ** data;
  
        XHeaderTerm(int p, int f) 
	{ 
		partial=p; full=f; 
		size=0; 
		maxlength=0;
		data=NULL; 
	}
        ~XHeaderTerm() { if (size>0) { for(int i=0;i<size;i++) { free(data[i]); } free(data); } }

	char * strip(const char *s)
	{
		int i=0;
		char * s2 = (char *)malloc(sizeof(char)*(strlen(s)+1));
		while(i<strlen(s))
		{
			if((s[i]=='"') || (s[i]=='<') || (s[i]=='>') || (s[i]=='\''))
			{
				s2[i]=' ';
			}
			else
			{
				s2[i]=s[i];
			}
			i++;
		}
		s2[i]=0;
		return s2;
	}

	char * clean(const char *s)
        {
        	int i=0,j=0;
                while((s[i]<=' ') && (s[i]>0))
                {
                	i++;
                }
		const char * s2=s+i;
		j=strlen(s2);
		while((j>0)&&(s2[j-1]<=' '))
		{
			j--;
		}
		char * s3 = (char *)malloc(sizeof(char)*(j+1));
                strncpy(s3, s+i,j);
		s3[j]=0;
  
                for(i=0;i<j;i++)
                {
                        s3[i]=tolower(s3[i]);
                }
                return s3;
        }

        bool is_ok_stem(const char s)
        {
                if((s>='0')&&(s<='9')) return true;
                if((s>='a')&&(s<='z')) return true;
                if(s=='@') return true;
                if(s=='_') return true;
                if((s>='#')&&(s<='/')) return true;
                return false;
        }

	char * clean_stem(const char *s)
	{
                int i=0,j;
                while(((s[i]<'a') || (s[i]>'z')) && (s[i]>0))
                {
                        i++;
                }
		const char * s2=s+i;
		j=strlen(s2);
		while((j>0) && (!is_ok_stem(s2[j-1])))
		{
			j--;
		}
		char * s3 = (char *)malloc(sizeof(char)*(j+1));
		strncpy(s3, s+i,j);
                s3[j]=0;
                return s3;
        }

	void add(const char * s)
        {
        	if(s==NULL) return;
 
                char * s2=strip(s);
		char * s3=clean(s2);
		free(s2);
		
                if(strlen(s3)<1) { free(s3); return; }
 
                char * blank=strstr(s3," ");
                if(blank!=NULL)
                {
                	blank[0]=0;
                        add(blank+1);
                }

		int i,j,k=strlen(s3);

		char *stem = (char *)malloc(sizeof(char)*(full+1)); 
		for(i=0;i<=k-partial;i++)
		{
			for(j=partial;(j+i<=k)&&(j<=full);j++)
			{
				strncpy(stem,s3+i,j);
				stem[j]=0;
				add_stem(stem);
			}
		}
		if(strlen(s3)<XAPIAN_TERM_SIZELIMIT) add_stem(s3);
		free(s3);
	}
	
	void add_stem(const char *s)
	{
		char * s2 = clean_stem(s);
		int l = strlen(s2);

		if(l<partial) return;

                if(data==NULL)
                {
                	data=(char **)malloc(sizeof(char*));
                }
                else
                {
			bool existing=false;
			int i=0;
			while((!existing) && (i<size))
                	{
				if(strcmp(data[i],s2)==0)
				{
					existing=true;
				}
				i++;
			}
			if(existing) return;
                	data=(char **)realloc(data,(size+1)*sizeof(char*));
                }
		if(l>maxlength) { maxlength=l; }
                data[size]=s2;
                size++;
	}
};

static int fts_backend_xapian_unset_box(struct xapian_fts_backend *backend)
{
	if(backend->box!=NULL)
	{
//		i_info("FTS Xapian: Committing/Refresh changes %s",backend->box->name);
	}
	backend->box = NULL;
	backend->db[0] = 0;
	backend->last_uid_indexed=0;
	if(backend->dbw !=NULL)
	{
		backend->dbw->commit();
		backend->dbw->close();
		free(backend->dbw);
		backend->dbw=NULL;
	}
	if(backend->dbr !=NULL)
        {
                free(backend->dbr);
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
	sprintf(backend->db,"%s/db_%s",backend->path,mb);

	backend->box = box;
	backend->nb_updates=0;
	return 0;
}

static bool fts_backend_xapian_check_read(struct xapian_fts_backend *backend)
{
	if(strlen(backend->db)<1) return false;

        if(backend->dbr != NULL) return true;

	struct stat sb;

        try
        {
		if(!(stat(backend->db, &sb) == 0 && S_ISDIR(sb.st_mode)))
		{
			Xapian::WritableDatabase db(backend->db,Xapian::DB_CREATE_OR_OPEN);
                        db.close();
		}
                backend->dbr = new Xapian::Database(backend->db); 
	}
        catch(Xapian::Error e)
        {
                i_error("FTS Xapian: Can not open RO index for %s (%s)",backend->box->name,backend->db);
		i_error("XapianError:%s",e.get_msg().c_str());
                return false;
        }
        return true;
}

static bool fts_backend_xapian_check_write(struct xapian_fts_backend *backend)
{
	if(strlen(backend->db)<1) 
	{
		i_warning("FTS Xapian: check_write : no DB name");
		return false;
	}

	if(backend->dbw != NULL) return true;
	try
	{
		i_info("Opening RW %s",backend->db);
		backend->dbw = new Xapian::WritableDatabase(backend->db,Xapian::DB_CREATE_OR_OPEN | Xapian::DB_RETRY_LOCK );
	}
	catch(Xapian::Error e)
        {
		i_error("FTS Xapian: Can not open RW index for %s (%s)",backend->box->name,backend->db);
                i_error("XapianError:%s",e.get_msg().c_str());
                return false;
        }
	return true;	
}


XResultSet * fts_backend_xapian_query(Xapian::Database * dbx, XQuerySet * query, long limit=0)
{
    	XResultSet * set= new XResultSet();
   
    	try
    	{
        	Xapian::QueryParser * qp = new Xapian::QueryParser;
        	qp->add_prefix("uid", "Q");
        	qp->add_prefix("subject", "S");
        	qp->add_prefix("from", "A");
        	qp->add_prefix("to", "XTO");
        	qp->add_prefix("cc", "XCC");
        	qp->add_prefix("bcc", "XBCC");
		qp->add_prefix("body", "XBDY");
		qp->add_prefix("message-id", "XMID");
        	qp->add_prefix("", "XBDY");
        	qp->set_database(*dbx);
    
        	char *query_string=query->get_query();
		Xapian::Enquire enquire(*dbx);
        	Xapian::Query q = qp->parse_query(query_string);
		enquire.set_query(q);
        	free(query_string);

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
		i_error("XapianError:%s",e.get_msg().c_str());
    	}
    	return set;
}

bool fts_backend_xapian_index_hdr(Xapian::WritableDatabase * dbx, uint uid, char* field, const char* data,int p, int f)
{
	try
	{
		XQuerySet xq(false);
        	char u[20]; sprintf(u,"%d",uid);
        	xq.add("uid",u);
        	XResultSet *result=fts_backend_xapian_query(dbx,&xq,1);

		Xapian::docid docid;
		Xapian::Document doc;
		if(result->size<1)
        	{
			doc.add_value(1,u);
			sprintf(u,"Q%d",uid);
			doc.add_term(u);
			docid=dbx->add_document(doc);
        	}
		else
		{
			docid=result->data[0];
			doc = dbx->get_document(docid);
		}
		free(result);
	
		const char *h;
		if(strlen(field)<1) { return true; }
		else if(strcmp(field,"uid")==0) { h="Q"; }
		else if(strcmp(field,"from")==0) { h="A"; }
		else if(strcmp(field,"to")==0) { h="XTO"; }
		else if(strcmp(field,"cc")==0) { h="XCC"; }
		else if(strcmp(field,"bcc")==0) { h="XBCC"; }
		else if(strcmp(field,"message-id")==0) { h="XMID"; }
		else 
		{
			return true;
		}

		XHeaderTerm xhs(p,f);
                xhs.add(data);

		i_warning("Index UID=%d '%s' : %d terms, maxlength: %d",uid,field,xhs.size,xhs.maxlength);
		
		char *t = (char*)malloc(sizeof(char)*(xhs.maxlength+6));
	
		for(int i=0;i<xhs.size;i++)
		{
			sprintf(t,"%s%s",h,xhs.data[i]);		
			try
			{
				doc.add_term(t);
			}
			catch(Xapian::Error e)
			{
				i_error(e.get_msg().c_str());
			}
		}

		dbx->replace_document(docid,doc);
	    	return true;
	}
	catch(Xapian::Error e)
	{
		i_error("fts_backend_xapian_index_hdr (%s) -> %s",field,data);
		i_error("XapianError:%s",e.get_msg().c_str());
	}
	return false;
}

bool fts_backend_xapian_index_text(Xapian::WritableDatabase * dbx,uint uid, char * field, const char * data)
{
	try
        {
        	XQuerySet xq(false);
                char u[20]; sprintf(u,"%d",uid);
                xq.add("uid",u);
                XResultSet * result=fts_backend_xapian_query(dbx,&xq,1);
  
                Xapian::docid docid;
                Xapian::Document doc;
                if(result->size<1)
                {
                	doc.add_value(1,u);
                        sprintf(u,"Q%d",uid);
                        doc.add_term(u);
                        docid=dbx->add_document(doc);
                }
                else
                {
			docid=result->data[0];
                        doc = dbx->get_document(docid);
                }
		free(result);

		Xapian::TermGenerator termgenerator;
		termgenerator.set_stemmer(Xapian::Stem("en"));
		termgenerator.set_document(doc);
	
		std::string h;
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
                return true;
          }
          catch(Xapian::Error e)
          {
		i_error("fts_backend_xapian_index_text");
		i_error("XapianError:%s",e.get_msg().c_str());
          }
          return false;
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
                        i_error("XapianError:%s",e.get_msg().c_str());
                }
        }
        return 0;
}
