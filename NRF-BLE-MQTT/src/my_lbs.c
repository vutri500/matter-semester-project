/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief LED Button Service (LBS) sample
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include "my_lbs.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(Lesson4_Exercise1);


static bool                   button_state;
static struct my_lbs_cb       lbs_cb;

/* STEP 6 - Implement the write callback function of the LED characteristic */
static ssize_t write_led(struct bt_conn *conn,
			 const struct bt_gatt_attr *attr,
			 const void *buf,
			 uint16_t len, uint16_t offset, uint8_t flags)
{
	char aux[20];
	memcpy(aux, (uint8_t *)buf, len); 
	aux[len] = '\0';
	lbs_cb.write_cb(aux);

	return len;
}


/* LED Button Service Declaration */
/* STEP 2 - Create and add the MY LBS service to the Bluetooth LE stack */
BT_GATT_SERVICE_DEFINE(my_lbs_svc,
BT_GATT_PRIMARY_SERVICE(BT_UUID_LBS),
/* STEP 4 - Create and add the LED characteristic. */
	BT_GATT_CHARACTERISTIC(BT_UUID_LBS_LED,
			       BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_WRITE,
			       NULL, write_led, NULL),
 
);

/* A function to register application callbacks for the LED and Button characteristics  */
int my_lbs_init(struct my_lbs_cb *callbacks)
{
	if (callbacks) {
		lbs_cb.write_cb    = callbacks->write_cb;
	}

	return 0;
}


