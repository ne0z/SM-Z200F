Name:       dnsmasq
Summary:    dnsmasq, DNS forwarder.
Version:    2.75
Release:    1
Group:      TO_BE/FILLED_IN
License:    GPL
URL: http://www.thekelleys.org.uk/dnsmasq
Source0:    %{name}-%{version}.tar.gz
BuildRequires: cmake
BuildRequires: pkgconfig(dbus-1)

%description
Dnsmasq is lightweight, easy to configure DNS forwarder and DHCP server. It
is designed to provide DNS and, optionally, DHCP, to a small network. It can
serve the names of local machines which are not in the global DNS. The DHCP
server integrates with the DNS server and allows machines with DHCP-allocated
addresses to appear in the DNS with names configured either in each host or
in a central configuration file. Dnsmasq supports static and dynamic DHCP
leases and BOOTP for network booting of diskless machines.

%prep
%setup -q

%build
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}
make %{?jobs:-j%jobs}

%post
mkdir -p /opt/var/lib/misc

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}/usr/share/license
cp COPYING %{buildroot}/usr/share/license/dnsmasq

%files
%manifest dnsmasq.manifest
%{_bindir}/dnsmasq
/usr/share/license/dnsmasq
