VERSION=1.5.3
FEDORA=fc35
REP=`pwd`
kinit grosjo@FEDORAPROJECT.ORG
rpmdev-setuptree
cat dovecot-fts-xapian.spec.in | sed -e s/FTSVERSION/${VERSION}/g > dovecot-fts-xapian.spec
cp dovecot-fts-xapian.spec ~/rpmbuild/SPECS/
wget https://github.com/grosjo/fts-xapian/archive/refs/tags/${VERSION}.tar.gz -O ~/rpmbuild/SOURCES/dovecot-fts-xapian-${VERSION}.tar.gz
QA_RPATHS=$(( 0x0001|0x0010 )) rpmbuild --bb --nodebuginfo ~/rpmbuild/SPECS/dovecot-fts-xapian.spec
QA_RPATHS=$(( 0x0001|0x0010 )) rpmbuild --bs --nodebuginfo ~/rpmbuild/SPECS/dovecot-fts-xapian.spec
cp ~/rpmbuild/SRPMS/dovecot-fts-xapian-${VERSION}-1.${FEDORA}.src.rpm ${REP}/
cp ~/rpmbuild/RPMS/x86_64/dovecot-fts-xapian-${VERSION}-1.${FEDORA}.x86_64.rpm ${REP}/
koji build --scratch f35 ./dovecot-fts-xapian-${VERSION}-1.${FEDORA}.src.rpm
rm -rf fedora
mkdir fedora
cd fedora
fedpkg clone dovecot-fts-xapian
cd dovecot-fts-xapian
fedpkg switch-branch rawhide
cp ../../dovecot-fts-xapian.spec ./
git add dovecot-fts-xapian.spec
git commit -m "Version $VERSION}"
git push
fedpkg import ../../dovecot-fts-xapian-${VERSION}-1.${FEDORA}.src.rpm
git add .gitignore sources
git commit -m "Version $VERSION}"
git push
fedpkg build
fedpkg update
fedpkg switch-branch f35
git merge rawhide
fedpkg push
fedpkg build
fedpkg update
fedpkg switch-branch f34
git merge rawhide
fedpkg push
fedpkg build
fedpkg update
fedpkg switch-branch f33
<<<<<<< HEAD
=======
git merge rawhide
fedpkg push
fedpkg build
fedpkg update
fedpkg switch-branch el8
>>>>>>> 709e1d0 (Fedaora 1.5.1)
git merge rawhide
fedpkg push
fedpkg build
fedpkg update
<<<<<<< HEAD
fedpkg switch-branch epel8
git merge rawhide
fedpkg push
fedpkg build
fedpkg update
=======
>>>>>>> 709e1d0 (Fedaora 1.5.1)

