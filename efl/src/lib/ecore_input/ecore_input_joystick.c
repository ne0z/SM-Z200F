#include <libudev.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

#include "Ecore.h"
#include "Ecore_Input.h"
#include "ecore_input_private.h"

static const char joystickPrefix[] = "/dev/input/event";

static Eina_List *joystick_list;

struct _Joystick_Info
{
   Ecore_Fd_Handler *fd_handler;
   char *system_path;
   int index;
};
typedef struct _Joystick_Info Joystick_Info;

static int _ecore_input_joystick_init_count = 0;
static struct udev_monitor *monitor = NULL;
static Ecore_Fd_Handler *monitor_handler = NULL;

static void
_joystick_connected_event_add(int index, Eina_Bool connected)
{
   Ecore_Event_Joystick *e;
   if (!(e = malloc(sizeof(Ecore_Event_Joystick)))) return;

   e->index = index;
   if (connected)
     e->type = ECORE_EVENT_JOYSTICK_EVENT_TYPE_CONNECTED;
   else
     e->type = ECORE_EVENT_JOYSTICK_EVENT_TYPE_DISCONNECTED;

   INF("index: %d, connected: %d", index, connected);
   ecore_event_add(ECORE_EVENT_JOYSTICK, e, NULL, NULL);
}

static void
_joystick_event_add(struct input_event *event, int index)
{
   Ecore_Event_Joystick *e;

   if ((event->type != EV_KEY) && (event->type != EV_ABS)) return;
   if (!(e = malloc(sizeof(Ecore_Event_Joystick)))) return;

   if (event->type == EV_KEY)
     {
        e->type = ECORE_EVENT_JOYSTICK_EVENT_TYPE_BUTTON;
        e->button.value = event->value;
     }
   else
     {
        e->type = ECORE_EVENT_JOYSTICK_EVENT_TYPE_AXIS;
        e->axis.value = event->value;
     }

   e->index = index;
   e->timestamp = ((event->time.tv_sec * 1000) + (event->time.tv_usec / 1000));

   switch (event->code)
     {
      case BTN_A:
        e->button.index = ECORE_EVENT_JOYSTICK_BUTTON_FACE_0;
        break;

      case BTN_B:
        e->button.index = ECORE_EVENT_JOYSTICK_BUTTON_FACE_1;
        break;

      case BTN_C:
        /* TODO: */
        break;

      case BTN_X:
        e->button.index = ECORE_EVENT_JOYSTICK_BUTTON_FACE_2;
        break;

      case BTN_Y:
        e->button.index = ECORE_EVENT_JOYSTICK_BUTTON_FACE_3;
        break;

      case BTN_Z:
        /* TODO: */
        break;

      case BTN_TL:
        e->button.index = ECORE_EVENT_JOYSTICK_BUTTON_LEFT_SHOULDER;
        break;

      case BTN_TR:
        e->button.index = ECORE_EVENT_JOYSTICK_BUTTON_RIGHT_SHOULDER;
        break;

      case BTN_SELECT:
        e->button.index = ECORE_EVENT_JOYSTICK_BUTTON_SELECT;
        break;

      case BTN_START:
        e->button.index = ECORE_EVENT_JOYSTICK_BUTTON_START;
        break;

      case BTN_THUMBL:
        e->button.index = ECORE_EVENT_JOYSTICK_BUTTON_LEFT_ANALOG_STICK;
        break;

      case BTN_THUMBR:
        e->button.index = ECORE_EVENT_JOYSTICK_BUTTON_RIGHT_ANALOG_STICK;
        break;

      case 0x13f:
        e->button.index = ECORE_EVENT_JOYSTICK_BUTTON_PLAY;
        break;

      case ABS_X:
        e->axis.index = ECORE_EVENT_JOYSTICK_AXIS_LEFT_ANALOG_HOR;
        e->axis.value = event->value / 32767.0f;
        break;

      case ABS_Y:
        e->axis.index = ECORE_EVENT_JOYSTICK_AXIS_LEFT_ANALOG_VER;
        e->axis.value = event->value / 32767.0f;
        break;

      case ABS_Z:
        e->axis.index = ECORE_EVENT_JOYSTICK_AXIS_LEFT_SHOULDER;
        break;

      case ABS_RX:
        e->axis.index = ECORE_EVENT_JOYSTICK_AXIS_RIGHT_ANALOG_HOR;
        e->axis.value = event->value / 32767.0f;
        break;

      case ABS_RY:
        e->axis.index = ECORE_EVENT_JOYSTICK_AXIS_RIGHT_ANALOG_HOR;
        e->axis.value = event->value / 32767.0f;
        break;

      case ABS_RZ:
        e->axis.index = ECORE_EVENT_JOYSTICK_AXIS_RIGHT_SHOULDER;
        break;

      case ABS_HAT0X: /* D-pad */
        e->axis.index = ECORE_EVENT_JOYSTICK_AXIS_HAT_X;
        break;

      case ABS_HAT0Y:
        e->axis.index = ECORE_EVENT_JOYSTICK_AXIS_HAT_Y;
        break;

      default:
        break;
     }

   ecore_event_add(ECORE_EVENT_JOYSTICK, e, NULL, NULL);
}

Eina_Bool _fd_handler_cb(void* userData, Ecore_Fd_Handler* fdHandler)
{
   int fd;
   Joystick_Info *ji = userData;
   struct input_event event;
   ssize_t len;

   fd = ecore_main_fd_handler_fd_get(fdHandler);

   len = read(fd, &event, sizeof(event));
   if (len == -1) return ECORE_CALLBACK_RENEW;

   INF("index: %d, type: %d, code: %d, value: %d",
       ji->index, event.type, event.code, event.value);

   _joystick_event_add(&event, ji->index);

   return ECORE_CALLBACK_RENEW;
}

static int
_joystick_index_get(const char *dev)
{
   int plen, dlen, diff, ret = -1;

   dlen = strlen(dev);
   plen = strlen(joystickPrefix);
   diff = dlen - plen;

   if (diff > 0)
     {
        ret = atoi(dev + plen);
     }

   return ret;
}

static void
_joystick_register(const char *dev, const char* syspath)
{
   int fd, index;
   Joystick_Info *ji;

   if (!dev) return;

   index = _joystick_index_get(dev);
   if (index == -1)
     {
        ERR("Invalid index value.");
        return;
     }

   ji = calloc(1, sizeof(Joystick_Info));
   if (!ji)
     {
        ERR("Cannot allocate memory.");
        return;
     }

   ji->index = index;
   ji->system_path = strdup(syspath);

   fd = open(dev, O_RDONLY | O_NONBLOCK);
   ji->fd_handler = ecore_main_fd_handler_add(fd, ECORE_FD_READ,
                                             _fd_handler_cb, ji, 0, 0);

   joystick_list = eina_list_append(joystick_list, ji);
   _joystick_connected_event_add(index, EINA_TRUE);
}

static void
_joystick_unregister(const char *syspath)
{
   int   fd;
   Eina_List *l;
   Joystick_Info *ji;

   EINA_LIST_FOREACH(joystick_list, l, ji)
     {
        if (!strcmp(syspath, ji->system_path))
          {
             fd = ecore_main_fd_handler_fd_get(ji->fd_handler);
             close(fd);
             ecore_main_fd_handler_del(ji->fd_handler);
             joystick_list = eina_list_remove(joystick_list, ji);
             _joystick_connected_event_add(ji->index, EINA_FALSE);
             free(ji->system_path);
             free(ji);
          }
     }
}

Eina_Bool _monitor_handler_cb(void* userData EINA_UNUSED, Ecore_Fd_Handler* fdHandler)
{
   struct udev_device *device;
   const char *syspath, *action, *devnode;

   if (!ecore_main_fd_handler_active_get(fdHandler, ECORE_FD_READ))
     return ECORE_CALLBACK_RENEW;

   device = udev_monitor_receive_device(monitor);
   if (!device)
     {
        ERR("Cannot get device from monitor.");
        return ECORE_CALLBACK_RENEW;
     }

   syspath = udev_device_get_syspath(device);
   if (!syspath)
     {
        ERR("Cannot get syspath from device.");
        return ECORE_CALLBACK_RENEW;
     }

   action = udev_device_get_action(device); 
   if (!action)
     {
        ERR("Action value is NULL.");
        return ECORE_CALLBACK_RENEW;
     }

   if (!strcmp(action, "add"))
     {
        devnode = udev_device_get_devnode(device);
        _joystick_register(devnode, syspath);
        udev_device_unref(device);
     }
   else if (!strcmp(action, "remove"))
     {
        _joystick_unregister(syspath);
     }
   else
     {
        WRN("Unhandled action: %s", action);
     }

   return ECORE_CALLBACK_RENEW;
}

EAPI int
ecore_input_joystick_init(void)
{
   struct udev *udev;
   struct udev_enumerate *en;
   struct udev_list_entry *devs, *cur;
   struct udev_device *device;
   const char *syspath;
   const char *devnode;
   int fd;

   if (++_ecore_input_joystick_init_count != 1)
     return _ecore_input_joystick_init_count;

   udev = udev_new();
   if (!udev)
     {
        ERR("Cannot create udev.");
        return --_ecore_input_joystick_init_count;
     }

   en = udev_enumerate_new(udev);
   if (!en)
     {
        ERR("Cannot create udev enumerate.");
        return --_ecore_input_joystick_init_count;
     }

   udev_enumerate_add_match_subsystem(en, "input");
   udev_enumerate_add_match_property(en, "ID_INPUT_JOYSTICK", "1");

   udev_enumerate_scan_devices(en);
   devs = udev_enumerate_get_list_entry(en);
   udev_list_entry_foreach(cur, devs)
     {
        syspath = udev_list_entry_get_name(cur);
        device = udev_device_new_from_syspath(udev, syspath);
        devnode = udev_device_get_devnode(device);

        if (devnode && eina_str_has_prefix(devnode, joystickPrefix))
          {
             INF("syspath: %s, devnode: %s", syspath, devnode);
             _joystick_register(devnode, syspath);
          }
        udev_device_unref(device);
     }

   /* add monitor */
   monitor = udev_monitor_new_from_netlink(udev, "udev");
   if (!monitor)
     {
        ERR("Cannot create monitor.");
        --_ecore_input_joystick_init_count;
        goto end;
     }

   udev_monitor_filter_add_match_subsystem_devtype(monitor, "input", NULL);
   if (udev_monitor_enable_receiving(monitor))
     {
        ERR("Cannot enable monitor.");
        --_ecore_input_joystick_init_count;
        goto end;
     }

   fd = udev_monitor_get_fd(monitor);
   monitor_handler =  ecore_main_fd_handler_add(fd, ECORE_FD_READ,
                                              _monitor_handler_cb,
                                                      NULL, 0, 0);
end:
   udev_enumerate_unref(en);
   return _ecore_input_joystick_init_count;
}

EAPI int
ecore_input_joystick_shutdown(void)
{
   int   fd;

   if (--_ecore_input_joystick_init_count != 0)
     return _ecore_input_joystick_init_count;

   if (monitor) udev_monitor_unref(monitor);
   if (monitor_handler) 
     {
        fd = ecore_main_fd_handler_fd_get(monitor_handler);
        close(fd);
        ecore_main_fd_handler_del(monitor_handler);
     }

   return _ecore_input_joystick_init_count;
}
