/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2012  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <gdbus.h>
#include <stdlib.h>
#include <string.h>

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/dbus.h>
#include <connman/inet.h>
#include <connman/plugin.h>
#include <connman/network.h>
#include <connman/setting.h>
#include <connman/technology.h>

#include <connman.h>

#define TELEPHONY_SERVICE			"org.tizen.telephony"
#define PS_DBUS_SERVICE				"com.tcore.ps"

#define PS_MASTER_INTERFACE			PS_DBUS_SERVICE ".master"
#define PS_MODEM_INTERFACE			PS_DBUS_SERVICE ".modem"
#define PS_SERVICE_INTERFACE		PS_DBUS_SERVICE ".service"
#define PS_CONTEXT_INTERFACE		PS_DBUS_SERVICE ".context"

/* methods */
#define GET_MODEMS				"GetModems"
#define GET_SERVICES			"GetServices"
#define GET_CONTEXTS			"GetContexts"
#define ACTIVATE_CONTEXT		"Activate"
#define DEACTIVATE_CONTEXT		"Deactivate"
#define GET_PROPERTIES			"GetProperties"
#define SET_PROPERTY			"SetProperties"
#define SET_ALWAYSON			"SetAlwayson"

/* signals */
#define MODEM_ADDED				"ModemAdded"
#define MODEM_REMOVED			"ModemRemoved"
#define SERVICE_ADDED			"ServiceAdded"
#define SERVICE_REMOVED			"ServiceRemoved"
#define CONTEXT_ADDED			"ContextAdded"
#define CONTEXT_REMOVED			"ContextRemoved"
#define PROPERTY_CHANGED		"PropertyChanged"

#if defined TIZEN_CONNMAN_VOWIFI
/*epdg dbus interface*/
#define EPDG_SERVICE				"org.tizen.epdg"
#define EPDG_OBJECT_PATH			"/org/tizen/epdg/service"

#define EPDG_PDP_INTERFACE			EPDG_SERVICE ".Pdp"
#define EPDG_STATE_INTERFACE		EPDG_SERVICE ".State"

/* methods */
#define ACTIVATE_PDP				"ActivatePdp"
#define DEACTIVATE_PDP 				"DeactivatePdp"

/* signals */
#define STATE_INFO					"StateInfo"
#define HANDOVER					"Handover"
#endif

#define TIMEOUT 130000

#define STRING2BOOL(a)	(!(g_strcmp0(a, "TRUE")) ? (TRUE):(FALSE))
#if defined TIZEN_CONNMAN_VOWIFI
#define STRING2INT(a)	(atoi(a))

/**
 * @brief Enumeration
 * ePDG state
 * @since_tizen 2.4.0.3
 */
typedef enum {
	CONNMAN_EPDG_STATE_UNAVAILABLE, /**< ePDG state unavailable - published when WiFi is OFF */
	CONNMAN_EPDG_STATE_AVAILABLE, /**< ePDG state available - published when WiFi is ON and DNS of ePDG server is resolved */
	CONNMAN_EPDG_STATE_NOT_CONNECTED, /**< ePDG state NOT connected - published when IPSec tunnel was disconnected */
	CONNMAN_EPDG_STATE_CONNECTED, /**< ePDG state connected - published when IPSec tunnel was connected */
	CONNMAN_EPDG_STATE_HO_LTE_TO_IWLAN, /**< ePDG state handover from LTE to IWLAN - published after successful handover from LTE to IWLAN */
	CONNMAN_EPDG_STATE_HO_IWLAN_TO_LTE, /**< ePDG state handover from IWLAN to LTE - published after successful handover from IWLAN to LTE */
} ConnmanEpdgState_t;

/**
 * @brief Enumeration
 * ePDG handover direction
 * @since_tizen 2.4.0.3
 */
typedef enum {
	CONNMAN_EPDG_HANDOVER_LTE_TO_IWLAN, /**< ePDG handover from LTE to WLAN - published when ePDG handover from LTE to IWLAN */
	CONNMAN_EPDG_HANDOVER_IWLAN_TO_LTE, /**< ePDG handover from WLAN to LTE - published when ePDG handover from WLAN to LTE */
	CONNMAN_EPDG_HANDOVER_UNKNOWN, /**< No handover information */
} ConnmanEpdgHandoverDirection_t;

static ConnmanEpdgState_t connman_epdg_state = CONNMAN_EPDG_STATE_UNAVAILABLE;
static gint epdg_default_subscription_id = 0;
#endif
#define IS_INTERNET_PROFILE(path) (g_str_has_suffix(path, "_1"))

static DBusConnection *connection;
static GHashTable	*modem_hash;
static GHashTable	*service_hash;
static GHashTable	*network_hash;

struct telephony_service {
	char *path;

	gpointer p_modem;
	char *act;
	gboolean roaming; /* global roaming state */
	gboolean ps_attached; /* packet service is available */
};

struct telephony_modem {
	char *path;

	char *operator;
	gboolean powered;
	gboolean sim_init;
	gboolean flight_mode;
	gboolean data_allowed;
	gboolean roaming_allowed;

	struct connman_device *device;
	struct telephony_service *s_service;
};

struct telephony_network {
	char *path;
	int if_index;
	gboolean routing_only;
	gboolean ipv6_link_only;

	struct connman_network *network;

	enum connman_ipconfig_method ipv4_method;
	struct connman_ipaddress *ipv4_address;

	enum connman_ipconfig_method ipv6_method;
	struct connman_ipaddress *ipv6_address;
};

#if defined TIZEN_CONNMAN_VOWIFI
struct ims_network {
	char *epdg_network_path[2];

	gboolean activating_on_lte;
	gboolean activated_on_lte;
	gboolean activating_on_epdg;
	gboolean activated_on_epdg;

	gboolean epdg_signal;
	gboolean lte_signal;
};

static struct ims_network ims_network_info;
#endif

static int telephony_default_subscription_id = 0;

/* function prototype */
static void telephony_connect(DBusConnection *connection, void *user_data);
static void telephony_disconnect(DBusConnection *connection, void *user_data);
static void __remove_modem(gpointer data);
static void __remove_service(gpointer data);
static void __remove_network(gpointer data);

static int __modem_probe(struct connman_device *device);
static void __modem_remove(struct connman_device *device);
static int __modem_enable(struct connman_device *device);
static int __modem_disable(struct connman_device *device);

static int __network_probe(struct connman_network *network);
static void __network_remove(struct connman_network *network);
static int __network_connect(struct connman_network *network);
static int __network_disconnect(struct connman_network *network);
static int __network_control(struct connman_network *network, connman_bool_t value);

/* dbus request and reply */
static int __dbus_request(const char *service, const char *path, const char *interface,
			const char *method,
			DBusPendingCallNotifyFunction notify, void *user_data,
			DBusFreeFunction free_function, int type, ...);

static int __request_get_modems(void);
static void __response_get_modems(DBusPendingCall *call, void *user_data);
static int __request_get_services(const char *path);
static void __response_get_services(DBusPendingCall *call, void *user_data);
static int __request_get_contexts(struct telephony_modem *modem);
static void __response_get_contexts(DBusPendingCall *call, void *user_data);
static int __request_network_activate(struct connman_network *network);
static void __response_network_activate(DBusPendingCall *call, void *user_data);
static int __request_network_deactivate(struct connman_network *network);
static int __request_control_always(connman_bool_t value);
#if defined TIZEN_CONNMAN_VOWIFI
static int __request_epdg_network_activate(int pdp_type);
#endif

/* telephony internal function */
static void __add_modem(const char *path, DBusMessageIter *prop);
static void __add_service(struct telephony_modem *modem,
			const char *service_path, DBusMessageIter *prop);
static void __add_connman_device(const char *modem_path, const char *operator);
static void __remove_connman_device(struct telephony_modem *modem);
static void __remove_connman_networks(struct connman_device *device);
static int __add_context(struct connman_device *device, const char *path,
							DBusMessageIter *prop);

/* signal handler */
static gboolean __changed_modem(DBusConnection *connection,
				DBusMessage *message, void *user_data);
static gboolean __added_modem(DBusConnection *connection,
				DBusMessage *message, void *user_data);
static gboolean __removed_modem(DBusConnection *connection,
				DBusMessage *message, void *user_data);
static gboolean __changed_service(DBusConnection *connection,
				DBusMessage *message, void *user_data);
static gboolean __added_service(DBusConnection *connection,
				DBusMessage *message, void *user_data);
static gboolean __removed_service(DBusConnection *connection,
				DBusMessage *message, void *user_data);
static gboolean __changed_context(DBusConnection *connection,
				DBusMessage *message, void *user_data);
static gboolean __added_context(DBusConnection *connection,
				DBusMessage *message, void *user_data);
static gboolean __removed_context(DBusConnection *connection,
				DBusMessage *message, void *user_data);

#if defined TIZEN_CONNMAN_VOWIFI
/* signal handler */
static const char *__convert_epdg_state_to_string(ConnmanEpdgState_t EpdgState);
static gboolean __connman_service_is_ims_profile(const char *path);
static int __connman_get_subscriber_id(const char *path);
static gboolean __changed_epdg_state(DBusConnection *connection,
					DBusMessage *message, void *user_data);
static gboolean __handover_context(DBusConnection *connection,
					DBusMessage *message, void *user_data);
#endif

/* device driver */
static struct connman_device_driver modem_driver = {
	.name		= "device",
	.type		= CONNMAN_DEVICE_TYPE_CELLULAR,
	.probe		= __modem_probe,
	.remove		= __modem_remove,
	.enable		= __modem_enable,
	.disable	= __modem_disable,
};

/* network driver */
static struct connman_network_driver network_driver = {
	.name		= "network",
	.type		= CONNMAN_NETWORK_TYPE_CELLULAR,
	.probe		= __network_probe,
	.remove		= __network_remove,
	.connect	= __network_connect,
	.disconnect	= __network_disconnect,
	.control	= __network_control,
};

static int tech_probe(struct connman_technology *technology)
{
	return 0;
}

static void tech_remove(struct connman_technology *technology)
{
	return;
}

static struct connman_technology_driver tech_driver = {
	.name		= "cellular",
	.type		= CONNMAN_SERVICE_TYPE_CELLULAR,
	.probe		= tech_probe,
	.remove		= tech_remove,
};

/* local function */
static void telephony_connect(DBusConnection *connection, void *user_data)
{
	DBG("connection %p", connection);
	modem_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
						g_free, __remove_modem);
	service_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
						g_free, __remove_service);
	network_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
						g_free, __remove_network);

	__request_get_modems();
}

static void telephony_disconnect(DBusConnection *connection, void *user_data)
{
	DBG("connection %p", connection);

	if (modem_hash != NULL) {
		g_hash_table_destroy(modem_hash);
		modem_hash = NULL;
	}

	if (network_hash != NULL) {
		g_hash_table_destroy(network_hash);
		network_hash = NULL;
	}
}

static void __remove_modem(gpointer data)
{
	struct telephony_modem *modem = data;

	__remove_connman_device(modem);

	g_free(modem->path);
	g_free(modem->operator);
	g_free(modem);
}

static void __remove_service(gpointer data)
{
	struct telephony_service *service = data;

	g_free(service->path);
	g_free(service->act);
	g_free(service);
}

static void __remove_network(gpointer data)
{
	struct telephony_network *info = data;
	struct connman_device *device;

	device = connman_network_get_device(info->network);
	if (device != NULL)
		connman_device_remove_network(device, info->network);

	connman_network_unref(info->network);

	g_free(info->path);

	connman_ipaddress_free(info->ipv4_address);
	connman_ipaddress_free(info->ipv6_address);

	g_free(info);
}

static void __set_device_powered(struct telephony_modem *modem,
					gboolean powered)
{
	DBG("set modem(%s) powered(%d)", modem->path, powered);

	if (modem->device)
		connman_device_set_powered(modem->device, powered);
}

static int __check_device_powered(const char *path, gboolean powered)
{
	struct telephony_modem *modem = g_hash_table_lookup(modem_hash, path);

	if (modem == NULL)
		return -ENODEV;

	DBG("check modem (%s) powered (%d)", modem->path, modem->powered);

	if (modem->powered == powered)
		return -EALREADY;

	return 0;
}

static int __modem_probe(struct connman_device *device)
{
	DBG("device %p", device);
	return 0;
}

static void __modem_remove(struct connman_device *device)
{
	DBG("device %p", device);
}

static int __modem_enable(struct connman_device *device)
{
	const char *path = connman_device_get_string(device, "Path");
	DBG("device %p, path, %s", device, path);

	return __check_device_powered(path, TRUE);
}

static int __modem_disable(struct connman_device *device)
{
	const char *path = connman_device_get_string(device, "Path");
	DBG("device %p, path, %s", device, path);

	return __check_device_powered(path, FALSE);
}

static int __network_probe(struct connman_network *network)
{
	DBG("network_prove network(%p)", network);
	return 0;
}

static int __network_connect(struct connman_network *network)
{
	DBG("network %p", network);

	return __request_network_activate(network);
}

static int __network_disconnect(struct connman_network *network)
{
	DBG("network %p", network);

	if (connman_network_get_associating(network) == TRUE)
		connman_network_clear_associating(network);

	connman_network_set_associating(network, FALSE);

	return __request_network_deactivate(network);
}

static int __network_control(struct connman_network *network, connman_bool_t value)
{
	DBG("network %p value(%d)", network, value);

	return __request_control_always(value);
}

static void __network_remove(struct connman_network *network)
{
	char const *path = connman_network_get_string(network, "Path");
	DBG("network %p path %s", network, path);

	g_hash_table_remove(network_hash, path);
}

static int __dbus_request(const char *service, const char *path, const char *interface,
		const char *method,
		DBusPendingCallNotifyFunction notify, void *user_data,
		DBusFreeFunction free_function, int type, ...)
{
	DBusMessage *message;
	DBusPendingCall *call;
	dbus_bool_t ok;
	va_list va;

	DBG("%s path %s %s.%s", service, path, interface, method);

	if (path == NULL)
		return -EINVAL;

	message = dbus_message_new_method_call(service, path, interface, method);
	if (message == NULL)
		return -ENOMEM;

	dbus_message_set_auto_start(message, FALSE);

	va_start(va, type);
	ok = dbus_message_append_args_valist(message, type, va);
	va_end(va);

	if (!ok)
		return -ENOMEM;

	if (dbus_connection_send_with_reply(connection, message,
						&call, TIMEOUT) == FALSE) {
		connman_error("Failed to call %s.%s", interface, method);
		dbus_message_unref(message);
		return -EINVAL;
	}

	if (call == NULL) {
		connman_error("D-Bus connection not available");
		dbus_message_unref(message);
		return -EINVAL;
	}

	dbus_pending_call_set_notify(call, notify, user_data, free_function);

	if(notify == NULL)
		dbus_pending_call_unref(call);

	dbus_message_unref(message);

	return -EINPROGRESS;
}

static int __request_get_modems(void)
{
	DBG("request get modem");
	/* call connect master */
	return __dbus_request(PS_DBUS_SERVICE, "/", PS_MASTER_INTERFACE, GET_MODEMS,
			__response_get_modems, NULL, NULL, DBUS_TYPE_INVALID);
}

static void __response_get_modems(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply;
	DBusError error;
	DBusMessageIter args, dict;

	DBG("");

	reply = dbus_pending_call_steal_reply(call);

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, reply)) {
		connman_error("GetModems() %s %s", error.name, error.message);
		dbus_error_free(&error);
		goto done;
	}

	DBG("message signature (%s)", dbus_message_get_signature(reply));

	if (dbus_message_iter_init(reply, &args) == FALSE)
		goto done;

	dbus_message_iter_recurse(&args, &dict);

	/* DBG("message type (%d) dic(%d)",
	 *	dbus_message_iter_get_arg_type(&dict), DBUS_TYPE_DICT_ENTRY);
	 */

	while (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_INVALID) {
		DBusMessageIter entry, property;
		const char *modem_path;

		dbus_message_iter_recurse(&dict, &entry);
		dbus_message_iter_get_basic(&entry, &modem_path);
		DBG("modem path (%s)", modem_path);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &property);

		__add_modem(modem_path, &property);

		dbus_message_iter_next(&dict);
	}

done:
	dbus_message_unref(reply);
	dbus_pending_call_unref(call);
}

static int __request_get_services(const char *path)
{
	DBG("request get service");
	return __dbus_request(PS_DBUS_SERVICE, path, PS_MODEM_INTERFACE, GET_SERVICES,
			__response_get_services, g_strdup(path),
			g_free, DBUS_TYPE_INVALID);
}

static void __response_get_services(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply;
	DBusError error;
	DBusMessageIter args, dict;

	const char *path = user_data;
	struct telephony_modem *modem;

	modem = g_hash_table_lookup(modem_hash, path);
	if (modem == NULL) {
		dbus_pending_call_unref(call);
		return;
	}
	if (modem->device == NULL) {
		dbus_pending_call_unref(call);
		return;
	}

	DBG("");

	reply = dbus_pending_call_steal_reply(call);

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, reply)) {
		connman_error("GetServices() %s %s", error.name, error.message);
		dbus_error_free(&error);
		goto done;
	}

	DBG("message signature (%s)", dbus_message_get_signature(reply));

	if (dbus_message_iter_init(reply, &args) == FALSE)
		goto done;

	dbus_message_iter_recurse(&args, &dict);

	/* DBG("message type (%d) dic(%d)",
	 *	 dbus_message_iter_get_arg_type(&dict), DBUS_TYPE_DICT_ENTRY);
	 */

	while (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_INVALID) {
		DBusMessageIter entry, property;
		const char *service_path;

		dbus_message_iter_recurse(&dict, &entry);
		dbus_message_iter_get_basic(&entry, &service_path);
		DBG("service path (%s)", service_path);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &property);

		__add_service(modem, service_path, &property);

		dbus_message_iter_next(&dict);
	}

done:
	dbus_message_unref(reply);
	dbus_pending_call_unref(call);
}

static int __request_get_contexts(struct telephony_modem *modem)
{
	DBG("request get contexts");
	return __dbus_request(PS_DBUS_SERVICE, modem->s_service->path,
			PS_SERVICE_INTERFACE, GET_CONTEXTS,
			__response_get_contexts, g_strdup(modem->path),
			g_free, DBUS_TYPE_INVALID);
}

static void __response_get_contexts(DBusPendingCall *call, void *user_data)
{
	DBusError error;
	DBusMessage *reply;
	DBusMessageIter args, dict;

	const char *path = user_data;
	struct telephony_modem *modem;

	DBG("");

	modem = g_hash_table_lookup(modem_hash, path);
	if (modem == NULL) {
		dbus_pending_call_unref(call);
		return;
	}
	if (modem->s_service == NULL) {
		dbus_pending_call_unref(call);
		return;
	}
	if (modem->device == NULL) {
		dbus_pending_call_unref(call);
		return;
	}

	reply = dbus_pending_call_steal_reply(call);

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, reply)) {
		connman_error("GetContexts() %s %s", error.name, error.message);
		dbus_error_free(&error);
		goto done;
	}

	DBG("message signature (%s)", dbus_message_get_signature(reply));

	if (dbus_message_iter_init(reply, &args) == FALSE)
		goto done;

	dbus_message_iter_recurse(&args, &dict);

	while (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_INVALID) {
		DBusMessageIter entry, property;
		const char *context_path;

		dbus_message_iter_recurse(&dict, &entry);
		dbus_message_iter_get_basic(&entry, &context_path);
		DBG("context path (%s)", context_path);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &property);

		__add_context(modem->device, context_path, &property);

		dbus_message_iter_next(&dict);
	}

done:
	dbus_message_unref(reply);
	dbus_pending_call_unref(call);
}

static int __request_network_activate(struct connman_network *network)
{
	int n_modems;
	const char *path = NULL;
	struct telephony_modem *modem = NULL;

	n_modems = g_hash_table_size(modem_hash);
	path = connman_network_get_string(network, "Path");
	modem = connman_device_get_data(connman_network_get_device(network));

	DBG("network %p, path %s, modem %s[%d]", network, path, modem->path,
			telephony_default_subscription_id);

	if (modem && n_modems > 1 && g_str_has_suffix(path, "_1") == TRUE) {
		char *subscribe_id = g_strdup_printf("%d", telephony_default_subscription_id);

		if (g_str_has_suffix(modem->path, subscribe_id) != TRUE) {
			g_free(subscribe_id);
			return -ENOLINK;
		}
		g_free(subscribe_id);
	}

	return __dbus_request(PS_DBUS_SERVICE, path, PS_CONTEXT_INTERFACE, ACTIVATE_CONTEXT,
			__response_network_activate,
			g_strdup(path), g_free, DBUS_TYPE_INVALID);
}

#if defined TIZEN_CONNMAN_VOWIFI
static int __request_epdg_network_activate(int pdp_type)
{
	int ret = 0;

	DBG("EPDG State %s path %s[%d], Activating IMS PDN over IWLAN.",
			__convert_epdg_state_to_string(connman_epdg_state), EPDG_OBJECT_PATH,
			epdg_default_subscription_id);

	ret = __dbus_request(EPDG_SERVICE, EPDG_OBJECT_PATH, EPDG_PDP_INTERFACE, ACTIVATE_PDP,
			__response_network_activate,
			g_strdup(EPDG_OBJECT_PATH), g_free,
			DBUS_TYPE_INT32, &epdg_default_subscription_id,
			DBUS_TYPE_INT32, &pdp_type, DBUS_TYPE_INVALID);

	if (ret != -EINPROGRESS)
		DBG("fail to activate IMS PDN over EPDG, error %d", ret);

	return ret;
}
#endif

static gboolean __check_network_available(struct connman_network *network)
{
	if (network == NULL || connman_network_get_device(network) == NULL)
		return FALSE;

	return TRUE;
}

static void __response_network_activate(DBusPendingCall *call, void *user_data)
{
	DBG("network activation response");

	DBusError error;
	DBusMessage *reply;

	struct telephony_network *info;
	const char *path = user_data;
#if defined TIZEN_CONNMAN_VOWIFI
	gboolean epdg_activation_request;

	if (g_strcmp0(path, EPDG_OBJECT_PATH) == 0) {
		if (!ims_network_info.epdg_network_path[epdg_default_subscription_id]) {
			DBG("epdg_network_path (nil)");
			dbus_pending_call_unref(call);
			return;
		}

		path = ims_network_info.epdg_network_path[epdg_default_subscription_id];
		epdg_activation_request = TRUE;
	} else
		epdg_activation_request = FALSE;
#endif
	info = g_hash_table_lookup(network_hash, path);
	reply = dbus_pending_call_steal_reply(call);

	if (info == NULL)
		goto done;

	if (__check_network_available(info->network) == FALSE) {
		g_hash_table_remove(network_hash, path);
		goto done;
	}

	dbus_error_init(&error);
	if (dbus_set_error_from_message(&error, reply)) {
		connman_error("connection activate() %s %s",
					error.name, error.message);

		if (connman_network_get_associating(info->network) == TRUE)
			connman_network_set_error(info->network,
					CONNMAN_NETWORK_ERROR_ASSOCIATE_FAIL);

		if (connman_network_get_connecting(info->network) == TRUE)
			connman_network_set_error(info->network,
					CONNMAN_NETWORK_ERROR_CONNECT_FAIL);

		if (connman_network_get_index(info->network) < 0)
			connman_network_set_error(info->network,
					CONNMAN_NETWORK_ERROR_ASSOCIATE_FAIL);

		dbus_error_free(&error);
		goto done;
	}
#if defined TIZEN_CONNMAN_VOWIFI
	if (__connman_service_is_ims_profile(path)) {
		if (epdg_activation_request)
			ims_network_info.activating_on_epdg = TRUE;
		else
			ims_network_info.activating_on_lte = TRUE;
		DBG("%s activation requested successfully",
				epdg_activation_request?"VoWiFi":"VoLTE");
	}
#endif

done:
	dbus_message_unref(reply);
	dbus_pending_call_unref(call);
}

static int __request_network_deactivate(struct connman_network *network)
{
	const char *path = connman_network_get_string(network, "Path");
#if defined TIZEN_CONNMAN_VOWIFI
	int ret = 0;
	DBG("network %p, path %s", network, path);

	if (__connman_service_is_ims_profile(path)) {
		if (ims_network_info.activated_on_epdg || ims_network_info.activating_on_epdg) {
			DBG("deactivate IMS PDN over IWLAN.");
			ret = __dbus_request(EPDG_SERVICE, EPDG_OBJECT_PATH, EPDG_PDP_INTERFACE, DEACTIVATE_PDP,
					NULL, NULL, NULL, DBUS_TYPE_INVALID);
		}

		if (ims_network_info.activated_on_lte || ims_network_info.activating_on_lte) {
			DBG("deactivate IMS PDN over LTE also.");
			ret = __dbus_request(PS_DBUS_SERVICE, path, PS_CONTEXT_INTERFACE, DEACTIVATE_CONTEXT,
					NULL, NULL, NULL, DBUS_TYPE_INVALID);
		}
	} else {
		ret = __dbus_request(PS_DBUS_SERVICE, path, PS_CONTEXT_INTERFACE, DEACTIVATE_CONTEXT,
				NULL, NULL, NULL, DBUS_TYPE_INVALID);
	}

	return ret;
#else
	DBG("network %p, path %s", network, path);

	return __dbus_request(PS_DBUS_SERVICE, path, PS_CONTEXT_INTERFACE, DEACTIVATE_CONTEXT,
			NULL, NULL, NULL, DBUS_TYPE_INVALID);
#endif
}

static void __response_get_default_subscription_id(DBusPendingCall *call,
		void *user_data)
{
	DBusMessage *reply;
	DBusError error;
	DBusMessageIter args;

	DBG("");

	reply = dbus_pending_call_steal_reply(call);

	dbus_error_init(&error);
	if (dbus_set_error_from_message(&error, reply)) {
		connman_error("GetDefaultDataSubscription() %s %s", error.name, error.message);
		dbus_error_free(&error);
		goto done;
	}

	DBG("message signature (%s)", dbus_message_get_signature(reply));

	if (dbus_message_iter_init(reply, &args) == FALSE)
		goto done;

	dbus_message_iter_get_basic(&args, &telephony_default_subscription_id);
	DBG("default subscription: %d", telephony_default_subscription_id);

done:
	dbus_message_unref(reply);
	dbus_pending_call_unref(call);
}

static int __request_get_default_subscription_id(const char *path)
{
	int ret;
	char *telephony_modem_path = NULL;

	telephony_modem_path = g_strdup_printf("/org/tizen/telephony%s", path);
	DBG("request get default subscription id %s", telephony_modem_path);

	ret = __dbus_request(TELEPHONY_SERVICE, telephony_modem_path,
			"org.tizen.telephony.Network", "GetDefaultDataSubscription",
			__response_get_default_subscription_id, NULL, NULL, DBUS_TYPE_INVALID);

	g_free(telephony_modem_path);
	return ret;
}

static int __request_control_always(connman_bool_t value)
{
	DBG("request set always on value(%d)", value);

	/* set always on */
	return __dbus_request(PS_DBUS_SERVICE, "/", PS_MASTER_INTERFACE, SET_ALWAYSON,
			NULL, NULL, NULL, DBUS_TYPE_BOOLEAN, &value, DBUS_TYPE_INVALID);
}

static void __add_modem(const char *path, DBusMessageIter *prop)
{
	struct telephony_modem *modem;

	modem = g_hash_table_lookup(modem_hash, path);
	if (modem != NULL)
		return;

	modem = g_try_new0(struct telephony_modem, 1);
	if (modem == NULL)
		return;

	modem->path = g_strdup(path);
	modem->device = NULL;
	modem->s_service = NULL;

	g_hash_table_insert(modem_hash, g_strdup(path), modem);

	while (dbus_message_iter_get_arg_type(prop) != DBUS_TYPE_INVALID) {
		DBusMessageIter entry;
		const char *key, *tmp;

		dbus_message_iter_recurse(prop, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_get_basic(&entry, &tmp);

		DBG("key (%s) value(%s)", key, tmp);

		if (g_strcmp0(key, "powered") == 0) {
			modem->powered = STRING2BOOL(tmp);
		} else if (g_strcmp0(key, "operator") == 0) {
			g_free(modem->operator);
			modem->operator = g_strdup(tmp);
		} else if (g_strcmp0(key, "sim_init") == 0) {
			modem->sim_init = STRING2BOOL(tmp);
		} else if (g_strcmp0(key, "flight_mode") == 0) {
			modem->flight_mode = STRING2BOOL(tmp);
		} else if (g_strcmp0(key, "roaming_allowed") == 0) {
			modem->roaming_allowed = STRING2BOOL(tmp);
		} else if (g_strcmp0(key, "data_allowed") == 0) {
			modem->data_allowed = STRING2BOOL(tmp);
		}
		dbus_message_iter_next(prop);
	}

	__add_connman_device(path, modem->operator);
	__set_device_powered(modem, modem->powered);

	if (g_hash_table_size(modem_hash) > 1)
		__request_get_default_subscription_id(modem->path);

	if (modem->powered != TRUE) {
		DBG("modem is not powered");
		return;
	}

	__request_get_services(modem->path);
}

static void __add_service(struct telephony_modem *modem,
				const char *service_path, DBusMessageIter *prop)
{
	struct telephony_service *service;

	if (modem->s_service != NULL)
		return;

	service = g_try_new0(struct telephony_service, 1);
	if (service == NULL)
		return;

	service->path = g_strdup(service_path);
	service->p_modem = modem;
	g_hash_table_insert(service_hash, g_strdup(service_path), service);

	while (dbus_message_iter_get_arg_type(prop) != DBUS_TYPE_INVALID) {
		DBusMessageIter entry;
		const char *key, *tmp;

		dbus_message_iter_recurse(prop, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_get_basic(&entry, &tmp);

		DBG("key (%s) value(%s)", key, tmp);

		if (g_strcmp0(key, "roaming") == 0) {
			service->roaming = STRING2BOOL(tmp);
		} else if (g_strcmp0(key, "act") == 0) {
			service->act = g_strdup(tmp);
		} else if (g_strcmp0(key, "ps_attached") == 0) {
			service->ps_attached = STRING2BOOL(tmp);
		}

		dbus_message_iter_next(prop);
	}

	modem->s_service = service;
	__request_get_contexts(modem);
}

static char *__get_ident(const char *path)
{
	char *pos;

	if (*path != '/')
		return NULL;

	pos = strrchr(path, '/');
	if (pos == NULL)
		return NULL;

	return pos + 1;
}

static void __add_connman_device(const char *modem_path, const char *operator)
{
	char* ident = NULL;
	struct telephony_modem *modem;
	struct connman_device *device;

	DBG("path %s operator %s", modem_path, operator);

	if (modem_path == NULL)
		return;

	if (operator == NULL)
		return;

	modem = g_hash_table_lookup(modem_hash, modem_path);
	if (modem == NULL)
		return;

	if (modem->device) {
		if (!g_strcmp0(operator,
				connman_device_get_ident(modem->device)))
			return;

		__remove_connman_device(modem);
	}

	if (strlen(operator) == 0)
		return;

	device = connman_device_create(operator, CONNMAN_DEVICE_TYPE_CELLULAR);
	if (device == NULL)
		return;

	ident = g_strdup_printf("%s_%s", __get_ident(modem_path), operator);
	connman_device_set_ident(device, ident);
	g_free(ident);

	connman_device_set_string(device, "Path", modem_path);
	connman_device_set_data(device, modem);

	if (connman_device_register(device) < 0) {
		connman_error("Failed to register cellular device");
		connman_device_unref(device);
		return;
	}

	modem->device = device;
}

static void __remove_connman_device(struct telephony_modem *modem)
{
	DBG("modem %p path %s device %p", modem, modem->path, modem->device);

	if (modem->device == NULL)
		return;

	__remove_connman_networks(modem->device);

	connman_device_unregister(modem->device);
	connman_device_unref(modem->device);

	modem->device = NULL;
}

static void __remove_connman_networks(struct connman_device *device)
{
	GHashTableIter iter;
	gpointer key, value;
	GSList *info_list = NULL;
	GSList *list;

	if (network_hash == NULL)
		return;

	g_hash_table_iter_init(&iter, network_hash);

	while (g_hash_table_iter_next(&iter, &key, &value) == TRUE) {
		struct telephony_network *info = value;

		if (connman_network_get_device(info->network) != device)
			continue;

		info_list = g_slist_append(info_list, info);
	}

	for (list = info_list; list != NULL; list = list->next) {
		struct telephony_network *info = list->data;
		connman_device_remove_network(device, info->network);
	}

	g_slist_free(info_list);
}

static gboolean connman_ipaddress_updated(struct connman_ipaddress *ipaddress,
					const char *address, const char *gateway)
{
	if (ipaddress == NULL || address == NULL)
		return FALSE;

	if (g_strcmp0(ipaddress->local, address) != 0)
		return TRUE;

	if (g_strcmp0(ipaddress->gateway, gateway) != 0)
		return TRUE;

	return FALSE;
}

static void __set_network_connected(struct telephony_network *network,
					connman_bool_t connected)
{
	gboolean setip = FALSE;

	DBG("network %p connected %d", network, connected);

	connman_network_set_index(network->network, network->if_index);
	if (connman_network_get_connected(network->network) == connected)
		return;

	switch (network->ipv4_method) {
	case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
	case CONNMAN_IPCONFIG_METHOD_DHCP:
	case CONNMAN_IPCONFIG_METHOD_AUTO:
	case CONNMAN_IPCONFIG_METHOD_OFF:
		connman_network_set_ipv4_method(network->network,
							network->ipv4_method);
		break;
	case CONNMAN_IPCONFIG_METHOD_MANUAL:
	case CONNMAN_IPCONFIG_METHOD_FIXED:
		connman_network_set_ipv4_method(network->network,
							network->ipv4_method);
		connman_network_set_ipaddress(network->network,
							network->ipv4_address);
		setip = TRUE;
		break;
	}

	switch (network->ipv6_method) {
	case CONNMAN_IPCONFIG_METHOD_UNKNOWN:
	case CONNMAN_IPCONFIG_METHOD_OFF:
	case CONNMAN_IPCONFIG_METHOD_DHCP:
		break;
	case CONNMAN_IPCONFIG_METHOD_AUTO:
		connman_network_set_ipv6_method(network->network,
							network->ipv6_method);
		setip = TRUE;
		break;
	case CONNMAN_IPCONFIG_METHOD_MANUAL:
	case CONNMAN_IPCONFIG_METHOD_FIXED:
		connman_network_set_ipv6_method(network->network,
							network->ipv6_method);
		connman_network_set_ipaddress(network->network,
							network->ipv6_address);
		setip = TRUE;
		break;
	}

	if (setip == TRUE)
		connman_network_set_connected(network->network, connected);
}

static connman_bool_t __set_network_context(
							struct telephony_network *network,
							DBusMessageIter *dict)
{
	int index = 0;
	connman_bool_t active = FALSE;
	gboolean routing_only = FALSE;
	gboolean ipv4_updated = FALSE;
	gboolean ipv6_updated = FALSE;
	gboolean ipv6_link_only = FALSE;
	gboolean default_internet = FALSE;
	gboolean active_proxy = FALSE;
	char **proxies = NULL;
	const char *dev_name = NULL;
	const char *proxy_addr = NULL;
	char *ipv4_addr = NULL, *ipv4_gw = NULL, *ipv4_netmask = NULL,
					*ipv4_dns1 = NULL, *ipv4_dns2 = NULL;
	char *ipv6_addr = NULL, *ipv6_gw = NULL, *ipv6_netmask = NULL,
					*ipv6_dns1 = NULL, *ipv6_dns2 = NULL;
	struct connman_service *service;
	gboolean is_internet_profile = FALSE;

	while (dbus_message_iter_get_arg_type(dict) != DBUS_TYPE_INVALID) {
		DBusMessageIter entry;
		const char *key, *value;

		dbus_message_iter_recurse(dict, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);

		if (g_strcmp0(key, "dev_name") == 0) {
			dbus_message_iter_get_basic(&entry, &dev_name);
			DBG("dev_name (%s)", dev_name);
		} else if (g_strcmp0(key, "proxy") == 0) {
			dbus_message_iter_get_basic(&entry, &proxy_addr);
			DBG("proxy_addr (%s)", proxy_addr);
		} else if (g_strcmp0(key, "ipv4_address") == 0) {
			dbus_message_iter_get_basic(&entry, &ipv4_addr);
			DBG("ipv4_addr (%s)", ipv4_addr);
		} else if (g_strcmp0(key, "ipv4_gateway") == 0) {
			dbus_message_iter_get_basic(&entry, &ipv4_gw);
			DBG("ipv4_gw (%s)", ipv4_gw);
		} else if (g_strcmp0(key, "ipv4_netmask") == 0) {
			dbus_message_iter_get_basic(&entry, &ipv4_netmask);
			DBG("ipv4_netmask (%s)", ipv4_netmask);
		} else if (g_strcmp0(key, "ipv4_dns1") == 0) {
			dbus_message_iter_get_basic(&entry, &ipv4_dns1);
			DBG("ipv4_dns1 (%s)", ipv4_dns1);
		} else if (g_strcmp0(key, "ipv4_dns2") == 0) {
			dbus_message_iter_get_basic(&entry, &ipv4_dns2);
			DBG("ipv4_dns2 (%s)", ipv4_dns2);
		} else if (g_strcmp0(key, "ipv6_address") == 0) {
			dbus_message_iter_get_basic(&entry, &ipv6_addr);
			DBG("ipv6 address (%s)", ipv6_addr);
		} else if (g_strcmp0(key, "ipv6_gateway") == 0) {
			dbus_message_iter_get_basic(&entry, &ipv6_gw);
			DBG("ipv6_gw (%s)", ipv6_gw);
		} else if (g_strcmp0(key, "ipv6_netmask") == 0) {
			dbus_message_iter_get_basic(&entry, &ipv6_netmask);
			DBG("ipv6_netmask (%s)", ipv6_netmask);
		} else if (g_strcmp0(key, "ipv6_dns1") == 0) {
			dbus_message_iter_get_basic(&entry, &ipv6_dns1);
			DBG("ipv6_dns1 (%s)", ipv6_dns1);
		} else if (g_strcmp0(key, "ipv6_dns2") == 0) {
			dbus_message_iter_get_basic(&entry, &ipv6_dns2);
			DBG("ipv6_dns2 (%s)", ipv6_dns2);
		} else if (g_strcmp0(key, "active") == 0) {
			dbus_message_iter_get_basic(&entry, &value);
			DBG("active (%s)", value);
			active = STRING2BOOL(value);
		} else if (g_strcmp0(key, "routing_only") == 0) {
			dbus_message_iter_get_basic(&entry, &value);
			DBG("routing_only (%s)", value);
			routing_only = STRING2BOOL(value);
			network->routing_only = routing_only;
		} else if (g_strcmp0(key, "ipv6_link_only") == 0) {
			dbus_message_iter_get_basic(&entry, &value);
			DBG("ipv6_link_only (%s)", value);
			ipv6_link_only = STRING2BOOL(value);
			network->ipv6_link_only = ipv6_link_only;
		} else if (g_strcmp0(key, "default_internet_conn") == 0) {
			dbus_message_iter_get_basic(&entry, &value);
			DBG("default_internet (%s)", value);
			default_internet = STRING2BOOL(value);
		}
		dbus_message_iter_next(dict);
	}

	if(routing_only){
		//context active does not effect the connman service status.
		//it only for setting the routing path.
		DBG("routing_only(%d), active(%d)", routing_only, active);
		return active;
	}

	if (g_strcmp0(proxy_addr, ":") == 0)
		proxy_addr = NULL;
	if (g_strcmp0(ipv4_addr, "0.0.0.0") == 0)
		ipv4_addr = NULL;
	if (g_strcmp0(ipv4_gw, "0.0.0.0") == 0)
		ipv4_gw = NULL;
	if (g_strcmp0(ipv4_netmask, "0.0.0.0") == 0)
		ipv4_netmask = NULL;
	if (g_strcmp0(ipv4_dns1, "0.0.0.0") == 0)
		ipv4_dns1 = NULL;
	if (g_strcmp0(ipv4_dns2, "0.0.0.0") == 0)
		ipv4_dns2 = NULL;
	if (g_strcmp0(ipv6_addr, "::") == 0)
		ipv6_addr = NULL;
	if (g_strcmp0(ipv6_gw, "::") == 0)
		ipv6_gw = NULL;
	if (g_strcmp0(ipv6_netmask, "::") == 0)
		ipv6_netmask = NULL;
	if (g_strcmp0(ipv6_dns1, "::") == 0)
		ipv6_dns1 = NULL;
	if (g_strcmp0(ipv6_dns2, "::") == 0)
		ipv6_dns2 = NULL;

	connman_network_set_bool(network->network, "DefaultInternet",
								default_internet);

	service = connman_service_lookup_from_network(network->network);
	if (service == NULL)
		return FALSE;

	/* If profile is IMS, do not check for single connected technology */
	is_internet_profile = IS_INTERNET_PROFILE(__connman_service_get_path(service));
	if (TRUE == is_internet_profile &&
			connman_setting_get_bool("SingleConnectedTechnology") == TRUE) {
		/* Wi-Fi technology is always a top priority */
		if (active == TRUE &&
				connman_service_is_no_ref_user_pdn_connection(service) == TRUE &&
				connman_service_get_type(connman_service_get_default_connection())
					== CONNMAN_SERVICE_TYPE_WIFI) {
			__request_control_always(FALSE);
			__request_network_deactivate(network->network);

			return FALSE;
		}
	}

#if defined TIZEN_CONNMAN_VOWIFI
	if (connman_service_is_ims_profile(service)) {
		if (!active && connman_network_get_connected(network->network)) {
			if (ims_network_info.lte_signal) {
				if (ims_network_info.activating_on_epdg ||
						ims_network_info.activated_on_epdg)
					return FALSE;
			} else if (ims_network_info.epdg_signal) {
				if (ims_network_info.activating_on_lte ||
						ims_network_info.activated_on_lte)
					return FALSE;
			}
		}
	}
#endif

	/* interface index set */
	if (dev_name != NULL) {
		index = connman_inet_ifindex(dev_name);
		network->if_index = index;
		DBG("interface index %d", index);
	}

	/* proxy set */
	if (active == TRUE &&
			connman_network_get_connected(network->network) == TRUE)
		active_proxy = TRUE;

	proxies = connman_service_get_proxy_servers(service);
	if (proxies != NULL) {
		if (proxy_addr == NULL)
			connman_service_set_proxy(service, proxy_addr, active_proxy);
		else if (g_strcmp0(proxy_addr, proxies[0]) != 0)
			connman_service_set_proxy(service, proxy_addr, active_proxy);
	} else if (proxy_addr != NULL)
		connman_service_set_proxy(service, proxy_addr, active_proxy);

	if (proxies != NULL)
		g_strfreev(proxies);

	__connman_service_nameserver_clear(service);

	/* ipv4 set */
	if (network->ipv4_address == NULL)
		network->ipv4_address =
				connman_ipaddress_alloc(CONNMAN_IPCONFIG_TYPE_IPV4);

	if (network->ipv4_address == NULL)
		return FALSE;

	if (ipv4_addr == NULL && active == TRUE)
		network->ipv4_method = CONNMAN_IPCONFIG_METHOD_OFF;
	else
		network->ipv4_method = CONNMAN_IPCONFIG_METHOD_FIXED;

	connman_network_set_ipv4_method(network->network, network->ipv4_method);

	ipv4_updated = connman_ipaddress_updated(network->ipv4_address,
						ipv4_addr, ipv4_gw);
	if (ipv4_updated == TRUE)
		connman_ipaddress_set_ipv4(network->ipv4_address, ipv4_addr,
						ipv4_netmask, ipv4_gw);

	if (ipv4_dns1 && ipv4_addr)
		__connman_service_nameserver_append(service, ipv4_dns1, FALSE);

	/*
	* Problem: PLM P160317-03838: Internet browsing fails
	* Cause: DNS request made to primary DNS server failed.
	* Reason: DNS request is not sent on sec DNS as primary DNS server is not working
	* Solution: Add secondary DNS server as well to nameserver list
	* In case if primary dns server is not working properly, DNS will be success with sec DNS
	*/
	if (ipv4_dns2 && ipv4_addr)
	/*if (ipv4_dns2)// && !ipv4_dns1) */
		__connman_service_nameserver_append(service, ipv4_dns2, FALSE);

	/* ipv6 set */
	if (network->ipv6_address == NULL)
		network->ipv6_address =
				connman_ipaddress_alloc(CONNMAN_IPCONFIG_TYPE_IPV6);

	if (network->ipv6_address == NULL)
		return FALSE;

	if(ipv6_link_only)
		network->ipv6_method = CONNMAN_IPCONFIG_METHOD_AUTO;
	else
		network->ipv6_method = CONNMAN_IPCONFIG_METHOD_FIXED;

	if (ipv6_addr == NULL)
		network->ipv6_method = CONNMAN_IPCONFIG_METHOD_OFF;

	connman_network_set_ipv6_method(network->network, network->ipv6_method);

	ipv6_updated = connman_ipaddress_updated(network->ipv6_address,
						ipv6_addr, ipv6_gw);
	if (ipv6_updated == TRUE)
		connman_ipaddress_set_ipv6(network->ipv6_address, ipv6_addr,
						64, ipv6_gw);

	if (ipv6_dns1 && ipv6_addr)
		__connman_service_nameserver_append(service, ipv6_dns1, FALSE);

	/* Add secondary DNS server as well to nameserver list */
	if (ipv6_dns2 && ipv6_addr)
	/*if (ipv6_dns2 && !ipv6_dns1)*/
		__connman_service_nameserver_append(service, ipv6_dns2, FALSE);

	if (active == TRUE &&
			connman_network_get_connected(network->network) == TRUE) {
		/* disconnect network only when IPv4 updated */
		if (ipv4_updated == TRUE) {
			DBG("IPv4 updated %d, IPv6 updated %d", ipv4_updated, ipv6_updated);
			__set_network_connected(network, FALSE);
		} else {
			DBG("Already connected");
			return active;
		}
	}

	if (active == TRUE)
		connman_network_set_associating(network->network, TRUE);

	return active;
}

static int __add_context(struct connman_device *device, const char *path,
				DBusMessageIter *prop)
{
	char *ident;
	connman_bool_t active = FALSE;
#if defined TIZEN_CONNMAN_VOWIFI
	gint subscription_id = 0;
#endif

	struct telephony_modem *modem = connman_device_get_data(device);
	struct connman_network *network;
	struct telephony_network *info;

	DBG("modem %p device %p path %s", modem, device, path);

	ident = __get_ident(path);

	network = connman_device_get_network(device, ident);
	if (network != NULL)
		return -EALREADY;

	info = g_hash_table_lookup(network_hash, path);
	if (info != NULL) {
		DBG("path %p already exists with device %p", path,
			connman_network_get_device(info->network));

		if (connman_network_get_device(info->network))
			return -EALREADY;

		g_hash_table_remove(network_hash, path);
	}

	network = connman_network_create(ident, CONNMAN_NETWORK_TYPE_CELLULAR);
	if (network == NULL)
		return -ENOMEM;

	info = g_try_new0(struct telephony_network, 1);
	if (info == NULL) {
		connman_network_unref(network);
		return -ENOMEM;
	}

	info->path = g_strdup(path);
#if defined TIZEN_CONNMAN_VOWIFI
	if (path && __connman_service_is_ims_profile(path)) {
		subscription_id = __connman_get_subscriber_id(path);
		g_free(ims_network_info.epdg_network_path[subscription_id]);
		ims_network_info.epdg_network_path[subscription_id] = g_strdup(path);
	}
#endif

	connman_ipaddress_clear(info->ipv4_address);
	connman_ipaddress_clear(info->ipv6_address);

	info->network = network;

	connman_network_set_string(network, "Path", path);
	connman_network_set_name(network, path);

	connman_network_set_group(network, ident);

	g_hash_table_insert(network_hash, g_strdup(path), info);

	connman_network_set_available(network, TRUE);
	connman_network_set_bool(network, "Roaming", modem->s_service->roaming);

	if (connman_device_add_network(device, network) != 0) {
		g_hash_table_remove(network_hash, path);
		return -EIO;
	}

	active = __set_network_context(info, prop);
	if(info->routing_only){
		int err = 0;
		struct connman_service *routing_service;
		struct connman_ipconfig *routing_ipconfig;

		if(!active)
			return TRUE;

		routing_service = connman_service_lookup_from_network(info->network);
		routing_ipconfig = __connman_service_get_ip4config(routing_service);
		err = __connman_ipconfig_gateway_add(routing_ipconfig, routing_service);

		DBG("set gateway rv(%d)", err);
		return TRUE;
	}

	if (active == TRUE && (connman_network_get_associating(network) == TRUE ||
			connman_network_get_connecting(network) == TRUE))
		__set_network_connected(info, active);

	return 0;
}

static gboolean __changed_modem(DBusConnection *connection,
				DBusMessage *message, void *user_data)
{
	gboolean old_powered;
	DBusMessageIter args, dict;
	struct telephony_modem *modem;
	const char *path = dbus_message_get_path(message);

	DBG("modem changed signal %s", path);

	modem = g_hash_table_lookup(modem_hash, path);
	if (modem == NULL) {
		DBG("modem object does not exists");
		return TRUE;
	}

	old_powered = modem->powered;

	DBG("message signature (%s)", dbus_message_get_signature(message));

	if (dbus_message_iter_init(message, &args) == FALSE) {
		DBG("error to read message");
		return TRUE;
	}

	dbus_message_iter_recurse(&args, &dict);

	while (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_INVALID) {
		DBusMessageIter entry;
		const char *key, *tmp;

		dbus_message_iter_recurse(&dict, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_get_basic(&entry, &tmp);

		DBG("key(%s), value(%s)", key, tmp);

		if (g_strcmp0(key, "powered") == 0) {
			modem->powered = STRING2BOOL(tmp);
		} else if (g_strcmp0(key, "operator") == 0) {
			g_free(modem->operator);
			modem->operator = g_strdup(tmp);
		} else if (g_strcmp0(key, "sim_init") == 0) {
			modem->sim_init = STRING2BOOL(tmp);
		} else if (g_strcmp0(key, "flight_mode") == 0) {
			modem->flight_mode = STRING2BOOL(tmp);
		} else if (g_strcmp0(key, "roaming_allowed") == 0) {
			modem->roaming_allowed = STRING2BOOL(tmp);
		} else if (g_strcmp0(key, "data_allowed") == 0) {
			modem->data_allowed = STRING2BOOL(tmp);
		}

		dbus_message_iter_next(&dict);
	}

	if (modem->device == NULL)
		__add_connman_device(path, modem->operator);

	if (old_powered != modem->powered)
		__set_device_powered(modem, modem->powered);

	if (modem->powered != TRUE) {
		DBG("modem is not powered");
		return TRUE;
	}

	if (modem->s_service == NULL) {
		__request_get_services(modem->path);
		return TRUE;
	}

	DBG("modem(%s) flight mode(%d) data allowed(%d)",
			modem->path, modem->flight_mode, modem->data_allowed);

	return TRUE;
}

static gboolean __added_modem(DBusConnection *connection,
				DBusMessage *message, void *user_data)
{
	char *modem_path = NULL;
	DBusMessageIter args, dict, tmp;

	DBG("modem added signal (%s)", dbus_message_get_signature(message));

	if (dbus_message_iter_init(message, &args) == FALSE) {
		DBG("error to read message");
		return TRUE;
	}

	dbus_message_iter_recurse(&args, &dict);
	memcpy(&tmp, &dict, sizeof(struct DBusMessageIter));

	while (dbus_message_iter_get_arg_type(&tmp) != DBUS_TYPE_INVALID) {
		DBusMessageIter entry;
		const char *key, *value;

		dbus_message_iter_recurse(&tmp, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_get_basic(&entry, &value);

		DBG("key (%s) value(%s)", key, value);

		if (g_strcmp0(key, "path") == 0)
			modem_path = g_strdup(value);

		dbus_message_iter_next(&tmp);
	}

	if (modem_path != NULL)
		__add_modem(modem_path, &dict);

	g_free(modem_path);

	return TRUE;
}

static gboolean __removed_modem(DBusConnection *connection,
					DBusMessage *message, void *user_data)
{
	DBusMessageIter iter;
	const char *modem_path;

	DBG("modem removed signal");

	if (dbus_message_iter_init(message, &iter) == FALSE) {
		DBG("error to read message");
		return TRUE;
	}

	dbus_message_iter_get_basic(&iter, &modem_path);
	g_hash_table_remove(modem_hash, modem_path);

	return TRUE;
}

static gboolean __changed_service(DBusConnection *connection,
					DBusMessage *message, void *user_data)
{
	DBusMessageIter args, dict;
	struct telephony_modem *modem;
	gboolean roaming_option = TRUE;
	struct telephony_service *s_service;
	const char *service_path = dbus_message_get_path(message);

	DBG("service changed signal %s", service_path);

	s_service = g_hash_table_lookup(service_hash, service_path);
	if (s_service == NULL) {
		DBG("service object does not exists");
		return TRUE;
	}

	modem = s_service->p_modem;
	if (modem == NULL) {
		DBG("modem object does not exists");
		return TRUE;
	}

	DBG("message signature (%s)", dbus_message_get_signature(message));

	if (dbus_message_iter_init(message, &args) == FALSE) {
		DBG("error to read message");
		return TRUE;
	}

	dbus_message_iter_recurse(&args, &dict);

	while (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_INVALID) {
		DBusMessageIter entry;
		const char *key, *tmp;

		dbus_message_iter_recurse(&dict, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_get_basic(&entry, &tmp);

		DBG("key(%s), value(%s)", key, tmp);

		if (g_strcmp0(key, "roaming") == 0) {
			s_service->roaming = STRING2BOOL(tmp);
		} else if (g_strcmp0(key, "act") == 0) {
			s_service->act = g_strdup(tmp);
		} else if (g_strcmp0(key, "ps_attached") == 0) {
			s_service->ps_attached = STRING2BOOL(tmp);
		}

		dbus_message_iter_next(&dict);
	}

	roaming_option &= (!s_service->roaming && !modem->roaming_allowed)
				|| modem->roaming_allowed;

	return TRUE;
}

static gboolean __added_service(DBusConnection *connection,
				DBusMessage *message, void *user_data)
{
	struct telephony_modem *modem;
	const char *service_path = NULL;
	DBusMessageIter args, dict, tmp;
	const char *path = dbus_message_get_path(message);

	DBG("service added signal %s", path);

	modem = g_hash_table_lookup(modem_hash, path);
	if (modem == NULL || modem->device == NULL)
		return TRUE;

	DBG("message signature (%s)", dbus_message_get_signature(message));
	if (dbus_message_iter_init(message, &args) == FALSE) {
		DBG("error to read message");
		return TRUE;
	}

	dbus_message_iter_recurse(&args, &dict);
	memcpy(&tmp, &dict, sizeof(struct DBusMessageIter));

	while (dbus_message_iter_get_arg_type(&tmp) != DBUS_TYPE_INVALID) {
		DBusMessageIter entry;
		const char *key, *value;

		dbus_message_iter_recurse(&tmp, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_get_basic(&entry, &value);

		DBG("key (%s) value(%s)", key, value);

		if (g_strcmp0(key, "path") == 0)
			service_path = value;

		dbus_message_iter_next(&tmp);
	}

	if (service_path != NULL)
		__add_service(modem, service_path, &dict);

	return TRUE;
}

static gboolean __removed_service(DBusConnection *connection,
					DBusMessage *message, void *user_data)
{
	DBusMessageIter iter;
	const char *service_path;

	DBG("service removed signal");

	if (dbus_message_iter_init(message, &iter) == FALSE) {
		DBG("error to read message");
		return TRUE;
	}

	dbus_message_iter_get_basic(&iter, &service_path);
	g_hash_table_remove(service_hash, service_path);

	return TRUE;
}

static gboolean __changed_context(DBusConnection *connection,
					DBusMessage *message, void *user_data)
{
	connman_bool_t active = FALSE;
	DBusMessageIter args, dict;
	struct telephony_network *info;
	const char *path = dbus_message_get_path(message);
#if defined TIZEN_CONNMAN_VOWIFI
	gint epdg_pdp_type = 0;
	ims_network_info.epdg_signal = FALSE;
	ims_network_info.lte_signal = FALSE;
	if (g_strcmp0(path, EPDG_OBJECT_PATH) == 0) {
		if (!ims_network_info.epdg_network_path[epdg_default_subscription_id]) {
			DBG("epdg_network_path (nil)");
			return TRUE;
		}
		path = ims_network_info.epdg_network_path[epdg_default_subscription_id];
		ims_network_info.epdg_signal = TRUE;
		DBG("VoWiFi network changed signal %s", path);
	} else if (__connman_service_is_ims_profile(path)) {
		ims_network_info.lte_signal = TRUE;
		DBG("VoLTE network changed signal %s", path);
	} else
#endif
	DBG("network changed signal %s", path);

	info = g_hash_table_lookup(network_hash, path);
	if (info == NULL)
		return TRUE;

	if (__check_network_available(info->network) == FALSE) {
		g_hash_table_remove(network_hash, path);
		return TRUE;
	}

	if (dbus_message_iter_init(message, &args) == FALSE) {
		DBG("error to read message");
		return TRUE;
	}

	dbus_message_iter_recurse(&args, &dict);

	active = __set_network_context(info, &dict);
	if(info->routing_only){
		int err = 0;
		struct connman_service *routing_service;
		struct connman_ipconfig *routing_ipconfig;

		if(!active)
			return TRUE;

		routing_service = connman_service_lookup_from_network(info->network);
		routing_ipconfig = __connman_service_get_ip4config(routing_service);
		err = __connman_ipconfig_gateway_add(routing_ipconfig, routing_service);

		DBG("set gateway rv(%d)", err);
		return TRUE;
	}

#if defined TIZEN_CONNMAN_VOWIFI
	if (__connman_service_is_ims_profile(path)) {
		if (active)
			__set_network_connected(info, active);
		else {
			if (ims_network_info.activated_on_epdg) {
				if (ims_network_info.epdg_signal) {
					if (ims_network_info.activated_on_lte)
						DBG("LTE already activated, ignore epdg off");
					else if (ims_network_info.activating_on_lte)
						DBG("LTE activation was requested, ignore epdg off");
					else {
						DBG("no LTE activation or activation request, deactivate normally");
						__set_network_connected(info, active);
					}
				} else if (ims_network_info.lte_signal)
					DBG("ignore LTE off");
			} else if (ims_network_info.activated_on_lte) {
				if (ims_network_info.lte_signal) {
					if (ims_network_info.activating_on_epdg)
						DBG("EPDG activation was requested, ignore LTE off");
					else {
						DBG("no EPDG activation or activation request, deactivate normally");
						__set_network_connected(info, active);
					}
				} else if (ims_network_info.epdg_signal)
					DBG("Ignore EPDG off");
			} else
				DBG("no previous activation, ignore signal");
		}
	} else
		__set_network_connected(info, active);
#else
	__set_network_connected(info, active);
#endif
	if (active == FALSE &&
			connman_network_get_connecting(info->network) == TRUE)
		connman_network_set_connected(info->network, FALSE);

#if defined TIZEN_CONNMAN_VOWIFI
	if (__connman_service_is_ims_profile(path)) {
		if (ims_network_info.epdg_signal) {
			if (active && connman_network_get_connected(info->network))
				ims_network_info.activated_on_epdg = TRUE;
			else
				ims_network_info.activated_on_epdg = FALSE;

			ims_network_info.activating_on_epdg = FALSE;
		} else if (ims_network_info.lte_signal) {
			if (active && connman_network_get_connected(info->network)) {
				ims_network_info.activated_on_lte = TRUE;
				if ((connman_epdg_state == CONNMAN_EPDG_STATE_AVAILABLE) &&
						!ims_network_info.activated_on_epdg &&
						!ims_network_info.activating_on_epdg)
					__request_epdg_network_activate(epdg_pdp_type);
			} else
				ims_network_info.activated_on_lte = FALSE;

			ims_network_info.activating_on_lte = FALSE;
		}
		DBG("IMS PDN status, connected %d, activated_on_lte: %d, activated_on_epdg: %d",
				connman_network_get_connected(info->network),
				ims_network_info.activated_on_lte,
				ims_network_info.activated_on_epdg);
	}
#endif
	return TRUE;
}

static gboolean __added_context(DBusConnection *connection,
					DBusMessage *message, void *user_data)
{
	char *network_path = NULL;
	DBusMessageIter args, dict, tmp;
	struct telephony_modem *modem = NULL;
	struct telephony_service *service = NULL;
	const char *path = dbus_message_get_path(message);

	DBG("network added signal %s", path);

	service = g_hash_table_lookup(service_hash, path);
	if (service == NULL || service->p_modem == NULL)
		return TRUE;

	modem = service->p_modem;
	if (modem == NULL || modem->device == NULL)
		return TRUE;

	DBG("message signature (%s)", dbus_message_get_signature(message));
	if (dbus_message_iter_init(message, &args) == FALSE) {
		DBG("error to read message");
		return TRUE;
	}

	dbus_message_iter_recurse(&args, &dict);
	memcpy(&tmp, &dict, sizeof(struct DBusMessageIter));

	while (dbus_message_iter_get_arg_type(&tmp) != DBUS_TYPE_INVALID) {
		DBusMessageIter entry;
		const char *key, *value;

		dbus_message_iter_recurse(&tmp, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_get_basic(&entry, &value);

		DBG("key (%s) value(%s)", key, value);

		if (g_strcmp0(key, "path") == 0)
			network_path = g_strdup(value);

		dbus_message_iter_next(&tmp);
	}

	if (network_path != NULL)
		__add_context(modem->device, network_path, &dict);

	g_free(network_path);

	return TRUE;
}

static gboolean __removed_context(DBusConnection *connection,
					DBusMessage *message, void *user_data)
{
	DBusMessageIter iter;
	const char *network_path = NULL;
	struct telephony_service *service = NULL;
	const char *path = dbus_message_get_path(message);
#if defined TIZEN_CONNMAN_VOWIFI
	gint subscription_id = 0;
#endif

	DBG("network removed signal %s", path);

	service = g_hash_table_lookup(service_hash, path);
	if (service == NULL || service->p_modem == NULL)
		return TRUE;

	if (dbus_message_iter_init(message, &iter) == FALSE) {
		DBG("error to read message");
		return TRUE;
	}

	dbus_message_iter_get_basic(&iter, &network_path);
	g_hash_table_remove(network_hash, network_path);
#if defined TIZEN_CONNMAN_VOWIFI
	if (network_path && __connman_service_is_ims_profile(network_path)) {
		subscription_id = __connman_get_subscriber_id(network_path);
		g_free(ims_network_info.epdg_network_path[subscription_id]);
		ims_network_info.epdg_network_path[subscription_id] = NULL;
	}
#endif
	return TRUE;
}

static gboolean __changed_default_subscription(DBusConnection *connection,
		DBusMessage *message, void *user_data)
{
	DBusMessageIter args;

	DBG("message signature (%s)", dbus_message_get_signature(message));
	if (dbus_message_iter_init(message, &args) == FALSE)
		return TRUE;

	dbus_message_iter_get_basic(&args, &telephony_default_subscription_id);
	DBG("default subscription: %d", telephony_default_subscription_id);

	return TRUE;
}

#if defined TIZEN_CONNMAN_VOWIFI
static const char *__convert_epdg_state_to_string(ConnmanEpdgState_t EpdgState)
{
	const char *state;
	switch (EpdgState) {
	case CONNMAN_EPDG_STATE_UNAVAILABLE:
		state =  "CONNMAN_EPDG_STATE_UNAVAILABLE";
		break;
	case CONNMAN_EPDG_STATE_AVAILABLE:
		state =  "CONNMAN_EPDG_STATE_AVAILABLE";
		break;
	case CONNMAN_EPDG_STATE_NOT_CONNECTED:
		state = "CONNMAN_EPDG_STATE_NOT_CONNECTED";
		break;
	case CONNMAN_EPDG_STATE_CONNECTED:
		state = "CONNMAN_EPDG_STATE_CONNECTED";
		break;
	case CONNMAN_EPDG_STATE_HO_LTE_TO_IWLAN:
		state = "CONNMAN_EPDG_STATE_HO_LTE_TO_IWLAN";
		break;
	case CONNMAN_EPDG_STATE_HO_IWLAN_TO_LTE:
		state = "CONNMAN_EPDG_STATE_HO_IWLAN_TO_LTE";
		break;
	default:
		state = "Invalid EPDG State";
	}

	return state;
}

static gboolean __connman_service_is_ims_profile(const char *path)
{
	const char ims_suffix[] = "_0";

	if (g_str_has_suffix(path, ims_suffix) == TRUE) {
		DBG("Service path: %s", path);
		return TRUE;
	}

	return FALSE;
}

static int __connman_get_subscriber_id(const char *path) {
	gint subcription_id = 0;

	if (strstr(path, "modem0"))
		subcription_id = 0;
	else if (strstr(path, "modem1"))
		subcription_id = 1;

	return subcription_id;
}

static gboolean __changed_epdg_state(DBusConnection *connection,
					DBusMessage *message, void *user_data)
{
	DBusMessageIter iter;
	const char *path = dbus_message_get_path(message);
	ConnmanEpdgState_t EpdgState = CONNMAN_EPDG_STATE_UNAVAILABLE;
	struct telephony_network *info;
	gint epdg_subscription_id = 0;

	DBG("epdg state changed signal %s", path);

	if (dbus_message_iter_init(message, &iter) == FALSE) {
		DBG("error to read message");
		return TRUE;
	}

	dbus_message_iter_get_basic(&iter, &EpdgState);
	dbus_message_iter_next(&iter);
	dbus_message_iter_get_basic(&iter, &epdg_subscription_id);

	if (epdg_subscription_id < 0 || epdg_subscription_id > 1) {
		DBG("invalid epdg subscriber id");
		return TRUE;
	}

	epdg_default_subscription_id = epdg_subscription_id;
	switch (EpdgState) {
	case CONNMAN_EPDG_STATE_UNAVAILABLE:
		connman_epdg_state = CONNMAN_EPDG_STATE_UNAVAILABLE;

		if (!ims_network_info.epdg_network_path[epdg_default_subscription_id]) {
			DBG("epdg_network_path (nil)");
			break;
		}

		path = ims_network_info.epdg_network_path[epdg_default_subscription_id];
		info = g_hash_table_lookup(network_hash, path);
		if (info == NULL)
			break;

		if (!connman_network_get_connected(info->network)) {
			DBG("no network connection, ignore signal");
			break;
		}

		if (ims_network_info.activating_on_epdg) {
			if (!ims_network_info.activated_on_lte &&
					!ims_network_info.activated_on_epdg) {
				__set_network_connected(info, FALSE);
				ims_network_info.activating_on_epdg = FALSE;
			}
		}
		break;
	case CONNMAN_EPDG_STATE_AVAILABLE:
		connman_epdg_state = CONNMAN_EPDG_STATE_AVAILABLE;
		break;
	case CONNMAN_EPDG_STATE_NOT_CONNECTED:
		connman_epdg_state = CONNMAN_EPDG_STATE_NOT_CONNECTED;
		break;
	case CONNMAN_EPDG_STATE_CONNECTED:
		connman_epdg_state = CONNMAN_EPDG_STATE_CONNECTED;
		break;
	case CONNMAN_EPDG_STATE_HO_LTE_TO_IWLAN:
		connman_epdg_state = CONNMAN_EPDG_STATE_HO_LTE_TO_IWLAN;

		if (!ims_network_info.epdg_network_path[epdg_default_subscription_id]) {
			DBG("epdg_network_path (nil)");
			break;
		}

		path = ims_network_info.epdg_network_path[epdg_default_subscription_id];
		info = g_hash_table_lookup(network_hash, path);
		if (info == NULL)
			break;

		if (!connman_network_get_connected(info->network)) {
			DBG("no network connection, ignore signal");
			break;
		}

		if (ims_network_info.activated_on_lte) {
			__dbus_request(PS_DBUS_SERVICE, path, PS_CONTEXT_INTERFACE, DEACTIVATE_CONTEXT,
					NULL, NULL, NULL, DBUS_TYPE_INVALID);
		}
		break;
	case CONNMAN_EPDG_STATE_HO_IWLAN_TO_LTE:
		connman_epdg_state = CONNMAN_EPDG_STATE_HO_IWLAN_TO_LTE;
		break;
	default:
		connman_epdg_state = CONNMAN_EPDG_STATE_UNAVAILABLE;
		DBG("No EPDG State Info");
	}

	DBG("EPDG State: %s, EPDG SubId: %d", __convert_epdg_state_to_string(connman_epdg_state),
			epdg_default_subscription_id);

	return TRUE;
}

static gboolean __handover_context(DBusConnection *connection,
					DBusMessage *message, void *user_data)
{
	DBusMessageIter iter;
	const char *path = dbus_message_get_path(message);
	gint epdg_subscription_id = 0;
	gint epdg_pdp_type = 0;
	ConnmanEpdgHandoverDirection_t epdg_handover_direction = CONNMAN_EPDG_HANDOVER_UNKNOWN;
	struct telephony_network *info;
	int ret = 0;

	if (dbus_message_iter_init(message, &iter) == FALSE) {
		DBG("error to read message");
		return TRUE;
	}

	dbus_message_iter_get_basic(&iter, &epdg_subscription_id);
	dbus_message_iter_next(&iter);
	dbus_message_iter_get_basic(&iter, &epdg_pdp_type);
	dbus_message_iter_next(&iter);
	dbus_message_iter_get_basic(&iter, &epdg_handover_direction);

	DBG("handover signal %s, handover_direction: %d, pdp_type: %d, sub_id: %d",
			path, epdg_handover_direction, epdg_pdp_type, epdg_subscription_id);
	if (epdg_subscription_id < 0 || epdg_subscription_id > 1) {
		DBG("invalid subscription id %d", epdg_subscription_id);
		return TRUE;
	}

	epdg_default_subscription_id = epdg_subscription_id;
	if (!ims_network_info.epdg_network_path[epdg_subscription_id]) {
		DBG("epdg_network_path (nil)");
		return TRUE;
	}

	path = ims_network_info.epdg_network_path[epdg_subscription_id];
	info = g_hash_table_lookup(network_hash, path);
	if (info == NULL)
		return TRUE;

	if (!connman_network_get_connected(info->network)) {
		DBG("no network connection, ignore signal");
		return TRUE;
	}

	if (epdg_handover_direction == CONNMAN_EPDG_HANDOVER_LTE_TO_IWLAN) {
		if (ims_network_info.activated_on_epdg || ims_network_info.activating_on_epdg)
			return TRUE;

		__request_epdg_network_activate(epdg_pdp_type);
	} else if (epdg_handover_direction == CONNMAN_EPDG_HANDOVER_IWLAN_TO_LTE) {
		if (ims_network_info.activated_on_lte || ims_network_info.activating_on_lte)
			return TRUE;

		DBG("EPDG State %s network_path %s[%d], Activating IMS PDN over LTE.",
				__convert_epdg_state_to_string(connman_epdg_state),
				path, epdg_subscription_id);

		ret = __request_network_activate(info->network);
		if (ret != -EINPROGRESS)
			DBG("fail to activate IMS PDN over LTE, error %d", ret);
	} else
		DBG ("invalid handover direction");

	return TRUE;
}
#endif
/* telephony initialization */
static guint watch = 0;
static guint modem_watch = 0;
static guint modem_added_watch = 0;
static guint modem_removed_watch = 0;
static guint service_watch = 0;
static guint service_added_watch = 0;
static guint service_removed_watch = 0;
static guint context_watch = 0;
static guint context_added_watch = 0;
static guint context_removed_watch = 0;
static guint default_subscription_watch = 0;

#if defined TIZEN_CONNMAN_VOWIFI
/* epdg initialization */
static guint epdg_state_watch = 0;
static guint epdg_property_watch = 0;
static guint epdg_handover_watch = 0;
#endif

static int telephony_init(void)
{
	int err;

	DBG("telephony plugin");

	connection = connman_dbus_get_connection();
	if (connection == NULL)
		return -EIO;

	/* telephony watch */
	watch = g_dbus_add_service_watch(connection, PS_DBUS_SERVICE,
					telephony_connect, telephony_disconnect,
					NULL, NULL);

	modem_watch = g_dbus_add_signal_watch(connection, NULL, NULL,
						PS_MODEM_INTERFACE,
						PROPERTY_CHANGED,
						__changed_modem,
						NULL, NULL);

	modem_added_watch = g_dbus_add_signal_watch(connection, NULL, NULL,
						PS_MASTER_INTERFACE,
						MODEM_ADDED,
						__added_modem,
						NULL, NULL);

	modem_removed_watch = g_dbus_add_signal_watch(connection, NULL, NULL,
						PS_MASTER_INTERFACE,
						MODEM_REMOVED,
						__removed_modem,
						NULL, NULL);

	service_watch = g_dbus_add_signal_watch(connection, NULL, NULL,
						PS_SERVICE_INTERFACE,
						PROPERTY_CHANGED,
						__changed_service,
						NULL, NULL);

	service_added_watch = g_dbus_add_signal_watch(connection, NULL, NULL,
						PS_MODEM_INTERFACE,
						SERVICE_ADDED,
						__added_service,
						NULL, NULL);

	service_removed_watch = g_dbus_add_signal_watch(connection, NULL, NULL,
						PS_MODEM_INTERFACE,
						SERVICE_REMOVED,
						__removed_service,
						NULL, NULL);

	context_watch = g_dbus_add_signal_watch(connection, NULL, NULL,
						PS_CONTEXT_INTERFACE,
						PROPERTY_CHANGED,
						__changed_context,
						NULL, NULL);

	context_added_watch = g_dbus_add_signal_watch(connection, NULL, NULL,
						PS_SERVICE_INTERFACE,
						CONTEXT_ADDED,
						__added_context,
						NULL, NULL);

	context_removed_watch = g_dbus_add_signal_watch(connection, NULL, NULL,
						PS_SERVICE_INTERFACE,
						CONTEXT_REMOVED,
						__removed_context,
						NULL, NULL);

	default_subscription_watch = g_dbus_add_signal_watch(connection, NULL, NULL,
						"org.tizen.telephony.Network",
						"DefaultDataSubscription",
						__changed_default_subscription,
						NULL, NULL);
#if defined TIZEN_CONNMAN_VOWIFI
	/* epdg watch */
	epdg_state_watch = g_dbus_add_signal_watch(connection, NULL, NULL,
						EPDG_STATE_INTERFACE,
						STATE_INFO,
						__changed_epdg_state,
						NULL, NULL);

	epdg_property_watch = g_dbus_add_signal_watch(connection, NULL, NULL,
						EPDG_PDP_INTERFACE,
						PROPERTY_CHANGED,
						__changed_context,
						NULL, NULL);

	epdg_handover_watch = g_dbus_add_signal_watch(connection, NULL, NULL,
						EPDG_PDP_INTERFACE,
						HANDOVER,
						__handover_context,
						NULL, NULL);
#endif
	if (watch == 0 || modem_watch == 0 || modem_added_watch == 0
			|| modem_removed_watch == 0 || service_watch == 0
			|| service_added_watch == 0 || context_watch == 0
			|| service_removed_watch == 0
			|| context_added_watch == 0
			|| context_removed_watch == 0
#if defined TIZEN_CONNMAN_VOWIFI
			|| epdg_state_watch == 0 || epdg_property_watch == 0
			|| epdg_handover_watch == 0
#endif
			|| default_subscription_watch == 0) {
		err = -EIO;
		goto remove;
	}

	err = connman_network_driver_register(&network_driver);
	if (err < 0)
		goto remove;

	err = connman_device_driver_register(&modem_driver);
	if (err < 0) {
		connman_network_driver_unregister(&network_driver);
		goto remove;
	}

	err = connman_technology_driver_register(&tech_driver);
	if (err < 0) {
		connman_device_driver_unregister(&modem_driver);
		connman_network_driver_unregister(&network_driver);
		goto remove;
	}

	return 0;

remove:
	g_dbus_remove_watch(connection, watch);
	g_dbus_remove_watch(connection, modem_watch);
	g_dbus_remove_watch(connection, modem_added_watch);
	g_dbus_remove_watch(connection, modem_removed_watch);
	g_dbus_remove_watch(connection, service_watch);
	g_dbus_remove_watch(connection, service_added_watch);
	g_dbus_remove_watch(connection, service_removed_watch);
	g_dbus_remove_watch(connection, context_watch);
	g_dbus_remove_watch(connection, context_added_watch);
	g_dbus_remove_watch(connection, context_removed_watch);
	g_dbus_remove_watch(connection, default_subscription_watch);
#if defined TIZEN_CONNMAN_VOWIFI
	g_dbus_remove_watch(connection, epdg_state_watch);
	g_dbus_remove_watch(connection, epdg_property_watch);
	g_dbus_remove_watch(connection, epdg_handover_watch);
#endif

	dbus_connection_unref(connection);
	return err;
}

static void telephony_exit(void)
{
	g_dbus_remove_watch(connection, watch);
	g_dbus_remove_watch(connection, modem_watch);
	g_dbus_remove_watch(connection, modem_added_watch);
	g_dbus_remove_watch(connection, modem_removed_watch);
	g_dbus_remove_watch(connection, service_watch);
	g_dbus_remove_watch(connection, service_added_watch);
	g_dbus_remove_watch(connection, service_removed_watch);
	g_dbus_remove_watch(connection, context_watch);
	g_dbus_remove_watch(connection, context_added_watch);
	g_dbus_remove_watch(connection, context_removed_watch);
	g_dbus_remove_watch(connection, default_subscription_watch);
#if defined TIZEN_CONNMAN_VOWIFI
	g_dbus_remove_watch(connection, epdg_state_watch);
	g_dbus_remove_watch(connection, epdg_property_watch);
	g_dbus_remove_watch(connection, epdg_handover_watch);
#endif

	telephony_disconnect(connection, NULL);

	connman_device_driver_unregister(&modem_driver);
	connman_network_driver_unregister(&network_driver);

	dbus_connection_unref(connection);
}

CONNMAN_PLUGIN_DEFINE(telephony, "Samsung Telephony Framework plug-in", VERSION,
		CONNMAN_PLUGIN_PRIORITY_DEFAULT, telephony_init, telephony_exit)
