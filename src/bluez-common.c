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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "bluez-common.h"

/* This map match with BlueZ src/error.c */
static const struct BTResult_Map {
	BTResult ret;
	const gchar *str;
} ret_map[] = {
#define MAP(ret, name) {ret, name}
	MAP(BT_RESULT_INVALID_ARGS, "org.bluez.Error.InvalidArguments"),
	MAP(BT_RESULT_NOT_READY, "org.bluez.Error.NotReady"),
	MAP(BT_RESULT_IN_PROGRESS, "org.bluez.Error.InProgress"),
	MAP(BT_RESULT_NOT_AUTHORIZED, "org.bluez.Error.NotAuthorized"),
	MAP(BT_RESULT_ALREADY_EXISTS, "org.bluez.Error.AlreadyExists"),
	MAP(BT_RESULT_NOT_SUPPORTED, "org.bluez.Error.NotSupported"),
	MAP(BT_RESULT_NOT_CONNECTED, "org.bluez.Error.NotConnected"),
	MAP(BT_RESULT_ALREADY_CONNECTED, "org.bluez.Error.AlreadyConnected"),
	MAP(BT_RESULT_NOT_AVAILABLE, "org.bluez.Error.NotAvailable"),
	MAP(BT_RESULT_NOT_EXIST, "org.bluez.Error.DoesNotExist"),
	MAP(BT_RESULT_NO_ADAPTER, "org.bluez.Error.NoSuchAdapter"),
	MAP(BT_RESULT_NO_AGENT, "org.bluez.Error.AgentNotAvailable"),
	MAP(BT_RESULT_FAILED, "org.bluez.Error.Failed"),
#undef MAP
	{0, NULL}
};

struct proxy_reply {
	bluez_response_cb cb;
	void *user_data;
};

BTResult error_to_result(GError *error)
{
	const struct BTResult_Map *iter;

	if (error == NULL)
		return BT_RESULT_OK;

	for (iter = ret_map; iter->str != NULL; ++iter) {
		if (g_strrstr(error->message, iter->str))
			return iter->ret;
	}

	return BT_RESULT_FAILED;
}

const gchar *ret2str(BTResult ret)
{
	const struct BTResult_Map *iter;

	if (ret == BT_RESULT_OK)
		return "OK";

	for (iter = ret_map; iter->str != NULL; ++iter) {
		if (ret == iter->ret)
			return iter->str;
	}

	return "Failed";
}

void proxy_method_call_reply(GObject *object, GAsyncResult *res,
						gpointer user_data)
{
	struct proxy_reply *proxy_reply = (struct proxy_reply *)user_data;
	GError *err = NULL;
	GVariant *reply;
	BTResult ret;

	reply = g_dbus_proxy_call_finish(G_DBUS_PROXY(object), res, &err);
	ret = error_to_result(err);

	if (err != NULL)
		g_error_free(err);

	if (proxy_reply->cb)
		proxy_reply->cb(ret, reply, proxy_reply->user_data);

	g_free(proxy_reply);

	if (reply)
		g_variant_unref(reply);
}

void proxy_method_call_with_reply(GDBusProxy *proxy, const gchar *name,
		GVariant *parameter, bluez_response_cb func, void *user_data)
{
	struct proxy_reply *proxy_reply;

	proxy_reply = g_try_new0(struct proxy_reply, 1);

	proxy_reply->cb = func;
	proxy_reply->user_data = user_data;

	return g_dbus_proxy_call(proxy, name, parameter, 0, -1, NULL,
					proxy_method_call_reply, proxy_reply);
}

BTResult proxy_method_call(GDBusProxy *proxy, const gchar *name,
							GVariant *parameter)
{
	GError *err = NULL;
	GVariant *res;
	BTResult ret;

	res = g_dbus_proxy_call_sync(proxy, name, parameter, 0, -1, NULL, &err);
	if (res != NULL)
		g_variant_unref(res);

	ret = error_to_result(err);

	if (err != NULL)
		g_error_free(err);

	return ret;
}

gboolean property_get_boolean(GDBusProxy *proxy, const char *property,
							gboolean *value)
{
	GVariant *bool_v;

	bool_v = g_dbus_proxy_get_cached_property(proxy, property);
	if (bool_v == NULL) {
		printf("no cached property %s\n", property);
		return FALSE;
	}

	*value = g_variant_get_boolean(bool_v);

	g_variant_unref(bool_v);

	return TRUE;
}

gboolean property_get_int16(GDBusProxy *proxy, const char *property,
							gint16 *value)
{
	GVariant *int16_v;

	int16_v = g_dbus_proxy_get_cached_property(proxy, property);
	if (int16_v == NULL) {
		printf("no cached property %s\n", property);
		return FALSE;
	}

	*value = g_variant_get_int16(int16_v);

	g_variant_unref(int16_v);

	return TRUE;
}
gboolean property_get_uint32(GDBusProxy *proxy, const char *property,
							guint32 *value)
{
	GVariant *u32_v;

	u32_v = g_dbus_proxy_get_cached_property(proxy, property);
	if (u32_v == NULL) {
		printf("no cached property %s\n", property);
		return FALSE;
	}

	*value = g_variant_get_uint32(u32_v);

	g_variant_unref(u32_v);

	return TRUE;
}

gchar *property_get_string(GDBusProxy *proxy, const char *property)
{
	GVariant *string_v;
	char *string;

	string_v = g_dbus_proxy_get_cached_property(proxy, property);
	if (string_v == NULL) {
		printf("no cached property %s\n", property);
		return NULL;
	}

	string = g_variant_dup_string(string_v, NULL);

	g_variant_unref(string_v);

	return string;
}

gchar **property_get_strings(GDBusProxy *proxy, const char *property)
{
	GVariant *string_v;
	char **strv;

	string_v = g_dbus_proxy_get_cached_property(proxy, property);
	if (string_v == NULL) {
		printf("no cached property %s\n", property);
		return NULL;
	}

	strv = g_variant_dup_strv(string_v, NULL);

	g_variant_unref(string_v);

	return strv;
}

BTResult property_set_variant(GDBusProxy *proxy, GVariant *variant)
{
	return proxy_method_call(proxy, "Set", variant);
}

gchar *get_addrstr_from_path(const gchar *path)
{
	gchar *temp, *str;

	temp = g_strrstr(path, "/");
	/* ignore '/dev_' */
	str = g_strdup(temp + 5);

	g_strdelimit(str, "_", ':');

	printf("str: %s\n", str);
	return str;
}
