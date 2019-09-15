%define baseline 2.12
Name:           atk
Version:        2.12.2
Release:        0
License:        LGPL-2.0+
Summary:        An Accessibility ToolKit
Url:            http://www.gtk.org/
Group:          System/Libraries
Source:         http://download.gnome.org/sources/%{name}/%{baseline}/%{name}-%{version}.tar.xz
Source98:       baselibs.conf
Source1001:     %{name}.manifest

BuildRequires:  fdupes
BuildRequires:  gcc-c++
BuildRequires:  glib2-devel >= 2.35.2
BuildRequires:  intltool
BuildRequires:  which
%if %{with introspection}
BuildRequires:  gobject-introspection-devel
%endif
Requires:       libatk

%description
The ATK library provides a set of accessibility interfaces. By
supporting the ATK interfaces, an application or toolkit can be used
with screen readers, magnifiers, and alternate input devices.

%package -n libatk
Summary:        An Accessibility ToolKit
Group:          System/Libraries
Provides:       %{name} = %{version}
Obsoletes:      %{name} < %{version}

%description -n libatk
The ATK library provides a set of accessibility interfaces. By
supporting the ATK interfaces, an application or toolkit can be used
with screen readers, magnifiers, and alternate input devices.

%package -n typelib-Atk
Summary:        An Accessibility ToolKit -- Introspection bindings
Group:          System/Libraries

%description -n typelib-Atk
The ATK library provides a set of accessibility interfaces. By
supporting the ATK interfaces, an application or toolkit can be used
with screen readers, magnifiers, and alternate input devices.

This package provides the GObject Introspection bindings for ATK.

%package devel
Summary:        Include Files and Libraries mandatory for Development
Group:          Development/Libraries/GNOME
Requires:       libatk = %{version}

%description devel
This package contains all necessary include files and libraries needed
to develop applications that require these.

%prep
%setup -q

%build
%autogen \
  --disable-static
make %{?_smp_mflags}

%install
%make_install
%fdupes %{buildroot}

mkdir -p %{buildroot}/usr/share/license
cp -f COPYING %{buildroot}/usr/share/license/%{name}

%post -n libatk -p /sbin/ldconfig

%postun -n libatk -p /sbin/ldconfig

%files -n libatk
%defattr(-, root, root)
%doc COPYING
/usr/share/license/%{name}
%{_libdir}/lib*.so.*

%files devel
%defattr(-, root, root)
%{_includedir}/atk-1.0
%{_libdir}/lib*.so
%{_libdir}/pkgconfig/*.pc

