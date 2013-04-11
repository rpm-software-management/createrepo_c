Name:           balicek-iso88591
Version:        1.1.1
Release:        1
License:        GPL
Summary:        Balicek s "אבגדהוח"
Group:          System Environment/Shells
Url:            http://fake_bash_shell.com/
#Source:         %{name}-%{version}.tar.gz

Provides: Balicek
Requires: אבגדהוח

%description
divny o: צ "אבגדהוח" divny u: ü

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
* Tue Apr 17 2012 Tomבs Mlcoch <tmlcoch@redhat.com> - 1.1.1-1
- Nejak‎ comment "אבגדהוח"
