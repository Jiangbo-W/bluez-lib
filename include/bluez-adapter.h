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

#ifndef __BLUEZ_ADAPTER_H__
#define __BLUEZ_ADAPTER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <glib.h>
#include <gio/gio.h>

#include "bluez-common.h"

struct bluez_adapter;
struct bluez_device;

typedef void (*adapter_property_watch) (struct bluez_adapter *adapter,
							gchar **prop_names);

/* adapter methods */
BTResult bluez_adapter_start_discovery(struct bluez_adapter *adapter);

BTResult bluez_adapter_stop_discovery(struct bluez_adapter *adapter);

BTResult bluez_adapter_remove_device(struct bluez_adapter *adapter,
						struct bluez_device *device);

gchar **bluez_adapter_get_property_names(struct bluez_adapter *adapter);

/* Get adapter propperties */
gchar *bluez_adapter_get_alias(struct bluez_adapter *adapter);

void bluez_adapter_get_powered(struct bluez_adapter *adapter,
						gboolean *powered);

gchar *bluez_adapter_get_address(struct bluez_adapter *adapter);

void bluez_adapter_get_class(struct bluez_adapter *adapter,
							guint32 *class);

void bluez_adapter_get_discoverable(struct bluez_adapter *adapter,
							gboolean *discoverable);

void bluez_adapter_get_pairable(struct bluez_adapter *adapter,
							gboolean *pairable);

void bluez_adapter_get_discoverable_timeout(struct bluez_adapter *adapter,
							guint32 *timeout);

void bluez_adapter_get_discovering(struct bluez_adapter *adapter,
							gboolean *discovering);

gchar **bluez_adapter_get_uuids(struct bluez_adapter *adapter);

/* Set adapter properties */
BTResult bluez_adapter_set_powered(struct bluez_adapter *adapter,
							gboolean powered);

BTResult bluez_adapter_set_alias(struct bluez_adapter *adapter, char *alias);

BTResult bluez_adapter_set_discoverable(struct bluez_adapter *adapter,
							gboolean discoverable);

BTResult bluez_adapter_set_pairable(struct bluez_adapter *adapter,
							gboolean pairable);

BTResult bluez_adapter_set_discoverable_timeout(struct bluez_adapter *adapter,
							guint32 timeout);

/* adapter constructer */
void bluez_adapter_set_properties_watch(struct bluez_adapter *adapter,
				adapter_property_watch func, gpointer user_data);

struct bluez_adapter *bluez_adapter_new(GDBusObject *object);

void bluez_adapter_free(struct bluez_adapter *adapter);

#ifdef __cplusplus
}
#endif

#endif
