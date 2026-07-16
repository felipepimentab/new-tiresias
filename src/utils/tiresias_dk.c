#include <nrfx_clock.h>

#include "button_handler.h"
#include "channel_assignment.h"
#include "led.h"
#include "tiresias_dk.h"
#include "zbus_common.h"

#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(tiresias_dk, CONFIG_MODULE_NRF5340_AUDIO_DK_LOG_LEVEL);

ZBUS_CHAN_DECLARE(led_chan);

static int leds_set(void)
{
	int ret;

  struct led_chan_msg_t led_msg;
  led_msg.led = LED_1;
  led_msg.cmd = TURN_ON;
  ret = zbus_chan_pub(&led_chan, &led_msg, K_MSEC(100));
  if (ret) {
    LOG_ERR("Failed to send LED command: %d", ret);
  }

  // led_msg.led = LED_2;
  // led_msg.cmd = TURN_ON;
  // ret = zbus_chan_pub(&led_chan, &led_msg, K_MSEC(100));
  // if (ret) {
  //   LOG_ERR("Failed to send LED command: %d", ret);
  // }

  // led_msg.led = LED_3;
  // led_msg.cmd = TURN_ON;
  // ret = zbus_chan_pub(&led_chan, &led_msg, K_MSEC(100));
  // if (ret) {
  //   LOG_ERR("Failed to send LED command: %d", ret);
  // }
	/* Blink LED2 to indicate that the application core is running. */
	// ret = led_blink(LED_APP_3_GREEN); TODO: change LED api call to zbus message
	if (ret) {
		return ret;
	}

#if (CONFIG_AUDIO_DEV == HEADSET)
	enum audio_channel channel;

	channel_assignment_get(&channel);

	if (channel == AUDIO_CH_L) {
		// ret = led_on(LED_APP_RGB); TODO: change LED api call to zbus message
	} else {
		// ret = led_on(LED_NET_RGB); TODO: change LED api call to zbus message
	}
#elif (CONFIG_AUDIO_DEV == GATEWAY)
  // This application is never a gateway
#endif /* (CONFIG_AUDIO_DEV == HEADSET) */

	if (ret) {
		return ret;
	}

	return 0;
}

static int channel_assign_check(void)
{
	return 0;
}

int tiresias_dk_init(void)
{
	int ret;

	ret = led_init();
	if (ret) {
		LOG_ERR("Failed to initialize LED module");
		return ret;
	}

	ret = button_handler_init();
	if (ret) {
		LOG_ERR("Failed to initialize button handler");
		return ret;
	}

	ret = channel_assign_check();
	if (ret) {
		LOG_ERR("Failed get channel assignment");
		return ret;
	}

	ret = leds_set();
	if (ret) {
		LOG_ERR("Failed to set LEDs");
		return ret;
	}

	/* Use this to turn on 128 MHz clock for cpu_app */
	ret = nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK, NRF_CLOCK_HFCLK_DIV_1);
	ret -= NRFX_ERROR_BASE_NUM;
	if (ret) {
		return ret;
	}

	return 0;
}

int nrf5340_audio_dk_init(void)
{
	return tiresias_dk_init();
}
