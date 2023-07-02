/*
 * Copyright (c) 2021 Huawei Inc.
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__


//#define SERVER_ADDR		"fdf9:62:8c0b:2:0:0:c0a8:8901" //Higor's laptop
#define SERVER_ADDR		"fde5:e361:ee82:2:0:0:ac15:1c7" //broker rasp 
#define SERVER_PORT		1883

#define APP_CONNECT_TIMEOUT_MS	2000
#define APP_SLEEP_MSECS		500

#define APP_CONNECT_TRIES	5

#define MQTT_CLIENTID		"zephyr_mqtt_client_1"

#endif
