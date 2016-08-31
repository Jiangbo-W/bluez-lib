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

#ifndef __BLUEZ_DEVICE_H__
#define __BLUEZ_DEVICE_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "bluez-common.h"

struct bluez_device;

typedef void (*device_property_watch) (struct bluez_device *device,
							gchar **prop_names);

void bluez_device_set_properties_watch(struct bluez_device *device,
				device_property_watch func, gpointer user_data);

BTResult bluez_device_connect(struct bluez_device *device);

BTResult bluez_device_disconnect(struct bluez_device *device);

BTResult bluez_device_connect_profile(struct bluez_device *device,
							const gchar *uuid);

BTResult bluez_device_disconnect_profile(struct bluez_device *device,
							const gchar *uuid);

void bluez_device_pair_with_reply(struct bluez_device *device,
					bluez_response_cb cb, void *user_data);

BTResult bluez_device_cancel_pair(struct bluez_device *device);

BTResult bluez_device_unpair(struct bluez_device *device);

gchar **bluez_device_get_property_names(struct bluez_device *device);

gchar *bluez_device_get_name(struct bluez_device *device);

BTResult bluez_device_set_alias(struct bluez_device *device, char *alias);

gchar *bluez_device_get_alias(struct bluez_device *device);

gchar *bluez_device_get_address(struct bluez_device *device);

void bluez_device_get_class(struct bluez_device *device, guint32 *class);

void bluez_device_get_paired(struct bluez_device *device, gboolean *paired);

void bluez_device_get_connected(struct bluez_device *device,
							gboolean *connected);

void bluez_device_get_rssi(struct bluez_device *device, gint16 *rssi);

gchar **bluez_device_get_uuids(struct bluez_device *device);

const gchar *bluez_device_get_path(struct bluez_device *device);

struct bluez_device *bluez_device_new(GDBusObject *object);

void bluez_device_free(struct bluez_device *device);

#ifdef __cplusplus
}
#endif

#endif
