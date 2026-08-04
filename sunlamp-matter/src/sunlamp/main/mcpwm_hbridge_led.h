#pragma once

#ifdef __cplusplus
extern "C" {
#endif

//led backend for weird double CT two wire led strips - one polarity is WC other is WW

#include <stdbool.h>
#include <sdkconfig.h>
#include <driver/gpio.h>

#ifndef CONFIG_SOC_MCPWM_SUPPORTED
    #error MCPWM is not supported by the target soc
#endif

typedef struct 
{
    gpio_num_t pin_ww_plus;
    gpio_num_t pin_wc_plus;

    int pwm_depth_bits; //i.e. if depth = 10 then physical brightness steps are 0..1023 
    int minimum_pulse_ns; //minimum pulse duration -- for example DRV8871 recommends at least 800ns 
} mcpwm_hbridge_led_config_t;

bool mcpwm_hbridge_led_init(const mcpwm_hbridge_led_config_t *config);
bool mcpwm_hbridge_led_set(float ww, float wc);

#ifdef __cplusplus
}
#endif