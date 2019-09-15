Name:       gst-plugins-camera-sc9830
Summary:    Gstreamer codec plugins package for SPRD series
Version:    0.0.16
Release:    1
Group:      libs
License:    TO_BE_FILL
Source0:    %{name}-%{version}.tar.gz
ExclusiveArch:  %arm
BuildRequires:  pkgconfig(gstreamer-plugins-base-1.0)
BuildRequires:  pkgconfig(gstreamer-1.0)
BuildRequires:  gstreamer-plugins-base1.0-devel
BuildRequires:  pkgconfig(mm-common)
BuildRequires:  pkgconfig(dlog)
#BuildRequires:  gstreamer-devel
#!BuildIgnore: kernel-headers
BuildRequires:  kernel-headers-tizen-dev
BuildRequires:  pkgconfig(camera-interface-sprd-sc9830)
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(libtbm)
BuildRequires:  pkgconfig(mmutil-imgp)
BuildRequires:  pkgconfig(mmutil-jpeg)
BuildRequires:  pkgconfig(libdri2)
BuildRequires:  pkgconfig(xfixes)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  sec-product-features
BuildRequires: pkgconfig(libdrm)
BuildRequires:  pkgconfig(csc-feature)
BuildRequires:  pkgconfig(dbus-1)


Provides: libcamerahal.so
Provides: libcamerahdr.so

%description
Gstreamer camera plugin package for SPRD series
 GStreamer is a streaming media framework, based on graphs of filters
 which operate on media data. Multimedia Framework using this plugins
 library can encode and decode video, audio, and speech..

%prep
%setup -q

%build
export CFLAGS+=" -DGST_EXT_TIME_ANALYSIS -include stdint.h"
export CPPFLAGS+=" -include stdint.h"
sh ./autogen.sh
%configure --disable-static
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp LICENSE.Apache-2.0 %{buildroot}/usr/share/license/%{name}
cat COPYING >>%{buildroot}/usr/share/license/%{name}
%make_install

%files
%manifest gst-plugins-camera-sc9830.manifest
%defattr(-,root,root,-)
%{_libdir}/gstreamer-1.0/lib*.so*
%{_libdir}/libcamerahal.so*
%{_libdir}/libcamerahdr.so*
%{_datadir}/license/%{name}

