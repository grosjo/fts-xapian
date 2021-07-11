#!/bin/bash
#HELP : https://saveriomiroddi.github.io/Building-a-debian-deb-source-package-and-publishing-it-on-an-ubuntu-ppa/#the-procedure
export VERSION=1.4.11
rm dovecot*
git clone https://github.com/grosjo/fts-xapian.git
mv fts-xapian dovecot-fts-xapian-${VERSION}
cd dovecot-fts-xapian-${VERSION}
autoreconf -vi
./configure --with-dovecot=/usr/lib/dovecot
make
dh_make -p dovecot-fts-xapian-${VERSION} --single --native --copyright gpl2 --email jom@grosjo.net -y
rm debian/*.ex debian/*.EX
perl -i -pe 's/^(Section:).*/$1 utils/' debian/control
perl -i -pe 's/^(Homepage:).*/$1 https:\/\/testpackage.barryfoo.org/'              debian/control
perl -i -pe 's/^#(Vcs-Browser:).*/$1 https:\/\/github.com\/barryfoo\/testpackage/' debian/control
perl -i -pe 's/^#(Vcs-Git:).*/$1 https:\/\/github.com\/barryfoo\/testpackage.git/' debian/control
perl -i -pe 's/^(Standards-Version:) 3.9.6/$1 3.9.7/' debian/control
cp ../control debian/
cp ../changelog debian/
debuild -S | tee /tmp/debuild.log 2>&1
cd ..
dput ppa:grosjo/dovecot-fts-xapian dovecot-fts-xapian-${VERSION}_${VERSION}_source.changes


