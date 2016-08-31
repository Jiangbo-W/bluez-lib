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

#ifndef __BLUEZ_MANAGER_H__
#define __BLUEZ_MANAGER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <glib.h>

#include "bluez-common.h"

struct bluez_manager;
struct bluez_adapter;

typedef void (*bluez_adapter_added_cb) (struct bluez_adapter *adapter,
						gpointer user_data);
typedef void (*bluez_adapter_removed_cb) (struct bluez_adapter *adapter,
						gpointer user_data);
struct bluez_manager *bluez_manager_new(void);

void bluez_manager_free(struct bluez_manager *manager);

void bluez_manager_refresh_objects(struct bluez_manager *manager);

gboolean bluez_manager_set_adapter_watch(struct bluez_manager *manager,
				bluez_adapter_added_cb adapter_added,
				bluez_adapter_removed_cb adapter_removed,
				gpointer user_data);

#ifdef __cplusplus
}
#endif

#endif
