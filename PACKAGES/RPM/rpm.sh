VERSION=1.4.9b
REP=`pwd`
rpmdev-setuptree
cat fts-xapian.spec.in | sed -e s/FTSVERSION/${VERSION}/g > fts-xapian.spec
cp fts-xapian.spec ~/rpmbuild/SPECS/
wget https://github.com/grosjo/fts-xapian/archive/refs/tags/${VERSION}.tar.gz -O ~/rpmbuild/SOURCES/fts-xapian-${VERSION}.tar.gz
QA_RPATHS=$(( 0x0001|0x0010 )) rpmbuild --bb --nodebuginfo ~/rpmbuild/SPECS/fts-xapian.spec
QA_RPATHS=$(( 0x0001|0x0010 )) rpmbuild --bs --nodebuginfo ~/rpmbuild/SPECS/fts-xapian.spec
cp ~/rpmbuild/SRPMS/dovecot-fts-xapian-${VERSION}-1.fc33.src.rpm ${REP}/
cp ~/rpmbuild/RPMS/x86_64/dovecot-fts-xapian-${VERSION}-1.fc33.x86_64.rpm ${REP}/
koji build --scratch f33 ./dovecot-fts-xapian-${VERSION}-1.fc33.src.rpm

