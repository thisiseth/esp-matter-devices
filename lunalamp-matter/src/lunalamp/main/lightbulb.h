#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PIN_LED_R 10
#define PIN_LED_G 3
#define PIN_LED_B 4
#define PIN_LED_WW 0
#define PIN_LED_WC 1

#define WW_CT 3200
#define WC_CT 6500

#define WW_CAL_R 330
#define WW_CAL_G 145
#define WW_CAL_B 18
#define WW_CAL_W 200

#define WC_CAL_R 300
#define WC_CAL_G 240
#define WC_CAL_B 90
#define WC_CAL_W 230

void lightbulb_init(void);

int lightbulb_set_on(bool value);
int lightbulb_set_brightness(float value);
int lightbulb_set_ct(uint32_t value);
int lightbulb_set_x(float value);
int lightbulb_set_y(float value);

#ifdef __cplusplus
}
#endif