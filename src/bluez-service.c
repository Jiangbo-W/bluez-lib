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
#include "bluez-service.h"

struct bluez_service {
	GDBusProxy *service_proxy;
	GDBusProxy *properties_proxy;

	service_property_watch property_func;
	gpointer property_data;
};

void bluez_service_set_properties_watch(struct bluez_service *service,
				service_property_watch func, gpointer user_data)
{
	service->property_func = func;
	service->property_data = user_data;
}

BTResult bluez_service_connect(struct bluez_service *service)
{
	return proxy_method_call(service->service_proxy, "Connect", NULL);
}

BTResult bluez_service_disconnect(struct bluez_service *service)
{
	return proxy_method_call(service->service_proxy, "Disconnect", NULL);
}

gchar *bluez_service_get_device_path(struct bluez_service *service)
{
	return property_get_string(service->service_proxy, "Device");
}

gchar *bluez_service_get_state(struct bluez_service *service)
{
	return property_get_string(service->service_proxy, "State");
}

gchar *bluez_service_get_remote_uuid(struct bluez_service *service)
{
	return property_get_string(service->service_proxy, "RemoteUUID");
}

static void service_properties_changed(GDBusProxy *proxy,
						GVariant *changed_properties,
						GStrv *invalidated_properties,
						gpointer user_data)
{
	struct bluez_service *service = (struct bluez_service *) user_data;
	GVariantIter iter;
	gchar *key;
	GPtrArray *p;
	gchar **prop_names;

	p = g_ptr_array_new();

	g_variant_iter_init(&iter, changed_properties);
	while (g_variant_iter_next(&iter, "{sv}", &key, NULL))
		g_ptr_array_add(p, key);

	g_ptr_array_add(p, NULL);

	prop_names = (gchar **) g_ptr_array_free(p, FALSE);

	if (service->property_func)
		service->property_func(service, prop_names);

	g_strfreev(prop_names);
}

static void service_interface_added(GDBusObject *object,
				GDBusInterface *interface, gpointer user_data)
{
	struct bluez_service *service = (struct bluez_service *) user_data;
	GDBusProxy *proxy = G_DBUS_PROXY(interface);
	const gchar *name;

	name = g_dbus_proxy_get_interface_name(proxy);
	if (g_strcmp0(name, DEVICE_INTERFACE) == 0) {
		if (service->service_proxy)
			g_object_unref(service->service_proxy);

		service->service_proxy = g_object_ref(proxy);

		/* connect signal */
		g_signal_connect(service->service_proxy, "g-properties-changed",
				G_CALLBACK(service_properties_changed), service);
	} else if (g_strcmp0(name, PROPERTIES_INTERFACE) == 0) {
		if (service->properties_proxy)
			g_object_unref(service->properties_proxy);

		service->properties_proxy = g_object_ref(proxy);
	}
}

static void service_interface_removed(GDBusObject *object,
				GDBusInterface *interface, gpointer user_data)
{
	struct bluez_service *service = (struct bluez_service *) user_data;
	GDBusProxy *proxy = G_DBUS_PROXY(interface);
	const gchar *name;

	name = g_dbus_proxy_get_interface_name(proxy);
	if (g_strcmp0(name, ADAPTER_INTERFACE) == 0) {
		if (service->service_proxy) {
			g_object_unref(service->service_proxy);
			service->service_proxy = NULL;
		}
	} else if (g_strcmp0(name, PROPERTIES_INTERFACE) == 0) {
		if (service->properties_proxy) {
			g_object_unref(service->properties_proxy);
			service->properties_proxy = NULL;
		}
	}
}

struct bluez_service *bluez_service_new(GDBusObject *object)
{
	struct bluez_service *service;
	GDBusInterface *interface;
	GDBusProxy *proxy;

	service = g_try_new0(struct bluez_service, 1);
	if (!service)
		return NULL;

	interface = g_dbus_object_get_interface(object, SERVICE_INTERFACE);
	proxy = G_DBUS_PROXY(interface);
	service->service_proxy = proxy;

	interface = g_dbus_object_get_interface(object, PROPERTIES_INTERFACE);
	proxy = G_DBUS_PROXY(interface);
	service->properties_proxy = proxy;

	/* connect signal */
	g_signal_connect(service->service_proxy, "g-properties-changed",
			G_CALLBACK(service_properties_changed), service);

	g_signal_connect(object, "interface-added",
			G_CALLBACK(service_interface_added), service);
	g_signal_connect(object, "interface-removed",
			G_CALLBACK(service_interface_removed), service);

	return service;
}

void bluez_service_free(struct bluez_service *service)
{
	if (!service)
		return;

	if (service->service_proxy)
		g_object_unref(service->service_proxy);
	if (service->properties_proxy)
		g_object_unref(service->properties_proxy);

	g_free(service);
}
