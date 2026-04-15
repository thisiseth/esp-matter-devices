#include <stdio.h>
#include <stdbool.h>
#include "esp_log.h"

#include "lightbulb.h"

#include "esp_system.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include <math.h>

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/task.h>

static const char *TAG = "lightbulb";

#define LUNA_LEDC_TIMER              LEDC_TIMER_0
#define LUNA_LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LUNA_LEDC_CHANNEL            LEDC_CHANNEL_0
#define LUNA_LEDC_DUTY_RES           LEDC_TIMER_11_BIT
#define LUNA_LEDC_FREQ 39060

#define LUNA_LEDC_CHANNEL_R     LEDC_CHANNEL_1
#define LUNA_LEDC_CHANNEL_G     LEDC_CHANNEL_2
#define LUNA_LEDC_CHANNEL_B     LEDC_CHANNEL_3
#define LUNA_LEDC_CHANNEL_WW    LEDC_CHANNEL_4
#define LUNA_LEDC_CHANNEL_WC    LEDC_CHANNEL_5

#define LUNA_LEDC_DUTY_MAX (1 << LUNA_LEDC_DUTY_RES)
#define LUNA_LEDC_DUTY_MIN (LUNA_LEDC_DUTY_MAX / 512)

#define MIN_COLOR_VALUE (1.0F / LUNA_LEDC_DUTY_MAX)

#define LUNA_LEDC_USE_FADE
#define LUNA_LEDC_FADE_TIME_MS 500

#define MATTER_MIN_BRIGHTNESS (1.0f/254.0f)

static TimerHandle_t leds_deferred_timer;
static TaskHandle_t leds_deferred_task;

static bool current_on;
static float current_brightness;
static bool current_mode_is_ct;
static uint32_t current_ct;
static float current_x;
static float current_y;

static void leds_deferred_update(bool longDelay);
static void leds_deferred_task_func(void* params);
static void leds_deferred_timer_callback(TimerHandle_t timer);

static void update_leds(void);
static void set_led_duty(float r, float g, float b, float ww, float wc);

void lightbulb_init(void)
{
    gpio_set_drive_capability(LUNA_LEDC_CHANNEL_R, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(LUNA_LEDC_CHANNEL_G, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(LUNA_LEDC_CHANNEL_B, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(LUNA_LEDC_CHANNEL_WW, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(LUNA_LEDC_CHANNEL_WC, GPIO_DRIVE_CAP_3);
    
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LUNA_LEDC_MODE,
        .timer_num        = LUNA_LEDC_TIMER,
        .duty_resolution  = LUNA_LEDC_DUTY_RES,
        .freq_hz          = LUNA_LEDC_FREQ,  // Set output frequency at 5 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

#ifdef LUNA_LEDC_USE_FADE
    ESP_ERROR_CHECK(ledc_fade_func_install(0));
#endif

    ESP_LOGI(TAG, "pwm timer freq: %lu", ledc_get_freq(LUNA_LEDC_MODE, LUNA_LEDC_TIMER));

    ledc_channel_config_t ledc_channel_r = {
        .speed_mode     = LUNA_LEDC_MODE,
        .channel        = LUNA_LEDC_CHANNEL_R,
        .timer_sel      = LUNA_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_LED_R,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_r));
    
    ledc_channel_config_t ledc_channel_g = {
        .speed_mode     = LUNA_LEDC_MODE,
        .channel        = LUNA_LEDC_CHANNEL_G,
        .timer_sel      = LUNA_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_LED_G,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_g));
    
    ledc_channel_config_t ledc_channel_b = {
        .speed_mode     = LUNA_LEDC_MODE,
        .channel        = LUNA_LEDC_CHANNEL_B,
        .timer_sel      = LUNA_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_LED_B,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_b));
    
    ledc_channel_config_t ledc_channel_ww = {
        .speed_mode     = LUNA_LEDC_MODE,
        .channel        = LUNA_LEDC_CHANNEL_WW,
        .timer_sel      = LUNA_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_LED_WW,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_ww));
    
    ledc_channel_config_t ledc_channel_wc = {
        .speed_mode     = LUNA_LEDC_MODE,
        .channel        = LUNA_LEDC_CHANNEL_WC,
        .timer_sel      = LUNA_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_LED_WC,
        .duty           = 0, 
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_wc));
    
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
    current_mode_is_ct = true;
    current_ct = 4000;
    current_x = 0.01f;
    current_y = 0.01f;

    update_leds();
}

int lightbulb_set_on(bool value)
{
    current_on = value;
    leds_deferred_update(current_brightness <= MATTER_MIN_BRIGHTNESS);
    return 0;
}

int lightbulb_set_brightness(float value)
{
    if (value > 1.0f || value < 0.0f)
        return 1;

    //add something like brightness gamma so the ui slider feels more natural
    current_brightness = value > 0.15f ? (value * value) : (value * 0.15f);
    leds_deferred_update(current_brightness <= MATTER_MIN_BRIGHTNESS);
    return 0;
}

int lightbulb_set_ct(uint32_t value)
{
    current_mode_is_ct = true;
    current_ct = value;
    leds_deferred_update(false);
    return 0;
}

int lightbulb_set_x(float value)
{
    if (value > 1.0f || value < 0.0f)
        return 1;

    current_mode_is_ct = false;
    current_x = value;
    leds_deferred_update(false);
    return 0;
}

int lightbulb_set_y(float value)
{
    if (value > 1.0f || value < 0.0f)
        return 1;

    current_mode_is_ct = false;
    current_y = value;
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

#define vec_length(x, y, z) sqrtf(x*x + y*y + z*z)

//hue 30 sat 67 from iphone app - 3200K-ish warm white, while 1.0,1.0,1.0 assumed to be 6500K
#define WW_VEC_R 1.0000F
#define WW_VEC_G 0.4422F
#define WW_VEC_B 0.1089F

//for convenience map e.g. 3200 +- some margin to exactly 3200 to select 'single led' white
#define CT_SINGLE_LED_MARGIN 75

//extend CT beyond 3200-6500 mixing in pure colors
#define RED_CT 0
#define RED_R 1.0f
#define RED_G 0.05f
#define RED_B 0.0f

#define BLUE_CT 18000
#define BLUE_R 0.05f
#define BLUE_G 0.0f
#define BLUE_B 1.0f

#define WW_VEC_LENGTH vec_length(WW_VEC_R, WW_VEC_G, WW_VEC_B)
#define WC_VEC_LENGTH vec_length(1, 1, 1)

#define CLAMP_COS(val) (val > 1.0F ? 1.0F : (val < -1.0F ? -1.0F : val))
#define CLAMP_RGB(val) ((val) < 0.0f ? 0.0f : (val))
#define MAX_RGB(r, g, b) (((r) >= (g) && (r) >= (b)) ? (r) : ((g) >= (r) && (g) >= (b)) ? (g) : (b))

static void update_leds_ct(uint32_t ct, float brightness) 
{
    float r, g, b, ww, wc;

    if (ct < (WW_CT - CT_SINGLE_LED_MARGIN)) //ww + red-ish
    {
        wc = 0;

        //linear mix
        ww = (ct - (float)RED_CT) / ((float)(WW_CT - RED_CT));

        r = RED_R * (1.0f - ww);
        g = RED_G * (1.0f - ww);
        b = RED_B * (1.0f - ww);

        r *= brightness;
        g *= brightness;
        b *= brightness;
        ww *= brightness;

        ESP_LOGI(TAG, "ct (<3200k) r: %5f g: %5f b: %5f ww: %5f wc: %5f", r, g, b, ww, wc);

        set_led_duty(r, g, b, ww, wc);
    }
    else if (ct <= (WC_CT + CT_SINGLE_LED_MARGIN)) //pure white
    {
        r = g = b = 0.0f;

        //reciprocal mix
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

        ESP_LOGI(TAG, "ct (pure) r: %5f g: %5f b: %5f ww: %5f wc: %5f", r, g, b, ww, wc);

        set_led_duty(r, g, b, ww, wc);
    }
    else //wc + blue-ish
    {
        ww = 0;
        
        //linear mix
        wc = ((float)BLUE_CT - ct) / ((float)(BLUE_CT - WC_CT));
        
        r = BLUE_R * (1.0f - wc);
        g = BLUE_G * (1.0f - wc);
        b = BLUE_B * (1.0f - wc);

        r *= brightness;
        g *= brightness;
        b *= brightness;
        wc *= brightness;

        ESP_LOGI(TAG, "ct (>6500k) r: %5f g: %5f b: %5f ww: %5f wc: %5f", r, g, b, ww, wc);

        set_led_duty(r, g, b, ww, wc);
    }
}

static void update_leds_linear_rgb(float r, float g, float b)
{
    ESP_LOGI(TAG, "gamma corrected r: %5f g: %5f b: %5f", r, g, b);

    if (r < MIN_COLOR_VALUE && 
        g < MIN_COLOR_VALUE && 
        b < MIN_COLOR_VALUE)
    {
        ESP_LOGI(TAG, "nothing to calculate, r: %8f g: %8f b: %8f", r, g, b);
        set_led_duty(0, 0, 0, 0, 0);
        return;
    }

    //find distance to both whites
    float cos_ww = (r*WW_VEC_R + g*WW_VEC_G + b*WW_VEC_B) / (vec_length(r, g, b) * WW_VEC_LENGTH);
    float cos_wc = (r + g + b) / (vec_length(r, g, b) * WC_VEC_LENGTH);

    cos_ww = CLAMP_COS(cos_ww);
    cos_wc = CLAMP_COS(cos_wc);

    float sin_ww = sqrtf(1 - cos_ww*cos_ww);
    float sin_wc = sqrtf(1 - cos_wc*cos_wc);

    ESP_LOGI(TAG, "cos_ww: %5f cos_wc: %5f sin_ww: %5f sin_wc: %5f", cos_ww, cos_wc, sin_ww, sin_wc);

    //close enough to use only one led
    if (sin_ww < 0.01F)
        sin_ww = 0;
    else if (sin_wc < 0.01F)
        sin_wc = 0;

    float wc_quantity = sin_ww / (sin_ww + (WC_VEC_LENGTH/WW_VEC_LENGTH)*sin_wc);
    float ww_quantity = 1 - wc_quantity;

    float whites_r = ww_quantity*WW_VEC_R + wc_quantity;
    float whites_g = ww_quantity*WW_VEC_G + wc_quantity;
    float whites_b = ww_quantity*WW_VEC_B + wc_quantity;

    float whites_quantity = fminf(r/whites_r, fminf(g/whites_g, b/whites_b));
    
    //subtract whites
    r -= whites_r*whites_quantity;
    g -= whites_g*whites_quantity;
    b -= whites_b*whites_quantity;

    //apply rgb to white coeffs
    //todo: lower coeffs when saturation is high to allow full green&blue ?
    r *= WC_CAL_R/(float)WC_CAL_W;
    g *= WC_CAL_G/(float)WC_CAL_W;
    b *= WC_CAL_B/(float)WC_CAL_W;

    ESP_LOGI(TAG, "rgb r: %5f g: %5f b: %5f ww: %5f wc: %5f", r, g, b, ww_quantity*whites_quantity, wc_quantity*whites_quantity);

    set_led_duty(r, g, b, ww_quantity*whites_quantity, wc_quantity*whites_quantity);
}

static void update_leds() 
{
    ESP_LOGI(TAG, "update_leds: on=%d, brightness=%.2f, mode=%d, ct=%d, x=%.2f, y=%.2f", 
        current_on, current_brightness, current_mode_is_ct, current_ct, current_x, current_y);

    if (!current_on)
    {
        set_led_duty(0, 0, 0, 0, 0);
        return;
    }

    if (current_mode_is_ct)
        update_leds_ct(current_ct, current_brightness);
    else 
    {
        float Y = 1.0f;
        float X = (Y / current_y) * current_x;
        float Z = (Y / current_y) * (1.0f - current_x - current_y);

        float r =  3.2406255f*X - 1.5372080f*Y - 0.4986286f*Z;
        float g = -0.9689307f*X + 1.8757561f*Y + 0.0415175f*Z;
        float b =  0.0557101f*X - 0.2040211f*Y + 1.0569959f*Z;

        r = CLAMP_RGB(r);
        g = CLAMP_RGB(g);
        b = CLAMP_RGB(b);

        float max = MAX_RGB(r, g, b);

        if (max > 1.0f) 
        {
            r /= max;
            g /= max;
            b /= max;
        }

        r*= current_brightness;
        g*= current_brightness;
        b*= current_brightness;

        update_leds_linear_rgb(r, g, b);
    }
}

#define CLAMP_DUTY(x) (x < LUNA_LEDC_DUTY_MIN ? (x > (LUNA_LEDC_DUTY_MIN/2) ? LUNA_LEDC_DUTY_MIN : 0) : (x > LUNA_LEDC_DUTY_MAX ? LUNA_LEDC_DUTY_MAX : x))

static void set_led_duty(float r, float g, float b, float ww, float wc)
{
    uint32_t duty_r = CLAMP_DUTY((int)(r*LUNA_LEDC_DUTY_MAX));
    uint32_t duty_g = CLAMP_DUTY((int)(g*LUNA_LEDC_DUTY_MAX));
    uint32_t duty_b = CLAMP_DUTY((int)(b*LUNA_LEDC_DUTY_MAX));
    uint32_t duty_ww = CLAMP_DUTY((int)(ww*LUNA_LEDC_DUTY_MAX));
    uint32_t duty_wc = CLAMP_DUTY((int)(wc*LUNA_LEDC_DUTY_MAX));

#ifndef LUNA_LEDC_USE_FADE
    ledc_set_duty(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_R, duty_r);
    ledc_set_duty(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_G, duty_g);
    ledc_set_duty(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_B, duty_b);
    ledc_set_duty(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_WW, duty_ww);
    ledc_set_duty(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_WC, duty_wc);

    ledc_update_duty(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_R);
    ledc_update_duty(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_G);
    ledc_update_duty(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_B);
    ledc_update_duty(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_WW);
    ledc_update_duty(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_WC);
#else
    ledc_fade_stop(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_R);
    ledc_fade_stop(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_G);
    ledc_fade_stop(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_B);
    ledc_fade_stop(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_WW);
    ledc_fade_stop(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_WC);

    ledc_set_fade_with_time(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_R, duty_r, LUNA_LEDC_FADE_TIME_MS);
    ledc_set_fade_with_time(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_G, duty_g, LUNA_LEDC_FADE_TIME_MS);
    ledc_set_fade_with_time(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_B, duty_b, LUNA_LEDC_FADE_TIME_MS);
    ledc_set_fade_with_time(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_WW, duty_ww, LUNA_LEDC_FADE_TIME_MS);
    ledc_set_fade_with_time(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_WC, duty_wc, LUNA_LEDC_FADE_TIME_MS);

    ledc_fade_start(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_R, LEDC_FADE_NO_WAIT);
    ledc_fade_start(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_G, LEDC_FADE_NO_WAIT);
    ledc_fade_start(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_B, LEDC_FADE_NO_WAIT);
    ledc_fade_start(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_WW, LEDC_FADE_NO_WAIT);
    ledc_fade_start(LUNA_LEDC_MODE, LUNA_LEDC_CHANNEL_WC, LEDC_FADE_NO_WAIT);
#endif
}