Name:           super_kernel
Version:        6.0.1
Release:        2
License:        LGPLv2
Summary:        Test package
Group:          Applications/System
Url:            http://so_super_kernel.com/it_is_awesome/yep_it_really_is
#Source:         %{name}-%{version}.tar.gz

BuildRequires: glib2-devel >= 2.26.0
BuildRequires: file-devel

Requires: glib >= 2.26.0
Requires: zlib

PreReq: bzip2 >= 1.0.0
PreReq: expat

Provides: super_kernel == 6.0.0
Provides: not_so_super_kernel < 5.8.0

Conflicts: kernel
Conflicts: super_kernel == 5.0.0
Conflicts: super_kernel < 4.0.0

Obsoletes: super_kernel == 5.9.0
Obsoletes: kernel

%description
This package has provides, requires, obsoletes, conflicts options.


#%prep
#%setup -q


%build
echo OK


%install
rm -rf $RPM_BUILD_ROOT
mkdir $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/share/man/
mkdir -p $RPM_BUILD_ROOT/usr/bin/
touch $RPM_BUILD_ROOT/usr/share/man/super_kernel.8.gz
touch $RPM_BUILD_ROOT/usr/bin/super_kernel


%clean
rm -rf $RPM_BUILD_ROOT


%files
%doc %_mandir/super_kernel.8.gz
%{_bindir}/super_kernel

%changelog
* Tue Apr 17 2012 Tomas Mlcoch <tmlcoch@redhat.com> - 6.0.1-2
- Second release

* Tue Apr 17 2012 Tomas Mlcoch <tmlcoch@redhat.com> - 6.0.1-1
- First release
