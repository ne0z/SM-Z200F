Name:       gst-plugins-gspscaler
Summary:    GStreamer GSP Scaler Plugin for SPRD based chipset
Version:    0.0.2
Release:    3
Group:      TO_BE/FILLED_IN
License:    TO_BE/FILLED_IN
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  sec-product-features
BuildRequires:  prelink
BuildRequires:  pkgconfig(gstreamer-plugins-base-1.0)
BuildRequires:  pkgconfig(gstreamer-1.0)
BuildRequires:  pkgconfig(libdri2)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(xext)
BuildRequires:  pkgconfig(xv)
BuildRequires:  pkgconfig(xdamage)
BuildRequires:  pkgconfig(libtbm)
BuildRequires:  pkgconfig(dri2proto)
BuildRequires:  pkgconfig(xfixes)
BuildRequires:  pkgconfig(iniparser)
BuildRequires:  pkgconfig(libcrypto)
BuildRequires:  pkgconfig(mm-common)
%ifarch %{arm}
#!BuildIgnore: kernel-headers
BuildRequires:  kernel-headers-tizen-dev
%endif

Provides: libdrminterface.so

%description
Description: GStreamer GSP Scaler Plugin

%prep
%setup -q

%build
./autogen.sh
CFLAGS=" %{optflags} -DGST_EXT_XV_ENHANCEMENT -DGST_EXT_TIME_ANALYSIS -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" "; export CFLAGS

%configure \
%ifarch %{arm}
    --disable-i386 \
%else
    --enable-i386                      \
    --disable-ext-gspscaler            \
%endif

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp LICENSE.Apache-2.0 %{buildroot}/usr/share/license/%{name}
cat COPYING >>%{buildroot}/usr/share/license/%{name}
%make_install

%ifarch %{arm}

%files -n gst-plugins-gspscaler
%manifest gst-plugins-gspscaler.manifest
%defattr(-,root,root,-)
%{_libdir}/gstreamer-1.0/libgspscaler.so*
%{_libdir}/libdrminterface.so*
/usr/share/license/%{name}

%endif
