FTS Xapian plugin for Dovecot
=============================

What is this?
-------------

This project intends to provide a straightforward and simple way to configure FTS plugin for [Dovecot](https://github.com/dovecot/), leveraging the efforts by the [Xapian.org](https://xapian.org/) team.

This effort came after Dovecot team decided to deprecate "fts_squat" included in the dovecot core, and due to the complexity of the Solr plugin capabilitles, un-needed for most users.



Prerequisites
-------------

You are going to need the following things to get this going:

```
* Dovecot above 2.3.x (untested with previous versions)
* Xapian 1.4.x (or above)
* ICU 63 or above
```

You will need to configure properly [Users Home Directories](https://wiki.dovecot.org/VirtualUsers/Home) in dovecot configuration


Installing the Dovecot plugin
-----------------------------

First install the following packages, or equivalent for your operating system. 

```
Ubuntu:
apt-get build-dep dovecot-core
apt-get install git dovecot-dev
apt-get install xapian-core
apt-get install libicu-dev

Archlinux:
pacman -S dovecot
pacman -S xapian-core
pacman -S icu
```

Clone this project:

```
git clone https://github.com/grosjo/fts-xapian
cd fts-xapian
```

Compile and install the plugin. 

```
autoreconf -vi
PANDOC=false ./configure --prefix=/usr --with-dovecot=/path/to/dovecot
make
sudo make install

Replace /path/to/dovecot by the actual path to 'dovecot-config'.
Type 'locate dovecot-config' in a shell to figure this out. On ArchLinux , it is /usr/lib/dovecot. 
```

Update your dovecot.conf file with something similar to:
```

default_vsz_limit = 2GB // or above

mail_plugins = fts fts_xapian (...)

(...)

plugin {
	plugin = fts fts_xapian (...)

	fts = xapian
	fts_xapian = partial=2 full=20

	fts_autoindex = yes
	fts_enforced = yes
(...)
}
```
note: 2 and 20 are the NGram values for header fields, which means the keywords created for fields (To, Cc, ...) are between is 2 and 20 chars long. Full words are also added by default.

Example: "<john@doe>" will create jo, oh, ... , @d, do, .. joh, ohn, hn@, ..., john@d, ohn@do, ..., and finally john@doe as searchable keywords.


Restart Dovecot:

```
sudo servicectl restart dovecot
```


If this is not a fresh install of dovecot, you need to re-index your mailboxes

```
doveadm index -A -q \*
```

*The first index will re-index all emails, therefore may take a while.*



Debugging/Support
-----------------

Please submit requests/bugs via the [GitHub issue tracker](https://github.com/grosjo/fts-xapian/issues).

Thanks to Aki Tuomi <aki.tuomi@open-xchange.com>, Stephan Bosh <stephan@rename-it.nl>, Paul Hecker <paul@iwascoding.com>
