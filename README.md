FTS Xapian plugin for Dovecot
=============================

What is this?
-------------

This project intends to provide a straightforward, simple and maintenance free, way to configure FTS plugin for [Dovecot](https://github.com/dovecot/), leveraging the efforts by the [Xapian.org](https://xapian.org/) team.

This effort came after Dovecot team decided to deprecate "fts_squat" included in the dovecot core, and due to the complexity of the Solr plugin capabilitles, un-needed for most users.



Prerequisites
-------------

You are going to need the following things to get this going:

```
* Dovecot 2.2.x (or above)
* Xapian 1.4.x (or above)
* ICU 60.x (or above)
```

You will need to configure properly [Users Home Directories](https://wiki.dovecot.org/VirtualUsers/Home) in dovecot configuration


Installing the Dovecot plugin
-----------------------------

First install the following packages, or equivalent for your operating system.

```
Ubuntu:
apt-get build-dep dovecot-core
apt-get install dovecot-dev
apt-get install git xapian-core libicu-dev

Archlinux:
pacman -S dovecot
pacman -S xapian-core icu

FreeBSD:
pkg install xapian-core
pkg install xapian-bindings
pkg install icu
```

Clone this project:

```
git clone https://github.com/grosjo/fts-xapian
cd fts-xapian
```

Compile and install the plugin.

```
autoreconf -vi
./configure --with-dovecot=/path/to/dovecot
make
sudo make install
```

Replace /path/to/dovecot by the actual path to 'dovecot-config'.
Type 'locate dovecot-config' in a shell to figure this out. On ArchLinux , it is /usr/lib/dovecot.

For specific configuration, you may have to 'export PKG_CONFIG_PATH=...'. To check that, type 'pkg-config --cflags-only-I icu-uc icu-io icu-i18n', it shall return no error.

The module will be placed into the module directory of your dovecot configuration

Update your dovecot.conf file with something similar to:

```
mail_plugins = fts fts_xapian (...)

(...)

plugin {
	plugin = fts fts_xapian (...)

	fts = xapian
	fts_xapian = partial=3 full=20 verbose=0

	fts_autoindex = yes
	fts_enforced = yes

	fts_autoindex_exclude = \Trash

	fts_decoder = decode2text // To index attachements
(...)
}

(...)
service indexer-worker {
	vsz_limit = 2G // or above (or 0 if you have rather large memory usable on your server, which is preferred for performance)
}

service decode2text {
   executable = script /usr/libexec/dovecot/decode2text.sh
   user = dovecot
   unix_listener decode2text {
     mode = 0666
   }
}
(...)

```

Indexing options
----------------

| Option         | Description                    | Possible values                      | Default value |
|----------------|--------------------------------|--------------------------------------|---------------|
| partial & full | NGram values for header fields | between 3 and 20 characters          | 3 & 20        |
| verbose        | Logs verbosity                 | 0 (silent), 1 (verbose) or 2 (debug) | 0             |
| attachments    | Index attachments or not       | 0 or 1                               | 0             |

NGrams details
--------------

Yhe partial & full parameters are the NGram values for header fields, which means the keywords created for fields (To,
Cc, ...) are between 3 and 20 chars long. Full words are also added by default (if not longer than 245 chars, which is
the limit of Xapian capability).

Example: "<john@doe>" will create joh, ohn, hn@, ..., john@d, ohn@do, ..., and finally john@doe as searchable keywords.

Index updating
--------------

Just restart Dovecot:

```sh
sudo servicectl restart dovecot
```


If this is not a fresh install of dovecot, you need to re-index your mailboxes:

```sh
doveadm index -A -q \*
```

*The first index will re-index all emails, therefore may take a while.*



You shall put in a cron the following command (for daily run for instance) :

```sh
doveadm fts optimize -A
```


Debugging/Support
-----------------

Please submit requests/bugs via the [GitHub issue tracker](https://github.com/grosjo/fts-xapian/issues).

Thanks to Aki Tuomi <aki.tuomi@open-xchange.com>, Stephan Bosch <stephan@rename-it.nl>, Paul Hecker <paul@iwascoding.com>
