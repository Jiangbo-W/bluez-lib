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
#include <stdio.h>

#include "bluez-device.h"

struct bluez_device {
	GDBusProxy *device_proxy;
	GDBusProxy *properties_proxy;

	device_property_watch property_func;
	gpointer property_data;
};

void bluez_device_set_properties_watch(struct bluez_device *device,
				device_property_watch func, gpointer user_data)
{
	device->property_func = func;
	device->property_data = user_data;
}

BTResult bluez_device_connect(struct bluez_device *device)
{
	return proxy_method_call(device->device_proxy, "Connect", NULL);
}

BTResult bluez_device_disconnect(struct bluez_device *device)
{
	return proxy_method_call(device->device_proxy, "Disconnect", NULL);
}

BTResult bluez_device_connect_profile(struct bluez_device *device,
							const gchar *uuid)
{
	GVariant *parameter;

	parameter = g_variant_new("(s)", uuid);

	return proxy_method_call(device->device_proxy,
					"ConnectProfile", parameter);
}

BTResult bluez_device_disconnect_profile(struct bluez_device *device,
							const gchar *uuid)
{
	GVariant *parameter;

	parameter = g_variant_new("(s)", uuid);

	return proxy_method_call(device->device_proxy,
					"DisconnectProfile", parameter);
}

void bluez_device_pair_with_reply(struct bluez_device *device,
					bluez_response_cb cb, void *user_data)
{
	return proxy_method_call_with_reply(device->device_proxy,
						"Pair", NULL, cb, user_data);
}

BTResult bluez_device_cancel_pair(struct bluez_device *device)
{
	return proxy_method_call(device->device_proxy, "CancelPairing", NULL);
}

BTResult bluez_device_unpair(struct bluez_device *device)
{
	return proxy_method_call(device->device_proxy, "UnPair", NULL);
}

gchar **bluez_device_get_property_names(struct bluez_device *device)
{
	return g_dbus_proxy_get_cached_property_names(device->device_proxy);
}

gchar *bluez_device_get_name(struct bluez_device *device)
{
	return property_get_string(device->device_proxy, "Name");
}

BTResult bluez_device_set_alias(struct bluez_device *device, char *alias)
{
	GVariant *value, *parameter;

	value = g_variant_new("s", alias);
	parameter = g_variant_new("(ssv)", DEVICE_INTERFACE, "Alias", value);

	return property_set_variant(device->properties_proxy, parameter);
}

gchar *bluez_device_get_alias(struct bluez_device *device)
{
	return property_get_string(device->device_proxy, "Alias");
}

gchar *bluez_device_get_address(struct bluez_device *device)
{
	return property_get_string(device->device_proxy, "Address");
}

void bluez_device_get_class(struct bluez_device *device, guint32 *class)
{
	property_get_uint32(device->device_proxy, "Class", class);
}

void bluez_device_get_paired(struct bluez_device *device, gboolean *paired)
{
	property_get_boolean(device->device_proxy, "Paired", paired);
}

void bluez_device_get_connected(struct bluez_device *device,
							gboolean *connected)
{
	property_get_boolean(device->device_proxy, "Connected", connected);
}

void bluez_device_get_rssi(struct bluez_device *device, gint16 *rssi)
{
	property_get_int16(device->device_proxy, "RSSI", rssi);
}

gchar **bluez_device_get_uuids(struct bluez_device *device)
{
	return property_get_strings(device->device_proxy, "UUIDs");
}

const gchar *bluez_device_get_path(struct bluez_device *device)
{
	return g_dbus_proxy_get_object_path(device->device_proxy);
}

static void device_properties_changed(GDBusProxy *proxy,
					GVariant *changed_properties,
					GStrv *invalidated_properties,
					gpointer user_data)
{
	struct bluez_device *device = (struct bluez_device *) user_data;
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

	if (device->property_func)
		device->property_func(device, prop_names);

	g_strfreev(prop_names);
}

struct bluez_device *bluez_device_new(GDBusObject *object)
{
	struct bluez_device *device;
	GDBusInterface *interface;
	GDBusProxy *proxy;

	device = g_try_new0(struct bluez_device, 1);
	if (!device)
		return NULL;

	interface = g_dbus_object_get_interface(object, DEVICE_INTERFACE);
	proxy = G_DBUS_PROXY(interface);
	device->device_proxy = proxy;

	interface = g_dbus_object_get_interface(object, PROPERTIES_INTERFACE);
	proxy = G_DBUS_PROXY(interface);
	device->properties_proxy = proxy;

	/* connect signal */
	g_signal_connect(device->device_proxy, "g-properties-changed",
			G_CALLBACK(device_properties_changed), device);

	return device;
}

void bluez_device_free(struct bluez_device *device)
{
	if (!device)
		return;

	g_object_unref(device->device_proxy);
	g_object_unref(device->properties_proxy);

	g_free(device);
}
