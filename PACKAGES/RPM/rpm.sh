VERSION=1.4.11
REP=`pwd`
rpmdev-setuptree
cat dovecot-fts-xapian.spec.in | sed -e s/FTSVERSION/${VERSION}/g > dovecot-fts-xapian.spec
cp dovecot-fts-xapian.spec ~/rpmbuild/SPECS/
wget https://github.com/grosjo/fts-xapian/archive/refs/tags/${VERSION}.tar.gz -O ~/rpmbuild/SOURCES/dovecot-fts-xapian-${VERSION}.tar.gz
QA_RPATHS=$(( 0x0001|0x0010 )) rpmbuild --bb --nodebuginfo ~/rpmbuild/SPECS/dovecot-fts-xapian.spec
QA_RPATHS=$(( 0x0001|0x0010 )) rpmbuild --bs --nodebuginfo ~/rpmbuild/SPECS/dovecot-fts-xapian.spec
cp ~/rpmbuild/SRPMS/dovecot-fts-xapian-${VERSION}-1.fc34.src.rpm ${REP}/
cp ~/rpmbuild/RPMS/x86_64/dovecot-fts-xapian-${VERSION}-1.fc34.x86_64.rpm ${REP}/
koji build --scratch f34 ./dovecot-fts-xapian-${VERSION}-1.fc34.src.rpm

