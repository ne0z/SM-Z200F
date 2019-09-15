/*
 * Remix Stream Player: TIZEN device output
 *
 * Govindaraju SM <govi.sm@samsung.com>, August 2011
 * Prince Kumar Dubey <prince.dubey@samsung.com>, August 2011
 */

#include "config.h"
#include <mm_sound.h>
#include <remix/remix.h>
#include <Eina.h>
#include <Ecore.h>
#ifdef HAVE_LIBSNDFILE
#include <sndfile.h>
#endif
#include <vconf.h>

int _edje_multisense_default_log_dom = 0;

#ifdef ERR
# undef ERR
#endif
#define ERR(...) EINA_LOG_DOM_ERR(_edje_multisense_default_log_dom, __VA_ARGS__)
#ifdef WRN
# undef WRN
#endif
#define WRN(...) EINA_LOG_DOM_WARN(_edje_multisense_default_log_dom, __VA_ARGS__)

#define STREAM_PLAYER_BUFFERLEN 2048
#define DEFAULT_FORMAT MMSOUND_PCM_S16_LE

typedef struct _RemixPlayerData RemixPlayerData;
typedef short PLAYER_PCM;

struct _RemixPlayerData {
   RemixPCM databuffer[STREAM_PLAYER_BUFFERLEN];
   PLAYER_PCM *playbuffer;
   MMSoundPcmHandle_t handle;
   MMSoundPcmChannel_t channel;
   RemixPCM max_value;
   RemixEnv *env;
   RemixBase *base;
   int snd_on;
   int tch_snd_on;
   int all_sound_off;
   Eina_Bool vrecord_on;
   int buffer_size;
   int old_buffer_size;
   int stereo;
   int frequency;
};

/* Optimisation dependencies: none */
static RemixBase *remix_player_optimise (RemixEnv *env, RemixBase *base);

static int
remix_player_open_device (RemixEnv *env, RemixBase *base)
{
   RemixPlayerData *player_data = remix_base_get_instance_data(env, base);

   if (!player_data) return -1;

   if (player_data->handle) return 0;

   player_data->old_buffer_size = player_data->buffer_size;
   player_data->buffer_size = mm_sound_pcm_play_open_no_session(&player_data->handle,
                                                     player_data->frequency,
                                                     player_data->channel,
                                                     DEFAULT_FORMAT,
                                                     VOLUME_TYPE_SYSTEM | VOLUME_GAIN_TOUCH);
   if (mm_sound_pcm_play_start(player_data->handle) < 0)
     {
        remix_set_error (env, REMIX_ERROR_SYSTEM);
        return -1;
     }

   if (player_data->buffer_size < 0)
     {
        remix_set_error (env, REMIX_ERROR_SYSTEM);
        return -1;
     }

   if (player_data->old_buffer_size < player_data->buffer_size)
     {
        if (player_data->playbuffer) free(player_data->playbuffer);
        player_data->playbuffer = calloc(sizeof(PLAYER_PCM), player_data->buffer_size);
        if (!player_data->playbuffer)
          {
             remix_set_error (env, REMIX_ERROR_SYSTEM);
             return -1;
          }
   }

   return 0;
}

static void
_vconf_noti_key_changed_cb(keynode_t *node, void *data)
{
   RemixPlayerData *player = (RemixPlayerData *)data;
   char *keyname = vconf_keynode_get_name(node);
   int sound_status = 0;

   if (strcmp(keyname, VCONFKEY_SETAPPL_SOUND_STATUS_BOOL) == 0)
     player->snd_on = vconf_keynode_get_bool(node);

   if (strcmp(keyname, VCONFKEY_SETAPPL_TOUCH_SOUNDS_BOOL) == 0)
     player->tch_snd_on = vconf_keynode_get_bool(node);

   if (strcmp(keyname, VCONFKEY_RECORDER_STATE) == 0)
     {
        vconf_get_int(VCONFKEY_RECORDER_STATE, &sound_status);
        player->vrecord_on = sound_status & VCONFKEY_RECORDER_STATE_RECORDING;
     }

   if(strcmp(keyname, VCONFKEY_SETAPPL_ACCESSIBILITY_TURN_OFF_ALL_SOUNDS) == 0)
     {
        player->all_sound_off = vconf_keynode_get_bool(node);
     }

}

static RemixBase *
remix_player_init (RemixEnv *env, RemixBase *base, CDSet *parameters)
   {
   RemixCount nr_channels;
   CDSet *channels;
   RemixPlayerData *player_data = calloc(1, sizeof (RemixPlayerData));
   int sound_status = 0;

   if (!player_data)
     {
        remix_set_error(env, REMIX_ERROR_SYSTEM);
        return RemixNone;
     }

   remix_base_set_instance_data(env, base, player_data);
   channels = remix_get_channels (env);

    nr_channels = cd_set_size (env, channels);
    if (nr_channels == 1)
      {
         player_data->stereo = 0;
         player_data->channel = MMSOUND_PCM_MONO;
      }
    else if (nr_channels == 2)
      {
         player_data->stereo = 1;
         player_data->channel = MMSOUND_PCM_STEREO;
      }

   player_data->frequency = remix_get_samplerate(env);
   player_data->buffer_size = 0;
   player_data->max_value = (RemixPCM) SHRT_MAX;

   base = remix_player_optimise (env, base);
   player_data->env = env;
   player_data->base = base;

   if (vconf_get_bool(VCONFKEY_SETAPPL_SOUND_STATUS_BOOL,
                      &player_data->snd_on) < 0)
     WRN("\nFail to get VCONFKEY_SETAPPL_SOUND_STATUS_BOOL boolean value");

   if (vconf_get_bool(VCONFKEY_SETAPPL_TOUCH_SOUNDS_BOOL,
                      &player_data->tch_snd_on) < 0)
     WRN("\nFail to get VCONFKEY_SETAPPL_TOUCH_SOUNDS_BOOL boolean value");

   if (vconf_notify_key_changed(VCONFKEY_SETAPPL_SOUND_STATUS_BOOL,
                                _vconf_noti_key_changed_cb, player_data) < 0)
     WRN("\nFail to register VCONFKEY_SETAPPL_SOUND_STATUS_BOOL key callback");

   if (vconf_notify_key_changed(VCONFKEY_SETAPPL_TOUCH_SOUNDS_BOOL,
                                _vconf_noti_key_changed_cb, player_data) < 0)
     WRN("\nFail to register VCONFKEY_SETAPPL_SOUND_STATUS_BOOL key callback");

   if (vconf_notify_key_changed(VCONFKEY_SYSMAN_EARJACK,
                                _vconf_noti_key_changed_cb, player_data) < 0)
     WRN("\nFail to register VCONFKEY_SYSMAN_EARJACK key callback");

   if (vconf_notify_key_changed(VCONFKEY_RECORDER_STATE,
                                   _vconf_noti_key_changed_cb, player_data) < 0)
     WRN("\nFail to get VCONFKEY_RECORDER_STATE int value");

   if (vconf_get_int(VCONFKEY_RECORDER_STATE, &sound_status) < 0)
     WRN("\nFail to get VCONFKEY_RECORDER_STATE int value");
   player_data->vrecord_on = sound_status & VCONFKEY_RECORDER_STATE_RECORDING;

   if (vconf_notify_key_changed(VCONFKEY_SETAPPL_ACCESSIBILITY_TURN_OFF_ALL_SOUNDS,
                                   _vconf_noti_key_changed_cb, player_data) < 0)
     WRN("\nFail to register VCONFKEY_SETAPPL_ACCESSIBILITY_TURN_OFF_ALL_SOUNDS key callback");
   return base;
}


static RemixBase *
remix_player_clone (RemixEnv *env, RemixBase *base)
{
   RemixBase *new_player = remix_base_new (env);

   remix_player_init( env, new_player,  NULL);
   return new_player;
}

static int
remix_player_destroy (RemixEnv *env, RemixBase *base)
{
   RemixPlayerData *player_data = remix_base_get_instance_data(env, base);

   if (vconf_ignore_key_changed(VCONFKEY_SETAPPL_SOUND_STATUS_BOOL,
                                _vconf_noti_key_changed_cb) < 0)
     WRN("\nFail to unregister VCONFKEY_SETAPPL_SOUND_STATUS_BOOL key callback");
   if (vconf_ignore_key_changed(VCONFKEY_SETAPPL_TOUCH_SOUNDS_BOOL,
                                _vconf_noti_key_changed_cb) < 0)
     WRN("\nFail to unregister VCONFKEY_SETAPPL_TOUCH_SOUNDS_BOOL key callback");
   if (vconf_ignore_key_changed(VCONFKEY_SYSMAN_EARJACK,
                                _vconf_noti_key_changed_cb) < 0)
     WRN("\nFail to unregister VCONFKEY_SYSMAN_EARJACK key callback");
   if (vconf_ignore_key_changed(VCONFKEY_RECORDER_STATE,
                                _vconf_noti_key_changed_cb) < 0)
     WRN("\nFail to unregister VCONFKEY_RECORDER_STATE key callback");
   if (vconf_ignore_key_changed(VCONFKEY_SETAPPL_ACCESSIBILITY_TURN_OFF_ALL_SOUNDS,
                                _vconf_noti_key_changed_cb) < 0)
     WRN("\nFail to unregister VCONFKEY_SETAPPL_ACCESSIBILITY_TURN_OFF_ALL_SOUNDS key callback");

   if (player_data->handle)
     {
        if (mm_sound_pcm_play_stop(player_data->handle) < 0)
          remix_set_error (env, REMIX_ERROR_SYSTEM);
        else
          mm_sound_pcm_play_close(player_data->handle);
        player_data->handle = NULL;
     }

   if (player_data->playbuffer) free(player_data->playbuffer);
   free (player_data);
   // TODO: base must be freed, otherwise memory leak.
   // Need to check why this call create problem in player plugin load.
//   if (base) remix_free(base);
   return 0;
}

static int
remix_player_ready (RemixEnv *env, RemixBase *base)
{
   RemixPlayerData *player_data = remix_base_get_instance_data(env, base);
   RemixCount nr_channels;
   CDSet *channels;
   int samplerate;

   channels = remix_get_channels (env);
   samplerate = (int) remix_get_samplerate (env);

   nr_channels = cd_set_size (env, channels);

   return (samplerate == player_data->frequency &&
          ((nr_channels == 1 && player_data->stereo == 0) ||
           (nr_channels > 1 && player_data->stereo == 1)));
}

static RemixBase *
remix_player_prepare (RemixEnv *env, RemixBase *base)
{
   return base;
}

static RemixCount
remix_player_playbuffer (RemixEnv *env, RemixPlayerData *player, RemixPCM *data,
          RemixCount count)
{
   RemixCount i;
   RemixPCM value;
   int ret = count;
   size_t length;

   length = count * sizeof(RemixCount);

   for (i = 0; i < count; i++)
     {
        value = *data++ * (player->max_value);
        *(player->playbuffer + i) = (PLAYER_PCM) value;
     }

   if (player->handle)
     ret = mm_sound_pcm_play_write(player->handle,
                                     player->playbuffer,
                                     length);
   if (ret < 0)
     ERR("Write Fail\n");

   return length;
}

/* An RemixChunkFunc for making noise */
static RemixCount
remix_player_chunk (RemixEnv *env, RemixChunk *chunk, RemixCount offset,
          RemixCount count, int channelname, void *data)
{
   RemixPlayerData *player = (RemixPlayerData *)data;
   RemixCount remaining = count, written = 0, n, playcount;
   RemixPCM *d;

   while (remaining > 0)
     {
        playcount = MIN (remaining, player->buffer_size);

        d = &chunk->data[offset];
        n = remix_player_playbuffer (env, player, d, playcount);

        if (n == -1)
          return -1;
        else
          n /= sizeof (PLAYER_PCM);

        offset += n;
        written += n;
        remaining -= n;
     }

   return written;
}

static RemixCount
remix_player_process (RemixEnv *env, RemixBase *base, RemixCount count,
            RemixStream *input, RemixStream *output)
{
   RemixCount remaining = count, processed = 0, n, nn, nr_channels;
   RemixPlayerData *player_data = remix_base_get_instance_data(env, base);

   if (!player_data) return 0;
   if ((!player_data->snd_on) || (!player_data->tch_snd_on)
       || (player_data->vrecord_on) || (player_data->all_sound_off)) return count;
   if (!player_data->handle) remix_player_open_device (env, base);

   nr_channels = remix_stream_nr_channels (env, input);

   if (nr_channels == 1 && player_data->stereo == 0)
     { /* MONO */
        return remix_stream_chunkfuncify (env, input, count,
                  remix_player_chunk, player_data);
     }
   else if (nr_channels == 2 && player_data->stereo == 1)
     { /* STEREO */
        while (remaining > 0)
          {
             n = MIN (remaining, (player_data->buffer_size / 2) );
             n = remix_stream_interleave_2 (env, input,
                 REMIX_CHANNEL_LEFT, REMIX_CHANNEL_RIGHT,
                 player_data->databuffer, n);
             nn = 2 * n;
             nn = remix_player_playbuffer (env, player_data,
                                           player_data->databuffer, nn);

             processed += n;
             remaining -= n;
          }
        return processed;
     }
   else
     {
        ERR("[remix_player_process] unsupported stream/output channel\n");
        ERR ("combination %ld / %d\n", nr_channels, player_data->stereo ? 2 : 1);
        return -1;
     }
}

static RemixCount
remix_player_length (RemixEnv *env, RemixBase *base)
{
   return REMIX_COUNT_INFINITE;
}

static RemixCount
remix_player_seek (RemixEnv *env, RemixBase *base, RemixCount count)
{
   return count;
}

static Eina_Bool
_remix_mm_handle_close(void *data)
{
   RemixPlayerData *player_data = data;

   if (!player_data) return EINA_FALSE;
   if (!player_data->handle) return EINA_FALSE;

   if (mm_sound_pcm_play_stop(player_data->handle) < 0)
     {
        player_data->handle = NULL;
        return EINA_FALSE;
     }
   else
     mm_sound_pcm_play_close(player_data->handle);
   player_data->handle = NULL;


   return EINA_TRUE;
}

static int
remix_player_flush (RemixEnv *env, RemixBase *base)
{
   return 0;
}

static int
remix_player_reset (RemixEnv *env, RemixBase *base)
{
   RemixPlayerData *player_data = remix_base_get_instance_data(env, base);

   if (!player_data) return -1;

   if (_remix_mm_handle_close(player_data)) return 0;
   return -1;
}

static struct _RemixMethods _remix_player_methods = {
   remix_player_clone,
   remix_player_destroy,
   remix_player_ready,
   remix_player_prepare,
   remix_player_process,
   remix_player_length,
   remix_player_seek,
   remix_player_flush,
   remix_player_reset,
};

static RemixBase *
remix_player_optimise (RemixEnv *env, RemixBase *base)
{
   remix_base_set_methods (env, base, &_remix_player_methods);
   return base;
}

static struct _RemixMetaText tizen_player_metatext = {
   "tizen_snd_player",
   "TIZEN Sound Player",
   "Output the stream into TIZEN System",
   "Copyright (C) 2011, Samsung Electronics Co., Ltd.",
   "http://www.samsung.com",
   REMIX_ONE_AUTHOR ("govi.sm@samsung.com", "prince.dubey@samsung.com"),
};

static struct _RemixPlugin tizen_player_plugin = {
   &tizen_player_metatext,
   REMIX_FLAGS_NONE,
   CD_EMPTY_SET, /* init scheme */
   remix_player_init,
   CD_EMPTY_SET, /* process scheme */
   NULL, /* suggests */
   NULL, /* plugin data */
   NULL  /* destroy */
};

EAPI CDList *
remix_load (RemixEnv *env)
{
   CDList *plugins = cd_list_new (env);
   plugins = cd_list_prepend (env, plugins,
                             CD_POINTER(&tizen_player_plugin));
   return plugins;
}
