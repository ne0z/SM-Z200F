# Combined cross-*-glibc specfile
%ifarch x86_64
%define x64 x64
%endif

Name: 	cross-aarch64-glibc

# crossbuild / accelerator section
# \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
%define crossbuild 0
%if "%{name}" != "glibc"
# this is the ix86 -> arm cross compiler (cross-*-glibc)
#
# cross arch retrieval
%define crossarch %{lua: x=string.gsub(rpm.expand("%{name}"), "cross%-(.*)%-glibc", "%1"); print(x)}
# We set requires/provides by hand and disable post-build-checks.
# Captain Trunk: Sledge, you cannot disarm that nuclear bomb!
# Sledge Hammer: Trust me, I know what I'm doing.
AutoReqProv: 0
AutoReq: false
#!BuildIgnore: rpmlint-Moblin rpmlint-mini post-build-checks
# cross platform
%if "%{crossarch}" == "armv7l"
%define cross_glibc_target_platform %{crossarch}-tizen-linux-gnueabi
%else
%define cross_glibc_target_platform %{crossarch}-tizen-linux-gnu
%endif
# glibc_target_platform holds the host (executing the compiler)
# cross_glibc_target_platform holds the target (for which the compiler is producing binaries)
# strip of 'foreign arch' symbols fails
%define __strip %{cross_glibc_target_platform}-strip
%define __objdump %{cross_glibc_target_platform}-objdump
# sysroot for cross-compiler
%define crosssysroot /opt/cross/%{cross_glibc_target_platform}/sys-root
# flag
%define crossbuild 1
# macros in buildrequires is hard to expand for the scheduler (e.g. crossarch) which would make this easier.
#BuildRequires: cross-%{crossarch}-kernel-headers cross-%{crossarch}-binutils cross-%{crossarch}-gcc
# Fixme: find way to make this without listing every package
%if "%{name}" == "cross-armv7l-glibc"
BuildRequires: cross-armv7l-kernel-headers cross-armv7l-binutils cross-armv7l-gcc
Provides: cross-armv7l-glibc cross-armv7l-glibc-devel cross-armv7l-glibc-headers
%endif
%if "%{name}" == "cross-aarch64-glibc"
BuildRequires: cross-aarch64-kernel-headers cross-aarch64-binutils cross-aarch64-gcc
Provides: cross-aarch64-glibc cross-aarch64-glibc-devel cross-aarch64-glibc-headers
%endif
# single target atm.
ExclusiveArch: %ix86 x86_64
%endif
# /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\
# end crossbuild / accelerator section

%define linaro_release 2014.11
%define release_prefix %{linaro_release}

Summary: a variant of the GNU C Library targetting embedded systems.
Version: 2.20
Release: %{linaro_release}
# GPLv2+ is used in a bunch of programs, LGPLv2+ is used for libraries.
# Things that are linked directly into dynamically linked programs
# and shared libraries (e.g. crt files, lib*_nonshared.a) have an additional
# exception which allows linking it into any kind of programs or shared
# libraries without restrictions.
License: LGPLv2+ and LGPLv2+ with exceptions and GPLv2+
Group: System/Libraries
URL: http://gcc.gnu.org
Source0: glibc-linaro-%{version}-%{linaro_release}.tar.xz
BuildRequires: elfutils >= 0.72

%define enablekernel 3.10

%description
Embedded GLIBC (EGLIBC) is a variant of the GNU C Library (GLIBC)  that is designed to work well on embedded systems. EGLIBC strives to be source and binary compatible with GLIBC. EGLIBC's goals include reduced footprint, configurable components, better support for cross-compilation and cross-testing.

%prep
%setup -q -n glibc-linaro-%{version}-%{linaro_release}

touch `find . -name configure`
touch locale/programs/*-kw.h

%build
PATH=/opt/cross/bin:$PATH
# Fail to build with system flags.syslog.o: inline failed.
BuildFlags="-O2 -g"
EnableKernel="--enable-kernel=%{enablekernel}"
AddOns=`echo */configure | sed -e 's!/configure!!g;s!\(linuxthreads\|nptl\|rtkaio\|powerpc-cpu\)\( \|$\)!!g;s! \+$!!;s! !,!g;s!^!,!;/^,\*$/d'`

builddir=build-%{crossarch}-linuxnptl
rm -rf $builddir
mkdir $builddir ; cd $builddir

../configure CFLAGS="$BuildFlags" \
	--build %{_target_platform} \
	--host %{cross_glibc_target_platform} \
	--target %{cross_glibc_target_platform} \
	--prefix=%{_prefix} \
	--bindir=%{_bindir} \
	--libdir=%{_prefix}/lib \
	--libexecdir=%{_libexecdir} \
	--enable-add-ons=$AddOns --without-cvs $EnableKernel \
	--with-headers=%{crosssysroot}/usr/include \
	--with-tls --with-__thread  \
	--disable-multi-arch \
	--enable-profile \
	--without-selinux \
	--enable-obsolete-rpc \
	--disable-force-install libc_cv_ssp=no \
	libc_cv_forced_unwind=yes \
	--enable-stackguard-randomization

make %{?_smp_mflags} -r PARALLELMFLAGS=-s

%install
PATH=/opt/cross/bin:$PATH

rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT

make install_root=$RPM_BUILD_ROOT%{crosssysroot} install -C build-%{crossarch}-linuxnptl PARALLELMFLAGS=-s

cp -a bits/stdio-lock.h $RPM_BUILD_ROOT%{crosssysroot}%{_prefix}/include/bits/stdio-lock.h
touch $RPM_BUILD_ROOT%{crosssysroot}%{_prefix}/include/gnu/stubs.h

PATH=/opt/cross/%{cross_glibc_target_platform}/bin:$PATH

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/opt/cross

