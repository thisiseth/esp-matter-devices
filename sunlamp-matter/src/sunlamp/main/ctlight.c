
#include "ctlight.h"

#include <stdio.h>
#include "esp_log.h"

#include "esp_system.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include <math.h>

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/task.h>

#include "mcpwm_hbridge_led.h"

static const char *TAG = "ctlight";

#define MATTER_MIN_BRIGHTNESS (1.0f/254.0f)

static TimerHandle_t leds_deferred_timer;
static TaskHandle_t leds_deferred_task;

static bool current_on;
static float current_brightness;
static uint32_t current_ct;

static void leds_deferred_update(bool longDelay);
static void leds_deferred_task_func(void* params);
static void leds_deferred_timer_callback(TimerHandle_t timer);

static void update_leds(void);
static void set_led_duty(float ww, float wc);

void ctlight_init(void)
{
    const mcpwm_hbridge_led_config_t led_config = {
        .pin_ww_plus = PIN_LED_WW,
        .pin_wc_plus = PIN_LED_WC,

        .pwm_frequency = MCPWM_HBRIDGE_PWM_BASE_FREQ,
        .pwm_depth_bits = MCPWM_HBRIDGE_PWM_DEPTH,
        .minimum_pulse_ns = MCPWM_HBRIDGE_MIN_PULSE_NS,

        .use_fade = MCPWM_HBRIDGE_USE_FADE
    };

    configASSERT(mcpwm_hbridge_led_init(&led_config));
        
    xTaskCreate(leds_deferred_task_func, "leds_update_task", 4096, NULL, tskIDLE_PRIORITY, &leds_deferred_task);
    configASSERT(leds_deferred_task);

    leds_deferred_timer = xTimerCreate("leds_update_timer",
                                        pdMS_TO_TICKS(50),
                                        pdFALSE, // one-shot
                                        NULL,
                                        leds_deferred_timer_callback);
    configASSERT(leds_deferred_timer);

    current_on = false;
    current_brightness = 0.0f;
    current_ct = 4000;

    update_leds();
}

int ctlight_set_on(bool value)
{
    current_on = value;
    leds_deferred_update(current_brightness <= MATTER_MIN_BRIGHTNESS);
    return 0;
}

int ctlight_set_brightness(float value)
{
    if (value > 1.0f || value < 0.0f)
        return 1;

    //add something like brightness gamma so the ui slider feels more natural
    current_brightness = value > 0.15f ? (value * value) : (value * 0.15f);
    leds_deferred_update(current_brightness <= MATTER_MIN_BRIGHTNESS);
    return 0;
}

int ctlight_set_ct(uint32_t value)
{
    current_ct = value;
    leds_deferred_update(false);
    return 0;
}

static void leds_deferred_update(bool longDelay)
{
    xTimerChangePeriod(leds_deferred_timer, longDelay ? pdMS_TO_TICKS(200) : pdMS_TO_TICKS(50), portMAX_DELAY);
}

static void leds_deferred_timer_callback(TimerHandle_t timer)
{
    xTaskNotifyGive(leds_deferred_task);
}

static void leds_deferred_task_func(void* params)
{
    for (;;) 
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        update_leds();
    }
}

//for convenience map e.g. 3200 +- some margin to exactly 3200 to select 'single led' white
#define CT_SINGLE_LED_MARGIN 75

static void update_leds_ct(uint32_t ct, float brightness) 
{
    float ww, wc;

    // reciprocal mix
    wc = (1.0f / (float)ct - 1.0f / (float)WW_CT) / (1.0f / (float)WC_CT - 1.0f / (float)WW_CT);

    //'dead-zone' around pure CT
    if (ct <= (WW_CT + CT_SINGLE_LED_MARGIN))
        wc = 0.0f;
    else if (ct >= (WC_CT - CT_SINGLE_LED_MARGIN))
        wc = 1.0f;

    wc = wc > 1.0f
             ? 1.0
         : wc < 0.0f
             ? 0.0f
             : wc;

    ww = 1.0f - wc;

    ww *= brightness;
    wc *= brightness;

    ESP_LOGI(TAG, "ct ww: %5f wc: %5f", ww, wc);

    set_led_duty(ww, wc);
}

static void update_leds() 
{
    ESP_LOGI(TAG, "update_leds: on=%d, brightness=%.2f, ct=%d", current_on, current_brightness, current_ct);

    if (!current_on)
    {
        set_led_duty(0, 0);
        return;
    }

    update_leds_ct(current_ct, current_brightness);
}

static void set_led_duty(float ww, float wc)
{
    mcpwm_hbridge_led_set(ww*MCPWM_HBRIDGE_MAX_BRIGHTNESS, wc*MCPWM_HBRIDGE_MAX_BRIGHTNESS);
}