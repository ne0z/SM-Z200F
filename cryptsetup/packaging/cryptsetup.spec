Summary: A utility for setting up encrypted disks
Name: cryptsetup
Version: 1.6.7
Release: 2
License: GPLv2+ and LGPLv2+
Group: Applications/System
Source0: cryptsetup-%{version}.tar.xz
URL: https://gitlab.com/cryptsetup/cryptsetup
BuildRequires: libgcrypt-devel, popt-devel, device-mapper-devel
BuildRequires: libgpg-error-devel, libuuid-devel
Provides: cryptsetup-luks = %{version}-%{release}
Obsoletes: cryptsetup-luks < 1.4.0
Requires: cryptsetup-libs = %{version}-%{release}

%description
The cryptsetup package contains a utility for setting up
disk encryption using dm-crypt kernel module.

%package devel
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}
Requires: libgcrypt-devel > 1.1.42, device-mapper-devel, libuuid-devel
Requires: pkgconfig
Summary: Headers and libraries for using encrypted file systems
Provides: cryptsetup-luks-devel = %{version}-%{release}
Obsoletes: cryptsetup-luks-devel < 1.4.0

%description devel
The cryptsetup-devel package contains libraries and header files
used for writing code that makes use of disk encryption.

%package libs
Group: System Environment/Libraries
Summary: Cryptsetup shared library
Provides: cryptsetup-luks-libs = %{version}-%{release}
Obsoletes: cryptsetup-luks-libs < 1.4.0
# Need support for fixed gcrypt PBKDF2 and fixed Whirlpool hash.
Requires: libgcrypt >= 1.6.1

%description libs
This package contains the cryptsetup shared library, libcryptsetup.

%package -n veritysetup
Group: Applications/System
Summary: A utility for setting up dm-verity volumes
Requires: cryptsetup-libs = %{version}-%{release}

%description -n veritysetup
The veritysetup package contains a utility for setting up
disk verification using dm-verity kernel module.

# %package reencrypt
# Group: Applications/System
# Summary: A utility for offline reencryption of LUKS encrypted disks.
# Requires: cryptsetup-libs = %{version}-%{release}

# %description reencrypt
# This package contains cryptsetup-reencrypt utility which
# can be used for offline reencryption of disk in situ.

%prep
%setup -q -n cryptsetup-%{version}
# chmod -x misc/dracut_90reencrypt/*

%build
%configure \
        --disable-nls \
        --disable-python \
        --disable-selinux \
        --disable-pwquality \

# %{?configure_pbkdf2}

# remove rpath
sed -i 's|^hardcode_libdir_flag_spec=.*|hardcode_libdir_flag_spec=""|g' libtool
sed -i 's|^runpath_var=LD_RUN_PATH|runpath_var=DIE_RPATH_DIE|g' libtool
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}
rm -rf %{buildroot}/%{_libdir}/*.la
# %find_lang cryptsetup

mkdir -p %{buildroot}%{_datadir}/license
cat COPYING > %{buildroot}%{_datadir}/license/cryptsetup
cat COPYING > %{buildroot}%{_datadir}/license/veritysetup
cat COPYING.LGPL > %{buildroot}%{_datadir}/license/cryptsetup-libs

%post -n cryptsetup-libs -p /sbin/ldconfig

%postun -n cryptsetup-libs -p /sbin/ldconfig

%files
%{!?_licensedir:%global license %%doc}
%license COPYING
%{_datadir}/license/cryptsetup
%doc AUTHORS FAQ docs/*ReleaseNotes
%{_mandir}/man8/cryptsetup.8.gz
%{_sbindir}/cryptsetup

%files -n veritysetup
%{!?_licensedir:%global license %%doc}
%license COPYING
%{_datadir}/license/veritysetup
%{_mandir}/man8/veritysetup.8.gz
%{_sbindir}/veritysetup

# %files reencrypt
# %{!?_licensedir:%global license %%doc}
# %license COPYING
# %doc misc/dracut_90reencrypt
# %{_mandir}/man8/cryptsetup-reencrypt.8.gz
# %{_sbindir}/cryptsetup-reencrypt

%files devel
%doc docs/examples/*
%{_includedir}/libcryptsetup.h
%{_libdir}/libcryptsetup.so
%{_libdir}/pkgconfig/libcryptsetup.pc

# %files libs -f cryptsetup.lang
%files libs
%{!?_licensedir:%global license %%doc}
%license COPYING COPYING.LGPL
%{_datadir}/license/cryptsetup-libs
%{_libdir}/libcryptsetup.so.*
