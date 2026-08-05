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

    uint32_t pwm_depth_bits; //i.e. if depth = 10 then physical brightness steps are 0..1023 

    //at least either minimum_pulse_ns, or pwm_frequency must be set

    uint32_t minimum_pulse_ns; //minimum pulse duration -- for example DRV8871 recommends at least 800ns 

    uint32_t pwm_frequency; //if not set freq is calculated so single width pwm pulse = minimum_pulse_ns
                       //for example for 800ns and bit depth 10 (1024 levels) period is 800ns*2*1024=~1.6ms => ~600hz
                       //if freq is set and single width pwm pulse is shorter than minimum_pulse_ns, PDM is used
} mcpwm_hbridge_led_config_t;

bool mcpwm_hbridge_led_init(const mcpwm_hbridge_led_config_t *config);
bool mcpwm_hbridge_led_set(float ww, float wc);

#ifdef __cplusplus
}
#endif