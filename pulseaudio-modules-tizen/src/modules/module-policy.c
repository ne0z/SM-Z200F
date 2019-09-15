
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdbool.h>
#include <strings.h>
#include <vconf.h> // for mono
#include <iniparser.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <pulse/proplist.h>
#include <pulse/timeval.h>
#include <pulse/util.h>
#include <pulse/rtclock.h>

#include <pulsecore/core.h>
#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/core-error.h>
#include <pulsecore/core-rtclock.h>
#include <pulsecore/core-scache.h>
#include <pulsecore/core-subscribe.h>
#include <pulsecore/core-util.h>
#include <pulsecore/mutex.h>
#include <pulsecore/log.h>
#include <pulsecore/namereg.h>
#include <pulsecore/sink-input.h>
#include <pulsecore/source-output.h>
#include <pulsecore/protocol-native.h>
#include <pulsecore/pstream-util.h>
#include <pulsecore/strbuf.h>
#include <pulsecore/sink-input.h>
#include <pulsecore/sound-file.h>
#include <pulsecore/play-memblockq.h>
#include <pulsecore/shared.h>

#include "module-policy-symdef.h"
#include <tizen-audio.h>

#define VCONFKEY_SOUND_HDMI_SUPPORT "memory/private/sound/hdmisupport"
#define VCONFKEY_SOUND_PRIMARY_VOLUME_TYPE "memory/private/Sound/PrimaryVolumetype"

PA_MODULE_AUTHOR("Seungbae Shin");
PA_MODULE_DESCRIPTION("Media Policy module");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(TRUE);
PA_MODULE_USAGE(
        "on_hotplug=<When new device becomes available, recheck streams?> "
        "use_wideband_voice=<Set to 1 to enable wb voice. Default nb>"
        "fragment_size=<fragment size>"
        "tsched_buffer_size=<buffer size when using timer based scheduling> ");

static const char* const valid_modargs[] = {
    "on_hotplug",
    "use_wideband_voice",
    "fragment_size",
    "tsched_buffersize",
    "tsched_buffer_size",
    NULL
};

typedef enum pa_hal_event_type {
    PA_HAL_EVENT_NONE,
    PA_HAL_EVENT_LOAD_DEVICE,
    PA_HAL_EVENT_OPEN_DEVICE,
    PA_HAL_EVENT_CLOSE_ALL_DEVICES,
    PA_HAL_EVENT_CLOSE_DEVICE,
    PA_HAL_EVENT_UNLOAD_DEVICE,
#ifdef FM_BT
    PA_HAL_EVENT_ROUTE_BT_FM,
    PA_HAL_EVENT_UNROUTE_BT_FM
#endif
} pa_hal_event_type_t;

struct pa_hal_device_event_data {
    audio_device_info_t device_info;
    audio_device_param_info_t params[AUDIO_DEVICE_PARAM_MAX];
};

struct pa_primary_volume_type_info {
    void* key;
    int volumetype;
    int priority;
    PA_LLIST_FIELDS(struct pa_primary_volume_type_info);
};

struct userdata {
    pa_core *core;
    pa_module *module;

    pa_hook_slot *sink_input_new_hook_slot,*sink_put_hook_slot;

    pa_hook_slot *sink_input_unlink_slot,*sink_unlink_slot;
    pa_hook_slot *sink_input_put_slot;
    pa_hook_slot *sink_input_unlink_post_slot, *sink_unlink_post_slot;
    pa_hook_slot *sink_input_move_start_slot,*sink_input_move_finish_slot;
    pa_hook_slot *source_output_new_hook_slot, *source_output_unlink_post_slot;
    pa_hook_slot *source_output_put_slot;
    pa_hook_slot *sink_state_changed_slot;
    pa_hook_slot *sink_input_state_changed_slot;

    pthread_t tid;
    struct {
        pa_defer_event *event;
        pa_cond *cond;
        pa_mutex *mutex;
        pa_hal_event_type_t event_type;
        void *event_data;
    } defer;
    int fd_defer[2];
    pa_io_event *defer_io;

    pa_time_event *debug_time_event;

    pa_subscription *subscription;

    pa_bool_t on_hotplug:1;
    int bt_off_idx;
    pa_sink* bluez_sink;

    uint32_t session;
    uint32_t subsession;
    uint32_t subsession_opt;
    uint32_t active_device_in;
    uint32_t active_device_out;
    uint32_t active_route_flag;
    uint32_t cache_route_flag;

    float balance;
    int muteall;
    int call_muted;

    uint32_t call_type;
    uint32_t call_nrec;
    uint32_t call_extra_volume;
    uint32_t link_direction;
    uint32_t bt_bandwidth;
    uint32_t bt_nrec;

    pa_bool_t wideband;
    int fragment_size;
    int tsched_buffer_size;
    board_config_t *board_config;

    int fade_rcount[AUDIO_VOLUME_TYPE_MAX];
    pa_idxset *fade_streams;
    pa_idxset *half_fade_streams;

    pa_module* module_combined;
    pa_native_protocol *protocol;

#ifdef FM_BT
    pa_module* loopback_fm;
#endif

    PA_LLIST_HEAD(struct pa_primary_volume_type_info, primary_volume);

    struct {
        void *dl_handle;
        void *data;
        audio_interface_t intf;
    } audio_mgr;

    struct  { // for burst-shot
        pa_bool_t is_running;
        pa_mutex* mutex;
        int count; /* loop count */
        pa_time_event *time_event;
        pa_scache_entry *e;
        pa_sink_input *i;
        pa_memblockq *q;
        pa_usec_t time_interval;
        pa_usec_t factor; /* timer boosting */
    } audio_sample_userdata;
};

// for soundalive
#define CUSTOM_EQ_BAND_MAX      9
#define EQ_USER_SLOT_NUM        7
#define DHA_GAIN_NUM            12

enum {
    CUSTOM_EXT_3D_LEVEL,
    CUSTOM_EXT_BASS_LEVEL,
    CUSTOM_EXT_CONCERT_HALL_VOLUME,
    CUSTOM_EXT_CONCERT_HALL_LEVEL,
    CUSTOM_EXT_CLARITY_LEVEL,
    CUSTOM_EXT_PARAM_MAX
};

enum {
    SUBCOMMAND_TEST,
    SUBCOMMAND_PLAY_SAMPLE,
    SUBCOMMAND_PLAY_SAMPLE_CONTINUOUSLY,
    SUBCOMMAND_MONO,
    SUBCOMMAND_BALANCE,
    SUBCOMMAND_MUTEALL,
    SUBCOMMAND_SET_USE_CASE,
    SUBCOMMAND_SET_SESSION,
    SUBCOMMAND_SET_SUBSESSION,
    SUBCOMMAND_SET_ACTIVE_DEVICE,
    SUBCOMMAND_RESET, // 10
    SUBCOMMAND_GET_VOLUME_LEVEL_MAX,
    SUBCOMMAND_GET_VOLUME_LEVEL,
    SUBCOMMAND_SET_VOLUME_LEVEL,
    SUBCOMMAND_UPDATE_VOLUME,
    SUBCOMMAND_GET_MUTE,
    SUBCOMMAND_SET_MUTE,
    SUBCOMMAND_VOLUME_FADE,
    SUBCOMMAND_VOLUME_FADE_BY_TYPE,
    SUBCOMMAND_IS_AVAILABLE_HIGH_LATENCY,
    SUBCOMMAND_UNLOAD_HDMI,
    SUBCOMMAND_SET_TOUCH_SOUND,

    // call settings
    SUBCOMMAND_SET_CALL_NETWORK_TYPE, // 22
    SUBCOMMAND_SET_CALL_NREC,
    SUBCOMMAND_SET_CALL_EXTRA_VOLUME,
    SUBCOMMAND_SET_NETWORK_LINK_DIRECTION,
    SUBCOMMAND_SET_BLUETOOTH_BANDWIDTH,
    SUBCOMMAND_SET_BLUETOOTH_NREC,
    SUBCOMMAND_SET_ROUTE_INFO,

    // audio filters
    SUBCOMMAND_VSP_SPEED,
    SUBCOMMAND_SA_FILTER_ACTION,
    SUBCOMMAND_SA_PRESET_MODE,
    SUBCOMMAND_SA_EQ,
    SUBCOMMAND_SA_EXTEND,
    SUBCOMMAND_SA_DEVICE,
    SUBCOMMAND_SA_SQUARE,
    SUBCOMMAND_DHA_PARAM,
};

#ifndef _TIZEN_PUBLIC_
enum {
    SUBSESSION_OPT_SVOICE                   = 0x00000001,
    SUBSESSION_OPT_WAKEUP                   = 0x00000010,
    SUBSESSION_OPT_COMMAND                  = 0x00000020,
};
#endif


/* DEFINEs */
#define AEC_SINK            "alsa_output.0.analog-stereo.echo-cancel"
#define AEC_SOURCE          "alsa_input.0.analog-stereo.echo-cancel"
#define SINK_VOIP           "alsa_output.3.analog-stereo"
#define SINK_VIRTUAL        "alsa_output.virtual.analog-stereo"
#define ALSA_VIRTUAL_CARD   "VIRTUALAUDIO"
#define ALSA_VIRTUAL_CARD_LTE   "saudiolte"
#define SOURCE_ALSA         "alsa_input.0.analog-stereo"
#define SOURCE_VIRTUAL      "alsa_input.virtual.analog-stereo"
#define SOURCE_VOIP         "alsa_input.3.analog-stereo"
#define SINK_ALSA           "alsa_output.0.analog-stereo"
#define SINK_ALSA_UHQA      "alsa_output.0.analog-stereo-uhqa"
#define SINK_COMBINED       "combined"
#define SINK_HIGH_LATENCY   "alsa_output.4.analog-stereo"
#define SINK_HIGH_LATENCY_UHQA   "alsa_output.4.analog-stereo-uhqa"
#define SINK_HDMI           "alsa_output.1.analog-stereo"
#define SINK_HDMI_UHQA           "alsa_output.1.analog-stereo-uhqa"
#define SOURCE_MIRRORING    "alsa_input.8.analog-stereo"
#define POLICY_AUTO         "auto"
#define POLICY_AUTO_UHQA    "auto-uhqa"
#define POLICY_PHONE        "phone"
#define POLICY_ALL          "all"
#define POLICY_VOIP         "voip"
#define POLICY_HIGH_LATENCY "high-latency"
#define POLICY_HIGH_LATENCY_UHQA "high-latency-uhqa"
#define BLUEZ_API           "bluez"
#define ALSA_API            "alsa"
#define VOIP_API            "voip"
#define POLICY_MIRRORING    "mirroring"
#define POLICY_LOOPBACK    "loopback"
#define ALSA_MONITOR_SOURCE "alsa_output.0.analog-stereo.monitor"
#define MONO_KEY            VCONFKEY_SETAPPL_ACCESSIBILITY_MONO_AUDIO
#define HIGH_LATENCY_API    "high-latency"
#define NULL_SOURCE         "source.null"
#define ALSA_SAUDIOVOIP_CARD "saudiovoip"

#define sink_is_hdmi(sink) !strncmp(sink->name, SINK_HDMI, strlen(SINK_HDMI))
#define sink_is_highlatency(sink) !strncmp(sink->name, SINK_HIGH_LATENCY, strlen(SINK_HIGH_LATENCY))
#define sink_is_alsa(sink) !strncmp(sink->name, SINK_ALSA, strlen(SINK_ALSA))
#define sink_is_voip(sink) !strncmp(sink->name, SINK_VOIP, strlen(SINK_VOIP))

#define CH_5_1 6
#define CH_7_1 8
#define CH_STEREO 2

#define DEFAULT_FRAGMENT_SIZE 8192
#define DEFAULT_TSCHED_BUFFER_SIZE 16384
#define START_THRESHOLD    4096
/**
  * UHQA sampling rate vary from 96 KHz to 192 KHz, currently the plan is to configure sink with highest sampling
  * rate possible i.e. 192 KHz. So that < 192 KHz will be resampled and played. This will avoid creating multiple sinks
  * for multiple rates
  */
#define UHQA_SAMPLING_RATE 192000
#define UHQA_BASE_SAMPLING_RATE 96000

#define LIB_TIZEN_AUDIO "libtizen-audio.so"
#define PA_DUMP_INI_DEFAULT_PATH                "/usr/etc/mmfw_audio_pcm_dump.ini"
#define PA_DUMP_INI_TEMP_PATH                   "/opt/system/mmfw_audio_pcm_dump.ini"

#define MAX_VOLUME_FOR_MONO         65535
/* check if this sink is bluez */

#define DEFAULT_BOOTING_SOUND_PATH "/usr/share/keysound/poweron.wav"
#define BOOTING_SOUND_SAMPLE "booting"
#define VCONF_BOOTING "memory/private/sound/booting"

#define VCONF_SOUND_BURSTSHOT "memory/private/sound/burstshot"

#define AUDIO_CODEC_SPRDPHONE "sprdphone"
#define AUDIO_CODEC_MSM8X16 "msm8x16sndcard"

#define SOUND_STATUS_KEY    "memory/Sound/SoundStatus"
#define ASM_STATUS_VOIP     0x40000000

#define DEFAULT_FADE_DURATION_MSEC 20

typedef enum
{
    DOCK_NONE      = 0,
    DOCK_DESKDOCK  = 1,
    DOCK_CARDOCK   = 2,
    DOCK_AUDIODOCK = 7,
    DOCK_SMARTDOCK = 8
} DOCK_STATUS;
enum
{
    SOUND_CALL_NETWORK_TYPE_NONE,
    SOUND_CALL_NETWORK_TYPE_VOICECALL_NB,
    SOUND_CALL_NETWORK_TYPE_VOICECALL_WB,
    SOUND_CALL_NETWORK_TYPE_COMPANION_NB,
    SOUND_CALL_NETWORK_TYPE_COMPANION_WB,
    SOUND_CALL_NETWORK_TYPE_VOLTE,
    SOUND_CALL_NETWORK_TYPE_MAX
};

enum
{
    SOUND_BLUETOOTH_BANDWIDTH_NONE,
    SOUND_BLUETOOTH_BANDWIDTH_NB,
    SOUND_BLUETOOTH_BANDWIDTH_WB,
    SOUND_BLUETOOTH_BANDWIDTH_MAX
};

enum {
    SOUND_LINK_DIRECTION_NONE = 0x0,
    SOUND_LINK_DIRECTION_UP_LINK = 0x1,
    SOUND_LINK_DIRECTION_DOWN_LINK = 0x2,
};

enum {
    FADE_NONE,
    FADE_IN,
    FADE_OUT,
    FADE_OUT_HALF,
    FADE_MAX
};

static pa_sink *__get_real_master_sink(pa_sink_input *si);
static audio_return_t __fill_audio_playback_stream_info(pa_proplist *sink_input_proplist, pa_sample_spec *sample_spec, audio_info_t *audio_info);
static audio_return_t __fill_audio_playback_device_info(pa_proplist *sink_proplist, audio_info_t *audio_info);
static audio_return_t __fill_audio_playback_info(pa_sink_input *si, audio_info_t *audio_info);
static pa_source *__get_real_master_source(pa_source_output *so);
static audio_return_t __fill_audio_capture_device_info(pa_proplist *source_proplist, audio_info_t *audio_info);
static inline int __compare_device_info(audio_device_info_t *device_info1, audio_device_info_t *device_info2);
static audio_return_t policy_play_sample(struct userdata *u, pa_native_connection *c, const char *name, uint32_t volume_type, uint32_t gain_type, uint32_t volume_level, uint32_t *stream_idx);
static audio_return_t policy_reset(struct userdata *u);
static audio_return_t policy_set_session(struct userdata *u, uint32_t session, uint32_t start);
static audio_return_t policy_set_active_device(struct userdata *u, uint32_t device_in, uint32_t device_out, uint32_t is_start, uint32_t* need_update);
static audio_return_t policy_get_volume_level_max(struct userdata *u, uint32_t volume_type, uint32_t *volume_level);
static audio_return_t __update_volume(struct userdata *u, uint32_t stream_idx, uint32_t volume_type, uint32_t volume_level, uint32_t fade);
static int __set_primary_volume(struct userdata *u, void* key, int volumetype, int is_new);
static audio_return_t policy_get_volume_level(struct userdata *u, uint32_t stream_idx, uint32_t *volume_type, uint32_t *volume_level);
static audio_return_t policy_set_volume_level(struct userdata *u, uint32_t stream_idx, uint32_t volume_type, uint32_t volume_level);
static audio_return_t policy_get_mute(struct userdata *u, uint32_t stream_idx, uint32_t volume_type, uint32_t direction, uint32_t *mute);
static audio_return_t policy_set_mute(struct userdata *u, uint32_t stream_idx, uint32_t volume_type, uint32_t direction, uint32_t mute, uint32_t fade);
#ifdef SUPPORT_HDMI
static int __get_hdmi_support_info(int *hdmi_support, int *channels, int *max_channel);
#endif
#ifdef SUPPORT_UHQA
static void create_uhqa_sink(struct userdata *u, const char* policy);
#endif
static pa_bool_t policy_is_filter (pa_proplist* proplit);
static pa_bool_t policy_is_call_active(struct userdata *u);
static void  policy_stop_sample_continuously(struct userdata *u);

static const char *__get_session_str(uint32_t session)
{
    switch (session) {
        case AUDIO_SESSION_MEDIA:                       return "media";
        case AUDIO_SESSION_VOICECALL:                   return "voicecall";
        case AUDIO_SESSION_VIDEOCALL:                   return "videocall";
        case AUDIO_SESSION_VOIP:                        return "voip";
        case AUDIO_SESSION_FMRADIO:                     return "fmradio";
        case AUDIO_SESSION_CAMCORDER:                   return "camcorder";
        case AUDIO_SESSION_NOTIFICATION:                return "notification";
        case AUDIO_SESSION_ALARM:                       return "alarm";
        case AUDIO_SESSION_EMERGENCY:                   return "emergency";
        case AUDIO_SESSION_VOICE_RECOGNITION:           return "vr";
        case AUDIO_SESSION_PTT:                         return "ptt";
        default:                                        return "invalid";
    }
}

static const char *__get_subsession_str(uint32_t subsession)
{
    switch (subsession) {
        case AUDIO_SUBSESSION_NONE:                     return "none";
        case AUDIO_SUBSESSION_VOICE:                    return "voice";
        case AUDIO_SUBSESSION_RINGTONE:                 return "ringtone";
        case AUDIO_SUBSESSION_MEDIA:                    return "media";
        case AUDIO_SUBSESSION_INIT:                     return "init";
        case AUDIO_SUBSESSION_VR_NORMAL:                return "vr_normal";
        case AUDIO_SUBSESSION_VR_DRIVE:                 return "vr_drive";
        case AUDIO_SUBSESSION_STEREO_REC:               return "stereo_rec";
        case AUDIO_SUBSESSION_STEREO_REC_INTERVIEW:     return "streo_rec_interview";
        case AUDIO_SUBSESSION_STEREO_REC_CONVERSATION:  return "streo_rec_conversation";
        case AUDIO_SUBSESSION_MONO_REC:                 return "mono_rec";
        case AUDIO_SUBSESSION_AM_PLAY:                  return "am_play";
        case AUDIO_SUBSESSION_AM_REC:                   return "am_rec";
        default:                                        return "invalid";
    }
}

static const char *__get_device_in_str(uint32_t device_in)
{
    switch (device_in) {
        case AUDIO_DEVICE_IN_NONE:                      return "none";
        case AUDIO_DEVICE_IN_MIC:                       return "mic";
        case AUDIO_DEVICE_IN_WIRED_ACCESSORY:           return "wired";
        case AUDIO_DEVICE_IN_BT_SCO:                    return "bt_sco";
        default:                                        return "invalid";
    }
}

static const char *__get_device_out_str(uint32_t device_out)
{
    switch (device_out) {
        case AUDIO_DEVICE_OUT_NONE:                     return "none";
        case AUDIO_DEVICE_OUT_SPEAKER:                  return "spk";
        case AUDIO_DEVICE_OUT_RECEIVER:                 return "recv";
        case AUDIO_DEVICE_OUT_WIRED_ACCESSORY:          return "wired";
        case AUDIO_DEVICE_OUT_BT_SCO:                   return "bt_sco";
        case AUDIO_DEVICE_OUT_BT_A2DP:                  return "bt_a2dp";
        case AUDIO_DEVICE_OUT_DOCK:                     return "dock";
        case AUDIO_DEVICE_OUT_HDMI:                     return "hdmi";
        case AUDIO_DEVICE_OUT_MIRRORING:                return "mirror";
        case AUDIO_DEVICE_OUT_USB_AUDIO:                return "usb";
        case AUDIO_DEVICE_OUT_MULTIMEDIA_DOCK:          return "multi_dock";
        default:                                        return "invalid";
    }
}

static void __load_dump_config(struct userdata *u)
{
    dictionary * dict = NULL;
    int vconf_dump = 0;

    dict = iniparser_load(PA_DUMP_INI_DEFAULT_PATH);
    if (!dict) {
        pa_log_debug("%s load failed. Use temporary file", PA_DUMP_INI_DEFAULT_PATH);
        dict = iniparser_load(PA_DUMP_INI_TEMP_PATH);
        if (!dict) {
            pa_log_warn("%s load failed", PA_DUMP_INI_TEMP_PATH);
            return;
        }
    }

    vconf_dump |= iniparser_getboolean(dict, "pcm_dump:decoder_out", 0) ? PA_PCM_DUMP_GST_DECODER_OUT : 0;
    vconf_dump |= iniparser_getboolean(dict, "pcm_dump:sa_sb_in", 0) ? PA_PCM_DUMP_GST_SA_SB_IN : 0;
    vconf_dump |= iniparser_getboolean(dict, "pcm_dump:sa_sb_out", 0) ? PA_PCM_DUMP_GST_SA_SB_OUT : 0;
    vconf_dump |= iniparser_getboolean(dict, "pcm_dump:resampler_in", 0) ? PA_PCM_DUMP_GST_RESAMPLER_IN : 0;
    vconf_dump |= iniparser_getboolean(dict, "pcm_dump:resampler_out", 0) ? PA_PCM_DUMP_GST_RESAMPLER_OUT : 0;
    vconf_dump |= iniparser_getboolean(dict, "pcm_dump:dha_in", 0) ? PA_PCM_DUMP_GST_DHA_IN : 0;
    vconf_dump |= iniparser_getboolean(dict, "pcm_dump:dha_out", 0) ? PA_PCM_DUMP_GST_DHA_OUT : 0;
    vconf_dump |= iniparser_getboolean(dict, "pcm_dump:gst_avsystem_sink", 0) ? PA_PCM_DUMP_GST_AUDIO_SINK_IN : 0;
    vconf_dump |= iniparser_getboolean(dict, "pcm_dump:avsystem_write", 0) ? PA_PCM_DUMP_PA_STREAM_WRITE : 0;
    u->core->pcm_dump |= (pa_bool_t)iniparser_getboolean(dict, "pcm_dump:pa_sink_input", 0) ? PA_PCM_DUMP_PA_SINK_INPUT : 0;
    u->core->pcm_dump |= (pa_bool_t)iniparser_getboolean(dict, "pcm_dump:pa_sink", 0) ? PA_PCM_DUMP_PA_SINK : 0;
    u->core->pcm_dump |= (pa_bool_t)iniparser_getboolean(dict, "pcm_dump:pa_source", 0) ? PA_PCM_DUMP_PA_SOURCE : 0;
    u->core->pcm_dump |= (pa_bool_t)iniparser_getboolean(dict, "pcm_dump:pa_source_output", 0) ? PA_PCM_DUMP_PA_SOURCE_OUTPUT : 0;
    vconf_dump |= iniparser_getboolean(dict, "pcm_dump:avsystem_read", 0) ? PA_PCM_DUMP_PA_STREAM_READ : 0;
    vconf_dump |= iniparser_getboolean(dict, "pcm_dump:gst_avsystem_src", 0) ? PA_PCM_DUMP_GST_AUDIO_SRC_OUT : 0;
    vconf_dump |= iniparser_getboolean(dict, "pcm_dump:sec_record_in", 0) ? PA_PCM_DUMP_GST_SEC_RECORD_IN : 0;
    vconf_dump |= iniparser_getboolean(dict, "pcm_dump:sec_record_out", 0) ? PA_PCM_DUMP_GST_SEC_RECORD_OUT : 0;
    vconf_dump |= iniparser_getboolean(dict, "pcm_dump:encoder_in", 0) ? PA_PCM_DUMP_GST_ENCODER_IN : 0;

    iniparser_freedict(dict);

    if (vconf_set_int(PA_PCM_DUMP_VCONF_KEY, vconf_dump)) {
        pa_log_warn("vconf_set_int %s=%x failed", PA_PCM_DUMP_VCONF_KEY, vconf_dump);
    }
}

static inline pa_bool_t __is_mute_policy(void)
{
    int sound_status = 1;

    /* If sound is mute mode, force ringtone/notification path to headset */
    if (vconf_get_bool(VCONFKEY_SETAPPL_SOUND_STATUS_BOOL, &sound_status)) {
        pa_log_warn("vconf_get_bool for %s failed", VCONFKEY_SETAPPL_SOUND_STATUS_BOOL);
    }

    return (sound_status) ? FALSE : TRUE;
}

static inline pa_bool_t __is_recording(void)
{
    int capture_status = 0;

    /* Check whether audio is recording */
    if (vconf_get_int(VCONFKEY_SOUND_CAPTURE_STATUS, &capture_status)) {
        pa_log_warn("vconf_get_int for %s failed", VCONFKEY_SOUND_CAPTURE_STATUS);
    }

    return (capture_status) ? TRUE : FALSE;
}

static inline pa_bool_t __is_noise_reduction_on(struct userdata *u)
{
#ifdef SUPPORT_NOISE_REDUCTION
    return (u->call_nrec == 1) ? TRUE : FALSE;
#else
    return false;
#endif
}

static bool __is_extra_volume_on(struct userdata *u)
{
    return (u->call_extra_volume == 1) ? TRUE : FALSE;
}

static int __get_dock_type()
{
#ifdef SUPPORT_DOCK
    int dock_status = DOCK_NONE;

    if(vconf_get_int(VCONFKEY_SYSMAN_CRADLE_STATUS, &dock_status)) {
        pa_log_warn("vconf_get_int for %s failed", VCONFKEY_SYSMAN_CRADLE_STATUS);
        return DOCK_NONE;
    } else {
        return dock_status;
    }
#else
    return DOCK_NONE;
#endif
}

static bool __is_wideband(struct userdata *u)
{
    bool ret = false;

    if(u->call_type == SOUND_CALL_NETWORK_TYPE_VOICECALL_WB || u->call_type == SOUND_CALL_NETWORK_TYPE_COMPANION_WB)
        ret = true;
    else
        ret = false;

    return ret;
}

static uint32_t __get_network_type_mask(struct userdata *u)
{
    uint32_t ret = 0;

    if(u->call_type == SOUND_CALL_NETWORK_TYPE_VOICECALL_NB || u->call_type == SOUND_CALL_NETWORK_TYPE_VOICECALL_WB)
        ret =  0;
    else if(u->call_type == SOUND_CALL_NETWORK_TYPE_COMPANION_NB || u->call_type == SOUND_CALL_NETWORK_TYPE_COMPANION_WB)
        ret = AUDIO_ROUTE_FLAG_NETWORK_TYPE_COMPANION;
    else if(u->call_type == SOUND_CALL_NETWORK_TYPE_VOLTE)
        ret = AUDIO_ROUTE_FLAG_NETWORK_TYPE_VOLTE;
    else
        ret = 0;

    return ret;
}

static uint32_t __get_bluetooth_bandwidth_mask(struct userdata *u)
{
    uint32_t ret = 0;

    if(u->bt_bandwidth == SOUND_BLUETOOTH_BANDWIDTH_WB)
        ret = AUDIO_ROUTE_FLAG_BT_WB;

    return ret;
}

static void __set_sink_input_role_type(pa_proplist *p, int gain_type)
{
    if(gain_type == AUDIO_GAIN_TYPE_SHUTTER1 || gain_type == AUDIO_GAIN_TYPE_SHUTTER2 ||
        gain_type == AUDIO_GAIN_TYPE_CAMCODING) {
        pa_proplist_sets (p, PA_PROP_MEDIA_ROLE, "phone");
    } else {
        const char* role = pa_proplist_gets (p, PA_PROP_MEDIA_ROLE);
        if(!role) {
            pa_proplist_sets (p, PA_PROP_MEDIA_ROLE, "music");
        }
    }
    return;
}

static pa_bool_t policy_is_bluez (pa_sink* sink)
{
    const char* api_name = NULL;

    if (sink == NULL) {
        pa_log_warn("input param sink is null");
        return FALSE;
    }

    api_name = pa_proplist_gets(sink->proplist, PA_PROP_DEVICE_API);
    if (api_name) {
        if (pa_streq (api_name, BLUEZ_API))
            return TRUE;
    }

    return FALSE;
}

/* check if this sink is bluez */
static pa_bool_t policy_is_usb_alsa (pa_sink* sink)
{
    const char* api_name = NULL;
    const char* device_bus_name = NULL;

    if (sink == NULL) {
        pa_log_warn("input param sink is null");
        return FALSE;
    }

    api_name = pa_proplist_gets(sink->proplist, PA_PROP_DEVICE_API);
    if (api_name) {
        if (pa_streq (api_name, ALSA_API)) {
            device_bus_name = pa_proplist_gets(sink->proplist, PA_PROP_DEVICE_BUS);
            if (device_bus_name) {
                if (pa_streq (device_bus_name, "usb"))
                    return TRUE;
            }
        }
    }

    return FALSE;
}

/*Get call status*/
static pa_bool_t policy_is_call_active(struct userdata *u)
{
    pa_assert (u);
    char *value = NULL;
    int size;
    int ret_val;
    if (u->audio_mgr.intf.get_route_info)
    {
        u->audio_mgr.intf.get_route_info(u->audio_mgr.data, "call_status", &value, &size);
        if (value) {
            ret_val = atoi(value);
            pa_log_warn("call active : [%d]", ret_val);
            free(value);
            return ret_val;
        }
    }
    return FALSE;
}

/* Get sink by name */
static pa_sink* policy_get_sink_by_name (pa_core *c, const char* sink_name)
{
    return (pa_sink*)pa_namereg_get(c, sink_name, PA_NAMEREG_SINK);
}

/* Get source by name */
static pa_source* policy_get_source_by_name (pa_core *c, const char* source_name)
{
    return (pa_source*)pa_namereg_get(c, source_name, PA_NAMEREG_SOURCE);
}

/* Get bt sink if available */
static pa_sink* policy_get_bt_sink (pa_core *c)
{
    pa_sink *s = NULL;
    uint32_t idx;

    if (c == NULL) {
        pa_log_warn("input param is null");
        return NULL;
    }

    PA_IDXSET_FOREACH(s, c->sinks, idx) {
        if (policy_is_bluez (s)) {
            pa_log_debug_verbose("return [%p] for [%s]\n", s, s->name);
            return s;
        }
    }
    return NULL;
}

/** This function chages the sink from normal sink to UHQA sink if UHQA sink is available*/
static pa_sink* switch_to_uhqa_sink(struct userdata *u, const char* policy)
{
    pa_sink_input *si = NULL;
    uint32_t idx;
    pa_core *c = u->core;
    pa_sink* sink = NULL;
    pa_sink* uhqa_sink = NULL;
    pa_sink* def = pa_namereg_get_default_sink(c);

    /** If default sink is HDMI sink*/
    if (sink_is_hdmi(def)) {
        /** Get the UHQA sink*/
        uhqa_sink = policy_get_sink_by_name(c, SINK_HDMI_UHQA);

        /** Get the normal sink*/
        sink = policy_get_sink_by_name(c, SINK_HDMI);

    /** If high latency UHQA policy means h:0,4 UHQA sink to be selected if h:0,4 UHQA sink already created*/
    } else if (pa_streq(policy, POLICY_HIGH_LATENCY_UHQA)) {

        /** Get the UHQA sink*/
        uhqa_sink = policy_get_sink_by_name(c, SINK_HIGH_LATENCY_UHQA);

        /** Get the normal sink*/
        sink = policy_get_sink_by_name(c, SINK_HIGH_LATENCY);
    } else if (pa_streq(policy, POLICY_AUTO_UHQA)) {   /** If UHQA policy choose UHQA sink*/

        pa_log_info ("---------------------------------");
        /** Get the UHQA sink*/
        uhqa_sink = policy_get_sink_by_name(c, SINK_ALSA_UHQA);

        /** Get the normal sink */
        sink = policy_get_sink_by_name(c, SINK_ALSA);
    }
    pa_log_info ("---------------------------------");
    if (uhqa_sink != NULL) {
        if (sink != NULL) {/** if the sink is null due to some reason ,it need to add protect code */

            /** Check if normal sink is in not suspended state then suspend normal sik so that pcm handle is closed*/
            if (PA_SINK_SUSPENDED != pa_sink_get_state(sink) ) {
                pa_sink_suspend(sink, TRUE, PA_SUSPEND_USER);
            }


            pa_log_info ("---------------------------------");

            /** Check any sink input connected to normal sink then move them to UHQA sink*/
            PA_IDXSET_FOREACH (si, sink->inputs, idx) {

                /* Get role (if role is filter, skip it) */
                if (policy_is_filter(si->proplist)) {
                    continue;
                }
                /* pa_sink_input_move_to(si, uhqa_sink, FALSE);*/
            }
        }


        if (PA_SINK_SUSPENDED == pa_sink_get_state(uhqa_sink)) {
            pa_sink_suspend(uhqa_sink, FALSE, PA_SUSPEND_USER);
        }

        sink = uhqa_sink;
    }

    return sink;
}

/** This function choose normal sink if UHQA sink is in suspended state*/
static pa_sink* switch_to_normal_sink(struct userdata *u, const char* policy)
{
    pa_sink_input *si = NULL;
    uint32_t idx;
    pa_core *c = u->core;
    pa_sink* sink = NULL;
    pa_sink* uhqa_sink = NULL;
    const char *sink_name  = SINK_ALSA;
    pa_sink* def = NULL;

    def = pa_namereg_get_default_sink(c);

    if (pa_streq(policy, POLICY_PHONE) || pa_streq(policy, POLICY_ALL)) {
      /** Get the UHQA sink */
      uhqa_sink = policy_get_sink_by_name (c, SINK_ALSA_UHQA);
    } else if (sink_is_hdmi(def)) {    /** If default sink is HDMI sink*/
        /** Get the UHQA sink handle, if it exists then suspend it if not in use*/
        uhqa_sink = policy_get_sink_by_name (c, SINK_HDMI_UHQA);

        sink_name  = SINK_HDMI;
    } else if(pa_streq(policy, POLICY_HIGH_LATENCY)) {      /** Choose the normal sink based on policy*/
        /** Get the UHQA sink handle, if it exists then suspend it if not in use*/
        uhqa_sink =  policy_get_sink_by_name(c, SINK_HIGH_LATENCY_UHQA);

        sink_name  = SINK_HIGH_LATENCY;
    } else {
        /** Get the UHQA sink */
        uhqa_sink = policy_get_sink_by_name(c, SINK_ALSA_UHQA);
    }

    sink = uhqa_sink;

    /**
      * If UHQA sink is in used or any UHQA sink is connected to UHQA sink then return UHQA sink else return normal sink
      */
    if ((sink != NULL) && pa_sink_used_by(sink)) {
       sink = uhqa_sink;
    } else {
        sink = policy_get_sink_by_name(c, sink_name);

        if (sink != NULL) {/**if the sink is null ,it need to add protect code*/
            /** Move all sink inputs from UHQA sink to normal sink*/
            if (uhqa_sink != NULL) {

                if (PA_SINK_SUSPENDED != pa_sink_get_state(uhqa_sink)) {
                    pa_sink_suspend(uhqa_sink, TRUE, PA_SUSPEND_USER);
                }

                PA_IDXSET_FOREACH (si, uhqa_sink->inputs, idx) {
                    /* Get role (if role is filter, skip it) */
                    if (policy_is_filter(si->proplist)) {
                       continue;
                    }
                   pa_sink_input_move_to(si, sink, FALSE);
               }
            }

            /* if there is sink-input that want to play some sound, sink device would be opened automatically. */
            /*
            if (PA_SINK_SUSPENDED == pa_sink_get_state(policy_get_sink_by_name(c, sink_name)) ) {
                pa_sink_suspend( policy_get_sink_by_name(c, sink_name), FALSE, PA_SUSPEND_USER);
            }
            */
        } else {/** if sink is null,it can not move to the normal sink ,still use uhqa sink */
            pa_log_warn ("The %s sink is null",sink_name);
            sink = uhqa_sink;
        }
    }

    return sink;
}

/* Select sink for given condition */
static pa_sink* policy_select_proper_sink (struct userdata *u, const char* policy, pa_sink_input *sink_input, pa_bool_t check_bt)
{
    pa_core *c = u->core;
    pa_sink *sink = NULL, *bt_sink = NULL, *def, *sink_null;
    pa_sink_input *si = NULL;
    uint32_t idx;
    const char *si_policy_str;
    char *args = NULL;

    pa_assert (u);
    c = u->core;
    pa_assert (c);
    if (policy == NULL) {
        pa_log_warn("input param is null");
        return NULL;
    }

    if (check_bt)
        bt_sink = policy_get_bt_sink(c);

    def = pa_namereg_get_default_sink(c);
    if (def == NULL) {
        pa_log_warn("pa_namereg_get_default_sink() returns null");
        return NULL;
    }

    sink_null = (pa_sink *)pa_namereg_get(u->core, "null", PA_NAMEREG_SINK);
    /* if default sink is set as null sink, we will use null sink */
    if (def == sink_null)
        return def;

    if (u->board_config && pa_streq(u->board_config->codec, AUDIO_CODEC_SPRDPHONE)) { /* this is sprd chipset */
        if ((def == pa_namereg_get(c, SINK_VIRTUAL, PA_NAMEREG_SINK)) || (def == pa_namereg_get(c, SINK_VOIP, PA_NAMEREG_SINK)))
            return def;
    }

    /* Select sink to */
    if (pa_streq(policy, POLICY_ALL)) {
        /* all */
        if (bt_sink) {
            if (u->module_combined == NULL) {
                pa_log_info("combined sink is not prepared, now load-modules...");
                /* load combine sink */
                args = pa_sprintf_malloc("sink_name=%s slaves=\"%s,%s\"", SINK_COMBINED, bt_sink->name, SINK_ALSA);
                u->module_combined = pa_module_load(u->module->core, "module-combine-sink", args);
                pa_xfree(args);

            }
            sink = policy_get_sink_by_name(c, SINK_COMBINED);
        } else {
            sink = switch_to_normal_sink(u, policy);
        }
    } else if (pa_streq(policy, POLICY_PHONE)) {
        /* phone */
        sink = switch_to_normal_sink(u, policy);
    } else if (pa_streq(policy, POLICY_VOIP)) {
        /* VOIP */
        /* NOTE: Check voip sink first, if not available, use AEC sink */
        sink = policy_get_sink_by_name (c,SINK_VOIP);
        if (sink == NULL) {
            pa_log_info("VOIP sink is not available, try to use AEC sink");
            sink = policy_get_sink_by_name (c, AEC_SINK);
            if (sink == NULL) {
                pa_log_info("AEC sink is not available, set to default sink");
                sink = def;
            }
        }
    } else {
        /* auto */
        if (check_bt && policy_is_bluez(def)) {
            sink = def;
        } else if (policy_is_usb_alsa(def)) {
            sink = def;
        } else if (sink_is_hdmi(def)) {
            if (pa_streq(policy, POLICY_AUTO_UHQA) || (pa_streq(policy, POLICY_HIGH_LATENCY_UHQA))) {
                sink = switch_to_uhqa_sink(u,policy);
            }
            else {
                sink = switch_to_normal_sink(u, policy);
            }
        } else {
            pa_bool_t highlatency_exist = 0;
            if ((pa_streq(policy, POLICY_HIGH_LATENCY)) || (pa_streq(policy, POLICY_HIGH_LATENCY_UHQA))) {
                PA_IDXSET_FOREACH(si, c->sink_inputs, idx) {
                    if ((si_policy_str = pa_proplist_gets(si->proplist, PA_PROP_MEDIA_POLICY))) {
                        if (pa_streq(si_policy_str, POLICY_HIGH_LATENCY) && (sink_is_highlatency(si->sink))
                            && (sink_input == NULL || sink_input->index != si->index)) {
                            highlatency_exist = 1;
                            break;
                        }
                    }
                }

                /** If high latency UHQA policy means h:0,4 UHQA sink to be selected if h:0,4 UHQA sink already created*/
                if (pa_streq(policy, POLICY_HIGH_LATENCY_UHQA)) {
                    sink = switch_to_uhqa_sink(u, policy);
                }

                /**
                  * If still sink is null means either policy is high-latency or UHQA sink does not exist
                  * Normal sink need to be selected
                  */
                if (!highlatency_exist && (sink == NULL)) {
                    sink = switch_to_normal_sink(u, policy);
                }
            }

            /** If sink is still null then it is required to choose hw:0,0 UHQA sink or normal sink  based on policy*/
            if (!sink) {
                /** If UHQA policy choose UHQA sink*/
                if (pa_streq(policy, POLICY_AUTO_UHQA)) {
                    sink = switch_to_uhqa_sink(u, policy);
                }

                /** If still no sink selected then select hw:0,0 normal sink this is the default case*/
                if (!sink) {
                    sink = switch_to_normal_sink(u, POLICY_AUTO);
                }
             }
        }
    }

    pa_log_debug_verbose("policy[%s] current default[%s] bt_sink[%s] selected_sink[%s]\n",
            policy, def->name, (bt_sink)? bt_sink->name:"null", (sink)? sink->name:"null");
    return sink;
}

static pa_bool_t policy_is_filter (pa_proplist* proplist)
{
    const char* role = NULL;

    if (proplist == NULL) {
        pa_log_warn("input param proplist is null");
        return FALSE;
    }

    if ((role = pa_proplist_gets(proplist, PA_PROP_MEDIA_ROLE))) {
        if (pa_streq(role, "filter"))
            return TRUE;
    }

    return FALSE;
}

static pa_bool_t volume_is_media (pa_proplist* proplist)
{
    const char* role = NULL;

    if (proplist == NULL) {
        pa_log_warn("input param proplist is null");
        return FALSE;
    }

    if ((role = pa_proplist_gets(proplist, PA_PROP_MEDIA_TIZEN_VOLUME_TYPE))) {
        if (pa_streq(role, "4")) //media
            return TRUE;
    }

    return FALSE;
}

static pa_sink *__get_real_master_sink(pa_sink_input *si)
{
    const char *master_name;
    pa_sink *s, *sink;

    s = (si->origin_sink) ? si->origin_sink : si->sink;
    master_name = pa_proplist_gets(s->proplist, PA_PROP_DEVICE_MASTER_DEVICE);
    if (master_name)
        sink = pa_namereg_get(si->core, master_name, PA_NAMEREG_SINK);
    else
        sink = s;
    return sink;
}

static void __free_audio_stream_info (audio_info_t *audio_info)
{
    pa_xfree(audio_info->stream.name);
}

static void __free_audio_device_info (audio_info_t *audio_info)
{
    if (audio_info->device.api == AUDIO_DEVICE_API_ALSA) {
        pa_xfree(audio_info->device.alsa.card_name);
    } else if (audio_info->device.api == AUDIO_DEVICE_API_BLUEZ) {
        pa_xfree(audio_info->device.bluez.protocol);
    }
    pa_xfree(audio_info->device.name);
}

static void __free_audio_info (audio_info_t *audio_info)
{
    __free_audio_stream_info(audio_info);
    __free_audio_device_info(audio_info);
}

static audio_return_t __fill_audio_playback_stream_info(pa_proplist *sink_input_proplist, pa_sample_spec *sample_spec, audio_info_t *audio_info)
{
    const char *si_volume_type_str, *si_gain_type_str;

    memset(&audio_info->stream, 0x00, sizeof(audio_stream_info_t));

    if (!sink_input_proplist) {
        return AUDIO_ERR_PARAMETER;
    }

    audio_info->stream.name = strdup(pa_strnull(pa_proplist_gets(sink_input_proplist, PA_PROP_MEDIA_NAME)));
    audio_info->stream.samplerate = sample_spec->rate;
    audio_info->stream.channels = sample_spec->channels;
    audio_info->stream.gain_type = AUDIO_GAIN_TYPE_DEFAULT;

    /* Get volume type of sink input */
    if ((si_volume_type_str = pa_proplist_gets(sink_input_proplist, PA_PROP_MEDIA_TIZEN_VOLUME_TYPE))) {
        pa_atou(si_volume_type_str, &audio_info->stream.volume_type);
    } else {
        pa_xfree(audio_info->stream.name);
        return AUDIO_ERR_UNDEFINED;
    }

    /* Get gain type of sink input */
    if ((si_gain_type_str = pa_proplist_gets(sink_input_proplist, PA_PROP_MEDIA_TIZEN_GAIN_TYPE))) {
        pa_atou(si_gain_type_str, &audio_info->stream.gain_type);
    }

    return AUDIO_RET_OK;
}

static audio_return_t __fill_audio_playback_device_info(pa_proplist *sink_proplist, audio_info_t *audio_info)
{
    const char *s_device_api_str;

    memset(&audio_info->device, 0x00, sizeof(audio_device_info_t));

    if (!sink_proplist) {
        return AUDIO_ERR_PARAMETER;
    }

    /* Get device api */
    if ((s_device_api_str = pa_proplist_gets(sink_proplist, PA_PROP_DEVICE_API))) {
        audio_info->device.name = strdup(pa_strnull(pa_proplist_gets(sink_proplist, PA_PROP_DEVICE_STRING)));
        if (pa_streq(s_device_api_str, "alsa")) {
            const char *card_idx_str, *device_idx_str;

            audio_info->device.api = AUDIO_DEVICE_API_ALSA;
            audio_info->device.direction = AUDIO_DIRECTION_OUT;
            audio_info->device.alsa.card_name = strdup(pa_strnull(pa_proplist_gets(sink_proplist, "alsa.card_name")));
            audio_info->device.alsa.card_idx = 0;
            audio_info->device.alsa.device_idx = 0;
            if ((card_idx_str = pa_proplist_gets(sink_proplist, "alsa.card")))
                pa_atou(card_idx_str, &audio_info->device.alsa.card_idx);
            if ((device_idx_str = pa_proplist_gets(sink_proplist, "alsa.device")))
                pa_atou(device_idx_str, &audio_info->device.alsa.device_idx);
        }
        else if (pa_streq(s_device_api_str, "bluez")) {
            const char *nrec_str;

            audio_info->device.api = AUDIO_DEVICE_API_BLUEZ;
            audio_info->device.bluez.nrec = 0;
            audio_info->device.bluez.protocol = strdup(pa_strnull(pa_proplist_gets(sink_proplist, "bluetooth.protocol")));
            if ((nrec_str = pa_proplist_gets(sink_proplist, "bluetooth.nrec")))
                pa_atou(nrec_str, &audio_info->device.bluez.nrec);
        }
    }

    return AUDIO_RET_OK;
}

static audio_return_t __fill_audio_playback_info(pa_sink_input *si, audio_info_t *audio_info)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    pa_sink *sink;

    sink = __get_real_master_sink(si);
    if (AUDIO_IS_ERROR((audio_ret = __fill_audio_playback_stream_info(si->proplist, &si->sample_spec, audio_info)))) {
        return audio_ret;
    }
    if (AUDIO_IS_ERROR((audio_ret = __fill_audio_playback_device_info(sink->proplist, audio_info)))) {
        __free_audio_stream_info(audio_info);
        return audio_ret;
    }

    return AUDIO_RET_OK;
}

static pa_source *__get_real_master_source(pa_source_output *so)
{
    const char *master_name;
    pa_source *s, *source;

    s = (so->destination_source) ? so->destination_source : so->source;
    master_name = pa_proplist_gets(s->proplist, PA_PROP_DEVICE_MASTER_DEVICE);
    if (master_name)
        source = pa_namereg_get(so->core, master_name, PA_NAMEREG_SINK);
    else
        source = s;
    return source;
}

#if 0 //fix the building warning, as this function not used now
static audio_return_t __fill_audio_capture_stream_info(pa_proplist *source_output_proplist, pa_sample_spec *sample_spec, audio_info_t *audio_info)
{
    if (!source_output_proplist) {
        return AUDIO_ERR_PARAMETER;
    }

    audio_info->stream.name = strdup(pa_strnull(pa_proplist_gets(source_output_proplist, PA_PROP_MEDIA_NAME)));
    audio_info->stream.samplerate = sample_spec->rate;
    audio_info->stream.channels = sample_spec->channels;

    return AUDIO_RET_OK;
}
#endif

static audio_return_t __fill_audio_capture_device_info(pa_proplist *source_proplist, audio_info_t *audio_info)
{
    const char *s_device_api_str;

    if (!source_proplist) {
        return AUDIO_ERR_PARAMETER;
    }

    memset(&audio_info->device, 0x00, sizeof(audio_device_info_t));

    /* Get device api */
    if ((s_device_api_str = pa_proplist_gets(source_proplist, PA_PROP_DEVICE_API))) {
        audio_info->device.name = strdup(pa_strnull(pa_proplist_gets(source_proplist, PA_PROP_DEVICE_STRING)));
        if (pa_streq(s_device_api_str, "alsa")) {
            const char *card_idx_str, *device_idx_str;

            audio_info->device.api = AUDIO_DEVICE_API_ALSA;
            audio_info->device.direction = AUDIO_DIRECTION_IN;
            audio_info->device.alsa.card_name = strdup(pa_strnull(pa_proplist_gets(source_proplist, "alsa.card_name")));
            audio_info->device.alsa.card_idx = 0;
            audio_info->device.alsa.device_idx = 0;
            if ((card_idx_str = pa_proplist_gets(source_proplist, "alsa.card")))
                pa_atou(card_idx_str, &audio_info->device.alsa.card_idx);
            if ((device_idx_str = pa_proplist_gets(source_proplist, "alsa.device")))
                pa_atou(device_idx_str, &audio_info->device.alsa.device_idx);
        }
        else if (pa_streq(s_device_api_str, "bluez")) {
            const char *nrec_str;

            audio_info->device.api = AUDIO_DEVICE_API_BLUEZ;
            audio_info->device.bluez.nrec = 0;
            audio_info->device.bluez.protocol = strdup(pa_strnull(pa_proplist_gets(source_proplist, "bluetooth.protocol")));
            if ((nrec_str = pa_proplist_gets(source_proplist, "bluetooth.nrec")))
                pa_atou(nrec_str, &audio_info->device.bluez.nrec);
        }
    }

    return AUDIO_RET_OK;
}

#if 0 //fix the building warning, as this function not used now
static audio_return_t __fill_audio_capture_info(pa_source_output *so, audio_info_t *audio_info)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    pa_source *source;

    source = __get_real_master_source(so);
    if (AUDIO_IS_ERROR((audio_ret = __fill_audio_capture_stream_info(so->proplist, &so->sample_spec, audio_info)))) {
        return audio_ret;
    }
    if (AUDIO_IS_ERROR((audio_ret = __fill_audio_capture_device_info(source->proplist, audio_info)))) {
        return audio_ret;
    }

    return AUDIO_RET_OK;
}
#endif

#define PROP_POLICY_CORK "policy_cork_by_device_switch"

static inline int __compare_device_info (audio_device_info_t *device_info1, audio_device_info_t *device_info2)
{
    if (device_info1->direction != device_info2->direction)
        return FALSE;

    pa_log_debug("card_name(%s), card_name2(%s), card_idx(%d), card_idx2(%d), device_idx(%d), device_idx2(%d)",
        pa_strnull(device_info1->alsa.card_name), pa_strnull(device_info2->alsa.card_name),
        device_info1->alsa.card_idx, device_info2->alsa.card_idx,
        device_info1->alsa.device_idx, device_info2->alsa.device_idx);

    if (device_info1->api == AUDIO_DEVICE_API_ALSA) {
        if ((!strcmp(device_info1->alsa.card_name, device_info2->alsa.card_name) || (device_info1->alsa.card_idx == device_info2->alsa.card_idx))
            && (device_info1->alsa.device_idx == device_info2->alsa.device_idx)) {
            return TRUE;
        }
    }
    if (device_info1->api == AUDIO_DEVICE_API_BLUEZ) {
        if (!strcmp(device_info1->bluez.protocol, device_info2->bluez.protocol) && (device_info1->bluez.nrec == device_info2->bluez.nrec)) {
            return TRUE;
        }
    }

    return FALSE;
}

static inline const char *__get_event_type_string (pa_hal_event_type_t hal_event_type)
{
    switch (hal_event_type) {
        case PA_HAL_EVENT_NONE:                 return "none";
        case PA_HAL_EVENT_LOAD_DEVICE:          return "load device";
        case PA_HAL_EVENT_OPEN_DEVICE:          return "open device";
        case PA_HAL_EVENT_CLOSE_ALL_DEVICES:    return "close all devices";
        case PA_HAL_EVENT_CLOSE_DEVICE:         return "close device";
        case PA_HAL_EVENT_UNLOAD_DEVICE:        return "unload device";
#ifdef FM_BT
        case PA_HAL_EVENT_ROUTE_BT_FM:          return "route fm to bt";
        case PA_HAL_EVENT_UNROUTE_BT_FM:        return "unroute fm to bt";
#endif
        default:                                return "undefined";
    }
}

static audio_return_t __load_n_open_device (struct userdata *u, audio_device_info_t *device_info, audio_device_param_info_t *params, pa_hal_event_type_t hal_event_type)
{
    audio_info_t audio_info;
    bool is_module_loaded = false;
    pa_strbuf *name_buf, *args_buf, *prop_buf;
    char *name = NULL, *args = NULL, *prop = NULL;
    pa_source *source;
    pa_sink *sink;
    const char *device_object_str = NULL, *dirction_str = NULL;
    const char *is_manual_corked_str = NULL;
    uint32_t is_manual_corked = 0;
    int i;
    bool is_param_set[AUDIO_DEVICE_PARAM_MAX] = {false, };
    uint32_t idx;

    pa_assert(u);
    pa_assert(device_info);
    pa_assert(device_info->direction == AUDIO_DIRECTION_IN || device_info->direction == AUDIO_DIRECTION_OUT);

     if (device_info->direction == AUDIO_DIRECTION_IN) {
        PA_IDXSET_FOREACH(source, u->core->sources, idx) {
            if (!AUDIO_IS_ERROR(__fill_audio_capture_device_info(source->proplist, &audio_info))) {
                if (__compare_device_info(&audio_info.device, device_info)) {
                    is_module_loaded = true;
                    __free_audio_device_info(&audio_info);
                    break;
                }
                __free_audio_device_info(&audio_info);
            }
        }
        device_object_str = "source";
        dirction_str = "input";
    } else {
        PA_IDXSET_FOREACH(sink, u->core->sinks, idx) {
            if (!AUDIO_IS_ERROR(__fill_audio_playback_device_info(sink->proplist, &audio_info))) {
                if (__compare_device_info(&audio_info.device, device_info)) {
                    is_module_loaded = true;
                    __free_audio_device_info(&audio_info);
                    break;
                }
                __free_audio_device_info(&audio_info);
            }
        }
        device_object_str = "sink";
        dirction_str = "output";
    }

    name_buf = pa_strbuf_new();
    if (device_info->api == AUDIO_DEVICE_API_ALSA) {
        if (device_info->alsa.card_name &&
                (!strncmp(device_info->alsa.card_name, ALSA_VIRTUAL_CARD, strlen(ALSA_VIRTUAL_CARD))) ||
                (!strncmp(device_info->alsa.card_name, ALSA_VIRTUAL_CARD_LTE, strlen(ALSA_VIRTUAL_CARD_LTE)))) {
            pa_strbuf_printf(name_buf, "%s", (device_info->direction == AUDIO_DIRECTION_OUT) ? SINK_VIRTUAL : SOURCE_VIRTUAL);
        } else if (device_info->alsa.card_name && !strncmp(device_info->alsa.card_name, ALSA_SAUDIOVOIP_CARD, strlen(ALSA_SAUDIOVOIP_CARD))) {
            pa_strbuf_printf(name_buf, "%s", (device_info->direction == AUDIO_DIRECTION_OUT) ? SINK_VOIP : SOURCE_VOIP);
        } else {
            pa_strbuf_printf(name_buf, "alsa_%s.%d.analog-stereo", dirction_str, device_info->alsa.device_idx);
        }
    } else if (device_info->api == AUDIO_DEVICE_API_BLUEZ) {
        /* HAL_TODO : do we need to consider BLUEZ here? */
        pa_strbuf_printf(name_buf, "bluez_%s.%s", device_object_str, device_info->bluez.protocol);
    } else {
        pa_strbuf_printf(name_buf, "unknown_%s", device_object_str);
    }
    name = pa_strbuf_tostring_free(name_buf);
    if (!name) {
        pa_log_error("invalid module name");
        return AUDIO_ERR_PARAMETER;
    }

    /* load module if is not loaded */
    if (is_module_loaded == false) {
        args_buf = pa_strbuf_new();
        prop_buf = pa_strbuf_new();

        pa_strbuf_printf(args_buf, "%s_name=\"%s\" ", device_object_str, name);
        if (device_info->name) {
            pa_strbuf_printf(args_buf, "device=\"%s\" ", device_info->name);
        }

        for (i = 0; i < AUDIO_DEVICE_PARAM_MAX; i++) {
            if (params[i].param == AUDIO_DEVICE_PARAM_NONE)
                break;
            is_param_set[params[i].param] = true;
            if (params[i].param == AUDIO_DEVICE_PARAM_CHANNELS) {
                pa_strbuf_printf(args_buf, "channels=\"%d\" ", params[i].u32_v);
            } else if (params[i].param == AUDIO_DEVICE_PARAM_SAMPLERATE) {
                pa_strbuf_printf(args_buf, "rate=\"%d\" ", params[i].u32_v);
            } else if (params[i].param == AUDIO_DEVICE_PARAM_FRAGMENT_SIZE) {
                pa_strbuf_printf(args_buf, "fragment_size=\"%d\" ", params[i].u32_v);
            } else if (params[i].param == AUDIO_DEVICE_PARAM_FRAGMENT_NB) {
                pa_strbuf_printf(args_buf, "fragments=\"%d\" ", params[i].u32_v);
            } else if (params[i].param == AUDIO_DEVICE_PARAM_START_THRESHOLD) {
                pa_strbuf_printf(args_buf, "start_threshold=\"%d\" ", params[i].s32_v);
            } else if (params[i].param == AUDIO_DEVICE_PARAM_USE_MMAP) {
                pa_strbuf_printf(args_buf, "mmap=\"%d\" ", params[i].u32_v);
            } else if (params[i].param == AUDIO_DEVICE_PARAM_USE_TSCHED) {
                pa_strbuf_printf(args_buf, "tsched=\"%d\" ", params[i].u32_v);
            } else if (params[i].param == AUDIO_DEVICE_PARAM_TSCHED_BUF_SIZE) {
                pa_strbuf_printf(args_buf, "tsched_buffer_size=\"%d\" ", params[i].u32_v);
            } else if (params[i].param == AUDIO_DEVICE_PARAM_SUSPEND_TIMEOUT) {
                pa_strbuf_printf(prop_buf, "module-suspend-on-idle.timeout=%d ", params[i].u32_v);
            } else if (params[i].param == AUDIO_DEVICE_PARAM_ALTERNATE_RATE) {
                pa_strbuf_printf(args_buf, "alternate_rate=\"%d\" ", params[i].u32_v);
            } else if (params[i].param == AUDIO_DEVICE_PARAM_VOIP_DEVICE) {
                pa_strbuf_printf(args_buf, "voip_device=\"%d\" ", params[i].u32_v);
            }
        }

        if (device_info->skip_pcm_rewind) {
            pa_strbuf_printf(prop_buf, "%s=1 ", PA_PROP_MEDIA_TIZEN_SKIP_PCM_REWIND);
        }

        if (device_info->direction == AUDIO_DIRECTION_IN) {
            pa_module *module_source = NULL;

            if (!is_param_set[AUDIO_DEVICE_PARAM_FRAGMENT_SIZE]) {
                pa_strbuf_printf(args_buf, "fragment_size=\"%d\" ", (u->fragment_size) ? u->fragment_size : DEFAULT_FRAGMENT_SIZE);
            }
            if ((prop = pa_strbuf_tostring_free(prop_buf))) {
                pa_strbuf_printf(args_buf, "source_properties=\"%s\" ", prop);
            }

            args = pa_strbuf_tostring_free(args_buf);

            if (device_info->api == AUDIO_DEVICE_API_ALSA) {
                module_source = pa_module_load(u->core, "module-alsa-source", args);
            } else if (device_info->api == AUDIO_DEVICE_API_BLUEZ) {
                module_source = pa_module_load(u->core, "module-bluez-source", args);
            }

            if (!module_source) {
                pa_log_error("load source module failed. api:%d args:%s", device_info->api, args);
            }
        } else {
            pa_module *module_sink = NULL;

            if (!is_param_set[AUDIO_DEVICE_PARAM_TSCHED_BUF_SIZE]) {
                pa_strbuf_printf(args_buf, "tsched_buffer_size=\"%d\" ", (u->tsched_buffer_size) ? u->tsched_buffer_size : DEFAULT_TSCHED_BUFFER_SIZE);
            }
            if ((prop = pa_strbuf_tostring_free(prop_buf))) {
                pa_strbuf_printf(args_buf, "sink_properties=\"%s\" ", prop);
            }

            args = pa_strbuf_tostring_free(args_buf);

            if (device_info->api == AUDIO_DEVICE_API_ALSA) {
                module_sink = pa_module_load(u->core, "module-alsa-sink", args);
            } else if (device_info->api == AUDIO_DEVICE_API_BLUEZ) {
                module_sink = pa_module_load(u->core, "module-bluez-sink", args);
            }

            if (!module_sink) {
                pa_log_error("load sink module failed. api:%d args:%s", device_info->api, args);
            }
        }
    }

    if (hal_event_type == PA_HAL_EVENT_LOAD_DEVICE) {
        goto exit;
    }

    if (device_info->direction == AUDIO_DIRECTION_IN) {
        /* set default source */
        pa_source *source_default = pa_namereg_get_default_source(u->core);
        pa_source *source_null = (pa_source *)pa_namereg_get(u->core, "source.null", PA_NAMEREG_SOURCE);

        if (source_default == source_null || device_info->is_default_device) {
            if ((source = pa_namereg_get(u->core, name, PA_NAMEREG_SOURCE))) {
                pa_source_output *so;

                if (source != source_default) {
                    pa_namereg_set_default_source(u->core, source);
                    pa_log_info("set %s as default source", name);
                }

                PA_IDXSET_FOREACH(so, u->core->source_outputs, idx) {
                    if (!so->source)
                        continue;
                    /* Get role (if role is filter, skip it) */
                    if (policy_is_filter(so->proplist))
                        continue;

                    pa_source_output_move_to(so, source, FALSE);

                    /* UnCork if corked by manually */
                    if ((is_manual_corked_str = pa_proplist_gets(so->proplist, PROP_POLICY_CORK))) {
                        pa_atou(is_manual_corked_str, &is_manual_corked);
                        if (is_manual_corked) {
                            pa_proplist_sets(so->proplist, PROP_POLICY_CORK, "0");
                            pa_source_output_cork(so, FALSE);
                            pa_log_info("<UnCork> for source-input[%d]:source[%s]", so->index, so->source->name);
                        }
                    }
                }
            }
        }
    } else {
        /* set default sink */
        pa_sink *sink_default = pa_namereg_get_default_sink(u->core);
        pa_sink *sink_null = (pa_sink *)pa_namereg_get(u->core, "null", PA_NAMEREG_SINK);

        if (sink_default == sink_null || device_info->is_default_device) {
            if ((sink = pa_namereg_get(u->core, name, PA_NAMEREG_SINK))) {
                pa_sink_input *si;

                if (sink != sink_default && device_info->is_default_device) {
                    pa_namereg_set_default_sink(u->core, sink);
                    pa_log_info("set %s as default sink. default_sink(%s), active_sink(%s)", name, sink_default->name, sink->name);
                }

                PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {
                    if (!si->sink)
                        continue;

                    /* Get role (if role is filter, skip it) */
                    if (policy_is_filter(si->proplist))
                        continue;

                    if (sink != sink_default && device_info->is_default_device) {
                        if (volume_is_media(si->proplist) && (!strcmp(sink->name, SINK_VIRTUAL)) && (pa_atomic_load(&si->thread_info.fade_onff) == true)) {
                            /* UnCork if corked by manually */
                            if ((is_manual_corked_str = pa_proplist_gets(si->proplist, PROP_POLICY_CORK))) {
                                pa_atou(is_manual_corked_str, &is_manual_corked);
                                if (is_manual_corked) {
                                    pa_proplist_sets(si->proplist, PROP_POLICY_CORK, "0");
                                    pa_sink_input_cork(si, FALSE);
                                    pa_log_info("<UnCork> for sink-input[%d]:sink[%s]", si->index, si->sink->name);
                                }
                            }
                            pa_log_info("call start but fading media stream still alive skip move sink-input");
                            continue;
                        }

                        pa_sink_input_move_to(si, sink, FALSE);
                    }

                    /* UnCork if corked by manually */
                    if ((is_manual_corked_str = pa_proplist_gets(si->proplist, PROP_POLICY_CORK))) {
                        pa_atou(is_manual_corked_str, &is_manual_corked);
                        if (is_manual_corked) {
                            pa_proplist_sets(si->proplist, PROP_POLICY_CORK, "0");
                            pa_sink_input_cork(si, FALSE);
                            pa_log_info("<UnCork> for sink-input[%d]:sink[%s]", si->index, si->sink->name);
                        }
                    }
                }
            }
        }
    }

exit:
    if(name)
        pa_xfree(name);
    if(args)
        pa_xfree(args);
    if(prop)
        pa_xfree(prop);

    return AUDIO_RET_OK;
}
static int __is_loopback()
{
    int loopback = 0;
#ifndef PROFILE_WEARABLE
    if (vconf_get_int("memory/factory/loopback", &loopback)) {
        pa_log_warn("vconf_get_int for %s failed", "memory/factory/loopback");
    }

    return (loopback) ? 1 : 0;
#else
    return 0;
#endif
}

static int __is_call_session(int session)
{
    if(session == AUDIO_SESSION_VOICECALL || session == AUDIO_SESSION_VIDEOCALL || session == AUDIO_SESSION_PTT || session == AUDIO_SESSION_VOIP)
        return 1;
    return 0;
}

static audio_return_t __load_n_open_device_callback (void *platform_data, audio_device_info_t *device_info, audio_device_param_info_t *params, pa_hal_event_type_t hal_event_type)
{
    struct userdata *u = (struct userdata *)platform_data;
    struct pa_hal_device_event_data *event_data;
    const char dummy = 'W';

    pa_assert(u);
    pa_assert(device_info);
    pa_assert(device_info->direction == AUDIO_DIRECTION_IN || device_info->direction == AUDIO_DIRECTION_OUT);
    pa_assert(hal_event_type == PA_HAL_EVENT_LOAD_DEVICE || hal_event_type == PA_HAL_EVENT_OPEN_DEVICE);

    if (u->tid != pthread_self()) {
        /* called from thread */
        pa_log_debug("%s is called within thread", __get_event_type_string(hal_event_type));

        pa_mutex_lock(u->defer.mutex);

        u->defer.event_type = hal_event_type;
        event_data = pa_xmalloc(sizeof(struct pa_hal_device_event_data));
        memcpy(&event_data->device_info, device_info, sizeof(audio_device_info_t));
        memcpy(&event_data->params[0], params, sizeof(audio_device_param_info_t) * AUDIO_DEVICE_PARAM_MAX);
        u->defer.event_data = (void *)event_data;

        /* trigger will be called because sub thread must not to access to mainloop context */
        write(u->fd_defer[1], &dummy, sizeof(dummy));

        pa_cond_wait(u->defer.cond, u->defer.mutex);
        pa_xfree(u->defer.event_data);
        u->defer.event_data = NULL;
        pa_mutex_unlock(u->defer.mutex);

        pa_log_info("%s is finished within thread", __get_event_type_string(hal_event_type));
    } else {
        /* called from mainloop */
        pa_log_debug("%s is called within mainloop", __get_event_type_string(hal_event_type));
        __load_n_open_device(u, device_info, params, hal_event_type);
        pa_log_info("%s is finished within mainloop", __get_event_type_string(hal_event_type));
    }

    return AUDIO_RET_OK;
}

static audio_return_t __load_device_callback (void *platform_data, audio_device_info_t *device_info, audio_device_param_info_t *params)
{
    return __load_n_open_device_callback(platform_data, device_info, params, PA_HAL_EVENT_LOAD_DEVICE);
}

static audio_return_t __open_device_callback (void *platform_data, audio_device_info_t *device_info, audio_device_param_info_t *params)
{
    return __load_n_open_device_callback(platform_data, device_info, params, PA_HAL_EVENT_OPEN_DEVICE);
}

static audio_return_t __close_all_devices (struct userdata *u)
{
    pa_source *source_null, *source;
    pa_source_output *so;
    pa_sink *sink_null, *sink;
    pa_sink_input *si;
    uint32_t idx;

    pa_assert(u);

    /* set default sink/src as null */
    source_null = (pa_source *)pa_namereg_get(u->core, "source.null", PA_NAMEREG_SOURCE);
    pa_namereg_set_default_source(u->core, source_null);
    sink_null = (pa_sink *)pa_namereg_get(u->core, "null", PA_NAMEREG_SINK);
    pa_namereg_set_default_sink(u->core, sink_null);

    /* close input devices */
    PA_IDXSET_FOREACH(so, u->core->source_outputs, idx) {
        if (!so->source)
            continue;
        /* Get role (if role is filter, skip it) */
        if (policy_is_filter(so->proplist))
            continue;
        /* Cork only if source-output was Running */
        if (pa_source_output_get_state(so) == PA_SOURCE_OUTPUT_RUNNING) {
            pa_proplist_sets(so->proplist, PROP_POLICY_CORK, "1");
            pa_source_output_cork(so, TRUE);
            pa_log_info("<Cork> for source-output[%d]:source[%s]", so->index, so->source->name);
        }
        if (source_null)
            pa_source_output_move_to(so, source_null, FALSE);
    }
    PA_IDXSET_FOREACH(source, u->core->sources, idx) {
        pa_source_suspend(source, TRUE, PA_SUSPEND_SWITCH);
    }

    /* close output devices */
    PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {
        if (!si->sink)
            continue;
        /* Get role (if role is filter, skip it) */
        if (policy_is_filter(si->proplist))
            continue;
        /* Cork only if sink-input was Running */
        if (pa_sink_input_get_state(si) == PA_SINK_INPUT_RUNNING) {
            pa_proplist_sets(si->proplist, PROP_POLICY_CORK, "1");
            pa_sink_input_cork(si, TRUE);
            pa_log_info("<Cork> for sink-input[%d]:sink[%s]", si->index, si->sink->name);
        }
        if (sink_null)
            pa_sink_input_move_to(si, sink_null, FALSE);
    }
    PA_IDXSET_FOREACH(sink, u->core->sinks, idx) {
        pa_sink_suspend(sink, TRUE, PA_SUSPEND_SWITCH);
    }
    if (u->audio_sample_userdata.is_running == TRUE) {
        policy_stop_sample_continuously(u);
    }

    return AUDIO_RET_OK;
}

static audio_return_t __close_all_devices_callback (void *platform_data)
{
    const char dummy = 'W';
    struct userdata *u = (struct userdata *)platform_data;

    pa_assert(u);

    if (u->tid != pthread_self()) {
        /* called from thread */
        pa_log_debug("%s is called within thread", __get_event_type_string(PA_HAL_EVENT_CLOSE_ALL_DEVICES));

        pa_mutex_lock(u->defer.mutex);

        u->defer.event_type = PA_HAL_EVENT_CLOSE_ALL_DEVICES;
        u->defer.event_data = NULL;

        /* trigger will be called because sub thread must not to access to mainloop context */
        write(u->fd_defer[1], &dummy, sizeof(dummy));

        pa_cond_wait(u->defer.cond, u->defer.mutex);
        pa_mutex_unlock(u->defer.mutex);

        pa_log_info("%s is finished within thread", __get_event_type_string(PA_HAL_EVENT_CLOSE_ALL_DEVICES));
    } else {
        /* called from mainloop */
        pa_log_debug("%s is called within mainloop", __get_event_type_string(PA_HAL_EVENT_CLOSE_ALL_DEVICES));
        __close_all_devices(u);
        pa_log_info("%s is finished within mainloop", __get_event_type_string(PA_HAL_EVENT_CLOSE_ALL_DEVICES));
    }

    return AUDIO_RET_OK;
}

#ifdef FM_BT
void __route_fm_to_bt(struct userdata *u)
{
    char *args = NULL;
    if (!u) {
        pa_log_error("userdata is NULL!!!");
        return;
    }
    if (u->session == AUDIO_SESSION_FMRADIO && !u->loopback_fm) {
        /*Session is fm radio*/
        pa_sink *pbluez_sink = NULL;
        pa_source *palsa_source = NULL;
        pbluez_sink = policy_get_bt_sink(u->core);
        palsa_source = pa_namereg_get_default_source(u->core);
        if (pbluez_sink && palsa_source) {
            args = pa_sprintf_malloc("source=%s sink=%s sink_input_properties=\"media.role=music media.policy=auto media.tizen_volume_type=4\" rate=%d adjust_time=0 source_dont_move=\"true\" sink_dont_move=\"true\"", palsa_source->name, pbluez_sink->name, palsa_source->sample_spec.rate);
            u->loopback_fm = pa_module_load(u->core, "module-loopback", args);
            pa_log_info("Module loopback alsa->BT sink- %s %x", args, u->loopback_fm);
            pa_xfree(args);
            pa_sink_set_mute(pbluez_sink, FALSE, TRUE);
        }
        else {
            pa_log_error("can't find sink or source BT - %x Alsa - %x", pbluez_sink, palsa_source);
            return;
        }
    } else {
        pa_log_error("already routed");
    }
    return;
}

static void __unroute_fm_to_bt(struct userdata *u)
{
    pa_log_info("unroute fm to bt");

    if (u->loopback_fm)
    {
        pa_sink *pbluez_sink = NULL;
        pbluez_sink = policy_get_bt_sink(u->core);
        if (pbluez_sink == NULL) {
            pa_log_info("bluez_sink is NULL. loopback module is unloaded by Bluetooth sink free");
            u->loopback_fm = NULL;
            return;
        }

        if (u->loopback_fm->core == NULL) {
            pa_log_warn("loopback_fm->core is NULL. loopback module is already unloaded");
            u->loopback_fm = NULL;
            return;
        }

        pa_log_info("try to unload loopback module");
        pa_module_unload(u->core, u->loopback_fm, true);
        u->loopback_fm = NULL;
        pa_log_info("loopback module is unloaded");
    } else {
        pa_log_warn("u->loopback_fm is NULL");
    }
}

static audio_return_t __route_fm_to_bt_callback (void *platform_data)
{
    struct userdata *u = (struct userdata *)platform_data;
    struct pa_hal_event *hal_event;
    const char dummy = 'W';

    pa_assert(u);

    if (u->tid != pthread_self()) {
        /* called from thread */
        pa_log_debug("%s is called within thread", __get_event_type_string(PA_HAL_EVENT_ROUTE_BT_FM));

        pa_mutex_lock(u->defer.mutex);
        u->defer.event_type = PA_HAL_EVENT_ROUTE_BT_FM;

        /* trigger will be called because sub thread must not to access to mainloop context */
        write(u->fd_defer[1], &dummy, sizeof(dummy));

        pa_cond_wait(u->defer.cond, u->defer.mutex);
        pa_mutex_unlock(u->defer.mutex);

        pa_log_info("%s is finished within thread", __get_event_type_string(PA_HAL_EVENT_ROUTE_BT_FM));
    } else {
        /* called from mainloop */
        pa_log_debug("%s is called within mainloop", __get_event_type_string(PA_HAL_EVENT_ROUTE_BT_FM));
        __route_fm_to_bt(u);
        pa_log_info("%s is finished within mainloop", __get_event_type_string(PA_HAL_EVENT_ROUTE_BT_FM));
    }

    return AUDIO_RET_OK;
}

static audio_return_t __unroute_fm_to_bt_callback (void *platform_data)
{
    struct userdata *u = (struct userdata *)platform_data;
    const char dummy = 'W';

    pa_assert(u);

    if (u->tid != pthread_self()) {
        /* called from thread */
        pa_log_debug("%s is called within thread", __get_event_type_string(PA_HAL_EVENT_UNROUTE_BT_FM));

        pa_mutex_lock(u->defer.mutex);
        u->defer.event_type = PA_HAL_EVENT_UNROUTE_BT_FM;

        /* trigger will be called because sub thread must not to access to mainloop context */
        write(u->fd_defer[1], &dummy, sizeof(dummy));

        pa_cond_wait(u->defer.cond, u->defer.mutex);
        pa_mutex_unlock(u->defer.mutex);

        pa_log_info("%s is finished within thread", __get_event_type_string(PA_HAL_EVENT_UNROUTE_BT_FM));
    } else {
        /* called from mainloop */
        pa_log_debug("%s is called within mainloop", __get_event_type_string(PA_HAL_EVENT_UNROUTE_BT_FM));
        __unroute_fm_to_bt(u);
        pa_log_info("%s is finished within mainloop", __get_event_type_string(PA_HAL_EVENT_UNROUTE_BT_FM));
    }

    return AUDIO_RET_OK;
}
#endif

static audio_return_t __close_n_unload_device (struct userdata *u, audio_device_info_t *device_info, pa_hal_event_type_t hal_event_type)
{
    audio_info_t audio_info;
    bool is_module_loaded = false;
    pa_source *source_null, *source;
    pa_source_output *so;
    pa_sink *sink_null, *sink;
    pa_sink_input *si;
    uint32_t idx;

    pa_assert(u);
    pa_assert(device_info);
    pa_assert(device_info->direction == AUDIO_DIRECTION_IN || device_info->direction == AUDIO_DIRECTION_OUT);

     if (device_info->direction == AUDIO_DIRECTION_IN) {
        PA_IDXSET_FOREACH(source, u->core->sources, idx) {
            if (!AUDIO_IS_ERROR(__fill_audio_capture_device_info(source->proplist, &audio_info))) {
                if (__compare_device_info(&audio_info.device, device_info)) {
                    is_module_loaded = true;
                    __free_audio_device_info(&audio_info);
                    break;
                }
            }
            __free_audio_device_info(&audio_info);
        }

        if (is_module_loaded) {
            source_null = (pa_source *)pa_namereg_get(u->core, "source.null", PA_NAMEREG_SOURCE);
            if (pa_namereg_get_default_source(u->core) == source) {
                pa_namereg_set_default_source(u->core, source_null);
            }

            PA_IDXSET_FOREACH(so, u->core->source_outputs, idx) {
                if (__get_real_master_source(so) != source)
                    continue;
                /* Get role (if role is filter, skip it) */
                if (policy_is_filter(so->proplist))
                    continue;
                /* Cork only if source-output was Running */
                if (pa_source_output_get_state(so) == PA_SOURCE_OUTPUT_RUNNING) {
                    pa_proplist_sets(so->proplist, PROP_POLICY_CORK, "1");
                    pa_source_output_cork(so, TRUE);
                    pa_log_info("<Cork> for source-output[%d]:source[%s]", so->index, so->source->name);
                }
                if (source_null)
                    pa_source_output_move_to(so, source_null, FALSE);
            }
            pa_source_suspend(source, TRUE, PA_SUSPEND_SWITCH);

            if (hal_event_type == PA_HAL_EVENT_UNLOAD_DEVICE) {
                pa_module_unload(u->core, source->module, TRUE);
            }
        }
    } else {
        PA_IDXSET_FOREACH(sink, u->core->sinks, idx) {
            if (!AUDIO_IS_ERROR(__fill_audio_playback_device_info(sink->proplist, &audio_info))) {
                if (__compare_device_info(&audio_info.device, device_info)) {
                    is_module_loaded = true;
                    __free_audio_device_info(&audio_info);
                    break;
                }
            }
            __free_audio_device_info(&audio_info);
        }

        if (is_module_loaded) {
            sink_null = (pa_sink *)pa_namereg_get(u->core, "null", PA_NAMEREG_SINK);
            if (pa_namereg_get_default_sink(u->core) == sink) {
                pa_namereg_set_default_sink(u->core, sink_null);
            }

            PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {
                if (__get_real_master_sink(si) != sink)
                    continue;
                /* Get role (if role is filter, skip it) */
                if (policy_is_filter(si->proplist))
                    continue;
                /* Cork only if sink-input was Running */
                if (pa_sink_input_get_state(si) == PA_SINK_INPUT_RUNNING) {
                    pa_proplist_sets(si->proplist, PROP_POLICY_CORK, "1");
                    pa_sink_input_cork(si, TRUE);
                    pa_log_info("<Cork> for sink-input[%d]:sink[%s]", si->index, si->sink->name);
                }
                if (sink_null)
                    pa_sink_input_move_to(si, sink_null, FALSE);
            }
            pa_sink_suspend(sink, TRUE, PA_SUSPEND_SWITCH);

            if (hal_event_type == PA_HAL_EVENT_UNLOAD_DEVICE) {
                pa_module_unload(u->core, sink->module, TRUE);
            }
        }
    }

    return AUDIO_RET_OK;
}

static audio_return_t __close_n_unload_device_callback (void *platform_data, audio_device_info_t *device_info, pa_hal_event_type_t hal_event_type)
{
    const char* dummy = 'W';
    struct userdata *u = (struct userdata *)platform_data;
    struct pa_hal_device_event_data *event_data;

    pa_assert(u);
    pa_assert(device_info);
    pa_assert(device_info->direction == AUDIO_DIRECTION_IN || device_info->direction == AUDIO_DIRECTION_OUT);
    pa_assert(hal_event_type == PA_HAL_EVENT_CLOSE_DEVICE || hal_event_type == PA_HAL_EVENT_UNLOAD_DEVICE);

    if (u->tid != pthread_self()) {
        /* called from thread */
        pa_log_debug("%s is called within thread", __get_event_type_string(hal_event_type));

        pa_mutex_lock(u->defer.mutex);

        u->defer.event_type = hal_event_type;
        event_data = pa_xmalloc(sizeof(struct pa_hal_device_event_data));
        memcpy(&event_data->device_info, device_info, sizeof(audio_device_info_t));
        u->defer.event_data = (void *)event_data;

        /* trigger will be called because sub thread must not to access to mainloop context */
        write(u->fd_defer[1], &dummy, sizeof(dummy));

        pa_cond_wait(u->defer.cond, u->defer.mutex);
        pa_xfree(u->defer.event_data);
        u->defer.event_data = NULL;
        pa_mutex_unlock(u->defer.mutex);

        pa_log_info("%s is finished within thread", __get_event_type_string(hal_event_type));
    } else {
        /* called from mainloop */
        pa_log_debug("%s is called within mainloop", __get_event_type_string(hal_event_type));
        __close_n_unload_device(u, device_info, hal_event_type);
        pa_log_info("%s is finished within mainloop", __get_event_type_string(hal_event_type));
    }

    return AUDIO_RET_OK;
}

static audio_return_t __close_device_callback (void *platform_data, audio_device_info_t *device_info)
{
    return __close_n_unload_device_callback(platform_data, device_info, PA_HAL_EVENT_CLOSE_DEVICE);
}

static audio_return_t __unload_device_callback (void *platform_data, audio_device_info_t *device_info)
{
    return __close_n_unload_device_callback(platform_data, device_info, PA_HAL_EVENT_UNLOAD_DEVICE);
}

static uint32_t __get_route_flag(struct userdata *u) {
    uint32_t route_flag = 0;

    if (u->session == AUDIO_SESSION_VOICECALL || u->session == AUDIO_SESSION_VIDEOCALL || u->session == AUDIO_SESSION_VOIP
        || u->session == AUDIO_SESSION_PTT ) {
        if (u->subsession == AUDIO_SUBSESSION_RINGTONE) {
            if (__is_mute_policy()) {
                route_flag |= AUDIO_ROUTE_FLAG_MUTE_POLICY;
            } else if (!__is_recording()) {
                route_flag |= AUDIO_ROUTE_FLAG_DUAL_OUT;
            }
        } else {
            if (u->call_type)
                route_flag |= __get_network_type_mask(u);
            if (u->bt_bandwidth)
                route_flag |= __get_bluetooth_bandwidth_mask(u);
            if (u->bt_nrec > 0)
                route_flag |= AUDIO_ROUTE_FLAG_BT_NREC;
            if (__is_noise_reduction_on(u))
                route_flag |= AUDIO_ROUTE_FLAG_NOISE_REDUCTION;
            if (__is_extra_volume_on(u))
                route_flag |= AUDIO_ROUTE_FLAG_EXTRA_VOL;
            if (__is_wideband(u))
                route_flag |= AUDIO_ROUTE_FLAG_NETWORK_WB;
            if (u->link_direction == SOUND_LINK_DIRECTION_UP_LINK)
                route_flag |= AUDIO_ROUTE_FLAG_NETWORK_UPLINK;
            if (u->link_direction == SOUND_LINK_DIRECTION_DOWN_LINK)
                route_flag |= AUDIO_ROUTE_FLAG_NETWORK_DOWNLINK;
            if (u->link_direction == (SOUND_LINK_DIRECTION_UP_LINK | SOUND_LINK_DIRECTION_DOWN_LINK))
                route_flag |= (AUDIO_ROUTE_FLAG_NETWORK_DOWNLINK|AUDIO_ROUTE_FLAG_NETWORK_UPLINK);
        }
    } else if (u->session == AUDIO_SESSION_NOTIFICATION) {
        if (__is_mute_policy()) {
            route_flag |= AUDIO_ROUTE_FLAG_MUTE_POLICY;
        } else if (!__is_recording()) {
            if(u->active_device_out == AUDIO_DEVICE_OUT_BT_A2DP || u->active_device_out == AUDIO_DEVICE_OUT_BT_SCO) {
                route_flag |= AUDIO_ROUTE_FLAG_DUAL_LAST_OUT;
            } else {
                route_flag |= AUDIO_ROUTE_FLAG_DUAL_OUT;
            }
        }
    } else if(u->session == AUDIO_SESSION_ALARM) {
        if (!__is_recording()) {
            if(u->active_device_out == AUDIO_DEVICE_OUT_BT_A2DP || u->active_device_out == AUDIO_DEVICE_OUT_BT_SCO) {
                route_flag |= AUDIO_ROUTE_FLAG_DUAL_LAST_OUT;
            } else {
                route_flag |= AUDIO_ROUTE_FLAG_DUAL_OUT;
            }
        }
    }
    if (u->session == AUDIO_SESSION_VOICE_RECOGNITION && u->subsession_opt & SUBSESSION_OPT_SVOICE) {
        if (u->subsession_opt & SUBSESSION_OPT_COMMAND)
            route_flag |= AUDIO_ROUTE_FLAG_SVOICE_COMMAND;
        else if (u->subsession_opt & SUBSESSION_OPT_WAKEUP)
            route_flag |= AUDIO_ROUTE_FLAG_SVOICE_WAKEUP;
    }

    return route_flag;
}

#ifndef PROFILE_WEARABLE
#define BURST_SOUND_DEFAULT_TIME_INTERVAL (0.09 * PA_USEC_PER_SEC)
static void __play_audio_sample_timeout_cb(pa_mainloop_api *m, pa_time_event *e, const struct timeval *t, void *userdata)
{
    struct userdata* u = (struct userdata*)userdata;
    pa_usec_t interval = u->audio_sample_userdata.time_interval;
    pa_usec_t now = 0ULL;

    pa_assert(m);
    pa_assert(e);
    pa_assert(u);

    pa_mutex_lock(u->audio_sample_userdata.mutex);

    /* These checks are added to avoid server crashed on unpredictable situation*/
    if ((u->audio_sample_userdata.time_event == NULL) ||
       (u->audio_sample_userdata.i == NULL) ||
       (u->audio_sample_userdata.q == NULL)) {
        pa_log_error("Timer should not have fired with this condition time_event=%p i=%p q=%p",
            u->audio_sample_userdata.time_event, u->audio_sample_userdata.i, u->audio_sample_userdata.q);

        if (u->audio_sample_userdata.time_event != NULL) {
            pa_core_rttime_restart(u->core, e, PA_USEC_INVALID);
            u->core->mainloop->time_free(u->audio_sample_userdata.time_event);
            u->audio_sample_userdata.time_event = NULL;
        }
        if (u->audio_sample_userdata.count > 1) {
            if (u->audio_sample_userdata.i != NULL) {
                pa_sink_input_set_mute(u->audio_sample_userdata.i, TRUE, TRUE);
                pa_sink_input_unlink(u->audio_sample_userdata.i);
            }
            if (u->audio_sample_userdata.q != NULL) {
                pa_memblockq_free(u->audio_sample_userdata.q);
                u->audio_sample_userdata.q = NULL;
            }
        }
        if (u->audio_sample_userdata.i != NULL) {
            pa_sink_input_unref(u->audio_sample_userdata.i);
            u->audio_sample_userdata.i = NULL;
        }
        u->audio_sample_userdata.is_running = FALSE;
    } else if (u->audio_sample_userdata.is_running) {
        // calculate timer boosting
        pa_log_info("- shot count = %d, memq len = %d ", u->audio_sample_userdata.count,
                    pa_memblockq_get_length(u->audio_sample_userdata.q));
        if (u->audio_sample_userdata.factor > 1ULL)
            interval = u->audio_sample_userdata.time_interval / u->audio_sample_userdata.factor;

        if (u->audio_sample_userdata.count == 0) {
            // 5. first post data
            pa_sink_input_put(u->audio_sample_userdata.i);
        } else {
            // 5. post data
            if (pa_memblockq_push(u->audio_sample_userdata.q, &u->audio_sample_userdata.e->memchunk) < 0) {
                pa_log_error("memory push fail cnt(%d), factor(%llu), interval(%llu)",
                    u->audio_sample_userdata.count, u->audio_sample_userdata.factor, u->audio_sample_userdata.time_interval);
                pa_assert(0);
            }
        }
        u->audio_sample_userdata.count++;

        pa_rtclock_now_args(&now);
        pa_core_rttime_restart(u->core, e, now + interval);
        if (u->audio_sample_userdata.factor > 1ULL)
            u->audio_sample_userdata.factor -= 1ULL;
    } else {
        pa_core_rttime_restart(u->core, e, PA_USEC_INVALID);
        u->core->mainloop->time_free(u->audio_sample_userdata.time_event);
        u->audio_sample_userdata.time_event = NULL;

        /* FIXME: How memblockq is freed when count is 1? */
        /* maybe fix? when sink go to suspend all memblockq free*/
        // fading. but should be emitted first shutter sound totally.
        if (u->audio_sample_userdata.count > 1) {
            if (PA_SINK_INPUT_IS_LINKED(u->audio_sample_userdata.i->state)) {
                pa_sink_input_set_mute(u->audio_sample_userdata.i, TRUE, TRUE);
                pa_sink_input_unlink(u->audio_sample_userdata.i);

                if (u->audio_sample_userdata.q != NULL) {
                    pa_memblockq_free(u->audio_sample_userdata.q);
                    u->audio_sample_userdata.q = NULL;
                }
            }
        }
        pa_sink_input_unref(u->audio_sample_userdata.i);
        u->audio_sample_userdata.i = NULL;
        pa_log_info("sample shot clear!!");

        /* Clear Burst vconf */
        if (vconf_set_int(VCONF_SOUND_BURSTSHOT, 0)) {
            pa_log_warn("vconf_set_int(%s) failed of errno = %d", VCONF_SOUND_BURSTSHOT, vconf_get_ext_errno());
        }
    }
    pa_mutex_unlock(u->audio_sample_userdata.mutex);
}

static audio_return_t policy_play_sample_continuously(struct userdata *u, pa_native_connection *c, const char *name, pa_usec_t interval,
    uint32_t volume_type, uint32_t gain_type, uint32_t volume_level, uint32_t *stream_idx)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    pa_proplist *p = 0;
    pa_sink *sink = NULL;
    audio_info_t audio_info;
    double volume_linear = 1.0f;
    pa_client *client = pa_native_connection_get_client(c);

    pa_scache_entry *e;
    pa_bool_t pass_volume = TRUE;
    pa_proplist *merged =0;
    pa_sink_input *i = NULL;
    pa_memblockq *q = NULL;
    pa_memchunk silence;
    pa_cvolume r;
    pa_usec_t now = 0ULL;

    if (!u->audio_sample_userdata.mutex)
        u->audio_sample_userdata.mutex = pa_mutex_new(FALSE, FALSE);

    pa_mutex_lock(u->audio_sample_userdata.mutex);

    pa_assert(u->audio_sample_userdata.is_running == FALSE); // allow one instace.

    memset(&audio_info, 0x00, sizeof(audio_info_t));

    p = pa_proplist_new();

    pa_proplist_setf(p, PA_PROP_MEDIA_TIZEN_VOLUME_TYPE, "%d", volume_type);
    pa_proplist_setf(p, PA_PROP_MEDIA_TIZEN_GAIN_TYPE, "%d", gain_type);
    pa_proplist_setf(p, PA_PROP_MEDIA_POLICY, "%s", volume_type == AUDIO_VOLUME_TYPE_FIXED ? POLICY_PHONE : POLICY_AUTO);
    __set_sink_input_role_type(p, gain_type);

    pa_proplist_update(p, PA_UPDATE_MERGE, client->proplist);

    sink = pa_namereg_get_default_sink(u->core);

    /* FIXME : Add gain_type parameter to API like volume_type */
    audio_info.stream.gain_type = gain_type;

    if (AUDIO_IS_ERROR((audio_ret = u->audio_mgr.intf.get_volume_value(u->audio_mgr.data, &audio_info, volume_type, volume_level, &volume_linear)))) {
        pa_log_warn("get_volume_value returns error:0x%x", audio_ret);
        goto exit;
    }

    /*
    1. load cam-shutter sample
    2. create memchunk using sample.
    3. create sink_input(cork mode)
    4. set timer
    5. post data(sink-input put or push memblockq)
    */

    //  1. load cam-shutter sample
    merged = pa_proplist_new();

    if (!(e = pa_namereg_get(u->core, name, PA_NAMEREG_SAMPLE)))
        goto exit;

    pa_proplist_sets(merged, PA_PROP_MEDIA_NAME, name);
    pa_proplist_sets(merged, PA_PROP_EVENT_ID, name);
    /* Set policy for selecting sink */
    pa_proplist_sets(merged, PA_PROP_MEDIA_POLICY_IGNORE_PRESET_SINK, "yes");

    if (e->lazy && !e->memchunk.memblock) {
        pa_channel_map old_channel_map = e->channel_map;

        if (pa_sound_file_load(u->core->mempool, e->filename, &e->sample_spec, &e->channel_map, &e->memchunk, merged) < 0)
            goto exit;

        pa_subscription_post(u->core, PA_SUBSCRIPTION_EVENT_SAMPLE_CACHE|PA_SUBSCRIPTION_EVENT_CHANGE, e->index);

        if (e->volume_is_set) {
            if (pa_cvolume_valid(&e->volume))
                pa_cvolume_remap(&e->volume, &old_channel_map, &e->channel_map);
            else
                pa_cvolume_reset(&e->volume, e->sample_spec.channels);
        }
    }

    if (!e->memchunk.memblock)
        goto exit;

    if (e->volume_is_set && PA_VOLUME_IS_VALID(pa_sw_volume_from_linear(volume_linear))) {
        pa_cvolume_set(&r, e->sample_spec.channels, pa_sw_volume_from_linear(volume_linear));
        pa_sw_cvolume_multiply(&r, &r, &e->volume);
    } else if (e->volume_is_set)
        r = e->volume;
    else if (PA_VOLUME_IS_VALID(pa_sw_volume_from_linear(volume_linear)))
        pa_cvolume_set(&r, e->sample_spec.channels, pa_sw_volume_from_linear(volume_linear));
    else
        pass_volume = FALSE;

    pa_proplist_update(merged, PA_UPDATE_MERGE, e->proplist);
    pa_proplist_update(p, PA_UPDATE_MERGE, merged);

    if (e->lazy)
        time(&e->last_used_time);

    // 2. create memchunk using sample.
    pa_silence_memchunk_get(&sink->core->silence_cache, sink->core->mempool, &silence, &e->sample_spec, 0);
    q = pa_memblockq_new("pa_play_memchunk() q", 0, e->memchunk.length * 35, 0, &e->sample_spec, 1, 1, 0, &silence);
    pa_memblock_unref(silence.memblock);

    pa_assert_se(pa_memblockq_push(q, &e->memchunk) >= 0);

    // 3. create sink_input(cork mode)
    if (!(i = pa_memblockq_sink_input_new(sink, &e->sample_spec, &e->channel_map, q, pass_volume ? &r : NULL,
        p, PA_SINK_INPUT_NO_CREATE_ON_SUSPEND|PA_SINK_INPUT_KILL_ON_SUSPEND)))
        goto exit;

    // 4. set timer
    u->audio_sample_userdata.e = e;
    u->audio_sample_userdata.i = i;
    u->audio_sample_userdata.q = q;
    u->audio_sample_userdata.time_interval = interval == (pa_usec_t)0 ? BURST_SOUND_DEFAULT_TIME_INTERVAL : interval;
    u->audio_sample_userdata.is_running = TRUE;
    u->audio_sample_userdata.factor = 4ULL; // for memory block boosting
    u->audio_sample_userdata.count = 0;

    pa_rtclock_now_args(&now); // doesn't use arm barrel shiter. SBF
    pa_log_warn("now(%llu), start interval(%llu)", now, interval / u->audio_sample_userdata.factor);
    u->audio_sample_userdata.factor -= 1ULL;
    u->audio_sample_userdata.time_event = pa_core_rttime_new(u->core, now, __play_audio_sample_timeout_cb, u);

exit:
    if (p)
        pa_proplist_free(p);
    if (merged)
        pa_proplist_free(merged);
    if (q && (u->audio_sample_userdata.is_running == FALSE))
        pa_memblockq_free(q);

    pa_mutex_unlock(u->audio_sample_userdata.mutex);

    return audio_ret;
}

static void  policy_stop_sample_continuously(struct userdata *u)
{
    if (u->audio_sample_userdata.time_event) {
        pa_mutex_lock(u->audio_sample_userdata.mutex);
        pa_assert(u->audio_sample_userdata.is_running);
        u->audio_sample_userdata.is_running = FALSE;
        pa_mutex_unlock(u->audio_sample_userdata.mutex);
        pa_log_info("timeout_cb called (%d) times", u->audio_sample_userdata.count);
    }
}
#endif
static audio_return_t policy_play_sample(struct userdata *u, pa_native_connection *c, const char *name, uint32_t volume_type, uint32_t gain_type, uint32_t volume_level, uint32_t *stream_idx)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    pa_proplist *p;
    pa_sink *sink = NULL;
    audio_info_t audio_info;
    double volume_linear = 1.0f;
    pa_client *client = pa_native_connection_get_client(c);
    pa_bool_t is_boot_sound;
    uint32_t sample_idx = 0;
    char* booting = NULL;
    const char* file_to_add = NULL;
    int sample_ret = 0;

    memset(&audio_info, 0x00, sizeof(audio_info_t));

    p = pa_proplist_new();

    /* Set volume type of stream */
    pa_proplist_setf(p, PA_PROP_MEDIA_TIZEN_VOLUME_TYPE, "%d", volume_type);
    /* Set gain type of stream */
    pa_proplist_setf(p, PA_PROP_MEDIA_TIZEN_GAIN_TYPE, "%d", gain_type);
    /* Set policy */
    pa_proplist_setf(p, PA_PROP_MEDIA_POLICY, "%s", volume_type == AUDIO_VOLUME_TYPE_FIXED ? POLICY_PHONE : POLICY_AUTO);

    pa_proplist_update(p, PA_UPDATE_MERGE, client->proplist);

    /* Set policy for selecting sink */
    pa_proplist_sets(p, PA_PROP_MEDIA_POLICY_IGNORE_PRESET_SINK, "yes");
    sink = pa_namereg_get_default_sink(u->core);

    /* FIXME : Add gain_type parameter to API like volume_type */
    audio_info.stream.gain_type = gain_type;

    if (u->audio_mgr.intf.get_volume_level) {
        if (AUDIO_IS_ERROR((audio_ret = u->audio_mgr.intf.get_volume_level(u->audio_mgr.data, volume_type, &volume_level)))) {
            pa_log_warn("get_volume_value returns error:0x%x", audio_ret);
            goto exit;
        }
    }
    if(u->audio_mgr.intf.get_volume_value) {
        if (AUDIO_IS_ERROR((audio_ret = u->audio_mgr.intf.get_volume_value(u->audio_mgr.data, &audio_info, volume_type, volume_level, &volume_linear)))) {
            pa_log_warn("get_volume_value returns error:0x%x", audio_ret);
            goto exit;
        }
    }

    pa_log_debug("play_sample volume_type:%d gain_type:%d volume_level(%d), volume_linear:%f", volume_type, gain_type, volume_level, volume_linear);

    is_boot_sound = pa_streq(name, BOOTING_SOUND_SAMPLE);
    if (is_boot_sound && pa_namereg_get(u->core, name, PA_NAMEREG_SAMPLE) == NULL) {
        booting = vconf_get_str(VCONF_BOOTING);
        file_to_add = (booting) ? booting : DEFAULT_BOOTING_SOUND_PATH;
        if ((sample_ret = pa_scache_add_file(u->core, name, file_to_add, &sample_idx)) != 0) {
            pa_log_error("failed to add sample [%s][%s]", name, file_to_add);
        } else {
            pa_log_info("success to add sample [%s][%s]", name, file_to_add);
        }
        if (booting)
            free(booting);
    }

    if (pa_scache_play_item(u->core, name, sink, pa_sw_volume_from_linear(volume_linear), p, stream_idx) < 0) {
        pa_log_error("pa_scache_play_item fail");
        audio_ret = AUDIO_ERR_UNDEFINED;
        goto exit;
    }

    if (is_boot_sound && sample_ret == 0) {
        if (pa_scache_remove_item(u->core, name) != 0) {
            pa_log_error("failed to remove sample [%s]", name);
        } else {
            pa_log_info("success to remove sample [%s]", name);
        }
    }

exit:
    pa_proplist_free(p);

    return audio_ret;
}

static audio_return_t policy_reset(struct userdata *u)
{
    audio_return_t audio_ret = AUDIO_RET_OK;

    pa_log_debug("reset");

    __load_dump_config(u);

    if (u->audio_mgr.intf.reset) {
        if (AUDIO_IS_ERROR((audio_ret = u->audio_mgr.intf.reset(&u->audio_mgr.data)))) {
            pa_log_error("audio_mgr reset failed");
            return audio_ret;
        }
    }

    return audio_ret;
}

static audio_return_t policy_set_session(struct userdata *u, uint32_t session, uint32_t start) {
    uint32_t prev_session = u->session;
    uint32_t prev_subsession = u->subsession;
    pa_sink_input *si;
    const char *si_volume_type_str;
    uint32_t volume_type;
    uint32_t idx;
    pa_bool_t need_route = false;

    pa_log_info("set_session:%s %s (current:%s,%s)",
            __get_session_str(session), (start) ? "start" : "end", __get_session_str(u->session), __get_subsession_str(u->subsession));

    if (start) {
        u->session = session;

        if ((u->session == AUDIO_SESSION_VOICECALL) || (u->session == AUDIO_SESSION_VIDEOCALL) || (u->session == AUDIO_SESSION_PTT)) {
            u->subsession = AUDIO_SUBSESSION_MEDIA;
            u->call_muted = 0;
        } else if (u->session == AUDIO_SESSION_VOIP) {
            u->subsession = AUDIO_SUBSESSION_MEDIA;
        } else if (u->session == AUDIO_SESSION_VOICE_RECOGNITION) {
            u->subsession = AUDIO_SUBSESSION_INIT;
        } else {
            u->subsession = AUDIO_SUBSESSION_NONE;
        }

        if (u->audio_mgr.intf.set_session) {
            u->audio_mgr.intf.set_session(u->audio_mgr.data, session, u->subsession, AUDIO_SESSION_CMD_START);
        }
    } else {
        if (u->audio_mgr.intf.set_session) {
            u->audio_mgr.intf.set_session(u->audio_mgr.data, session, u->subsession, AUDIO_SESSION_CMD_END);
        }

        if(__is_call_session(u->session) && session == AUDIO_SESSION_FMRADIO) {
            pa_log_info("during call fm end case");
        } else {
            u->session = AUDIO_SESSION_MEDIA;
            u->subsession = AUDIO_SUBSESSION_NONE;
        }
        u->call_type = u->call_nrec = u->call_extra_volume = u->link_direction = 0;
    }

    if (prev_session != session) {
        if ((session == AUDIO_SESSION_ALARM) || (session == AUDIO_SESSION_NOTIFICATION)) { // Notification Fading
            pa_log_info("switch route to dual output due to new session");
            need_route = true;
        } else if ((prev_session == AUDIO_SESSION_ALARM) || (prev_session == AUDIO_SESSION_NOTIFICATION)
            || (((prev_session == AUDIO_SESSION_VOICECALL) || (prev_session == AUDIO_SESSION_VIDEOCALL)) && (prev_subsession == AUDIO_SUBSESSION_RINGTONE))) { // Notification Fading
            pa_log_info("switch route from dual output due to previous session");
            need_route = true;
        }
    }

    /* notification start if faded stream exist */
    if (session == AUDIO_SESSION_NOTIFICATION && start && !pa_idxset_isempty(u->half_fade_streams)) {
        /* enter notificaiton session but faded stream exist. it doesn't need to route. */
        need_route = false;
        /* Move notificaiton sink-input from combined sink to default sink. */
        pa_sink *sink_default = pa_namereg_get_default_sink(u->core);
        PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {
            if ((si_volume_type_str = pa_proplist_gets(si->proplist, PA_PROP_MEDIA_TIZEN_VOLUME_TYPE))) {
                pa_atou(si_volume_type_str, &volume_type);
                if (volume_type == AUDIO_VOLUME_TYPE_NOTIFICATION) {
                    pa_sink_input_move_to(si, sink_default, FALSE);
                }
            }
        }
    }

    if (need_route) {
        uint32_t route_flag = __get_route_flag(u);

        if (u->audio_mgr.intf.set_route) {
            u->audio_mgr.intf.set_route(u->audio_mgr.data, u->session, u->subsession, u->active_device_in, u->active_device_out, route_flag);
        }
        u->active_route_flag = route_flag;
    } else {
        /* route should be updated */
        u->active_route_flag = (uint32_t)-1;
    }

    return AUDIO_RET_OK;
}

static audio_return_t policy_set_subsession(struct userdata *u, uint32_t subsession, uint32_t subsession_opt) {
    uint32_t prev_subsession = u->subsession;
    pa_bool_t need_route = false;

    pa_log_info("set_subsession:%s->%s opt:%x->%x (session:%s)",
            __get_subsession_str(u->subsession), __get_subsession_str(subsession), u->subsession_opt, subsession_opt,
            __get_session_str(u->session));

    if (u->subsession == subsession && u->subsession_opt == subsession_opt) {
        pa_log_debug("duplicated request is ignored subsession(%d) opt(0x%x)", subsession, subsession_opt);
        return AUDIO_RET_OK;
    }

    u->subsession = subsession;
#ifndef _TIZEN_PUBLIC_
    if (u->subsession == AUDIO_SUBSESSION_VR_NORMAL || u->subsession == AUDIO_SUBSESSION_VR_DRIVE)
        u->subsession_opt = subsession_opt;
    else
        u->subsession_opt = 0;
#else
    u->subsession_opt = subsession_opt;
#endif

    if (u->audio_mgr.intf.set_session) {
        u->audio_mgr.intf.set_session(u->audio_mgr.data, u->session, u->subsession, AUDIO_SESSION_CMD_SUBSESSION);
    }

    if (prev_subsession!= subsession) {
        if ((u->session == AUDIO_SESSION_VOICECALL) || (u->session == AUDIO_SESSION_VIDEOCALL) || (u->session == AUDIO_SESSION_VOIP) || (u->session == AUDIO_SESSION_PTT)) {
            if (subsession == AUDIO_SUBSESSION_RINGTONE) {
                pa_log_info("switch route to dual output due to new subsession");
                need_route = true;
            } else if (prev_subsession == AUDIO_SUBSESSION_RINGTONE) {
                pa_log_info("switch route from dual output due to previous subsession");
                need_route = true;
            }
        }
    }

    if (need_route) {
        uint32_t route_flag = __get_route_flag(u);

        if (u->audio_mgr.intf.set_route) {
            u->audio_mgr.intf.set_route(u->audio_mgr.data, u->session, u->subsession, u->active_device_in, u->active_device_out, route_flag);
        }
        u->active_route_flag = route_flag;
    } else {
        /* route should be updated */
        u->active_route_flag = (uint32_t)-1;
    }

    return AUDIO_RET_OK;
}

static audio_return_t policy_set_active_device(struct userdata *u, uint32_t device_in, uint32_t device_out, uint32_t is_start, uint32_t* need_update) {
    audio_return_t audio_ret = AUDIO_RET_OK;
    pa_sink *sink_to_move = NULL;
    pa_sink_input *si = NULL;
    uint32_t idx;
    const char *device_switching_str;
    uint32_t device_switching = 0;
    const char *policy = NULL;

    /* skip volume changed callback if output device is not changed */
    *need_update = (u->active_device_out == device_out) ? FALSE : TRUE;

    /* update route flag if start callback is not called */
    if (u->cache_route_flag == (uint32_t)-1) {
        u->cache_route_flag = __get_route_flag(u);
    }

    pa_log_info("set_active_device is_start:%d, session:%s,%s in:%s->%s out:%s->%s flag:%x->%x muteall:%d call_muted:%d need_update(%d)",
            is_start, __get_session_str(u->session), __get_subsession_str(u->subsession),__get_device_in_str(u->active_device_in),
            __get_device_in_str(device_in), __get_device_out_str(u->active_device_out), __get_device_out_str(device_out),
            u->active_route_flag, u->cache_route_flag, u->muteall, u->call_muted, *need_update);

    /* Skip duplicated request */
    if ((device_in == AUDIO_DEVICE_IN_NONE || u->active_device_in == device_in) &&
        (device_out == AUDIO_DEVICE_OUT_NONE || u->active_device_out == device_out) &&
        u->active_route_flag == u->cache_route_flag && !__is_loopback()) {
        pa_log_debug("duplicated request is ignored device_in(%d) device_out(%d) flag(0x%x)", device_in, device_out, u->cache_route_flag);

        goto exit;
    }

    if (is_start) {
        /* device change start */
        if (device_out != AUDIO_DEVICE_OUT_NONE && u->active_device_out != device_out &&
            u->session != AUDIO_SESSION_VOICECALL && u->session != AUDIO_SESSION_VIDEOCALL) {
            /* Mark & mute sink inputs which are unmuted */
            PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {
                if (!pa_sink_input_get_mute(si)) {
                    pa_proplist_sets(si->proplist, "module-policy.device_switching", "1");
                    if (AUDIO_IS_ERROR((audio_ret = policy_set_mute(u, si->index, (uint32_t)-1, AUDIO_DIRECTION_OUT, 1, false)))) {
                        pa_log_warn("policy_set_mute(1) for stream[%d] returns error:0x%x", si->index, audio_ret);
                    }
                }
            }
        }
        *need_update = FALSE;
    } else {
        /* device change end */
        if (u->audio_mgr.intf.set_route) {
            u->audio_mgr.intf.set_route(u->audio_mgr.data, u->session, u->subsession, device_in, device_out, u->cache_route_flag);
        }

        /* select default sink. move from libmm-sound*/
        if (u->session != AUDIO_SESSION_VOICECALL && u->session != AUDIO_SESSION_VIDEOCALL && u->session != AUDIO_SESSION_VOIP) {
            if (!policy_is_call_active(u)) {
                if (device_out == AUDIO_DEVICE_OUT_BT_A2DP) {
                    pa_namereg_set_default_sink(u->core, u->bluez_sink);
                } else if (device_out == AUDIO_DEVICE_OUT_SPEAKER || device_out == AUDIO_DEVICE_OUT_WIRED_ACCESSORY) {
                    pa_sink *default_sink = (pa_sink *)pa_namereg_get(u->core, SINK_ALSA, PA_NAMEREG_SINK);
                    pa_namereg_set_default_sink(u->core, default_sink);
                    pa_log_info("we set default sink to alsa-sink");
                } else {
                    pa_log_info("don't need to set default sink. device_out(%d)", device_out);
                }
            } else {
                pa_log_info("call is active");
            }
        }

        /* Unmute sink inputs which are muted due to device switching */
        PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {

            if (!si->sink)
                continue;

            /* Get role (if role is filter, skip it) */
            if (policy_is_filter(si->proplist))
                continue;

            /* Get policy */
            if (!(policy = pa_proplist_gets(si->proplist, PA_PROP_MEDIA_POLICY))) {
                /* No policy exists, this means auto */
                pa_log_debug("set policy of sink-input[%d] from [%s] to [auto]", si->index, "null");
                policy = POLICY_AUTO;
            }


            if ((si->sample_spec.rate >= UHQA_BASE_SAMPLING_RATE)
                 && (pa_streq(policy, POLICY_HIGH_LATENCY) || pa_streq(policy, POLICY_AUTO))) {

                char uhqa_policy[100] = {0};
                sprintf(uhqa_policy, "%s-uhqa", policy);
                sink_to_move = policy_select_proper_sink (u, uhqa_policy, si, TRUE);
            } else {
                sink_to_move = policy_select_proper_sink (u, policy, si, TRUE);
            }
            pa_sink_input_move_to(si, sink_to_move, FALSE);

            if (*need_update) {
                if (AUDIO_IS_ERROR((audio_ret = __update_volume(u, si->index, (uint32_t)-1, (uint32_t)-1, false)))) {
                    pa_log_warn("__update_volume for stream[%d] returns error:0x%x", si->index, audio_ret);
                }
            } else {
                pa_log_info("Skip to update volume. u->active_device_out(%d), device_out(%d)", u->active_device_out, device_out);
            }

            if ((device_switching_str = pa_proplist_gets(si->proplist, "module-policy.device_switching"))) {
                pa_atou(device_switching_str, &device_switching);
                if (device_switching) {
                    if (AUDIO_IS_ERROR((audio_ret = policy_set_mute(u, si->index, (uint32_t)-1, AUDIO_DIRECTION_OUT, 0, false)))) {
                        pa_log_warn("policy_set_mute(0) for stream[%d] returns error:0x%x", si->index, audio_ret);
                    }
                    pa_proplist_sets(si->proplist, "module-policy.device_switching", "0");
                }
            }
        }

        /* Update active devices */
        u->active_device_in = device_in;
        u->active_device_out = device_out;
        u->active_route_flag = u->cache_route_flag;

        if (u->session == AUDIO_SESSION_VOICECALL) {
            if (u->muteall) {
                policy_set_mute(u, (-1), AUDIO_VOLUME_TYPE_CALL, AUDIO_DIRECTION_OUT, 1, false);
            }
            /* workaround for keeping call mute setting */
            policy_set_mute(u, (-1), AUDIO_VOLUME_TYPE_CALL, AUDIO_DIRECTION_IN, u->call_muted, false);
        }

        /* clear fade information */
        memset(u->fade_rcount, 0, sizeof(u->fade_rcount));
    }

exit:
    if (is_start == FALSE && u->active_device_out == AUDIO_DEVICE_OUT_BT_A2DP) {
        if (u->bluez_sink) {
            if (pa_sink_get_mute(u->bluez_sink, FALSE)) {
                pa_log_info("bt headset is connected. bluez sink will be unmuted.");
                pa_sink_set_mute(u->bluez_sink, FALSE, TRUE);
            }
        }
    }

    if (!is_start)
        u->cache_route_flag = (uint32_t)-1;
    return AUDIO_RET_OK;
}

static audio_return_t policy_get_volume_level_max(struct userdata *u, uint32_t volume_type, uint32_t *volume_level) {
    audio_return_t audio_ret = AUDIO_RET_OK;

    /* Call HAL function if exists */
    if (u->audio_mgr.intf.get_volume_level_max) {
        if (AUDIO_IS_ERROR((audio_ret = u->audio_mgr.intf.get_volume_level_max(u->audio_mgr.data, volume_type, volume_level)))) {
            pa_log_error("get_volume_level_max returns error:0x%x", audio_ret);
            return audio_ret;
        }
    }

    pa_log_info("get volume level max type:%d level:%d", volume_type, *volume_level);
    return AUDIO_RET_OK;
}

static audio_return_t __update_volume(struct userdata *u, uint32_t stream_idx, uint32_t volume_type, uint32_t volume_level, uint32_t fade)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    pa_sink_input *si = NULL;
    uint32_t idx;
    uint32_t dummy;
    audio_info_t audio_info;

    pa_log_debug_verbose("update volume. stream_idx(%d), volume_type(%d), volume_level(%d), fade(%d)",
        stream_idx, volume_type, volume_level, fade);

    /* Update volume as current level if volume_level has -1 */
    if (volume_level == (uint32_t)-1 && stream_idx != (uint32_t)-1) {
        /* Skip updating if stream doesn't have volume type */
        if (policy_get_volume_level(u, stream_idx, &volume_type, &volume_level) == AUDIO_ERR_UNDEFINED) {
            return AUDIO_RET_OK;
        }
    }

    if (u->muteall && (volume_type != AUDIO_VOLUME_TYPE_FIXED)) {
        pa_log_debug("set_mute is called from __update_volume by muteall stream_idx:%d type:%d", stream_idx, volume_type);

        if (policy_set_mute(u, stream_idx, volume_type, AUDIO_DIRECTION_OUT, 1, false) == AUDIO_RET_USE_HW_CONTROL) {
            return AUDIO_RET_USE_HW_CONTROL;
        };
    }

    /* Call HAL function if exists */
    if (u->audio_mgr.intf.set_volume_level && (stream_idx == PA_INVALID_INDEX)) {
        if (AUDIO_IS_ERROR((audio_ret = u->audio_mgr.intf.set_volume_level(u->audio_mgr.data, NULL, volume_type, volume_level)))) {
            pa_log_error("set_volume_level returns error:0x%x", audio_ret);
            return audio_ret;
        }
    }

    PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {

        if (AUDIO_IS_ERROR(__fill_audio_playback_info(si, &audio_info))) {
            /* skip mono sink-input */
            continue;
        }

        /* Update volume of stream if it has requested volume type */
        if ((stream_idx == idx) || ((stream_idx == PA_INVALID_INDEX) && (audio_info.stream.volume_type == volume_type))) {
            double volume_linear = 1.0f;
            pa_cvolume cv;

            /* Call HAL function if exists */
            if (u->audio_mgr.intf.set_volume_level) {
                if (AUDIO_IS_ERROR((audio_ret = u->audio_mgr.intf.set_volume_level(u->audio_mgr.data, &audio_info, audio_info.stream.volume_type, volume_level)))) {
                    pa_log_error("set_volume_level for sink-input[%d] returns error:0x%x", idx, audio_ret);
                    __free_audio_info(&audio_info);
                    return audio_ret;
                }
            }

            /* Get volume value by type & level */
            if (u->audio_mgr.intf.get_volume_value && (audio_ret != AUDIO_RET_USE_HW_CONTROL)) {
                if (AUDIO_IS_ERROR((audio_ret = u->audio_mgr.intf.get_volume_value(u->audio_mgr.data, &audio_info, audio_info.stream.volume_type, volume_level, &volume_linear)))) {
                    pa_log_warn("get_volume_value for sink-input[%d] returns error:0x%x", idx, audio_ret);
                    __free_audio_info(&audio_info);
                    return audio_ret;
                }
            }
            pa_cvolume_set(&cv, si->sample_spec.channels, pa_sw_volume_from_linear(volume_linear));

            if (fade && pa_sink_input_get_state(si) == PA_SINK_INPUT_RUNNING)
                pa_sink_input_set_smooth_volume(si, &cv, DEFAULT_FADE_DURATION_MSEC, TRUE, TRUE);
            else
                pa_sink_input_set_volume(si, &cv, TRUE, TRUE);
            if (idx == stream_idx) {
                __free_audio_info(&audio_info);
                break;
            }
        }
        __free_audio_info(&audio_info);
    }

    return audio_ret;
}

static audio_return_t __update_volume_by_value(struct userdata *u, uint32_t stream_idx, uint32_t volume_type, double* value)
{
    audio_return_t audio_ret = AUDIO_ERR_NOT_IMPLEMENTED;
    pa_sink_input *si = NULL;
    uint32_t idx;
    audio_info_t audio_info;

    double volume = *value;
    double gain = 1.0f;

    PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {

        if (AUDIO_IS_ERROR(__fill_audio_playback_info(si, &audio_info))) {
            /* skip mono sink-input */
            continue;
        }

        /* Update volume of stream if it has requested volume type */
        if ((stream_idx == idx) || ((stream_idx == PA_INVALID_INDEX) && (audio_info.stream.volume_type == volume_type))) {
            double volume_linear = 1.0f;
            pa_cvolume cv;

            // 1. get gain first
            if (u->audio_mgr.intf.get_gain_value) {
                if (AUDIO_IS_ERROR((audio_ret = u->audio_mgr.intf.get_gain_value(u->audio_mgr.data, &audio_info, audio_info.stream.volume_type, &gain)))) {
                    pa_log_warn("get_gain_value for sink-input[%d] volume_type(%d), returns error:0x%x", idx, audio_info.stream.volume_type, audio_ret);
                    __free_audio_info(&audio_info);
                    return audio_ret;
                }
            }

            // 2. mul gain value
            volume *= gain;

            /* 3. adjust hw volume(LPA), Call HAL function if exists */
            if (u->audio_mgr.intf.set_volume_value) {
                if (AUDIO_IS_ERROR((audio_ret = u->audio_mgr.intf.set_volume_value(u->audio_mgr.data, &audio_info, audio_info.stream.volume_type, &volume)))) {
                    pa_log_error("set_volume_level for sink-input[%d] returns error:0x%x", idx, audio_ret);
                    __free_audio_info(&audio_info);
                    return audio_ret;
                }
            }

            // 4. adjust sw volume.
            if(!AUDIO_IS_ERROR(audio_ret) && audio_ret != AUDIO_RET_USE_HW_CONTROL)
                pa_cvolume_set(&cv, si->sample_spec.channels, pa_sw_volume_from_linear(volume));
            else
                pa_cvolume_set(&cv, si->sample_spec.channels, pa_sw_volume_from_linear(volume_linear));

            pa_sink_input_set_volume(si, &cv, TRUE, TRUE);

            if (idx == stream_idx) {
                __free_audio_info(&audio_info);
                break;
            }
        }
        __free_audio_info(&audio_info);
    }

    return audio_ret;
}

static int __set_primary_volume(struct userdata *u, void* key, int volumetype, int is_new)
{
    const int NO_INSTANCE = -1;
    const int CAPURE_ONLY = -2; // check mm_sound.c

    int ret = -1;
    int default_primary_vol = NO_INSTANCE;
    int default_primary_vol_prio = NO_INSTANCE;

    struct pa_primary_volume_type_info* p_volume = NULL;
    struct pa_primary_volume_type_info* n_p_volume = NULL;
    struct pa_primary_volume_type_info* new_volume = NULL;

    // descending order
    int priority[] = {
        AUDIO_PRIMARY_VOLUME_TYPE_SYSTEM,
        AUDIO_PRIMARY_VOLUME_TYPE_NOTIFICATION,
        AUDIO_PRIMARY_VOLUME_TYPE_ALARM,
        AUDIO_PRIMARY_VOLUME_TYPE_RINGTONE,
        AUDIO_PRIMARY_VOLUME_TYPE_MEDIA,
        AUDIO_PRIMARY_VOLUME_TYPE_CALL,
        AUDIO_PRIMARY_VOLUME_TYPE_VOIP,
        AUDIO_PRIMARY_VOLUME_TYPE_FIXED,
        AUDIO_PRIMARY_VOLUME_TYPE_EXT_JAVA,
        AUDIO_PRIMARY_VOLUME_TYPE_MAX // for capture handle
    };

    if(is_new) {
        new_volume = pa_xnew0(struct pa_primary_volume_type_info, 1);
        new_volume->key = key;
        new_volume->volumetype = volumetype;
        new_volume->priority = priority[volumetype];

        // no items.
        if(u->primary_volume == NULL) {
            PA_LLIST_PREPEND(struct pa_primary_volume_type_info, u->primary_volume, new_volume);
        } else {
            // already added
            PA_LLIST_FOREACH_SAFE(p_volume, n_p_volume, u->primary_volume) {
                if(p_volume->key == key) {
                    ret = 0;
                    pa_xfree(new_volume);
                    goto exit;
                }
            }

            // add item.
            PA_LLIST_FOREACH_SAFE(p_volume, n_p_volume, u->primary_volume) {
                if(p_volume->priority <= priority[volumetype]) {
                    PA_LLIST_INSERT_AFTER(struct pa_primary_volume_type_info, u->primary_volume, p_volume, new_volume);
                    break;
                } else if(p_volume->priority > priority[volumetype]) {
                    PA_LLIST_PREPEND(struct pa_primary_volume_type_info, u->primary_volume, new_volume);
                    break;
                }
            }
        }
        pa_log_info("add volume data to primary volume list. volumetype(%d), priority(%d)", new_volume->volumetype, new_volume->priority);
    } else { // remove(unlink)
        PA_LLIST_FOREACH_SAFE(p_volume, n_p_volume, u->primary_volume) {
            if(p_volume->key == key) {
                PA_LLIST_REMOVE(struct pa_primary_volume_type_info, u->primary_volume, p_volume);
                pa_log_info("remove volume data from primary volume list. volumetype(%d), priority(%d)", p_volume->volumetype, p_volume->priority);
                pa_xfree(p_volume);
                break;
            }
        }
    }

    if(u->primary_volume) {
        if(u->primary_volume->volumetype == AUDIO_PRIMARY_VOLUME_TYPE_MAX) {
            default_primary_vol = CAPURE_ONLY;
            default_primary_vol_prio = CAPURE_ONLY;
        } else {
            default_primary_vol = u->primary_volume->volumetype;
            default_primary_vol_prio = u->primary_volume->priority;
        }
    }
    pa_log_info("current primary volumetype(%d), priority(%d)", default_primary_vol, default_primary_vol_prio);

    if(vconf_set_int(VCONFKEY_SOUND_PRIMARY_VOLUME_TYPE, default_primary_vol) < 0) {
        ret = -1;
        pa_log_info("VCONFKEY_SOUND_PRIMARY_VOLUME_TYPE set failed default_primary_vol(%d)", default_primary_vol);
    }

exit:

    return ret;
}

#ifdef SUPPORT_HDMI
static int __get_hdmi_support_info(int *hdmi_support, int *channels, int *max_channel) {
    int i;
    int bit_mask = 0x00000080;

    if (vconf_get_int(VCONFKEY_SOUND_HDMI_SUPPORT, hdmi_support)) {
        pa_log_warn("vconf_get_int for %s failed", VCONFKEY_SOUND_HDMI_SUPPORT);
    }

    *channels = *hdmi_support & 0x000000FF;

    if (*channels == 0xFF) {
        *channels = 0;
        *max_channel = 0;
        return 0;
    }

    for (i = 8; i >= 0; i--) {
        if ((bit_mask & *channels)!=0) {
            *max_channel = i;
            break;
        }
        bit_mask = bit_mask >> 1;
    }
    return 1;
}
#endif

static audio_return_t policy_get_volume_level(struct userdata *u, uint32_t stream_idx, uint32_t *volume_type, uint32_t *volume_level) {
    pa_sink_input *si = NULL;
    const char *si_volume_type_str;

    if (*volume_type == (uint32_t)-1 && stream_idx != (uint32_t)-1) {
        if ((si = pa_idxset_get_by_index(u->core->sink_inputs, stream_idx))) {
            if ((si_volume_type_str = pa_proplist_gets(si->proplist, PA_PROP_MEDIA_TIZEN_VOLUME_TYPE))) {
                pa_atou(si_volume_type_str, volume_type);
            } else {
                pa_log_debug_verbose("stream[%d] doesn't have volume type", stream_idx);
                return AUDIO_ERR_UNDEFINED;
            }
        } else {
            pa_log_warn("stream[%d] doesn't exist", stream_idx);
            return AUDIO_ERR_PARAMETER;
        }
    }

    if (*volume_type >= AUDIO_VOLUME_TYPE_MAX) {
        pa_log_warn("volume_type (%d) invalid", *volume_type);
        return AUDIO_ERR_PARAMETER;
    }
    if (u->audio_mgr.intf.get_volume_level) {
        u->audio_mgr.intf.get_volume_level(u->audio_mgr.data, *volume_type, volume_level);
    }

    pa_log_info("get_volume_level stream_idx:%d type:%d level:%d", stream_idx, *volume_type, *volume_level);
    return AUDIO_RET_OK;
}

static audio_return_t policy_get_volume_value(struct userdata *u, uint32_t stream_idx, uint32_t *volume_type, uint32_t *volume_level, double* volume_linear) {
    audio_return_t audio_ret = AUDIO_RET_OK;
    audio_info_t audio_info;
    pa_sink_input *si = NULL;

    *volume_linear = 1.0f;

    si = pa_idxset_get_by_index(u->core->sink_inputs, stream_idx);
    if (si != NULL) {
        if (AUDIO_IS_ERROR(__fill_audio_playback_info(si, &audio_info))) {
            pa_log_debug("fill info failed. stream_idx[%d]", stream_idx);
            return AUDIO_ERR_UNDEFINED;
        }

        if(u->audio_mgr.intf.get_volume_value) {
            if (AUDIO_IS_ERROR((audio_ret = u->audio_mgr.intf.get_volume_value(u->audio_mgr.data, &audio_info, audio_info.stream.volume_type, *volume_level, volume_linear)))) {
                pa_log_warn("get_volume_value for stream_idx[%d] returns error:0x%x", stream_idx, audio_ret);
                return audio_ret;
            }
        }
        __free_audio_info(&audio_info);
    }
    return audio_ret;
}

static audio_return_t policy_set_volume_level(struct userdata *u, uint32_t stream_idx, uint32_t volume_type, uint32_t volume_level) {

    pa_log_info("set_volume_level stream_idx:%d type:%d level:%d", stream_idx, volume_type, volume_level);

    return __update_volume(u, stream_idx, volume_type, volume_level, true);
}

static audio_return_t policy_set_volume_value(struct userdata *u, uint32_t stream_idx, uint32_t volume_type, double* value) {

    pa_log_info("set_volume_value stream_idx:%d type:%d value:%f", stream_idx, volume_type, *value);

    return __update_volume_by_value(u, stream_idx, volume_type, value);
}

static audio_return_t policy_update_volume(struct userdata *u) {
    uint32_t volume_type;
    uint32_t volume_level = 0;

    pa_log_info("update_volume");

    for (volume_type = 0; volume_type < AUDIO_VOLUME_TYPE_MAX; volume_type++) {
        if (u->audio_mgr.intf.get_volume_level) {
            u->audio_mgr.intf.get_volume_level(u->audio_mgr.data, volume_type, &volume_level);
        }
        __update_volume(u, (uint32_t)-1, volume_type, volume_level, false);
    }

    return AUDIO_RET_OK;
}

static audio_return_t policy_get_mute(struct userdata *u, uint32_t stream_idx, uint32_t volume_type, uint32_t direction, uint32_t *mute) {
    audio_return_t audio_ret = AUDIO_RET_OK;
    pa_sink_input *si = NULL;
    uint32_t idx;
    audio_info_t audio_info;

    if (u->audio_mgr.intf.get_mute && (stream_idx == PA_INVALID_INDEX)) {
        audio_ret = u->audio_mgr.intf.get_mute(u->audio_mgr.data, NULL, volume_type, direction, mute);
        if (audio_ret == AUDIO_RET_USE_HW_CONTROL) {
            return audio_ret;
        } else {
            pa_log_error("get_mute returns error:0x%x", audio_ret);
            return audio_ret;
        }
    }

    if (direction == AUDIO_DIRECTION_OUT) {
        PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {
            if (AUDIO_IS_ERROR(__fill_audio_playback_info(si, &audio_info))) {
                /* skip mono sink-input */
                continue;
            }

            /* Update mute of stream if it has requested stream or volume type */
            if ((stream_idx == idx) || ((stream_idx == PA_INVALID_INDEX) && (audio_info.stream.volume_type == volume_type))) {

                /* Call HAL function if exists */
                if (u->audio_mgr.intf.get_mute) {
                    audio_ret = u->audio_mgr.intf.get_mute(u->audio_mgr.data, &audio_info, audio_info.stream.volume_type, direction, mute);
                    if (audio_ret == AUDIO_RET_USE_HW_CONTROL) {
                        return audio_ret;
                    } else if (AUDIO_IS_ERROR(audio_ret)) {
                        pa_log_error("get_mute for sink-input[%d] returns error:0x%x", idx, audio_ret);
                        return audio_ret;
                    }
                }

                *mute = (uint32_t)pa_sink_input_get_mute(si);
                break;
            }
            __free_audio_info(&audio_info);
        }
    }

    pa_log_info("get mute stream_idx:%d type:%d direction:%d mute:%d", stream_idx, volume_type, direction, *mute);
    return audio_ret;
}

static audio_return_t policy_set_mute(struct userdata *u, uint32_t stream_idx, uint32_t volume_type, uint32_t direction, uint32_t mute, uint32_t fade) {
    audio_return_t audio_ret = AUDIO_RET_OK;
    pa_sink_input *si = NULL;
    uint32_t idx;
    uint32_t dummy;
    audio_info_t audio_info;
    const char *si_volume_type_str;

    pa_log_info("set_mute stream_idx:%d type:%d direction:%d mute:%d", stream_idx, volume_type, direction, mute);

    if (volume_type == (uint32_t)-1 && stream_idx != (uint32_t)-1) {
        if ((si = pa_idxset_get_by_index(u->core->sink_inputs, stream_idx))) {
            if ((si_volume_type_str = pa_proplist_gets(si->proplist, PA_PROP_MEDIA_TIZEN_VOLUME_TYPE))) {
                pa_atou(si_volume_type_str, &volume_type);
            } else {
                pa_log_debug_verbose("stream[%d] doesn't have volume type", stream_idx);
                return AUDIO_ERR_UNDEFINED;
            }
        } else {
            pa_log_warn("stream[%d] doesn't exist", stream_idx);
            return AUDIO_ERR_PARAMETER;
        }
    }

    /* workaround for keeping call mute setting */
    if ((volume_type == AUDIO_VOLUME_TYPE_CALL) && (direction == AUDIO_DIRECTION_IN)) {
        u->call_muted = mute;
    }

    if (u->muteall && !mute && (direction == AUDIO_DIRECTION_OUT) && (volume_type != AUDIO_VOLUME_TYPE_FIXED)) {
        pa_log_info("set_mute is ignored by muteall");
        return audio_ret;
    }

    /* Call HAL function if exists */
    if (u->audio_mgr.intf.set_mute && (stream_idx == PA_INVALID_INDEX)) {
        audio_ret = u->audio_mgr.intf.set_mute(u->audio_mgr.data, NULL, volume_type, direction, mute);
        if (audio_ret == AUDIO_RET_USE_HW_CONTROL) {
            pa_log_info("set_mute(call) returns error:0x%x mute:%d", audio_ret, mute);
            return audio_ret;
        } else if (AUDIO_IS_ERROR(audio_ret)) {
            pa_log_error("set_mute returns error:0x%x", audio_ret);
            return audio_ret;
        }
    }

    if (direction == AUDIO_DIRECTION_OUT) {
        PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {
            if (AUDIO_IS_ERROR(__fill_audio_playback_info(si, &audio_info))) {
                /* skip mono sink-input */
                continue;
            }

            if (pa_idxset_get_by_data(u->fade_streams, si, &dummy)) {
                /* skip fade stream */
                pa_log_info("skip to mute for faded stream. already exist sink-input[%d]", si->index);
                continue;
            }

            /* Update mute of stream if it has requested stream or volume type */
            if ((stream_idx == idx) || ((stream_idx == PA_INVALID_INDEX) && (audio_info.stream.volume_type == volume_type))) {

                /* Call HAL function if exists */
                if (u->audio_mgr.intf.set_mute) {
                    audio_ret = u->audio_mgr.intf.set_mute(u->audio_mgr.data, &audio_info, audio_info.stream.volume_type, direction, mute);
                    if (AUDIO_IS_ERROR(audio_ret)) {
                        pa_log_error("set_mute for sink-input[%d] returns error:0x%x", idx, audio_ret);
                        return audio_ret;
                    }
                }

                if (fade && pa_sink_input_get_state(si) == PA_SINK_INPUT_RUNNING)
                    pa_sink_input_set_smooth_mute(si, (pa_bool_t)mute, DEFAULT_FADE_DURATION_MSEC, TRUE);
                else
                    pa_sink_input_set_mute(si, (pa_bool_t)mute, TRUE);

                if (idx == stream_idx)
                    break;
            }
            __free_audio_info(&audio_info);
        }
    }

    return audio_ret;
}

static int policy_volume_fade(struct userdata* u, uint32_t stream_idx, uint32_t up_down, uint32_t duration)
{
    uint32_t volume_type = AUDIO_VOLUME_TYPE_MEDIA;
    uint32_t volume_level = 0;
    double volume_linear = 1.0f;
    pa_sink_input *si = NULL;
    audio_info_t audio_info;
    pa_cvolume cv;

    if (volume_type >= AUDIO_VOLUME_TYPE_MAX) {
        pa_log_error("invalid volume_type(%d)", volume_type);
        return -1;
    }

    if(!u) {
        pa_log_error("userdata is NULL");
        return -1;
    }

    pa_log_info ("policy_volume_fade. stream_idx(%d), up_down(%d), volume_level(%d), duration(%d), linear(%f)", stream_idx, up_down, volume_level, duration, volume_linear);
    si = (pa_sink_input*)pa_idxset_get_by_index(u->core->sink_inputs, stream_idx);
    if (si == NULL) {
        pa_log_warn("can't find sink-input");
        return -1;
    }
    /*
    (0): fade-none        - fade-none
    (1): fade-in          - fade-in
    (2): fade-out         - fade-out
    */

    if (up_down >= FADE_MAX) {
        pa_log_error("invalid up_down(%d)", up_down);
        return -1;
    }

    if (policy_get_volume_level(u, stream_idx, &volume_type, &volume_level) == AUDIO_ERR_UNDEFINED) {
        pa_log_error("can't get volume level");
        return -1;
    }

    if (policy_get_volume_value(u, stream_idx, &volume_type, &volume_level, &volume_linear) == AUDIO_ERR_UNDEFINED) {
        pa_log_debug("can't get volume value");
        return -1;
    } else {
        if(up_down == FADE_OUT) {
            volume_linear = 0.0f;
        } else if (up_down == FADE_IN) {
            /* exception case. sink that is connected to LPA should be set max volume */
            if (sink_is_highlatency(si->sink)) {
                volume_linear = 1.0f;
            } else {
                volume_linear = volume_linear;
            }
        } else if (up_down == FADE_OUT_HALF) {
            volume_linear /= 4.0f;
        } else {
            pa_log_error("unknown fade info.up_down(%d)", up_down);
            return -1;
        }
    }

    if (AUDIO_IS_ERROR(__fill_audio_playback_info(si, &audio_info))) {
        pa_log_error("can't fill information");
        return -1;
    }

    pa_cvolume_set(&cv, si->sample_spec.channels, pa_sw_volume_from_linear(volume_linear));
    pa_sink_input_set_smooth_volume(si, &cv, duration, TRUE, TRUE);
    __free_audio_info(&audio_info);

    return 0;
}

static int policy_volume_fade_by_type(struct userdata* u, uint32_t volume_type, uint32_t up_down, uint32_t duration)
{
    uint32_t idx;
    pa_sink_input *si = NULL;

    int ret = 1;
    audio_info_t audio_info;
    uint32_t volume_level = 0;
    double volume_linear = 1.0f;
    audio_return_t audio_ret = AUDIO_RET_OK;
    uint32_t dummy;

    if (volume_type >= AUDIO_VOLUME_TYPE_MAX) {
        pa_log_error("invalid volume_type(%d)", volume_type);
        return AUDIO_ERR_PARAMETER;
    }

    /*
    (0): fade-none        - fade-none
    (1): fade-in          - fade-in
    (2): fade-out         - fade-out
    */
    if (up_down >= FADE_MAX) {
        pa_log_error("invalid up_down(%d)", up_down);
        return -1;
    } else {
        if (up_down == FADE_OUT || up_down == FADE_OUT_HALF) {
            u->fade_rcount[volume_type]++;
            if (u->fade_rcount[volume_type] > 1) {
                pa_log_info("fade-out requested twice for volume_type(%d), count(%d)", volume_type, u->fade_rcount[volume_type]);
                return 1;
            }
        } else if (up_down == FADE_IN) {
            if (u->fade_rcount[volume_type] > 0)
                u->fade_rcount[volume_type]--;
            if (u->fade_rcount[volume_type] > 0) {
                pa_log_info("fade-in requested twice for volume_type(%d). skip to fade-up. count(%d)", volume_type, u->fade_rcount[volume_type]);
                return -1;
            }
        } else {
            pa_log_error("unknown fade info. volume_type(%d), count(%d)", volume_type, u->fade_rcount[volume_type]);
            return -1;
        }
    }

    if (policy_get_volume_level(u, (uint32_t)-1, &volume_type, &volume_level) == AUDIO_ERR_UNDEFINED) {
        pa_log_error("can't get volume level");
        return -1;
    }

    pa_log_info ("policy_volume_fade_by_type. volume_type(%d), up_down(%d), volume_level(%d), duration(%d)", volume_type, up_down, volume_level, duration);

    PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {
        if (si == NULL) {
            pa_log_warn("sink-input is null");
            continue;
        }

        if (AUDIO_IS_ERROR(__fill_audio_playback_info(si, &audio_info))) {
            /* skip mono sink-input */
            continue;
        }

        /* Update volume of stream if it has requested volume type */
        if (audio_info.stream.volume_type == volume_type) {
            pa_cvolume cv;
            if (policy_get_volume_value(u, si->index, &volume_type, &volume_level, &volume_linear) == AUDIO_ERR_UNDEFINED) {
                pa_log_debug("can't get volume value");
                return -1;
            } else {
                /* 0:down, 1:up, 2:half */
                if (up_down == FADE_OUT) {
                    pa_idxset_put(u->fade_streams, si, &dummy);
                    volume_linear = 0.0f;
                    /* mute */
                    pa_cvolume_set(&cv, si->sample_spec.channels, pa_sw_volume_from_linear(volume_linear));
                    pa_sink_input_set_smooth_mute(si, TRUE, duration, TRUE);
                    pa_log_info("sink-input[%d] fade_out. linear(%f)", si->index, volume_linear);
                    ret = 0;
                } else if (up_down == FADE_IN) {
                    pa_idxset_remove_by_data(u->fade_streams, si, NULL);
                    pa_idxset_remove_by_data(u->half_fade_streams, si, NULL);

                    /* exception case. sink that is connected to LPA should be set max volume */
                    if (sink_is_highlatency(si->sink)) {
                        volume_linear = 1.0f;
                    } else {
                        volume_linear = volume_linear;
                    }

                    if (pa_sink_input_get_mute(si)) {
                        pa_cvolume_set(&cv, si->sample_spec.channels, pa_sw_volume_from_linear(volume_linear));
                        pa_sink_input_set_volume(si, &cv, TRUE, TRUE);
                        pa_sink_input_set_smooth_mute(si, FALSE, duration, TRUE);
                    } else {
                        pa_cvolume_set(&cv, si->sample_spec.channels, pa_sw_volume_from_linear(volume_linear));
                        pa_sink_input_set_smooth_volume(si, &cv, duration, TRUE, TRUE);
                    }

                    pa_log_info("sink-input[%d] fade_in. linear(%f)", si->index, volume_linear);
                    ret = 1;
                } else if (up_down == FADE_OUT_HALF) {
                    pa_idxset_put(u->half_fade_streams, si, &dummy);

                    if (sink_is_highlatency(si->sink))
                        volume_linear *= 0.98f; /* because volume would be adjusted twice by HW/SW*/
                    else
                        volume_linear *= 0.18f;

                    /* volume down */
                    pa_cvolume_set(&cv, si->sample_spec.channels, pa_sw_volume_from_linear(volume_linear));
                    pa_sink_input_set_smooth_volume(si, &cv, duration, TRUE, TRUE);
                    pa_log_info("sink-input[%d] fade_out-half volume. linear(%f)", si->index, volume_linear);
                    ret = 0;
                } else {
                    pa_log_error("unknown fade info.");
                    return -1;
                }
            }
        }
        __free_audio_info(&audio_info);
    }

    return ret; /* negative : error, 0 = do fade, 1 = skip to fade */
}

static pa_bool_t policy_is_available_high_latency(struct userdata *u)
{
    pa_sink_input *si = NULL;
    pa_sink *sink = NULL;
    uint32_t idx;
    const char *si_policy_str;

    sink = pa_namereg_get(u->core, SINK_HIGH_LATENCY, PA_NAMEREG_SINK);
    if(!sink) {
        pa_log_info("LPA device is not available. normal device will be selected.");
        return FALSE;
    }

    PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {
        if ((si_policy_str = pa_proplist_gets(si->proplist, PA_PROP_MEDIA_POLICY))) {
            if (pa_streq(si_policy_str, POLICY_HIGH_LATENCY) && sink_is_highlatency(si->sink)) {
                pa_log_info("high latency device is in use.");
                return FALSE;
            }
        }
    }

    return TRUE;
}

static void policy_set_vsp(struct userdata *u, uint32_t stream_idx, uint32_t value)
{
    pa_sink_input *si = NULL;
    audio_info_t audio_info;

    if ((si = pa_idxset_get_by_index(u->core->sink_inputs, stream_idx))) {
        if (AUDIO_IS_ERROR(__fill_audio_playback_info(si, &audio_info))) {
            pa_log_debug("skip set_vsp to sink-input[%d]", stream_idx);
            return;
        }
        if (u->audio_mgr.intf.set_vsp != NULL)
            u->audio_mgr.intf.set_vsp(u->audio_mgr.data, &audio_info, value);

        __free_audio_info(&audio_info);
    }

    return;
}

static void policy_set_soundalive_filter_action(struct userdata *u, uint32_t stream_idx, uint32_t value)
{
    pa_sink_input *si = NULL;
    audio_info_t audio_info;

    if ((si = pa_idxset_get_by_index(u->core->sink_inputs, stream_idx))) {
        if (AUDIO_IS_ERROR(__fill_audio_playback_info(si, &audio_info))) {
            pa_log_debug("skip set_soundalive_filter_action to sink-input[%d]", stream_idx);
            return;
        }
        if (u->audio_mgr.intf.set_soundalive_filter_action != NULL)
            u->audio_mgr.intf.set_soundalive_filter_action(u->audio_mgr.data, &audio_info, value);

        __free_audio_info(&audio_info);
    }

    return;
}

static void policy_set_soundalive_preset_mode(struct userdata *u, uint32_t stream_idx, uint32_t value)
{
    pa_sink_input *si = NULL;
    audio_info_t audio_info;

    if ((si = pa_idxset_get_by_index(u->core->sink_inputs, stream_idx))) {
        if (AUDIO_IS_ERROR(__fill_audio_playback_info(si, &audio_info))) {
            pa_log_debug("skip set_soundalive_preset_mode to sink-input[%d]", stream_idx);
            return;
        }
        if (u->audio_mgr.intf.set_soundalive_preset_mode != NULL)
            u->audio_mgr.intf.set_soundalive_preset_mode(u->audio_mgr.data, &audio_info, value);

        __free_audio_info(&audio_info);
    }

    return;
}

static void policy_set_soundalive_equalizer(struct userdata *u, uint32_t stream_idx, uint32_t* eq)
{
    pa_sink_input *si = NULL;
    audio_info_t audio_info;

    pa_return_if_fail(eq);

    if ((si = pa_idxset_get_by_index(u->core->sink_inputs, stream_idx))) {
        if (AUDIO_IS_ERROR(__fill_audio_playback_info(si, &audio_info))) {
            pa_log_debug("skip set_soundalive_equalizer to sink-input[%d]", stream_idx);
            return;
        }
        if (u->audio_mgr.intf.set_soundalive_equalizer != NULL)
            u->audio_mgr.intf.set_soundalive_equalizer(u->audio_mgr.data, &audio_info, eq);

        __free_audio_info(&audio_info);
    }

    return;
}

static void policy_set_soundalive_extend(struct userdata *u, uint32_t stream_idx, uint32_t* ext)
{
    pa_sink_input *si = NULL;
    audio_info_t audio_info;

    pa_return_if_fail(ext);

    if ((si = pa_idxset_get_by_index(u->core->sink_inputs, stream_idx))) {
        if (AUDIO_IS_ERROR(__fill_audio_playback_info(si, &audio_info))) {
            pa_log_debug("skip set_soundalive_extend to sink-input[%d]", stream_idx);
            return;
        }
        if (u->audio_mgr.intf.set_soundalive_extend != NULL)
            u->audio_mgr.intf.set_soundalive_extend(u->audio_mgr.data, &audio_info, ext);

        __free_audio_info(&audio_info);
    }

    return;
}

static void policy_set_soundalive_device(struct userdata *u, uint32_t stream_idx, uint32_t value)
{
    pa_sink_input *si = NULL;
    audio_info_t audio_info;

    if ((si = pa_idxset_get_by_index(u->core->sink_inputs, stream_idx))) {
        if (AUDIO_IS_ERROR(__fill_audio_playback_info(si, &audio_info))) {
            pa_log_debug("skip set_soundalive_device to sink-input[%d]", stream_idx);
            return;
        }
        if (u->audio_mgr.intf.set_soundalive_device != NULL)
            u->audio_mgr.intf.set_soundalive_device(u->audio_mgr.data, &audio_info, value);

        __free_audio_info(&audio_info);
    }

    return;
}

static void policy_set_soundalive_square(struct userdata *u, uint32_t stream_idx, uint32_t row, uint32_t col)
{
    pa_sink_input *si = NULL;
    audio_info_t audio_info;

    if ((si = pa_idxset_get_by_index(u->core->sink_inputs, stream_idx))) {
        if (AUDIO_IS_ERROR(__fill_audio_playback_info(si, &audio_info))) {
            pa_log_debug("skip set_soundalive_square to sink-input[%d]", stream_idx);
            return;
        }
        if (u->audio_mgr.intf.set_soundalive_square != NULL)
            u->audio_mgr.intf.set_soundalive_square(u->audio_mgr.data, &audio_info, row, col);

        __free_audio_info(&audio_info);
    }

    return;
}

static void policy_set_dha_param(struct userdata *u, uint32_t stream_idx, uint32_t onoff, uint32_t *gain)
{
    pa_sink_input *si = NULL;
    audio_info_t audio_info;

    pa_return_if_fail(gain);

    if ((si = pa_idxset_get_by_index(u->core->sink_inputs, stream_idx))) {
        if (AUDIO_IS_ERROR(__fill_audio_playback_info(si, &audio_info))) {
            pa_log_debug("skip set_dha_param to sink-input[%d]", stream_idx);
            return;
        }
        if (u->audio_mgr.intf.set_dha_param != NULL)
            u->audio_mgr.intf.set_dha_param(u->audio_mgr.data, &audio_info, onoff, gain);

        __free_audio_info(&audio_info);
    }

    return;
}

#ifdef SUPPORT_HDMI
static void policy_unload_hdmi(struct userdata *u) {
    struct pa_alsa_sink_info *sink_info = NULL;
    struct pa_alsa_sink_info *sink_info_n = NULL;
    pa_sink_input *i = NULL;
    pa_sink *sink_null = NULL,*sink_tmp = NULL;
    int idx = 0;
    pa_sink *uhqa_sink = NULL;
    pa_sink *null_sink = NULL;

    PA_LLIST_FOREACH_SAFE(sink_info, sink_info_n, u->alsa_sinks) {
        if (sink_info->name && pa_streq(sink_info->name, SINK_HDMI_UHQA)) {
            pa_log_info("UHQA HDMI sink[%s] found, now unloading this", sink_info->name);
            sink_tmp = (pa_sink *)pa_namereg_get(u->core, sink_info->name, PA_NAMEREG_SINK);
            sink_null = (pa_sink *)pa_namereg_get(u->core, "null", PA_NAMEREG_SINK);
            if(sink_null == NULL || sink_tmp == NULL){
                pa_log_warn("Null sink is not available");
                break;
            }
            pa_log_info("Moving all the sink-input connected to HDMI uhqa sink to null-sink");
            PA_IDXSET_FOREACH(i, sink_tmp->inputs, idx) {
                pa_sink_input_move_to(i, sink_null, TRUE);
            }
            pa_sink_suspend(sink_tmp,TRUE,PA_SUSPEND_APPLICATION);
            PA_LLIST_REMOVE(struct pa_alsa_sink_info, u->alsa_sinks, sink_info);
            pa_module_unload(u->core, sink_info->sink, TRUE);
            pa_xfree(sink_info->pcm_device);
            pa_xfree(sink_info->name);
            pa_xfree(sink_info);

            sink_info = NULL;
            sink_info_n = NULL;
            i = NULL;
            sink_null = NULL;
            sink_tmp = NULL;
            idx = 0;
            break;
        }
    }

    PA_LLIST_FOREACH_SAFE(sink_info, sink_info_n, u->alsa_sinks) {
        if (sink_info->name && pa_streq(sink_info->name, SINK_HDMI)) {
            pa_log_info("HDMI sink[%s] found, now unloading this", sink_info->name);
            sink_tmp = (pa_sink *)pa_namereg_get(u->core, sink_info->name, PA_NAMEREG_SINK);
            sink_null = (pa_sink *)pa_namereg_get(u->core, "null", PA_NAMEREG_SINK);
            if (sink_null == NULL || sink_tmp == NULL) {
                pa_log_warn("Null sink is not available");
                break;
            }
            PA_LLIST_REMOVE(struct pa_alsa_sink_info, u->alsa_sinks, sink_info);

            pa_log_info("Moving all the sink-input connected to HDMI sink to null-sink");
            PA_IDXSET_FOREACH (i, sink_tmp->inputs, idx) {
                pa_sink_input_move_to(i, sink_null, TRUE);
            }

            pa_sink_suspend(sink_tmp,TRUE,PA_SUSPEND_APPLICATION);

            pa_module_unload(u->core, sink_info->sink, TRUE);

            pa_xfree(sink_info->pcm_device);
            pa_xfree(sink_info->name);
            pa_xfree(sink_info);
            break;
        }
    }
}
#endif
#define EXT_VERSION 1

static int extension_cb(pa_native_protocol *p, pa_module *m, pa_native_connection *c, uint32_t tag, pa_tagstruct *t) {
    struct userdata *u = NULL;
    uint32_t command;
    pa_tagstruct *reply = NULL;

    pa_sink_input *si = NULL;
    pa_sink *s = NULL;
    uint32_t idx;

    pa_assert(p);
    pa_assert(m);
    pa_assert(c);
    pa_assert(t);

    u = m->userdata;

    if (u->audio_mgr.dl_handle && !(u->audio_mgr.data)) {
        pa_log_error("audio-hal exist but audio-hal loading failed. audio_mgr(%x)", u->audio_mgr.data);
        goto fail;
    }

    if (pa_tagstruct_getu32(t, &command) < 0)
        goto fail;

    reply = pa_tagstruct_new(NULL, 0);
    pa_tagstruct_putu32(reply, PA_COMMAND_REPLY);
    pa_tagstruct_putu32(reply, tag);

    switch (command) {
        case SUBCOMMAND_TEST: {
            if (!pa_tagstruct_eof(t))
                goto fail;

            pa_tagstruct_putu32(reply, EXT_VERSION);
            break;
        }

        case SUBCOMMAND_PLAY_SAMPLE: {
            const char *name;
            uint32_t volume_type = 0;
            uint32_t gain_type = 0;
            uint32_t volume_level = 0;
            uint32_t stream_idx = PA_INVALID_INDEX;

            if (pa_tagstruct_gets(t, &name) < 0 ||
                pa_tagstruct_getu32(t, &volume_type) < 0 ||
                pa_tagstruct_getu32(t, &gain_type) < 0 ||
                pa_tagstruct_getu32(t, &volume_level) < 0 ||
                !pa_tagstruct_eof(t)) {
                pa_log_error("protocol error");
                goto fail;
            }

            policy_play_sample(u, c, name, volume_type, gain_type, volume_level, &stream_idx);

            pa_tagstruct_putu32(reply, stream_idx);
            break;
        }

#ifndef PROFILE_WEARABLE
        case SUBCOMMAND_PLAY_SAMPLE_CONTINUOUSLY: {
            const char *name;
            pa_bool_t start;
            uint32_t volume_type = 0;
            uint32_t gain_type = 0;
            uint32_t volume_level = 0;
            uint32_t stream_idx = PA_INVALID_INDEX;
            pa_usec_t interval;
            uint32_t sound_status = 0;

            if (pa_tagstruct_gets(t, &name) < 0 ||
                pa_tagstruct_get_boolean(t, &start) < 0 ||
                pa_tagstruct_getu32(t, &volume_type) < 0 ||
                pa_tagstruct_getu32(t, &gain_type) < 0 ||
                pa_tagstruct_getu32(t, &volume_level) < 0 ||
                pa_tagstruct_get_usec(t, &interval) < 0 ||
                !pa_tagstruct_eof(t)) {
                pa_log_error("protocol error");
                goto fail;
            }

            /* FIXME if open sco can't route*/
            if (vconf_get_int(SOUND_STATUS_KEY, &sound_status)) {
                pa_log_warn("SOUND_STATUS_KEY vconf get fail");
                goto fail;
            } else if(sound_status & ASM_STATUS_VOIP) {
                pa_log_info("during voip can't route");
            } else {
                /*When play sample continuous is in running state another instance is not allowed*/
                if (start == TRUE) {
                    if (u->audio_sample_userdata.is_running == FALSE) {
                        /* Now it is time to prepare burstshot...set burstshot vconf */
                        if (vconf_set_int(VCONF_SOUND_BURSTSHOT, 1)) {
                            pa_log_warn("vconf_set_int(%s) failed of errno = %d", VCONF_SOUND_BURSTSHOT, vconf_get_ext_errno());
                        }

                        pa_log_warn("play_sample_continuously start. name(%s), vol_type(%d), gain_type(%d), vol_level(%d), interval(%lu ms)",
                            name, volume_type, gain_type, volume_level, (unsigned long) (interval / PA_USEC_PER_MSEC));
                        policy_play_sample_continuously(u, c, name, interval, volume_type, gain_type, volume_level, &stream_idx);

                        /* Running false after start means, start failed....unset burstshot vconf */
                        if (u->audio_sample_userdata.is_running == FALSE) {
                            if (vconf_set_int(VCONF_SOUND_BURSTSHOT, 0)) {
                                pa_log_warn("vconf_set_int(%s) failed of errno = %d", VCONF_SOUND_BURSTSHOT, vconf_get_ext_errno());
                            }
                        }
                    } else {
                        pa_log_warn("play_sample_continuously is in running state - do nothing");
                    }
                } else if ((start == FALSE) && (u->audio_sample_userdata.is_running == TRUE)) {
                    pa_log_warn("play_sample_continuously end.");
                    policy_stop_sample_continuously(u);
                } else {
                    pa_log_error("play sample continuously unknown command. name(%s), start(%d)", name, start);
                }
            }
            pa_tagstruct_putu32(reply, stream_idx);
            break;
        }
#endif
        case SUBCOMMAND_MONO: {

            pa_bool_t enable;

            if (pa_tagstruct_get_boolean(t, &enable) < 0)
                goto fail;

            pa_log_debug("new mono value = %d\n", enable);
            if (enable == !!u->core->is_mono) {
                pa_log_debug("No changes in mono value = %d", u->core->is_mono);
                break;
            }

            u->core->is_mono = enable;
            break;
        }

        case SUBCOMMAND_BALANCE: {
            unsigned i;
            float balance;
            float x;
            const float EPSINON= 0.00000001;
            pa_cvolume cvol;
            const pa_cvolume *scvol;
            pa_channel_map map;

            if (pa_tagstruct_get_cvolume(t, &cvol) < 0)
                goto fail;

            pa_channel_map_init_stereo(&map);
            balance = pa_cvolume_get_balance(&cvol, &map);

            pa_log_debug("new balance value = [%f]\n", balance);

            x = u->balance - balance;
            if ((x >= - EPSINON)&& (x <= EPSINON)) {
                pa_log_debug ("No changes in balance value = [%f]", u->balance);
                break;
            }

            u->balance = balance;

            /* Apply balance value to each Sinks */
            PA_IDXSET_FOREACH(s, u->core->sinks, idx) {
                scvol = pa_sink_get_volume(s, FALSE);
                for (i = 0; i < scvol->channels; i++) {
                    cvol.values[i] = scvol->values[i];
                }
                cvol.channels = scvol->channels;

                pa_cvolume_set_balance(&cvol, &s->channel_map, u->balance);
                pa_sink_set_volume(s, &cvol, TRUE, TRUE);
            }
            break;
        }
        case SUBCOMMAND_MUTEALL: {
            const char *si_gain_type_str;
            pa_bool_t enable;
            unsigned i;
            uint32_t gain_type;

            if (pa_tagstruct_get_boolean(t, &enable) < 0)
                goto fail;

            pa_log_debug("new muteall value = %d\n", enable);
            if (enable == u->muteall) {
                pa_log_debug("No changes in muteall value = %d", u->muteall);
                break;
            }

            u->muteall = enable;

/* Use mute instead of volume for muteall */
#if 1
            for (i = 0; i < AUDIO_VOLUME_TYPE_MAX; i++) {
                policy_set_mute(u, (-1), i, AUDIO_DIRECTION_OUT, u->muteall, false);
            }
            PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {
                /* Skip booting sound for power off mute streams policy. */
                if (u->muteall && (si_gain_type_str = pa_proplist_gets(si->proplist, PA_PROP_MEDIA_TIZEN_GAIN_TYPE))) {
                    pa_atou(si_gain_type_str, &gain_type);
                    if (gain_type == AUDIO_GAIN_TYPE_BOOTING)
                        continue;
                }
                pa_sink_input_set_mute(si, u->muteall, TRUE);
            }
#else
            /* Apply new volume  value to each Sink_input */
            if (u->muteall) {
                PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {
                    scvol = pa_sink_input_get_volume (si, &cvol,TRUE);
                    for (i = 0; i < scvol->channels; i++) {
                        scvol->values[i] = 0;
                    }
                    pa_sink_input_set_volume(si,scvol,TRUE,TRUE);
                }
            } else {
                PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {
                    if (pa_streq(si->module->name,"module-remap-sink")) {
                        scvol = pa_sink_input_get_volume (si, &cvol,TRUE);
                        for (i = 0; i < scvol->channels; i++) {
                            scvol->values[i] = MAX_VOLUME_FOR_MONO;
                        }
                        pa_sink_input_set_volume(si,scvol,TRUE,TRUE);
                    }
                }
            }
#endif
            break;
        }

        case SUBCOMMAND_SET_USE_CASE: {
            pa_log_warn("deprecated api");
            break;
        }

        case SUBCOMMAND_SET_SESSION: {
            uint32_t session = 0;
            uint32_t start = 0;

            pa_tagstruct_getu32(t, &session);
            pa_tagstruct_getu32(t, &start);

            policy_set_session(u, session, start);
            break;
        }

        case SUBCOMMAND_SET_SUBSESSION: {
            uint32_t subsession = 0;
            uint32_t subsession_opt = 0;

            pa_tagstruct_getu32(t, &subsession);
            pa_tagstruct_getu32(t, &subsession_opt);

            policy_set_subsession(u, subsession, subsession_opt);
            break;
        }

        case SUBCOMMAND_SET_ACTIVE_DEVICE: {
            uint32_t device_in = 0;
            uint32_t device_out = 0;
            uint32_t is_start = 0;
            uint32_t need_update = FALSE;

            pa_tagstruct_getu32(t, &device_in);
            pa_tagstruct_getu32(t, &device_out);
            pa_tagstruct_getu32(t, &is_start);

            policy_set_active_device(u, device_in, device_out, is_start, &need_update);

            pa_tagstruct_putu32(reply, need_update);
            break;
        }

        case SUBCOMMAND_RESET: {

            policy_reset(u);

            break;
        }

        case SUBCOMMAND_GET_VOLUME_LEVEL_MAX: {
            uint32_t volume_type = 0;
            uint32_t volume_level = 0;

            pa_tagstruct_getu32(t, &volume_type);

            policy_get_volume_level_max(u, volume_type, &volume_level);

            pa_tagstruct_putu32(reply, volume_level);
            break;
        }

        case SUBCOMMAND_GET_VOLUME_LEVEL: {
            uint32_t stream_idx = PA_INVALID_INDEX;
            uint32_t volume_type = 0;
            uint32_t volume_level = 0;

            pa_tagstruct_getu32(t, &stream_idx);
            pa_tagstruct_getu32(t, &volume_type);

            policy_get_volume_level(u, stream_idx, &volume_type, &volume_level);

            pa_tagstruct_putu32(reply, volume_level);
            break;
        }

        case SUBCOMMAND_SET_VOLUME_LEVEL: {
            uint32_t stream_idx = PA_INVALID_INDEX;
            uint32_t volume_type = 0;
            uint32_t volume_level = 0;

            pa_tagstruct_getu32(t, &stream_idx);
            pa_tagstruct_getu32(t, &volume_type);
            pa_tagstruct_getu32(t, &volume_level);

            policy_set_volume_level(u, stream_idx, volume_type, volume_level);
            break;
        }

        case SUBCOMMAND_UPDATE_VOLUME: {
            policy_update_volume(u);
            break;
        }

        case SUBCOMMAND_GET_MUTE: {
            uint32_t stream_idx = PA_INVALID_INDEX;
            uint32_t volume_type = 0;
            uint32_t direction = 0;
            uint32_t mute = 0;

            pa_tagstruct_getu32(t, &stream_idx);
            pa_tagstruct_getu32(t, &volume_type);
            pa_tagstruct_getu32(t, &direction);

            policy_get_mute(u, stream_idx, volume_type, direction, &mute);

            pa_tagstruct_putu32(reply, mute);
            break;
        }

        case SUBCOMMAND_SET_MUTE: {
            uint32_t stream_idx = PA_INVALID_INDEX;
            uint32_t volume_type = 0;
            uint32_t direction = 0;
            uint32_t mute = 0;

            pa_tagstruct_getu32(t, &stream_idx);
            pa_tagstruct_getu32(t, &volume_type);
            pa_tagstruct_getu32(t, &direction);
            pa_tagstruct_getu32(t, &mute);

            policy_set_mute(u, stream_idx, volume_type, direction, mute, true);
            break;
        }
        case SUBCOMMAND_SET_TOUCH_SOUND: {
            pa_bool_t enable = FALSE;

            if (pa_tagstruct_get_boolean(t, &enable) < 0)
                goto fail;

            u->core->touch_sound = enable;
            pa_log_info ("touch sound is %s now\n", enable ? "enable" : "disable");

            break;
        }
        case SUBCOMMAND_VOLUME_FADE: {
            uint32_t stream_idx = PA_INVALID_INDEX;
            uint32_t up_down = 0;
            uint32_t duration = 0; // msec

            if (pa_tagstruct_getu32(t, &stream_idx) < 0)
                goto fail;
            if (pa_tagstruct_getu32(t, &up_down) < 0)
                goto fail;
            if (pa_tagstruct_getu32(t, &duration) < 0)
                goto fail;

            policy_volume_fade(u, stream_idx, up_down, duration);
            break;
        }
        case SUBCOMMAND_VOLUME_FADE_BY_TYPE: {
            uint32_t volume_type = 0;
            uint32_t up_down = 0;
            uint32_t duration = 0; // msec
            audio_return_t ret = 0;

            if (pa_tagstruct_getu32(t, &volume_type) < 0)
                goto fail;
            if (pa_tagstruct_getu32(t, &up_down) < 0)
                goto fail;
            if (pa_tagstruct_getu32(t, &duration) < 0)
                goto fail;

            ret = policy_volume_fade_by_type(u, volume_type, up_down, duration);
            pa_tagstruct_putu32(reply, ret);
            break;
        }
        case SUBCOMMAND_IS_AVAILABLE_HIGH_LATENCY: {
            pa_bool_t available = FALSE;

            available = policy_is_available_high_latency(u);

            pa_tagstruct_putu32(reply, (uint32_t)available);
            break;
        }
        case SUBCOMMAND_VSP_SPEED : {
            uint32_t stream_idx = PA_INVALID_INDEX;
            uint32_t value;

            if (pa_tagstruct_getu32(t, &stream_idx) < 0)
                goto fail;
            if (pa_tagstruct_getu32(t, &value) < 0)
                goto fail;

            policy_set_vsp(u, stream_idx, value);
            break;
        }

        case SUBCOMMAND_SA_FILTER_ACTION: {
            uint32_t stream_idx = PA_INVALID_INDEX;
            uint32_t value;

            if (pa_tagstruct_getu32(t, &stream_idx) < 0)
                goto fail;
            if (pa_tagstruct_getu32(t, &value) < 0)
                goto fail;

            policy_set_soundalive_filter_action(u, stream_idx, value);
            break;
        }

        case SUBCOMMAND_SA_PRESET_MODE: {
            uint32_t stream_idx = PA_INVALID_INDEX;
            uint32_t value;

            if (pa_tagstruct_getu32(t, &stream_idx) < 0)
                goto fail;
            if (pa_tagstruct_getu32(t, &value) < 0)
                goto fail;

            policy_set_soundalive_preset_mode(u, stream_idx, value);
            break;
        }

        case SUBCOMMAND_SA_EQ: {
            uint32_t stream_idx = PA_INVALID_INDEX;
            int i = 0;
            uint32_t eq[EQ_USER_SLOT_NUM];

            if (pa_tagstruct_getu32(t, &stream_idx) < 0)
                goto fail;
            for (i = 0; i < EQ_USER_SLOT_NUM; i++) {
                if (pa_tagstruct_getu32(t, &eq[i]) < 0)
                    goto fail;
            }
            policy_set_soundalive_equalizer(u, stream_idx, eq);
            break;
        }

        case SUBCOMMAND_SA_EXTEND: {
            uint32_t stream_idx = PA_INVALID_INDEX;
            int i = 0;
            uint32_t ext[CUSTOM_EXT_PARAM_MAX];

            if (pa_tagstruct_getu32(t, &stream_idx) < 0)
                goto fail;
            for (i = 0; i < CUSTOM_EXT_PARAM_MAX; i++) {
                if (pa_tagstruct_getu32(t, &ext[i]) < 0)
                    goto fail;
            }

            policy_set_soundalive_extend(u, stream_idx, ext);
            break;
        }

        case SUBCOMMAND_SA_DEVICE: {
            uint32_t stream_idx = PA_INVALID_INDEX;
            uint32_t value;

            if (pa_tagstruct_getu32(t, &stream_idx) < 0)
                goto fail;
            if (pa_tagstruct_getu32(t, &value) < 0)
                goto fail;

            policy_set_soundalive_device(u, stream_idx, value);
            break;
        }

        case SUBCOMMAND_SA_SQUARE: {
            uint32_t stream_idx = PA_INVALID_INDEX;
            uint32_t row, col;

            if (pa_tagstruct_getu32(t, &stream_idx) < 0)
                goto fail;
            if (pa_tagstruct_getu32(t, &row) < 0)
                goto fail;
            if (pa_tagstruct_getu32(t, &col) < 0)
                goto fail;

            policy_set_soundalive_square(u, stream_idx, row, col);
            break;
        }

        case SUBCOMMAND_DHA_PARAM: {
            uint32_t stream_idx = PA_INVALID_INDEX;
            uint32_t onoff = 0;
            int i = 0;
            uint32_t gain[DHA_GAIN_NUM];

            if (pa_tagstruct_getu32(t, &stream_idx) < 0)
                goto fail;
            if (pa_tagstruct_getu32(t, &onoff) < 0)
                goto fail;
            for (i = 0; i < DHA_GAIN_NUM; i++) {
                if (pa_tagstruct_getu32(t, &gain[i]) < 0)
                    goto fail;
            }
            policy_set_dha_param(u, stream_idx, onoff, gain);
            break;
        }

        case SUBCOMMAND_UNLOAD_HDMI: {
#ifdef SUPPORT_HDMI
            policy_unload_hdmi(u);
#endif
            break;
        }

        case SUBCOMMAND_SET_CALL_NETWORK_TYPE: {
            uint32_t type;

            if (pa_tagstruct_getu32(t, &type) < 0)
                goto fail;

            u->call_type = type;
            pa_log_info ("network type is %d\n", type);

            pa_tagstruct_putu32(reply, type);
            break;
        }

        case SUBCOMMAND_SET_CALL_NREC: {
            uint32_t is_nrec;

            if (pa_tagstruct_getu32(t, &is_nrec) < 0)
                goto fail;

            u->call_nrec = is_nrec;
            pa_log_info ("call noise reduction is %s\n", is_nrec ? "true" : "false");

            pa_tagstruct_putu32(reply, is_nrec);
            break;
        }

        case SUBCOMMAND_SET_CALL_EXTRA_VOLUME: {
            uint32_t is_extra_volume;
            uint32_t route_flag = 0;

            if (pa_tagstruct_getu32(t, &is_extra_volume) < 0)
                goto fail;

            u->call_extra_volume = is_extra_volume;
            pa_log_info ("call extra volume is %s\n", is_extra_volume ? "true" : "false");

            if (u->subsession == AUDIO_SUBSESSION_VOICE) {
                route_flag = __get_route_flag(u);

                if (u->audio_mgr.intf.set_route) {
                    u->audio_mgr.intf.set_route(u->audio_mgr.data, u->session, u->subsession, u->active_device_in, u->active_device_out, route_flag);
                    u->active_route_flag = route_flag;
                }
            }

            pa_tagstruct_putu32(reply, is_extra_volume);
            break;
        }
        case SUBCOMMAND_SET_NETWORK_LINK_DIRECTION: {
            uint32_t link_direction;

            if (pa_tagstruct_getu32(t, &link_direction) < 0)
                goto fail;

            u->link_direction = link_direction;
            pa_log_info("call link direction %d\n", link_direction);

            if (link_direction == SOUND_LINK_DIRECTION_UP_LINK) {
                if (u->audio_mgr.intf.set_route_info) {
                    u->audio_mgr.intf.set_route_info(u->audio_mgr.data, "", "bike-mode=up-link", NULL);
                } else {
                    pa_log_warn("bike mode not supported\n");
                }
            } else if (link_direction == SOUND_LINK_DIRECTION_DOWN_LINK) {
                if (u->audio_mgr.intf.set_route_info) {
                    u->audio_mgr.intf.set_route_info(u->audio_mgr.data, "", "bike-mode=down-link", NULL);
                } else {
                    pa_log_warn("bike mode not supported\n");
                }
            } else {
                pa_log_error("wrong link direction\n");
            }

            pa_tagstruct_putu32(reply, link_direction);
            break;
        }
        case SUBCOMMAND_SET_BLUETOOTH_BANDWIDTH: {
            uint32_t bt_bandwidth;

            if (pa_tagstruct_getu32(t, &bt_bandwidth) < 0)
                goto fail;

            u->bt_bandwidth = bt_bandwidth;
            pa_log_debug("bt bt_bandwidth is %d\n", bt_bandwidth);

            pa_tagstruct_putu32(reply, bt_bandwidth);
            break;
        }
        case SUBCOMMAND_SET_BLUETOOTH_NREC: {
            uint32_t bt_nrec;

            if (pa_tagstruct_getu32(t, &bt_nrec) < 0)
                goto fail;

            u->bt_nrec = bt_nrec;
            pa_log_debug("bt nrec is %d\n", bt_nrec);

            pa_tagstruct_putu32(reply, bt_nrec);
            break;
        }
        case SUBCOMMAND_SET_ROUTE_INFO: {
            const char *key = NULL;
            const char *value = NULL;
            void* addtional_info = NULL;

            if (pa_tagstruct_gets(t, &key) < 0 ||
                pa_tagstruct_gets(t, &value) < 0 ||
                !pa_tagstruct_eof(t)) {
                pa_log_error("protocol error");
                goto fail;
            }

            if (u->audio_mgr.intf.set_route_info) {
                pa_log_info("set_route_info. key(%s), value(%s), addtional_info(%p)", key, value, addtional_info);
                u->audio_mgr.intf.set_route_info(u->audio_mgr.data, key, value, addtional_info);
                u->active_route_flag = (uint32_t)-1; /* route_flag should be updated. initialize route_flag */
            }
            break;
        }
        default:
            goto fail;
    }

    pa_pstream_send_tagstruct(pa_native_connection_get_pstream(c), reply);
    return 0;

    fail:

    if (reply)
        pa_tagstruct_free(reply);

    return -1;
}

/*  Called when new sink-input is creating  */
static pa_hook_result_t sink_input_new_hook_callback(pa_core *c, pa_sink_input_new_data *new_data, struct userdata *u)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    audio_info_t audio_info;
    const char *policy = NULL;
    const char *ignore_preset_sink = NULL;
    const char *master_name = NULL;
    pa_sink *realsink = NULL;
    uint32_t volume_level = 0;
    pa_strbuf *s = NULL;
    const char *rate_str = NULL;
    const char *ch_str = NULL;
    char *s_info = NULL;

    pa_assert(c);
    pa_assert(new_data);
    pa_assert(u);

    if (!new_data->proplist) {
        pa_log_debug(" New stream lacks property data.");
        return PA_HOOK_OK;
    }

    /* If no policy exists, skip */
    if (!(policy = pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_POLICY))) {
        pa_log_debug("Not setting device for stream [%s], because it lacks policy.",
                pa_strnull(pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_NAME)));
        return PA_HOOK_OK;
    }

    /* Parse request formats for samplerate & channel infomation */
    if (new_data->req_formats) {
        pa_format_info* req_format = pa_idxset_first(new_data->req_formats, NULL);
        if (req_format && req_format->plist) {
            rate_str = pa_proplist_gets(req_format->plist, PA_PROP_FORMAT_RATE);
            ch_str = pa_proplist_gets(req_format->plist, PA_PROP_FORMAT_CHANNELS);
            pa_log_info("req rate = %s, req ch = %s", rate_str, ch_str);

            if (ch_str)
                new_data->sample_spec.channels = atoi (ch_str);
            if (rate_str)
                new_data->sample_spec.rate = atoi (rate_str);
        }
    } else {
        pa_log_debug("no request formats available");
    }

    /* Check if this input want to be played via the sink selected by module-policy */
    if ((ignore_preset_sink = pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_POLICY_IGNORE_PRESET_SINK))) {
        pa_log_debug_verbose("ignore_preset_sink is enabled. module-policy will judge a proper sink for stream [%s]",
                pa_strnull(pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_NAME)));
    } else {
        ignore_preset_sink = "no";
    }

    /* If sink-input has already sink, skip */
    if (new_data->sink && (strncmp("yes", ignore_preset_sink, strlen("yes")))) {
        /* sink-input with filter role will be also here because sink is already set */
#ifdef DEBUG_DETAIL
        pa_log_debug(" Not setting device for stream [%s], because already set.",
                pa_strnull(pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_NAME)));
#endif
    } else {

        /* Set proper sink to sink-input */
        new_data->save_sink = FALSE;

#ifdef SUPPORT_UHQA
        /** If input sample rate is more than 96khz its consider UHQA*/
        if (new_data->sample_spec.rate >= UHQA_BASE_SAMPLING_RATE) {

            char uhqa_policy[128] = {0};
            pa_log_info("UHQA stream arrived. rate(%d), ch(%d), format(%d)",
                new_data->sample_spec.rate, new_data->sample_spec.channels, new_data->sample_spec.format);

            /** Create the UHQA sink if not created*/
            create_uhqa_sink(u, policy);

            /** Create a new policy to distinguish it from normal stream*/
            snprintf(uhqa_policy, sizeof(uhqa_policy), "%s-uhqa", policy);
            new_data->sink = policy_select_proper_sink (u, uhqa_policy, NULL, TRUE);
        } else
#endif
        {
            const char *si_volume_type_str = NULL;
            uint32_t volume_type = AUDIO_VOLUME_TYPE_SYSTEM;

            if((si_volume_type_str = pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_TIZEN_VOLUME_TYPE))) {
                pa_atou(si_volume_type_str, &volume_type);
            }

            /* exception case : if fade stream exist, notification should be played though BT */
            if (volume_type == AUDIO_VOLUME_TYPE_NOTIFICATION && !pa_idxset_isempty(u->half_fade_streams)) {
                new_data->sink = policy_select_proper_sink (u, "auto", NULL, TRUE);
            } else {
                new_data->sink = policy_select_proper_sink (u, policy, NULL, TRUE);
            }
        }
        if (new_data->sink == NULL) {
            pa_log_error("new_data->sink is null");
            goto exit;
        }
    }

    s = pa_strbuf_new();
    master_name = pa_proplist_gets(new_data->sink->proplist, PA_PROP_DEVICE_MASTER_DEVICE);
    if (master_name)
        realsink = pa_namereg_get(c, master_name, PA_NAMEREG_SINK);

    if (AUDIO_IS_ERROR((audio_ret = __fill_audio_playback_stream_info(new_data->proplist, &new_data->sample_spec, &audio_info)))) {
        pa_log_debug("__fill_audio_playback_stream_info returns 0x%x", audio_ret);
    } else if (AUDIO_IS_ERROR((audio_ret = __fill_audio_playback_device_info(realsink? realsink->proplist : new_data->sink->proplist, &audio_info)))) {
        pa_log_debug("__fill_audio_playback_device_info returns 0x%x", audio_ret);
    } else {
        double volume_linear = 1.0f;

        // set role type
        __set_sink_input_role_type(new_data->proplist, audio_info.stream.gain_type);

        if (u->audio_mgr.intf.get_volume_level) {
            u->audio_mgr.intf.get_volume_level(u->audio_mgr.data, audio_info.stream.volume_type, &volume_level);
        }

        pa_strbuf_printf(s, "[%s] policy[%s] role[%s] ch[%d] rate[%d] volume&gain[%d,%d] level[%d]",
                audio_info.stream.name, policy, pa_strnull(pa_proplist_gets (new_data->proplist, PA_PROP_MEDIA_ROLE)),
                audio_info.stream.channels, audio_info.stream.samplerate, audio_info.stream.volume_type,
                audio_info.stream.gain_type, volume_level);

        if (audio_info.device.api == AUDIO_DEVICE_API_ALSA) {
            pa_strbuf_printf(s, " device:ALSA[%d,%d]", audio_info.device.alsa.card_idx, audio_info.device.alsa.device_idx);
        } else if (audio_info.device.api == AUDIO_DEVICE_API_BLUEZ) {
            pa_strbuf_printf(s, " device:BLUEZ[%s] nrec[%d]", audio_info.device.bluez.protocol, audio_info.device.bluez.nrec);
        }
        pa_strbuf_printf(s, " sink[%s]", (new_data->sink)? new_data->sink->name : "null");

        /* Call HAL function if exists */
        if (u->audio_mgr.intf.set_volume_level) {
            if (AUDIO_IS_ERROR((audio_ret = u->audio_mgr.intf.set_volume_level(u->audio_mgr.data, &audio_info, audio_info.stream.volume_type, volume_level)))) {
                pa_log_warn("set_volume_level for new sink-input returns error:0x%x", audio_ret);
                goto exit;
            }
        }

        /* Get volume value by type & level */
        if (u->audio_mgr.intf.get_volume_value && (audio_ret != AUDIO_RET_USE_HW_CONTROL)) {
            if (audio_info.stream.volume_type != AUDIO_VOLUME_TYPE_FIXED) {
                if (AUDIO_IS_ERROR((audio_ret = u->audio_mgr.intf.get_volume_value(u->audio_mgr.data, &audio_info, audio_info.stream.volume_type, volume_level, &volume_linear)))) {
                    pa_log_warn("get_volume_value for new sink-input returns error:0x%x", audio_ret);
                    goto exit;
                }
            } else {
                double gain_value = 1.0f;

                /* FIXED volume is for cam-shutter. it always use spk volume.*/
                if (u->audio_mgr.intf.get_gain_value != NULL &&
                    AUDIO_IS_ERROR((audio_ret = u->audio_mgr.intf.get_gain_value(u->audio_mgr.data, &audio_info, audio_info.stream.volume_type, &gain_value)))) {
                    pa_log_warn("get_volume_value for new sink-input returns error:0x%x", audio_ret);
                    goto exit;
                }
                volume_linear *= gain_value;
            }
        }

        pa_cvolume_init(&new_data->volume);
        pa_cvolume_set(&new_data->volume, new_data->sample_spec.channels, pa_sw_volume_from_linear(volume_linear));

        new_data->volume_is_set = TRUE;

        pa_strbuf_printf(s, " volume_linear(%f)", volume_linear);

        if (u->muteall && audio_info.stream.volume_type != AUDIO_VOLUME_TYPE_FIXED) {
            pa_sink_input_new_data_set_muted(new_data, TRUE); // pa_simpe api use muted stream always. for play_sample_xxx apis
        }

        __free_audio_info(&audio_info);
    }

exit:
    if (s) {
        s_info = pa_strbuf_tostring_free(s);
        pa_secure_log_info("new %s", s_info);
        pa_xfree(s_info);
    }

    return PA_HOOK_OK;
}

#ifdef SUPPORT_UHQA
/** This function create a new UHQA sink based on the policy*/
static void create_uhqa_sink(struct userdata *u, const char* policy)
{
    pa_sink* uhqa_sink = NULL;
    pa_sink* sink = NULL;
    const char *sink_name_uhqa = NULL;
    const char * device= NULL;
    int32_t  start_threshold = -1;

    pa_log_info("Creating UHQA sink policy =%s",policy);

    /** If policy is High latency*/
    if (pa_streq(policy, POLICY_HIGH_LATENCY)) {

        uhqa_sink = policy_get_sink_by_name(u->core, SINK_HIGH_LATENCY_UHQA);

        /** If sink already created no need to create again*/
        if (uhqa_sink != NULL) {
            pa_log_info("UHQA sink already created, policy =%s",policy);
            return;
        }

        sink = policy_get_sink_by_name(u->core, SINK_HIGH_LATENCY);
        sink_name_uhqa = SINK_HIGH_LATENCY_UHQA;
        start_threshold = START_THRESHOLD;
        device = "hw:0,4";
    } else if (pa_streq(policy, POLICY_AUTO)) {    /** If policy is auto*/

        uhqa_sink = policy_get_sink_by_name(u->core, SINK_ALSA_UHQA);

        /** If sink already created no need to create again*/
        if (uhqa_sink != NULL) {
            pa_log_info("UHQA sink already created, policy =%s",policy);
            return;
        }

        sink = policy_get_sink_by_name(u->core, SINK_ALSA);
        sink_name_uhqa = SINK_ALSA_UHQA;
        device = "hw:0,0";
    }

    /** TODO: In future capability check may be added*/
    /** TODO: In future sink will be added to sink_info list*/

    /** Going to create the UHQA sink*/
    if (pa_streq(policy, POLICY_HIGH_LATENCY) || pa_streq(policy, POLICY_AUTO)) {

        pa_strbuf *args_buf= NULL;
        char *args = NULL;

        pa_log_info("Going to create the UHQA sink, policy =%s",policy);

        /** Suspend the normal sink as device might have been already opened*/
        if (sink) {
            pa_sink_suspend(sink, TRUE, PA_SUSPEND_USER);
        }

        args_buf = pa_strbuf_new();

        /**
          * Fill the argbuff sampling rate is fixed for 192khz
          * For UHQA only STEREO supported
          * alsa is supprting only PA_SAMPLE_S24_32LE --"s24-32le"
          * rate and alternate rate are kept same so that sink rate will never be updated if pa_sink_update_rate is called.
          * Because update_rate is kept NULL.
          */
        pa_strbuf_printf(args_buf,
        "sink_name=\"%s\" "
        "device=\"%s\" "
        "rate=%d "
        "channels=%d "
        "sink_properties=\"module-suspend-on-idle.timeout=0\" "
        "format=%s "
        "start_threshold=%d "
        "alternate_rate=%d",

        "alsa_output.0.analog-stereo-uhqa",
        "hw:0,0",
        UHQA_SAMPLING_RATE,
        CH_STEREO,
        pa_sample_format_to_string(PA_SAMPLE_S24_32LE),
        -1,
        UHQA_SAMPLING_RATE);

        args = pa_strbuf_tostring_free(args_buf);

        /* Create a new UHQA sink */
        if (!pa_module_load(u->core, "module-alsa-sink", args)) {
            pa_log_info("module loaded for %s", args);

        } else {
            pa_log_error("Failed to Load module-alsa-sink: %s",sink_name_uhqa);
        }
        if (args) {
            pa_xfree(args);
        }
    }
    return;
}
#endif

static pa_hook_result_t sink_input_unlink_post_hook_callback(pa_core *c, pa_sink_input *i, struct userdata *u)
{
    uint32_t volume_type = 0;
    const char *si_volume_type_str;
    uint32_t gain_type;
    const char *si_gain_type_str;

    pa_assert(c);
    pa_assert(i);
    pa_assert(u);

    if((si_volume_type_str = pa_proplist_gets(i->proplist, PA_PROP_MEDIA_TIZEN_VOLUME_TYPE))) {
        pa_atou(si_volume_type_str, &volume_type);
        __set_primary_volume(u, (void*)i, volume_type, false);
    }

    if ((si_gain_type_str = pa_proplist_gets(i->proplist, PA_PROP_MEDIA_TIZEN_GAIN_TYPE))) {
        pa_atou(si_gain_type_str, &gain_type);
        if (gain_type == AUDIO_GAIN_TYPE_BOOTING) {
            u->core->touch_sound = 1;
        }
    }

    pa_idxset_remove_by_data(u->fade_streams, i, NULL);
    pa_idxset_remove_by_data(u->half_fade_streams, i, NULL);

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_input_put_callback(pa_core *core, pa_sink_input *i, struct userdata *u)
{
    uint32_t volume_type = 0;
    const char *si_volume_type_str;
    uint32_t gain_type;
    const char *si_gain_type_str;

    pa_core_assert_ref(core);
    pa_sink_input_assert_ref(i);
    pa_assert(u);

    if ((si_volume_type_str = pa_proplist_gets(i->proplist, PA_PROP_MEDIA_TIZEN_VOLUME_TYPE)) &&
        pa_sink_input_get_state(i) != PA_SINK_INPUT_CORKED /* if sink-input is created by pulsesink, sink-input init state is cork.*/) {
        pa_atou(si_volume_type_str, &volume_type);
        __set_primary_volume(u, (void*)i, volume_type, true);
    }
    if ((si_gain_type_str = pa_proplist_gets(i->proplist, PA_PROP_MEDIA_TIZEN_GAIN_TYPE))) {
        pa_atou(si_gain_type_str, &gain_type);
        if (gain_type == AUDIO_GAIN_TYPE_BOOTING) {
            u->core->touch_sound = 0;
        }
    }

    return PA_HOOK_OK;
}

/*  Called when new sink is added while sink-input is existing  */
static pa_hook_result_t sink_put_hook_callback(pa_core *c, pa_sink *sink, struct userdata *u)
{
    pa_sink_input *si;
    pa_sink *sink_to_move;
    uint32_t idx;
    unsigned i;
    pa_cvolume cvol;
    const pa_cvolume *scvol;
    pa_bool_t is_bt;
    pa_bool_t is_usb_alsa;
    pa_bool_t is_need_to_move = true;
    int dock_status;
    uint32_t device_out = AUDIO_DEVICE_OUT_BT_A2DP;

    pa_assert(c);
    pa_assert(sink);
    pa_assert(u);
    pa_assert(u->on_hotplug);

    /* If connected sink is BLUETOOTH, set as default */
    /* we are checking with device.api property */
    is_bt = policy_is_bluez(sink);
    is_usb_alsa = policy_is_usb_alsa(sink);

    if (is_bt || is_usb_alsa) {
        if (u->session == AUDIO_SESSION_VOICECALL || u->session == AUDIO_SESSION_VIDEOCALL || u->session == AUDIO_SESSION_VOIP) {
            pa_log_info("current session is communication mode [%d], no need to move", u->session);
            is_need_to_move = false;

            /* keep bluez_sink when BT headset is connected during voicecall*/
            if (is_bt)
                u->bluez_sink = sink;
        } else if (is_usb_alsa) {
            dock_status = __get_dock_type();
            if ((dock_status == DOCK_DESKDOCK) || (dock_status == DOCK_CARDOCK)) {
                device_out = AUDIO_DEVICE_OUT_DOCK;
            } else if (dock_status == DOCK_AUDIODOCK) {
                device_out = AUDIO_DEVICE_OUT_MULTIMEDIA_DOCK;
            } else if (dock_status == DOCK_SMARTDOCK) {
                is_need_to_move = false;
            } else {
                device_out = AUDIO_DEVICE_OUT_USB_AUDIO;
                pa_log_info("This device might be general USB Headset");
            }
        } else if (is_bt) {
            /* sound path will be routed set_active_device func */
            is_need_to_move = false;
            device_out = AUDIO_DEVICE_OUT_BT_A2DP;
            u->bluez_sink = sink;
            pa_sink_set_mute(sink, TRUE, TRUE);
        }
    } else {
        pa_log_debug("this sink [%s][%d] is not a bluez....return", sink->name, sink->index);
        return PA_HOOK_OK;
    }

    if (is_need_to_move) {
        int ret = 0;
        uint32_t route_flag = 0;

        /* Iterate each sink inputs to decide whether we should move to new sink */
        PA_IDXSET_FOREACH(si, c->sink_inputs, idx) {
            const char *policy = NULL;

            if (si->sink == sink)
                continue;

            /* Skip this if it is already in the process of being moved
                    * anyway */
            if (!si->sink)
                continue;

            /* It might happen that a stream and a sink are set up at the
                    same time, in which case we want to make sure we don't
                    interfere with that */
            if (!PA_SINK_INPUT_IS_LINKED(pa_sink_input_get_state(si)))
                continue;

            /* Get role (if role is filter, skip it) */
            if (policy_is_filter(si->proplist))
                continue;

            /* Check policy */
            if (!(policy = pa_proplist_gets(si->proplist, PA_PROP_MEDIA_POLICY))) {
                /* No policy exists, this means auto */
                pa_log_debug("set policy of sink-input[%d] from [%s] to [auto]", si->index, "null");
                policy = POLICY_AUTO;
            }

             /** If UHQA sink input then connect the sink inpu to UHQA sink*/
            if ((si->sample_spec.rate >= UHQA_BASE_SAMPLING_RATE)
                 && (pa_streq(policy, POLICY_HIGH_LATENCY) || pa_streq(policy, POLICY_AUTO))) {

                char tmp_policy[100] = {0};

                sprintf(tmp_policy, "%s-uhqa", policy);

                pa_log_info ("------------------------------------");
                sink_to_move = policy_select_proper_sink (u, tmp_policy, si, TRUE);
            } else {
                pa_log_info ("------------------------------------");
                sink_to_move = policy_select_proper_sink (u, policy, si, TRUE);
            }

            if (sink_to_move) {
                pa_log_debug("Moving sink-input[%d] from [%s] to [%s]", si->index, si->sink->name, sink_to_move->name);
                pa_sink_input_move_to(si, sink_to_move, FALSE);
            } else {
                pa_log_debug("Can't move sink-input....");
            }
        }

        /* Set active device out */
        if (u->active_device_out != device_out) {
            route_flag = __get_route_flag(u);

            if (u->audio_mgr.intf.set_route) {
                ret = u->audio_mgr.intf.set_route(u->audio_mgr.data, u->session, u->subsession, u->active_device_in, device_out, route_flag);
                if (ret >= 0) {
                    pa_log_debug("route failed(normal operation). session(%d), subsession(%d), active_device_out(%d), device_out(%d)\n",
                        u->session, u->subsession, u->active_device_out, device_out);
                    pa_log_info("set default sink to sink[%s][%d], active_device_out(%d), device_out(%d)",
                            sink->name, sink->index, u->active_device_out, device_out);
                    pa_namereg_set_default_sink (c,sink);

                    /* update volume will be called sink_input_move callback */
                    u->active_device_out = device_out;
                    u->active_route_flag = route_flag;
                }
            }
        }
    }

    /* Reset sink volume with balance from userdata */
    scvol = pa_sink_get_volume(sink, FALSE);
    for (i = 0; i < scvol->channels; i++) {
        cvol.values[i] = scvol->values[i];
    }
    cvol.channels = scvol->channels;

    pa_cvolume_set_balance(&cvol, &sink->channel_map, u->balance);
    pa_sink_set_volume(sink, &cvol, TRUE, TRUE);

    /* Reset sink muteall from userdata */
//    pa_sink_set_mute(sink,u->muteall,TRUE);

    return PA_HOOK_OK;
}

static void defer_event_cb (pa_mainloop_api *m, pa_defer_event *e, void *userdata)
{
    struct userdata *u = userdata;

    pa_assert(m);
    pa_assert(e);
    pa_assert(u);

    m->defer_enable(u->defer.event, 0);

    /* Dispatch event */
    if ((u->defer.event_type == PA_HAL_EVENT_LOAD_DEVICE) || (u->defer.event_type == PA_HAL_EVENT_OPEN_DEVICE)) {
        struct pa_hal_device_event_data *event_data = (struct pa_hal_device_event_data *)u->defer.event_data;

        pa_log_info("dispatch %s event", __get_event_type_string(u->defer.event_type));

        __load_n_open_device(u, &event_data->device_info, &event_data->params[0], u->defer.event_type);

        pa_log_debug("completed %s event", __get_event_type_string(u->defer.event_type));
    } else if (u->defer.event_type == PA_HAL_EVENT_CLOSE_ALL_DEVICES) {
        pa_log_info("dispatch %s event", __get_event_type_string(PA_HAL_EVENT_CLOSE_ALL_DEVICES));

        __close_all_devices(u);

        pa_log_debug("completed %s event", __get_event_type_string(PA_HAL_EVENT_CLOSE_ALL_DEVICES));
    } else if ((u->defer.event_type == PA_HAL_EVENT_CLOSE_DEVICE) || (u->defer.event_type == PA_HAL_EVENT_UNLOAD_DEVICE)) {
        struct pa_hal_device_event_data *event_data = (struct pa_hal_device_event_data *)u->defer.event_data;

        pa_log_info("dispatch %s event", __get_event_type_string(u->defer.event_type));

        __close_n_unload_device(u, &event_data->device_info, u->defer.event_type);

        pa_log_debug("completed %s event", __get_event_type_string(u->defer.event_type));
    }
#ifdef FM_BT
        else if (u->defer.event_type == PA_HAL_EVENT_ROUTE_BT_FM) {
            pa_log_info("dispatch %s event", __get_event_type_string(u->defer.event_type));
            __route_fm_to_bt(u);
            pa_log_debug("completed %s event", __get_event_type_string(u->defer.event_type));
        }
        else if (u->defer.event_type == PA_HAL_EVENT_UNROUTE_BT_FM) {
            pa_log_info("dispatch %s event", __get_event_type_string(u->defer.event_type));
            __unroute_fm_to_bt(u);
            pa_log_debug("completed %s event", __get_event_type_string(u->defer.event_type));
        }
#endif
    if (u->defer.cond) {
        pa_mutex_lock(u->defer.mutex);
        pa_cond_signal(u->defer.cond, 0);
        pa_mutex_unlock(u->defer.mutex);
    }
}

static void defer_trigger_cb(pa_mainloop_api *m, pa_io_event* e, int fd, pa_io_event_flags_t events, void *userdata)
{
    struct userdata *u = userdata;
    char dummy[16];
    int ret = 0;

    if (u->defer.event == NULL) {
        u->defer.event = u->core->mainloop->defer_new(u->core->mainloop, defer_event_cb, u);
        m->defer_enable(u->defer.event, 0);
    }

    ret = read(fd, dummy, sizeof(dummy));
    if (ret < 0) {
        pa_log_error("error. wrong meesage");
    } else {
        m->defer_enable(u->defer.event, 1);
    }
}

static void subscribe_cb(pa_core *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata)
{
    struct userdata *u = userdata;
    pa_sink *def;
    pa_sink_input *si;
    uint32_t idx2;
    pa_sink *sink_to_move = NULL;
    pa_sink *sink_cur = NULL;
    pa_source *source_cur = NULL;
    pa_source_state_t source_state;
    int vconf_source_status = 0;
    uint32_t si_index;
    int audio_ret;

    pa_assert(u);

    pa_log_debug_verbose("t=[0x%x], idx=[%d]", t, idx);

    if (t == (PA_SUBSCRIPTION_EVENT_SERVER|PA_SUBSCRIPTION_EVENT_CHANGE)) {

    } else if (t == (PA_SUBSCRIPTION_EVENT_SINK|PA_SUBSCRIPTION_EVENT_CHANGE)) {

    } else if (t == (PA_SUBSCRIPTION_EVENT_SOURCE|PA_SUBSCRIPTION_EVENT_CHANGE)) {
        if ((source_cur = pa_idxset_get_by_index(c->sources, idx))) {
#ifdef USE_FM_REC_DEDICATED_DEVICE /* FIXME: if we have dedicated device for FM recording */
            if (pa_streq (source_cur->name, "alsa_input.0.analog-stereo")|| /* FM rec dev */
                pa_streq (source_cur->name, "alsa_input.2.analog-stereo")) /* normal rec dev */
#else
            if (pa_streq (source_cur->name, SOURCE_ALSA))
#endif
            {
                source_state = pa_source_get_state(source_cur);
                pa_log_debug_verbose("source[%s] changed to state[%d]", source_cur->name, source_state);
                if (source_state == PA_SOURCE_RUNNING) {
                    if (vconf_set_int(VCONFKEY_SOUND_CAPTURE_STATUS, 1)) {
                        pa_log_warn("vconf_set_int(%s) failed of errno = %d", VCONFKEY_SOUND_CAPTURE_STATUS, vconf_get_ext_errno());
                    }
                } else {
                    if (vconf_get_int(VCONFKEY_SOUND_CAPTURE_STATUS, &vconf_source_status)) {
                        pa_log_warn("vconf_get_int for %s failed", VCONFKEY_SOUND_CAPTURE_STATUS);
                    }
                    if (vconf_source_status) {
                        if (vconf_set_int(VCONFKEY_SOUND_CAPTURE_STATUS, 0)) {
                            pa_log_warn("vconf_set_int(%s) failed of errno = %d", VCONFKEY_SOUND_CAPTURE_STATUS, vconf_get_ext_errno());
                        }
                    }
                }
            }
        }
    }
}

static pa_hook_result_t sink_unlink_hook_callback(pa_core *c, pa_sink *sink, void* userdata) {
    struct userdata *u = userdata;
    uint32_t idx;
    pa_sink *sink_to_move;
    pa_sink_input *si;

    pa_assert(c);
    pa_assert(sink);
    pa_assert(u);

     /* There's no point in doing anything if the core is shut down anyway */
    if (c->state == PA_CORE_SHUTDOWN)
        return PA_HOOK_OK;

    /* if unloading sink is not bt, just return */
    if (!policy_is_bluez (sink)) {
        pa_log_debug("sink[%s][%d] unlinked but not a bluez....return\n", sink->name, sink->index);
        return PA_HOOK_OK;
    }

    pa_log_debug("========= sink [%s][%d], bt_off_idx was [%d], now set to [%d]", sink->name, sink->index,u->bt_off_idx, sink->index);
    u->bt_off_idx = sink->index;
    u->bluez_sink = NULL;

    sink_to_move = pa_namereg_get(c, "null", PA_NAMEREG_SINK);

    /* BT sink is unloading, move sink-input to proper sink */
    PA_IDXSET_FOREACH(si, c->sink_inputs, idx) {
        const char *policy = NULL;

        if (!si->sink)
            continue;

        /* Get role (if role is filter, skip it) */
        if (policy_is_filter(si->proplist))
            continue;

        /* Find who were using bt sink or bt related sink and move them to proper sink (alsa) */
        if (pa_streq (si->sink->name, SINK_COMBINED) ||
            policy_is_bluez (si->sink)) {

            pa_log_info("[%d] Moving sink-input[%d][%s] from [%s] to [%s]", idx, si->index, policy, si->sink->name, sink_to_move->name);
            pa_sink_input_move_to(si, sink_to_move, FALSE);
        }
    }

    pa_log_debug("unload sink in dependencies");

    /* Unload combine sink */
    if (u->module_combined) {
        pa_module_unload(u->module->core, u->module_combined, TRUE);
        u->module_combined = NULL;
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_unlink_post_hook_callback(pa_core *c, pa_sink *sink, void* userdata) {
    struct userdata *u = userdata;

    pa_assert(c);
    pa_assert(sink);
    pa_assert(u);

    pa_log_debug("========= sink [%s][%d]", sink->name, sink->index);

     /* There's no point in doing anything if the core is shut down anyway */
    if (c->state == PA_CORE_SHUTDOWN)
        return PA_HOOK_OK;

    /* if unloading sink is not bt, just return */
    if (!policy_is_bluez (sink)) {
        pa_log_debug("not a bluez....return\n");
        return PA_HOOK_OK;
    }

    u->bt_off_idx = -1;
    pa_log_debug ("bt_off_idx is cleared to [%d]", u->bt_off_idx);

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_input_move_start_cb(pa_core *core, pa_sink_input *i, struct userdata *u) {
    audio_return_t audio_ret = AUDIO_RET_OK;

    pa_core_assert_ref(core);
    pa_sink_input_assert_ref(i);

    /* There's no point in doing anything if the core is shut down anyway */
    if (core->state == PA_CORE_SHUTDOWN)
       return PA_HOOK_OK;

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_input_move_finish_cb(pa_core *core, pa_sink_input *i, struct userdata *u) {
    audio_return_t audio_ret = AUDIO_RET_OK;

    pa_core_assert_ref(core);
    pa_sink_input_assert_ref(i);

    /* There's no point in doing anything if the core is shut down anyway */
    if (core->state == PA_CORE_SHUTDOWN)
        return PA_HOOK_OK;

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_input_state_changed_hook_cb(pa_core *core, pa_sink_input *i, struct userdata *u)
{
    pa_sink* sink_to_move = NULL;
    pa_sink* sink_default = NULL;
    const char * policy = NULL;

    uint32_t volume_type = 0;
    const char *si_volume_type_str = NULL;
    const char *si_policy_str = NULL;
    pa_sink_input_state_t state;

    pa_assert(i);
    pa_assert(u);

    if((si_volume_type_str = pa_proplist_gets(i->proplist, PA_PROP_MEDIA_TIZEN_VOLUME_TYPE))) {
        pa_atou(si_volume_type_str, &volume_type);

        state = pa_sink_input_get_state(i);

        switch(state) {
            case PA_SINK_INPUT_CORKED:
                si_policy_str = pa_proplist_gets(i->proplist, PA_PROP_MEDIA_POLICY);

                /* special case. media volume should be set using fake sink-input by fmradio*/
                if(si_policy_str && pa_streq(si_policy_str, "fmradio"))
                    break;

                __set_primary_volume(u, (void*)i, volume_type, false);
                break;
            case PA_SINK_INPUT_DRAINED:
            case PA_SINK_INPUT_RUNNING:
                __set_primary_volume(u, (void*)i, volume_type, true);
                break;
            default:
                break;
        }
    }

    if(i->state == PA_SINK_INPUT_RUNNING) {
        policy = pa_proplist_gets (i->proplist, PA_PROP_MEDIA_POLICY);

        pa_log_info("---------------------------------------------");

        /** If sink input is an UHQA sink sink input then connect it to UHQA sink if not connected to UHQA sink*/
        if ( ( i->sample_spec.rate >= UHQA_BASE_SAMPLING_RATE) && (policy != NULL) &&
             ( pa_streq (policy, POLICY_HIGH_LATENCY) || pa_streq (policy, POLICY_AUTO) )) {
            char tmp_policy[100] = {0};

            pa_log_info ("------------------------------------");

            sprintf(tmp_policy, "%s-uhqa", policy);
            sink_to_move = policy_select_proper_sink (u, tmp_policy, i, TRUE);

            if (i->sink != sink_to_move) {
                 if (sink_to_move) {
                    pa_log_debug("Moving sink-input[%d] from [%s] to [%s]", i->index, i->sink->name, sink_to_move->name);
                    pa_sink_input_move_to(i, sink_to_move, FALSE);
                }
            }
        }

        /** Get the normal sink and move all sink input from normal sink to UHQA sink if normal sink and UHQA sink are different*/
        sink_default = policy_select_proper_sink (u, policy, i, TRUE);
        if ((sink_to_move != NULL) && (sink_default != NULL) && (sink_to_move != sink_default)) {
            pa_sink_input *si = NULL;
            uint32_t idx;

            /** Check any sink input connected to normal sink then move them to UHQA sink*/
            PA_IDXSET_FOREACH (si, sink_default->inputs, idx) {
                pa_log_info ("------------------------------------");
                /* Get role (if role is filter, skip it) */
                if (policy_is_filter (si->proplist)) {
                    continue;
                }
                pa_sink_input_move_to (si,  sink_to_move, FALSE);
            }
        }
    }
    return PA_HOOK_OK;
}

static pa_hook_result_t sink_state_changed_hook_cb(pa_core *c, pa_object *o, struct userdata *u) {
    return PA_HOOK_OK;
}

/* Select source for given condition */
static pa_source* policy_select_proper_source (struct userdata *u, const char* policy)
{
    pa_core *c;
    pa_source *source = NULL, *def, *source_null;

    pa_assert (u);
    c = u->core;
    pa_assert (c);
    if (policy == NULL) {
        pa_log_warn("input param is null");
        return NULL;
    }

    def = pa_namereg_get_default_source(c);
    if (def == NULL) {
        pa_log_warn("pa_namereg_get_default_source() returns null");
        return NULL;
    }

    source_null = (pa_source *)pa_namereg_get(u->core, "source.null", PA_NAMEREG_SOURCE);
    /* if default source is set as null source, we will use it */
    if (def == source_null)
        return def;

    /* FIXME: spreadtrum */
    if ((def == pa_namereg_get(c, SOURCE_VIRTUAL, PA_NAMEREG_SOURCE)) || (def == pa_namereg_get(c, SOURCE_VOIP, PA_NAMEREG_SOURCE)))
        return def;

    /* Select source  to */
    if (pa_streq(policy, POLICY_VOIP)) {
        /* NOTE: Check voip source first, if not available, use AEC source  */
        source = policy_get_source_by_name (c, SOURCE_VOIP);
        if (source == NULL) {
            pa_log_info("VOIP source is not available, try to use AEC source");
            source = policy_get_source_by_name (c, AEC_SOURCE);
            if (source == NULL) {
                pa_log_warn("AEC source is not available, set to default source");
                source = def;
            }
        }
    } else if (pa_streq(policy, POLICY_MIRRORING)) {
        source = policy_get_source_by_name (c, SOURCE_MIRRORING);
        if (source == NULL) {
            pa_log_info("MIRRORING source is not available, try to use ALSA MONITOR SOURCE");
            source = policy_get_source_by_name (c, ALSA_MONITOR_SOURCE);
            if (source == NULL) {
                pa_log_warn(" ALSA MONITOR SOURCE source is not available, set to default source");
                source = def;
            }
        }
    } else if (pa_streq(policy, POLICY_LOOPBACK)) {
        source = policy_get_source_by_name (c, ALSA_MONITOR_SOURCE);
        if (source == NULL) {
            pa_log_warn (" ALSA MONITOR SOURCE source is not available, set to default source");
            source = def;
        }
    } else {
        source = def;
    }

    pa_log_debug("selected source : [%s]\n", (source)? source->name : "null");
    return source;
}


/*  Called when new source-output is creating  */
static pa_hook_result_t source_output_new_hook_callback(pa_core *c, pa_source_output_new_data *new_data, struct userdata *u) {
    const char *policy = NULL;
    pa_assert(c);
    pa_assert(new_data);
    pa_assert(u);

    if (!new_data->proplist) {
        pa_log_debug("New stream lacks property data.");
        return PA_HOOK_OK;
    }

    if (new_data->source) {
        pa_log_debug("Not setting device for stream %s, because already set.", pa_strnull(pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_NAME)));
        return PA_HOOK_OK;
    }

    /* If no policy exists, skip */
    if (!(policy = pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_POLICY))) {
        pa_log_debug("Not setting device for stream [%s], because it lacks policy.",
                pa_strnull(pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_NAME)));
        return PA_HOOK_OK;
    }
    pa_log_debug("Policy for stream [%s] = [%s]",
            pa_strnull(pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_NAME)), policy);

    /* Set proper source to source-output */
    new_data->save_source= FALSE;
    new_data->source= policy_select_proper_source (u, policy);

    pa_log_debug("set source of source-input to [%s]", (new_data->source)? new_data->source->name : "null");

    return PA_HOOK_OK;
}

static pa_hook_result_t source_output_put_callback(pa_core *c, pa_source_output *o, struct userdata *u)
{
    pa_core_assert_ref(c);
    pa_source_output_assert_ref(o);
    pa_assert(u);

    __set_primary_volume(u, (void*)o, AUDIO_PRIMARY_VOLUME_TYPE_MAX/*source-output use PRIMARY_MAX*/, true);
    return PA_HOOK_OK;
}

static pa_hook_result_t source_output_unlink_post_hook_callback(pa_core *c, pa_source_output *o, struct userdata *u)
{
    __set_primary_volume(u, (void*)o, AUDIO_PRIMARY_VOLUME_TYPE_MAX/*source-output use PRIMARY_MAX*/, false);
    return PA_HOOK_OK;
}

#define DEBUG_PATH "/tmp/.pulse_debug"
#define DEBUG_MAX_LINE 1024
#define DEBUG_MAX_BUFFER 1024*1024
static int policy_show_stream_information(struct userdata *u, pa_strbuf* buffer)
{
    uint32_t idx;
    char dummy[16] = "";

    pa_sink *sink = NULL;
    pa_sink_input *si = NULL;
    pa_source *source = NULL;
    pa_source_output *so = NULL;

    const char* sink_source_state[3] = { "running", "idle", "suspend" };
    const char* si_state[5] = { "init", "drained", "running", "corked", "unlinked" };
    const char* so_state[5] = { "init", "running", "corked", "unlinked", "unlinked" };
    pa_usec_t latency;

    pa_sink *default_sink = pa_namereg_get_default_sink(u->core);
    pa_source *default_source = pa_namereg_get_default_source(u->core);

    char mainloop_dump[DEBUG_MAX_LINE];

    if (!buffer || !u)
        return -1;

    /* print sinks and sink-inputs */
    PA_IDXSET_FOREACH(sink, u->core->sinks, idx) {
        pa_strbuf_printf(buffer, "Sink #%d : name(%s), state(%s), rate(%d), channels(%d), format(%s), mute(%d)\n",
            sink->index, sink->name, sink_source_state[pa_sink_get_state(sink)], sink->sample_spec.rate, sink->sample_spec.channels,
            pa_sample_format_to_string(sink->sample_spec.format), sink->muted);
    }

    PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {
        pa_cvolume_snprint(dummy, sizeof(dummy), &si->thread_info.soft_volume);
        pa_strbuf_printf(buffer, "Sink-Input #%d : state(%s) owner(Sink #%d) rate(%d), channels(%d), format(%s), mute(%d), volume(%s), " \
            "resampler(%s), buffer-latency(%lluusec), sink-latency(%lluusec) vol&gain(%s,%s) pid(%s), name(%s)\n",
            si->index,
            pa_sink_input_get_state(si) > 0 && pa_sink_input_get_state(si) < sizeof(si_state) / sizeof(char*) ? si_state[pa_sink_input_get_state(si)] : "unknown",
            si->sink->index, si->sample_spec.rate, si->sample_spec.channels, pa_sample_format_to_string(si->sample_spec.format),
            si->muted, dummy, pa_resample_method_to_string(pa_sink_input_get_resample_method(si)),
            pa_sink_input_get_latency(si, &latency), latency,
            pa_strnull(pa_proplist_gets(si->proplist, PA_PROP_MEDIA_TIZEN_VOLUME_TYPE)),
            pa_strnull(pa_proplist_gets(si->proplist, PA_PROP_MEDIA_TIZEN_GAIN_TYPE)),
            pa_strnull(pa_proplist_gets(si->proplist, PA_PROP_APPLICATION_PROCESS_ID)),
            pa_strnull(pa_proplist_gets(si->proplist, PA_PROP_APPLICATION_PROCESS_BINARY)));
    }

    /* print sources and source-outputs */
    PA_IDXSET_FOREACH(source, u->core->sources, idx) {
        pa_strbuf_printf(buffer, "Source #%d : name(%s), state(%s), rate(%d), channels(%d), format(%s), mute(%d)\n",
            source->index, source->name, sink_source_state[pa_source_get_state(source)],
            source->sample_spec.rate, source->sample_spec.channels,
            pa_sample_format_to_string(source->sample_spec.format), source->muted);
    }
    PA_IDXSET_FOREACH(so, u->core->source_outputs, idx) {
        pa_cvolume_snprint(dummy, sizeof(dummy), &so->thread_info.soft_volume);
        pa_strbuf_printf(buffer, "Source-Output : #%d state(%s) owner(Source #%d) rate(%d), channels(%d), format(%s), mute(%d), volume(%s), " \
            "resampler(%s), buffer-latency(%lluusec), source-latency(%lluusec), pid(%s), name(%s)\n",
            so->index,
            pa_source_output_get_state(so) > 0 && pa_source_output_get_state(so) < sizeof(so_state) / sizeof(char*) ? so_state[pa_source_output_get_state(so)] : "unknown",
            so->source->index, so->sample_spec.rate, so->sample_spec.channels, pa_sample_format_to_string(so->sample_spec.format),
            so->muted, dummy, pa_resample_method_to_string(pa_source_output_get_resample_method(so)),
            pa_source_output_get_latency(so, &latency), latency,
            pa_strnull(pa_proplist_gets(so->proplist, PA_PROP_APPLICATION_PROCESS_ID)),
            pa_strnull(pa_proplist_gets(so->proplist, PA_PROP_APPLICATION_PROCESS_BINARY)));
    }
    pa_strbuf_printf(buffer, "default-sink(%s), default-source(%s)\n", pa_strnull(default_sink->name), pa_strnull(default_source->name));
    pa_strbuf_printf(buffer, "Bluez_sink(%p) \n\n",u->bluez_sink);

    pa_strbuf_printf(buffer, "session(%d), subsession(%d), subsession_opt(%d)\n", u->session, u->subsession, u->subsession_opt);
    pa_strbuf_printf(buffer, "active_device_in(%d), active_device_out(%d), active_route_flag(%d)\n", u->active_device_in, u->active_device_out, u->active_route_flag);
    pa_strbuf_printf(buffer, "call_type(%d), call_nrec(%d), call_extra_volume(%d)\n", u->call_type, u->call_nrec, u->call_extra_volume);
    pa_strbuf_printf(buffer, "bt_bandwidth(%d), bt_nrec(%d)\n\n", u->bt_bandwidth, u->bt_nrec);

    /* dump mainloop */
    pa_strbuf_printf(buffer, "pulseaudio mainloop dump\n");
    u->core->mainloop->dump_context(u->core->mainloop, mainloop_dump, DEBUG_MAX_LINE);
    pa_strbuf_printf(buffer, "%s\n", mainloop_dump);

    return 0;
}


static void debug_thread_func(void *userdata)
{
#ifndef OS_IS_WIN32
    struct userdata *u = (struct userdata *)userdata;

    int ret = 0;
    socklen_t client_len;
    int sockfd, client_sockfd;
    struct sockaddr_un addr, clientaddr;

    char buf[32] = "";
    char *logs = NULL;
    int retry = 10;
    pa_strbuf *buffer = pa_strbuf_new();

    pa_log_info("debug thread starting up");

    while (retry--) {
        if (unlink(DEBUG_PATH) < 0) {
            if (errno != ENOENT) {
                pa_log_error("unlink(%s) failed. please check permission or smack", DEBUG_PATH);
                usleep(100 * 1000);
                continue;
            }
        }
    }

    if ((sockfd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
        pa_log_error("socket creation failed. debug thread would be terminated.");
        return;
    }

    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", DEBUG_PATH);
    addr.sun_path[strlen(DEBUG_PATH)] = '\0';

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        pa_log_error("bind failed. debug thread would be terminated.");
        close(sockfd);
        return;
    }

    if (listen(sockfd, 5) == -1) {
        pa_log_error("listen failed. debug thread would be terminated.");
        close(sockfd);
        return;
    }

    for (;;) {
        client_sockfd = accept(sockfd, (struct sockaddr *)&clientaddr, &client_len);
        ret = read(client_sockfd, buf, sizeof(buf));
        if (ret < 0) {
            pa_log_info("wrong message");
            close(client_sockfd);
            continue;
        }

        buffer = pa_strbuf_new();
        if (buffer == NULL) {
            char *error_msg = "out of memory";
            pa_log_error("%s", error_msg);
            write(client_sockfd, error_msg, strlen(error_msg));
            close(client_sockfd);
            continue;
        }

        if (!policy_show_stream_information(u, buffer)) {
            logs = pa_strbuf_tostring_free(buffer);
            ret = write(client_sockfd, logs, PA_MIN(strlen(logs), DEBUG_MAX_BUFFER));
            if (ret < 0)
                pa_log_info("write failed");

            free(logs);
        } else {
            char *error_msg = "policy_show_stream_information failed.";
            write(client_sockfd, error_msg, strlen(error_msg));
            if(ret < 0)
                pa_log_info("write failed");
        }

        close(client_sockfd);
    }
#else
    return;
#endif
}

#define DEBUG_TIME_OUT_FREQUENCY    60*60*PA_USEC_PER_SEC
static void debug_timeout_cb(pa_mainloop_api*a, pa_time_event* e, const struct timeval *t, void *userdata)
{
    struct userdata *u = (struct userdata *)userdata;
    pa_usec_t now = 0ULL;
    char *debug_buffer = NULL;

    debug_buffer = (char*)malloc(DEBUG_MAX_BUFFER);
    if (debug_buffer == NULL) {
        pa_log_error("out of memory");
        pa_core_rttime_restart(u->core, e, pa_rtclock_now() + DEBUG_TIME_OUT_FREQUENCY);
        return;
    }

    memset(debug_buffer, 0, DEBUG_MAX_BUFFER);

    policy_show_stream_information(u, debug_buffer);
    pa_log_info("%s", debug_buffer);

    if (debug_buffer != NULL) {
        free(debug_buffer);
        debug_buffer = NULL;
    }

    pa_core_rttime_restart(u->core, e, pa_rtclock_now() + DEBUG_TIME_OUT_FREQUENCY);

    return;
}

int pa__init(pa_module *m)
{
    pa_modargs *ma = NULL;
    struct userdata *u;
    pa_bool_t on_hotplug = TRUE, on_rescue = TRUE, wideband = FALSE;
    uint32_t frag_size = 0, tsched_size = 0;
    char *thread_name = NULL;

    pa_assert(m);

    pa_log_info("We are using pulseaudio-module-tizen");

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    if (pa_modargs_get_value_boolean(ma, "on_hotplug", &on_hotplug) < 0 ||
        pa_modargs_get_value_boolean(ma, "on_rescue", &on_rescue) < 0) {
        pa_log("on_hotplug= and on_rescue= expect boolean arguments");
        goto fail;
    }

        if (pa_modargs_get_value_boolean(ma, "use_wideband_voice", &wideband) < 0 ||
            pa_modargs_get_value_u32(ma, "fragment_size", &frag_size) < 0 ||
            pa_modargs_get_value_u32(ma, "tsched_buffer_size", &tsched_size) < 0) {
            pa_log("Failed to parse module arguments buffer info");
            goto fail;
    }
    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->on_hotplug = on_hotplug;
    u->wideband = wideband;
    u->fragment_size = frag_size;
    u->tsched_buffer_size = tsched_size;

    /* A little bit later than module-stream-restore */
    u->sink_state_changed_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_STATE_CHANGED], PA_HOOK_NORMAL, (pa_hook_cb_t)sink_state_changed_hook_cb, u);

    u->sink_input_new_hook_slot =
            pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_NEW], PA_HOOK_EARLY+10, (pa_hook_cb_t) sink_input_new_hook_callback, u);
    u->sink_input_unlink_post_slot =
            pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_UNLINK_POST], PA_HOOK_EARLY+10, (pa_hook_cb_t) sink_input_unlink_post_hook_callback, u);
    u->sink_input_put_slot =
            pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_PUT], PA_HOOK_EARLY+10, (pa_hook_cb_t) sink_input_put_callback, u);
    u->sink_input_state_changed_slot =
             pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_STATE_CHANGED], PA_HOOK_EARLY+10, (pa_hook_cb_t) sink_input_state_changed_hook_cb, u);

    u->source_output_new_hook_slot =
            pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_NEW], PA_HOOK_EARLY+10, (pa_hook_cb_t) source_output_new_hook_callback, u);
    u->source_output_unlink_post_slot =
            pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_UNLINK_POST], PA_HOOK_EARLY+10, (pa_hook_cb_t) source_output_unlink_post_hook_callback, u);
    u->source_output_put_slot =
            pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_PUT], PA_HOOK_EARLY+10, (pa_hook_cb_t) source_output_put_callback, u);

    if (on_hotplug) {
        /* A little bit later than module-stream-restore */
        u->sink_put_hook_slot =
            pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_PUT], PA_HOOK_LATE+10, (pa_hook_cb_t) sink_put_hook_callback, u);
    }

    /* sink unlink comes before sink-input unlink */
    u->sink_unlink_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_UNLINK], PA_HOOK_EARLY, (pa_hook_cb_t) sink_unlink_hook_callback, u);
    u->sink_unlink_post_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_UNLINK_POST], PA_HOOK_EARLY, (pa_hook_cb_t) sink_unlink_post_hook_callback, u);

    u->sink_input_move_start_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_MOVE_START], PA_HOOK_LATE, (pa_hook_cb_t) sink_input_move_start_cb, u);
    u->sink_input_move_finish_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_MOVE_FINISH], PA_HOOK_LATE, (pa_hook_cb_t) sink_input_move_finish_cb, u);

    u->tid = pthread_self();

    if (pipe(u->fd_defer))
        pa_log_info("can't make pipe for defer callback.");
    else {
        pa_assert_se(u->defer_io = u->core->mainloop->io_new(u->core->mainloop, u->fd_defer[0], PA_IO_EVENT_INPUT, defer_trigger_cb, u));
        pa_log_info("defer_trigger_cb is registered");
    }

    u->defer.mutex = pa_mutex_new(TRUE, TRUE);
    u->defer.cond = pa_cond_new();
    u->subscription = pa_subscription_new(u->core, PA_SUBSCRIPTION_MASK_SERVER | PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE, subscribe_cb, u);

    pa_log_debug("subscription done");

    u->bt_off_idx = -1;    /* initial bt off sink index */
    u->bluez_sink = NULL;

    u->module_combined = NULL;

    u->call_type = u->call_nrec = u->call_extra_volume = u->link_direction = 0;
    memset(u->fade_rcount, 0, sizeof(u->fade_rcount));
    u->fade_streams = pa_idxset_new(NULL, NULL);
    u->half_fade_streams = pa_idxset_new(NULL, NULL);

    u->protocol = pa_native_protocol_get(m->core);
    pa_native_protocol_install_ext(u->protocol, m, extension_cb);

    /* Get mono key value for init */
    vconf_get_bool(MONO_KEY, &u->core->is_mono);
    if (vconf_set_int(VCONFKEY_SOUND_PRIMARY_VOLUME_TYPE, -1)) {
        pa_log_warn("vconf_set_int(%s) failed of errno = %d",VCONFKEY_SOUND_PRIMARY_VOLUME_TYPE, vconf_get_ext_errno());
    }

    /* Load library & init audio mgr */
    u->audio_mgr.dl_handle = dlopen(LIB_TIZEN_AUDIO, RTLD_NOW);
    if (u->audio_mgr.dl_handle) {
        u->audio_mgr.intf.init = dlsym(u->audio_mgr.dl_handle, "audio_init");
        u->audio_mgr.intf.deinit = dlsym(u->audio_mgr.dl_handle, "audio_deinit");
        u->audio_mgr.intf.reset = dlsym(u->audio_mgr.dl_handle, "audio_reset");
        u->audio_mgr.intf.set_callback = dlsym(u->audio_mgr.dl_handle, "audio_set_callback");
        u->audio_mgr.intf.get_volume_level_max = dlsym(u->audio_mgr.dl_handle, "audio_get_volume_level_max");
        u->audio_mgr.intf.get_volume_level = dlsym(u->audio_mgr.dl_handle, "audio_get_volume_level");
        u->audio_mgr.intf.get_volume_value = dlsym(u->audio_mgr.dl_handle, "audio_get_volume_value");
        u->audio_mgr.intf.set_volume_level = dlsym(u->audio_mgr.dl_handle, "audio_set_volume_level");
        u->audio_mgr.intf.set_volume_value = dlsym(u->audio_mgr.dl_handle, "audio_set_volume_value");
        u->audio_mgr.intf.get_gain_value = dlsym(u->audio_mgr.dl_handle, "audio_get_gain_value");
        u->audio_mgr.intf.get_mute = dlsym(u->audio_mgr.dl_handle, "audio_get_mute");
        u->audio_mgr.intf.set_mute = dlsym(u->audio_mgr.dl_handle, "audio_set_mute");
        u->audio_mgr.intf.alsa_pcm_open = dlsym(u->audio_mgr.dl_handle, "audio_alsa_pcm_open");
        u->audio_mgr.intf.alsa_pcm_close = dlsym(u->audio_mgr.dl_handle, "audio_alsa_pcm_close");
        u->audio_mgr.intf.set_session = dlsym(u->audio_mgr.dl_handle, "audio_set_session");
        u->audio_mgr.intf.set_route = dlsym(u->audio_mgr.dl_handle, "audio_set_route");
        u->audio_mgr.intf.get_route = dlsym(u->audio_mgr.dl_handle, "audio_get_route");
        u->audio_mgr.intf.set_mixer_value = dlsym(u->audio_mgr.dl_handle, "audio_set_mixer_value");
        u->audio_mgr.intf.set_mixer_value_string = dlsym(u->audio_mgr.dl_handle, "audio_set_mixer_value_string");
        u->audio_mgr.intf.set_route_info = dlsym(u->audio_mgr.dl_handle, "audio_set_route_info");
        u->audio_mgr.intf.get_route_info = dlsym(u->audio_mgr.dl_handle, "audio_get_route_info");

        u->audio_mgr.intf.set_vsp = dlsym(u->audio_mgr.dl_handle, "audio_set_vsp");
        u->audio_mgr.intf.set_soundalive_device = dlsym(u->audio_mgr.dl_handle, "audio_set_soundalive_device");
        u->audio_mgr.intf.set_soundalive_square = dlsym(u->audio_mgr.dl_handle, "audio_set_soundalive_square");
        u->audio_mgr.intf.set_soundalive_filter_action = dlsym(u->audio_mgr.dl_handle, "audio_set_soundalive_filter_action");
        u->audio_mgr.intf.set_soundalive_preset_mode = dlsym(u->audio_mgr.dl_handle, "audio_set_soundalive_preset_mode");
        u->audio_mgr.intf.set_soundalive_equalizer = dlsym(u->audio_mgr.dl_handle, "audio_set_soundalive_equalizer");
        u->audio_mgr.intf.set_soundalive_extend = dlsym(u->audio_mgr.dl_handle, "audio_set_soundalive_extend");
        u->audio_mgr.intf.set_dha_param = dlsym(u->audio_mgr.dl_handle, "audio_set_dha_param");
        u->audio_mgr.intf.get_board_config = dlsym(u->audio_mgr.dl_handle, "audio_get_board_config");
        u->audio_mgr.intf.update_audio_effect = dlsym(u->audio_mgr.dl_handle, "audio_update_effect");
        u->audio_mgr.intf.device_state_change = dlsym(u->audio_mgr.dl_handle, "audio_device_state_change");

        if (u->audio_mgr.intf.init) {
            if (u->audio_mgr.intf.init(&u->audio_mgr.data, (void *)u) != AUDIO_RET_OK) {
                pa_log_error("audio_mgr init failed");
            } else {
                pa_log_info("audio_mgr init success");
                pa_shared_set(u->core, "tizen-audio-data", u->audio_mgr.data);
                pa_shared_set(u->core, "tizen-audio-interface", &u->audio_mgr.intf);
            }
        }

        if (u->audio_mgr.intf.set_callback) {
            audio_cb_interface_t cb_interface;

            cb_interface.load_device = __load_device_callback;
            cb_interface.open_device = __open_device_callback;
            cb_interface.close_all_devices = __close_all_devices_callback;
            cb_interface.close_device = __close_device_callback;
            cb_interface.unload_device = __unload_device_callback;
#ifdef FM_BT
            cb_interface.route_fm_to_bt = __route_fm_to_bt_callback;
            cb_interface.unroute_fm_to_bt = __unroute_fm_to_bt_callback;
#endif

            u->audio_mgr.intf.set_callback(u->audio_mgr.data, &cb_interface);
        }
    } else {
        pa_log_error("open audio_mgr failed :%s", dlerror());
    }

    if (u->audio_mgr.intf.get_board_config) {
        u->audio_mgr.intf.get_board_config(u->audio_mgr.data, &u->board_config);
    }

    __load_dump_config(u);

    /* make debug thread */
    if (!pa_thread_new("policy-debug", debug_thread_func, u)) {
        pa_log_info("Failed to create policy-debug thread.");
        goto fail;
    }
    u->debug_time_event = pa_core_rttime_new(u->core, pa_rtclock_now() + DEBUG_TIME_OUT_FREQUENCY, debug_timeout_cb, u);

    pa_log_info("policy module is loaded. audio_mgr.data(%x)\n", u->audio_mgr.data);

    if (ma)
        pa_modargs_free(ma);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    pa__done(m);

    return -1;
}

void pa__done(pa_module *m)
{
    struct userdata* u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->sink_input_new_hook_slot)
        pa_hook_slot_free(u->sink_input_new_hook_slot);
    if (u->sink_put_hook_slot)
        pa_hook_slot_free(u->sink_put_hook_slot);
    if(u->sink_input_state_changed_slot)
         pa_hook_slot_free(u->sink_input_state_changed_slot);
    if (u->subscription)
        pa_subscription_free(u->subscription);
    if (u->protocol) {
        pa_native_protocol_remove_ext(u->protocol, m);
        pa_native_protocol_unref(u->protocol);
    }
    if (u->source_output_new_hook_slot)
        pa_hook_slot_free(u->source_output_new_hook_slot);

    if (u->fade_streams)
        pa_idxset_free(u->fade_streams, NULL);

    if (u->half_fade_streams)
        pa_idxset_free(u->half_fade_streams, NULL);

    if (u->defer.mutex)
        pa_mutex_free(u->defer.mutex);
    if (u->defer.cond)
        pa_cond_free(u->defer.cond);
    if (u->defer.event)
        u->core->mainloop->defer_free(u->defer.event);

    /* Deinit audio mgr & unload library */
    if (u->audio_mgr.intf.deinit) {
        if (u->audio_mgr.intf.deinit(&u->audio_mgr.data) != AUDIO_RET_OK) {
            pa_log_error("audio_mgr deinit failed");
        }
    }
    if (u->audio_mgr.dl_handle) {
        dlclose(u->audio_mgr.dl_handle);
        pa_shared_remove(u->core, "tizen-audio-data");
        pa_shared_remove(u->core, "tizen-audio-interface");
    }

    pa_xfree(u);

    pa_log_info("policy module is unloaded\n");
}
