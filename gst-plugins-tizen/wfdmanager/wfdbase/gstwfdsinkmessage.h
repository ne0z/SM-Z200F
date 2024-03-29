/*
 * wfdrtsp message
 *
 * Copyright (c) 2011 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Taewan Kim<taewan79.kim@samsung.com>, Yejin Cho<cho.yejin@samsung.com>, Sangkyu Park<sk1122.park@samsung.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library (COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GST_WFD_SINK_MESSAGE_H__
#define __GST_WFD_SINK_MESSAGE_H__

#include <glib.h>

G_BEGIN_DECLS

#define GST_STRING_WFD_AUDIO_CODECS               "wfd_audio_codecs"
#define GST_STRING_WFD_VIDEO_FORMATS              "wfd_video_formats"
#define GST_STRING_WFD_3D_VIDEO_FORMATS           "wfd_3d_video_formats"
#define GST_STRING_WFD_CONTENT_PROTECTION         "wfd_content_protection"
#define GST_STRING_WFD_DISPLAY_EDID               "wfd_display_edid"
#define GST_STRING_WFD_COUPLED_SINK               "wfd_coupled_sink"
#define GST_STRING_WFD_TRIGGER_METHOD             "wfd_trigger_method"
#define GST_STRING_WFD_PRESENTATION_URL           "wfd_presentation_URL"
#define GST_STRING_WFD_CLIENT_RTP_PORTS           "wfd_client_rtp_ports"
#define GST_STRING_WFD_ROUTE                      "wfd_route"
#define GST_STRING_WFD_I2C                        "wfd_I2C"
#define GST_STRING_WFD_AV_FORMAT_CHANGE_TIMING    "wfd_av_format_change_timing"
#define GST_STRING_WFD_PREFERRED_DISPLAY_MODE     "wfd_preferred_display_mode"
#define GST_STRING_WFD_STANDBY_RESUME_CAPABILITY  "wfd_standby_resume_capability"
#define GST_STRING_WFD_STANDBY                    "wfd_standby"
#define GST_STRING_WFD_CONNECTOR_TYPE             "wfd_connector_type"
#define GST_STRING_WFD_IDR_REQUEST                "wfd_idr_request"
#define GST_STRING_WFD_NONE                       "none"
#define GST_STRING_WFD_ENABLE                     "enable"
#define GST_STRING_WFD_DISABLE                    "disable"
#define GST_STRING_WFD_CRLF                       "\r\n"
#define GST_STRING_WFD_SPACE                      " "
#define GST_STRING_WFD_INPUT_CATEGORY_LIST        "input_category_list"
#define GST_STRING_WFD_GENERIC                    "GENERIC"
#define GST_STRING_WFD_HIDC                       "HIDC"
#define GST_STRING_WFD_GENERIC_CAP_LIST           "generic_cap_list"
#define GST_STRING_WFD_KEYBOARD                   "Keyboard"
#define GST_STRING_WFD_MOUSE                      "Mouse"
#define GST_STRING_WFD_SINGLE_TOUCH               "SingleTouch"
#define GST_STRING_WFD_MULTI_TOUCH                "MultiTouch"
#define GST_STRING_WFD_JOYSTICK                   "Joystick"
#define GST_STRING_WFD_CAMERA                     "Camera"
#define GST_STRING_WFD_GESTURE                    "Gesture"
#define GST_STRING_WFD_REMOTE_CONTROL             "RemoteControl"
#define GST_STRING_WFD_HIDC_CAP_LIST              "hidc_cap_list"
#define GST_STRING_WFD_INFRARED                   "Infrared"
#define GST_STRING_WFD_USB                        "USB"
#define GST_STRING_WFD_BT                         "BT"
#define GST_STRING_WFD_ZIGBEE                     "Zigbee"
#define GST_STRING_WFD_WIFI                       "Wi-Fi"
#define GST_STRING_WFD_NO_SP                      "No-SP"
#define GST_STRING_WFD_PORT                       "port"
#define GST_STRING_WFD_SUPPORTED                  "supported"
#define GST_STRING_WFD_LPCM                       "LPCM"
#define GST_STRING_WFD_AAC                        "AAC"
#define GST_STRING_WFD_AC3                        "AC3"
#define GST_STRING_WFD_HDCP2_0                    "HDCP2.0"
#define GST_STRING_WFD_HDCP2_1                    "HDCP2.1"
#define GST_STRING_WFD_SETUP                      "SETUP"
#define GST_STRING_WFD_PAUSE                      "PAUSE"
#define GST_STRING_WFD_TEARDOWN                   "TEARDOWN"
#define GST_STRING_WFD_PLAY                       "PLAY"
#define GST_STRING_WFD_RTP                        "RTP"
#define GST_STRING_WFD_RDT                        "RDT"
#define GST_STRING_WFD_AVP                        "AVP"
#define GST_STRING_WFD_SAVP                       "SAVP"
#define GST_STRING_WFD_UDP                        "UDP"
#define GST_STRING_WFD_TCP                        "TCP"
#define GST_STRING_WFD_UNICAST                    "unicast"
#define GST_STRING_WFD_MULTICAST                  "multicast"
#define GST_STRING_WFD_TCP_HTTP                   "HTTP"
#define GST_STRING_WFD_PRIMARY                    "primary"
#define GST_STRING_WFD_SECONDARY                  "secondary"
#define GST_STRING_WFD_COMMA                      ","
#define GST_STRING_WFD_EQUALS                     "="
#define GST_STRING_WFD_COLON                      ":"
#define GST_STRING_WFD_SEMI_COLON                 ";"
#define GST_STRING_WFD_SLASH                      "/"

/**
 * GstWFDResult:
 * @GST_WFD_OK: A successful return value
 * @GST_WFD_EINVAL: a function was given invalid parameters
 *
 * Return values for the WFD functions.
 */
typedef enum {
  GST_WFD_OK     = 0,
  GST_WFD_EINVAL = -1,
  GST_WFD_NOT_IMPLEMENTED = -2,
  GST_WFD_NOT_SUPPORTED = -3
} GstWFDResult;

typedef enum {
  GST_WFD_AUDIO_UNKNOWN   = 0,
  GST_WFD_AUDIO_LPCM      = (1 << 0),
  GST_WFD_AUDIO_AAC       = (1 << 1),
  GST_WFD_AUDIO_AC3       = (1 << 2)
} GstWFDAudioFormats;

typedef enum {
  GST_WFD_FREQ_UNKNOWN  = 0,
  GST_WFD_FREQ_44100    = (1 << 0),
  GST_WFD_FREQ_48000    = (1 << 1)
} GstWFDAudioFreq;

typedef enum {
  GST_WFD_CHANNEL_UNKNOWN = 0,
  GST_WFD_CHANNEL_2       = (1 << 0),
  GST_WFD_CHANNEL_4       = (1 << 1),
  GST_WFD_CHANNEL_6       = (1 << 2),
  GST_WFD_CHANNEL_8       = (1 << 3)
} GstWFDAudioChannels;


typedef enum {
  GST_WFD_VIDEO_UNKNOWN = 0,
  GST_WFD_VIDEO_H264    = (1 << 0)
} GstWFDVideoCodecs;

typedef enum {
  GST_WFD_VIDEO_CEA_RESOLUTION = 0,
  GST_WFD_VIDEO_VESA_RESOLUTION,
  GST_WFD_VIDEO_HH_RESOLUTION
} GstWFDVideoNativeResolution;

typedef enum {
  GST_WFD_CEA_UNKNOWN      = 0,
  GST_WFD_CEA_640x480P60   = (1 << 0),
  GST_WFD_CEA_720x480P60   = (1 << 1),
  GST_WFD_CEA_720x480I60   = (1 << 2),
  GST_WFD_CEA_720x576P50   = (1 << 3),
  GST_WFD_CEA_720x576I50   = (1 << 4),
  GST_WFD_CEA_1280x720P30  = (1 << 5),
  GST_WFD_CEA_1280x720P60  = (1 << 6),
  GST_WFD_CEA_1920x1080P30 = (1 << 7),
  GST_WFD_CEA_1920x1080P60 = (1 << 8),
  GST_WFD_CEA_1920x1080I60 = (1 << 9),
  GST_WFD_CEA_1280x720P25  = (1 << 10),
  GST_WFD_CEA_1280x720P50  = (1 << 11),
  GST_WFD_CEA_1920x1080P25 = (1 << 12),
  GST_WFD_CEA_1920x1080P50 = (1 << 13),
  GST_WFD_CEA_1920x1080I50 = (1 << 14),
  GST_WFD_CEA_1280x720P24  = (1 << 15),
  GST_WFD_CEA_1920x1080P24 = (1 << 16)
} GstWFDVideoCEAResolution;

typedef enum {
  GST_WFD_VESA_UNKNOWN       = 0,
  GST_WFD_VESA_800x600P30    = (1 << 0),
  GST_WFD_VESA_800x600P60    = (1 << 1),
  GST_WFD_VESA_1024x768P30   = (1 << 2),
  GST_WFD_VESA_1024x768P60   = (1 << 3),
  GST_WFD_VESA_1152x864P30   = (1 << 4),
  GST_WFD_VESA_1152x864P60   = (1 << 5),
  GST_WFD_VESA_1280x768P30   = (1 << 6),
  GST_WFD_VESA_1280x768P60   = (1 << 7),
  GST_WFD_VESA_1280x800P30   = (1 << 8),
  GST_WFD_VESA_1280x800P60   = (1 << 9),
  GST_WFD_VESA_1360x768P30   = (1 << 10),
  GST_WFD_VESA_1360x768P60   = (1 << 11),
  GST_WFD_VESA_1366x768P30   = (1 << 12),
  GST_WFD_VESA_1366x768P60   = (1 << 13),
  GST_WFD_VESA_1280x1024P30  = (1 << 14),
  GST_WFD_VESA_1280x1024P60  = (1 << 15),
  GST_WFD_VESA_1400x1050P30  = (1 << 16),
  GST_WFD_VESA_1400x1050P60  = (1 << 17),
  GST_WFD_VESA_1440x900P30   = (1 << 18),
  GST_WFD_VESA_1440x900P60   = (1 << 19),
  GST_WFD_VESA_1600x900P30   = (1 << 20),
  GST_WFD_VESA_1600x900P60   = (1 << 21),
  GST_WFD_VESA_1600x1200P30  = (1 << 22),
  GST_WFD_VESA_1600x1200P60  = (1 << 23),
  GST_WFD_VESA_1680x1024P30  = (1 << 24),
  GST_WFD_VESA_1680x1024P60  = (1 << 25),
  GST_WFD_VESA_1680x1050P30  = (1 << 26),
  GST_WFD_VESA_1680x1050P60  = (1 << 27),
  GST_WFD_VESA_1920x1200P30  = (1 << 28),
  GST_WFD_VESA_1920x1200P60  = (1 << 29)
} GstWFDVideoVESAResolution;

typedef enum {
  GST_WFD_HH_UNKNOWN     = 0,
  GST_WFD_HH_800x480P30  = (1 << 0),
  GST_WFD_HH_800x480P60  = (1 << 1),
  GST_WFD_HH_854x480P30  = (1 << 2),
  GST_WFD_HH_854x480P60  = (1 << 3),
  GST_WFD_HH_864x480P30  = (1 << 4),
  GST_WFD_HH_864x480P60  = (1 << 5),
  GST_WFD_HH_640x360P30  = (1 << 6),
  GST_WFD_HH_640x360P60  = (1 << 7),
  GST_WFD_HH_960x540P30  = (1 << 8),
  GST_WFD_HH_960x540P60  = (1 << 9),
  GST_WFD_HH_848x480P30  = (1 << 10),
  GST_WFD_HH_848x480P60  = (1 << 11)
} GstWFDVideoHHResolution;

typedef enum {
  GST_WFD_H264_UNKNOWN_PROFILE = 0,
  GST_WFD_H264_BASE_PROFILE    = (1 << 0),
  GST_WFD_H264_HIGH_PROFILE    = (1 << 1)
} GstWFDVideoH264Profile;

typedef enum {
  GST_WFD_H264_LEVEL_UNKNOWN = 0,
  GST_WFD_H264_LEVEL_3_1   = (1 << 0),
  GST_WFD_H264_LEVEL_3_2   = (1 << 1),
  GST_WFD_H264_LEVEL_4     = (1 << 2),
  GST_WFD_H264_LEVEL_4_1   = (1 << 3),
  GST_WFD_H264_LEVEL_4_2   = (1 << 4)
} GstWFDVideoH264Level;

typedef enum {
  GST_WFD_HDCP_NONE = 0,
  GST_WFD_HDCP_2_0  = (1 << 0),
  GST_WFD_HDCP_2_1  = (1 << 1)
} GstWFDHDCPProtection;

typedef enum {
  GST_WFD_SINK_UNKNOWN = -1,
  GST_WFD_SINK_NOT_COUPLED = 0,
  GST_WFD_SINK_COUPLED,
  GST_WFD_SINK_TEARDOWN_COUPLING,
  GST_WFD_SINK_RESERVED
} GstWFDCoupledSinkStatus;

typedef enum {
  GST_WFD_TRIGGER_UNKNOWN = 0,
  GST_WFD_TRIGGER_SETUP,
  GST_WFD_TRIGGER_PAUSE,
  GST_WFD_TRIGGER_TEARDOWN,
  GST_WFD_TRIGGER_PLAY
} GstWFDTrigger;

typedef enum {
  GST_WFD_RTSP_TRANS_UNKNOWN =  0,
  GST_WFD_RTSP_TRANS_RTP     = (1 << 0),
  GST_WFD_RTSP_TRANS_RDT     = (1 << 1)
} GstWFDRTSPTransMode;

typedef enum {
  GST_WFD_RTSP_PROFILE_UNKNOWN =  0,
  GST_WFD_RTSP_PROFILE_AVP     = (1 << 0),
  GST_WFD_RTSP_PROFILE_SAVP    = (1 << 1)
} GstWFDRTSPProfile;

typedef enum {
  GST_WFD_RTSP_LOWER_TRANS_UNKNOWN   = 0,
  GST_WFD_RTSP_LOWER_TRANS_UDP       = (1 << 0),
  GST_WFD_RTSP_LOWER_TRANS_UDP_MCAST = (1 << 1),
  GST_WFD_RTSP_LOWER_TRANS_TCP       = (1 << 2),
  GST_WFD_RTSP_LOWER_TRANS_HTTP      = (1 << 3)
} GstWFDRTSPLowerTrans;

typedef enum {
  GST_WFD_PRIMARY_SINK   = 0,
  GST_WFD_SECONDARY_SINK
} GstWFDSinkType;

typedef enum {
  GST_WFD_CONNECTOR_VGA           = 0,
  GST_WFD_CONNECTOR_S,
  GST_WFD_CONNECTOR_COMPOSITE,
  GST_WFD_CONNECTOR_COMPONENT,
  GST_WFD_CONNECTOR_DVI,
  GST_WFD_CONNECTOR_HDMI,
  GST_WFD_CONNECTOR_LVDS,
  GST_WFD_CONNECTOR_RESERVED_7,
  GST_WFD_CONNECTOR_JAPANESE_D,
  GST_WFD_CONNECTOR_SDI,
  GST_WFD_CONNECTOR_DP,
  GST_WFD_CONNECTOR_RESERVED_11,
  GST_WFD_CONNECTOR_UDI,
  GST_WFD_CONNECTOR_NO           = 254,
  GST_WFD_CONNECTOR_PHYSICAL     = 255
} GstWFDConnector;

typedef enum {
  GST_WFD_PREFERRED_DISPLAY_MODE_NOT_SUPPORTED = 0,
  GST_WFD_PREFERRED_DISPLAY_MODE_SUPPORTED = 1
} GstWFDPreferredDisplayModeEnum;

typedef struct {
  gchar  *audio_format;
  guint32 modes;
  guint latency;
} GstWFDAudioCodec;

typedef struct {
  guint  count;
  GstWFDAudioCodec *list;
} GstWFDAudioCodeclist;


typedef struct {
  guint CEA_Support;
  guint VESA_Support;
  guint HH_Support;
  guint latency;
  guint min_slice_size;
  guint slice_enc_params;
  guint frame_rate_control_support;
} GstWFDVideoH264MiscParams;

typedef struct {
  guint profile;
  guint level;
  guint max_hres;
  guint max_vres;
  GstWFDVideoH264MiscParams misc_params;
} GstWFDVideoH264Codec;

typedef struct {
  guint native;
  guint preferred_display_mode_supported;
  GstWFDVideoH264Codec H264_codec;
} GstWFDVideoCodec;

typedef struct {
  guint count;
  GstWFDVideoCodec *list;
} GstWFDVideoCodeclist;

typedef struct {
  guint64 video_3d_capability;
  guint latency;
  guint min_slice_size;
  guint slice_enc_params;
  guint frame_rate_control_support;
} GstWFD3DVideoH264MiscParams;

typedef struct {
  guint profile;
  guint level;
  GstWFD3DVideoH264MiscParams misc_params;
  guint max_hres;
  guint max_vres;
} GstWFD3DVideoH264Codec;

typedef struct {
  guint native;
  guint preferred_display_mode_supported;
  GstWFD3DVideoH264Codec H264_codec;
} GstWFD3dCapList;

typedef struct {
  guint count;
  GstWFD3dCapList *list;
} GstWFD3DFormats;

typedef struct {
  gchar *hdcpversion;
  gchar *TCPPort;
} GstWFDHdcp2Spec;

typedef struct {
  GstWFDHdcp2Spec *hdcp2_spec;
} GstWFDContentProtection;

typedef struct {
  guint edid_supported;
  guint edid_block_count;
  gchar *edid_payload;
} GstWFDDisplayEdid;

typedef struct {
  guint status;
  gchar *sink_address;
} GstWFDCoupledSinkCap;

typedef struct {
  GstWFDCoupledSinkCap *coupled_sink_cap;
} GstWFDCoupledSink;

typedef struct {
  gchar *wfd_trigger_method;
} GstWFDTriggerMethod;

typedef struct {
  gchar *wfd_url0;
  gchar *wfd_url1;
} GstWFDPresentationUrl;

typedef struct {
  gchar *profile;
  guint32 rtp_port0;
  guint32 rtp_port1;
  gchar *mode;
} GstWFDClientRtpPorts;

typedef struct {
  gchar *destination;
} GstWFDRoute;

typedef struct {
  gboolean I2CPresent;
  guint32 I2C_port;
} GstWFDI2C;

typedef struct {
  guint64 PTS;
  guint64 DTS;
} GstWFDAVFormatChangeTiming;

typedef struct {
  gboolean displaymodesupported;
  guint64 p_clock;
  guint32 H;
  guint32 HB;
  guint32 HSPOL_HSOFF;
  guint32 HSW;
  guint32 V;
  guint32 VB;
  guint32 VSPOL_VSOFF;
  guint32 VSW;
  guint VBS3D;
  guint R;
  guint V2d_s3d_modes;
  guint P_depth;
  GstWFDVideoH264Codec H264_codec;
} GstWFDPreferredDisplayMode;

typedef struct {
  gboolean standby_resume_cap;
} GstWFDStandbyResumeCapability;

typedef struct {
  gboolean wfd_standby;
} GstWFDStandby;

typedef struct {
  gboolean supported;
  gint32 connector_type;
} GstWFDConnectorType;

typedef struct {
  gboolean idr_request;
} GstWFDIdrRequest;

/***********************************************************/

typedef struct {

  GstWFDAudioCodeclist *audio_codecs;
  GstWFDVideoCodeclist *video_formats;
  GstWFD3DFormats *video_3d_formats;
  GstWFDContentProtection *content_protection;
  GstWFDDisplayEdid *display_edid;
  GstWFDCoupledSink *coupled_sink;
  GstWFDTriggerMethod *trigger_method;
  GstWFDPresentationUrl *presentation_url;
  GstWFDClientRtpPorts *client_rtp_ports;
  GstWFDRoute *route;
  GstWFDI2C *I2C;
  GstWFDAVFormatChangeTiming *av_format_change_timing;
  GstWFDPreferredDisplayMode *preferred_display_mode;
  GstWFDStandbyResumeCapability *standby_resume_capability;
  GstWFDStandby *standby;
  GstWFDConnectorType *connector_type;
  GstWFDIdrRequest *idr_request;
} GstWFDMessage;

/* Session descriptions */
GstWFDResult gst_wfd_message_new(GstWFDMessage **msg);
GstWFDResult gst_wfd_message_init(GstWFDMessage *msg);
GstWFDResult gst_wfd_message_uninit(GstWFDMessage *msg);
GstWFDResult gst_wfd_message_free(GstWFDMessage *msg);
GstWFDResult gst_wfd_message_parse_buffer(const guint8 *data, guint size, GstWFDMessage *msg);
gchar *gst_wfd_message_as_text(const GstWFDMessage *msg);
gchar *gst_wfd_message_param_names_as_text(const GstWFDMessage *msg);
GstWFDResult gst_wfd_message_dump(const GstWFDMessage *msg);


GstWFDResult gst_wfd_message_set_supported_audio_format(GstWFDMessage *msg,
                                               guint aCodec, guint aFreq, guint aChanels,
                                               guint aBitwidth, guint32 aLatency);
GstWFDResult gst_wfd_message_set_prefered_audio_format(GstWFDMessage *msg,
                                              GstWFDAudioFormats aCodec, GstWFDAudioFreq aFreq, GstWFDAudioChannels aChanels,
                                              guint aBitwidth, guint32 aLatency);
GstWFDResult gst_wfd_message_get_supported_audio_format(GstWFDMessage *msg,
                                               guint *aCodec, guint *aFreq, guint *aChanels,
                                               guint *aBitwidth, guint32 *aLatency);
GstWFDResult gst_wfd_message_get_prefered_audio_format(GstWFDMessage *msg,
                                              GstWFDAudioFormats *aCodec, GstWFDAudioFreq *aFreq, GstWFDAudioChannels *aChanels,
                                              guint *aBitwidth, guint32 *aLatency);

GstWFDResult gst_wfd_message_set_supported_video_format(GstWFDMessage *msg, GstWFDVideoCodecs vCodec,
                                               GstWFDVideoNativeResolution vNative, guint64 vNativeResolution,
                                               guint64 vCEAResolution, guint64 vVESAResolution, guint64 vHHResolution,
                                               guint vProfile, guint vLevel, guint32 vLatency, guint32 vMaxHeight,
                                               guint32 vMaxWidth, guint32 min_slice_size, guint32 slice_enc_params, guint frame_rate_control,
                                               guint preferred_display_mode);
GstWFDResult gst_wfd_message_set_prefered_video_format(GstWFDMessage *msg, GstWFDVideoCodecs vCodec,
                                              GstWFDVideoNativeResolution vNative, guint64 vNativeResolution,
                                              GstWFDVideoCEAResolution vCEAResolution, GstWFDVideoVESAResolution vVESAResolution,
                                              GstWFDVideoHHResolution vHHResolution, GstWFDVideoH264Profile vProfile,
                                              GstWFDVideoH264Level vLevel, guint32 vLatency, guint32 vMaxHeight,
                                              guint32 vMaxWidth, guint32 min_slice_size, guint32 slice_enc_params, guint frame_rate_control);
GstWFDResult gst_wfd_message_get_supported_video_format(GstWFDMessage *msg, GstWFDVideoCodecs *vCodec,
                                               GstWFDVideoNativeResolution *vNative, guint64 *vNativeResolution,
                                               guint64 *vCEAResolution, guint64 *vVESAResolution, guint64 *vHHResolution,
                                               guint *vProfile, guint *vLevel, guint32 *vLatency, guint32 *vMaxHeight,
                                               guint32 *vMaxWidth, guint32 *min_slice_size, guint32 *slice_enc_params, guint *frame_rate_control);
GstWFDResult gst_wfd_message_get_prefered_video_format(GstWFDMessage *msg, GstWFDVideoCodecs *vCodec,
                                              GstWFDVideoNativeResolution *vNative, guint64 *vNativeResolution,
                                              GstWFDVideoCEAResolution *vCEAResolution, GstWFDVideoVESAResolution *vVESAResolution,
                                              GstWFDVideoHHResolution *vHHResolution, GstWFDVideoH264Profile *vProfile,
                                              GstWFDVideoH264Level *vLevel, guint32 *vLatency, guint32 *vMaxHeight,
                                              guint32 *vMaxWidth, guint32 *min_slice_size, guint32 *slice_enc_params, guint *frame_rate_control);

/* Todo wfd-3d-formats */

GstWFDResult gst_wfd_message_set_contentprotection_type(GstWFDMessage *msg, GstWFDHDCPProtection hdcpversion, guint32 TCPPort);
GstWFDResult gst_wfd_message_get_contentprotection_type(GstWFDMessage *msg, GstWFDHDCPProtection *hdcpversion, guint32 *TCPPort);

GstWFDResult gst_wfd_message_set_display_EDID(GstWFDMessage *msg, gboolean edid_supported, guint32 edid_blockcount, gchar *edid_playload);
GstWFDResult gst_wfd_message_get_display_EDID(GstWFDMessage *msg, gboolean *edid_supported, guint32 *edid_blockcount, gchar **edid_playload);


GstWFDResult gst_wfd_message_set_coupled_sink(GstWFDMessage *msg, GstWFDCoupledSinkStatus status, gchar *sink_address);
GstWFDResult gst_wfd_message_get_coupled_sink(GstWFDMessage *msg, GstWFDCoupledSinkStatus *status, gchar **sink_address);

GstWFDResult gst_wfd_message_set_trigger_type(GstWFDMessage *msg, GstWFDTrigger trigger);
GstWFDResult gst_wfd_message_get_trigger_type(GstWFDMessage *msg, GstWFDTrigger *trigger);

GstWFDResult gst_wfd_message_set_presentation_url(GstWFDMessage *msg, gchar *wfd_url0, gchar *wfd_url1);
GstWFDResult gst_wfd_message_get_presentation_url(GstWFDMessage *msg, gchar **wfd_url0, gchar **wfd_url1);

GstWFDResult gst_wfd_message_set_prefered_RTP_ports(GstWFDMessage *msg, GstWFDRTSPTransMode trans, GstWFDRTSPProfile profile,
                                           GstWFDRTSPLowerTrans lowertrans, guint32 rtp_port0, guint32 rtp_port1);
GstWFDResult gst_wfd_message_get_prefered_RTP_ports(GstWFDMessage *msg, GstWFDRTSPTransMode *trans, GstWFDRTSPProfile *profile,
                                           GstWFDRTSPLowerTrans *lowertrans, guint32 *rtp_port0, guint32 *rtp_port1);

GstWFDResult gst_wfd_message_set_audio_sink_type(GstWFDMessage *msg, GstWFDSinkType sinktype);
GstWFDResult gst_wfd_message_get_audio_sink_type(GstWFDMessage *msg, GstWFDSinkType *sinktype);

GstWFDResult gst_wfd_message_set_I2C_port(GstWFDMessage *msg, gboolean i2csupport, guint32 i2cport);
GstWFDResult gst_wfd_message_get_I2C_port(GstWFDMessage *msg, gboolean *i2csupport, guint32 *i2cport);

GstWFDResult gst_wfd_message_set_av_format_change_timing(GstWFDMessage *msg, guint64 PTS, guint64 DTS);
GstWFDResult gst_wfd_message_get_av_format_change_timing(GstWFDMessage *msg, guint64 *PTS, guint64 *DTS);

/* Todo wfd-preferred-display-mode */

GstWFDResult gst_wfd_message_set_standby_resume_capability(GstWFDMessage *msg, gboolean supported);
GstWFDResult gst_wfd_message_get_standby_resume_capability(GstWFDMessage *msg, gboolean *supported);

GstWFDResult gst_wfd_message_set_standby(GstWFDMessage *msg, gboolean standby_enable);
GstWFDResult gst_wfd_message_get_standby(GstWFDMessage *msg, gboolean *standby_enable);

GstWFDResult gst_wfd_message_set_connector_type(GstWFDMessage *msg, GstWFDConnector connector);
GstWFDResult gst_wfd_message_get_connector_type(GstWFDMessage *msg, GstWFDConnector *connector);

GstWFDResult gst_wfd_message_set_idr_request(GstWFDMessage *msg);

G_END_DECLS

#endif /* __GST_WFD_SINK_MESSAGE_H__ */
