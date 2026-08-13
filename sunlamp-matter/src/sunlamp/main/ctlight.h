#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

//for the double polarity led strip this gets weird
//something like when PIN_LED_WW is 1 and PIN_LED_WC is 0 => WW is lit
#define PIN_LED_WW 5
#define PIN_LED_WC 6

//power limiting factor, 0.0f<factor<=1.0f
#define MCPWM_HBRIDGE_MAX_BRIGHTNESS 0.5f

#define MCPWM_HBRIDGE_PWM_BASE_FREQ 10000
#define MCPWM_HBRIDGE_PWM_DEPTH 10
#define MCPWM_HBRIDGE_MIN_PULSE_NS 1400
#define MCPWM_HBRIDGE_USE_FADE true

#define WW_CT 3200
#define WC_CT 6500

void ctlight_init(void);

int ctlight_set_on(bool value);
int ctlight_set_brightness(float value);
int ctlight_set_ct(uint32_t value);

#ifdef __cplusplus
}
#endif