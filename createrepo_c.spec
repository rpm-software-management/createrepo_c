%global libmodulemd_version 2.3.0

# Bash completion (we need different approach for RHEL-6)
%if 0%{?rhel} == 6
%global bash_completion %config%{_sysconfdir}/bash_completion.d/createrepo_c.bash
%else
%global bash_completion %{_datadir}/bash-completion/completions/*
%endif

%if 0%{?rhel} && 0%{?rhel} <= 7
%bcond_with python3
%bcond_with drpm
%else
%bcond_without python3
%bcond_without drpm
%endif

%if 0%{?fedora} > 29 || 0%{?rhel} > 7
%bcond_with python2
%else
%bcond_without python2
%endif

%if 0%{?rhel} || 0%{?fedora} < 29
%bcond_with zchunk
%else
%bcond_without zchunk
%endif

%if 0%{?rhel} || 0%{?fedora} < 29
%bcond_with libmodulemd
%else
%bcond_without libmodulemd
%endif

Summary:        Creates a common metadata repository
Name:           createrepo_c
Version:        0.15.8
Release:        1%{?dist}
License:        GPLv2+
URL:            https://github.com/rpm-software-management/createrepo_c
Source0:        %{url}/archive/%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc
BuildRequires:  bzip2-devel
BuildRequires:  doxygen
BuildRequires:  file-devel
BuildRequires:  glib2-devel >= 2.22.0
BuildRequires:  libcurl-devel
BuildRequires:  libxml2-devel
BuildRequires:  openssl-devel
BuildRequires:  rpm-devel >= 4.8.0-28
BuildRequires:  sqlite-devel
BuildRequires:  xz-devel
BuildRequires:  zlib-devel
%if %{with zchunk}
BuildRequires:  pkgconfig(zck) >= 0.9.11
BuildRequires:  zchunk
%endif
%if %{with libmodulemd}
BuildRequires:  pkgconfig(modulemd-2.0) >= %{libmodulemd_version}
BuildRequires:  libmodulemd
Requires:       libmodulemd%{?_isa} >= %{libmodulemd_version}
%endif
Requires:       %{name}-libs =  %{version}-%{release}
%if 0%{?rhel} == 6
Requires: rpm >= 4.8.0-28
%else
BuildRequires:  bash-completion
Requires: rpm >= 4.9.0
%endif
%if %{with drpm}
BuildRequires:  drpm-devel >= 0.4.0
%endif

%if 0%{?fedora} || 0%{?rhel} > 7
Obsoletes:      createrepo < 0.11.0
Provides:       createrepo = %{version}-%{release}
%endif

%description
C implementation of Createrepo.
A set of utilities (createrepo_c, mergerepo_c, modifyrepo_c)
for generating a common metadata repository from a directory of
rpm packages and maintaining it.

%package libs
Summary:    Library for repodata manipulation

%description libs
Libraries for applications using the createrepo_c library
for easy manipulation with a repodata.

%package devel
Summary:    Library for repodata manipulation
Requires:   %{name}-libs%{?_isa} = %{version}-%{release}

%description devel
This package contains the createrepo_c C library and header files.
These development files are for easy manipulation with a repodata.

%if %{with python2}
%package -n python2-%{name}
Summary:        Python bindings for the createrepo_c library
%{?python_provide:%python_provide python2-%{name}}
BuildRequires:  python2-devel
BuildRequires:  python2-nose
%if 0%{?rhel} && 0%{?rhel} <= 7
BuildRequires:  python-sphinx
%else
BuildRequires:  python2-sphinx
%endif
Requires:       %{name}-libs = %{version}-%{release}

%description -n python2-%{name}
Python bindings for the createrepo_c library.
%endif

%if %{with python3}
%package -n python3-%{name}
Summary:        Python 3 bindings for the createrepo_c library
%{?python_provide:%python_provide python3-%{name}}
BuildRequires:  python3-devel
BuildRequires:  python3-nose
BuildRequires:  python3-sphinx
Requires:       %{name}-libs = %{version}-%{release}

%description -n python3-%{name}
Python 3 bindings for the createrepo_c library.
%endif

%prep
%autosetup -p1
%if %{with python2}
mkdir build-py2
%endif

%if %{with python3}
mkdir build-py3
%endif

%build
# Build createrepo_c with Python 2
%if %{with python2}
pushd build-py2
  %cmake .. -DPYTHON_DESIRED:FILEPATH=%{__python2} %{!?with_zchunk:-DWITH_ZCHUNK=OFF} %{!?with_libmodulemd:-DWITH_LIBMODULEMD=OFF}
  make %{?_smp_mflags} RPM_OPT_FLAGS="%{optflags}"
  %if %{without python3}
  # Build C documentation
  make doc-c
  %endif
popd
%endif

# Build createrepo_c with Pyhon 3
%if %{with python3}
pushd build-py3
  %cmake .. -DPYTHON_DESIRED:FILEPATH=%{__python3} %{!?with_zchunk:-DWITH_ZCHUNK=OFF} %{!?with_libmodulemd:-DWITH_LIBMODULEMD=OFF}
  make %{?_smp_mflags} RPM_OPT_FLAGS="%{optflags}"
  # Build C documentation
  make doc-c
popd
%endif

%check
%if %{with python2}
pushd build-py2
  %if %{without python3}
  # Compile C tests
  make tests
  %endif

  # Run Python 2 tests
  make ARGS="-V" test
popd
%endif

# Run Python 3 tests
%if %{with python3}
pushd build-py3
  # Compile C tests
  make tests

  # Run Python 3 tests
  make ARGS="-V" test
popd
%endif

%install
%if %{with python2}
pushd build-py2
  # Install createrepo_c with Python 2
  make install DESTDIR=%{buildroot}
popd
%endif

%if %{with python3}
pushd build-py3
  # Install createrepo_c with Python 3
  make install DESTDIR=%{buildroot}
popd
%endif

%if 0%{?fedora} || 0%{?rhel} > 7
ln -sr %{buildroot}%{_bindir}/createrepo_c %{buildroot}%{_bindir}/createrepo
ln -sr %{buildroot}%{_bindir}/mergerepo_c %{buildroot}%{_bindir}/mergerepo
ln -sr %{buildroot}%{_bindir}/modifyrepo_c %{buildroot}%{_bindir}/modifyrepo
%endif

%if 0%{?rhel} && 0%{?rhel} <= 7
%post libs -p /sbin/ldconfig
%postun libs -p /sbin/ldconfig
%else
%ldconfig_scriptlets libs
%endif

%files
%doc README.md
%{_mandir}/man8/createrepo_c.8*
%{_mandir}/man8/mergerepo_c.8*
%{_mandir}/man8/modifyrepo_c.8*
%{_mandir}/man8/sqliterepo_c.8*
%{bash_completion}
%{_bindir}/createrepo_c
%{_bindir}/mergerepo_c
%{_bindir}/modifyrepo_c
%{_bindir}/sqliterepo_c

%if 0%{?fedora} || 0%{?rhel} > 7
%{_bindir}/createrepo
%{_bindir}/mergerepo
%{_bindir}/modifyrepo
%endif

%files libs
%license COPYING
%{_libdir}/lib%{name}.so.*

%files devel
%if %{with python3}
%doc build-py3/doc/html
%else
%doc build-py2/doc/html
%endif
%{_libdir}/lib%{name}.so
%{_libdir}/pkgconfig/%{name}.pc
%{_includedir}/%{name}/

%if %{with python2}
%files -n python2-%{name}
%{python2_sitearch}/%{name}/
%{python2_sitearch}/%{name}-%{version}-py%{python2_version}.egg-info
%endif

%if %{with python3}
%files -n python3-%{name}
%{python3_sitearch}/%{name}/
%{python3_sitearch}/%{name}-%{version}-py%{python3_version}.egg-info
%endif

%changelog
