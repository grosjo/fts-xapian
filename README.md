FTS Xapian plugin for Dovecot
=============================

What is this?
-------------

This project intends to provide a straightforward, simple and maintenance free, way to configure FTS plugin for [Dovecot](https://github.com/dovecot/), leveraging the efforts by the [Xapian.org](https://xapian.org/) team.

This effort came after Dovecot team decided to deprecate "fts_squat" included in the dovecot core, and due to the complexity of the Solr plugin capabilitles, unneeded for most users.

If you feel donating, kindly use Paypal : moreaujoan@gmail.com


Debugging/Support
-----------------
Please submit requests/bugs via the [GitHub issue tracker](https://github.com/grosjo/fts-xapian/issues).
A Matrix Room exists also at : #xapian-dovecot:matrix.grosjo.net


Availability in major distributions
-----------------------------------

THis plugin is readly available in major distributions under the name "dovecot-fts-xapian"
- Archlinux : https://archlinux.org/packages/?q=dovecot-fts-xapian
- Debian : https://packages.debian.org/bookworm/dovecot-fts-xapian
- Fedora : https://src.fedoraproject.org/rpms/dovecot-fts-xapian


Configuration - dovecot.conf file
---------------------------------

Update your dovecot.conf file with something similar to:

(Example in [conf.d/90-fts.conf](https://github.com/grosjo/fts-xapian/blob/master/contrib/conf.d/90-fts.conf) )

```
mail_plugins = (...) fts fts_xapian

(...)

plugin {
    fts = xapian
    fts_xapian = partial=3 

    fts_autoindex = yes
    fts_enforced = yes

    fts_autoindex_exclude = \Trash

    # Index attachements
    fts_decoder = decode2text
}

service indexer-worker {
    # Increase vsz_limit to 2GB or above.
    # Or 0 if you have rather large memory usable on your server, which is preferred for performance)
    vsz_limit = 2G
    # This one must be 0
    process_limit = 0
}

service decode2text {
    executable = script /usr/libexec/dovecot/decode2text.sh
    user = dovecot
    unix_listener decode2text {
        mode = 0666
    }
}
```

Make sure also that dovecot is started with enough files opening capacity (ideally set 'LimitNOFILE=65535' in the systemd start file).


Configuration - Indexing options
--------------------------------

| Option         | Optional | Description                     | Possible values                                     | Default value |
|----------------|----------|---------------------------------|-----------------------------------------------------|---------------|
| partial        |   no     | Minimum size of search keyword  | 3 or above                                          | 3             |
| verbose        |   yes    | Logs verbosity                  | 0 (silent), 1 (verbose) or 2 (debug)                | 0             |
| lowmemory      |   yes    | Memory limit before disk commit | 0 (default, meaning 250MB), or set value (in MB)    | 0             |
| maxthreads     |   yes    | Maximum number of threads       | 0 (default, hardware limit), or value above 2       | 0             |


Configuration - Index updating
------------------------------

Please make sure that "ulimit" is high enough. Typically, set "DefaultLimitNOFILE=16384:524288"  in /etc/systemd/system.conf

Just restart Dovecot:

```sh
sudo service restart dovecot
```

If this is not a fresh install of dovecot, you need to re-index your mailboxes:

```sh
doveadm index -A -q \*
```

- With argument `-A`, it will re-index all mailboxes, therefore may take a while.
- With argument `-q`, doveadm queues the indexing to be run by indexer process.
  Remove `-q` if you want to index immediately.

You shall put in a cron the following command (daily for instance) to cleanup indexes :

```sh
doveadm fts optimize -A
```



Building yourself - Prerequisites
----------------------------------

You are going to need the following things to get this going:

```
* Dovecot 2.2.x (or above)
* Xapian 1.2.x (or above)
* ICU 50.x (or above)
* SQlite 3.x
```

You will need to configure properly [Users Home Directories](https://doc.dovecot.org/2.3/configuration_manual/home_directories_for_virtual_users/) in dovecot configuration



Building yourself - Installing the Dovecot plugin
-----------------------------

First install the following packages, or equivalent for your operating system.

```
Ubuntu:
apt-get build-dep dovecot-core 
apt-get install dovecot-dev git libxapian-dev libicu-dev libsqlite3-dev autoconf automake libtool pkg-config

Archlinux:
pacman -S dovecot
pacman -S xapian-core icu git sqlite

FreeBSD:
pkg install xapian-core
pkg install xapian-bindings
pkg install icu
pkg install git

Fedora:
dnf install sqlite-devel libicu-devel xapian-core-devel
dnf install dovecot-devel git 
```

Clone this project:

```
git clone https://github.com/grosjo/fts-xapian
cd fts-xapian
```

Compile and install the plugin.

```
autoupdate
autoreconf -vi
./configure --with-dovecot=/path/to/dovecot
make
sudo make install
```

Note: if your system is quite old, you may change gnu++20 by gnu++11 in src/Makefile.in

Replace /path/to/dovecot by the actual path to 'dovecot-config'.
Type 'locate dovecot-config' in a shell to figure this out. On ArchLinux , it is /usr/lib/dovecot.

For specific configuration, you may have to 'export PKG_CONFIG_PATH=...'. To check that, type 'pkg-config --cflags-only-I icu-uc icu-io icu-i18n', it shall return no error.

The module will be placed into the module directory of your dovecot configuration



------


Thanks to Aki Tuomi <aki.tuomi@open-xchange.com>, Stephan Bosch <stephan@rename-it.nl>, Paul Hecker <paul@iwascoding.com>
