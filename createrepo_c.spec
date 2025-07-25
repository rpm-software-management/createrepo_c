%global libmodulemd_version 2.3.0

%define __cmake_in_source_build 1

%global bash_completion %{_datadir}/bash-completion/completions/*

# Fedora infrastructure needs it for producing Fedora ≤ 39 and EPEL ≤ 7 repositories
# See https://github.com/rpm-software-management/createrepo_c/issues/398
%if ( 0%{?rhel} && ( 0%{?rhel} <= 7 || 0%{?rhel} >= 9 ) ) || ( 0%{?fedora} && 0%{?fedora} >= 45 )
%bcond_with drpm
%else
%bcond_without drpm
%endif

%if 0%{?rhel}
%bcond_with zchunk
%else
%bcond_without zchunk
%endif

%if 0%{?rhel} && 0%{?rhel} < 7
%bcond_with libmodulemd
%else
%bcond_without libmodulemd
%endif

%if 0%{?rhel} && 0%{?rhel} <= 8
%bcond_without legacy_hashes
%else
%bcond_with legacy_hashes
%endif

%bcond_with sanitizers

%if %{defined gitrev}
%define package_version %{?gitrev}
%else
%define package_version 1.2.1
%endif

Summary:        Creates a common metadata repository
Name:           createrepo_c
Version:        %{package_version}
Release:        1%{?dist}
License:        GPL-2.0-or-later
URL:            https://github.com/rpm-software-management/createrepo_c
Source0:        %{url}/archive/%{version}/%{name}-%{version}.tar.gz

%global epoch_dep %{?epoch:%{epoch}:}

BuildRequires:  cmake >= 3.7.0
BuildRequires:  gcc
BuildRequires:  bzip2-devel
BuildRequires:  doxygen
BuildRequires:  glib2-devel >= 2.22.0
BuildRequires:  libcurl-devel
BuildRequires:  libxml2-devel
BuildRequires:  openssl-devel
BuildRequires:  rpm-devel >= 4.8.0-28
BuildRequires:  sqlite-devel >= 3.6.18
BuildRequires:  xz
BuildRequires:  xz-devel
BuildRequires:  zlib-devel
%if %{with zchunk}
BuildRequires:  pkgconfig(zck) >= 0.9.11
BuildRequires:  zchunk
%endif
%if %{with libmodulemd}
BuildRequires:  pkgconfig(modulemd-2.0) >= %{libmodulemd_version}
%if 0%{?rhel} && 0%{?rhel} <= 7
BuildRequires:  libmodulemd2
Requires:       libmodulemd2%{?_isa} >= %{libmodulemd_version}
%else
BuildRequires:  libmodulemd
Requires:       libmodulemd%{?_isa} >= %{libmodulemd_version}
%endif
%endif
Requires:       %{name}-libs = %{epoch_dep}%{version}-%{release}
%if 0%{?fedora} > 40 || 0%{?rhel} > 10
BuildRequires:  bash-completion-devel
%else
BuildRequires:  bash-completion
%endif
Requires: rpm >= 4.9.0
%if %{with drpm}
BuildRequires:  drpm-devel >= 0.4.0
%endif
# dnf supports zstd since 8.4: https://bugzilla.redhat.com/show_bug.cgi?id=1914876
BuildRequires:  pkgconfig(libzstd)

%if %{with sanitizers}
BuildRequires:  libasan
BuildRequires:  liblsan
BuildRequires:  libubsan
%endif

%if 0%{?fedora} || 0%{?rhel} > 7
Obsoletes:      createrepo < 0.11.0
Provides:       createrepo = %{epoch_dep}%{version}-%{release}
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
Requires:   %{name}-libs%{?_isa} = %{epoch_dep}%{version}-%{release}

%description devel
This package contains the createrepo_c C library and header files.
These development files are for easy manipulation with a repodata.

%package -n python3-%{name}
Summary:        Python 3 bindings for the createrepo_c library
%{?python_provide:%python_provide python3-%{name}}
BuildRequires:  python3-devel
BuildRequires:  python3-setuptools
BuildRequires:  python3-sphinx
Requires:       %{name}-libs = %{epoch_dep}%{version}-%{release}

%description -n python3-%{name}
Python 3 bindings for the createrepo_c library.

%prep
%autosetup -p1
%py3_shebang_fix examples/python
mkdir build-py3

%build
# Build createrepo_c with Pyhon 3
pushd build-py3
  %cmake .. \
      -DWITH_ZCHUNK=%{?with_zchunk:ON}%{!?with_zchunk:OFF} \
      -DWITH_LIBMODULEMD=%{?with_libmodulemd:ON}%{!?with_libmodulemd:OFF} \
      -DWITH_LEGACY_HASHES=%{?with_legacy_hashes:ON}%{!?with_legacy_hashes:OFF} \
      -DENABLE_DRPM=%{?with_drpm:ON}%{!?with_drpm:OFF} \
      -DWITH_SANITIZERS=%{?with_sanitizers:ON}%{!?with_sanitizers:OFF}
  %cmake_build
  # Build C documentation
  %cmake_build -t doc-c
popd

%check
# Run Python 3 tests
pushd build-py3
  # Compile C tests
  %cmake_build -t tests

  # Run Python 3 tests
  %ctest -V
popd

%install
pushd build-py3
  # Install createrepo_c with Python 3
  %cmake_install
popd

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
%doc build-py3/doc/html
%{_libdir}/lib%{name}.so
%{_libdir}/pkgconfig/%{name}.pc
%{_includedir}/%{name}/

%files -n python3-%{name}
%doc examples/python/*
%{python3_sitearch}/%{name}/
%{python3_sitearch}/%{name}-*-py%{python3_version}.egg-info

%changelog
