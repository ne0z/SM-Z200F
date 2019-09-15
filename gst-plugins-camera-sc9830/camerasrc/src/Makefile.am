############################################################################
# Samsung camera Gstreamer Plugin library(LGPL2.1 LICENSE)
############################################################################

plugin_LTLIBRARIES = libgstcamerasrc.la

libgstcamerasrc_la_SOURCES := gstcamerasrccontrol.c
libgstcamerasrc_la_SOURCES += gstcamerasrccolorbalance.c
libgstcamerasrc_la_SOURCES += gstcamerasrc.c
libgstcamerasrc_la_SOURCES += HAL/ss/camera_main.c
libgstcamerasrc_la_SOURCES += HAL/ss/camera_preview.c
libgstcamerasrc_la_SOURCES += HAL/ss/camera_snapshot.c
libgstcamerasrc_la_SOURCES += HAL/ss/camera_record.c

libgstcamerasrc_la_CFLAGS := $(X11_CFLAGS)
libgstcamerasrc_la_CFLAGS += $(DRI2_CFLAGS)
libgstcamerasrc_la_CFLAGS += $(TBM_CFLAGS)
libgstcamerasrc_la_CFLAGS += $(SENSOR_CFLAGS)
libgstcamerasrc_la_CFLAGS += $(XFIXES_CFLAGS)
libgstcamerasrc_la_CFLAGS += $(GST_CFLAGS)
libgstcamerasrc_la_CFLAGS += $(GST_BASE_CFLAGS)
libgstcamerasrc_la_CFLAGS += $(GST_VIDEO_FLAGS)
libgstcamerasrc_la_CFLAGS += $(CAMSRCJPEGENC_CFLAGS)
libgstcamerasrc_la_CFLAGS += $(GST_INTERFACES_CFLAGS)
libgstcamerasrc_la_CFLAGS += $(MMUTIL_IMGP_CFLAGS)
libgstcamerasrc_la_CFLAGS += $(MMUTIL_JPEG_CFLAGS)
libgstcamerasrc_la_CFLAGS += $(MM_COMMON_CFLAGS)
libgstcamerasrc_la_CFLAGS += $(CSC_FEATURE_CFLAGS)
libgstcamerasrc_la_CFLAGS += $(GLIB_CFLAGS)
libgstcamerasrc_la_CFLAGS += -I$(srcdir)/include
libgstcamerasrc_la_CFLAGS += -I$(srcdir)/include/HAL/android
libgstcamerasrc_la_CFLAGS += -I$(srcdir)/include/HAL/ss

libgstcamerasrc_la_CFLAGS += -DCAMERA_INTF
libgstcamerasrc_la_CFLAGS += -DCAMERA_ION_FALLBACK_HEAP_ID=ION_IOMMU_HEAP_ID
libgstcamerasrc_la_CFLAGS += -DCAMERA_ION_HEAP_ID=ION_IOMMU_HEAP_ID
libgstcamerasrc_la_CFLAGS += -D__PLATFORM_TIZEN__
libgstcamerasrc_la_CFLAGS += -D__MINT_CONF_SPRD__
libgstcamerasrc_la_CFLAGS += -DENABLE_ZERO_COPY
libgstcamerasrc_la_CFLAGS += -DCONFIG_PRE_ALLOC_CAPTURE_MEM
libgstcamerasrc_la_CFLAGS += -D_VSP_
libgstcamerasrc_la_CFLAGS += -DJPEG_ENC
libgstcamerasrc_la_CFLAGS += -D_VSP_LINUX_
libgstcamerasrc_la_CFLAGS += -DCHIP_ENDIAN_LITTLE
libgstcamerasrc_la_CFLAGS += -DCONFIG_CAMERA_ISP
libgstcamerasrc_la_CFLAGS += -DCONFIG_CAMERA_ISP_VERSION_V3
#libgstcamerasrc_la_CFLAGS += -DRAW_CB_FOR_SHUTTER_SOUND
libgstcamerasrc_la_CFLAGS += -DCONFIG_CAMERA_ZSL_CAPTURE
libgstcamerasrc_la_CFLAGS += -DCONFIG_CAMERA_FORCE_ZSL_CAPTURE
libgstcamerasrc_la_CFLAGS += -DCONFIG_CAMERA_SUPPORT_5M
libgstcamerasrc_la_CFLAGS += -DCONFIG_MODULE_J1MINI3G
libgstcamerasrc_la_CFLAGS += -DCONFIG_CAMERA_AUTOFOCUS_NOT_SUPPORT
#libgstcamerasrc_la_CFLAGS += -DCONFIG_CAMERA_FLASH_NOT_SUPPORT


libgstcamerasrc_la_LIBADD = $(X11_LIBS) \
                            $(DRI2_LIBS) \
                            $(TBM_LIBS) \
                            $(SENSOR_LIBS) \
                            $(XFIXES_LIBS) \
                            $(GST_LIBS) \
                            $(GST_BASE_LIBS) \
                            $(GST_PLUGINS_BASE_LIBS) \
                            $(GST_VIDEO_LIBS) \
                            -ldl -lpthread -lrt -lm -ldlog\
                            $(CAMSRCJPEGENC_LIBS) \
                            $(GST_INTERFACES_LIBS) \
                            $(MMUTIL_IMGP_LIBS) \
                            $(MMUTIL_JPEG_LIBS) \
                            $(CSC_FEATURE_LIBS) \
                            $(GLIB_LIBS) \
                            $(DBUS_LIBS) \
                            -lcamerahal \
                            -lcamerahdr

libgstcamerasrc_la_LDFLAGS = $(X11_LDFLAGS) $(XFIXES_LDFLAGS) $(GST_PLUGIN_LDFLAGS) -L./libs

libgstcamerasrc_la_CPPFLAGS = -I$(srcdir)/include
libgstcamerasrc_la_CPPFLAGS += -I$(srcdir)/include/HAL/ss
libgstcamerasrc_la_CPPFLAGS += -I$(srcdir)/include/HAL/
libgstcamerasrc_la_CPPFLAGS += -I$(srcdir)/HAL/sprd/hal1.0/inc
libgstcamerasrc_la_CPPFLAGS += -I$(srcdir)/HAL/ss

libgstcamerasrc_la_CPPFLAGS += -D__PLATFORM_TIZEN__
libgstcamerasrc_la_CPPFLAGS += -D__MINT_CONF_SPRD__
libgstcamerasrc_la_CPPFLAGS += -DENABLE_ZERO_COPY
libgstcamerasrc_la_CPPFLAGS += -DUSE_TBM
libgstcamerasrc_la_CPPFLAGS += -DCONFIG_PRE_ALLOC_CAPTURE_MEM
libgstcamerasrc_la_CPPFLAGS += -D_VSP_
libgstcamerasrc_la_CPPFLAGS += -DJPEG_ENC
libgstcamerasrc_la_CPPFLAGS += -D_VSP_LINUX_
libgstcamerasrc_la_CPPFLAGS += -DCHIP_ENDIAN_LITTLE
#libgstcamerasrc_la_CPPFLAGS += -DRAW_CB_FOR_SHUTTER_SOUND
libgstcamerasrc_la_CPPFLAGS += -DCONFIG_CAMERA_ISP
libgstcamerasrc_la_CPPFLAGS += -DCONFIG_CAMERA_ISP_VERSION_V3
libgstcamerasrc_la_CPPFLAGS += -DCONFIG_CAMERA_ZSL_CAPTURE
libgstcamerasrc_la_CPPFLAGS += -DCONFIG_CAMERA_FORCE_ZSL_CAPTURE
libgstcamerasrc_la_CPPFLAGS += -DCONFIG_CAMERA_SUPPORT_5M
libgstcamerasrc_la_CPPFLAGS += -DCONFIG_MODULE_J1MINI3G
libgstcamerasrc_la_CPPFLAGS += -DCONFIG_CAMERA_AUTOFOCUS_NOT_SUPPORT
#libgstcamerasrc_la_CPPFLAGS += -DCONFIG_CAMERA_FLASH_NOT_SUPPORT

install-exec-hook:
	mkdir -p $(DESTDIR)$(prefix)/lib && \
	cp -P $(srcdir)/libs/* $(DESTDIR)$(prefix)/lib/
