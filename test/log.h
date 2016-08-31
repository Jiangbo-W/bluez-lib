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

#ifndef __BLUEZ_LOG__
#define __BLUEZ_LOG__

#include <stdio.h>

#define ERROR(fmt, arg...) do { \
			printf("%s %d %s(): " fmt "\n", \
				__FILE__, __LINE__, __func__, ##arg); \
		} while (0)

#define DBG ERROR

#endif
