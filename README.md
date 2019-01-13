FTS Xapian plugin for Dovecot
=============================

What is this?
-------------

This project intends to provide a straightforward and simple to configure FTS plugin for [Dovecot](https://github.com/dovecot/), leveraging the efforts by the [Xapian.org](https://xapian.org/) team.

This effort came after Dovecot team decided to deprecate "fts_squat" included in the dovecot core, and due to the complexity of the Solr plugin capabilitles, un-needed for most users.


Installation
============

Prerequisites
-------------

You are going to need the following things to get this going:

* Dovecot above 2.3.x (untested with previous versions)
* Properly configured [Users Home Directories](https://wiki.dovecot.org/VirtualUsers/Home) in dovecot configuration



Installing the Dovecot plugins
------------------------------

First install the following packages, or equivalent for your operating system. 

```
Ubuntu:
apt-get build-dep dovecot-core
apt-get install git dovecot-dev
apt-get install xapian-core

Archlinux:
pacman -S dovecot
pacman -S xapian-core
```

Clone this project:

```
git clone https://github.com/grosjo/fts-xapian
cd fts-xapian
```

Compile and install the plugins. 

```
autoreconf -vi
./configure --prefix=/usr --with-dovecot=/path/to/dovecot
make
sudo make install
```

Update your dovecot.conf file with something similar to:
```
mail_plugins = fts fts_xapian (...)

(...)

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
note: 2 and 20 are the NGram values for header fields, which means the keywords created for fields (To, Cc, ...) are between is 2 and 20 chars long. Full words are also added by default.

Example: "<john@doe>" will create jo, oh, ... , @d, do, .. joh, ohn, hn@, ..., john@d, ohn@do, ..., and finally john@doe as searchable keywords.


Restart Dovecot:

```
sudo service dovecot restart
```

Debugging/Support
=================

Please feel free to send your questions, together with the dovecot log file, to jom@grosjo.net or to the dovecot ML dovecot@dovecot.org

Thanks to Aki Tuomi <aki.tuomi@open-xchange.com> and Stephan Bosh <stephan@rename-it.nl>
