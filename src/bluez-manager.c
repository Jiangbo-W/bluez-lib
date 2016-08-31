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

#include <glib.h>
#include <gio/gio.h>

#include "bluez-manager.h"

struct bluez_manager {
	GDBusConnection *conn;
	GDBusObjectManager *object_manager;
	GCancellable *get_managed_objects_call;
};

static void parse_managed_objects(struct bluez_manager *manager)
{
}

static void object_added(GDBusObjectManager *manager, GDBusObject *object,
							gpointer user_data)
{
}

static void object_removed(GDBusObjectManager *manager, GDBusObject *object,
							gpointer user_data)
{
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

	return manager;
}

void bluez_manager_free(struct bluez_manager *manager)
{
	if (!manager)
		return;

	if (manager->get_managed_objects_call != NULL) {
		g_cancellable_cancel(manager->get_managed_objects_call);
		g_object_unref(manager->get_managed_objects_call);
	}

	if (manager->object_manager)
		g_object_unref(manager->object_manager);

	g_object_unref(manager->conn);

	g_free(manager);
}

void bluez_manager_refresh_objects(struct bluez_manager *manager)
{
	if (manager == NULL)
		return;

	get_managed_objects(manager);
}
