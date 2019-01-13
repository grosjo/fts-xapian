FTS Xapian plugin for Dovecot
=============================

What is this?
-------------

This project intends to provide a straightforward and simple to configure FTS plugin for [Dovecot](https://github.com/dovecot/).

The effort came after Dovecot team decided to deprecate "fts_squat" included in the dovecot code, and the complexity of the Solr plugin un-needed for msot of users.


Installation
============

Prerequisites
-------------

You are going to need the following things to get this going:

* Dovecot above 2.3.x (untested with previous versions)
* Properly configured [Users Home Directories](https://wiki.dovecot.org/VirtualUsers/Home) in dovecot configuration



Installing the Dovecot plugins
------------------------------

First install the following Ubuntu packages, or equivalent for your operating system. 

```
sudo apt-get build-dep dovecot-core
sudo apt-get install git dovecot-dev
sudo apt-get install xapian-core
```

Clone this project:

```
git clone https://github.com/grosjo/fts-xapian
cd fts-xapian
```

Compile and install the plugins. 

```
make
sudo make install
```

Update your dovecot.conf file with something similar to:
```
plugin {
	plugin = fts fts_xapian (...)

	fts = xapian
	fts_xapian = partial=2 full=20

	fts_autoindex = yes
	fts_enforced = no
	fts_autoindex_exclude = \Junk
  	fts_autoindex_exclude2 = \Trash
	fts_autoindex_exclude3 = \Drafts
	fts_autoindex_exclude4 = \Spam
(...)
}
```
note: 2 and 20 are the NGram value for headers, this means the keywords created for fields (To, Cc, ...) are between is 2 and 20 chars long. Full words are also added by default.

Example: "<john@doe>" will create jo, oh, ... , @d, do, .. joh, ohn, hn@, ..., john@d, ohn@do, ..., and finally john@doe as searchable keywords.


Restart Dovecot:

```
sudo service dovecot restart
```

Debugging/Support
=================

Please feel free to send your questions, together with the dovecot log file, to jom@grosjo.net

