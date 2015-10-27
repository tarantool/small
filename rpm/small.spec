Name: small
Version: 1.0.0
Release: 1%{?dist}
Summary: Tarantool C connector
Group: Development/Languages
License: BSD
URL: https://github.com/tarantool/small
Source0: https://github.com/tarantool/small/archive/%{version}.tar.gz
# BuildRequires: cmake
# Strange bug.
# Fix according to http://www.jethrocarr.com/2012/05/23/bad-packaging-habits/
BuildRequires: cmake
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Vendor: tarantool.org
Group: Applications/Databases
%description
Collection of Specialized Memory ALLocators for small allocations

%package devel
Summary: Development files for C libtnt
Requires: small%{?_isa} = %{version}-%{release}
%description devel
Collection of Specialized Memory ALLocators for small allocations
This package contains development files.

##################################################################

%prep
%setup -c -q %{name}-%{version}

%build
cmake . -DCMAKE_INSTALL_LIBDIR='%{_libdir}' -DCMAKE_INSTALL_INCLUDEDIR='%{_includedir}' -DCMAKE_BUILD_TYPE='Release'
make

%install
make DESTDIR=%{buildroot} install

%files
"%{_libdir}/libsmall.a"
"%{_libdir}/libsmall.so*"

%files devel
%dir "%{_includedir}/small"
"%{_includedir}/small/*.h"

%changelog
*  Oct 27 2015 Eugine Blikh <bigbes@gmail.com> 1.0.0-1
- Initial version of the RPM spec
