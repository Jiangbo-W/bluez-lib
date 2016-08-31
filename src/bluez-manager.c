/*
 * BlueZ-Lib - C API library for BlueZ
 *
 * Copyright (c) 2013-2016 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *              http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>
#include <string.h>

#include "bluez-adapter.h"
#include "bluez-device.h"
#include "bluez-service.h"
#include "bluez-manager.h"

struct bluez_manager {
	GDBusConnection *conn;
	GDBusObjectManager *object_manager;
	GCancellable *get_managed_objects_call;

	GHashTable *adapters_hash;
	GHashTable *devices_hash;
	GHashTable *services_hash;

	GDBusProxy *agent_proxy;
	GDBusProxy *profile_proxy;

	guint agent_id;				/* registered agent ID */
	GDBusMethodInvocation *ivct;		/* Agent reply invocation */
	agent_request_cb agent_cb;		/* Agent request callback */
	void *agent_user_data;			/* Agent user data */

	bluez_adapter_added_cb adapter_added;
	bluez_adapter_removed_cb adapter_removed;
	gpointer adapter_user_data;

	bluez_device_added_cb device_added;
	bluez_device_removed_cb device_removed;
	gpointer device_user_data;

	bluez_service_added_cb service_added;
	bluez_service_removed_cb service_removed;
	gpointer service_user_data;
};

static GDBusNodeInfo *node_info;

static const gchar introspection_xml[] =
	"<node>"
	"  <interface name='org.bluez.Agent1'>"
	"    <method name='Release'>"
	"    </method>"
	"    <method name='RequestPinCode'>"
	"      <arg type='o' name='device' direction='in'/>"
	"      <arg type='s' direction='out'/>"
	"    </method>"
	"    <method name='DisplayPinCode'>"
	"      <arg type='o' name='device' direction='in'/>"
	"      <arg type='s' name='pincode' direction='in'/>"
	"    </method>"
	"    <method name='RequestPasskey'>"
	"      <arg type='o' name='device' direction='in'/>"
	"      <arg type='u' direction='out'/>"
	"    </method>"
	"    <method name='DisplayPasskey'>"
	"      <arg type='o' name='device' direction='in'/>"
	"      <arg type='u' name='passkey' direction='in'/>"
	"      <arg type='q' name='entered' direction='in'/>"
	"    </method>"
	"    <method name='RequestConfirmation'>"
	"      <arg type='o' name='device' direction='in'/>"
	"      <arg type='u' name='passkey' direction='in'/>"
	"    </method>"
	"    <method name='RequestAuthorization'>"
	"      <arg type='o' name='device' direction='in'/>"
	"    </method>"
	"    <method name='AuthorizeService'>"
	"      <arg type='o' name='device' direction='in'/>"
	"      <arg type='s' name='uuid' direction='in'/>"
	"    </method>"
	"    <method name='Cancel'>"
	"    </method>"
	"  </interface>"
	"</node>";

static void passkey_reply(struct bluez_manager *manager,
						int accept, uint8_t *code)
{
	guint32 *passkey = (guint32 *) code;
	GVariant *value;

	if (accept == FALSE) {
		g_dbus_method_invocation_return_dbus_error(manager->ivct,
				BLUEZ_SERVICE_NAME "Error.Rejected", "Rejected");

		return;
	}

	value = g_variant_new("(u)", *passkey);

	g_dbus_method_invocation_return_value(manager->ivct, value);
}

static void pincode_reply(struct bluez_manager *manager,
						int accept, uint8_t *code)
{
	char *pincode = (char *) code;
	GVariant *value;

	if (accept == FALSE) {
		g_dbus_method_invocation_return_dbus_error(manager->ivct,
				BLUEZ_SERVICE_NAME "Error.Rejected", "Rejected");

		return;
	}

	value = g_variant_new("(s)", pincode);

	g_dbus_method_invocation_return_value(manager->ivct, value);
}

static void display_reply(struct bluez_manager *manager,
						int accept, uint8_t *code)
{
	/* Ignore paremters, reply directly */
	g_dbus_method_invocation_return_value(manager->ivct, NULL);
}

static void confirm_reply(struct bluez_manager *manager,
						int accept, uint8_t *code)
{
	if (accept == FALSE) {
		g_dbus_method_invocation_return_dbus_error(manager->ivct,
				BLUEZ_SERVICE_NAME "Error.Rejected", "Rejected");

		return;
	}

	g_dbus_method_invocation_return_value(manager->ivct, NULL);
}

static void release_reply(struct bluez_manager *manager)
{
	if (manager->ivct == NULL)
		return;

	g_object_unref(manager->ivct);
	manager->ivct = NULL;
}

BTResult bluez_manager_agent_reply(struct bluez_manager *manager,
						int accept, uint8_t *code)
{
	const gchar *method;

	if (manager == NULL)
		return BT_RESULT_INVALID_ARGS;

	if (manager->ivct == NULL)
		return BT_RESULT_NOT_EXIST;

	method = g_dbus_method_invocation_get_method_name(manager->ivct);
	if (!g_strcmp0(method, "DisplayPinCode"))
		display_reply(manager, accept, code);
	else if (!g_strcmp0(method, "RequestPinCode"))
		pincode_reply(manager, accept, code);
	else if (!g_strcmp0(method, "DisplayPasskey"))
		display_reply(manager, accept, code);
	else if (!g_strcmp0(method, "RequestPasskey"))
		passkey_reply(manager, accept, code);
	else if (!g_strcmp0(method, "RequestConfirmation"))
		confirm_reply(manager, accept, code);
	else if (!g_strcmp0(method, "RequestAuthorization"))
		confirm_reply(manager, accept, code);
	else if (!g_strcmp0(method, "AuthorizeService"))
		confirm_reply(manager, accept, code);
	else if (!g_strcmp0(method, "Cancel"))
		display_reply(manager, accept, code);
	else if (!g_strcmp0(method, "Release"))
		release_reply(manager);

	return BT_RESULT_OK;
}

static void handle_agent_method(GDBusConnection *conn, const gchar *sender,
				const gchar *path, const gchar *interface,
				const char *method, GVariant *value,
				GDBusMethodInvocation *ivct, gpointer data)
{
	struct bluez_manager *manager = (struct bluez_manager *) data;
	gchar *device_path = NULL, *pincode = NULL, *uuid = NULL;
	enum agent_request_type type;
	void *request_data;
	guint32 passkey;
	guint16 entered;
	gchar *address;

	if (manager->agent_cb == NULL) {
		printf("Failed to auth request. No agent request callback\n");

		/* TODO: Reply error */
		g_dbus_method_invocation_return_value(ivct, NULL);

		return;
	}

	manager->ivct = ivct;

	request_data = NULL;
	printf("agent method: %s\n", method);
	if (g_strcmp0(method, "Release") == 0) {
		type = AGENT_REQUEST_RELEASE;
	} else if (g_strcmp0(method, "DisplayPinCode") == 0) {
		g_variant_get(value, "(os)", &device_path, &pincode);

		type = AGENT_REQUEST_DISPLAY_PINCODE;
		request_data = pincode;
	} else if (g_strcmp0(method, "RequestPinCode") == 0) {
		g_variant_get(value, "(o)", &device_path);

		type = AGENT_REQUEST_PINCODE;
	} else if (g_strcmp0(method, "DisplayPasskey") == 0) {
		g_variant_get(value, "(ouq)", &device_path,
						&passkey, &entered);

		type = AGENT_REQUEST_DISPLAY_PASSKEY;
	} else if (g_strcmp0(method, "RequestPasskey") == 0) {
		g_variant_get(value, "(o)", &device_path);

		type = AGENT_REQUEST_PASSKEY;
	} else if (g_strcmp0(method, "RequestConfirmation") == 0) {
		g_variant_get(value, "(ou)", &device_path, &passkey);

		type = AGENT_REQUEST_CONFIRMATION;
		request_data = &passkey;
	} else if (g_strcmp0(method, "RequestAuthorization") == 0) {
		g_variant_get(value, "(o)", &device_path);

		type = AGENT_REQUEST_AUTHORIZATION;
	} else if (g_strcmp0(method, "AuthorizeService") == 0) {
		g_variant_get(value, "(os)", &device_path, &uuid);

		type = AGENT_REQUEST_AUTHORIZESERVICE;
	} else if (g_strcmp0(method, "Cancel") == 0) {
		type = AGENT_REQUEST_CANCEL;
	} else {
		type = AGENT_REQUEST_CANCEL;
		printf("Agent Method: %s\n", method);
	}

	if (type != AGENT_REQUEST_CANCEL && type != AGENT_REQUEST_RELEASE) {
		address = g_strdup(g_strrstr(device_path, "dev_") + strlen("dev_"));
		g_strdelimit(address, "_", ':');
	} else
		address = NULL;

	manager->agent_cb(type, address, request_data,
					manager->agent_user_data);

	g_free(address);
	g_free(device_path);

	if (pincode)
		g_free(pincode);

	if (uuid)
		g_free(uuid);
}

static BTResult register_agent(struct bluez_manager *manager, const gchar *path)
{
	GVariant *parameter;

	if (manager->agent_proxy == NULL)
		return BT_RESULT_NOT_READY;

	parameter = g_variant_new("(os)", path, "DisplayYesNo");

	return proxy_method_call(manager->agent_proxy,
					"RegisterAgent", parameter);
}

static BTResult set_default_agent(struct bluez_manager *manager,
							const gchar *path)
{
	GVariant *parameter;

	if (manager->agent_proxy == NULL)
		return BT_RESULT_NOT_READY;

	parameter = g_variant_new("(o)", path);

	return proxy_method_call(manager->agent_proxy,
					"RequestDefaultAgent", parameter);
}

static const GDBusInterfaceVTable interface_handle = {
	handle_agent_method,
	NULL,
	NULL
};

gboolean bluez_manager_register_agent(struct bluez_manager *manager,
					agent_request_cb cb, void *user_data)
{
	guint agent_id;

	node_info = g_dbus_node_info_new_for_xml(introspection_xml, NULL);

	agent_id = g_dbus_connection_register_object(manager->conn, AGENT_PATH,
				node_info->interfaces[0], &interface_handle,
				manager, NULL, NULL);

	manager->agent_id = agent_id;
	manager->agent_cb = cb;
	manager->agent_user_data = user_data;

	g_dbus_node_info_unref(node_info);

	/* Register agent to BlueZ */
	if (register_agent(manager, AGENT_PATH) != BT_RESULT_OK) {
		printf("BlueZ Agent is not available, auto register later.\n");

		return TRUE;
	}

	set_default_agent(manager, AGENT_PATH);

	printf("BlueZ Agent register success\n");

	return TRUE;
}

struct bluez_device *find_device_by_address(struct bluez_manager *manager,
							const gchar *address)
{
	GHashTableIter iter;
	gpointer key, device;
	gchar *path;

	path = g_strdup(address);
	g_strdelimit(path, ":", '_');

	g_hash_table_iter_init(&iter, manager->devices_hash);
	while (g_hash_table_iter_next(&iter, &key, &device)) {
		if (g_strrstr(key, path)) {
			g_free(path);
			return device;
		}
	}

	g_free(path);

	return NULL;
}

static gboolean add_bluez_adapter(struct bluez_manager* manager,
						GDBusObject *object)
{
	struct bluez_adapter *adapter;
	const gchar *object_path;

	object_path = g_dbus_object_get_object_path(object);
	adapter = g_hash_table_lookup(manager->adapters_hash, object_path);
	if (adapter) {
		printf("adapter already exist in adapter HashTable.\n");
		return FALSE;
	}

	adapter = bluez_adapter_new(object);
	if (!adapter)
		return FALSE;

	g_hash_table_replace(manager->adapters_hash,
				g_strdup(object_path), adapter);

	if (manager->adapter_added)
		manager->adapter_added(adapter, manager->adapter_user_data);

	return TRUE;
}

static gboolean remove_bluez_adapter(struct bluez_manager *manager,
						GDBusObject *object)
{
	struct bluez_adapter *adapter;
	const gchar *object_path;

	object_path = g_dbus_object_get_object_path(object);
	adapter = g_hash_table_lookup(manager->adapters_hash, object_path);
	if (!adapter) {
		printf("adapter is not exist in adapter HashTable\n");
		return FALSE;
	}

	if (manager->adapter_removed)
		manager->adapter_removed(adapter, manager->adapter_user_data);

	g_hash_table_remove(manager->adapters_hash, object_path);

	return TRUE;
}

static gboolean add_bluez_device(struct bluez_manager *manager,
						GDBusObject *object)
{
	struct bluez_device *device;
	const gchar *object_path;

	object_path = g_dbus_object_get_object_path(object);
	device = g_hash_table_lookup(manager->devices_hash, object_path);
	if (device) {
		printf("device already exist in device HashTable.\n");
		return FALSE;
	}

	device = bluez_device_new(object);
	if (!device)
		return FALSE;

	g_hash_table_replace(manager->devices_hash,
				g_strdup(object_path), device);

	if (manager->device_added)
		manager->device_added(device, manager->device_user_data);

	return TRUE;
}

static gboolean remove_bluez_device(struct bluez_manager *manager,
						GDBusObject *object)
{
	struct bluez_device *device;
	const gchar *object_path;

	object_path = g_dbus_object_get_object_path(object);
	device = g_hash_table_lookup(manager->devices_hash, object_path);
	if (!device) {
		printf("device is not exist in device HashTable.\n");
		return FALSE;
	}

	if (manager->device_removed)
		manager->device_removed(device, manager->device_user_data);

	g_hash_table_remove(manager->devices_hash, object_path);

	return TRUE;
}

static gboolean add_bluez_service(struct bluez_manager *manager,
						GDBusObject *object)
{
	struct bluez_service *service;
	const gchar *object_path;

	object_path = g_dbus_object_get_object_path(object);
	service = g_hash_table_lookup(manager->services_hash, object_path);
	if (service) {
		printf("service already exist in service HashTable.\n");
		return FALSE;
	}

	service = bluez_service_new(object);
	if (!service)
		return FALSE;

	g_hash_table_replace(manager->services_hash,
					g_strdup(object_path), service);

	if (manager->service_added)
		manager->service_added(service, manager->service_user_data);

	return TRUE;
}

static gboolean remove_bluez_service(struct bluez_manager *manager,
						GDBusObject *object)
{
	struct bluez_service *service;
	const gchar *object_path;

	object_path = g_dbus_object_get_object_path(object);
	service = g_hash_table_lookup(manager->services_hash, object_path);
	if (!service) {
		printf("service is not exist in service HashTable.\n");
		return FALSE;
	}

	if (manager->service_removed)
		manager->service_removed(service, manager->service_user_data);

	g_hash_table_remove(manager->services_hash, object_path);

	return TRUE;
}

static void parse_bluez_root(struct bluez_manager *manager,
						GDBusObject *object)
{
	GDBusInterface *interface;
	GDBusProxy *proxy;
	BTResult ret;

	/* org.bluez.AgentManager1 */
	interface = g_dbus_object_get_interface(object, AGENT_INTERFACE);
	if (interface != NULL) {
		proxy = G_DBUS_PROXY(interface);
		manager->agent_proxy = proxy;

		if (manager->agent_id) {
			ret = register_agent(manager, AGENT_PATH);
			if (ret == BT_RESULT_OK) {
				set_default_agent(manager, AGENT_PATH);

				printf("BlueZ Agent register success\n");
			} else
				printf("Failed to register agent\n");
		}
	}

	/* org.bluez.ProfileManager1 */
	interface = g_dbus_object_get_interface(object, PROFILE_INTERFACE);
	if (interface != NULL) {
		proxy = G_DBUS_PROXY(interface);
		manager->profile_proxy = proxy;
	}
}

static gboolean remove_bluez_root(struct bluez_manager *manager,
						GDBusObject *object)
{
	if (manager->agent_proxy)
		g_object_unref(manager->agent_proxy);

	if (manager->profile_proxy)
		g_object_unref(manager->profile_proxy);

	manager->agent_proxy = NULL;
	manager->profile_proxy = NULL;

	return TRUE;
}

static gboolean foreach_adapter_removed(gpointer key, gpointer value,
							gpointer user_data)
{
	struct bluez_manager *manager = (struct bluez_manager *) user_data;

	if (manager->adapter_removed)
		manager->adapter_removed(value, manager->adapter_user_data);

	return TRUE;
}

static gboolean foreach_device_removed(gpointer key, gpointer value,
							gpointer user_data)
{
	struct bluez_manager *manager = (struct bluez_manager *) user_data;

	if (manager->device_removed)
		manager->device_removed(value, manager->adapter_user_data);

	return TRUE;
}

static gboolean foreach_service_removed(gpointer key, gpointer value,
							gpointer user_data)
{
	struct bluez_manager *manager = (struct bluez_manager *) user_data;

	if (manager->service_removed)
		manager->service_removed(value, manager->service_user_data);

	return TRUE;
}

static gboolean bluez_object_has_interface(GDBusObject *object,
						const gchar *interface_name)
{
	GDBusInterface *interface;

	interface = g_dbus_object_get_interface(object, interface_name);

	if (interface == NULL)
		return FALSE;

	g_object_unref(interface);

	return TRUE;
}

static void parse_bluez_object(struct bluez_manager *manager,
					GDBusObject *object)
{
	if (bluez_object_has_interface(object, ADAPTER_INTERFACE)) {
		add_bluez_adapter(manager, object);
		return;
	}

	if (bluez_object_has_interface(object, DEVICE_INTERFACE)) {
		add_bluez_device(manager, object);
		return;
	}

	if (bluez_object_has_interface(object, SERVICE_INTERFACE)) {
		add_bluez_service(manager, object);
		return;
	}

	if (bluez_object_has_interface(object, AGENT_INTERFACE)) {
		parse_bluez_root(manager, object);
		return;
	}
}

static void object_added(GDBusObjectManager *manager, GDBusObject *object,
							gpointer user_data)
{
	parse_bluez_object((struct bluez_manager *)user_data, object);
}

static void object_removed(GDBusObjectManager *manager, GDBusObject *object,
							gpointer user_data)
{
	struct bluez_manager *bluez_manager = (struct bluez_manager *)user_data;

	if (bluez_object_has_interface(object, ADAPTER_INTERFACE)) {
		remove_bluez_adapter(bluez_manager, object);
		return;
	}

	if (bluez_object_has_interface(object, DEVICE_INTERFACE)) {
		remove_bluez_device(bluez_manager, object);
		return;
	}

	if (bluez_object_has_interface(object, SERVICE_INTERFACE)) {
		remove_bluez_service(bluez_manager, object);
		return;
	}

	if (bluez_object_has_interface(object, AGENT_INTERFACE)) {
		remove_bluez_root(bluez_manager, object);
		return;
	}
}

static gint object_sort(GDBusObject *a, GDBusObject *b)
{
	const gchar *path_a, *path_b;

	path_a = g_dbus_object_get_object_path(a);
	path_b = g_dbus_object_get_object_path(b);

	return g_strcmp0(path_a, path_b);
}

static void parse_managed_objects(struct bluez_manager *manager)
{
	GList *objects, *list, *next;
	GDBusObject *object;

	objects = g_dbus_object_manager_get_objects(manager->object_manager);
	objects = g_list_sort(objects, (GCompareFunc) object_sort);

	for  (list = objects; list; list = next) {
		next = g_list_next(list);

		object = list->data;

		parse_bluez_object(manager,object);

		g_object_unref(object);
	}

	g_list_free(objects);
}

static void get_managed_objects_reply(GObject *object, GAsyncResult *res,
							gpointer user_data)
{
	struct bluez_manager *bluez_manager = (struct bluez_manager *)user_data;
	GDBusObjectManager *manager;
	GError *error = NULL;

	manager = g_dbus_object_manager_client_new_for_bus_finish(res, &error);
	if (manager == NULL) {
		g_error_free(error);
		goto done;
	}

	g_signal_connect(manager, "object-added",
				G_CALLBACK(object_added), bluez_manager);
	g_signal_connect(manager, "object-removed",
				G_CALLBACK(object_removed), bluez_manager);

	bluez_manager->object_manager = manager;

	parse_managed_objects(bluez_manager);

done:
	g_object_unref(bluez_manager->get_managed_objects_call);
	bluez_manager->get_managed_objects_call = NULL;
}

static void get_managed_objects(struct bluez_manager *manager)
{
	if (manager->object_manager)
		return parse_managed_objects(manager);

	if (manager->get_managed_objects_call)
		return;

	manager->get_managed_objects_call = g_cancellable_new();

	g_dbus_object_manager_client_new(manager->conn, 0,
					BLUEZ_SERVICE_NAME, BLUEZ_MANAGER_PATH,
					NULL, NULL, NULL,
					manager->get_managed_objects_call,
					get_managed_objects_reply, manager);
}

struct bluez_manager *bluez_manager_new(void)
{
	struct bluez_manager *manager;

	manager = g_try_new0(struct bluez_manager, 1);
	if (!manager)
		return NULL;

	manager->conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);

	manager->adapters_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
					g_free,
					(GDestroyNotify) bluez_adapter_free);
	manager->devices_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
					g_free,
					(GDestroyNotify) bluez_device_free);
	manager->services_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
					g_free,
					(GDestroyNotify) bluez_service_free);

	return manager;
}

void bluez_manager_free(struct bluez_manager *manager)
{
	if (!manager)
		return;

	if (manager->services_hash) {
		g_hash_table_foreach_remove(manager->services_hash,
					foreach_service_removed, manager);
		g_hash_table_unref(manager->services_hash);
	}

	if (manager->devices_hash) {
		g_hash_table_foreach_remove(manager->devices_hash,
					foreach_device_removed, manager);
		g_hash_table_unref(manager->devices_hash);
	}

	if (manager->adapters_hash) {
		g_hash_table_foreach_remove(manager->adapters_hash,
					foreach_adapter_removed, manager);
		g_hash_table_unref(manager->adapters_hash);
	}

	if (manager->get_managed_objects_call != NULL) {
		g_cancellable_cancel(manager->get_managed_objects_call);
		g_object_unref(manager->get_managed_objects_call);
	}

	if (manager->agent_id)
		g_dbus_connection_unregister_object(manager->conn,
							manager->agent_id);

	if (manager->agent_proxy)
		g_object_unref(manager->agent_proxy);

	if (manager->profile_proxy)
		g_object_unref(manager->profile_proxy);

	if (manager->object_manager)
		g_object_unref(manager->object_manager);

	g_object_unref(manager->conn);

	g_free(manager);
}

gboolean bluez_manager_set_adapter_watch(struct bluez_manager *manager,
				bluez_adapter_added_cb adapter_added,
				bluez_adapter_removed_cb adapter_removed,
				gpointer user_data)
{
	if (manager == NULL)
		return FALSE;

	manager->adapter_added = adapter_added;
	manager->adapter_removed = adapter_removed;
	manager->adapter_user_data = user_data;

	return TRUE;
}

gboolean bluez_manager_set_device_watch(struct bluez_manager *manager,
				bluez_device_added_cb device_added,
				bluez_device_removed_cb device_removed,
				gpointer user_data)
{
	if (manager == NULL)
		return FALSE;

	manager->device_added = device_added;
	manager->device_removed = device_removed;
	manager->device_user_data = user_data;

	return TRUE;
}

gboolean bluez_manager_set_service_watch(struct bluez_manager *manager,
				bluez_service_added_cb service_added,
				bluez_service_removed_cb service_removed,
				gpointer user_data)
{
	if (manager == NULL)
		return FALSE;

	manager->service_added = service_added;
	manager->service_removed = service_removed;
	manager->service_user_data = user_data;

	return TRUE;
}

void bluez_manager_refresh_objects(struct bluez_manager *manager)
{
	if (manager == NULL)
		return;

	get_managed_objects(manager);
}
