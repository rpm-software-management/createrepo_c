Name:           fake_bash
Version:        1.1.1
Release:        1
License:        GPL
Summary:        Fake bash
Group:          System Environment/Shells
Url:            http://fake_bash_shell.com/
#Source:         %{name}-%{version}.tar.gz

Requires: super_kernel

Provides: bash

%description
Fake bash package


#%prep
#%setup -q


%build
echo OK


%install
rm -rf $RPM_BUILD_ROOT
mkdir $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin/
touch $RPM_BUILD_ROOT/usr/bin/fake_bash


%clean
rm -rf $RPM_BUILD_ROOT


%files
%{_bindir}/fake_bash

%changelog
* Tue Apr 17 2012 Tomas Mlcoch <tmlcoch@redhat.com> - 1.1.1-1
- First release
