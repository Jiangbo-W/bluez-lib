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

#include <gio/gio.h>
#include <sys/signalfd.h>
#include <string.h>

#include "log.h"
#include "bluez-adapter.h"
#include "bluez-device.h"
#include "bluez-manager.h"

GMainLoop *mainloop;

GSList *adapters, *devices;
struct bluez_manager *manager;
struct bluez_adapter *default_adapter;

static void quit(void)
{
	g_main_loop_quit(mainloop);
}

static gboolean signal_handler(GIOChannel *channel, GIOCondition condition,
							gpointer user_data)
{
	gint fd;
	ssize_t readlen;
	struct signalfd_siginfo si;

	if (condition & (G_IO_NVAL | G_IO_ERR | G_IO_HUP | G_IO_NVAL))
		return FALSE;

	fd = g_io_channel_unix_get_fd(channel);

	readlen = read(fd, &si, sizeof(struct signalfd_siginfo));
	if (readlen != sizeof(si))
		return FALSE;

	switch (si.ssi_signo) {
	case SIGINT:
	case SIGTERM:
		DBG("Terminate.");
		quit();
		break;
	default:
		break;
	}

	return TRUE;
}

static guint setup_signal_handle(void)
{
	sigset_t mask;
	int signal_fd;
	guint id;
	GIOChannel *channel;

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGQUIT);

	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
		ERROR("Error to set signal handle");
		return 0;
	}

	signal_fd = signalfd(-1, &mask, 0);
	if (signal_fd < 0) {
		ERROR("Error to create signal file.");
		return 0;
	}

	channel = g_io_channel_unix_new(signal_fd);

	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);
	g_io_channel_set_close_on_unref(channel, TRUE);

	id = g_io_add_watch(channel,
			G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
			signal_handler, NULL);

	g_io_channel_unref(channel);

	return id;
}

static void agent_request_callback(enum agent_request_type type,
			gchar *address, void *request_data, void *user_data)
{
	DBG("pairing request %s %d\n", address, type);

	bluez_manager_agent_reply(manager, TRUE, NULL);
}

static void adapter_properties_changed(struct bluez_adapter *adapter,
							gchar **prop_names)
{
	char *name;
	int i;

	for (i = 0; i < g_strv_length(prop_names); ++i) {
		name = prop_names[i];

		if (!g_strcmp0(name, "Alias")) {
			gchar *alias;
			alias = bluez_adapter_get_alias(adapter);
			DBG("Alias: %s", alias);
			g_free(alias);
		} else if (!g_strcmp0(name, "Class")) {
			guint32 class;
			bluez_adapter_get_class(adapter, &class);
			DBG("Class: %d", class);
		} else if (!g_strcmp0(name, "Powered")) {
			gboolean powered;
			bluez_adapter_get_powered(adapter, &powered);
			DBG("Powered: %d", powered);
		} else if (!g_strcmp0(name, "Discoverable")) {
			gboolean discoverable;
			bluez_adapter_get_discoverable(adapter, &discoverable);
			DBG("Discoverable: %d", discoverable);
		} else if (!g_strcmp0(name, "Pairable")) {
			gboolean pairable;
			bluez_adapter_get_pairable(adapter, &pairable);
			DBG("Piarable: %d", pairable);
		} else if (!g_strcmp0(name, "DiscoverableTimeout")) {
			guint32 time;
			bluez_adapter_get_discoverable_timeout(adapter, &time);
			DBG("Timeout: %d", time);
		} else if (!g_strcmp0(name, "UUIDs")) {
			DBG("UUIDs");
		} else if (!g_strcmp0(name, "Discovering")) {
			gboolean discovering;
			bluez_adapter_get_discovering(adapter, &discovering);
			DBG("Discovering: %d", discovering);
		} else
			DBG("Unknown: %s", name);
	}
}

static void device_properties_changed(struct bluez_device *device,
							gchar **prop_names)
{
	char *name;
	int i;

	for (i = 0; i < g_strv_length(prop_names); ++i) {
		name = prop_names[i];

		if (!g_strcmp0(name, "Address")) {
			gchar *dev_addr;
			dev_addr = bluez_device_get_address(device);
			DBG("Address: %s", dev_addr);
			g_free(dev_addr);
		} else if (!g_strcmp0(name, "Name")) {
			gchar *dev_name;
			dev_name = bluez_device_get_name(device);
			DBG("Name: %s", dev_name);
			g_free(dev_name);
		} else if (!g_strcmp0(name, "Alias")) {
			gchar *dev_alias;
			dev_alias = bluez_device_get_alias(device);
			DBG("Alias: %s", dev_alias);
			g_free(dev_alias);
		} else if (!g_strcmp0(name, "Class")) {
			guint32 class;
			bluez_device_get_class(device, &class);
			DBG("Class: %d", class);
		} else if (!g_strcmp0(name, "RSSI")) {
			gint16 rssi;
			bluez_device_get_rssi(device, &rssi);
			DBG("RSSI: %d", rssi);
		} else if (!g_strcmp0(name, "Paired")) {
			gboolean paired;
			bluez_device_get_paired(device, &paired);
			DBG("Paired: %d", paired);
		} else if (!g_strcmp0(name, "Connected")) {
			gboolean connected;
			bluez_device_get_connected(device, &connected);
			DBG("Connected: %d", connected);
		} else if (!g_strcmp0(name, "UUIDs")) {
			DBG("UUIDs");
		} else
			DBG("Unknown: %s", name);
	}
}

static void adapter_added(struct bluez_adapter *adapter, gpointer user_data)
{
	gchar *name, *addr;
	gchar **uuids;
	int i;
	guint32 class, timeout;
	gboolean powered, discoverable, pairable;

	DBG("adapter %p added", adapter);

	name = bluez_adapter_get_alias(adapter);
	addr = bluez_adapter_get_address(adapter);
	bluez_adapter_get_class(adapter, &class);
	bluez_adapter_get_powered(adapter, &powered);
	bluez_adapter_get_discoverable(adapter, &discoverable);
	bluez_adapter_get_pairable(adapter, &pairable);
	bluez_adapter_get_discoverable_timeout(adapter, &timeout);

	uuids = bluez_adapter_get_uuids(adapter);

	DBG("Adapter Info:");
	DBG("\tName: %s", name);
	DBG("\tAddress: %s", addr);
	DBG("\tClass: %d", class);
	DBG("\tPowered: %d", powered);
	DBG("\tDiscoverable: %d", discoverable);
	DBG("\tPairable: %d", pairable);
	DBG("\tDiscoverable Timeout: %d", timeout);

	g_free(name);
	g_free(addr);

	for (i = 0; i < g_strv_length(uuids); ++i)
		DBG("\tUUIDs: %s", uuids[i]);

	g_strfreev(uuids);

	bluez_adapter_set_alias(adapter, "xxx1");
	bluez_adapter_set_discoverable(adapter, FALSE);
	bluez_adapter_set_pairable(adapter, TRUE);
	bluez_adapter_set_discoverable_timeout(adapter, 0);

	adapters = g_slist_append(adapters, adapter);

	default_adapter = adapters ? adapters->data : NULL;

	bluez_adapter_set_properties_watch(adapter,
					adapter_properties_changed, NULL);
}

static void device_added(struct bluez_device *device, gpointer user_data)
{
	DBG("device added");

	bluez_device_set_properties_watch(device,
					device_properties_changed, NULL);
}

static void adapter_removed(struct bluez_adapter *adapter, gpointer user_data)
{
	DBG("adapter %p removed", adapter);

	adapters = g_slist_remove(adapters, adapter);

	default_adapter = adapters ? adapters->data : NULL;
}

static void device_removed(struct bluez_device *device, gpointer user_data)
{
	DBG("device removed");
}

static void object_free(gpointer data)
{
	/*
	 * Bluez adapters and devices are freed by BlueZ manager,
	 * at the end, if it has some object don't free, it means
	 * leak some memory
	 */
	DBG("Leaking bluez object: %p", data);
}

int main(int argc, char **argv)
{
	guint signal;

	mainloop = g_main_loop_new(NULL, FALSE);

	signal = setup_signal_handle();

	manager = bluez_manager_new();
	if (manager == NULL)
		return -1;

	bluez_manager_set_adapter_watch(manager, adapter_added,
					adapter_removed, NULL);
	bluez_manager_set_device_watch(manager, device_added,
					device_removed, NULL);

	bluez_manager_register_agent(manager, agent_request_callback, NULL);

	bluez_manager_refresh_objects(manager);

	g_main_loop_run(mainloop);

	bluez_manager_free(manager);

	g_slist_free_full(adapters, object_free);
	g_slist_free_full(devices, object_free);

	g_source_remove(signal);

	g_main_loop_unref(mainloop);

	return 0;
}
