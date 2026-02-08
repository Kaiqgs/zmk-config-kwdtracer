/*
 * Copyright (c) 2024 KWD Tracer
 * SPDX-License-Identifier: MIT
 *
 * Travel switch module: GPIO-based hold detection with LED feedback
 * and soft_off integration for ZMK keyboards.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/poweroff.h>

#include <dt-bindings/input/input-event-codes.h>

LOG_MODULE_REGISTER(travel_switch, CONFIG_LOG_DEFAULT_LEVEL);

#define DT_DRV_COMPAT zmk_travel_switch

#if DT_HAS_COMPAT_STATUS_OKAY(zmk_travel_switch)

#define TRAVEL_NODE DT_INST(0, zmk_travel_switch)

static const struct gpio_dt_spec led_gpio = GPIO_DT_SPEC_GET(TRAVEL_NODE, led_gpios);

static const uint32_t hold_time_ms = DT_PROP(TRAVEL_NODE, hold_time_ms);
static const uint32_t led_timeout_ms = DT_PROP(TRAVEL_NODE, led_timeout_ms);
static const uint32_t blink_count = DT_PROP(TRAVEL_NODE, blink_count);
static const uint32_t blink_interval_ms = DT_PROP(TRAVEL_NODE, blink_interval_ms);

enum travel_state {
    STATE_IDLE,
    STATE_HOLD_PENDING,
    STATE_LED_COOLDOWN,
    STATE_BLINK_SEQUENCE,
    STATE_WAKEUP_HOLD_PENDING,
    STATE_SHUTTING_DOWN,
};

static enum travel_state state = STATE_IDLE;
static uint32_t blink_remaining;
static bool blink_led_is_on;
static bool switch_pressed;

static void hold_timer_handler(struct k_work *work);
static void led_timeout_handler(struct k_work *work);
static void blink_step_handler(struct k_work *work);
static void soft_off_handler(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(hold_timer_work, hold_timer_handler);
static K_WORK_DELAYABLE_DEFINE(led_timeout_work, led_timeout_handler);
static K_WORK_DELAYABLE_DEFINE(blink_step_work, blink_step_handler);
static K_WORK_DEFINE(soft_off_work, soft_off_handler);

static void led_on(void)
{
    gpio_pin_set_dt(&led_gpio, 1);
}

static void led_off(void)
{
    gpio_pin_set_dt(&led_gpio, 0);
}

static void enter_soft_off(void)
{
    LOG_INF("Entering soft_off");
    state = STATE_SHUTTING_DOWN;
    led_off();
    sys_poweroff();
}

static void soft_off_handler(struct k_work *work)
{
    enter_soft_off();
}

static void start_blink_sequence(void)
{
    state = STATE_BLINK_SEQUENCE;
    blink_remaining = blink_count;
    blink_led_is_on = false;
    /* Start with LED off (it was on during hold), then blink on/off cycles */
    led_off();
    k_work_schedule(&blink_step_work, K_MSEC(blink_interval_ms / 2));
}

static void hold_timer_handler(struct k_work *work)
{
    if (state == STATE_HOLD_PENDING || state == STATE_WAKEUP_HOLD_PENDING) {
        LOG_INF("Hold detected (state=%d)", state);
        start_blink_sequence();
    }
}

static void led_timeout_handler(struct k_work *work)
{
    if (state == STATE_LED_COOLDOWN) {
        LOG_INF("LED timeout, returning to idle");
        led_off();
        state = STATE_IDLE;
    }
}

static void blink_step_handler(struct k_work *work)
{
    if (state != STATE_BLINK_SEQUENCE) {
        return;
    }

    if (blink_remaining == 0) {
        /* Blink sequence complete */
        led_off();
        LOG_INF("Blink sequence complete, switch_pressed=%d", switch_pressed);
        if (!switch_pressed) {
            /* Switch already released, enter soft_off now */
            enter_soft_off();
        }
        /* If switch still pressed, we wait for release in the input callback */
        return;
    }

    if (!blink_led_is_on) {
        /* Turn LED on for first half of blink cycle */
        led_on();
        blink_led_is_on = true;
        k_work_schedule(&blink_step_work, K_MSEC(blink_interval_ms / 2));
    } else {
        /* Turn LED off for second half, one blink cycle complete */
        led_off();
        blink_led_is_on = false;
        blink_remaining--;
        k_work_schedule(&blink_step_work, K_MSEC(blink_interval_ms / 2));
    }
}

static void on_press(void)
{
    switch_pressed = true;

    switch (state) {
    case STATE_IDLE:
        LOG_INF("Press: IDLE -> HOLD_PENDING");
        state = STATE_HOLD_PENDING;
        led_on();
        k_work_schedule(&hold_timer_work, K_MSEC(hold_time_ms));
        break;

    case STATE_LED_COOLDOWN:
        LOG_INF("Press: LED_COOLDOWN -> HOLD_PENDING (restart)");
        k_work_cancel_delayable(&led_timeout_work);
        state = STATE_HOLD_PENDING;
        led_on();
        k_work_schedule(&hold_timer_work, K_MSEC(hold_time_ms));
        break;

    case STATE_BLINK_SEQUENCE:
    case STATE_HOLD_PENDING:
    case STATE_WAKEUP_HOLD_PENDING:
    case STATE_SHUTTING_DOWN:
        /* Ignore presses during these states */
        break;
    }
}

static void on_release(void)
{
    switch_pressed = false;

    switch (state) {
    case STATE_HOLD_PENDING:
        LOG_INF("Release: HOLD_PENDING -> LED_COOLDOWN");
        k_work_cancel_delayable(&hold_timer_work);
        state = STATE_LED_COOLDOWN;
        /* LED stays on, start cooldown timer */
        k_work_schedule(&led_timeout_work, K_MSEC(led_timeout_ms));
        break;

    case STATE_WAKEUP_HOLD_PENDING:
        LOG_INF("Release during wakeup hold -> re-enter soft_off");
        k_work_cancel_delayable(&hold_timer_work);
        led_off();
        enter_soft_off();
        break;

    case STATE_BLINK_SEQUENCE:
        if (blink_remaining == 0) {
            LOG_INF("Release after blink complete -> soft_off");
            /* Submit to work queue to avoid calling from input callback context */
            k_work_submit(&soft_off_work);
        }
        /* If blinks still in progress, the blink handler will check switch_pressed */
        break;

    case STATE_IDLE:
    case STATE_LED_COOLDOWN:
    case STATE_SHUTTING_DOWN:
        break;
    }
}

static void travel_switch_input_cb(struct input_event *evt, void *user_data)
{
    if (evt->code != INPUT_KEY_POWER) {
        return;
    }

    if (evt->value) {
        on_press();
    } else {
        on_release();
    }
}

INPUT_CALLBACK_DEFINE(NULL, travel_switch_input_cb, NULL);

static int travel_switch_init(void)
{
    int ret;

    if (!gpio_is_ready_dt(&led_gpio)) {
        LOG_ERR("LED GPIO device not ready");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&led_gpio, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure LED GPIO: %d", ret);
        return ret;
    }

    /*
     * Boot-time wakeup validation:
     * If waking from soft_off, the travel switch GPIO must be held.
     * Read the switch state via the gpio-keys node's GPIO.
     */
    const struct gpio_dt_spec switch_gpio =
        GPIO_DT_SPEC_GET(DT_NODELABEL(travel_key), gpios);

    if (!gpio_is_ready_dt(&switch_gpio)) {
        LOG_WRN("Switch GPIO not ready for boot check, proceeding normally");
        state = STATE_IDLE;
        return 0;
    }

    ret = gpio_pin_configure_dt(&switch_gpio, GPIO_INPUT);
    if (ret < 0) {
        LOG_WRN("Failed to configure switch GPIO for read: %d", ret);
        state = STATE_IDLE;
        return 0;
    }

    int pin_state = gpio_pin_get_dt(&switch_gpio);
    if (pin_state < 0) {
        LOG_WRN("Failed to read switch GPIO: %d", pin_state);
        state = STATE_IDLE;
        return 0;
    }

    if (pin_state) {
        /* Switch is held at boot - start wakeup hold validation */
        LOG_INF("Boot: switch pressed, starting wakeup hold validation");
        switch_pressed = true;
        state = STATE_WAKEUP_HOLD_PENDING;
        led_on();
        k_work_schedule(&hold_timer_work, K_MSEC(hold_time_ms));
    } else {
        /* Switch not pressed at boot - likely a bump, go back to soft_off */
        LOG_INF("Boot: switch not pressed, re-entering soft_off");
        enter_soft_off();
    }

    return 0;
}

SYS_INIT(travel_switch_init, APPLICATION, 99);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(zmk_travel_switch) */
