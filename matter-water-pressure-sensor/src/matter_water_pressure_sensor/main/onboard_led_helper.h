#pragma once

#include <led_driver.h>

static inline void onboard_led_off(led_driver_handle_t led)
{
    led_driver_set_power(led, false);
}

static inline void onboard_led_red(led_driver_handle_t led)
{
    led_driver_set_hue(led, 0);
    led_driver_set_brightness(led, 10);
    led_driver_set_saturation(led, 100);
    led_driver_set_power(led, true);
}

static inline void onboard_led_yellow(led_driver_handle_t led)
{
    led_driver_set_hue(led, 40);
    led_driver_set_brightness(led, 10);
    led_driver_set_saturation(led, 100);
    led_driver_set_power(led, true);
}

static inline void onboard_led_green(led_driver_handle_t led)
{
    led_driver_set_hue(led, 120);
    led_driver_set_brightness(led, 10);
    led_driver_set_saturation(led, 100);
    led_driver_set_power(led, true);
}

static inline void onboard_led_blue(led_driver_handle_t led)
{
    led_driver_set_hue(led, 240);
    led_driver_set_brightness(led, 10);
    led_driver_set_saturation(led, 100);
    led_driver_set_power(led, true);
}
