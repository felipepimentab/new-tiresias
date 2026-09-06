/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef TIRESIAS_SERVICE_H
#define TIRESIAS_SERVICE_H

#include "codec_contract.h"
#include "zbus_common.h"

#include <stdint.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>

#define TIRESIAS_SERVICE_UUID BT_UUID_128_ENCODE(0x7b9a0001, 0x6e4f, 0x4b2d, 0xa9c8, 0x4f2e6f5d1000)
#define TIRESIAS_PROTOCOL_INFO_UUID BT_UUID_128_ENCODE(0x7b9a0002, 0x6e4f, 0x4b2d, 0xa9c8, 0x4f2e6f5d1000)
#define TIRESIAS_STATUS_UUID BT_UUID_128_ENCODE(0x7b9a0004, 0x6e4f, 0x4b2d, 0xa9c8, 0x4f2e6f5d1000)
#define TIRESIAS_REQUEST_UUID BT_UUID_128_ENCODE(0x7b9a0005, 0x6e4f, 0x4b2d, 0xa9c8, 0x4f2e6f5d1000)
#define TIRESIAS_RESPONSE_UUID BT_UUID_128_ENCODE(0x7b9a0006, 0x6e4f, 0x4b2d, 0xa9c8, 0x4f2e6f5d1000)

#define TIRESIAS_PROTOCOL_MAJOR 5U
#define TIRESIAS_PROTOCOL_MINOR 0U
#define TIRESIAS_PROTOCOL_INFO_SIZE 24U
#define TIRESIAS_REQUEST_HEADER_SIZE 8U
#define TIRESIAS_RESPONSE_HEADER_SIZE 12U
#define TIRESIAS_REQUEST_SIZE (TIRESIAS_REQUEST_HEADER_SIZE + CODEC_PARAMETER_MAX_BYTE_COUNT)
#define TIRESIAS_RESPONSE_SIZE (TIRESIAS_RESPONSE_HEADER_SIZE + CODEC_PARAMETER_MAX_BYTE_COUNT)
#define TIRESIAS_STATUS_SIZE 16U

enum tiresias_capability {
  TIRESIAS_CAPABILITY_GET_PARAMETER = 1U << 0,
  TIRESIAS_CAPABILITY_SET_PARAMETER = 1U << 1,
  TIRESIAS_CAPABILITY_PERSISTENCE = 1U << 2,
  TIRESIAS_CAPABILITY_DSP_APPLY_DEFERRED = 1U << 3,
};

enum tiresias_opcode {
  TIRESIAS_OPCODE_GET_PARAMETER = 1,
  TIRESIAS_OPCODE_SET_PARAMETER = 2,
};

enum tiresias_result {
  TIRESIAS_RESULT_OK = 0,
  TIRESIAS_RESULT_BAD_REQUEST = 1,
  TIRESIAS_RESULT_NOT_FOUND = 2,
  TIRESIAS_RESULT_READ_ONLY = 3,
  TIRESIAS_RESULT_OUT_OF_RANGE = 4,
  TIRESIAS_RESULT_BUSY = 5,
  TIRESIAS_RESULT_PERSIST_FAILED = 6,
  TIRESIAS_RESULT_INTERNAL = 7,
  TIRESIAS_RESULT_DSP_FAILED = 8,
};

enum tiresias_status_flag {
  TIRESIAS_STATUS_PARAMETERS_LOADED = 1U << 0,
  TIRESIAS_STATUS_LAST_SET_PERSISTED = 1U << 1,
  TIRESIAS_STATUS_DSP_APPLY_DEFERRED = 1U << 2,
};

int tiresias_service_init(void);
void tiresias_service_on_connected(struct bt_conn* conn);
void tiresias_service_on_disconnected(struct bt_conn* conn);
void tiresias_service_set_control_state(control_link_state state);

#endif /* TIRESIAS_SERVICE_H */
