/*
 * Copyright (c) 2021 Huawei Inc.
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_mqtt_publisher_sample, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/random/rand32.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/types.h>
#include <zephyr/sys/printk.h>
#include <errno.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gap.h>


#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "config.h"
#include "my_lbs.h" //BLE lib

/* Buffers for MQTT client. */
static uint8_t rx_buffer[256];
static uint8_t tx_buffer[256];

/* The mqtt client struct */
static struct mqtt_client client_ctx;

/* MQTT Broker details. */
static struct sockaddr_storage broker;

static struct zsock_pollfd fds[1];
static int nfds;

static bool connected;
char value[10];


#define SUCCESS_OR_EXIT(rc) { if (rc != 0) { return 1; } }
#define SUCCESS_OR_BREAK(rc) { if (rc != 0) { break; } }
#define RC_STR(rc) ((rc) == 0 ? "OK" : "ERROR")
#define PRINT_RESULT(func, rc) \
	LOG_INF("%s: %d <%s>", (func), rc, RC_STR(rc))

struct mqtt_utf8 broker_password = {
	.utf8 = "mqtt_password",
	.size = 13,
};

struct mqtt_utf8 broker_username = {
	.utf8 = "mqtt_username",
	.size = 13,
};

char MQTT_TOPIC[12] = "ESP/Sense";
static int publish(struct mqtt_client *client, char *topic, char *payload, enum mqtt_qos qos);


//thread/queue multiprocessing definitions
#define THREAD_PRIORITY 7

#define STACKSIZE 1024
void publish_value();
//K_THREAD_DEFINE(thread0_id, STACKSIZE, publish_sensor, NULL, NULL, NULL,
//						THREAD_PRIORITY, 0, 0);

K_WORK_DEFINE(queue_work, publish_value);
//end thread/queue multiprocessing definitions

//BLE definitions
static struct bt_le_adv_param *adv_param = BT_LE_ADV_PARAM((BT_LE_ADV_OPT_CONNECTABLE|BT_LE_ADV_OPT_USE_IDENTITY), /* Connectable advertising and use identity address */
                800, /*Min Advertising Interval 500ms (800*0.625ms) */
                801, /*Max Advertising Interval 500.625ms (801*0.625ms)*/
                NULL); /* Set to NULL for undirected advertising*/

#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),

};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_LBS_VAL),
};

static void app_write_cb(char *value);

static struct my_lbs_cb app_callbacks = {
	.write_cb    = app_write_cb,
};

static void on_connected(struct bt_conn *conn, uint8_t err);
static void on_disconnected(struct bt_conn *conn, uint8_t reason);

struct bt_conn_cb connection_callbacks = {
    .connected              = on_connected,
    .disconnected           = on_disconnected,  
};
//end BLE definitions

//BLE functions
static void app_write_cb(char *data)
{
	/*
	char anchor_id[5];

	char* token = strtok(data, ",");

	if(token == NULL){
		printk("data processing error on get anchor id");
	}else{
		strcpy(anchor_id, token);
        
		token = strtok(NULL, ",");
		if(token == NULL){
			printk("data processing error on get value");
		}else{
			strcpy(value, token);
			//publishing the new value
			if(connected)
				publish_value();
			else
				printk("Broker is not connect");
		}
	}
	printk("id=%s dist=%s\n", anchor_id, value);
	*/
	if (data == NULL){
		printk("data processing error on get sensor");
	}else {
		strcpy(value, data);
		if(connected)
				publish_value();
			else
				printk("Broker is not connect");
	}
	printk("value= %s\n", value);
}

static void on_connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err %u)\n", err);
		return;
	}

	printk("Connected\n");
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason %u)\n", reason);
}

//end BLE functions

static void prepare_fds(struct mqtt_client *client)
{
	if (client->transport.type == MQTT_TRANSPORT_NON_SECURE) {
		fds[0].fd = client->transport.tcp.sock;
	}

	fds[0].events = ZSOCK_POLLIN;
	nfds = 1;
}

static void clear_fds(void)
{
	nfds = 0;
}

static int wait(int timeout)
{
	int ret = 0;

	if (nfds > 0) {
		ret = zsock_poll(fds, nfds, timeout);
		if (ret < 0) {
			LOG_ERR("poll error: %d", errno);
		}
	}

	return ret;
}

void mqtt_evt_handler(struct mqtt_client *const client,
		      const struct mqtt_evt *evt)
{
	int err;
	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT connect failed %d", evt->result);
			break;
		}
		LOG_INF("MQTT client connected!");
		connected = true;
		break;

	case MQTT_EVT_DISCONNECT:
		LOG_INF("MQTT client disconnected %d", evt->result);

		connected = false;
		clear_fds();

		break;

	case MQTT_EVT_PUBLISH:
	{
		const struct mqtt_publish_param *pub = &evt->param.publish;
		uint8_t payload[33];
		int len = pub->message.payload.len;
		int bytes_read;

		LOG_INF("MQTT publish received %d, %d bytes",
			evt->result, len);
		LOG_INF("   id: %d, qos: %d",
			pub->message_id,
			pub->message.topic.qos);
		//printk("   item: %s",
		//	log_strdup(pub->message.topic.topic.utf8));

		/* assuming the config message is textual */
		while (len) {
			bytes_read = mqtt_read_publish_payload(
				client, payload,
				len >= (sizeof(payload) - 1) ? (sizeof(payload) - 1) : len);
			if (bytes_read < 0 && bytes_read != -EAGAIN) {
				LOG_ERR("failure to read payload");
				break;
			}

			payload[bytes_read] = '\0';
			//printk("   payload: %s", log_strdup(payload));
			len -= bytes_read;
		}

		/* for MQTT_QOS_0_AT_MOST_ONCE no acknowledgment needed */
		if (pub->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
			struct mqtt_puback_param puback = {
				.message_id = pub->message_id
			};

			mqtt_publish_qos1_ack(client, &puback);
		}
		break;
	}

	case MQTT_EVT_PUBACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBACK error %d", evt->result);
			break;
		}

		LOG_INF("PUBACK packet id: %u", evt->param.puback.message_id);

		break;

	case MQTT_EVT_PUBREC:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBREC error %d", evt->result);
			break;
		}

		LOG_INF("PUBREC packet id: %u", evt->param.pubrec.message_id);

		const struct mqtt_pubrel_param rel_param = {
			.message_id = evt->param.pubrec.message_id
		};

		err = mqtt_publish_qos2_release(client, &rel_param);
		if (err != 0) {
			LOG_ERR("Failed to send MQTT PUBREL: %d", err);
		}

		break;

	case MQTT_EVT_PUBCOMP:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBCOMP error %d", evt->result);
			break;
		}

		LOG_INF("PUBCOMP packet id: %u",
			evt->param.pubcomp.message_id);

		break;

	case MQTT_EVT_PINGRESP:
		LOG_INF("PINGRESP packet");
		break;

	case MQTT_EVT_SUBACK:
		LOG_INF("SUBACK packet id: %u", evt->param.suback.message_id);
		break;

	case MQTT_EVT_UNSUBACK:
		LOG_INF("UNSUBACK packet id: %u", evt->param.suback.message_id);
		break;

	default:
		break;
	}
}

static int publish(struct mqtt_client *client, char *topic, char *payload, enum mqtt_qos qos)
{
	struct mqtt_publish_param param;
	int ret;

	param.message.topic.qos = qos;
	param.message.topic.topic.utf8 = (uint8_t *)topic;
	param.message.topic.topic.size =
			strlen(param.message.topic.topic.utf8);
	param.message.payload.data = payload;
	param.message.payload.len =
			strlen(param.message.payload.data);
	param.message_id = sys_rand32_get();
	param.dup_flag = 0U;
	param.retain_flag = 0U;
	
	ret = mqtt_publish(client, &param);
	printk("mqtt_publish: %d %s %s\n", ret, topic, payload);
	return ret;
}

void publish_value(){

	publish(&client_ctx, MQTT_TOPIC, value, MQTT_QOS_0_AT_MOST_ONCE);

}

static void broker_init(void)
{

	struct sockaddr_in6 *broker6 = (struct sockaddr_in6 *)&broker;

	broker6->sin6_family = AF_INET6;
	broker6->sin6_port = htons(SERVER_PORT);
	zsock_inet_pton(AF_INET6, SERVER_ADDR, &broker6->sin6_addr);
	LOG_INF("broker_init IPv6 for address %s %i",SERVER_ADDR, SERVER_PORT);

}

static void client_init(struct mqtt_client *client)
{
	mqtt_client_init(client);

	broker_init();

	/* MQTT client configuration */
	client->broker = &broker;
	client->evt_cb = mqtt_evt_handler;
	client->client_id.utf8 = (uint8_t *)MQTT_CLIENTID;
	client->client_id.size = strlen(MQTT_CLIENTID);
	client->password = &broker_password;
	client->user_name = &broker_username;
	//client->password = NULL;
	//client->user_name = NULL;
	client->protocol_version = MQTT_VERSION_3_1_1;
	client->transport.type = MQTT_TRANSPORT_NON_SECURE;

	/* MQTT buffers configuration */
	client->rx_buf = rx_buffer;
	client->rx_buf_size = sizeof(rx_buffer);
	client->tx_buf = tx_buffer;
	client->tx_buf_size = sizeof(tx_buffer);
}

/* In this routine we block until the connected variable is 1 */
static int try_to_connect(struct mqtt_client *client)
{
	int rc, i = 0;

	while (i++ < APP_CONNECT_TRIES && !connected) {

		client_init(client);

		rc = mqtt_connect(client);
		if (rc != 0) {
			PRINT_RESULT("mqtt_connect", rc);
			k_sleep(K_MSEC(APP_SLEEP_MSECS));
			continue;
		}

		prepare_fds(client);

		if (wait(APP_CONNECT_TIMEOUT_MS)) {
			mqtt_input(client);
		}

		if (!connected) {
			mqtt_abort(client);
		}
	}

	if (connected) {
		return 0;
	}

	return -EINVAL;
}

static int process_mqtt_and_sleep(struct mqtt_client *client, int timeout)
{
	int64_t remaining = timeout;
	int64_t start_time = k_uptime_get();
	int rc;

	while (remaining > 0 && connected) {
		if (wait(remaining)) {
			rc = mqtt_input(client);
			if (rc != 0) {
				PRINT_RESULT("mqtt_input", rc);
				return rc;
			}
		}

		rc = mqtt_live(client);
		if (rc != 0 && rc != -EAGAIN) {
			PRINT_RESULT("mqtt_live", rc);
			return rc;
		} else if (rc == 0) {
			rc = mqtt_input(client);
			if (rc != 0) {
				PRINT_RESULT("mqtt_input", rc);
				return rc;
			}
		}

		remaining = timeout + start_time - k_uptime_get();
	}

	return 0;
}

/*
__unused
static int publisher(void)
{
	int i, rc, r = 0;

	LOG_INF("attempting to connect: ");
	rc = try_to_connect(&client_ctx);
	PRINT_RESULT("try_to_connect", rc);
	SUCCESS_OR_EXIT(rc);

	i = 0;
	while (i++ < CONFIG_NET_SAMPLE_APP_MAX_ITERATIONS && connected) {
		r = -1;

		rc = mqtt_ping(&client_ctx);
		PRINT_RESULT("mqtt_ping", rc);
		SUCCESS_OR_BREAK(rc);

		rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
		SUCCESS_OR_BREAK(rc);

		rc = publish(&client_ctx, 1, get_mqtt_payload(), MQTT_QOS_0_AT_MOST_ONCE);
		PRINT_RESULT("mqtt_publish", rc);
		SUCCESS_OR_BREAK(rc);

		k_sleep(K_MSEC(1000));
		
		rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
		SUCCESS_OR_BREAK(rc);

		rc = publish(&client_ctx, 1, get_mqtt_payload(), MQTT_QOS_1_AT_LEAST_ONCE);
		PRINT_RESULT("mqtt_publish", rc);
		SUCCESS_OR_BREAK(rc);

		rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
		SUCCESS_OR_BREAK(rc);

		k_sleep(K_MSEC(1000));

		rc = publish(&client_ctx, 1, get_mqtt_payload(), MQTT_QOS_2_EXACTLY_ONCE);
		PRINT_RESULT("mqtt_publish", rc);
		SUCCESS_OR_BREAK(rc);

		rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
		SUCCESS_OR_BREAK(rc);

		r = 0;
		k_sleep(K_MSEC(1000));
	}

	rc = mqtt_disconnect(&client_ctx);
	PRINT_RESULT("mqtt_disconnect", rc);

	LOG_INF("Bye!");

	return r;
}
*/

void main(void)
{	
	k_sleep(K_MSEC(10000));
	
	int ret,err;
	
	LOG_INF("attempting to connect: ");
	int rc = try_to_connect(&client_ctx);
	PRINT_RESULT("try_to_connect", rc);
	SUCCESS_OR_EXIT(rc);
	
	ret = bt_enable(NULL);
	if (ret) {
		printk("Bluetooth init failed (err %d)\n", ret);
		return;
	}
	printk("Bluetooth initialized\n");
	
	bt_conn_cb_register(&connection_callbacks);

	err = my_lbs_init(&app_callbacks);
	if (err) {
		printk("Failed to init LBS (err:%d)\n", err);
		return;
	}
	LOG_INF("Bluetooth initialized\n");
	err = bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)\n", err);
		return;
	}

	LOG_INF("Advertising successfully started\n");
	/*
	for (;;) {
		k_sleep(K_MSEC(5000));
	}
	*/
}

// try to connect on the broker
//  fixing/undestanding the ping problem (install wireshark)

