/**
 * @file controller.h
 * @brief Main application controller interface
 *
 * This module provides the main application controller that manages the overall
 * system state and coordinates between different services (Bluetooth, audio codec)
 * and system modules (peripherals). The controller implements a state machine
 * that handles system initialization, operation modes, and error conditions.
 *
 * @author Tiresias Firmware Team
 * @version 1.0
 */

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "zbus_common.h"

#include <stdbool.h>
#include <zephyr/zbus/zbus.h>

/* === Public Function Declarations === */

/**
 * @brief Get the current controller state
 *
 * @return controller_state Current state of the controller
 */
controller_state controller_get_state(void);

/**
 * @brief Check if the controller is in an operational state
 *
 * @return true if controller is ready for operation, false otherwise
 */
bool controller_is_operational(void);

int controller_init(void);

#endif /* CONTROLLER_H */
