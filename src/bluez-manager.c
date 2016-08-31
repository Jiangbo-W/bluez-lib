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

#include "bluez-adapter.h"
#include "bluez-manager.h"

struct bluez_manager {
	GDBusConnection *conn;
	GDBusObjectManager *object_manager;
	GCancellable *get_managed_objects_call;

	GHashTable *adapters_hash;

	bluez_adapter_added_cb adapter_added;
	bluez_adapter_removed_cb adapter_removed;
	gpointer adapter_user_data;
};

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

static gboolean foreach_adapter_removed(gpointer key, gpointer value,
							gpointer user_data)
{
	struct bluez_manager *manager = (struct bluez_manager *) user_data;

	if (manager->adapter_removed)
		manager->adapter_removed(value, manager->adapter_user_data);

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

	return manager;
}

void bluez_manager_free(struct bluez_manager *manager)
{
	if (!manager)
		return;

	if (manager->adapters_hash) {
		g_hash_table_foreach_remove(manager->adapters_hash,
					foreach_adapter_removed, manager);
		g_hash_table_unref(manager->adapters_hash);
	}

	if (manager->get_managed_objects_call != NULL) {
		g_cancellable_cancel(manager->get_managed_objects_call);
		g_object_unref(manager->get_managed_objects_call);
	}

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

void bluez_manager_refresh_objects(struct bluez_manager *manager)
{
	if (manager == NULL)
		return;

	get_managed_objects(manager);
}
