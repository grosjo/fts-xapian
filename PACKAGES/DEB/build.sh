#!/bin/bash
#HELP : https://saveriomiroddi.github.io/Building-a-debian-deb-source-package-and-publishing-it-on-an-ubuntu-ppa/#the-procedure
export VERSION=1.4.11-6
export BASE=1.4.11
rm -rf dovecot*
git clone https://github.com/grosjo/fts-xapian.git dovecot-fts-xapian-${VERSION}
tar -czf dovecot-fts-xapian-${VERSION}_${BASE}.orig.tar.gz dovecot-fts-xapian-${VERSION}
cd dovecot-fts-xapian-${VERSION}
autoreconf -vi
./configure --with-dovecot=/usr/lib/dovecot
make
dh_make -p dovecot-fts-xapian-${VERSION} --single --native --copyright gpl2 --email jom@grosjo.net -y
rm debian/*.ex debian/*.EX
cp ../control debian/
cp ../changelog debian/
cp ../compat debian/
debuild -S | tee /tmp/debuild.log 2>&1
cd ..
dput ppa:grosjo/dovecot-fts-xapian dovecot-fts-xapian-${VERSION}_${VERSION}_source.changes


