Summary: Library for error values used by GnuPG components
Name: libgpg-error
Version: 1.19
Release: 3
URL: ftp://ftp.gnupg.org/gcrypt/libgpg-error/
Source0: ftp://ftp.gnupg.org/gcrypt/libgpg-error/%{name}-%{version}.tar.bz2
Source1: ftp://ftp.gnupg.org/gcrypt/libgpg-error/%{name}-%{version}.tar.bz2.sig
Source1001: %{name}.manifest
Patch1: libgpg-error-1.19-multilib.patch
Group: System Environment/Libraries
License: LGPLv2+
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: gawk, gettext, autoconf, automake, gettext-devel, libtool
BuildRequires: texinfo
%if 0%{?fedora} > 13
BuildRequires: gettext-autopoint
%endif
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description
This is a library that defines common error values for all GnuPG
components.  Among these are GPG, GPGSM, GPGME, GPG-Agent, libgcrypt,
pinentry, SmartCard Daemon and possibly more in the future.

%package devel
Summary: Development files for the %{name} package
Group: Development/Libraries
Requires: %{name}%{?_isa} = %{version}-%{release}
Requires(pre): /sbin/install-info
Requires(post): /sbin/install-info

%description devel
This is a library that defines common error values for all GnuPG
components.  Among these are GPG, GPGSM, GPGME, GPG-Agent, libgcrypt,
pinentry, SmartCard Daemon and possibly more in the future. This package
contains files necessary to develop applications using libgpg-error.

%prep
%setup -q
%patch1 -p1 -b .multilib
# The config script already suppresses the -L if it's /usr/lib, so cheat and
# set it to a value which we know will be suppressed.
sed -i -e 's|^libdir=@libdir@$|libdir=@exec_prefix@/lib|g;s|@GPG_ERROR_CONFIG_HOST@|none|g' src/gpg-error-config.in

# Modify configure to drop rpath for /usr/lib64
sed -i -e 's|sys_lib_dlsearch_path_spec="/lib /usr/lib|sys_lib_dlsearch_path_spec="/lib /usr/lib %{_libdir}|g' configure

%build
cp %{SOURCE1001} .
%configure --disable-static --disable-rpath --disable-languages --disable-nls
make %{?_smp_mflags}

%install
rm -fr $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
rm -f $RPM_BUILD_ROOT/%{_libdir}/*.la
rm -f $RPM_BUILD_ROOT/%{_infodir}/dir

mkdir -p %{buildroot}%{_datadir}/license
cat COPYING > %{buildroot}%{_datadir}/license/libgpg-error
cat COPYING.LIB >> %{buildroot}%{_datadir}/license/libgpg-error

%clean
rm -fr $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%post devel
[ -f %{_infodir}/gpgrt.info.gz ] && \
    /sbin/install-info %{_infodir}/gpgrt.info.gz %{_infodir}/dir
exit 0

%preun devel
if [ $1 = 0 -a -f %{_infodir}/gpgrt.info.gz ]; then
    /sbin/install-info --delete %{_infodir}/gpgrt.info.gz %{_infodir}/dir
fi
exit 0

%files
%defattr(-,root,root)
%{!?_licensedir:%global license %%doc}
%license COPYING COPYING.LIB
%{_datadir}/license/libgpg-error
%doc AUTHORS README NEWS ChangeLog
%{_bindir}/gpg-error
%{_libdir}/libgpg-error.so.0*
%manifest %{name}.manifest

%files devel
%defattr(-,root,root)
%{_bindir}/gpg-error-config
%{_libdir}/libgpg-error.so
%{_includedir}/gpg-error.h
%{_datadir}/aclocal/gpg-error.m4
%{_infodir}/gpgrt.info*
%{_mandir}/man1/gpg-error-config.*
