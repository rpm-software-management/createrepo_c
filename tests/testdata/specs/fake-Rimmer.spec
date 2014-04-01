Name:           Rimmer
Version:        1.0.2
Release:        2
License:        GPL
Summary:        Package with weak deps.
Group:          Development/Tools
Url:            http://pkgwithweakdeps.eu/
Packager:       Arnold Rimmer

Requires: req <= 1
Requires(pre): reqpre = 2
Provides: pro <= 3
Obsoletes: obs <= 4
Conflicts: con <= 5
Suggests: sug > 6
Enhances: enh < 7
Recommends: rec = 8
Supplements: sup > 9

%description
Package with weak deps.

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
* Wed Apr 18 2012 Tomas Mlcoch <tmlcoch@redhat.com> - 2.2.2-2
- Look, we've all got something to contribute to this discussion. And
  I think what you should contribute from now on is silence.

* Tue Apr 17 2012 Tomas Mlcoch <tmlcoch@redhat.com> - 1.1.1-1
- First changelog.
