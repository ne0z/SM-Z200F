#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>

#include <Eo.h>

#include "Ecore.h"
#include "ecore_private.h"

#define MY_CLASS ECORE_JOB_CLASS

#define MY_CLASS_NAME "Ecore_Job"

static Eina_Bool _ecore_job_event_handler(void *data,
                                          int   type,
                                          void *ev);
static void _ecore_job_event_free(void *data,
                                  void *ev);

static int ecore_event_job_type = 0;
static Ecore_Event_Handler *_ecore_job_handler = NULL;

typedef struct _Ecore_Job_Data Ecore_Job_Data;

struct _Ecore_Job_Data
{
   Ecore_Event *event;
   Ecore_Cb     func;
   void        *data;
};

void
_ecore_job_init(void)
{
   ecore_event_job_type = ecore_event_type_new();
   _ecore_job_handler = ecore_event_handler_add(ecore_event_job_type, _ecore_job_event_handler, NULL);
}

void
_ecore_job_shutdown(void)
{
   _ecore_event_handler_del(_ecore_job_handler);
   _ecore_job_handler = NULL;
}

EAPI Ecore_Job *
ecore_job_add(Ecore_Cb    func,
              const void *data)
{
   Ecore_Job *job = eo_add(MY_CLASS, _ecore_parent, ecore_job_constructor(func, data));
   return job;
}

EOLIAN static void
_ecore_job_constructor(Eo *obj, Ecore_Job_Data *job, Ecore_Cb func, const void *data)
{
   if (EINA_UNLIKELY(!eina_main_loop_is()))
     {
// TIZEN_ONLY(20150810): Add multi thread error message
        ERR("You are calling %s from outside of the main loop threads. Program cannot run nomally", __FUNCTION__);
//
        EINA_MAIN_LOOP_CHECK_RETURN;
     }
   eo_manual_free_set(obj, EINA_TRUE);

   if (!func)
     {
        ERR("callback function must be set up for an object of class: '%s'", MY_CLASS_NAME);
        return;
     }

   job->event = ecore_event_add(ecore_event_job_type, job, _ecore_job_event_free, obj);
   if (!job->event)
     {
        ERR("no event was assigned to object '%p' of class '%s'", obj, MY_CLASS_NAME);
        return;
     }
   job->func = func;
   job->data = (void *)data;
}

EAPI void *
ecore_job_del(Ecore_Job *obj)
{
   void *data;

   if (!obj) return NULL;
   EINA_MAIN_LOOP_CHECK_RETURN_VAL(NULL);
   Ecore_Job_Data *job = eo_data_scope_get(obj, MY_CLASS);
   data = job->data;
   ecore_event_del(job->event);
   eo_do(obj, eo_parent_set(NULL));
   return data;
}

EOLIAN static void
_ecore_job_eo_base_destructor(Eo *obj, Ecore_Job_Data *_pd EINA_UNUSED)
{
   /*FIXME: check if ecore_event_del should be called from here*/
   eo_do_super(obj, MY_CLASS, eo_destructor());
}

EOLIAN static Eo *
_ecore_job_eo_base_finalize(Eo *obj, Ecore_Job_Data *pd)
{
   if (!pd->func)
     {
        return NULL;
     }

   return eo_do_super_ret(obj, MY_CLASS, obj, eo_finalize());
}

static Eina_Bool
_ecore_job_event_handler(void *data EINA_UNUSED,
                         int   type EINA_UNUSED,
                         void *ev)
{
   Ecore_Job_Data *job;

   job = ev;
   job->func(job->data);
   return ECORE_CALLBACK_DONE;
}

static void
_ecore_job_event_free(void *data,
                      void *job EINA_UNUSED)
{
   eo_do(data, eo_parent_set(NULL));

   Ecore_Job *obj = data;

   if (eo_destructed_is(obj))
      eo_manual_free(obj);
   else
      eo_manual_free_set(obj, EINA_FALSE);
}

#include "ecore_job.eo.c"
