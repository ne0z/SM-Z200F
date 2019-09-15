Name:             pulseaudio-modules-tizen
Summary:          Pulseaudio modules for Tizen
Version:          4.0.14
Release:          0
Group:            Multimedia/Audio
License:          LGPL-2.1+
Source0:          %{name}-%{version}.tar.gz
BuildRequires:    libtool-ltdl-devel
BuildRequires:    libtool
BuildRequires:    intltool
BuildRequires:    pkgconfig(dbus-1)
BuildRequires:    pkgconfig(iniparser)
BuildRequires:    pkgconfig(json-c)
BuildRequires:    pkgconfig(vconf)
BuildRequires:    pkgconfig(audio-headers)
BuildRequires:    pkgconfig(libpulse)
BuildRequires:    pkgconfig(pulsecore)
BuildRequires:    pkgconfig(alsa)
BuildRequires:    pulseaudio
BuildRequires:    m4
BuildRequires:    pkgconfig(dlog)
BuildRequires:    sec-product-features
Requires(post):   /sbin/ldconfig
Requires(postun): /sbin/ldconfig
 
%description
This package contains pulseaudio modules for tizen audio system.
 
%prep
%setup -q
 
%build
%if 0%{?sec_build_binary_debug_enable}
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"
%endif

%if 0%{?sec_product_feature_profile_wearable}
export CFLAGS+=" -DPROFILE_WEARABLE"
%endif

%if 0%{?sec_product_feature_mmfw_audio_uhqa}
export CFLAGS+=" -DSUPPORT_UHQA"
%endif
%if 0%{?sec_product_feature_mmfw_audio_call_noise_reduction}
export CFLAGS+=" -DSUPPORT_NOISE_REDUCTION"
%endif
%if 0%{?sec_product_feature_mmfw_audio_dock}
export CFLAGS+=" -DSUPPORT_DOCK"
%endif
%if 0%{?sec_product_feature_mmfw_audio_hdmi}
export CFLAGS+=" -DSUPPORT_HDMI"
%endif

%if 0%{?sec_product_feature_bt_a2dp_sink_enable}
export CFLAGS+=" -DSUPPORT_A2DP_SINK"
%endif

%if "%{sec_product_feature_system_chipset_name}" == "sc9830i"
export CFLAGS+=" -DUSE_FM_REC_DEDICATED_DEVICE"
%endif

export CFLAGS+=" -DFM_BT"

unset LD_AS_NEEDED
%ifarch %{arm}
export CFLAGS+=" -mfloat-abi=softfp -mfpu=neon -fPIE"
%endif
export CFLAGS+=" -D__TIZEN__ -DPA_SMOOTH_VOLUME -D__TIZEN_BT__ -D__TIZEN_LOG__ -DBLUETOOTH_APTX_SUPPORT"
export LDFLAGS+="-Wl,--no-as-needed -pie"
%reconfigure \
%if 0%{?tizen_build_binary_release_type_eng}
 --enable-pcm-dump \
%endif
%if "%{sec_product_feature_mmfw_audio_echo_cancel}" == "lvvefs"
 --enable-lvvefs \
%endif
%ifarch %{arm}
 --enable-secsrc --enable-security \
%endif
 %{conf_option}
make %{?_smp_mflags}

%install
%make_install
mkdir -p %{buildroot}/%{_datadir}/license
cp LICENSE.LGPL-2.1+ %{buildroot}/%{_datadir}/license/pulseaudio-modules
 
%post
/sbin/ldconfig
 
%postun
/sbin/ldconfig
 
%files
%manifest pulseaudio-modules-tizen.manifest
%defattr(-,root,root,-)
%{_libdir}/pulse-4.0/modules/module-*.so
%{_datadir}/license/pulseaudio-modules
