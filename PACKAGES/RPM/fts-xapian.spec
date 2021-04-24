Name:           fts-xapian
Version:        1.4.9a
Release:        1%{?dist}
Summary:        Dovecot FTS plugin based on Xapian

License:        LGPL-2.1
URL:            https://github.com/grosjo/fts-xapian
Source0:        fts-xapian-1.4.9a.tar.gz

BuildRequires:  xapian-core-devel, libicu-devel, dovecot-devel
Requires:       xapian-core, xapian-core-libs, dovecot

%description
This project intends to provide a straightforward, simple and maintenance free, way to configure FTS plugin for Dovecot, leveraging the efforts by the Xapian.org team.

This effort came after Dovecot team decided to deprecate "fts_squat" included in the dovecot core, and due to the complexity of the Solr plugin capabilitles, un-needed for most users.


%prep
%autosetup
autoreconf -vi
./configure --with-dovecot=/usr/lib64/dovecot


%build
make %{?_smp_mflags}


%install
%make_install


%files
/usr/lib64/dovecot/lib21_fts_xapian_plugin.la
/usr/lib64/dovecot/lib21_fts_xapian_plugin.so
/usr/lib64/dovecot/lib21_fts_xapian_plugin.a


%changelog
* Tue Apr  6 2021 xapian
- 
