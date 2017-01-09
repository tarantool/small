Name: small
Version: 1.0.1
Release: 1%{?dist}
Summary: Collection of Specialized Memory ALLocators
Group: Development/Languages
License: BSD
URL: https://github.com/tarantool/small
Source0: https://github.com/tarantool/%{name}/archive/%{version}/%{name}-%{version}.tar.gz
BuildRequires: cmake >= 2.8
BuildRequires: gcc >= 4.5
%description
Collection of Specialized Memory ALLocators for small allocations

%package devel
Summary: Collection of Specialized Memory ALLocators
Requires: %{name}%{?_isa} = %{version}-%{release}

%description devel
Collection of Specialized Memory ALLocators for small allocations
This package contains development files.

%prep
%setup -q -n %{name}-%{version}

%build
%cmake . -DCMAKE_BUILD_TYPE=RelWithDebInfo
make %{?_smp_mflags}

%check
make %{?_smp_mflags} test

%install
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%{_libdir}/libsmall.so.1*

%files devel
%dir %{_includedir}/small
%{_includedir}/small/*.h
%{_includedir}/small/third_party/*.h
%{_includedir}/small/third_party/valgrind/*.h

%{_libdir}/libsmall.a
# unversioned libraries should belong devel package
%{_libdir}/libsmall.so

%changelog
* Wed Feb 17 2016 Roman Tsisyk <roman@tarabtool.org> 1.0.1-1
- Fix to comply Fedora Package Guidelines

* Tue Oct 27 2015 Eugine Blikh <bigbes@gmail.com> 1.0.0-1
- Initial version of the RPM spec
