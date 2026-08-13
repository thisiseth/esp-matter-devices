#include <stdio.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "../../sunlamp/main/mcpwm_hbridge_led.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "trying to initialize mcpwm");

    const mcpwm_hbridge_led_config_t led_config = {
        .pin_ww_plus = 5,
        .pin_wc_plus = 6,

        .pwm_frequency = 10000,
        .pwm_depth_bits = 10,
        .minimum_pulse_ns = 1400,

        .use_fade = true
    };

    if (!mcpwm_hbridge_led_init(&led_config)) 
    {
        ESP_LOGE(TAG, "failed to initialize mcpwm");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_system_abort("no mcpwm");
    }

    ESP_LOGI(TAG, "mcpwm iniialized");

    for (;;) 
    {
        mcpwm_hbridge_led_set(0.03, 0.6);
        vTaskDelay(pdMS_TO_TICKS(3000));
        mcpwm_hbridge_led_set(1.0, 1.0);
        vTaskDelay(pdMS_TO_TICKS(3000));
        mcpwm_hbridge_led_set(1.0, 0.0);
        vTaskDelay(pdMS_TO_TICKS(3000));
        mcpwm_hbridge_led_set(0.0, 1.0);
        vTaskDelay(pdMS_TO_TICKS(3000));
        mcpwm_hbridge_led_set(0.0000001, 0.0000001);
        vTaskDelay(pdMS_TO_TICKS(3000));
        mcpwm_hbridge_led_set(0.0, 0.0);
        vTaskDelay(pdMS_TO_TICKS(3000));        
        
        mcpwm_hbridge_led_set(0.1, 0.1);
        vTaskDelay(pdMS_TO_TICKS(3000));
        mcpwm_hbridge_led_set(0.0, 0.1);
        vTaskDelay(pdMS_TO_TICKS(3000));
        mcpwm_hbridge_led_set(0.1, 0.0);
        vTaskDelay(pdMS_TO_TICKS(3000));

        mcpwm_hbridge_led_set(0.013, 0.013);
        vTaskDelay(pdMS_TO_TICKS(3000));
        mcpwm_hbridge_led_set(0.013, 0.0);
        vTaskDelay(pdMS_TO_TICKS(3000));
        mcpwm_hbridge_led_set(0.0, 0.013);
        vTaskDelay(pdMS_TO_TICKS(3000));

        // mcpwm_hbridge_led_set(0.01, 0.01);
        // vTaskDelay(pdMS_TO_TICKS(3000));
        // mcpwm_hbridge_led_set(0.009, 0.009);
        // vTaskDelay(pdMS_TO_TICKS(3000));
        // mcpwm_hbridge_led_set(0.008, 0.008);
        // vTaskDelay(pdMS_TO_TICKS(3000));
        // mcpwm_hbridge_led_set(0.007, 0.007);
        // vTaskDelay(pdMS_TO_TICKS(3000));
        // mcpwm_hbridge_led_set(0.006, 0.006);
        // vTaskDelay(pdMS_TO_TICKS(3000));
        // mcpwm_hbridge_led_set(0.005, 0.005);
        // vTaskDelay(pdMS_TO_TICKS(3000));
        mcpwm_hbridge_led_set(0.002, 0.002);
        vTaskDelay(pdMS_TO_TICKS(3000));
        mcpwm_hbridge_led_set(0.001, 0.001);
        vTaskDelay(pdMS_TO_TICKS(3000));
        mcpwm_hbridge_led_set(0.0001, 0.0001);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
