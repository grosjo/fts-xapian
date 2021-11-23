%global __brp_check_rpaths %{nil}
Name:           dovecot-fts-xapian
Version:        1.5.2
Release:        1%{?dist}
Summary:        Dovecot FTS plugin based on Xapian

License:        LGPLv2
URL:            https://github.com/grosjo/fts-xapian
Source0:        %{url}/archive/%{version}/%{name}-%{version}.tar.gz

BuildRequires:  xapian-core-devel, libicu-devel, dovecot-devel
BuildRequires:  gcc, gcc-c++, make, automake, autoconf, libtool
Requires:       xapian-core, dovecot

%description
This project intends to provide a straightforward, simple and
maintenance free, way to configure FTS plugin for Dovecot, 
leveraging the efforts by the Xapian.org team.

This effort came after Dovecot team decided to deprecate 
"fts_squat" included in the dovecot core, and due to the 
complexity of the Solr plugin capabilitles, un-needed for most
users.


%prep
%autosetup -n fts-xapian-%{version}
autoreconf -vi
%configure --enable-static=no --with-dovecot=%{_libdir}/dovecot


%build
%make_build

%install
%make_install
rm %{buildroot}%{_libdir}/dovecot/lib21_fts_xapian_plugin.la

%post
echo ">>> Inside post <<<"

%files
%license COPYING
%doc AUTHORS README.md
%{_libdir}/dovecot/lib21_fts_xapian_plugin.so


%changelog
* Tue Nov 23 2021 Joan Moreau <jom@grosjo.net> - 1.5.2
- Issues 103, 106, 109, 110
* Thu Nov 11 2021 Joan Moreau <jom@grosjo.net> - 1.5.1
- Fixed preprocessor issue 
* Wed Nov 10 2021 Joan Moreau <jom@grosjo.net> - 1.5.0
- FreeBSD compatibility
* Mon Nov 1 2021 Joan Moreau <jom@grosjo.net> - 1.4.14-1
- Alignment with Dovecot 2.3.17
- Better memory management for FreeBSD
* Sun Sep 12 2021 Joan Moreau <jom@grosjo.net> - 1.4.13-1
- Rebuild for dovecot 2.3.16
- Epel7 comptability
* Sat Aug 14 2021 Joan Moreau <jom@grosjo.net> - 1.4.12-1
- cf Github
* Sun Jul  4 2021 Joan Moreau <jom@grosjo.net> - 1.4.11-1
- cf Github
* Sat Jun 26 2021 Joan Moreau <jom@grosjo.net> - 1.4.10-1
- cf Github
* Tue Apr  6 2021 Joan Moreau <jom@grosjo.net> - 1.4.9b-1
- Initial RPM
