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

#include "bluez-common.h"
#include "bluez-adapter.h"

struct bluez_adapter {
	GDBusProxy *adapter_proxy;
	GDBusProxy *properties_proxy;

	adapter_property_watch property_func;
	gpointer property_data;
};

void bluez_adapter_set_properties_watch(struct bluez_adapter *adapter,
				adapter_property_watch func, gpointer user_data)
{
	adapter->property_func = func;
	adapter->property_data = user_data;
}

BTResult bluez_adapter_start_discovery(struct bluez_adapter *adapter)
{
	return proxy_method_call(adapter->adapter_proxy, "StartDiscovery", NULL);
}

BTResult bluez_adapter_stop_discovery(struct bluez_adapter *adapter)
{
	return proxy_method_call(adapter->adapter_proxy, "StopDiscovery", NULL);
}

gchar **bluez_adapter_get_property_names(struct bluez_adapter *adapter)
{
	return g_dbus_proxy_get_cached_property_names(adapter->adapter_proxy);
}

BTResult bluez_adapter_set_powered(struct bluez_adapter *adapter, gboolean powered)
{
	GVariant *value = g_variant_new("b", powered);
	GVariant *parameter = g_variant_new("(ssv)", ADAPTER_INTERFACE,
							"Powered", value);

	return property_set_variant(adapter->properties_proxy, parameter);
}

void bluez_adapter_get_powered(struct bluez_adapter *adapter, gboolean *powered)
{
	property_get_boolean(adapter->adapter_proxy, "Powered", powered);
}

BTResult bluez_adapter_set_alias(struct bluez_adapter *adapter, char *alias)
{
	GVariant *value, *parameter;

	value = g_variant_new("s", alias);
	parameter = g_variant_new("(ssv)", ADAPTER_INTERFACE, "Alias", value);

	return property_set_variant(adapter->properties_proxy, parameter);
}

gchar *bluez_adapter_get_alias(struct bluez_adapter *adapter)
{
	return property_get_string(adapter->adapter_proxy, "Alias");
}

gchar *bluez_adapter_get_address(struct bluez_adapter *adapter)
{
	return property_get_string(adapter->adapter_proxy, "Address");
}

void bluez_adapter_get_class(struct bluez_adapter *adapter, guint32 *class)
{
	property_get_uint32(adapter->adapter_proxy, "Class", class);
}

BTResult bluez_adapter_set_discoverable(struct bluez_adapter *adapter,
							gboolean discoverable)
{
	GVariant *value, *parameter;

	value = g_variant_new("b", discoverable);
	parameter = g_variant_new("(ssv)", ADAPTER_INTERFACE,
						"Discoverable", value);

	return property_set_variant(adapter->properties_proxy, parameter);
}

void bluez_adapter_get_discoverable(struct bluez_adapter *adapter,
							gboolean *discoverable)
{
	property_get_boolean(adapter->adapter_proxy,
						"Discoverable", discoverable);
}

BTResult bluez_adapter_set_pairable(struct bluez_adapter *adapter,
							gboolean pairable)
{
	GVariant *value, *parameter;

	value = g_variant_new("b", pairable);
	parameter = g_variant_new("(ssv)", ADAPTER_INTERFACE,
						"Pairable", value);

	return property_set_variant(adapter->properties_proxy, parameter);
}

void bluez_adapter_get_pairable(struct bluez_adapter *adapter,
							gboolean *pairable)
{
	property_get_boolean(adapter->adapter_proxy, "Pairable", pairable);
}

BTResult bluez_adapter_set_discoverable_timeout(struct bluez_adapter *adapter,
							guint32 timeout)
{
	GVariant *value, *parameter;

	value = g_variant_new("u", timeout);
	parameter = g_variant_new("(ssv)", ADAPTER_INTERFACE,
						"DiscoverableTimeout", value);

	return property_set_variant(adapter->properties_proxy, parameter);
}

void bluez_adapter_get_discoverable_timeout(struct bluez_adapter *adapter,
							guint32 *timeout)
{
	property_get_uint32(adapter->adapter_proxy,
						"DiscoverableTimeout", timeout);
}

void bluez_adapter_get_discovering(struct bluez_adapter *adapter,
							gboolean *discovering)
{
	property_get_boolean(adapter->adapter_proxy, "Discovering", discovering);
}

gchar **bluez_adapter_get_uuids(struct bluez_adapter *adapter)
{
	return property_get_strings(adapter->adapter_proxy, "UUIDs");
}

static void adapter_properties_changed(GDBusProxy *proxy,
					GVariant *changed_properties,
					GStrv *invalidated_properties,
					gpointer user_data)
{
	struct bluez_adapter *adapter = (struct bluez_adapter *) user_data;
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

	if (adapter->property_func)
		adapter->property_func(adapter, prop_names);

	g_strfreev(prop_names);
}

struct bluez_adapter *bluez_adapter_new(GDBusObject *object)
{
	struct bluez_adapter *adapter;
	GDBusInterface *interface;
	GDBusProxy *proxy;

	adapter = g_try_new0(struct bluez_adapter, 1);
	if (!adapter)
		return NULL;

	/* org.bluez.Adapter1 */
	interface = g_dbus_object_get_interface(object, ADAPTER_INTERFACE);
	proxy = G_DBUS_PROXY(interface);
	adapter->adapter_proxy = proxy;

	/* org.freedesktop.DBus.Properties */
	interface = g_dbus_object_get_interface(object, PROPERTIES_INTERFACE);
	proxy = G_DBUS_PROXY(interface);
	adapter->properties_proxy = proxy;

	/* connect signal */
	g_signal_connect(adapter->adapter_proxy, "g-properties-changed",
			G_CALLBACK(adapter_properties_changed), adapter);

	return adapter;
}

void bluez_adapter_free(struct bluez_adapter *adapter)
{
	if (!adapter)
		return;

	g_object_unref(adapter->adapter_proxy);
	g_object_unref(adapter->properties_proxy);

	g_free(adapter);
}
