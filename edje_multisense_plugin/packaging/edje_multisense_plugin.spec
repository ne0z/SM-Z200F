#sbs-git:slp/pkgs/e/edje_multisense_plugin
Name:       edje_multisense_plugin
Summary:    multisense plugin of edje
Version:    0.1.21
Release:    1
Group:      System/Libraries
License:    BSD 2-Clause
Source0:    %{name}-%{version}.tar.gz
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(mm-sound)
BuildRequires:  pkgconfig(remix)

%description
EDJE & libremix Plugins for TIZEN sound/tone play
Multise plugin for use with libedje
It includes sound, haptic, and vibration.

%prep
%setup -q

%build
./autogen.sh
make %{?jobs:-j%jobs}

%install
if [ -d %{buildroot} ]; then rm -rf %{buildroot}; fi
make install DESTDIR=%{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp %{_builddir}/%{buildsubdir}/COPYING %{buildroot}/usr/share/license/%{name}

%clean
if [ -d %{buildroot} ]; then rm -rf %{buildroot}; fi
rm -f edje-multisense-plugin*.tar.bz2 edje-multisense-plugin-*.tar.bz2.cdbs-config_list

%files
%defattr(-,root,root,-)
%doc AUTHORS COPYING ChangeLog INSTALL NEWS README
%{_libdir}/remix/*
%manifest %{name}.manifest
/usr/share/license/%{name}
