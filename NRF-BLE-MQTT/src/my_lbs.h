/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BT_LBS_H_
#define BT_LBS_H_

/**@file
 * @defgroup bt_lbs LED Button Service API
 * @{
 * @brief API for the LED Button Service (LBS).
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/types.h>

/* STEP 1 - Define the 128 bit UUIDs for the GATT service and its characteristics in */
/** @brief LBS Service UUID. */
#define BT_UUID_LBS_VAL \
	BT_UUID_128_ENCODE(0x6e400001, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)

/** @brief LED Characteristic UUID. */
#define BT_UUID_LBS_LED_VAL \
	BT_UUID_128_ENCODE(0x6e400002, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)


#define BT_UUID_LBS           BT_UUID_DECLARE_128(BT_UUID_LBS_VAL)
#define BT_UUID_LBS_LED       BT_UUID_DECLARE_128(BT_UUID_LBS_LED_VAL)

/** @brief Callback type for when an LED state change is received. */
typedef void (*write_cb_t)(char *value);

/** @brief Callback type for when the button state is pulled. */
typedef bool (*button_cb_t)(void);

/** @brief Callback struct used by the LBS Service. */
struct my_lbs_cb {
	/** LED state change callback. */
	write_cb_t    write_cb;
};

/** @brief Initialize the LBS Service.
 *
 * This function registers application callback functions with the My LBS
 * Service 
 *
 * @param[in] callbacks Struct containing pointers to callback functions
 *			used by the service. This pointer can be NULL
 *			if no callback functions are defined.
 *
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int my_lbs_init(struct my_lbs_cb *callbacks);


#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* BT_LBS_H_ */
