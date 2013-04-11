Name:           balicek-utf8
Version:        1.1.1
Release:        1
License:        GPL
Summary:        Balíček s "ěščřžýáíéů"
Group:          System Environment/Shells
Url:            http://fake_bash_shell.com/
#Source:         %{name}-%{version}.tar.gz

Provides: Balíček
Requires: bílýkůň

%description
Bílý kůň pěl ódy ná ná.
"ěščřžýáíéů"

#%prep
#%setup -q

%build
echo OK

%install
rm -rf $RPM_BUILD_ROOT
mkdir $RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files

%changelog
* Tue Apr 17 2012 Tomáš Mlčoch <tmlcoch@redhat.com> - 1.1.1-1
- Nějaký comment "ěščřžýáíéůúÁŠČŘŽÝÁÍÉŮÚ"
