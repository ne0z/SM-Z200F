Name:       mmfw-sysconf
Summary:    Multimedia Framework system configuration package
Version:    0.1.428
Release:    0
VCS:        framework/multimedia/mmfw-sysconf#mmfw-sysconf_0.1.110-0-144-g25720f9f7daad9a897662eb6059d71d958cc3f06
Group:      System Environment/Multimedia
License:    LGPL-2.1+ and Apache-2.0
Source0:    mmfw-sysconf-%{version}.tar.gz

%description
Multimedia Framework system configuration package

%ifarch %{arm}
%package cleansdk-e4x12
Summary: Multimedia Framework system configuration package for cleansdk-e4x12
Group: System Environment/Multimedia
License:    LGPL-2.1+ and Apache-2.0

%description cleansdk-e4x12
Multimedia Framework system configuration package for cleansdk-e4x12

%package e3250
Summary: Multimedia Framework system configuration package for e3250
Group: System Environment/Multimedia
License:    LGPL-2.1+ and Apache-2.0

%description e3250
Multimedia Framework system configuration package for e3250

%package e3475
Summary: Multimedia Framework system configuration package for e3475
Group: System Environment/Multimedia
License:    LGPL-2.1+ and Apache-2.0

%description e3475
Multimedia Framework system configuration package for e3475

%package sc7727s
Summary: Multimedia Framework system configuration package for sc7727s
Group: System Environment/Multimedia
License:    LGPL-2.1+ and Apache-2.0

%description sc7727s
Multimedia Framework system configuration package for sc7727s

%package sc7727se
Summary: Multimedia Framework system configuration package for sc7727se
Group: System Environment/Multimedia
License:    LGPL-2.1+ and Apache-2.0

%description sc7727se
Multimedia Framework system configuration package for sc7727se

%package sc7730s
Summary: Multimedia Framework system configuration package for sc7730s
Group: System Environment/Multimedia
License:    LGPL-2.1+ and Apache-2.0

%description sc7730s
Multimedia Framework system configuration package for sc7730s

%package sc9830
Summary: Multimedia Framework system configuration package for sc9830
Group: System Environment/Multimedia
License:    LGPL-2.1+ and Apache-2.0

%description sc9830
Multimedia Framework system configuration package for sc9830

%package e4x12
Summary: Multimedia Framework system configuration package for e4x12
Group: TO_BE/FILLED_IN
License:    Apache-2.0

%description e4x12
Multimedia Framework system configuration package for e4x12

%package e4212
Summary: Multimedia Framework system configuration package for e4212
Group: TO_BE/FILLED_IN
License:    Apache-2.0

%description e4212
Multimedia Framework system configuration package for e4212

%package msm8x74
Summary: Multimedia Framework system configuration package for msm8x74
Group: System Environment/Multimedia
License:    LGPL-2.1+ and Apache-2.0

%description msm8x74
Multimedia Framework system configuration package for msm8x74

%package msm8974pro
Summary: Multimedia Framework system configuration package for msm8974pro
Group: System Environment/Multimedia
License:    LGPL-2.1+ and Apache-2.0

%description msm8974pro
Multimedia Framework system configuration package for msm8974pro

%package msm8x16
Summary: Multimedia Framework system configuration package for msm8x16
Group: TO_BE/FILLED_IN
License:    Apache-2.0

%description msm8x16
Multimedia Framework system configuration package for msm8x16

%else
%package simulator
Summary: Multimedia Framework system configuration package for simulator
Group: System Environment/Multimedia
License:    LGPL-2.1+ and Apache-2.0

%description simulator
Multimedia Framework system configuration package for simulator
%endif

%prep
%setup -q -n %{name}-%{version}

%build

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/etc
mkdir -p %{buildroot}/usr
mkdir -p %{buildroot}/usr/share/license
%ifarch %{arm}

mkdir -p %{buildroot}/mmfw-sysconf-e4x12
cp -arf mmfw-sysconf-e4x12/* %{buildroot}/mmfw-sysconf-e4x12
mkdir -p %{buildroot}/mmfw-sysconf-e4x12/usr/share/license
cp LICENSE.APLv2.0 %{buildroot}/mmfw-sysconf-e4x12/usr/share/license/mmfw-sysconf-e4x12
cat LICENSE.LGPLv2.1 >> %{buildroot}/mmfw-sysconf-e4x12/usr/share/license/mmfw-sysconf-e4x12

mkdir -p %{buildroot}/mmfw-sysconf-e4212
cp -arf mmfw-sysconf-e4212/* %{buildroot}/mmfw-sysconf-e4212
mkdir -p %{buildroot}/mmfw-sysconf-e4212/usr/share/license
cp LICENSE.APLv2.0 %{buildroot}/mmfw-sysconf-e4212/usr/share/license/mmfw-sysconf-e4212

cp LICENSE.APLv2.0 %{buildroot}/usr/share/license/%{name}-e3250
cat LICENSE.LGPLv2.1 >> %{buildroot}/usr/share/license/%{name}-e3250

cp LICENSE.APLv2.0 %{buildroot}/usr/share/license/%{name}-e3475
cat LICENSE.LGPLv2.1 >> %{buildroot}/usr/share/license/%{name}-e3475

cp LICENSE.APLv2.0 %{buildroot}/usr/share/license/%{name}-sc7727s
cat LICENSE.LGPLv2.1 >> %{buildroot}/usr/share/license/%{name}-sc7727s

cp LICENSE.APLv2.0 %{buildroot}/usr/share/license/%{name}-sc7730s
cat LICENSE.LGPLv2.1 >> %{buildroot}/usr/share/license/%{name}-sc7730s

cp LICENSE.APLv2.0 %{buildroot}/usr/share/license/%{name}-sc9830
cat LICENSE.LGPLv2.1 >> %{buildroot}/usr/share/license/%{name}-sc9830

cp LICENSE.APLv2.0 %{buildroot}/usr/share/license/%{name}-msm8x74
cat LICENSE.LGPLv2.1 >> %{buildroot}/usr/share/license/%{name}-msm8x74

for f in `find mmfw-sysconf-msm8x74/etc/pulse -name "*.in"`; do \
	echo $f;\
	cat $f > ${f%.in}; \
	# if this packge is built for JPN, we would use the below.
	%if "%{?sec_build_conf_tizen_product_name}" == "redwood8974om"
		sed -i -e "s#@DCM@##g" ${f%.in}; \
		sed -i -e "s#@ORG@#\#\#\# #g" ${f%.in}; \
	%else
		sed -i -e "s#@DCM@#\#\#\# #g" ${f%.in}; \
		sed -i -e "s#@ORG@##g" ${f%.in}; \
	%endif
done

cp LICENSE.APLv2.0 %{buildroot}/usr/share/license/%{name}-msm8974pro
cat LICENSE.LGPLv2.1 >> %{buildroot}/usr/share/license/%{name}-msm8974pro

for f in `find mmfw-sysconf-msm8974pro/etc/pulse -name "*.in"`; do \
	echo $f;\
	cat $f > ${f%.in}; \
	# if this packge is built for JPN, we would use the below.
	%if "%{?sec_build_conf_tizen_product_name}" == "redwood8974om"
		sed -i -e "s#@DCM@##g" ${f%.in}; \
		sed -i -e "s#@ORG@#\#\#\# #g" ${f%.in}; \
	%else
		sed -i -e "s#@DCM@#\#\#\# #g" ${f%.in}; \
		sed -i -e "s#@ORG@##g" ${f%.in}; \
	%endif
done

cp LICENSE.APLv2.0 %{buildroot}/usr/share/license/%{name}-cleansdk-e4x12
cat LICENSE.LGPLv2.1 >> %{buildroot}/usr/share/license/%{name}-cleansdk-e4x12

%else
cp LICENSE.APLv2.0 %{buildroot}/usr/share/license/mmfw-sysconf-simulator
cat LICENSE.LGPLv2.1 >> %{buildroot}/usr/share/license/mmfw-sysconf-simulator
%endif


%ifarch %{arm}
mkdir -p %{buildroot}/mmfw-sysconf-cleansdk-e4x12
cp -arf mmfw-sysconf-cleansdk-e4x12/* %{buildroot}/mmfw-sysconf-cleansdk-e4x12

mkdir -p %{buildroot}/mmfw-sysconf-e3250
cp -arf mmfw-sysconf-e3250/* %{buildroot}/mmfw-sysconf-e3250

mkdir -p %{buildroot}/mmfw-sysconf-e3475
cp -arf mmfw-sysconf-e3475/* %{buildroot}/mmfw-sysconf-e3475

mkdir -p %{buildroot}/mmfw-sysconf-sc7727s
cp -arf mmfw-sysconf-sc7727s/* %{buildroot}/mmfw-sysconf-sc7727s

mkdir -p %{buildroot}/mmfw-sysconf-sc7727se
cp -arf mmfw-sysconf-sc7727se/* %{buildroot}/mmfw-sysconf-sc7727se

mkdir -p %{buildroot}/mmfw-sysconf-sc7730s
cp -arf mmfw-sysconf-sc7730s/* %{buildroot}/mmfw-sysconf-sc7730s

mkdir -p %{buildroot}/mmfw-sysconf-sc9830
cp -arf mmfw-sysconf-sc9830/* %{buildroot}/mmfw-sysconf-sc9830

mkdir -p %{buildroot}/mmfw-sysconf-msm8x74
cp -arf mmfw-sysconf-msm8x74/* %{buildroot}/mmfw-sysconf-msm8x74

mkdir -p %{buildroot}/mmfw-sysconf-msm8x16
cp -arf mmfw-sysconf-msm8x16/* %{buildroot}/mmfw-sysconf-msm8x16

mkdir -p %{buildroot}/mmfw-sysconf-msm8974pro
cp -arf mmfw-sysconf-msm8974pro/* %{buildroot}/mmfw-sysconf-msm8974pro

mkdir -p %{buildroot}/mmfw-sysconf-msm8x16/usr/share/license
cp LICENSE.APLv2.0 %{buildroot}/mmfw-sysconf-msm8x16/usr/share/license/mmfw-sysconf-msm8x16
cat LICENSE.LGPLv2.1 >> %{buildroot}/mmfw-sysconf-msm8x16/usr/share/license/mmfw-sysconf-msm8x16

%else
mkdir -p %{buildroot}/mmfw-sysconf-simulator
cp -arf mmfw-sysconf-simulator/* %{buildroot}/mmfw-sysconf-simulator
%endif

%ifarch %{arm}
%post cleansdk-e4x12
cp -arf /mmfw-sysconf-cleansdk-e4x12/* /
rm -rf /mmfw-sysconf-cleansdk-e4x12
%post e3250
cp -arf /mmfw-sysconf-e3250/* /
rm -rf /mmfw-sysconf-e3250
%post e3475
cp -arf /mmfw-sysconf-e3475/* /
rm -rf /mmfw-sysconf-e3475
%post e4x12
cp -arf /mmfw-sysconf-e4x12/* /
rm -rf /mmfw-sysconf-e4x12
%post e4212
cp -arf /mmfw-sysconf-e4212/* /
rm -rf /mmfw-sysconf-e4212
%post sc7727s
cp -arf /mmfw-sysconf-sc7727s/* /
rm -rf /mmfw-sysconf-sc7727s
%post sc7727se
cp -arf /mmfw-sysconf-sc7727se/* /
rm -rf /mmfw-sysconf-sc7727se
%post sc7730s
cp -arf /mmfw-sysconf-sc7730s/* /
rm -rf /mmfw-sysconf-sc7730s
%post sc9830
cp -arf /mmfw-sysconf-sc9830/* /
rm -rf /mmfw-sysconf-sc9830
%post msm8x74
cp -arf /mmfw-sysconf-msm8x74/* /
rm -rf /mmfw-sysconf-msm8x74
%post msm8974pro
cp -arf /mmfw-sysconf-msm8974pro/* /
rm -rf /mmfw-sysconf-msm8974pro
%post msm8x16
cp -arf /mmfw-sysconf-msm8x16/* /
rm -rf /mmfw-sysconf-msm8x16
%else
%post simulator
cp -arf /mmfw-sysconf-simulator/* /
rm -rf /mmfw-sysconf-simulator
%endif

%ifarch %{arm}
%files cleansdk-e4x12
%manifest mmfw-sysconf-cleansdk-e4x12.manifest
%defattr(-,root,root,-)
/mmfw-sysconf-cleansdk-e4x12/etc/asound.conf
/mmfw-sysconf-cleansdk-e4x12/etc/pulse/*
/mmfw-sysconf-cleansdk-e4x12/usr/etc/*.ini
/mmfw-sysconf-cleansdk-e4x12/usr/etc/gst-openmax.conf
/mmfw-sysconf-cleansdk-e4x12/usr/share/pulseaudio/alsa-mixer/paths/*.conf
/mmfw-sysconf-cleansdk-e4x12/usr/share/pulseaudio/alsa-mixer/paths/*.common
/mmfw-sysconf-cleansdk-e4x12/usr/share/pulseaudio/alsa-mixer/profile-sets/*.conf
/usr/share/license/*

%files e3250
%manifest mmfw-sysconf-e3250.manifest
%defattr(-,root,root,-)
/mmfw-sysconf-e3250/etc/asound.conf
/mmfw-sysconf-e3250/etc/pulse/*
/mmfw-sysconf-e3250/etc/profile.d/*
/mmfw-sysconf-e3250/usr/etc/*.ini
/mmfw-sysconf-e3250/usr/etc/*.txt
/mmfw-sysconf-e3250/usr/etc/gst-openmax.conf
/mmfw-sysconf-e3250/usr/share/pulseaudio/alsa-mixer/paths/*.conf
/mmfw-sysconf-e3250/usr/share/pulseaudio/alsa-mixer/paths/*.common
/mmfw-sysconf-e3250/usr/share/pulseaudio/alsa-mixer/profile-sets/*.conf
/mmfw-sysconf-e3250/opt/system/*
/usr/share/license/*

%files e3475
%manifest mmfw-sysconf-e3475.manifest
%defattr(-,root,root,-)
/mmfw-sysconf-e3475/etc/asound.conf
/mmfw-sysconf-e3475/etc/pulse/*
/mmfw-sysconf-e3475/etc/profile.d/*
/mmfw-sysconf-e3475/usr/etc/*.ini
/mmfw-sysconf-e3475/usr/etc/*.txt
/mmfw-sysconf-e3475/usr/etc/gst-openmax.conf
/mmfw-sysconf-e3475/usr/share/pulseaudio/alsa-mixer/paths/*.conf
/mmfw-sysconf-e3475/usr/share/pulseaudio/alsa-mixer/paths/*.common
/mmfw-sysconf-e3475/usr/share/pulseaudio/alsa-mixer/profile-sets/*.conf
/mmfw-sysconf-e3475/opt/system/*
/usr/share/license/*

%files e4x12
%manifest mmfw-sysconf-e4x12.manifest
%defattr(-,root,root,-)
/mmfw-sysconf-e4x12/etc/asound.conf
/mmfw-sysconf-e4x12/etc/pulse/*
/mmfw-sysconf-e4x12/etc/profile.d/*
/mmfw-sysconf-e4x12/usr/etc/*.ini
/mmfw-sysconf-e4x12/usr/etc/*.txt
/mmfw-sysconf-e4x12/usr/etc/gst-openmax.conf
/mmfw-sysconf-e4x12/usr/share/pulseaudio/alsa-mixer/paths/*.conf
/mmfw-sysconf-e4x12/usr/share/pulseaudio/alsa-mixer/paths/*.common
/mmfw-sysconf-e4x12/usr/share/pulseaudio/alsa-mixer/profile-sets/*.conf
/mmfw-sysconf-e4x12/opt/system/*
/mmfw-sysconf-e4x12/usr/share/license/mmfw-sysconf-e4x12

%files e4212
%manifest mmfw-sysconf-e4212.manifest
%defattr(-,root,root,-)
/mmfw-sysconf-e4212/etc/asound.conf
/mmfw-sysconf-e4212/etc/pulse/*
/mmfw-sysconf-e4212/etc/profile.d/*
/mmfw-sysconf-e4212/usr/etc/*.ini
/mmfw-sysconf-e4212/usr/etc/*.txt
/mmfw-sysconf-e4212/usr/etc/gst-openmax.conf
/mmfw-sysconf-e4212/usr/share/pulseaudio/alsa-mixer/paths/*.conf
/mmfw-sysconf-e4212/usr/share/pulseaudio/alsa-mixer/paths/*.common
/mmfw-sysconf-e4212/usr/share/pulseaudio/alsa-mixer/profile-sets/*.conf
/mmfw-sysconf-e4212/opt/system/*
/mmfw-sysconf-e4212/usr/share/license/mmfw-sysconf-e4212

%files sc7727s
%manifest mmfw-sysconf-sc7727s.manifest
%defattr(-,root,root,-)
/mmfw-sysconf-sc7727s/etc/asound.conf
/mmfw-sysconf-sc7727s/etc/pulse/*
/mmfw-sysconf-sc7727s/etc/profile.d/*
/mmfw-sysconf-sc7727s/usr/etc/*.ini
/mmfw-sysconf-sc7727s/usr/etc/miccalib.txt
/mmfw-sysconf-sc7727s/usr/etc/gst-openmax.conf
/mmfw-sysconf-sc7727s/usr/etc/codec_pga.xml
/mmfw-sysconf-sc7727s/usr/etc/audio_hw.xml
/mmfw-sysconf-sc7727s/usr/share/pulseaudio/alsa-mixer/paths/*.conf
/mmfw-sysconf-sc7727s/usr/share/pulseaudio/alsa-mixer/paths/*.common
/mmfw-sysconf-sc7727s/usr/share/pulseaudio/alsa-mixer/profile-sets/*.conf
/mmfw-sysconf-sc7727s/opt/system/*
/usr/share/license/*

%files sc7727se
%manifest mmfw-sysconf-sc7727se.manifest
%defattr(-,root,root,-)
/mmfw-sysconf-sc7727se/etc/asound.conf
/mmfw-sysconf-sc7727se/etc/pulse/*
/mmfw-sysconf-sc7727se/etc/profile.d/*
/mmfw-sysconf-sc7727se/usr/etc/*.ini
/mmfw-sysconf-sc7727se/usr/etc/miccalib.txt
/mmfw-sysconf-sc7727se/usr/etc/gst-openmax.conf
/mmfw-sysconf-sc7727se/usr/etc/codec_pga.xml
/mmfw-sysconf-sc7727se/usr/etc/audio_hw.xml
/mmfw-sysconf-sc7727se/usr/share/pulseaudio/alsa-mixer/paths/*.conf
/mmfw-sysconf-sc7727se/usr/share/pulseaudio/alsa-mixer/paths/*.common
/mmfw-sysconf-sc7727se/usr/share/pulseaudio/alsa-mixer/profile-sets/*.conf
/mmfw-sysconf-sc7727se/opt/system/*
/usr/share/license/*

%files sc7730s
%manifest mmfw-sysconf-sc7730s.manifest
%defattr(-,root,root,-)
/mmfw-sysconf-sc7730s/etc/asound.conf
/mmfw-sysconf-sc7730s/etc/pulse/*
/mmfw-sysconf-sc7730s/etc/profile.d/*
/mmfw-sysconf-sc7730s/usr/etc/*.ini
/mmfw-sysconf-sc7730s/usr/etc/miccalib.txt
/mmfw-sysconf-sc7730s/usr/etc/aeqcoe.txt
/mmfw-sysconf-sc7730s/usr/etc/gst-openmax.conf
/mmfw-sysconf-sc7730s/usr/etc/codec_pga.xml
/mmfw-sysconf-sc7730s/usr/etc/audio_hw.xml
/mmfw-sysconf-sc7730s/usr/share/pulseaudio/alsa-mixer/paths/*.conf
/mmfw-sysconf-sc7730s/usr/share/pulseaudio/alsa-mixer/paths/*.common
/mmfw-sysconf-sc7730s/usr/share/pulseaudio/alsa-mixer/profile-sets/*.conf
/mmfw-sysconf-sc7730s/opt/system/*
/usr/share/license/*

%files sc9830
%manifest mmfw-sysconf-sc9830.manifest
%defattr(-,root,root,-)
/mmfw-sysconf-sc9830/etc/asound.conf
/mmfw-sysconf-sc9830/etc/pulse/*
/mmfw-sysconf-sc9830/etc/profile.d/*
/mmfw-sysconf-sc9830/usr/etc/*.ini
/mmfw-sysconf-sc9830/usr/etc/miccalib.txt
/mmfw-sysconf-sc9830/usr/etc/gst-openmax.conf
/mmfw-sysconf-sc9830/usr/etc/codec_pga.xml
/mmfw-sysconf-sc9830/usr/etc/audio_hw.xml
/mmfw-sysconf-sc9830/usr/share/pulseaudio/alsa-mixer/paths/*.conf
/mmfw-sysconf-sc9830/usr/share/pulseaudio/alsa-mixer/paths/*.common
/mmfw-sysconf-sc9830/usr/share/pulseaudio/alsa-mixer/profile-sets/*.conf
/mmfw-sysconf-sc9830/opt/system/*
/usr/share/license/*

%files msm8x74
%manifest mmfw-sysconf-msm8x74.manifest
%defattr(-,root,root,-)
/mmfw-sysconf-msm8x74/etc/asound.conf
/mmfw-sysconf-msm8x74/etc/pulse/client.conf
/mmfw-sysconf-msm8x74/etc/pulse/daemon.conf
/mmfw-sysconf-msm8x74/etc/pulse/system.pa
/mmfw-sysconf-msm8x74/etc/pulse/default.pa
%exclude /mmfw-sysconf-msm8x74/etc/pulse/system.pa.in
/mmfw-sysconf-msm8x74/etc/profile.d/*
/mmfw-sysconf-msm8x74/usr/etc/*.ini
/mmfw-sysconf-msm8x74/usr/etc/gst-openmax.conf
/mmfw-sysconf-msm8x74/usr/etc/*.txt
/mmfw-sysconf-msm8x74/usr/share/pulseaudio/alsa-mixer/paths/*.conf
/mmfw-sysconf-msm8x74/usr/share/pulseaudio/alsa-mixer/paths/*.common
/mmfw-sysconf-msm8x74/usr/share/pulseaudio/alsa-mixer/profile-sets/*.conf
/mmfw-sysconf-msm8x74/opt/system/*
/usr/share/license/*

%files msm8974pro
%manifest mmfw-sysconf-msm8974pro.manifest
%defattr(-,root,root,-)
/mmfw-sysconf-msm8974pro/etc/asound.conf
/mmfw-sysconf-msm8974pro/etc/pulse/client.conf
/mmfw-sysconf-msm8974pro/etc/pulse/daemon.conf
/mmfw-sysconf-msm8974pro/etc/pulse/system.pa
/mmfw-sysconf-msm8974pro/etc/pulse/default.pa
%exclude /mmfw-sysconf-msm8974pro/etc/pulse/system.pa.in
/mmfw-sysconf-msm8974pro/etc/profile.d/*
/mmfw-sysconf-msm8974pro/usr/etc/*.ini
/mmfw-sysconf-msm8974pro/usr/etc/gst-openmax.conf
/mmfw-sysconf-msm8974pro/usr/etc/*.txt
/mmfw-sysconf-msm8974pro/usr/share/pulseaudio/alsa-mixer/paths/*.conf
/mmfw-sysconf-msm8974pro/usr/share/pulseaudio/alsa-mixer/paths/*.common
/mmfw-sysconf-msm8974pro/usr/share/pulseaudio/alsa-mixer/profile-sets/*.conf
/mmfw-sysconf-msm8974pro/opt/system/*
/usr/share/license/*

%files msm8x16
%manifest mmfw-sysconf-msm8x16.manifest
%defattr(-,root,root,-)
/mmfw-sysconf-msm8x16/etc/asound.conf
/mmfw-sysconf-msm8x16/etc/pulse/client.conf
/mmfw-sysconf-msm8x16/etc/pulse/daemon.conf
/mmfw-sysconf-msm8x16/etc/pulse/system.pa
/mmfw-sysconf-msm8x16/etc/pulse/default.pa
/mmfw-sysconf-msm8x16/etc/profile.d/*
/mmfw-sysconf-msm8x16/usr/etc/*.ini
/mmfw-sysconf-msm8x16/usr/etc/*.txt
/mmfw-sysconf-msm8x16/usr/etc/miccalib.txt
/mmfw-sysconf-msm8x16/usr/etc/gst-openmax.conf
/mmfw-sysconf-msm8x16/usr/etc/codec_pga.xml
/mmfw-sysconf-msm8x16/usr/etc/audio_hw.xml
/mmfw-sysconf-msm8x16/usr/share/pulseaudio/alsa-mixer/paths/*.conf
/mmfw-sysconf-msm8x16/usr/share/pulseaudio/alsa-mixer/paths/*.common
/mmfw-sysconf-msm8x16/usr/share/pulseaudio/alsa-mixer/profile-sets/*.conf
/mmfw-sysconf-msm8x16/opt/system/*
/mmfw-sysconf-msm8x16/usr/share/license/mmfw-sysconf-msm8x16

%else
%files simulator
%manifest mmfw-sysconf-simulator.manifest
%defattr(-,root,root,-)
/mmfw-sysconf-simulator/etc/asound.conf
/mmfw-sysconf-simulator/etc/pulse/*
/mmfw-sysconf-simulator/usr/etc/*.ini
/mmfw-sysconf-simulator/usr/etc/gst-openmax.conf
/mmfw-sysconf-simulator/usr/share/pulseaudio/alsa-mixer/paths/*.conf
/mmfw-sysconf-simulator/usr/share/pulseaudio/alsa-mixer/paths/*.common
/mmfw-sysconf-simulator/usr/share/pulseaudio/alsa-mixer/profile-sets/*.conf
/usr/share/license/mmfw-sysconf-simulator
%endif

