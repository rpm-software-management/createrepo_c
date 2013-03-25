Name:           balicek-iso88592
Version:        1.1.1
Release:        1
License:        GPL
Summary:        Balíèek s "ì¹èø¾ıáíéù"
Group:          System Environment/Shells
Url:            http://fake_bash_shell.com/
#Source:         %{name}-%{version}.tar.gz

Provides: Balíèek
Requires: bílıkùò

%description
Bílı kùò pìl ódy ná ná.
"ì¹èø¾ıáíéù"

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
* Tue Apr 17 2012 Tomá¹ Mlèoch <tmlcoch@redhat.com> - 1.1.1-1
- Nìjakı comment "ì¹èø¾ıáíéùúÁ©ÈØ®İÁÍÉÙÚ"
