/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief Button assignments
 *
 * Button mappings are listed here.
 *
 */

#ifndef _BUTTON_ASSIGNMENTS_H_
#define _BUTTON_ASSIGNMENTS_H_

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#define BUTTON_UNASSIGNED 0xff

/** @brief List of buttons and associated metadata
 */
enum button_pin_names {
	BUTTON_VOLUME_DOWN = 0xf1,
	BUTTON_VOLUME_UP = 0xf2,
	BUTTON_PLAY_PAUSE = DT_GPIO_PIN(DT_ALIAS(sw0), gpios),
	BUTTON_4 = 0xf4,
	BUTTON_5 = 0xf5,
};

#endif /* _BUTTON_ASSIGNMENTS_H_ */
