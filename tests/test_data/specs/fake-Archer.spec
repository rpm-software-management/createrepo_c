Name:           Archer
Epoch:          2
Version:        3.4.5
Release:        6
License:        GPL
Summary:        Complex package.
Group:          Development/Tools
Url:            http://soo_complex_package.eu/
Packager:       Sterling Archer
Vendor:         ISIS
#Source:         %{name}-%{version}.tar.gz

Requires: fooa <= 2
Requires: foob >= 1.0.0-1
Requires: fooc = 3
Requires: food < 4
Requires: fooe > 5
Requires(pre): foof = 6

Provides: bara <= 22
Provides: barb >= 11.22.33-44
Provides: barc = 33
Provides: bard < 44
Provides: bare > 55

Obsoletes: aaa <= 222
Obsoletes: aab >= 111.2.3-4
Obsoletes: aac = 333
Obsoletes: aad < 444
Obsoletes: aae > 555

Conflicts: bba <= 2222
Conflicts: bbb >= 1111.2222.3333-4444
Conflicts: bbc = 3333
Conflicts: bbd < 4444
Conflicts: bbe > 5555

%description
Archer package

#%prep
#%setup -q

%build
touch README
echo OK

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin/
mkdir -p $RPM_BUILD_ROOT/usr/share/doc/%{name}-%{version}/
touch $RPM_BUILD_ROOT/usr/bin/complex_a

%clean
rm -rf $RPM_BUILD_ROOT

%files
%{_bindir}/complex_a
%doc README

%changelog
* Mon Apr  8 2013 Tomas Mlcoch <tmlcoch@redhat.com> - 3.3.3-3
- 3. changelog.

* Wed Apr 18 2012 Tomas Mlcoch <tmlcoch@redhat.com> - 2.2.2-2
- That was totally ninja!

* Tue Apr 17 2012 Tomas Mlcoch <tmlcoch@redhat.com> - 1.1.1-1
- First changelog.
