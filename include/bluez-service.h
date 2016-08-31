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

#ifndef __BLUEZ_SERVICE_H__
#define __BLUEZ_SERVICE_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "bluez-common.h"

struct bluez_service;

typedef void (*service_property_watch) (struct bluez_service *service,
							gchar **prop_names);

void bluez_service_set_properties_watch(struct bluez_service *service,
				service_property_watch func, gpointer user_data);

BTResult bluez_service_connect(struct bluez_service *service);

BTResult bluez_service_disconnect(struct bluez_service *service);

gchar *bluez_service_get_device_path(struct bluez_service *service);

gchar *bluez_service_get_state(struct bluez_service *service);

gchar *bluez_service_get_remote_uuid(struct bluez_service *service);

struct bluez_service *bluez_service_new(GDBusObject *object);

void bluez_service_free(struct bluez_service *service);

#ifdef __cplusplus
}
#endif

#endif
