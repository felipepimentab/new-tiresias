#include "bluetooth.h"

#include "broadcast_sink.h"
#include "bt_mgmt.h"
#include "bt_rendering_and_capture.h"
#include "le_audio_rx.h"
#include "macros_common.h"
#include "zbus_common.h"

#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(bluetooth, LOG_LEVEL_INF);

#define BT_THREAD_STACK_SIZE 4096
#define BT_THREAD_PRIORITY   4
#define BT_ZBUS_TIMEOUT_MS   100

ZBUS_SUBSCRIBER_DEFINE(bt_service_sub, 8);

ZBUS_CHAN_DECLARE(bt_mgmt_chan);
ZBUS_CHAN_DECLARE(le_audio_chan);

ZBUS_CHAN_DEFINE(bt_state_chan, bt_state_chan_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(.state = BT_STATE_OFF));

ZBUS_CHAN_DEFINE(bt_cmd_chan, bt_cmd_chan_msg, NULL, NULL, ZBUS_OBSERVERS(bt_service_sub),
		 ZBUS_MSG_INIT(.cmd = BT_CMD_INIT));

static bt_state current_state = BT_STATE_OFF;
static uint32_t last_broadcast_id = BRDCAST_ID_NOT_USED;
static bool observers_registered;

static void set_bt_state(bt_state state)
{
	bt_state_chan_msg msg = {
		.state = state,
	};
	int ret;

	if (current_state == state) {
		return;
	}

	current_state = state;

	ret = zbus_chan_pub(&bt_state_chan, &msg, K_MSEC(BT_ZBUS_TIMEOUT_MS));
	if (ret) {
		LOG_ERR("Failed to publish Bluetooth state: %d", ret);
	}
}

static int observers_register(void)
{
	int ret;

	if (observers_registered) {
		return 0;
	}

	ret = zbus_chan_add_obs(&bt_mgmt_chan, &bt_service_sub, K_MSEC(BT_ZBUS_TIMEOUT_MS));
	if (ret) {
		return ret;
	}

	ret = zbus_chan_add_obs(&le_audio_chan, &bt_service_sub, K_MSEC(BT_ZBUS_TIMEOUT_MS));
	if (ret) {
		return ret;
	}

	observers_registered = true;
	return 0;
}

static int ext_adv_populate(struct bt_data *ext_adv_buf, size_t ext_adv_buf_size,
			    size_t *ext_adv_count)
{
	int ret;
	size_t ext_adv_buf_cnt = 0;

	NET_BUF_SIMPLE_DEFINE_STATIC(uuid_buf, CONFIG_EXT_ADV_UUID_BUF_MAX);

	ext_adv_buf[ext_adv_buf_cnt].type = BT_DATA_UUID16_ALL;
	ext_adv_buf[ext_adv_buf_cnt].data = uuid_buf.data;
	ext_adv_buf_cnt++;

	ret = bt_mgmt_manufacturer_uuid_populate(&uuid_buf, CONFIG_BT_DEVICE_MANUFACTURER_ID);
	if (ret) {
		LOG_ERR("Failed to add manufacturer UUID: %d", ret);
		return ret;
	}

	ret = broadcast_sink_uuid_populate(&uuid_buf);
	if (ret < 0) {
		LOG_ERR("Failed to add broadcast sink UUID: %d", ret);
		return ret;
	}

	ret = bt_r_and_c_uuid_populate(&uuid_buf);
	if (ret) {
		LOG_ERR("Failed to add renderer UUID: %d", ret);
		return ret;
	}

	ret = broadcast_sink_adv_populate(&ext_adv_buf[ext_adv_buf_cnt],
					  ext_adv_buf_size - ext_adv_buf_cnt);
	if (ret < 0) {
		LOG_ERR("Failed to add broadcast sink advertising data: %d", ret);
		return ret;
	}

	ext_adv_buf_cnt += ret;
	ext_adv_buf[0].data_len = uuid_buf.len;
	*ext_adv_count = ext_adv_buf_cnt;

	return 0;
}

static int broadcast_discovery_start(void)
{
	int ret;

	if (IS_ENABLED(CONFIG_BT_AUDIO_SCAN_DELEGATOR)) {
		static struct bt_data ext_adv_buf[CONFIG_EXT_ADV_BUF_MAX];
		size_t ext_adv_buf_cnt = 0;

		bt_mgmt_scan_delegator_init();

		ret = bt_r_and_c_init();
		if (ret) {
			return ret;
		}

		ret = ext_adv_populate(ext_adv_buf, ARRAY_SIZE(ext_adv_buf), &ext_adv_buf_cnt);
		if (ret) {
			return ret;
		}

		return bt_mgmt_adv_start(0, ext_adv_buf, ext_adv_buf_cnt, NULL, 0, true);
	}

	return bt_mgmt_scan_start(0, 0, BT_MGMT_SCAN_TYPE_BROADCAST,
				  CONFIG_BT_AUDIO_BROADCAST_NAME, BRDCAST_ID_NOT_USED);
}

static int broadcast_scan_restart(uint32_t broadcast_id)
{
	int ret;

	if (!IS_ENABLED(CONFIG_BT_OBSERVER)) {
		return 0;
	}

	ret = bt_mgmt_scan_start(0, 0, BT_MGMT_SCAN_TYPE_BROADCAST, NULL, broadcast_id);
	if (ret == -EALREADY) {
		return 0;
	}

	if (ret) {
		LOG_ERR("Failed to restart scanning: %d", ret);
		return ret;
	}

	LOG_INF("Restarted scanning for broadcaster");
	return 0;
}

static void handle_bt_init(void)
{
	int ret;

	if (current_state != BT_STATE_OFF) {
		return;
	}

	set_bt_state(BT_STATE_INITIALIZING);

	ret = observers_register();
	if (ret) {
		LOG_ERR("Failed to observe Bluetooth events: %d", ret);
		set_bt_state(BT_STATE_ERROR);
		return;
	}

	ret = bt_mgmt_init();
	if (ret) {
		LOG_ERR("Failed to initialize Bluetooth management: %d", ret);
		set_bt_state(BT_STATE_ERROR);
		return;
	}

	ret = le_audio_rx_init();
	if (ret) {
		LOG_ERR("Failed to initialize LE Audio RX path: %d", ret);
		set_bt_state(BT_STATE_ERROR);
		return;
	}

	ret = broadcast_sink_enable(le_audio_rx_data_handler);
	if (ret) {
		LOG_ERR("Failed to enable broadcast sink: %d", ret);
		set_bt_state(BT_STATE_ERROR);
		return;
	}

	ret = broadcast_discovery_start();
	if (ret) {
		LOG_ERR("Failed to start broadcast discovery: %d", ret);
		set_bt_state(BT_STATE_ERROR);
		return;
	}

	set_bt_state(BT_STATE_READY);
}

static void handle_bt_mgmt_event(const struct bt_mgmt_msg *msg)
{
	uint8_t *broadcast_code;
	int ret;

	switch (msg->event) {
	case BT_MGMT_CONNECTED:
		LOG_DBG("Connected");
		break;
	case BT_MGMT_DISCONNECTED:
		LOG_DBG("Disconnected");
		break;
	case BT_MGMT_SECURITY_CHANGED:
		LOG_DBG("Security changed");
		break;
	case BT_MGMT_PA_SYNCED:
		ret = broadcast_sink_pa_sync_set(msg->pa_sync, msg->broadcast_id);
		if (ret) {
			last_broadcast_id = BRDCAST_ID_NOT_USED;
			LOG_WRN("Failed to set PA sync: %d", ret);
		} else {
			last_broadcast_id = msg->broadcast_id;
		}
		break;
	case BT_MGMT_PA_SYNC_LOST:
		LOG_INF("PA sync lost, reason: %d", msg->pa_sync_term_reason);
		if (msg->pa_sync_term_reason != BT_HCI_ERR_LOCALHOST_TERM_CONN) {
			(void)broadcast_scan_restart(BRDCAST_ID_NOT_USED);
		}
		break;
	case BT_MGMT_BROADCAST_SINK_DISABLE:
		ret = broadcast_sink_disable();
		if (ret) {
			LOG_ERR("Failed to disable broadcast sink: %d", ret);
		}
		break;
	case BT_MGMT_BROADCAST_CODE_RECEIVED:
		bt_mgmt_broadcast_code_ptr_get(&broadcast_code);
		ret = broadcast_sink_broadcast_code_set(broadcast_code);
		if (ret) {
			LOG_ERR("Failed to set broadcast code: %d", ret);
		}
		break;
	default:
		LOG_WRN("Unexpected Bluetooth management event: %d", msg->event);
		break;
	}
}

static void handle_le_audio_event(const struct le_audio_msg *msg)
{
	int ret;

	switch (msg->event) {
	case LE_AUDIO_EVT_SYNC_LOST:
		ret = bt_mgmt_pa_sync_delete(msg->pa_sync);
		if (ret) {
			LOG_WRN("Failed to delete PA sync: %d", ret);
		}
		(void)broadcast_scan_restart(last_broadcast_id);
		break;
	case LE_AUDIO_EVT_NO_VALID_CFG:
		LOG_WRN("No valid configurations found, disabling broadcast sink");
		ret = broadcast_sink_disable();
		if (ret) {
			LOG_ERR("Failed to disable broadcast sink: %d", ret);
		}
		break;
	default:
		break;
	}
}

static void bt_thread(void *arg1, void *arg2, void *arg3)
{
	struct zbus_channel *chan;
	int ret;

	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	LOG_INF("Bluetooth service thread started");

	while (1) {
		ret = zbus_sub_wait(&bt_service_sub, &chan, K_FOREVER);
		if (ret) {
			LOG_ERR("Error waiting for Bluetooth service event: %d", ret);
			continue;
		}

		if (chan == &bt_cmd_chan) {
			bt_cmd_chan_msg msg;

			ret = zbus_chan_read(chan, &msg, K_MSEC(BT_ZBUS_TIMEOUT_MS));
			if (ret) {
				LOG_ERR("Error reading Bluetooth command: %d", ret);
				continue;
			}

			if (msg.cmd == BT_CMD_INIT) {
				handle_bt_init();
			}
		} else if (chan == &bt_mgmt_chan) {
			struct bt_mgmt_msg msg;

			ret = zbus_chan_read(chan, &msg, K_MSEC(BT_ZBUS_TIMEOUT_MS));
			if (ret) {
				LOG_ERR("Error reading Bluetooth management event: %d", ret);
				continue;
			}

			handle_bt_mgmt_event(&msg);
		} else if (chan == &le_audio_chan) {
			struct le_audio_msg msg;

			ret = zbus_chan_read(chan, &msg, K_MSEC(BT_ZBUS_TIMEOUT_MS));
			if (ret) {
				LOG_ERR("Error reading LE Audio event: %d", ret);
				continue;
			}

			handle_le_audio_event(&msg);
		}
	}
}

K_THREAD_DEFINE(bt_thread_id, BT_THREAD_STACK_SIZE, bt_thread, NULL, NULL, NULL,
		BT_THREAD_PRIORITY, 0, 0);
