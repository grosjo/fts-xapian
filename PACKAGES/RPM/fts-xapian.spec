Name:           dovecot-fts-xapian
Version:        1.4.9b
Release:        1%{?dist}
Summary:        Dovecot FTS plugin based on Xapian

License:        LGPLv2
URL:            https://github.com/grosjo/fts-xapian
Source0:        %{url}/archive/refs/tags/fts-xapian-%{version}.tar.gz

BuildRequires:  xapian-core-devel, libicu-devel, dovecot-devel
BuildRequires:  gcc, gcc-c++
BuildRequires:  automake, autoconf, libtool
Requires:       xapian-core, dovecot

%description
This project intends to provide a straightforward, simple and
maintenance free, way to configure FTS plugin for Dovecot, 
leveraging the efforts by the Xapian.org team.

This effort came after Dovecot team decided to deprecate 
"fts_squat" included in the dovecot core, and due to the 
complexity of the Solr plugin capabilitles, un-needed for most users.


%prep
%autosetup -n fts-xapian-%{version}
autoreconf -vi
%configure --enable-static=no --with-dovecot=%{_libdir}/dovecot


%build
%make_build

%install
%make_install

# We do not want the libtool archive or static library
rm %{buildroot}%{_libdir}/dovecot/lib21_fts_xapian_plugin.la


%files
%{_libdir}/dovecot/lib21_fts_xapian_plugin.so


%changelog
* Tue Apr  6 2021 xapian
- Initial RPM
