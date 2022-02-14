Name: ucs-csummerize
Version: %{build_version}
%if 0%{?rhel}
    %define dist .el7
%endif
Release: %{build_release}%{?dist}
Summary: Annotation C++ library.
License: Commercial
Source0: %{name}-%{version}.tar.gz
BuildRequires: cmake, make, gcc
BuildRequires: cmake3, zlib-devel
BuildRequires: rapidjson, abseil-cpp-devel, libevent-devel, grpc-re2
BuildRequires: tinyxml2-devel
BuildRequires: ucs-mtc, ucs-moonycode, ucs-libmorph
BuildRequires: ucs-baalbek
Requires: tinyxml2

%description
Annotation C++ library.

%define project csummerize
%define _libdir /usr/local/lib
%define _incdir /usr/local/include/%{project}

%prep
%setup
cmake3 ./ -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_STATS=ON -DCMAKE_INSTALL_PREFIX=%{_prefix}

%build
make VERBOSE=1 all

%install
%make_install

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{_libdir}/*.a
%{_incdir}/*.h
%{_incdir}/*.hpp

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%changelog
