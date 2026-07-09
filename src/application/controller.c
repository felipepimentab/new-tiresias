#include "controller.h"

#include "macros_common.h"
#include "zbus_common.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(controller, CONFIG_LOG_DEFAULT_LEVEL);

#define CONTROLLER_THREAD_STACK_SIZE 1024
#define CONTROLLER_THREAD_PRIORITY   3
#define ZBUS_TIMEOUT_MS	      100

ZBUS_SUBSCRIBER_DEFINE(controller_sub, 8);

ZBUS_CHAN_DECLARE(button_chan);
ZBUS_CHAN_DECLARE(bt_cmd_chan);
ZBUS_CHAN_DECLARE(bt_state_chan);
ZBUS_CHAN_DECLARE(audio_cmd_chan);
ZBUS_CHAN_DECLARE(audio_state_chan);

ZBUS_CHAN_DEFINE(controller_event_chan, controller_event_chan_msg, NULL, NULL,
		 ZBUS_OBSERVERS(controller_sub), ZBUS_MSG_INIT(.event = CONTROLLER_EVENT_INIT));

static controller_state current_state = CONTROLLER_STATE_OFF;
static bt_state latest_bt_state = BT_STATE_OFF;
static audio_state latest_audio_state = AUDIO_STATE_OFF;
static bool observers_registered;

static void set_controller_state(controller_state state)
{
	if (current_state == state) {
		return;
	}

	LOG_INF("Controller state transition: %d -> %d", current_state, state);
	current_state = state;
}

controller_state controller_get_state(void)
{
	return current_state;
}

bool controller_is_operational(void)
{
	return current_state == CONTROLLER_STATE_STANDARD ||
	       current_state == CONTROLLER_STATE_BROADCAST_STREAMING;
}

static int observers_register(void)
{
	int ret;

	if (observers_registered) {
		return 0;
	}

	ret = zbus_chan_add_obs(&bt_state_chan, &controller_sub, K_MSEC(ZBUS_TIMEOUT_MS));
	if (ret) {
		return ret;
	}

	ret = zbus_chan_add_obs(&audio_state_chan, &controller_sub, K_MSEC(ZBUS_TIMEOUT_MS));
	if (ret) {
		return ret;
	}

	ret = zbus_chan_add_obs(&button_chan, &controller_sub, K_MSEC(ZBUS_TIMEOUT_MS));
	if (ret) {
		return ret;
	}

	observers_registered = true;
	return 0;
}

static void audio_codec_switch_request(void)
{
	audio_cmd_chan_msg msg = {
		.cmd = AUDIO_CMD_CODEC_SWITCH,
	};
	int ret;

	ret = zbus_chan_pub(&audio_cmd_chan, &msg, K_MSEC(ZBUS_TIMEOUT_MS));
	if (ret) {
		LOG_ERR("Failed to publish audio codec switch command: %d", ret);
	}
}

static void handle_button_event(void)
{
	btn_chan_msg_t msg;
	int ret;

	ret = zbus_chan_read(&button_chan, &msg, K_MSEC(ZBUS_TIMEOUT_MS));
	ERR_CHK(ret);

	if (msg.event != BUTTON_1_PRESSED) {
		LOG_WRN("Unhandled button event: %d", msg.event);
		return;
	}

	LOG_INF("Button 1 pressed, switching codec");
	audio_codec_switch_request();
}

static void handle_bt_state_event(void)
{
	bt_state_chan_msg msg;
	int ret;

	ret = zbus_chan_read(&bt_state_chan, &msg, K_MSEC(ZBUS_TIMEOUT_MS));
	ERR_CHK(ret);

	latest_bt_state = msg.state;
	if (latest_bt_state == BT_STATE_ERROR) {
		set_controller_state(CONTROLLER_STATE_ERROR);
	}
}

static void handle_audio_state_event(void)
{
	audio_state_chan_msg msg;
	int ret;

	ret = zbus_chan_read(&audio_state_chan, &msg, K_MSEC(ZBUS_TIMEOUT_MS));
	ERR_CHK(ret);

	latest_audio_state = msg.state;

	switch (latest_audio_state) {
	case AUDIO_STATE_STANDARD:
		if (current_state != CONTROLLER_STATE_INITIALIZING) {
			set_controller_state(CONTROLLER_STATE_STANDARD);
		}
		break;
	case AUDIO_STATE_BROADCAST_STREAMING:
		set_controller_state(CONTROLLER_STATE_BROADCAST_STREAMING);
		break;
	case AUDIO_STATE_ERROR:
		set_controller_state(CONTROLLER_STATE_ERROR);
		break;
	default:
		break;
	}
}

static void handle_init_event(void)
{
	bt_cmd_chan_msg bt_msg = {
		.cmd = BT_CMD_INIT,
	};
	audio_cmd_chan_msg audio_msg = {
		.cmd = AUDIO_CMD_INIT,
	};
	int ret;

	if (current_state != CONTROLLER_STATE_OFF) {
		return;
	}

	ret = observers_register();
	ERR_CHK(ret);

	set_controller_state(CONTROLLER_STATE_INITIALIZING);

	ret = zbus_chan_pub(&bt_cmd_chan, &bt_msg, K_MSEC(ZBUS_TIMEOUT_MS));
	ERR_CHK(ret);

	ret = zbus_chan_pub(&audio_cmd_chan, &audio_msg, K_MSEC(ZBUS_TIMEOUT_MS));
	ERR_CHK(ret);
}

static void maybe_finish_initialization(void)
{
	if (current_state != CONTROLLER_STATE_INITIALIZING) {
		return;
	}

	if (latest_bt_state == BT_STATE_READY && latest_audio_state == AUDIO_STATE_STANDARD) {
		set_controller_state(CONTROLLER_STATE_STANDARD);
	}
}

static void controller_state_machine(struct zbus_channel *chan)
{
	if (chan == &controller_event_chan) {
		handle_init_event();
		return;
	}

	if (chan == &bt_state_chan) {
		handle_bt_state_event();
		maybe_finish_initialization();
		return;
	}

	if (chan == &audio_state_chan) {
		handle_audio_state_event();
		maybe_finish_initialization();
		return;
	}

	if (chan == &button_chan) {
		if (controller_is_operational()) {
			handle_button_event();
		}
		return;
	}

	LOG_WRN("Unhandled controller zbus channel: %p", (void *)chan);
}

static void controller_thread(void)
{
	struct zbus_channel *chan;
	int ret;

	LOG_INF("Controller thread started");

	while (1) {
		ret = zbus_sub_wait(&controller_sub, &chan, K_FOREVER);
		if (ret) {
			LOG_ERR("Failed to wait for controller event: %d", ret);
			continue;
		}

		controller_state_machine(chan);
	}
}

int controller_init(void)
{
	int ret;

	ret = zbus_chan_notify(&controller_event_chan, K_NO_WAIT);
	ERR_CHK(ret);

	return ret;
}

K_THREAD_DEFINE(controller_thread_id, CONTROLLER_THREAD_STACK_SIZE, controller_thread, NULL, NULL, NULL,
		CONTROLLER_THREAD_PRIORITY, 0, 0);
