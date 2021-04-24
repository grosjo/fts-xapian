As root:

    Install the development environment and required devel packages:
    -- dnf groupinstall "Development Tools"
    -- dnf install rpm-build rpm-devel rpmlint make coreutils diffutils patch rpmdevtools
    -- dnf install dovecot-devel dovecot libicu-devel icu xapian-core xapian-core-devel

As a normal user:

    Create the ~/rpmbuild tree as a normal user (never build rpms as root):
    -- rpmdev-setuptree
    Place the spec file under:
    ~/rpmbuild/SPECS/fts-xapian.spec
    Place the tar.gz sources under:
    ~/rpmbuild/SOURCES/fts-xapian-1.4.9a.tar.gz
    Generate the binary rpm with:
    -- QA_RPATHS=$(( 0x0001|0x0010 )) rpmbuild -bb ~/rpmbuild/SPECS/fts-xapian.spec

Your RPM packages will be under ~/rpmbuild/RPMS/x86_64/

